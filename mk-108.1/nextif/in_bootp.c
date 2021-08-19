/*
 * Copyright (c) 1988, by NeXT Inc.
 *
 **********************************************************************
 * HISTORY
 * 09-Apr-90  Bradley Taylor (btaylor) at NeXT
 *	Changes because of new network API (netbuf and netif)
 *
 * 25-May-89  Peter King (king) at NeXT
 *	Added support for "Sexy Net Init" -- NeXT vendor protocol 1.
 *
 * 15-Aug-88  Peter King (king) at NeXT, Inc.
 *	Created.
 **********************************************************************
 */ 

#import <machine/spl.h>
#import <sys/param.h>
#import <sys/types.h>
#import <sys/boolean.h>
#import <sys/kernel.h>
#import <sys/errno.h>
#import <sys/file.h>
#import <sys/uio.h>
#import <sys/ioctl.h>
#import <sys/time.h>
#import <sys/mbuf.h>
#import <sys/user.h>
#import <sys/vnode.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <net/if.h>
#import <net/route.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/in_pcb.h>
#import <netinet/if_ether.h>
#import <netinet/ip.h>
#import <netinet/ip_var.h>
#import <netinet/udp.h>
#import <netinet/udp_var.h>
#import <netinet/ip_icmp.h>
#import <mon/bootp.h>
#import <nextdev/kmreg.h>

#define	CONSOLE	"/dev/console"


/*
 * Routine: in_bootp_initnet
 * Function:
 *	Set up the interface to handle BOOTP packets.  Return the interface
 *	address in ifr.
 * Returns:
 *	-1:	interface previously autoconfigured
 *	0:	success
 *	>0:	unix error 
 */
int
in_bootp_initnet(ifp, ifr, sop)
	register volatile struct ifnet	*ifp;
	register struct ifreq		*ifr;
	struct socket			**sop;
{
	struct sockaddr_in	lsin;
	struct sockaddr_in	*sin;
	struct mbuf		*m;
	int	error;
	
	/*
	 * Get a socket (wouldn't it be nice to make system calls from
	 * within the kernel?)
	 */
	if (error = socreate(AF_INET, sop, SOCK_DGRAM, 0)) {
		*sop = NULL;
		return (error);
	}

	/*
	 * Bring the interface up if neccessary.
	 */
	if ((ifp->if_flags & IFF_UP) == 0) {
		ifr->ifr_flags = ifp->if_flags | IFF_UP;
		if (error = ifioctl(*sop, SIOCSIFFLAGS, (caddr_t)ifr)) {
			return (error);
		}
	} else {
		/* If the interface was previously autoconfigured, return */
		if (ifp->if_flags & IFF_AUTODONE) {
			if ((error = ifioctl(*sop, SIOCGIFADDR, (caddr_t)ifr)))
				return (error);
			ifp->if_flags &= ~(IFF_AUTOCONF);
			return(-1);
		}
	}

	/*
	 * Assign a bogus address.
	 */
	ifp->if_flags |= IFF_AUTOCONF;

	bzero((caddr_t)&lsin, sizeof(lsin));
	lsin.sin_family = AF_INET;
	ifr->ifr_addr = *(struct sockaddr *)&lsin;

	if (error = ifioctl(*sop, SIOCSIFADDR, (caddr_t)ifr)) {
		return (error);
	}

	/*
	 * Now set up the socket to receive the BOOTP response, i.e. bind
	 * a name to it.
	 */
	m = m_get(M_WAIT, MT_SONAME);
	if (m == NULL) {
		error = ENOBUFS;
		return (error);
	}
	m->m_len = sizeof (struct sockaddr_in);
	sin = mtod(m, struct sockaddr_in *);
	sin->sin_family = AF_INET;
	sin->sin_port = IPPORT_BOOTPC;
	sin->sin_addr.s_addr = INADDR_ANY;

	error = sobind(*sop, m);
	m_freem(m);
	if (error) {
		return (error);
	}

	/* We will be doing non-blocking IO on this socket */
	(*sop)->so_state |= SS_NBIO;
	
	return (0);
}

/*
 * Routine: in_bootp_buildpacket
 * Function:
 *	Put together a BOOTP request packet.  RFC951.
 */
