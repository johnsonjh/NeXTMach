/*	@(#)tftp.c	1.0	10/06/86	(c) 1986 NeXT	*/

#define	DEBUG	0

#if	DEBUG
#define	dbug1	printf
#define	dbug2	printf
#define	dbug4	printf
#define	dbug8	printf
#else	DEBUG
#define	dbug1
#define	dbug2
#define	dbug4
#define	dbug8
#endif	DEBUG

#import <sys/types.h>
#import <sys/param.h>
#import <sys/time.h>
#import <sys/vnode.h>
#import <ufs/inode.h>
#import <ufs/fs.h>
#import <sys/dir.h>
#import <mon/tftp.h>
#import <mon/monparam.h>
#import <mon/global.h>
#import <mon/nvram.h>
#import <arpa/tftp.h>
#import <nextdev/dma.h>
#import <stand/saio.h>

u_char etherbcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
struct tftp_softc tftp_softc;

enopen(io)
	struct iob *io;
{
	register struct tftp_softc *ts = &tftp_softc;
	register u_char *e;

	io->i_secsize = 1024;
	bcopy (etherbcastaddr, ts->ts_serveretheraddr, 6);
	e = (u_char*) en_open();
	bcopy (e, ts->ts_clientetheraddr, 6);
	return (0);
}

enclose()
{
	return (en_close());
}

enstrategy (io, dir)
	struct iob *io;
{
	if (dir == READ)
		return (tftp_boot (io->i_ma, io->i_cc, io->i_filename));
}

