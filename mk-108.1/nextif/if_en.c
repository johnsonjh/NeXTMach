/*	@(#)if_en.c	1.0	11/17/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 22-Aug-90  John Seamons (jks) at NeXT
 *	Changed call to dma_sync_cache to pass direction argument.
 *
 *  4-Aug-90  John Seamons (jks) at NeXT
 *	Added automatic switching between thin and TPE.
 *
 *  4-Jun-90  John Seamons (jks) at NeXT
 *	Use bytecopy() instead of bcopy() when setting interface address
 *	as required by 040.
 *
 * 09-Apr-90  Bradley Taylor (btaylor) at NeXT
 *	Open driver to non-IP protocols using new netbuf and netif 
 *	interfaces. Some cleanup still remains tbd (XXX).
 *
 * 19-Jun-89  Mike DeMoney (mike) at NeXT
 *	Cleaned up spls(), make sure timeouts running at splimp()
 *	(i.e. device level).
 *
 * 17-Nov-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

/*
 * Driver for the NeXT integrated Ethernet controller
 *
 * FIXME:
 *	- write "next" at "next+0x200" when not chunk aligned!
 *	- remove endbug printf's.
 *	- instrumentation to find out if more receive buffers are needed.
 *	- grow and shrink receive buffers as needed.
 *	- don't always copy transmit data.
 *	- on receive, should look at BOP/EOP bits to determine false chains.
 *	- splimp() could be lowered to be equal to splnet() as long as dma
 *	  softint's (i.e. effectively device interrupts) are at splnet
 */

#import <en.h>
#if NEN > 0
#import <gdb.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/time.h>
#import <sys/kernel.h>
#import <sys/mbuf.h>
#import <sys/buf.h>
#import <sys/protosw.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <sys/errno.h>
#import <sys/callout.h>
#import <net/if.h>
#import <net/netbuf.h>
#import <net/netisr.h>
#import <net/route.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/psl.h>
#import <next/vm_param.h>
#import <next/event_meter.h>
#import <next/bmap.h>
#import <nextdev/dma.h>
#import <nextdev/busvar.h>
#import <nextif/if_enreg.h>

#import <next/spl.h>
#import <kern/xpr.h>

#import <net/etherdefs.h>
#import <nextif/if_bus.h>

#if defined(INET) && (defined(DEBUG) || defined(GDB))
/*
 * We'd really like to rid ourselves of any IP-level protocol stuff
 * in this module. Oh, well....
 */
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/in_var.h>
#import <netinet/ip.h>
#import <netinet/if_ether.h>
#import <netinet/tcp.h>
#import <netinet/udp.h>
#import <netinet/ip_icmp.h>
#endif

#ifndef ntohs
#define ntohs(x) (x)
#endif

#if	DEBUG
int	endbug;
#define	dbug1		if (endbug & 1) printf		/* rx/tx dbug */
#define	dbug2		if (endbug & 2) printf		/* init dbug */
#define	DBUG_PPKT	0x04
#define	dbug4		if (endbug & DBUG_PPKT) printf	/* dump pkts */
#else	DEBUG
#define	dbug1
#define	dbug2
#define	dbug4
#endif	DEBUG

#define NRXBUFS		16
#define NTXBUFS		8
#define	EN_TX_TIMEOUT	hz
#define	EN_RX_TIMEOUT	10

u_char etherbcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

int	enprobe(), enattach();
void	en_rx_dmaintr(), en_tx_dmaintr();
int	eninit(netif_t);
int 	enoutput(netif_t, netbuf_t, void *);
int	encontrol(netif_t, const char *, void *);
int	enreset();

extern netbuf_t engetbuf();

struct	bus_device *eninfo[NEN];
struct	bus_driver endriver = {
	enprobe, 0, enattach, 0, 0, 0, 0, 1, "en", eninfo,
};

struct	en_softc {
	netif_t is_if;			/* network visible interface */
	struct ether_addr is_addr;	/* hardware Ethernet address */
#if GDB
	struct in_addr is_ac_ipaddr;	/* copy of ip address for GDB */
#endif
	struct	dma_chan is_tx_dmachan;	/* transmitter DMA info */
	struct	dma_chan is_rx_dmachan;	/* receiver DMA info */
	volatile struct	en_regs *is_regs; /* controller registers */
	int	is_flags;
#define ENF_RUNNING	0x00000001	/* eninit has been called */
#define ENF_OACTIVE	0x00000002	/* transmit in progress */
#define ENF_SETADDR	0x00000004	/* change physical address */
#define	ENF_JAMMED	0x00000008	/* down because of excessive jams */
#define	ENF_JAM_MSG	0x00000010	/* message has been seen */
#define	EN_JAM_MAX	10		/* shutdown after this many jams */
#define	EN_ANTIJAM	(hz * 10)	/* recheck network after this time */
	int	is_jam_count;		/* current jam count */
	struct	timeval is_last_rx;	/* last time something received */
	struct	en_dmabuf is_rx_dhp[NRXBUFS];
	struct	en_dmabuf is_tx_dhp[NTXBUFS];
	struct	dma_hdr *is_tx_free;	/* list of free transmit headers */
	struct	dma_hdr *is_rx_free;	/* list of free receive headers */
} en_softc[NEN];