struct bootp_packet *
in_bootp_buildpacket(
		     volatile struct ifnet *ifp,	/* Interface */
		     struct sockaddr_in	*sin, 		/* Local IP address */
		     u_char my_enaddr[6]		/* Local EN address */
		     )
{
	struct bootp_packet	*bp_s;
	struct nextvend		*nv;

	bp_s = (struct bootp_packet *)kalloc(sizeof (*bp_s));
	nv = (struct nextvend *)&bp_s->bp_bootp.bp_vend;

	bzero(bp_s, sizeof (*bp_s));
	bp_s->bp_ip.ip_v = IPVERSION;
	bp_s->bp_ip.ip_hl = sizeof (struct ip) >> 2;
	bp_s->bp_ip.ip_id = ip_id++;
	bp_s->bp_ip.ip_ttl = MAXTTL;
	bp_s->bp_ip.ip_p = IPPROTO_UDP;
	bp_s->bp_ip.ip_src.s_addr = sin->sin_addr.s_addr;
	bp_s->bp_ip.ip_dst.s_addr = INADDR_BROADCAST;
	bp_s->bp_udp.uh_sport = htons(IPPORT_BOOTPC);
	bp_s->bp_udp.uh_dport = htons(IPPORT_BOOTPS);
	bp_s->bp_udp.uh_sum = 0;
	bp_s->bp_bootp.bp_op = BOOTREQUEST;
	bp_s->bp_bootp.bp_htype = ARPHRD_ETHER;
	bp_s->bp_bootp.bp_hlen = 6;
	bp_s->bp_bootp.bp_ciaddr.s_addr = 0;
	bcopy((caddr_t)my_enaddr, bp_s->bp_bootp.bp_chaddr, 6);
	bcopy(VM_NEXT, &nv->nv_magic, 4);
	if (rootdir) {
		/* Only allow sexy net init if we have a root filesystem */
		nv->nv_version = 1;
	}
	nv->nv_opcode = BPOP_OK;
	bp_s->bp_udp.uh_ulen = htons(sizeof (bp_s->bp_udp) +
				     sizeof (bp_s->bp_bootp));
	bp_s->bp_ip.ip_len = htons(sizeof (struct ip) +
				   ntohs(bp_s->bp_udp.uh_ulen));
	bp_s->bp_ip.ip_sum = 0;
	return (bp_s);
}

/*
 * Routine: in_bootp_bptombuf
 * Function:
 *	Put the outgoing BOOTP packet into an mbuf chain.
 */
struct mbuf *
in_bootp_bptombuf(bp)
	struct bootp_packet	*bp;
{
	register struct ip	*ip;
	struct mbuf		*top;
	struct mbuf		**mp = &top;
	struct mbuf		*m;
	int			resid = sizeof (*bp);
	caddr_t			cp = (caddr_t) bp;
	int			len;

	while (resid > 0) {
		MGET(m, M_WAIT, MT_DATA);
		if (resid >= MCLBYTES / 2) {
			MCLGET(m);
			if (m->m_len != MCLBYTES) {
				goto nopages;
			}
			len = MIN(MCLBYTES, resid);
		} else {
nopages:
			len = MIN(MLEN, resid);
		}
		bcopy(cp, mtod(m, caddr_t), len);
		resid -= len;
		cp += len;
		m->m_len = len;
		*mp = m;
		mp = &m->m_next;
	}

	/* Interface wants mbuf pointing at IP header */
	top->m_off += sizeof (struct ether_header);
	top->m_len -= sizeof (struct ether_header);

	/* Compute the checksum */
	ip = mtod(top, struct ip *);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(top, sizeof (struct ip));

	return (top);
}

/*
 * Routine: in_bootp_timeout
 * Function:
 *	Wakeup the process waiting to receive something on a socket.
 *	Boy wouldn't it be nice if we could do system calls from the
 *	kernel.  Then I could do a select!
 */
in_bootp_timeout(socketflag)
	struct socket 		**socketflag;
{
	struct socket *so =	*socketflag;

	*socketflag = NULL;
	sbwakeup(&so->so_rcv);
}