tftp_boot(ma, cc, filename)
	char *filename;
{
	register struct tftp_softc *ts = &tftp_softc;
	register struct tftp_packet *tx = &ts->ts_xmit;
	register struct tftp_packet *tr = &ts->ts_recv;
	register struct bootp_packet *tb = &ts->ts_bootpr;
	char *bp, *lp;
	register int i;
	register int len;
	int variation = 0, p_d = 0;
	int warned = 0, resid, size = cc, total = 0;
	char *index();
	struct udpiphdr *ui;
	struct mon_global *mg = restore_mg();

	if (ts->ts_clientipaddr.s_addr == 0) {
		if (bootp_query(mg, filename, 0) == 0)
			return (0);
		if (*filename == 0) {
			/*
			 * Start over, because need to check whether or
			 * not server actually has file. Any server can
			 * respond to request for "", but only servers
			 * who actually have a non-empty file will respond
			 * to the request.
			 */
			bcopy (etherbcastaddr, ts->ts_serveretheraddr, 6);
			if (bootp_query(mg, tb->bp_bootp.bp_file, 1) == 0) {
				return (0);
			}
		}
		printf ("Booting %s from %s\n", tb->bp_bootp.bp_file,
			tb->bp_bootp.bp_sname);
	tx->tp_ip.ip_v = IPVERSION;
	tx->tp_ip.ip_hl = sizeof (struct ip) >> 2;
	tx->tp_ip.ip_ttl = MAXTTL;
	tx->tp_ip.ip_p = IPPROTO_UDP;
	tx->tp_ip.ip_src = ts->ts_clientipaddr;
	tx->tp_ip.ip_dst = tb->bp_bootp.bp_siaddr;
	tx->tp_udp.uh_sport = IPPORT_RESERVED +
		(mon_time() & (IPPORT_RESERVED-1));
	tx->tp_udp.uh_dport = IPPORT_TFTP;
	ts->ts_bno = 1;
	tx->tp_hdr.th_func = RRQ;

	/* Copy filename given by bootp server here */
	bp = tx->tp_hdr.th_buf;
	lp = (char *) tb->bp_bootp.bp_file;
	while (*lp != 0)
		*bp++ = *lp++;
	*bp++ = 0;
	strcpy (bp, "octet");
	bp += 6;
	tx->tp_udp.uh_ulen = sizeof (struct udphdr) +
		sizeof (tx->tp_hdr.th_func) + (bp - tx->tp_hdr.th_buf);
	tx->tp_ip.ip_len = sizeof (struct ip) + tx->tp_udp.uh_ulen;
	tx->tp_udp.uh_sum = 0;	/* FIXME: figure out proper checksum? */

	/*
 	 * timeout/retry policy:
	 * Two different strategies are used.  Before a server has been
	 * locked to we retransmit at a random time in a binary window
	 * which is doubled each transmission (up to a maximum).  This
	 * avoids flooding the net after, say, a power failure when all
	 * machines are trying to reboot simultaneously.  After a connection
	 * is established the binary window is reset and only doubled after
	 * a number of retries fail at each window value.
	 */
	ts->timemask = TFTP_MIN_BACKOFF;
	ts->timeout = 0;
	ts->lockon = 0;
	ts->time = 0;	/* force immediate transmission */
	}
	
	for (ts->retry = 0, ts->xcount = 0; ts->xcount < TFTP_XRETRY;) {
		if (mon_time() - ts->time >= ts->timeout) {
retrans:
			if (ts->time)
				printf ("T");
			ts->time = mon_time();
			dbug4 ("%d", tx->tp_hdr.th_bno);
			tftp_ipoutput (mg, ts, IPIO_TFTP, ts->retry);
			if (ts->lockon == 0 || ts->retry > TFTP_RETRY) {
				ts->xcount++;
				if (ts->timemask < TFTP_MAX_BACKOFF)
					ts->timemask = (ts->timemask<<1) | 1;
				ts->timeout = ts->timemask & mon_time();
			} else 
				ts->retry++;
		}
		len = tftp_ipinput (mg, ts, IPIO_TFTP);
		if (len == 0)
			continue;
		ts->timemask = TFTP_MIN_BACKOFF;
		if (len < TFTP_HDRSIZE) {
#if	DEBUG
			printf ("?");
#if 0
			if (si->si_unit & 8) {
				printf ("et 0x%x dst %s ",
					tr->tp_ether.ether_type,
					inet_ntoa (tr->tp_ip.ip_dst.s_addr));
				printf ("client %s\n", inet_ntoa
					(ts->ts_clientipaddr.s_addr));
			}

			if (si->si_unit & 16) {
				for (i = 0; i < 100; i += 4) {
					if (((i/4) % 12) == 0)
						printf ("\n%08x: ", (int)tr+i);
					printf ("%08x ", *(int*)((int)tr+i));
				}
				en_txstat(si);
			}
#endif
#endif	DEBUG
			continue;
		}
		if (tr->tp_ip.ip_p != IPPROTO_UDP ||
		    tr->tp_udp.uh_dport != tx->tp_udp.uh_sport) {
#if	DEBUG
			printf ("P");
#if 0
			if (si->si_unit & 8) {
				ppkt (tr);
				for (i = 0; i < 0x100; i += 4) {
					if (((i/4) % 12) == 0)
						printf ("\n%08x: ",
							(int)tr+i);
					printf ("%08x ", *(int*)((int)tr+i));
				}
				en_txstat(si);
			}
#endif
#endif	DEBUG
			continue;
		}
		if (ts->lockon &&
		    (tx->tp_ip.ip_dst.s_addr != tr->tp_ip.ip_src.s_addr)) {
			continue;
		}
		if (tr->tp_hdr.th_func == ERROR) {
			printf ("\ntftp: %s\n", &tr->tp_hdr.th_error + 1);
			return (0);
		}
		if (tr->tp_hdr.th_func != DATA) {
			continue;
		}
		if (tr->tp_hdr.th_bno != ts->ts_bno) {
			tx->tp_hdr.th_bno = ts->ts_bno - 1;
			ts->time = 0;
#if	DEBUG
			printf ("S");
#if 0
			if (si->si_unit & 8) {
				ppkt (tr);
				for (i = 0; i < 0x100; i += 4) {
					if (((i/4) % 12) == 0)
						printf ("\n%08x: ", (int)tr+i);
					printf ("%08x ", *(int*)((int)tr+i));
				}
				en_txstat(si);
			}
#endif
#endif	DEBUG
			/* drain delayed packets */
			for (i = 0; i < 1000; i++)
				tftp_ipinput (mg, ts, IPIO_TFTP);
			continue;
		}
#ifdef	notdef
		if (tr->tp_udp.uh_sum) {
			ui = (struct udpiphdr*) &tr->tp_ip;
			ui->ui_next = ui->ui_prev = 0;
			ui->ui_x1 = 0;
			ui->ui_len = ui->ui_ulen;
			if (cksum (ui, ui->ui_ulen + sizeof (struct ip))) {
				printf ("C");
				/* flush input packets before retrans */
				do {
					if (resid = tftp_ipinput (mg, ts,
					    IPIO_TFTP))
						printf ("$");
				} while (resid);

				goto retrans;
			}
		} else
		if (warned == 0) {
			printf ("Warning: server not generating UDP checksums\n");
			warned = 1;
		}
#endif	notdef
		printf (".");
		dbug4 ("%d", tr->tp_hdr.th_bno);
		ts->xcount = 0;
		ts->retry = 0;

		/* we have an in-sequence DATA packet */
		if (ts->ts_bno == 1) {	/* lock on to server and port */
			tx->tp_udp.uh_dport = tr->tp_udp.uh_sport;

			/* revert to original backoff once locked on */
			ts->timemask = TFTP_MIN_BACKOFF;
			ts->lockon = 1;
		}
		len = tr->tp_udp.uh_ulen - (sizeof (struct udphdr) +
			sizeof (struct tftp_hdr));
		bcopy (tr->tp_buf, ma, len);
		ma += len;
		size -= len;
		total += len;

		/* flush input packets before ACK */
		do {
			if (resid = tftp_ipinput (mg, ts, IPIO_TFTP))
				dbug2 ("#");
		} while (resid);

		/* send ACK */
		ts->xcount = 0;
		ts->retry = 0;
		tx->tp_hdr.th_func = ACK;
		tx->tp_hdr.th_bno = ts->ts_bno++;
		tx->tp_udp.uh_ulen = sizeof (struct udphdr) +
			sizeof (struct tftp_hdr);
		tx->tp_ip.ip_len = sizeof (struct ip) + tx->tp_udp.uh_ulen;
		ts->time = mon_time();
		dbug2 ("A");
		dbug4 ("%d", tx->tp_hdr.th_bno);
		tftp_ipoutput (mg, ts, IPIO_TFTP, 0);
		if (len < TFTP_BSIZE || size <= 0)
			return (total);
	}
	printf ("tftp: timeout\n");
	return (0);
}