#if GDB
const char IFCONTROL_SETIPADDRESS[] = "set-ipaddress";
#endif

enprobe (reg, ctrl)
	register caddr_t reg;
{
	reg += slot_id_bmap;
	return((int)reg);
}

enattach (bd)
	register struct bus_device *bd;
{
	register struct en_softc *is = &en_softc[bd->bd_unit];
	volatile register struct en_regs *regs =
		(struct en_regs*) bd->bd_addr;
	extern u_char etheraddr[];


	is->is_regs = regs;

	/* etheraddr is passed from the monitor */
	bcopy (etheraddr, &is->is_addr, sizeof (is->is_addr));
	bytecopy (etheraddr, regs->en_addr, sizeof (regs->en_addr));
	printf("en%d: Ethernet address %s\n", bd->bd_unit,
		ether_sprintf (&is->is_addr));
	
	is->is_if = if_attach(eninit, NULL, enoutput, engetbuf, encontrol,
			      "en", bd->bd_unit, IFTYPE_ETHERNET, ETHERMTU,
			      0, NETIFCLASS_REAL, NULL);
}

/* initialization of interface */
int
eninit(
       netif_t netif
       )
{
	unsigned unit = if_unit(netif);
	register struct en_softc *is = &en_softc[unit];
	register struct dma_chan *tcp = &is->is_tx_dmachan;
	register struct dma_chan *rcp = &is->is_rx_dmachan;
	register struct bus_device *bd = eninfo[unit];
	volatile register struct en_regs *regs =
		(struct en_regs*) bd->bd_addr;
	register struct ifnet *ifp = (struct ifnet *)netif;
	struct dma_hdr *dhp;
	register int i;

	if (is->is_flags & ENF_RUNNING)
		return;

	is->is_flags |= ENF_RUNNING;

	/*
	 * Initialize controller.
	 * Receive  and transmit interrupts are driven by
	 * DMA completion.
	 */
	regs->en_reset = EN_RESET_MODE;

	/* set physical address if required */
	if (is->is_flags & ENF_SETADDR) {
		bytecopy (&is->is_addr, regs->en_addr, sizeof (regs->en_addr));
		is->is_flags &= ~ENF_SETADDR;
	}
	regs->en_txstat = EN_TXSTAT_CLEAR;
	regs->en_txmask = 0;
	regs->en_txmode = EN_TXMODE_LB_DISABLE;
	regs->en_rxstat = EN_RXSTAT_CLEAR;
	regs->en_rxmask = 0;
	regs->en_rxmode = EN_RXMODE_OFF;
	regs->en_reset = 0;

	/* initialize transmitter DMA */
	bzero (tcp, sizeof *tcp);
	tcp->dc_handler = en_tx_dmaintr;
	tcp->dc_hndlrarg = unit;
	tcp->dc_hndlrpri = CALLOUT_PRI_SOFTINT1;	/* splnet */
	tcp->dc_ddp = (struct dma_dev*) P_ENETX_CSR;
	tcp->dc_flags = DMACHAN_AUTOSTART | DMACHAN_ENETX | DMACHAN_INTR;
	tcp->dc_direction = DMACSR_WRITE;
	dma_init (tcp, I_ENETX_DMA);
	is->is_tx_free = 0;
	is->is_rx_free = 0;
	for (i = 0; i < NTXBUFS; i++) {
		dhp = end2dh(&is->is_tx_dhp[i]);
		dhp->dh_link = is->is_tx_free;
		is->is_tx_free = dhp;
	}

	/* initialize receiver DMA */
	bzero (rcp, sizeof *rcp);
	rcp->dc_handler = en_rx_dmaintr;
	rcp->dc_hndlrarg = unit;
	rcp->dc_hndlrpri = CALLOUT_PRI_SOFTINT1;	/* splnet */
	rcp->dc_ddp = (struct dma_dev*) P_ENETR_CSR;
	rcp->dc_flags = DMACHAN_AUTOSTART | DMACHAN_ENETR | DMACHAN_INTR |
		DMACHAN_CHAININTR;
	rcp->dc_direction = DMACSR_READ;
	dma_init (rcp, I_ENETR_DMA);
	for (i = 0; i < NRXBUFS; i++) {
		dhp = end2dh(&is->is_rx_dhp[i]);
		if (!if_busalloc(dh2end(dhp), NULL)) {
			dhp->dh_link = is->is_rx_free;
			is->is_rx_free = dhp;
			continue;
		}
		dhp->dh_stop = (char*) roundup ((int)dhp->dh_start + BUF_SIZE,
			DMA_ENDALIGNMENT);
		dhp->dh_flags = DMADH_INITBUF;
		dma_enqueue (rcp, dhp);
	}

	regs->en_rxmode = EN_RXMODE_NORMAL;
}