/*
 * Routine: in_bootp_promisctimeout
 * Function:
 *	Flag that it is ok to accept a response from a promiscuous BOOTP
 *	server.
 */
in_bootp_promisctimeout(ignoreflag)
	boolean_t	*ignoreflag;
{
	*ignoreflag = FALSE;
}
 

/*
 * Routine: in_bootp_getpacket
 * Function:
 *	Wait for an incoming packet on the socket.
 */
int
in_bootp_getpacket(so, pp, psize)
	struct socket		*so;
	caddr_t			pp;
	int			psize;
{
	struct	iovec		aiov;
	struct	uio		auio;

	aiov.iov_base = pp;
	aiov.iov_len = psize;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_offset = 0;
	auio.uio_resid = psize;

	return (soreceive(so, 0, &auio, 0, 0));
}

/*
 * Routine: in_bootp_sendrequest
 * Function:
 *	Do the actual work of sending a BOOTP request and getting the
 *	response.
 */
in_bootp_sendrequest(
		     volatile struct ifnet *ifp,
		     struct socket *so,
		     struct bootp_packet *bp_s,
		     struct bootp *bp_r,
		     u_char my_enaddr[6]
		     )
{
	struct mbuf		*m;
	register int		error;
	int			xid;
	struct timeval		tv;
	int			timeo;
	int			timecount;
	int			totaltime;
	struct socket		*timeflag;
	label_t			lqsave;
	struct sockaddr_in	lsin;
	struct nextvend		*nv_s =
		(struct nextvend *) &bp_s->bp_bootp.bp_vend;
	struct nextvend		*nv_r = (struct nextvend *)&bp_r->bp_vend;
	boolean_t		alertprinted = FALSE;
	boolean_t		promisc_rcvd = FALSE;
	boolean_t		promisc_ignore;

	/* Create a transaction ID */
	microtime(&tv);
	xid = my_enaddr[5] ^ tv.tv_sec;
	bp_s->bp_bootp.bp_xid = xid;

	/* Address to send to */
	lsin.sin_family = AF_INET;
	lsin.sin_port = IPPORT_BOOTPS;
	lsin.sin_addr.s_addr = INADDR_BROADCAST;

	/*
 	 * timeout/retry policy:
	 * Before a server has responded we retransmit at a random time
	 * in a binary window which is doubled each transmission.  This
	 * avoids flooding the net after, say, a power failure when all
	 * machines are trying to reboot simultaneously.
	 */
	timeo = 1;
	timecount = 0;
	totaltime = 0;
	while (alert_key != 'c' && alert_key != 'C') {
		if (timecount == 0) {
			/* Put the outgoing packet in an mbuf */
			m = in_bootp_bptombuf(bp_s);

			if (error = if_output_mbuf(ifp, m,
						   (struct sockaddr *)&lsin)) {
				goto errout;
			}
		}

		/* Handle interrupts elegantly */
		lqsave = u.u_qsave;
		if (setjmp(&u.u_qsave)) {
			u.u_qsave = lqsave;
			untimeout(in_bootp_timeout, &timeflag);
			error = EINTR;
			goto errout;
		}

		/*
		 * Sleep for a second and then check for the abort
		 * character again.
		 */
		timeflag = so;
		timeout(in_bootp_timeout, &timeflag, hz);
keepwaiting:
		while (((error = in_bootp_getpacket(so, bp_r, sizeof (*bp_r)))
			== EWOULDBLOCK) && (timeflag == so)) {
			sbwait(&so->so_rcv);
		}
		if (error && (error != EWOULDBLOCK)) {
			u.u_qsave = lqsave;
			untimeout(in_bootp_timeout, &timeflag);
			goto errout;
		}
		if (timeflag == NULL) {
			u.u_qsave = lqsave;
			timecount++;
			totaltime++;
			if (timecount == timeo) {
				if (timeo < 64) {
					timeo <<= 1;
				}
				timecount = 0;
			}
			/*
			 * If we have been waiting a while (20 secs),
			 * let the user know what is going on.
			 */
			if ( totaltime == 20 ) {
				extern char *mach_title;

				alert(60, 8, mach_title,
		       " \nNo response from network configuration server.\n%s",
	      "Type 'c' to start up computer without a network connection.\n",
				      0, 0, 0, 0, 0, 0, 0);
				alertprinted = TRUE;
			}
			continue;
		}

		/* We have a packet.  Check it out. */
		if (bp_r->bp_xid != xid ||
		    bp_r->bp_op != BOOTREPLY ||
		    bcmp(bp_r->bp_chaddr, my_enaddr, 6) != 0) {
			goto keepwaiting;
		}

		/*
		 * It's for us.  If it is not an authoritative answer,
		 * wait for a while (10 secs) to see if there is an
		 * authoritative server out there.
		 */
		if (nv_s->nv_opcode == BPOP_OK && nv_r->nv_opcode != BPOP_OK) {
			if (!promisc_rcvd) {
				promisc_rcvd = TRUE;
				promisc_ignore = TRUE;
				timeout(in_bootp_promisctimeout,
					&promisc_ignore, 10 * hz);
				goto keepwaiting;
			}
			if (promisc_ignore == TRUE) {
				goto keepwaiting;
			}
		}


		/* OK, it's for us */
		u.u_qsave = lqsave;
		untimeout(in_bootp_timeout, &timeflag);

		error = 0;
		goto errout;
	}

	/* We aborted.  Return appropriate error */
	error = ETIMEDOUT;

errout:
	untimeout(in_bootp_promisctimeout, &promisc_ignore);
	if (alertprinted) {
	    alert_done();
	}
	return (error);
}