bootp_query (mg, filename, silent)
	register struct mon_global *mg;
	char *filename;
{
	register struct tftp_softc *ts = &tftp_softc;
	register struct bootp_packet *tb = &ts->ts_bootpx;
	register struct bootp_packet *tr = &ts->ts_bootpr;
	register struct nvram_info *ni = &mg->mg_nvram;
	char *bp, *lp = filename;
	int time, retry, variation = 0, len, timeout, timemask;
	extern char *index();

	/* Choose a transaction id based on hardware address and clock */
	ts->ts_xid = ts->ts_clientetheraddr[5] ^ mon_time();
	tb->bp_ip.ip_v = IPVERSION;
	tb->bp_ip.ip_hl = sizeof (struct ip) >> 2;
	tb->bp_ip.ip_id++;
	tb->bp_ip.ip_ttl = MAXTTL;
	tb->bp_ip.ip_p = IPPROTO_UDP;
	tb->bp_ip.ip_src.s_addr = 0;
	tb->bp_ip.ip_dst.s_addr = INADDR_BROADCAST;
	tb->bp_udp.uh_sport = htons(IPPORT_BOOTPC);
	tb->bp_udp.uh_dport = htons(IPPORT_BOOTPS);
	tb->bp_udp.uh_sum = 0;
	bzero(&tb->bp_bootp, sizeof tb->bp_bootp);
	tb->bp_bootp.bp_op = BOOTREQUEST;
	tb->bp_bootp.bp_htype = ARPHRD_ETHER;
	tb->bp_bootp.bp_hlen = 6;
	tb->bp_bootp.bp_xid = ts->ts_xid;
	tb->bp_bootp.bp_ciaddr.s_addr = 0;
	if (!silent)
		printf("Requesting BOOTP information");
	if (bp = index (lp, ':')) {
		*bp = 0;
		strncpy (tb->bp_bootp.bp_sname, lp, 64);
		if (!silent)
			printf (" from %s", lp);
		lp = bp + 1;
	}
	bcopy((caddr_t)ts->ts_clientetheraddr, tb->bp_bootp.bp_chaddr, 6);
	bp = (char *) tb->bp_bootp.bp_file;
	while (*lp != 0 && *lp != '-' && *lp != ' ')
		*bp++ = *lp++;
	*bp++ = 0;
	tb->bp_udp.uh_ulen = htons(sizeof tb->bp_udp + sizeof tb->bp_bootp);
	tb->bp_ip.ip_len = sizeof (struct ip) + tb->bp_udp.uh_ulen;

	/*
 	 * timeout/retry policy:
	 * Before a server has responded we retransmit at a random time
	 * in a binary window which is doubled each transmission.  This
	 * avoids flooding the net after, say, a power failure when all
	 * machines are trying to reboot simultaneously.
	 */
	timemask = BOOTP_MIN_BACKOFF;
	timeout = 0;
	time = 0;	/* force immediate transmission */

	for (retry = 0; retry < BOOTP_RETRY;) {
	if (mon_time() - time >= timeout) {
		time = mon_time();
		tftp_ipoutput (mg, ts, IPIO_BOOTP, retry);
		if (!silent)
			printf (".");
		if (timemask < BOOTP_MAX_BACKOFF)
			timemask = (timemask << 1) | 1;
		timeout = timemask & mon_time();
		retry++;
	}
	len = tftp_ipinput (mg, ts, IPIO_BOOTP);
	if (len == 0 || len < BOOTP_PKTSIZE)
		continue;
	    if (tr->bp_ip.ip_p != IPPROTO_UDP ||
		tr->bp_udp.uh_dport != tb->bp_udp.uh_sport)
		    continue;
	    if (tr->bp_udp.uh_dport != htons(IPPORT_BOOTPC))
	      continue;
	    if (tr->bp_bootp.bp_xid != ts->ts_xid || 
		tr->bp_bootp.bp_op != BOOTREPLY ||
		bcmp(tr->bp_bootp.bp_chaddr, ts->ts_clientetheraddr,
		     sizeof ts->ts_clientetheraddr) != 0)
	      continue;

	    /* it's for us! */
	    if (!silent)
	    	printf("[OK]\n");
	    bcopy (tr->bp_ether.ether_shost, ts->ts_serveretheraddr, 6);
	    ts->ts_clientipaddr = tr->bp_bootp.bp_yiaddr;

	    /* pass along the filename we're booting from */
	    mg->mg_boot_file = (char*) tr->bp_bootp.bp_file;
	    return(1);
	}
	printf("[timeout]\n");
	return(0);
}

