/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 09-Apr-90  Bradley Taylor (btaylor) at NeXT
 *	Modifications to support new network API (netbuf and netif)
 *
 * 26-Sep-88  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: undo changes to ifunit() and if_down(),
 *			 addition of ifb_ifwithaf() and address_known().
 *
 * 20-Oct-87  Peter King (king) at NeXT, Inc.
 *	SUN_RPC:  Sun's fixes in ifunit() and if_down().
 */ 
 
/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)if.c	7.1 (Berkeley) 6/4/86
 */

#import <ether.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/protosw.h>
#import <sys/user.h>
#import <sys/kernel.h>
#if	NeXT
#import <sys/mbuf.h>
#endif
#import <sys/ioctl.h>
#import <sys/errno.h>

#import <net/if.h>
#if	NeXT
#import <net/netbuf.h>
#endif
#import <net/af.h>

#import <machine/spl.h>

int	ifqmaxlen = IFQ_MAXLEN;



/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as
 * parameters.
 */

ifinit()
{
#if	NeXT
#else	NeXT
	register struct ifnet *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->if_next)
		if (ifp->if_snd.ifq_maxlen == 0)
			ifp->if_snd.ifq_maxlen = ifqmaxlen;
	if_slowtimo();
#endif	NeXT
}

#ifdef vax
/*
 * Call each interface on a Unibus reset.
 */
ifubareset(uban)
	int uban;
{
	register struct ifnet *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->if_next)
		if (ifp->if_reset)
			(*ifp->if_reset)(ifp->if_unit, uban);
}
#endif

#if NeXT
#else NeXT
/*
 * Attach an interface to the
 * list of "active" interfaces.
 */
if_attach(ifp)
	struct ifnet *ifp;
{
	register struct ifnet **p = &ifnet;

	while (*p)
		p = &((*p)->if_next);
	*p = ifp;
#if	NeXT
	ifp->if_next = 0;
#endif	NeXT
}
#endif	NeXT

#if	NeXT
#else	NeXT
/*
 *	Detach an interface from the list of "active" interfaces.
 */
if_detach(ifp)
	struct ifnet *ifp;
{
	register struct ifnet **p = &ifnet;

	while (*p) {
		if (*p == ifp) {
			*p = ifp->if_next;
			return;
		}
		p = &((*p)->if_next);
	}
}
#endif	NeXT

/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(addr)
	struct sockaddr *addr;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;

#define	equal(a1, a2) \
	(bcmp((caddr_t)((a1)->sa_data), (caddr_t)((a2)->sa_data), 14) == 0)
	for (ifp = ifnet; ifp; ifp = ifp->if_next)
	    for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr.sa_family != addr->sa_family)
			continue;
		if (equal(&ifa->ifa_addr, addr))
			return (ifa);
		if ((ifp->if_flags & IFF_BROADCAST) &&
		    equal(&ifa->ifa_broadaddr, addr))
			return (ifa);
	}
	return ((struct ifaddr *)0);
}
/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithdstaddr(addr)
	struct sockaddr *addr;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;

	for (ifp = ifnet; ifp; ifp = ifp->if_next) 
	    if (ifp->if_flags & IFF_POINTOPOINT)
		for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr.sa_family != addr->sa_family)
				continue;
			if (equal(&ifa->ifa_dstaddr, addr))
				return (ifa);
	}
	return ((struct ifaddr *)0);
}

/*
 * Find an interface on a specific network.  If many, choice
 * is first found.
 */
struct ifaddr *
ifa_ifwithnet(addr)
	register struct sockaddr *addr;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;
	register u_int af = addr->sa_family;
	register int (*netmatch)();

	if (af >= AF_MAX)
		return (0);
	netmatch = afswitch[af].af_netmatch;
	for (ifp = ifnet; ifp; ifp = ifp->if_next)
	    for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr.sa_family != addr->sa_family)
			continue;
		if ((*netmatch)(&ifa->ifa_addr, addr))
			return (ifa);
	}
	return ((struct ifaddr *)0);
}

#ifdef notdef
/*
 * Find an interface using a specific address family
 */
struct ifaddr *
ifa_ifwithaf(af)
	register int af;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;

	for (ifp = ifnet; ifp; ifp = ifp->if_next)
	    for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
		if (ifa->ifa_addr.sa_family == af)
			return (ifa);
	return ((struct ifaddr *)0);
}
#endif

/*
 * At this point, we have not initialised any interface (except
 * loopback!), so we choose an arbitrary interface on which to
 * try a revarp.
 */
/*ARGSUSED*/
struct ifnet *
ifb_ifwithaf(af)
	int af;
{

	return(ifnet);
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
if_down(ifp)
	register struct ifnet *ifp;
{
	register struct ifaddr *ifa;

	ifp->if_flags &= ~IFF_UP;
	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
		pfctlinput(PRC_IFDOWN, &ifa->ifa_addr);
}

#if	NeXT
#else	NeXT
/*
 * Handle interface watchdog timer routines.  Called
 * from softclock, we decrement timers (if set) and
 * call the appropriate interface routine on expiration.
 */
if_slowtimo()
{
	register struct ifnet *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		if (ifp->if_timer == 0 || --ifp->if_timer)
			continue;
		if (ifp->if_watchdog)
			(*ifp->if_watchdog)(ifp->if_unit);
	}
	timeout(if_slowtimo, (caddr_t)0, hz / IFNET_SLOWHZ);
}
#endif	NeXT