en_rx_grabbufs(is)
	struct en_softc *is;
{
	register struct dma_chan *rcp = &is->is_rx_dmachan;
	int s;
	struct dma_hdr *dhp;

	/*
	 * Queue up any receive buffers waiting for memory, if possible.
	 */
	
	s = splimp();
	while (dhp = is->is_rx_free) {
		is->is_rx_free = dhp->dh_link;
		splx(s);
		if (!if_busalloc(dh2end(dhp), NULL)) {
			splimp();
			dhp->dh_link = is->is_rx_free;
			is->is_rx_free = dhp;
			break;
		}
		dhp->dh_stop = (char*) roundup ((int)dhp->dh_start + BUF_SIZE,
			DMA_ENDALIGNMENT);
		dhp->dh_flags = DMADH_INITBUF;
		dma_enqueue (rcp, dhp);
		s = splimp();
	}
	splx(s);
}


/*
 * Ethernet interface receiver DMA interrupt.
 * If input error just drop packet.
 * Otherwise examine packet to determine type.  If can't determine length
 * from type, then have to drop packet.  Othewise decapsulate packet based
 * on type and pass to type specific higher-level input routine.
 */
void
en_rx_dmaintr (unit)
	int unit;
{
	register struct en_softc *is = &en_softc[unit];
	register struct dma_chan *rcp = &is->is_rx_dmachan;
	register struct dma_hdr *dhp;
	netbuf_t nb;
	int len, off, resid, s;
	register struct ifqueue *inq;
	char *bp;
	struct ether_header *eh;


	ASSERT(curipl() == IPLNET);
dbug1 ("\nrxint: ");
	while ((dhp = dma_dequeue (rcp, DMADQ_DONE)) != NULL) {
dbug1 ("dhp 0x%x ", dhp);

	event_meter (EM_ENET);
	if (rcp->dc_flags & DMACHAN_ERROR) {
		printf ("en%d: DMA error on receive\n", unit);
		rcp->dc_flags &= ~DMACHAN_ERROR;
	}
	if_ipackets_set(is->is_if, if_ipackets(is->is_if) + 1);
	bp = dhp->dh_start;
	/*
	 * Determine packet length from dh_next.
	 * Strip BOP & EOP bits first.
	 */
	len = ((int) dhp->dh_next & ~(ENRX_EOP|ENRX_BOP)) -
		(int) bp - HDR_SIZE - FCS_SIZE;
	if (len < ETHERMIN || len > ETHERMTU) {
#ifdef	notdef
		printf ("enrx: packet length %d bad, dh_next 0x%x bp 0x%x\n",
			len, dhp->dh_next, bp);
#endif	notdef
#if	DEBUG
if (endbug & 8) { int i;
for (i = 0; i < len + HDR_SIZE + FCS_SIZE; i += 4) {
if (((i/4) % 8) == 0)
	printf ("\n%08x: ", (int)bp+i);
printf ("%08x ", *(int*)((int)bp+i));
}
printf ("\n");
}
#endif	DEBUG

		if_ierrors_set(is->is_if, if_ierrors(is->is_if) + 1);
		goto resetup;
	}
#if	DEBUG
{
	int i;
	dbug1 ("len %d bp 0x%x\n", len, bp);
	if (endbug & DBUG_PPKT) {
		ppkt (bp);
		for (i = 0; i < 0x30; i += 4) {
			if (((i/4) % 12) == 0)
				printf ("\n%08x: ", (int)bp+i);
			printf ("%08x ", *(int*)((int)bp+i));
		}
		printf ("\n");
	}
}
#endif	DEBUG
	if (len == 0) {
dbug1 ("len=0! ");
		goto resetup;
	}

	nb = if_rbusget (dh2end(dhp), HDR_SIZE + len);
	if (nb == 0) {
		printf ("enrx: no network buffers\n");
		goto resetup;
	} else {
		if_handle_input(is->is_if, nb, NULL);
	}
		    
setup:
	if (!if_busalloc(dh2end(dhp), NULL)) {
		s = splimp();
		dhp->dh_link = is->is_rx_free;
		is->is_rx_free = dhp;
		splx(s);
		timeout(en_rx_grabbufs, is, hz);
	} else {
resetup:
		dhp->dh_stop = (char*) roundup ((int)dhp->dh_start + BUF_SIZE,
			DMA_ENDALIGNMENT);
		dhp->dh_flags = DMADH_INITBUF;
		dma_enqueue (rcp, dhp);	/* queue up buffer on rx chain */
	}
	}	/* while */
	microtime (&is->is_last_rx);
dbug1 ("enrx: done ");
}

