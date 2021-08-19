/*
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * Copyright (c) 1982 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)if_loop.c	6.7 (Berkeley) 9/16/85
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/mbuf.h>
#import <sys/socket.h>
#import <sys/errno.h>
#import <sys/ioctl.h>

#import <machine/spl.h>

#ifdef	BBNNET
#define INET
#endif
#import <net/if.h>
#import <net/netisr.h>
#import <net/route.h>

#ifdef	INET
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/ip_var.h>
#endif

#ifdef vax
#import <vax/mtpr.h>
#endif

#ifdef NS
#import <netns/ns.h>
#import <netns/ns_if.h>
#endif

#define	LOMTU	(1024+512)

struct	ifnet loif;
int	looutput(), loioctl();

loattach()
{
	register struct ifnet *ifp = &loif;

	ifp->if_name = "lo";
	ifp->if_mtu = LOMTU;
	ifp->if_ioctl = loioctl;
	ifp->if_output = looutput;
	if_attach(ifp);
}

looutput(ifp, m0, dst)
	struct ifnet *ifp;
	register struct mbuf *m0;
	struct sockaddr *dst;
{
	int s;
	register struct ifqueue *ifq;
	struct mbuf *m;

	/*
	 * Place interface pointer before the data
	 * for the receiving protocol.
	 */
	if (m0->m_off <= MMAXOFF &&
	    m0->m_off >= MMINOFF + sizeof(struct ifnet *)) {
		m0->m_off -= sizeof(struct ifnet *);
		m0->m_len += sizeof(struct ifnet *);
	} else {
		MGET(m, M_DONTWAIT, MT_HEADER);
		if (m == (struct mbuf *)0)
			return (ENOBUFS);
		m->m_off = MMINOFF;
		m->m_len = sizeof(struct ifnet *);
		m->m_next = m0;
		m0 = m;
	}
	*(mtod(m0, struct ifnet **)) = ifp;
	s = splimp();
	ifp->if_opackets++;
	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		ifq = &ipintrq;
		if (IF_QFULL(ifq)) {
			IF_DROP(ifq);
			m_freem(m0);
			splx(s);
			return (ENOBUFS);
		}
		IF_ENQUEUE(ifq, m0);
		schednetisr(NETISR_IP);
		break;
#endif
#ifdef NS
	case AF_NS:
		ifq = &nsintrq;
		if (IF_QFULL(ifq)) {
			IF_DROP(ifq);
			m_freem(m0);
			splx(s);
			return (ENOBUFS);
		}
		IF_ENQUEUE(ifq, m0);
		schednetisr(NETISR_NS);
		break;
#endif
	default:
		splx(s);
		printf("lo%d: can't handle af%d\n", ifp->if_unit,
			dst->sa_family);
		m_freem(m0);
		return (EAFNOSUPPORT);
	}
	ifp->if_ipackets++;
	splx(s);
	return (0);
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
loioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
{
	int error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	default:
		error = EINVAL;
	}
	return (error);
}