/*
 * Routine: in_bootp_openconsole
 * Function:
 *	Pop up a console window so that we can do some user I/O to
 *	answer the server's questions.
 */
int
in_bootp_openconsole(vpp)
	struct vnode	**vpp;
{
	int	ret;
	label_t	lqsave;

	lqsave = u.u_qsave;
	ret = vn_open(CONSOLE, UIO_SYSSPACE, O_RDWR|O_ALERT, 0,
			    vpp);
	u.u_qsave = lqsave;
	return(ret);
}

/*
 * Routine: in_bootp_closeconsole
 * Function:
 *	Restore and close and the console window.
 * Note:
 *	There is some wierd code in this routine stolen from vfs_io.c
 *	to deal with multiple fd's having the same vnode open.  Ick.
 *	This should be cleaned up with NFS 4.0.
 */
int
in_bootp_closeconsole(vp)
	struct vnode	*vp;
{
	register struct file	*ffp;
	register struct vnode	*tvp;
	register enum vtype	type;
	register dev_t		dev;
	int			error;
	label_t			lqsave;

	/* Remove the window */
	VOP_IOCTL(vp, KMIOCRESTORE, 0, O_ALERT, 0);

	/* Call vn_close if necessary */
	type = vp->v_type;
	dev = vp->v_rdev;
	for (ffp = (struct file *) queue_first(&file_list);
	     !queue_end(&file_list, (queue_entry_t) ffp);
	     ffp = (struct file *) queue_next(&ffp->links)) {
		if (ffp->f_type != DTYPE_VNODE) {	/* XXX */
			continue;
		}
		if (ffp->f_count &&
		    (tvp = (struct vnode *)ffp->f_data) &&
		    tvp->v_rdev == dev && tvp->v_type == type) {
			VN_RELE(vp);
			return (0);
		}
	}
	lqsave = u.u_qsave;
	error = vn_close(vp, 0);
	u.u_qsave = lqsave;
	VN_RELE(vp);
	return (error);
}

/*
 * Routine: in_bootp_noecho
 * Function:
 *	Set up the console so that it doesn't echo typed characters.
 */
int
in_bootp_noecho(vp)
	struct vnode	*vp;
{
	struct	sgttyb	sg;
	int		error;

	if (error = VOP_IOCTL(vp, TIOCGETP, &sg, 0, 0)) {
		return (error);
	}
	sg.sg_flags &= ~(ECHO);
	if (error = VOP_IOCTL(vp, TIOCSETP, &sg, 0, 0)) {
		return (error);
	}
	return (0);
}

int
in_bootp_echo(vp)
	struct vnode	*vp;
{
	struct	sgttyb	sg;
	int		error;