/*
 * Ethernet output routine.
 * Encapsulate a packet of type family for the local net.
 * Use trailer local net encapsulation if enough data in first
 * packet leaves a multiple of 512 bytes of data in remainder.
 */
enoutput(
	 netif_t netif,
	 netbuf_t nb,
	 void *dst
	 )
{
	int type, s, error;
 	u_char edst[6];
	struct ifnet *ifp = (struct ifnet *)netif; /* XXX */
	struct mbuf *m = (struct mbuf *)nb; /* XXX */ 
	register struct en_softc *is = &en_softc[ifp->if_unit];
	register struct ether_header *outeh;
	register int off;
	int usetrailers;


	if (!(is->is_flags & ENF_RUNNING) || (is->is_flags & ENF_JAMMED)) {
		error = ENETDOWN;
		goto bad;
	}

	outeh = mtod(m, struct ether_header *);
 	bcopy(dst, (caddr_t)outeh->ether_dhost, sizeof (outeh->ether_dhost));
 	bcopy((caddr_t)&is->is_addr, (caddr_t)outeh->ether_shost,
	      sizeof(outeh->ether_shost));

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	s = splimp();
	if (IF_QFULL(&ifp->if_snd)) {
		IF_DROP(&ifp->if_snd);
		splx(s);
		m_freem(m);
		/* printf ("entx: q full\n"); */
		return (ENOBUFS);
	}
	IF_ENQUEUE(&ifp->if_snd, m);
	if ((is->is_flags & ENF_OACTIVE) == 0)
		enstart(ifp->if_unit);
	splx(s);
	return (0);

bad:
	nb_free(nb);
	return (error);
}


/*
 * Start output on interface.
 * Get another datagram to send off of the interface queue,
 * and map it to the interface before starting the output.
 */
enstart(dev)
	dev_t dev;
{
        int unit = minor(dev), len;
	register struct en_softc *is = &en_softc[unit];
	register struct dma_chan *tcp = &is->is_tx_dmachan;
	netbuf_t nb;
	struct mbuf *m;
	register struct dma_hdr *dhp;
	int s, en_jam();
	struct ifnet *ifp = (struct ifnet *)is->is_if; /* XXX */

	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == 0)
		return;
	nb = (netbuf_t)m; /* XXX */
	s = splimp();
	if ((dhp = is->is_tx_free) == 0) {
		splx (s);
		printf ("entx: no dma headers\n");
		nb_free(nb);
		return;
	}
	is->is_tx_free = dhp->dh_link;
	splx (s);
	len = nb_size(nb);
	if_busalloc(dh2end(dhp), nb);
#if	DEBUG
{
	int i, bp;
	bp = (int)dhp->dh_start;
	if (endbug & DBUG_PPKT) {
		printf ("\ntx: len %d\n", len);
		ppkt (bp);
		for (i = 0; i < 0x30; i += 4) {
			if (((i/4) % 12) == 0)
				printf ("\n%08x: ", (int)bp+i);
			printf ("%08x ", *(int*)((int)bp+i));
		}
		printf ("\n");
	}
}
#endif	DEBUG
	if (len - sizeof(struct ether_header) < ETHERMIN)
		len = ETHERMIN + sizeof(struct ether_header);
	dhp->dh_stop = (char*) ((int) dhp->dh_start + len | ENTX_EOP);
	dhp->dh_stop += 15;

	dhp->dh_flags = DMADH_INITBUF;

	/*
	 *  When the network is disconnected, sometimes carrier detect
	 *  remains continuously busy which prevents any packet from
	 *  being transmitted and the 16-collisions-in-a-row error from
	 *  hapenning (which is normally used to detect network disconnects).
	 *  So a guard timer is maintained to catch this possiblity.
	 */
	s = splimp();
	timeout (en_jam, is, EN_TX_TIMEOUT);
	dma_enqueue (tcp, dhp);
	is->is_flags |= ENF_OACTIVE;
	splx(s);
}

