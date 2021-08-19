/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)if_ether.c	7.6 (Berkeley) 12/7/87
 *
 * HISTORY
 *
 * 09-Apr-90  Bradley Taylor (btaylor) at NeXT
 *	Changes to support new network API (netbuf and netif)
 *
 * 26-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: Improved RARP support.  Added RARP client support
 *			 but left it turned off.
 *
 * 11-Oct-88  Peter King (king) at NeXT, Inc.
 *	Fixed ARPTAB_LOOKUP and associated routines so that SIOCSARP would
 *	work with for all interfaces.  Removed CS_INET switches (too messy)!
 *
 *  2-Aug-88  Peter King (king) at NeXT, Inc.
 *	Merged D/NFS release changes.
 *
 * 20-Oct-87  Peter King (king) at NeXT, Inc.
 *	SUN_RPC: Allow passing of ethernet info to the IP layer.
 *		 Add RARP support.
 *
 * 27-Jan-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Fixed bug in ioctl() processing which failed to pass new
 *	interface parameter to arptnew() as reported by Bill Bolosky.
 *	[ V5.1(F1) ]
 *
 * 21-Aug-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Moved return in broadcast check within arpresolve() out of CS_INET
 *	conditional so that it gets executed in both cases.
 *
 * 11-Aug-86  David Golub (dbg) at Carnegie-Mellon University
 *	Fixed to compile without 'vax' or 3Mb ethernet.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 13-Aug-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_INET:  Parameterized to support multiple ethernet types and
 *	allow multiple heterogenous interfaces to co-exist.
 *	NEN:  Added support for 3Mb ethernet.
 *	[V1(1)]
 *
 * 27-Jun-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_INET: disabled silly ARP bypass by local host address check
 *	by setting local host upper limit above the maximum legal local
 *	host.
 *	[V1(1)].
 *
 **********************************************************************
 */
 
#ifdef	vax
#import <en.h>
#else	vax
#define	NEN 0
#endif	vax

/*
 * Ethernet address resolution protocol.
 * TODO:
 *	run at splnet (add ARP protocol intr.)
 *	link entries onto hash chains, keep free list
 *	add "inuse/lock" bit (or ref. count) along with valid bit
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/mbuf.h>
#import <sys/socket.h>
#import <sys/time.h>
#import <sys/kernel.h>
#import <sys/errno.h>
#import <sys/ioctl.h>
#import <sys/syslog.h>

#import <machine/spl.h>

#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/if_ether.h>
#if	NEN > 0
#import <vaxif/if_en.h>
#endif	NEN

#ifdef GATEWAY
#define	ARPTAB_BSIZ	16		/* bucket size */
#define	ARPTAB_NB	37		/* number of buckets */
#else
#define	ARPTAB_BSIZ	9		/* bucket size */
#define	ARPTAB_NB	19		/* number of buckets */
#endif

#define	ARPTAB_SIZE	(ARPTAB_BSIZ * ARPTAB_NB)
struct	arptab arptab[ARPTAB_SIZE];
int	arptab_size = ARPTAB_SIZE;	/* for arp command */

int arptab_bsiz = ARPTAB_BSIZ;
int arptab_nb = ARPTAB_NB;

/*
 * ARP trailer negotiation.  Trailer protocol is not IP specific,
 * but ARP request/response use IP addresses.
 */
#define ETHERTYPE_IPTRAILERS ETHERTYPE_TRAIL
#if	NEN > 0
#define ENTYPE_IPTRAILERS ENTYPE_TRAIL
#endif	NEN > 0

#define	ARPTAB_HASH(a) \
	((u_long)(a) % ARPTAB_NB)

/*
 *  Change to permit multiple heterogenous interfaces to co-exist.
 */
#define	ARPTAB_LOOK(at,addr,ifp) { \
	register n; \
	at = &arptab[ARPTAB_HASH(addr) * ARPTAB_BSIZ]; \
	for (n = 0 ; n < ARPTAB_BSIZ ; n++,at++) \
		if (at->at_iaddr.s_addr == addr && \
		    (!(ifp) || at->at_if == (ifp))) \
			break; \
	if (n >= ARPTAB_BSIZ) \
		at = 0; \
}

/* timer values */
#define	ARPT_AGE	(60*1)	/* aging timer, 1 min. */
#define	ARPT_KILLC	20	/* kill completed entry in 20 mins. */
#define	ARPT_KILLI	3	/* kill incomplete entry in 3 minutes */

#if	NEN > 0
#if	NeXT
#define arpisen(ifp)	\
	(*((short *)(ifp)->if_name) == ((('n'<<8))+'e'))
#else	NeXT
#define arpisen(ac)	\
	(*((short *)(ac)->ac_if.if_name) == ((('n'<<8))+'e'))
#endif	NeXT
#endif	NEN > 0

struct ether_arp arpethertempl =
{
  