arp_input (mg, ts, packet)
     register struct mon_global *mg;
     register struct tftp_softc *ts;
     caddr_t packet;
{
    register struct ether_header *eh = (struct ether_header *) packet;
    register struct ether_arp *arp = (struct ether_arp *) (packet+sizeof(*eh));
    int len;

    if (arp->arp_op == ARPOP_REQUEST &&	arp->arp_hrd == ARPHRD_ETHER &&
	arp->arp_pro == ETHERTYPE_IP &&
	((struct in_addr*)arp->arp_tpa)->s_addr ==
	ts->ts_clientipaddr.s_addr) {
	    /* It's our arp request */
	    bcopy (arp->arp_sha, arp->arp_tha, 6);
	    bcopy (arp->arp_spa, arp->arp_tpa, 4);
	    bcopy (ts->ts_clientetheraddr, arp->arp_sha, 6);
	    *(struct in_addr*) arp->arp_spa = ts->ts_clientipaddr;
	    arp->arp_op = ARPOP_REPLY;
	    bcopy (arp->arp_tha, eh->ether_dhost, 6);
	    bcopy (ts->ts_clientetheraddr, eh->ether_shost, 6);
	    len = sizeof (struct ether_header) + sizeof (struct ether_arp);
	    if (len < ETHERMIN + sizeof (struct ether_header))
	      len = ETHERMIN + sizeof (struct ether_header);
	    return en_write ((caddr_t)eh, len, 0);
	}
    return(-1);
}