en_down (unit)
{
	register struct en_softc *is = &en_softc[unit];
	register s;

	s = splimp();
	if_down (is->is_if);
	splx(s);
}

en_antijam (is)
	register struct en_softc *is;
{
	register int s;

	s = splimp();
	if ((is->is_flags & ENF_JAMMED) == 0) {
		splx(s);
		return;
	}
	is->is_flags &= ~ENF_JAMMED;
/*	enstart (makedev (0, is - en_softc)); */
	splx(s);
}

/* 150 mS max between TPE heartbeats */
#define	EN_TPE_TIMEOUT	150000

en_jam (is)
	register struct en_softc *is;
{
	volatile register struct en_regs *en = is->is_regs;
	register int s, i;

	s = splimp();
	is->is_jam_count = 0;
	splx(s);
	
	if (bmap_chip) {
		/* if connected to thin, switch to TPE if heartbeat heard */
		if (!(bmap_chip->bm_drw & BMAP_TPE_RXSEL)) {
			for (i = event_get(); event_delta(i) < EN_TPE_TIMEOUT;)
				if ((bmap_chip->bm_drw & BMAP_HEARTBEAT) == 0)
					break;
			if (event_delta(i) < EN_TPE_TIMEOUT) {
				bmap_chip->bm_drw |= BMAP_TPE;
				en->en_txmode &= ~EN_TXMODE_LB_DISABLE;
				return (0);
			}
		} else {
			for (i = event_get(); event_delta(i) < EN_TPE_TIMEOUT;)
				if ((bmap_chip->bm_drw & BMAP_HEARTBEAT) == 0)
					break;
			if (event_delta(i) >= EN_TPE_TIMEOUT) {
				bmap_chip->bm_drw &= ~BMAP_TPE;
				en->en_txmode |= EN_TXMODE_LB_DISABLE;
				return (1);
			}
			return (0);
		}
	}
	s = splimp();
	if ((is->is_flags & ENF_JAM_MSG) == 0) {
		printf ("The network is disabled or your computer "
			"isn't connected to it.\n");
		is->is_flags |= ENF_JAM_MSG;
	}
	is->is_flags |= ENF_JAMMED;
	en_down (is - en_softc);
	timeout (en_antijam, is, EN_ANTIJAM);
	splx(s);
	return (1);
}

/* transmitter device interrupt */
void
en_tx_dmaintr (unit)
	int unit;
{
	register struct en_softc *is = &en_softc[unit];
	register struct dma_chan *tcp = &is->is_tx_dmachan;
	register struct dma_hdr *dhp;
	volatile register struct en_regs *regs = is->is_regs;
	register int i, txstat;
	struct timeval tv;

	ASSERT(curipl() == IPLNET);
dbug1 ("\ntxint: ");
	if ((is->is_flags & ENF_OACTIVE) == 0) {
		printf ("en%d: stray xmit interrupt\n", unit);
		return;
	}
	untimeout (en_jam, is);
	while ((dhp = dma_dequeue (tcp, DMADQ_DONE)) != NULL) {

	event_meter (EM_ENET);
	if (tcp->dc_flags & DMACHAN_ERROR) {
		printf ("en%d: DMA error on transmit\n", unit);
		tcp->dc_flags &= ~DMACHAN_ERROR;
		dhp->dh_flags = DMADH_INITBUF;
		dma_enqueue (tcp, dhp);
		return;
	}

	for (i = 0; i < 100000 & (regs->en_txstat & EN_TXSTAT_READY) == 0;)
		i++;
	if (i == 100000)
		printf ("en%d: transmitter not ready\n", unit);
	txstat = regs->en_txstat;
	regs->en_txstat = EN_TXSTAT_CLEAR;

	if (txstat & EN_TXSTAT_COLLERR16) {
		if_oerrors_set(is->is_if, if_oerrors(is->is_if) + 1);
		if (is->is_jam_count++ > EN_JAM_MAX &&
		    (is->is_flags & ENF_JAMMED) == 0) {
			if (en_jam (is))
				goto out;
		}
		dhp->dh_flags = DMADH_INITBUF;
		dma_enqueue (tcp, dhp);
		return;
	}
	microtime (&tv);
	if (bmap_chip && (bmap_chip->bm_drw & BMAP_TPE_RXSEL) &&
	    ((tv.tv_sec - is->is_last_rx.tv_sec) > EN_RX_TIMEOUT)) {
		if (en_jam (is)) {
			dhp->dh_flags = DMADH_INITBUF;
			dma_enqueue (tcp, dhp);
			return;
		}
	}
/*	is->is_jam_count = 0; */
	if_opackets_set(is->is_if, if_opackets(is->is_if) + 1);
	is->is_flags &= ~ENF_JAM_MSG;
out:
	if_busfree(dh2end(dhp));
	dhp->dh_link = is->is_tx_free;
	is->is_tx_free = dhp;
	}

	is->is_flags &= ~ENF_OACTIVE;
/*	if ((is->is_flags & ENF_JAMMED) == 0) */
		enstart (unit);
}