    ARPHRD_ETHER,  ETHERTYPE_IP, 6, 4, ARPOP_REQUEST,
    0x0,  0x0,  0x0,  0x0,  0x0 , 0x0 ,
    0x0,  0x0,  0x0 , 0x0,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x0,  0x0,  0x0 , 0x0,
};
#if	NEN > 0
struct ether_arp arpentempl =
{
    ARPHRD_XETHER, ENTYPE_IP,       1, 4, ARPOP_REQUEST,
    0x0,
    0x0, 0x0, 0x0, 0x0,
    0x0,
    0x0, 0x0, 0x0, 0x0,
};
#endif	NEN > 0

u_char	etherbroadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#if	NeXT
#else	NeXT
extern struct ifnet loif;
#endif	NeXT

/*
 * Timeout routine.  Age arp_tab entries once a minute.
 */
arptimer()
{
	register struct arptab *at;
	register i;

	timeout(arptimer, (caddr_t)0, ARPT_AGE * hz);
	at = &arptab[0];
	for (i = 0; i < ARPTAB_SIZE; i++, at++) {
		if (at->at_flags == 0 || (at->at_flags & ATF_PERM))
			continue;
		if (++at->at_timer < ((at->at_flags&ATF_COM) ?
		    ARPT_KILLC : ARPT_KILLI))
			continue;
		/* timer has expired, clear entry */
		arptfree(at);
	}
}

/*
 * Broadcast an ARP packet, asking who has addr on interface ac.
 */
#if	NeXT
arpwhohas(
	  struct ifnet *ifp,
	  u_char my_enaddr[6],
	  struct in_addr my_ipaddr,
	  struct in_addr *addr
	  )
#else	NeXT
arpwhohas(ac, addr)
	register struct arpcom *ac;
	struct in_addr *addr;