tftp_ipoutput (mg, ts, type, timeout)
	register struct mon_global *mg;
	register struct tftp_softc *ts;
	int type;
{
	register struct tftp_packet *tp = &ts->ts_xmit;
	register struct bootp_packet *bp = &ts->ts_bootpx;
	register struct ether_header *eh;
	register struct ip *ip;
	register short len;
	caddr_t pp;

	switch (type) {
	  case IPIO_TFTP:
	    eh = &tp->tp_ether;
	    ip = &tp->tp_ip;
	    len = tp->tp_ip.ip_len + sizeof (struct ether_header);
	    pp = (caddr_t) tp;
	    break;
	  case IPIO_BOOTP:
	    eh = &bp->bp_ether;
	    ip = &bp->bp_ip;
	    len = bp->bp_ip.ip_len + sizeof (struct ether_header);
	    pp = (caddr_t) bp;
	    break;
	  default:
	    return(-1);
	}

	eh->ether_type = ETHERTYPE_IP;
	bcopy (ts->ts_clientetheraddr, eh->ether_shost, 6);
	bcopy (ts->ts_serveretheraddr, eh->ether_dhost, 6);
	ip->ip_sum = 0;
	ip->ip_sum = ~cksum ((caddr_t)ip, sizeof (struct ip));
	if (len < ETHERMIN + sizeof (struct ether_header))
		len = ETHERMIN + sizeof (struct ether_header);
	return en_write (pp, len, timeout);
}

tftp_ipinput (mg, ts, type)
     register struct mon_global *mg;
     register struct tftp_softc *ts;
     int type;
{
    register short len;
    register struct tftp_packet *tp = &ts->ts_recv;
    register struct bootp_packet *bp = &ts->ts_bootpr;
    register struct ether_header *eh;
    register struct ip *ip;
    struct ether_arp *ap;

    switch (type) {
      case IPIO_TFTP:
	eh = &tp->tp_ether;
	ip = &tp->tp_ip;
	len = en_poll ((caddr_t)tp, ts, sizeof (*tp));
	break;
      case IPIO_BOOTP:
	eh = &bp->bp_ether;
	ip = &bp->bp_ip;
	len = en_poll ((caddr_t)bp, ts, sizeof (*bp));
	break;

      default:
	return(0);
    }

    if (len == 0)
	return (0);
    if (ts->ts_clientipaddr.s_addr) { /* We know our address */
	if (bcmp (eh->ether_dhost, etherbcastaddr, 6) == 0)
		return (0);
	/* First check for incoming ARP request's */
	if (eh->ether_type == ETHERTYPE_ARP) {
	    arp_input(mg, ts, eh);
	    return(0);	/* We don't want to see this higher up */
	}
	/* Not ARP, is it IP for us? */
	if (len >= sizeof (struct ether_header) + sizeof (struct ip) &&
	    eh->ether_type == ETHERTYPE_IP &&
	    ip->ip_dst.s_addr == ts->ts_clientipaddr.s_addr) {
	  return (len);
	}
	return (1);
    }

    /* We don't know our IP address yet, so accept packets aimed at us */
    if (len >= sizeof (struct ether_header) + sizeof (struct ip) &&
	eh->ether_type == ETHERTYPE_IP &&
	! bcmp(eh->ether_dhost, ts->ts_clientetheraddr, 6))
	    return(len);
    return (0);
}

#define	EVENTC_H	(volatile u_char*) (P_EVENTC + 1)
#define	EVENTC_M	(volatile u_char*) (P_EVENTC + 2)
#define	EVENTC_L	(volatile u_char*) (P_EVENTC + 3)
#define EVENT_HIGHBIT 0x80000
#define EVENT_MASK 0xfffff

/* Return 32 bit representation of event counter */
unsigned int event_get(void)
{
	register struct mon_global *mg = restore_mg();
	u_int high, low;

	high = mg->event_high;
	low = *(mg->eventc_latch);	/* load the latch from the event counter */
	low = (*EVENTC_H << 16) | (*EVENTC_M << 8) | *EVENTC_L;
	low &= EVENT_MASK;
	if ((high ^ low) & EVENT_HIGHBIT)
		high += EVENT_HIGHBIT;
	mg->event_high = high;
	return (high | low);
}

/* Return the number of microseconds between now and passed event */
static inline unsigned int event_delta(unsigned int prev_time)
{
	return (event_get() - prev_time);
}

/*
 *	Delay for the specified number of microseconds.
 */
void delay(unsigned int n)
{
	register int d = event_get();

	while (event_delta(d) < (n+1))
		continue;
}

/*
 * Returns the value of a free running, millisecond resolution clock.
 */
mon_time() {
	register struct mon_global *mg = restore_mg();

	return (event_get() / 1000);
}