/*
 * Process an ioctl request.
 */
int
encontrol(
	  netif_t netif,
	  const char *command,
	  void *data
	  )
{
	struct ifnet *ifp = (struct ifnet *)netif; /* XXX */
	struct en_softc *is = &en_softc[ifp->if_unit];
	int s;
	int error = 0;

	s = splimp();
	if (strcmp(command, IFCONTROL_SETFLAGS) == 0) {
		if (is->is_flags & ENF_RUNNING) {
			((volatile struct en_regs *)
			 	(eninfo[ifp->if_unit]->bd_addr))->en_reset = 
				EN_RESET_MODE;
			is->is_flags &= ~ENF_RUNNING;
			untimeout (en_antijam, is);
		} else
		if ((is->is_flags & ENF_RUNNING) == 0)
			eninit(netif);
	} else if (strcmp(command, IFCONTROL_GETADDR) == 0) {
		bcopy(&is->is_addr, data, sizeof(is->is_addr));
#if GDB
	} else if (strcmp(command, IFCONTROL_SETIPADDRESS) == 0) {
		bcopy(data, &is->is_ac_ipaddr, sizeof(struct in_addr));
#endif
	} else {
		error = EINVAL;
	}
	splx(s);
	return (error);
}

/*
 * set ethernet address for unit
 */
en_setaddr(physaddr, unit)
u_char *physaddr;
int unit;
{
	register struct en_softc *is = &en_softc[unit];
	register int s;
	
	s = splimp();
	bcopy((caddr_t)physaddr, (caddr_t)&is->is_addr, sizeof is->is_addr);
	is->is_flags |= ENF_SETADDR;
	if (!(is->is_flags & ENF_RUNNING)) {
		splx(s);
		return;
	}
	is->is_flags &= ~ENF_RUNNING;
	eninit(is->is_if);
	splx(s);
}

#if	DEBUG
ppkt (bp)
	char *bp;
{
	struct ether_header *eh = (struct ether_header*) bp;
	struct ip *ip = (struct ip*) ((int)eh + sizeof (struct ether_header));
	struct tcphdr *th = (struct tcphdr*)((int)ip + sizeof (struct ip));
	struct udphdr *uh = (struct udphdr*)((int)ip + sizeof (struct ip));
	struct ether_arp *ar = (struct ether_arp*) ip;
	u_char *e;
	struct icmp *ic = (struct icmp*)((int)ip + sizeof (struct ip));

	printf ("d ");
	if (bcmp (eh->ether_dhost, etherbcastaddr, 6) == 0)
		printf ("bcast ");
	else {
		e = (u_char*) eh->ether_dhost;
		printf ("%02x%02x%02x%02x%02x%02x ",
			e[0], e[1], e[2], e[3], e[4], e[5]);
	}
	printf ("s ");
	if (bcmp (eh->ether_shost, etherbcastaddr, 6) == 0)
		printf ("bcast ");
	else {
		e = (u_char*) eh->ether_shost;
		printf ("%02x%02x%02x%02x%02x%02x ",
			e[0], e[1], e[2], e[3], e[4], e[5]);
	}

	switch (eh->ether_type) {

	case ETHERTYPE_IP:
		printf ("IP ");
		if (ip->ip_p == IPPROTO_UDP) {
			printf ("UDP sp %d dp %d ",
				uh->uh_sport, uh->uh_dport);
		} else
		if (ip->ip_p == IPPROTO_ICMP) {
			printf ("ICMP t %d c %d ",
				ic->icmp_type, ic->icmp_code);
		} else
		if (ip->ip_p == IPPROTO_TCP) {
			printf ("TCP sp %d dp %d ",
				th->th_sport, th->th_dport);
		} else
		if (ip->ip_p == IPPROTO_RAW) {
			printf ("RAW len %d src %s dst %s sum 0x%x ",
				ip->ip_len, 
				inet_ntoa (&ip->ip_src.s_addr),
				inet_ntoa (&ip->ip_dst.s_addr),
				ip->ip_sum);
		} else
			printf ("proto %d ", ip->ip_p);
		break;

	case ETHERTYPE_ARP:
		printf ("ARP %s ", ar->ea_hdr.ar_op == ARPOP_REQUEST?
			"req" : "rep");
		e = (u_char*) ar->arp_sha;
		printf ("sh %02x%02x%02x%02x%02x%02x sp %s ",
			e[0], e[1], e[2], e[3], e[4], e[5],
			inet_ntoa (ar->arp_spa));
		e = (u_char*) ar->arp_tha;
		printf ("th %02x%02x%02x%02x%02x%02x tp %s ",
			e[0], e[1], e[2], e[3], e[4], e[5],
			inet_ntoa (ar->arp_tpa));

		break;

	default:
		printf ("type 0x%x ", eh->ether_type);
		break;
	}
	printf ("\n");
}
#endif	DEBUG