/*
 * Map interface name to
 * interface structure pointer.
 */
struct ifnet *
ifunit(name)
	register char *name;
{
	register char *cp;
	register struct ifnet *ifp;
	int unit;

	for (cp = name; cp < name + IFNAMSIZ && *cp; cp++)
		if (*cp >= '0' && *cp <= '9')
			break;
	if (*cp == '\0' || cp == name + IFNAMSIZ)
		return ((struct ifnet *)0);
	unit = *cp - '0';
	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
#if	NeXT
		if (bcmp(ifp->if_name, name, (unsigned)(cp - name)) != 0 ||
		    ifp->if_class != NETIFCLASS_VIRTUAL) {
			continue;
		}
#else	NeXT
		if (bcmp(ifp->if_name, name, (unsigned)(cp - name)))
			continue;
#endif	NeXT
		if (unit == ifp->if_unit)
			break;
	}
	return (ifp);
}

/*
 * Interface ioctls.
 */
ifioctl(so, cmd, data)
	struct socket *so;
	int cmd;
	caddr_t data;
{
	register struct ifnet *ifp;
	register struct ifreq *ifr;

	switch (cmd) {

	case SIOCGIFCONF:
		return (ifconf(cmd, data));

#if defined(INET) && NETHER > 0
	case SIOCSARP:
	case SIOCDARP:
		if (!suser())
			return (u.u_error);
		/* FALL THROUGH */
	case SIOCGARP:
		return (arpioctl(cmd, data));
#endif
	}
	ifr = (struct ifreq *)data;
	ifp = ifunit(ifr->ifr_name);

	if (ifp == 0)
		return (ENXIO);
	switch (cmd) {

	case SIOCGIFFLAGS:
		ifr->ifr_flags = ifp->if_flags;
		break;

	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCSIFFLAGS:
		if (!suser())
			return (u.u_error);
		if (ifp->if_flags & IFF_UP && (ifr->ifr_flags & IFF_UP) == 0) {
			int s = splimp();
			if_down(ifp);
			splx(s);
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(ifr->ifr_flags &~ IFF_CANTCHANGE);
#if	NeXT
		if_ioctl((netif_t)ifp, cmd, data);
#else	NeXT
		if (ifp->if_ioctl)
			(void) (*ifp->if_ioctl)(ifp, cmd, data);
#endif	NeXT
		break;

	case SIOCSIFMETRIC:
		if (!suser())
			return (u.u_error);
		ifp->if_metric = ifr->ifr_metric;
		break;

	case SIOCSIFMTU:
	case SIOCGIFMTU:
	case SIOCSIFASYNCMAP:
	case SIOCGIFASYNCMAP:
#if	NeXT
		if (!ifp->if_control)
			return (EOPNOTSUPP);
		return (if_ioctl((netif_t)ifp, cmd, data));
#else	NeXT
		if (!ifp->if_ioctl)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
		break;
#endif	NeXT


	default:
		if (so->so_proto == 0)
			return (EOPNOTSUPP);
		return ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
			cmd, data, ifp));
	}
	return (0);
}

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 */
/*ARGSUSED*/
ifconf(cmd, data)
	int cmd;
	caddr_t data;
{
	register struct ifconf *ifc = (struct ifconf *)data;
	register struct ifnet *ifp = ifnet;
	register struct ifaddr *ifa;
	register char *cp, *ep;
	struct ifreq ifr, *ifrp;
	int space = ifc->ifc_len, error = 0;

	ifrp = ifc->ifc_req;
	ep = ifr.ifr_name + sizeof (ifr.ifr_name) - 2;
	for (; space > sizeof (ifr) && ifp; ifp = ifp->if_next) {
		bcopy(ifp->if_name, ifr.ifr_name, sizeof (ifr.ifr_name) - 2);
		for (cp = ifr.ifr_name; cp < ep && *cp; cp++)
			;
		*cp++ = '0' + ifp->if_unit; *cp = '\0';
		if ((ifa = ifp->if_addrlist) == 0) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp, sizeof (ifr));
			if (error)
				break;
			space -= sizeof (ifr), ifrp++;
		} else 
		    for ( ; space > sizeof (ifr) && ifa; ifa = ifa->ifa_next) {
			ifr.ifr_addr = ifa->ifa_addr;
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp, sizeof (ifr));
			if (error)
				break;
			space -= sizeof (ifr), ifrp++;
		}
	}
	ifc->ifc_len -= space;
	return (error);
}

address_known(ifp)
	struct ifnet *ifp;
{

	return (ifp->if_addrlist != NULL);
}