	if (error = VOP_IOCTL(vp, TIOCGETP, &sg, 0, 0)) {
		return (error);
	}
	sg.sg_flags |= ECHO;
	if (error = VOP_IOCTL(vp, TIOCSETP, &sg, 0, 0)) {
		return (error);
	}
	return (0);
}

/*
 * Routine: in_bootp_processreply
 * Function:
 *	The heart of "Sexy Net Init".  This routine handles requests from
 *	the BOOTP server to interact with the user.
 */
int
in_bootp_processreply(vp, bp_s, bp_r)
	struct vnode	*vp;
	struct bootp_packet	*bp_s;
	struct bootp		*bp_r;
{
	struct nextvend		*nv_s =
		(struct nextvend *) &bp_s->bp_bootp.bp_vend;
	struct nextvend		*nv_r = (struct nextvend *)&bp_r->bp_vend;
	int			resid;
	int			error;
	int			flush = FREAD;
	boolean_t		noecho = FALSE;
	char			*cp;

	/* Truncate text if needed */
	if (strlen(nv_r->nv_text) > NVMAXTEXT) {
		nv_r->nv_null = '\0';
	}

	/* Write out the server message */
	printf("%s", nv_r->nv_text);

	/* Flush any left over input and output */
	if (error = VOP_IOCTL(vp, TIOCFLUSH, &flush, 0, 0)) {
		goto errout;
	}

	switch (nv_r->nv_opcode) {
	    case BPOP_QUERY_NE:
		if (error = in_bootp_noecho(vp)) {
			goto errout;
		}
		noecho = TRUE;
		/* Fall through */

	    case BPOP_QUERY:
		/* Read in the user response */
		if (error = vn_rdwr(UIO_READ, vp, (caddr_t)nv_s->nv_text,
				    NVMAXTEXT, 0, UIO_SYSSPACE, 0, &resid)) {
			goto errout;
		}

		/* If we are not echoing, move the cursor to the next line */
		if (noecho) {
			printf("\n");
		}

		/* Get rid of the CR or LF if either exists. */
		for (cp = (char *)nv_s->nv_text; *cp; cp++) {
			if (*cp == '\n' || *cp == '\r') {
				*cp = '\0';
				break;
			}
		}

		/* Set the destination, opcode and xid */
		bp_s->bp_ip.ip_dst = *(struct in_addr *)&bp_r->bp_siaddr;
		nv_s->nv_opcode = nv_r->nv_opcode;
		nv_s->nv_xid = nv_r->nv_xid;
		break;

	    case BPOP_ERROR:
		/* Reset the destination, opcode and xid */
		bp_s->bp_ip.ip_dst.s_addr = INADDR_BROADCAST;
		nv_s->nv_opcode = BPOP_OK;
		nv_s->nv_xid = 0;
		break;
	}
errout:
	if (noecho) {
		in_bootp_echo(vp);
	}
	return  (error);
}

/*
 * Routine: in_bootp_setaddress
 * Function:
 *	Set the address of the interface.  Leave the new address in ifr
 *	to be returned to the user.
 */
int
in_bootp_setaddress(ifp, ifr, so, addr)
	register volatile struct ifnet	*ifp;
	register struct ifreq		*ifr;
	struct socket			*so;
	struct in_addr			*addr;
{
	struct sockaddr_in		*sin;
	int				error;

	ifr->ifr_flags = ifp->if_flags & ~(IFF_UP);
	if (error = ifioctl(so, SIOCSIFFLAGS, (caddr_t) ifr)) {
		return (error);
	}	

	/* Turn off our autoconf flag */
	ifp->if_flags &= ~(IFF_AUTOCONF);

	/* Clear the netmask first */
	sin = (struct sockaddr_in *)&ifr->ifr_addr;
	bzero((caddr_t)sin, sizeof(struct sockaddr_in));
	sin->sin_family = AF_INET;
	if (error = ifioctl(so, SIOCSIFNETMASK, (caddr_t)ifr)) {
		return (error);
	}

	/* Now set the new address */
	sin->sin_addr = *addr;
	if (error = ifioctl(so, SIOCSIFADDR, (caddr_t)ifr)) {
		return (error);
	}