/* standalone routines for debugger interface */
#if	GDB

struct en_pkt {
	struct	ether_header ether;
	struct	ip ip;
	struct udphdr udp;
	char pad[ETHERMTU+DMA_ENDALIGNMENT];
} en_rpkt1, en_rpkt2, en_tpkt, en_proto, *ep[2];

en_recv(bp, maxlen, locked)
	struct en_pkt *bp;
{
	volatile struct dma_dev *rdma = (struct dma_dev*) P_ENETR_CSR;
	struct en_pkt *eproto =
		(struct en_pkt*) roundup((int)&en_proto, DMA_ENDALIGNMENT);
	int dmacsr, len = 0;
	static rx_setup, rx_init, epi;
	volatile register struct en_regs *regs = (struct en_regs*) P_ENET;
	extern u_char etheraddr[];
	
	if (!rx_init) {

		/* etheraddr is passed from the monitor */
		bytecopy (etheraddr, regs->en_addr, sizeof (regs->en_addr));
		regs->en_reset = EN_RESET_MODE;
		regs->en_txstat = EN_TXSTAT_CLEAR;
		regs->en_txmask = 0;
		regs->en_txmode = EN_TXMODE_LB_DISABLE;
		regs->en_rxstat = EN_RXSTAT_CLEAR;
		regs->en_rxmask = 0;
		regs->en_rxmode = EN_RXMODE_OFF;
		regs->en_reset = 0;
		regs->en_rxmode = EN_RXMODE_NORMAL;
		ep[0] = (struct en_pkt*) roundup((int)&en_rpkt1,
			DMA_ENDALIGNMENT);
		ep[1] = (struct en_pkt*) roundup((int)&en_rpkt2,
			DMA_ENDALIGNMENT);
		dma_sync_cache (ep[1], ep[1] + sizeof *ep, DMACSR_READ);
		epi = 0;
		rx_init = 1;
	}
	if (rx_setup) {
		while ((dmacsr = rdma->dd_csr) == 0)
			;
		if (dmacsr & DMACSR_ENABLE)
			return(0);
		
		switch (ep[epi]->ether.ether_type) {
		
		case ETHERTYPE_IP:
			if (ep[epi]->ip.ip_p == IPPROTO_UDP &&
			    ep[epi]->udp.uh_sport == 1138)
				len = ((int)rdma->dd_next &
					~(ENRX_EOP|ENRX_BOP)) - (int)ep[epi] - FCS_SIZE;
			break;
			
		case ETHERTYPE_ARP:
			en_arp_input(ep[epi]);
			/* fall through ... */
		
		default:
			break;
		}
		if (maxlen < len)
			len = maxlen;
		if (len)
			bcopy (ep[epi], bp, len);
		if (len && !locked)
			bcopy (ep[epi], eproto, len);
		if (len < 0 || len > ETHERMTU)
			len = 0;
	}
	epi ^= 1;
	rdma->dd_csr = DMACSR_RESET | DMACSR_INITBUF | DMACSR_READ;
	rdma->dd_next = (char*)ep[epi];
	rdma->dd_limit = (char*)ep[epi] + ((ETHERMTU + 15) &  ~0xf);
	rdma->dd_csr = DMACSR_READ | DMACSR_SETENABLE;
	
