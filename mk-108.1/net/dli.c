/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 **********************************************************************
 * HISTORY
 * 15-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 06-May-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Created.
 *	[ V5.1(F10) ]
 *
 **********************************************************************
 *
 *  Direct data-link interface protocol
 *
 *  The AF_DLI address family is used to provide raw access at the
 *  data-link level of a network interface (I have no idea if this
 *  is the usage for which it was originally intended).
 */

#import <dli.h>

#if	DLI
#import <sys/param.h>
#import <sys/mbuf.h>
#import <sys/socket.h>
#import <sys/protosw.h>
#import <sys/socketvar.h>
#import <sys/errno.h>
#import <sys/ioctl.h>

#import <net/if.h>
#import <net/route.h>
#import <net/raw_cb.h>

#import <net/dli_var.h>

#import <machine/spl.h>

/*
 *  Direct data-link interface to device.
 */

int dli_active = 0;	/* active raw DLI sockets */



/*
 *  Send a packet directly at the data-link interface level
 *
 *  The local address bound to the socket is used to determine the
 *  interface over which to send the packet with no other routing.
 */

dli_output(m, so)
	register struct mbuf *m;
	struct socket *so;
{
	register struct rawcb *rp = sotorawcb(so);
	struct ifaddr *ia = ifa_ifwithaddr((struct sockaddr *)&rp->rcb_laddr);

	if (ia)
        {
		struct ifnet *ifp = ia->ifa_ifp;

		return ((*ifp->if_output)(ifp, m,
				          (struct sockaddr *)&rp->rcb_faddr));
	}
	m_freem(m);
	return (ENETUNREACH);
}



/*
 *  Receive a packet directly from the data-link interface level
 *
 *  Stuff the receive protocol identifier and source address into the
 *  appropriate structures expected by raw_iput().
 *
 *  Remove the interface pointer at the beginning of the mbuf and prepend a
 *  packet header mbuf containing the physical data-link interface header (this
 *  is different from most other protocols where the source address information
 *  is only provided out of band from the message data since it also permits
 *  the destination addess and protocol to be included which would otherwise be
 *  unavailable).
 *
 *  As an optimization, we keep track of whether or not any raw sockets are
 *  currently open for input at the DLI protocol and don't bother to do any of
 *  the raw packet filtering if we know that nobody will want the packet.
 */

dli_input(m, proto, src, dlv, lh)
	struct mbuf *m;
	int proto;
	char *src;
	register struct dli_var *dlv;
	char *lh;
{
	if (dli_active)
	{
		register struct mbuf *b;

		dlv->dlv_rproto.sp_protocol = proto;
		bcopy(src, &dlv->dlv_rsrc.sa_data[0], dlv->dlv_hln);
		IF_ADJ(m);
		MGET(b, M_DONTWAIT, MT_HEADER);
		if (b)
		{
		    b->m_len = dlv->dlv_lhl;
		    b->m_off = MMAXOFF-b->m_len;
		    b->m_next = m;
		    bcopy(lh, mtod(b, char *), dlv->dlv_lhl);
		    raw_input(b, &dlv->dlv_rproto, &dlv->dlv_rsrc, &dlv->dlv_da.da_addr);
		    return;
		}
	}
	m_freem(m);
}



/*
 *  Process "user" request
 *
 *  Forward all "control" requests on to dli_control().
 *
 *  Pass all other requests on to the standard raw_usrreq() handler although we
 *  eavesdrop on the communication to:
 *
 *  - monitor ATTACH and DETACH operations on raw sockets to maintain a count 
 *    of open sockets for dli_input().
 *  - allow ATTACH and DETACH operations on other sockets (only SOCK_DGRAM at
 *    the moment) to allow them to be opened for control purposes.
 */

/*ARGSUSED*/
dli_usrreq(so, req, m, nam, rights)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *rights;
{
	if (req != PRU_CONTROL)
	{
		int error = 0;

		switch (req)
		{
		    case PRU_DETACH:
			if (so->so_type == SOCK_RAW)
			     dli_active--;
			break;
		    case PRU_ATTACH:
			break;
		    default:
			error = EOPNOTSUPP;
			break;
		}
		if (so->so_type == SOCK_RAW)
		{
		    error = raw_usrreq(so, req, m, nam, rights);
		    if (error == 0 && req == PRU_ATTACH)
			dli_active++;
		}
		else
		{
		    if (m != NULL)
			    m_freem(m);
		}
		return(error);
	}
	else
		return (dli_control(so, (int)m, (caddr_t)nam,
			(struct ifnet *)rights));
}



/*
 *  Handle interface status and control requests
 *
 *  SIOCGIFADDR - returns hardware address of interface
 *  SIOCGIFBRDADDR - returns hardware broadcast address of interface
 *  SIOCGIFDSTADDR - returns hardware destination address of interface (unused)
 *  SIOCGIFGIFNETMASK - returns mask of hardware address length for interface
 */

dli_control(so, cmd, data, ifp)
	struct socket *so;
	int cmd;
	caddr_t data;
	register struct ifnet *ifp;
{
	register struct ifreq *ifr = (struct ifreq *)data;
	register struct ifaddr *ifa = 0;

#ifdef	lint
	so++;
#endif	lint

	if (ifp)
		for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
			if (ifa->ifa_addr.sa_family == AF_DLI)
				break;
	if (ifa == 0)
		return (EOPNOTSUPP);

	switch (cmd) {

	case SIOCGIFADDR:
		ifr->ifr_addr = ifa->ifa_addr;
		break;

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		ifr->ifr_dstaddr = ifa->ifa_broadaddr;
		break;

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		ifr->ifr_dstaddr = ifa->ifa_dstaddr;
		break;

	case SIOCGIFNETMASK:
	{
		register int i;

		ifr->ifr_addr.sa_family = AF_DLI;
		bzero(&ifr->ifr_addr.sa_data[0], sizeof(ifr->ifr_addr.sa_data));
		for (i=0; i<((struct dli_ifaddr *)(ifa))->da_dlv->dlv_hln; i++)
			ifr->ifr_addr.sa_data[i] = 0xff;
		break;
	}

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}
	return(0);
}



/*
 *  Initialize the DLI protocol for a network interface
 *
 *  Set the address family in all the socket address structures and our
 *  hardware address and broadcast address.
 *
 *  Initialize the hardware address and header length fields.
 *
 *  Link our DLI address onto the master address list for this interface.
 */

dli_init(dlv, ifp, addr, broadaddr, addrlen, lhlen)
	register struct dli_var *dlv;
	register struct ifnet *ifp;
	char *addr;
	char *broadaddr;
	u_int addrlen;
{
	register struct ifaddr *ifa;
	struct dli_ifaddr *da = &dlv->dlv_da;

#ifdef	lint
	broadaddr++;
#endif	lint

	if (da->da_addr.sa_family)
		return;

	da->da_addr.sa_family =
        da->da_broadaddr.sa_family = 
	dlv->dlv_rsrc.sa_family = 
	dlv->dlv_rproto.sp_family = AF_DLI;
	dlv->dlv_hln = addrlen;
	dlv->dlv_lhl = lhlen;
	bcopy(addr, da->da_addr.sa_data, addrlen);
	bcopy(addr, da->da_broadaddr.sa_data, addrlen);
        da->da_dlv = dlv;
	da->da_ifp = ifp;
	if (ifa = ifp->if_addrlist) {
		for ( ; ifa->ifa_next; ifa = ifa->ifa_next)
			;
		ifa->ifa_next = (struct ifaddr *) da;
	} else
		ifp->if_addrlist = (struct ifaddr *) da;
}
#endif	DLI