	/* Flag that we have autoconfigured */
	ifp->if_flags |= IFF_AUTODONE;
	return (0);
}

/*
 * Routine: in_bootp_makeifreq
 * Function:
 *	Make a valid ifreq struct from interface pointer and an address
 */
static struct ifreq
in_bootp_makeifreq(
		   volatile struct ifnet *ifp,
		   struct sockaddr_in *sin
		   )
{
	struct ifreq ifr;
	char *p;
	
	strcpy(ifr.ifr_name, ifp->if_name);
	for (p = ifr.ifr_name; *p; p++) {
	}
	*p++ = ifp->if_unit + '0';
	*p = 0;
	bcopy(sin, &ifr.ifr_addr, sizeof(*sin));
	return (ifr);
}

/*
 * Routine: in_bootp
 * Function:
 *	Use the BOOTP protocol to resolve what our IP address should be
 *	on a particular interface.
 * Note:
 *	The ifreq structure returns the newly configured address.  If there
 *	is an error, it can be full of bogus information.
 *
 *	This is called from driver level ioctl routines to assure that
 *	the interface is a 10Meg Ethernet.
 */
in_bootp(
	 volatile struct ifnet	*ifp,
	 struct sockaddr_in *sin,
	 u_char my_enaddr[6]
	 )
{
	struct mbuf		*m;
	struct socket		*so;
	register int		error;
	struct bootp_packet	*bp_s = NULL;
	struct bootp		*bp_r = NULL;
	struct vnode		*vp = NULL;
	struct nextvend		*nv_s;
	struct nextvend		*nv_r;
	struct ifreq		ifr;

	ifr = in_bootp_makeifreq(ifp, sin);
	if (error = in_bootp_initnet(ifp, &ifr, &so)) {
		if (error == -1) {
			/*
			 * This means the interface was already
			 * autoconfigured.  Reset the address to make
			 * sure the netmask is right.
			 */
			error = ifioctl(so, SIOCSIFADDR, (caddr_t)&ifr);
			if (error == 0) {
				bcopy(&ifr.ifr_addr, sin, sizeof(*sin));
			}
			soclose(so);
			return (error);
		}
		goto errout;
	}

	/* Build a packet */
	bp_s = in_bootp_buildpacket(ifp, 
				    sin,
				    my_enaddr);
	nv_s = (struct nextvend *)&bp_s->bp_bootp.bp_vend;

	/* Set up place to receive BOOTP response */
	bp_r = (struct bootp *)kalloc(sizeof (*bp_r));
	nv_r = (struct nextvend *)&bp_r->bp_vend;

	while (TRUE) {
		/* Send the request */
		if (error = in_bootp_sendrequest(ifp, so, bp_s, bp_r, 
						 my_enaddr)) {
			goto errout;
		}
		if (nv_r->nv_opcode == BPOP_OK) {
			break;
		}
		if (vp == NULL && (error = in_bootp_openconsole(&vp))) {
			goto errout;
		}
		if (error = in_bootp_processreply(vp, bp_s, bp_r)) {
			goto errout;
		}
	}

	/* It's for us. */
	if (vp) {
		(void) in_bootp_closeconsole(vp);
		vp = NULL;
	}

	error = in_bootp_setaddress(ifp, &ifr, so,
				    (struct in_addr *)&bp_r->bp_yiaddr);
	if (error == 0) {
		bcopy(&ifr.ifr_addr, sin, sizeof(*sin));
	}

errout:	
	if (error && so) {
		/* Make sure the interface is shut down */
		ifr.ifr_flags = ifp->if_flags & ~(IFF_UP);
		(void) ifioctl(so, SIOCSIFFLAGS, (caddr_t) &ifr);
	}
	ifp->if_flags &= ~(IFF_AUTOCONF);
	if (so) {
		soclose(so);
	}
	if (bp_s) {
		kfree((caddr_t)bp_s, sizeof (*bp_s));
	}
	if (bp_r) {
		kfree((caddr_t)bp_r, sizeof (*bp_r));
	}
	if (vp) {
		(void) in_bootp_closeconsole(vp);
	}
	return (error);
}