	/*
	 *  Cache flush takes a long time to run causing us to miss
	 *  back-to-back packets if we use a single buffer.
	 *  Ping-pong between two receive buffers and flush the inactive
	 *  one here.
	 */
	dma_sync_cache (ep[epi^1], ep[epi^1] + sizeof *ep, DMACSR_READ);
	rx_setup = 1;
	return (len);
}

en_arp_input(bp)
	struct en_pkt *bp;
{
	register struct ether_header *eh = (struct ether_header *) bp;
	register struct ether_arp *arp = (struct ether_arp *) ((char*)bp+sizeof(*eh));
	int len;
	struct en_softc *is = en_softc;
	
	if (arp->arp_op == ARPOP_REQUEST && arp->arp_hrd == ARPHRD_ETHER &&
	    arp->arp_pro == ETHERTYPE_IP &&
	    ((struct in_addr*)arp->arp_tpa)->s_addr == is->is_ac_ipaddr.s_addr) {
		/* It's our arp request */
		bcopy (arp->arp_sha, arp->arp_tha, 6);
		bcopy (arp->arp_spa, arp->arp_tpa, 4);
		bcopy (&is->is_addr, arp->arp_sha, 6);
		*(struct in_addr*) arp->arp_spa = is->is_ac_ipaddr;
		arp->arp_op = ARPOP_REPLY;
		bcopy (arp->arp_tha, eh->ether_dhost, 6);
		bcopy (&is->is_addr, eh->ether_shost, 6);
		len = sizeof (struct ether_header) + sizeof (struct ether_arp);
		en_xmit((caddr_t)eh, len);
	    }
}

int en_send(tp, len)
	struct en_pkt *tp;
{
	struct en_pkt *ep = (struct en_pkt*) roundup((int)&en_proto, DMA_ENDALIGNMENT);
	struct en_pkt *bp = (struct en_pkt*) roundup((int)&en_tpkt, DMA_ENDALIGNMENT);
	u_short checksum_16();

	bcopy(tp, bp, len);
	bp->ether = ep->ether;
	bcopy(ep->ether.ether_shost, bp->ether.ether_dhost,
		sizeof (struct ether_addr));
	bcopy(ep->ether.ether_dhost, bp->ether.ether_shost,
		sizeof (struct ether_addr));
	bp->ip = ep->ip;
	bp->ip.ip_src = ep->ip.ip_dst;
	bp->ip.ip_dst = ep->ip.ip_src;
	bp->ip.ip_len = len - sizeof (struct ether_header);
	bp->ip.ip_sum = 0;
	bp->ip.ip_sum = ~checksum_16(&bp->ip, sizeof (struct ip) >> 1);
	bp->udp.uh_dport = ep->udp.uh_sport;
	bp->udp.uh_sport = ep->udp.uh_dport;
	bp->udp.uh_ulen = len - (sizeof (struct ether_header) + sizeof (struct ip));
	bp->udp.uh_sum = 0;
	return (en_xmit(bp, len + sizeof (struct ether_header)));
}

int en_xmit(bp, len)
	struct en_pkt *bp;
{
	volatile struct en_regs *en = (struct en_regs*) P_ENET;
	volatile struct dma_dev *tdma = (struct dma_dev*) P_ENETX_CSR;
	u_int lim, time, timeouts = 0;

	if (len < ETHERMIN + sizeof (struct ether_header))
		len = ETHERMIN + sizeof (struct ether_header);
	do  {	
		while (!(en->en_txstat & EN_TXSTAT_READY))
			;
		dma_sync_cache (bp, bp + sizeof *bp, DMACSR_WRITE);
		tdma->dd_csr = DMACSR_INITBUF | DMACSR_RESET | DMACSR_WRITE;
		tdma->dd_next_initbuf = (char*)bp;
		tdma->dd_saved_next = (char*)bp;
		lim = (u_int)bp + len + 15 | ENTX_EOP;
		tdma->dd_limit = tdma->dd_saved_limit = (char*)lim;
		tdma->dd_csr = DMACSR_WRITE | DMACSR_SETENABLE;
	
		time = event_get();
		while ((tdma->dd_csr & DMACSR_COMPLETE) == 0)  {
			DELAY(1);
			if (event_get() - time > 10000)  {
				timeouts++;
				printf ("en_xmit: timeout\n");
				break;
			}
		}
		if (timeouts == 0)
			break;
	} while (timeouts < 4);
	tdma->dd_csr = DMACSR_RESET;
	if (timeouts < 4)
		return (1);
	else
		return (-1);
}
#endif	GDB
#endif