#endif	NeXT
{
	register struct mbuf *m;
	register struct ether_arp *eat;
	register struct ether_arp *ea;
	struct sockaddr sa;

	if ((m = m_get(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof *ea;
#if	NEN >  0
#if	NeXT
	if (arpisen(ifp))
#else	NeXT
	if (arpisen(ac))
#endif	NeXT
	{
	    /*
	     *  3Mb ethernet.
	     *
	     *  The ARP packet is smaller by twice the difference in the
	     *  hardware lengths of the 3Mb ethernet from the 10Mb ethernet
	     *  (from the source and target hardware address fields).
	     */
	    eat = &arpentempl;
	    sa.sa_family = ENTYPE_ARP;	/* if_output will swap */
	    m->m_len = sizeof *ea - (2*(6-1));
        }
	else
#endif	NEN >  0
	{
	    /*
	     *  10Mb ethernet.
	     */
	    eat = &arpethertempl;
	    sa.sa_family = ETHERTYPE_ARP;	/* if_output will swap */
	    m->m_len = sizeof *ea;
	}
	/*
	 *  Set packet type and hardware destination in socket address.  For
	 *  both 3Mb and 10Mb ethernet, the type is a short word preceded by a
	 *  destination and source hardware address.  The destination address
	 *  is actually not the first byte of the 3Mb packet header but the
	 *  socket address is interpreted by the output routine and not passed
	 *  directly to the hardware so this doesn't matter.
	 */
	*((u_short *)&sa.sa_data[2*eat->arp_hln]) = sa.sa_family;
	bcopy((caddr_t)&eat->arp_sha[eat->arp_hln+eat->arp_pln],
	      (caddr_t)sa.sa_data, eat->arp_hln);
	/* reate pointer to beginning of ARP packet. */
 	m->m_off = MMAXOFF - m->m_len;
 	ea = mtod(m, struct ether_arp *);
	/* copy in the template packet. */
	bcopy((caddr_t)eat, (caddr_t)ea, (unsigned)m->m_len);
#ifdef	romp	
	if ((!bcmp((caddr_t)ac->ac_if.if_name, (caddr_t)"lan", 3)) &&
	    (ac->ac_if.if_flags & IFF_SNAP))
		ea->arp_hrd = htons(ARPHRD_802);
#endif	romp
	/* copy the source hardware address (from the if) */
#if	NeXT
	bcopy((caddr_t)&my_enaddr[0],
	      (caddr_t)&ea->arp_sha[0],
	      eat->arp_hln);
#else	NeXT
	bcopy((caddr_t)&ac->ac_enaddr[0],
	      (caddr_t)&ea->arp_sha[0],
	      eat->arp_hln);
#endif	NeXT
	/* copy the source protocol address (from the if) */
#if	NeXT
	bcopy((caddr_t)&my_ipaddr,
	      (caddr_t)&ea->arp_sha[eat->arp_hln],
	      eat->arp_pln);
#else	NeXT
	bcopy((caddr_t)&(ac->ac_ipaddr),
	      (caddr_t)&ea->arp_sha[eat->arp_hln],
	      eat->arp_pln);
#endif	NeXT
	/* keep the target hardware address (from the template) */
	/* copy the target protocol address (parameter) */
	bcopy((caddr_t)addr,
	      (caddr_t)&ea->arp_sha[2*(eat->arp_hln)+eat->arp_pln],
	      eat->arp_pln);
	/* byte swap the appopriate quantities depending on the ethernet type */
	if (eat == &arpethertempl)
	/*
	 *  10Mb ethernet - byte oriented
	 *
	 *  Swap all word quantities.
	 */
	{
	     ea->arp_hrd = htons(ea->arp_hrd);
	     ea->arp_pro = htons(ea->arp_pro);
	     ea->arp_op  = htons(ea->arp_op);
	}
        else
	/*
	 *  3Mb ethernet - word oriented
	 *
	 *  Swap all byte quantities.
	 */
	{
	    register int i;
	
	    *((u_short *)&ea->arp_hln) = htons(*((u_short *)&ea->arp_hln));
	    for (i=2*(eat->arp_pln+eat->arp_hln); i; i-=2)
	        *((u_short *)&ea->arp_sha[i-2]) = htons(*((u_short *)&ea->arp_sha[i-2]));
	}
	sa.sa_family = AF_UNSPEC;
#if	NeXT
	if_output_mbuf(ifp, m, &sa);
#else	NeXT
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa);
#endif	NeXT
}

int	useloopback = 1;	/* use loopback interface for local traffic */

/*
 * Resolve an IP address into an ethernet address.  If success, 
 * desten is filled in.  If there is no entry in arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desten has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 *
 * We do some (conservative) locking here at splimp, since
 * arptab is also altered from input interrupt service (ecintr/ilintr
 * calls arpinput when ETHERTYPE_ARP packets come in).
 */
#if	NeXT
arpresolve(
	   struct ifnet *ifp,
	   u_char my_enaddr[6],
	   struct in_addr my_ipaddr,
	   struct mbuf *m,
	   register struct in_addr *destip,
	   register u_char *desten,
	   int *usetrailers
	   )
#else	NeXT
arpresolve(ac, m, destip, desten, usetrailers)
	register struct arpcom *ac;
	struct mbuf *m;
	register struct in_addr *destip;
	register u_char *desten;
	int *usetrailers;
#endif	NeXT
{
	register struct arptab *at;
#if	NeXT
#else	NeXT
	register struct ifnet *ifp;
#endif	NeXT
	struct sockaddr_in sin;
	u_long lna;
	int s;

	*usetrailers = 0;

	if (in_broadcast(*destip)) {	/* broadcast address */
		register struct ether_arp *eat;

#if	NEN > 0
#if	NeXT
		if (arpisen(ifp))
#else	NeXT
		if (arpisen(ac))
#endif	NeXT
			eat = &arpentempl;
		else
#endif	NEN > 0
			eat = &arpethertempl;
		bcopy((caddr_t)&eat->arp_sha[eat->arp_hln+eat->arp_pln],
		      (caddr_t)desten,
		      eat->arp_hln);
		return (1);
	}
	lna = in_lnaof(*destip);
#if	NeXT
#else	NeXT
	ifp = &ac->ac_if;
#endif	NeXT
	/* if for us, use software loopback driver if up */
#if	NeXT
	if (destip->s_addr == my_ipaddr.s_addr) {
#else	NeXT
	if (destip->s_addr == ac->ac_ipaddr.s_addr) {
#endif	NeXT
		/*
		 * This test used to be
		 *	if (loif.if_flags & IFF_UP)
		 * It allowed local traffic to be forced
		 * through the hardware by configuring the loopback down.
		 * However, it causes problems during network configuration
		 * for boards that can't receive packets they send.
		 * It is now necessary to clear "useloopback"
		 * to force traffic out to the hardware.
		 */
		if (useloopback) {
#if	NeXT
			extern struct ifnet *loifp;
#endif	NeXT

			sin.sin_family = AF_INET;
			sin.sin_addr = *destip;
#if	NeXT
			(void) looutput(loifp, m, (struct sockaddr *)&sin);
#else	NeXT
			(void) looutput(&loif, m, (struct sockaddr *)&sin);
#endif	NeXT
			/*
			 * The packet has already been sent and freed.
			 */
			return (0);
		} else {
#if	NeXT
			bcopy((caddr_t)my_enaddr, (caddr_t)desten,
			    sizeof(my_enaddr));
#else	NeXT
			bcopy((caddr_t)ac->ac_enaddr, (caddr_t)desten,
			    sizeof(ac->ac_enaddr));
#endif	NeXT
			return (1);
		}
	}
	s = splimp();
	ARPTAB_LOOK(at, destip->s_addr, ifp);
	if (at == 0) {			/* not found */
#if	NeXT
		if (ifp->if_flags & IFF_NOARP) {
			bcopy((caddr_t)my_enaddr, (caddr_t)desten, 3);
#else	NeXT
		if (ac->ac_if.if_flags & IFF_NOARP) {
			bcopy((caddr_t)ac->ac_enaddr, (caddr_t)desten, 3);
#endif	NeXT
			desten[3] = (lna >> 16) & 0x7f;
			desten[4] = (lna >> 8) & 0xff;
			desten[5] = lna & 0xff;
			splx(s);
			return (1);
		} else {
			at = arptnew(ifp, destip);
			if (at == 0)
				panic("arpresolve: no free entry");
			at->at_hold = m;
#if	NeXT
			arpwhohas(ifp, my_enaddr, my_ipaddr, destip);
#else	NeXT
			arpwhohas(ac, destip);
#endif	NeXT
			splx(s);
			return (0);
		}
	}
	at->at_timer = 0;		/* restart the timer */
	if (at->at_flags & ATF_COM) {	/* entry IS complete */
		bcopy((caddr_t)at->at_enaddr, (caddr_t)desten,
		    sizeof(at->at_enaddr));
		if (at->at_flags & ATF_USETRAILERS)
			*usetrailers = 1;
		splx(s);
		return (1);
	}
	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet.  Replace the held mbuf with this
	 * latest one.
	 */
	if (at->at_hold)
		m_freem(at->at_hold);
	at->at_hold = m;
#if	NeXT
	arpwhohas(ifp, my_enaddr, my_ipaddr, destip);		/* ask again */
#else	NeXT
	arpwhohas(ac, destip);		/* ask again */
#endif	NeXT
	splx(s);
	return (0);
}

/*
 * Called from 10 Mb/s Ethernet interrupt handlers
 * when ether packet type ETHERTYPE_ARP
 * is received.  Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
#if	NeXT
arpinput(
	 struct ifnet *ifp,
	 u_char my_enaddr[6],
	 struct in_addr my_ipaddr,
	 struct mbuf *m
	 )
#else	NeXT
arpinput(ac, m)
	struct arpcom *ac;
	struct mbuf *m;
#endif	NeXT
{
	register struct arphdr *ar;
	register struct ether_arp *eat;
	short hrd;
	short pro;

#if	NeXT
	if (ifp->if_flags & IFF_NOARP)
		goto out;
#else	NeXT
	if (ac->ac_if.if_flags & IFF_NOARP)
		goto out;
	IF_ADJ(m);
#endif	NeXT
	if (m->m_len < sizeof(struct arphdr))
		goto out;
	ar = mtod(m, struct arphdr *);
#if	NEN > 0
#if	NeXT
	if (arpisen(ifp))
#else	NeXT
	if (arpisen(ac))
#endif	NeXT
	{
		eat = &arpentempl;
		hrd = ntohs(ARPHRD_XETHER);
		switch (ar->ar_pro)
		{
		    case ENTYPE_IP:
			pro = ETHERTYPE_IP;
			break;
		    case ENTYPE_IPTRAILERS:
			pro = ETHERTYPE_IPTRAILERS;
			break;
		    default:
			pro = 0;
			break;
		}
	}
	else
#endif	NEN > 0
	{
		eat = &arpethertempl;
		hrd = ARPHRD_ETHER;
		pro = ntohs(ar->ar_pro);
	}
	if (ntohs(ar->ar_hrd) != hrd)
		goto out;
	if (m->m_len < (sizeof (struct arphdr) +
			2*(eat->arp_hln+eat->arp_pln)))
		goto out;

	switch (pro) {

	case ETHERTYPE_IP:
	case ETHERTYPE_IPTRAILERS:
#if	NeXT
		in_arpinput(ifp, my_enaddr, my_ipaddr, m);
#else	NeXT
		in_arpinput(ac, m);
#endif	NeXT
		return;

	default:
		break;
	}
out:
	m_freem(m);
}

/*
 * ARP for Internet protocols on 10 Mb/s Ethernet.
 * Algorithm is that given in RFC 826.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 * We also handle negotiations for use of trailer protocol:
 * ARP replies for protocol type ETHERTYPE_TRAIL are sent
 * along with IP replies if we want trailers sent to us,
 * and also send them in response to IP replies.
 * This allows either end to announce the desire to receive
 * trailer packets.
 * We reply to requests for ETHERTYPE_TRAIL protocol as well,
 * but don't normally send requests.
 */
#if	NeXT
in_arpinput(
	    struct ifnet *ifp,
	    u_char my_enaddr[6],
	    struct in_addr myaddr,
	    struct mbuf *m
	    )
#else	NeXT
in_arpinput(ac, m)
	register struct arpcom *ac;
	struct mbuf *m;
#endif	NeXT
{
	register struct ether_arp *ea;
	register struct ether_arp *eat;
	register struct arptab *at;  /* same as "merge" flag */
	struct mbuf *mcopy = 0;
	struct sockaddr_in sin;
	struct sockaddr sa;
#if	NeXT
	struct in_addr isaddr, itaddr;
#else
	struct in_addr isaddr, itaddr, myaddr;
#endif
	int proto, op, s, completed = 0;

#if	NeXT
#else	NeXT
	myaddr = ac->ac_ipaddr;
#endif	NeXT
	ea = mtod(m, struct ether_arp *);
#if	NEN > 0
	if (arpisen(ac))
	{
		register int i;
	    
		eat = &arpentempl;
		sa.sa_family = ENTYPE_ARP;
		*((u_short *)&ea->arp_hln) = htons(*((u_short *)&ea->arp_hln));
		for (i=2*(eat->arp_hln+eat->arp_pln); i; i-=2)
			*((u_short *)(&ea->arp_sha[i-2])) = ntohs(*((u_short *)(&ea->arp_sha[i-2])));
		proto = (ea->arp_pro == ENTYPE_IP)?ETHERTYPE_IP:0;
		op    = ea->arp_op;
	}
	else
#endif	NEN > 0
	{
		eat = &arpethertempl;
		sa.sa_family = ETHERTYPE_ARP;
		proto = ntohs(ea->arp_pro);
		op    = ntohs(ea->arp_op);
	}
	if (ea->arp_hln != eat->arp_hln ||
	    ea->arp_pln != eat->arp_pln)
		goto out;
	isaddr.s_addr = ((struct in_addr *)&ea->arp_sha[eat->arp_hln])->s_addr;
	itaddr.s_addr = ((struct in_addr *)&ea->arp_sha[eat->arp_hln*2+eat->arp_pln])->s_addr;
#if	NeXT
	if (!bcmp((caddr_t)ea->arp_sha, (caddr_t)my_enaddr,
		  eat->arp_hln))
		goto out;	/* it's from me, ignore it. */
#else	NeXT
	if (!bcmp((caddr_t)ea->arp_sha, (caddr_t)ac->ac_enaddr,
	  eat->arp_hln))
		goto out;	/* it's from me, ignore it. */
#endif	NeXT
	if (!bcmp((caddr_t)ea->arp_sha, (caddr_t)etherbroadcastaddr,
	    sizeof (ea->arp_sha))) {
		log(LOG_ERR,
		    "arp: ether address is broadcast for IP address %x!\n",
		    ntohl(isaddr.s_addr));
		goto out;
	}
	if (isaddr.s_addr == myaddr.s_addr) {
		log(LOG_ERR, "%s: %s\n",
			"duplicate IP address!! sent from ethernet address",
			ether_sprintf(ea->arp_sha));
		itaddr = myaddr;
		if (op == ARPOP_REQUEST)
			goto reply;
		goto out;
	}
	s = splimp();
#if	NeXT
	ARPTAB_LOOK(at, isaddr.s_addr, ifp);
#else	NeXT
	ARPTAB_LOOK(at, isaddr.s_addr, &ac->ac_if);
#endif	NeXT
	if (at) {
		bcopy((caddr_t)ea->arp_sha, (caddr_t)at->at_enaddr,
		   eat->arp_hln);
		if (eat->arp_hln < sizeof at->at_enaddr)
			bzero((caddr_t)&at->at_enaddr[eat->arp_hln],
			      sizeof(at->at_enaddr)-eat->arp_hln);
		at->at_flags |= ATF_COM;
		if (at->at_hold) {
			sin.sin_family = AF_INET;
			sin.sin_addr = isaddr;
#if	NeXT
			if_output_mbuf(ifp,
			    at->at_hold, (struct sockaddr *)&sin);
#else	NeXT
			(*ac->ac_if.if_output)(&ac->ac_if, 
			    at->at_hold, (struct sockaddr *)&sin);
#endif	NeXT
			at->at_hold = 0;
		}
	}
	if (at == 0 && itaddr.s_addr == myaddr.s_addr) {
		/* ensure we have a table entry */
#if	NeXT
		at = arptnew(ifp, &isaddr);
#else	NeXT
		at = arptnew(&ac->ac_if, &isaddr);
#endif	NeXT
		bcopy((caddr_t)ea->arp_sha, (caddr_t)at->at_enaddr,
		   eat->arp_hln);
		if (eat->arp_hln < sizeof at->at_enaddr)
			bzero((caddr_t)&at->at_enaddr[eat->arp_hln],
			      sizeof(at->at_enaddr)-eat->arp_hln);
		at->at_flags |= ATF_COM;
	}
	(void) splx(s);
reply:
	switch (proto) {

	case ETHERTYPE_IPTRAILERS:
		/* partner says trailers are OK */
		if (at)
			at->at_flags |= ATF_USETRAILERS;
		/*
		 * Reply to request iff we want trailers.
		 */
#if	NeXT
		if (op != ARPOP_REQUEST || ifp->if_flags & IFF_NOTRAILERS)
			goto out;
#else	NeXT
		if (op != ARPOP_REQUEST || ac->ac_if.if_flags & IFF_NOTRAILERS)
			goto out;
#endif	NeXT
		break;

	case ETHERTYPE_IP:
		/*
		 * Reply if this is an IP request, or if we want to send
		 * a trailer response.
		 */
#if	NeXT
		if (op != ARPOP_REQUEST && ifp->if_flags & IFF_NOTRAILERS)
			goto out;
#else	NeXT
		if (op != ARPOP_REQUEST && ac->ac_if.if_flags & IFF_NOTRAILERS)
			goto out;
#endif	NeXT
	}
	if (itaddr.s_addr == myaddr.s_addr) {
		/* I am the target */
		bcopy((caddr_t)&ea->arp_sha[0],
		      (caddr_t)&ea->arp_sha[eat->arp_hln+eat->arp_pln],
		      eat->arp_hln);
#if	NeXT
		bcopy((caddr_t)&my_enaddr[0],
		      (caddr_t)&ea->arp_sha[0],
		      eat->arp_hln);
#else	NeXT
		bcopy((caddr_t)&ac->ac_enaddr[0],
		      (caddr_t)&ea->arp_sha[0],
		      eat->arp_hln);
#endif	NeXT
	} else {
#if	NeXT
		ARPTAB_LOOK(at, itaddr.s_addr, ifp);
#else	NeXT
		ARPTAB_LOOK(at, itaddr.s_addr, &ac->ac_if);
#endif	NeXT
		if (at == NULL || (at->at_flags & ATF_PUBL) == 0)
			goto out;
		bcopy((caddr_t)&ea->arp_sha[0],
		      (caddr_t)&ea->arp_sha[eat->arp_hln+eat->arp_pln],
		      eat->arp_hln);
#if	NeXT
		bcopy((caddr_t)&my_enaddr[0],
		      (caddr_t)&ea->arp_sha[0],
		      eat->arp_hln);
#else	NeXT
		bcopy((caddr_t)&ac->ac_enaddr[0],
		      (caddr_t)&ea->arp_sha[0],
		      eat->arp_hln);
#endif	NeXT
	}
	bcopy((caddr_t)&ea->arp_sha[eat->arp_hln],
	      (caddr_t)&ea->arp_sha[eat->arp_hln*2+eat->arp_pln],
	      eat->arp_pln);
	bcopy((caddr_t)&itaddr,
	      (caddr_t)&ea->arp_sha[eat->arp_hln],
	      eat->arp_pln);
	ea->arp_op = ARPOP_REPLY;
	bcopy((caddr_t)&ea->arp_sha[eat->arp_hln+eat->arp_pln],
	      (caddr_t)sa.sa_data,
	      eat->arp_hln);
	*((u_short *)&sa.sa_data[2*eat->arp_hln]) = sa.sa_family;
	/*
	 * If incoming packet was an IP reply,
	 * we are sending a reply for type IPTRAILERS.
	 * If we are sending a reply for type IP
	 * and we want to receive trailers,
	 * send a trailer reply as well.
	 */
	if (op == ARPOP_REPLY)
#if	NEN > 0
		ea->arp_pro = (eat == &arpethertempl)?htons(ETHERTYPE_IPTRAILERS):ENTYPE_IPTRAILERS;
#else	NEN > 0
		ea->arp_pro = htons(ETHERTYPE_IPTRAILERS);
#endif	NEN > 0
	else if (proto == ETHERTYPE_IP &&
#if	NeXT
	    (ifp->if_flags & IFF_NOTRAILERS) == 0)
#else	NeXT
	    (ac->ac_if.if_flags & IFF_NOTRAILERS) == 0)
#endif	NeXT
		mcopy = m_copy(m, 0, M_COPYALL);
	if (eat == &arpethertempl)
	/*
	 *  10Mb ethernet - byte oriented
	 *
	 *  Swap all word quantities.
	 */
	{
	     /* protocol remains as originally supplied */
	     ea->arp_op  = htons(ea->arp_op);
	}
        else
	/*
	 *  3Mb ethernet - word oriented
	 *
	 *  Swap all byte quantities.
	 */
	{
	    register int i;
	
	    *((u_short *)&ea->arp_hln) = htons(*((u_short *)&ea->arp_hln));
	    for (i=2*(eat->arp_pln+eat->arp_hln); i; i-=2)
	        *((u_short *)&ea->arp_sha[i-2]) = htons(*((u_short *)&ea->arp_sha[i-2]));
	}
	sa.sa_family = AF_UNSPEC;
#if	NeXT
	if_output_mbuf(ifp, m, &sa);
#else	NeXT
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa);
#endif	NeXT
	if (mcopy) {
		ea = mtod(mcopy, struct ether_arp *);
#if	NEN > 0
		ea->arp_pro = (eat == &arpethertempl)?htons(ETHERTYPE_IPTRAILERS):ENTYPE_IPTRAILERS;
#else	NEN > 0
		ea->arp_pro = htons(ETHERTYPE_IPTRAILERS);
#endif	NEN > 0
#if	NeXT
		if_output_mbuf(ifp, mcopy, &sa);
#else 	NeXT
		(*ac->ac_if.if_output)(&ac->ac_if, mcopy, &sa);
#endif	NeXT
	}
	return;
out:
	m_freem(m);
	return;
}

/*
 * Free an arptab entry.
 */
arptfree(at)
	register struct arptab *at;
{
	int s = splimp();

	if (at->at_hold)
		m_freem(at->at_hold);
	at->at_hold = 0;
	at->at_timer = at->at_flags = 0;
	at->at_iaddr.s_addr = 0;
	splx(s);
}

/*
 * Enter a new address in arptab, pushing out the oldest entry 
 * from the bucket if there is no room.
 * This always succeeds since no bucket can be completely filled
 * with permanent entries (except from arpioctl when testing whether
 * another permanent entry will fit).
 * MUST BE CALLED AT SPLIMP.
 */
struct arptab * 
arptnew(ifp, addr)
	struct ifnet *ifp;
	struct in_addr *addr;
{
	register n;
	int oldest = -1;
	register struct arptab *at, *ato = NULL;
	static int first = 1;

	if (first) {
		first = 0;
		timeout(arptimer, (caddr_t)0, hz);
	}
	at = &arptab[ARPTAB_HASH(addr->s_addr) * ARPTAB_BSIZ];
	for (n = 0; n < ARPTAB_BSIZ; n++,at++) {
		if (at->at_flags == 0)
			goto out;	 /* found an empty entry */
		if (at->at_flags & ATF_PERM)
			continue;
#if	SUN_RPC
		if (ato == NULL || (int)at->at_timer > oldest) {
#else	SUN_RPC
		if ((int)at->at_timer > oldest) {
#endif	SUN_RPC
			oldest = at->at_timer;
			ato = at;
		}
	}
	if (ato == NULL)
		return (NULL);
	at = ato;
	arptfree(at);
out:
	at->at_iaddr = *addr;
	at->at_flags = ATF_INUSE;
	at->at_if = ifp;
	return (at);
}

arpioctl(cmd, data)
	int cmd;
	caddr_t data;
{
	register struct arpreq *ar = (struct arpreq *)data;
	register struct arptab *at;
	register struct sockaddr_in *sin;
	int s;
	struct ifaddr *ifa;

	if (ar->arp_pa.sa_family != AF_INET ||
	    ar->arp_ha.sa_family != AF_UNSPEC)
		return (EAFNOSUPPORT);
	sin = (struct sockaddr_in *)&ar->arp_pa;
	s = splimp();
	/*
	 * Look for first arptable entry to do ioctl on.
	 */
	ARPTAB_LOOK(at, sin->sin_addr.s_addr, 0);
	if (at == NULL) {		/* not found */
		if (cmd != SIOCSARP) {
			splx(s);
			return (ENXIO);
		}
		if ((ifa = ifa_ifwithnet(&ar->arp_pa)) == NULL) {
			splx(s);
			return (ENETUNREACH);
		}
	}
	switch (cmd) {

	case SIOCSARP:		/* set entry */
		if (at == NULL) {
			at = arptnew(ifa->ifa_ifp, &sin->sin_addr);
			if (at == NULL) {
				splx(s);
				return (EADDRNOTAVAIL);
			}
			if (ar->arp_flags & ATF_PERM) {
			/* never make all entries in a bucket permanent */
				register struct arptab *tat;
				
				/* try to re-allocate */
				tat = arptnew(at->at_if, &sin->sin_addr);
				if (tat == NULL) {
					arptfree(at);
					splx(s);
					return (EADDRNOTAVAIL);
				}
				arptfree(tat);
			}
		}
		bcopy((caddr_t)ar->arp_ha.sa_data, (caddr_t)at->at_enaddr,
		    sizeof(at->at_enaddr));
		at->at_flags = ATF_COM | ATF_INUSE |
			(ar->arp_flags & (ATF_PERM|ATF_PUBL|ATF_USETRAILERS));
		at->at_timer = 0;
		break;

	case SIOCDARP:		/* delete entry */
		arptfree(at);
		break;

	case SIOCGARP:		/* get entry */
		bcopy((caddr_t)at->at_enaddr, (caddr_t)ar->arp_ha.sa_data,
		    sizeof(at->at_enaddr));
		ar->arp_flags = at->at_flags;
		break;
	}
	splx(s);
	return (0);
}

int revarp = 1;

#ifdef	notdef
/*
 * This is Sun's RARP client support.  I left the code here in case
 * you want to use it instead of BOOTP
 */
struct in_addr myaddr;

revarp_myaddr(ifp)
	register struct ifnet *ifp;
{
	register struct sockaddr_in *sin;
	struct ifreq ifr;
	int s;

	/*
	 * We need to give the interface a temporary address just
	 * so it gets initialized. Hopefully, the address won't get used.
	 * Also force trailers to be off on this interface.
	 */
	bzero((caddr_t)&ifr, sizeof(ifr));
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	sin->sin_family = AF_INET;
	sin->sin_addr = in_makeaddr(INADDR_ANY, 
		(u_long) 0xFFFFFF );
	ifp->if_flags |= IFF_NOTRAILERS | IFF_BROADCAST;
	if (in_control((struct socket *)0, SIOCSIFADDR, (caddr_t)&ifr, ifp))
		printf("revarp: can't set temp inet addr\n");
	if (revarp) {
		myaddr.s_addr = 0;
		revarp_start(ifp);
		s = splimp();
		while (myaddr.s_addr == 0)
			(void) sleep((caddr_t)&myaddr, PZERO-1);
		(void) splx(s);
		sin->sin_addr = myaddr;
		if (in_control((struct socket*)0, SIOCSIFADDR, (caddr_t)&ifr, ifp))
			printf("revarp: can't set perm inet addr\n");
	}
}

revarp_start(ifp)
	register struct ifnet *ifp;
{
	register struct mbuf *m;
	register struct ether_arp *ea;
	register struct ether_header *eh;
	static int retries = 0;
	struct ether_addr myether;
	struct sockaddr sa;

	if (myaddr.s_addr != 0) {
		if (retries >= 2)
			printf("Found Internet address %x\n", myaddr.s_addr);
		retries = 0;
		return;
	}
	(void) localetheraddr((struct ether_addr *)NULL, &myether);
	if (++retries == 2) {
		printf("revarp: Requesting Internet address for %s\n",
		    ether_sprintf(&myether));
	}
	if ((m = m_get(M_DONTWAIT, MT_DATA)) == NULL)
		panic("revarp: no mbufs");
	m->m_len = sizeof(struct ether_arp);
	m->m_off = MMAXOFF - m->m_len;
	ea = mtod(m, struct ether_arp *);
	bzero((caddr_t)ea, sizeof (*ea));

	sa.sa_family = AF_UNSPEC;
	eh = (struct ether_header *)sa.sa_data;
	bcopy((caddr_t)etherbroadcastaddr,(caddr_t)eh->ether_dhost,
			sizeof(eh->ether_dhost));
	bcopy((caddr_t)&myether,(caddr_t)eh->ether_shost,sizeof(eh->ether_shost));
	eh->ether_type = ETHERTYPE_REVARP;

	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(REVARP_REQUEST);
	bcopy((caddr_t)&myether,(caddr_t)ea->arp_sha,sizeof(ea->arp_sha));
	bcopy((caddr_t)myether,(caddr_t)ea->arp_tha,sizeof(ea->arp_tha));
#if	NeXT
	if_output_mbuf(ifp, m, &sa);
#else	NeXT
	(*ifp->if_output)(ifp, m, &sa);
#endif	NeXT
	timeout(revarp_start, (caddr_t)ifp, 3*hz);
}
#endif	notdef

int revarpdebug = 0;
  
/*
 * Reverse-ARP input
 * Server side answers requests based on the arp table
 */
revarpinput(ac, m)
        register struct arpcom *ac;
        struct mbuf *m;
{
        register struct ether_arp *ea;
	register struct arptab *at = 0;
        register struct ether_header *eh;
	struct ether_addr myether;
	struct ifnet *ifp;
	struct ifaddr *ifa;
        struct sockaddr sa;

	IF_ADJ(m);
	ea = mtod(m, struct ether_arp *);
	if (m->m_len < sizeof *ea)
		goto out;
	if (ac->ac_if.if_flags & IFF_NOARP)
		goto out;
	if (ntohs(ea->arp_pro) != ETHERTYPE_IP)
		goto out;
	if(!revarp)
		goto out;
	switch(ntohs(ea->arp_op)) {
#ifdef	notdef
	case REVARP_REPLY:
		(void) localetheraddr((struct ether_addr *)NULL, &myether);
		if (bcmp((caddr_t)ea->arp_tha, (caddr_t)&myether, 6) == 0) {
			bcopy((caddr_t)ea->arp_tpa, (caddr_t)&myaddr, sizeof(myaddr));
			wakeup((caddr_t)&myaddr);
		}
		break;
#endif	notdef
	case REVARP_REQUEST:
 		for (at = arptab ; at < &arptab[ARPTAB_SIZE] ; at++) {
                        if (at->at_flags & ATF_PERM &&
                            !bcmp((caddr_t)at->at_enaddr,
                            (caddr_t)ea->arp_tha, 6))
				/* FIXME: We might want to check at->at_if */
                                break;
                }
                if (at < &arptab[ARPTAB_SIZE]) {
                        /* found a match, send it back */
                        eh = (struct ether_header *)sa.sa_data;
                        bcopy(ea->arp_sha, eh->ether_dhost, 
				sizeof(ea->arp_sha));
                        bcopy((caddr_t)(&at->at_iaddr), ea->arp_tpa,
				sizeof(at->at_iaddr));
			/* search for interface address to use */
			ifp = &ac->ac_if;
			for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
				if (ifa->ifa_ifp == ifp) {
				    bcopy((caddr_t)&((struct sockaddr_in *)&ifa->ifa_addr)->sin_addr,
					ea->arp_spa, sizeof(ea->arp_spa));
				    break;
				}
			}
			if (ifa == 0) {
				if (revarpdebug)
				    printf("revarp: can't find ifaddr\n");
				goto out;
			}
			bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->arp_sha,
			    sizeof(ea->arp_sha));
			bcopy((caddr_t)ac->ac_enaddr, (caddr_t)eh->ether_shost,
			    sizeof(ea->arp_sha));
                        eh->ether_type = ETHERTYPE_REVARP;
                        ea->arp_op = htons(REVARP_REPLY);
                        sa.sa_family = AF_UNSPEC;
                        if (revarpdebug) {
                                printf("revarp reply to %X from %X\n",
					ntohl(*(u_long *)ea->arp_tpa),
					ntohl(*(u_long *)ea->arp_spa));
			}
                        (*ac->ac_if.if_output)(&ac->ac_if, m, &sa);
                        return;
		}
		break;

	default:
		break;
	}
out:
        m_freem(m);
        return;
}

localetheraddr(hint, result)
	struct ether_addr *hint, *result;
{
	static int found = 0;
	static struct ether_addr addr;

	if (!found) {
		found = 1;
		if (hint == NULL)
			return (0);
		addr = *hint;
		printf("Ethernet address = %s\n", ether_sprintf(&addr) );
	}
	if (result != NULL)
		*result = addr;
	return (1);
}

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
char *
ether_sprintf(ap)
	register u_char *ap;
{
	register i;
	static char etherbuf[18];
	register char *cp = etherbuf;
	static char digits[] = "0123456789abcdef";

	for (i = 0; i < 6; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (etherbuf);
}
