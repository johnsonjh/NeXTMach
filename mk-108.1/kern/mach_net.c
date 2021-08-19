/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 * $Log:	mach_net.c,v $
 *  1-Mar-90  Gregg Kellogg (gk) at NeXT
 *	NeXT:	Don't make the mach_net_kmsg_zone quite so big.
 *	Turn off optimization around netipc_listen().
 *
 * Revision 2.11  89/02/25  18:05:43  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.10  89/01/15  16:24:27  rpd
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  15:02:07  rpd]
 * 
 * Revision 2.8  88/10/11  10:19:58  rpd
 * 	Removed SEND_KERNEL from the msg_queue operation.
 * 	Removed redundant log messages.
 * 	[88/10/09  08:46:23  rpd]
 * 	
 * 	Changed "struct KMsg" to "struct kern_msg".
 * 	Removed redundant "#if MACH_NET/#endif" wrapper.
 * 	[88/10/04  07:07:58  rpd]
 * 
 * Revision 2.7  88/10/01  21:57:33  rpd
 * 	Changed msg_remote_port to msg_local_port, because that is
 * 	the destination port of a copied-in message now.
 * 	[88/10/01  21:30:34  rpd]
 * 
 * Revision 2.6  88/08/24  01:51:51  mwyoung
 * 	Corrected include file references.
 * 	[88/08/22  22:06:23  mwyoung]
 * 
 * 22-Aug-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Removed ancient history.
 *
 * 15-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Changed number of allocated messages for mach_net from 8 to 4.
 *
 * Revision 2.5  88/08/06  18:22:03  rpd
 * Eliminated use of kern/mach_ipc_defs.h.
 * 
 * Revision 2.4  88/07/20  16:38:58  rpd
 * When queueing a message, bump current and ip_data_grams
 * ipc_statistics counters.
 * 
 * 18-May-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Added another access check for mach_net_server. Only privileged
 *	users should be allowed to send packets this way, because there
 *	is little or no check against false source addresses.
 *
 *  6-May-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Added an access check for netipc_listen and
 *	(from code by rds).
 *
 *  3-Apr-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 *  1-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	Receive_ip_datagram is called at splnet.  It must drop priority
 *	to spl0 to call the IPC routines, and raise back to splnet when
 *	it exits.  NOTE that this works ONLY if this is always called
 *	from the netisr thread - it will garble interrupt handling
 *	horribly.
 *
 *  8-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Removed spl check code; interrupt disabling is no longer needed
 *	because all network code runs in a thread context.  Removed TT
 *	conditionals.
 *
 *  9-Nov-87  David Golub (dbg) at Carnegie-Mellon University
 *	Since network routines now run in a thread context, use
 *	msg_queue to send message.
 *
 */ 

#import <sys/types.h>
#import <sys/mbuf.h>
#import <sys/socket.h>

#import <net/if.h>
#import <net/route.h>

#import <netinet/in.h>
#import <netinet/in_pcb.h>
#import <netinet/in_systm.h>

#import <netinet/ip.h>
#import <netinet/ip_var.h>

#import <sys/boolean.h>
#import <kern/kern_port.h>
#import <kern/zalloc.h>
#import <kern/kern_msg.h>

#import <sys/param.h>		/* NBPG, CLBYTES, etc. */
#import <sys/user.h>

#import <vm/vm_kern.h>	/* kernel_map */
#import <kern/parallel.h>

#import <kern/ipc_statistics.h>

#import <machine/spl.h>

#if	NeXT
/* NO WAY! */
#else	NeXT
#define MACH_NET_GID_1	2	/* sys */
#define MACH_NET_GID_2	38	/* network */
#endif	NeXT


#define IP_MSG_ID	1959

boolean_t	mach_net_server(in_msg, out_msg)
	msg_header_t	*in_msg;
	msg_header_t	*out_msg;
{
	struct ip_msg {
		msg_header_t	header;
		struct ip	ip;
	} *ip_msg = (struct ip_msg *) in_msg;

	struct mbuf		*top;
	register struct mbuf	**mp;
	register int		size;
	register caddr_t	cp;
	static struct route	cached_route = { 0 };
#ifdef	lint
	out_msg++;
#endif	lint

#if	NeXT
	if (!(u.u_uid == 0))
		return(FALSE);
#else	NeXT
	if (!((u.u_uid == 0) || 
		groupmember(MACH_NET_GID_1) || 
		groupmember(MACH_NET_GID_2)))
		return(FALSE);

#endif	NeXT
	/*
	 *	Only accept simple messages large enough
	 *	to contain an IP header.
	 */

	if ((!ip_msg->header.msg_simple) ||
	    (ip_msg->header.msg_size < sizeof (struct ip_msg)) ||
	    (ip_msg->header.msg_id != IP_MSG_ID)
	   )
		return(FALSE);

	/*
	 *	Check that user has supplied a reasonable IP datagram length.
	 */

	size = ip_msg->ip.ip_len;
	if ((size <= sizeof(struct ip)) || (size > (ip_msg->header.msg_size - sizeof(msg_header_t))))
		return(FALSE);

	/*
	 *	Remainder of this procedure must execute on master.
	 */
	unix_master();

	/*
	 *	Copy data to mbuf chain.
	 *	This loop is similar to those in sosend() (in sys/uipc_socket.c)
	 *	and if_ubaget() (in vaxif/if_uba.c).
	 */
	top = 0;
	mp = &top;
	cp = (caddr_t) &ip_msg->ip;
	while (size > 0) {
		register struct mbuf *m;
		register unsigned int len;

		MGET(m, M_WAIT, MT_DATA);
		if (m == 0) {
			m_freem(top);
			unix_release();
			return(FALSE);
		}
		if (size >= NBPG/2) {	/* heuristic */
			MCLGET(m);
			if (m->m_len != CLBYTES)
				m->m_len = len = MIN(MLEN, size);
			else
				m->m_len = len = MIN(CLBYTES, size);
		} else
			m->m_len = len = MIN(MLEN, size);

		bcopy(cp, mtod(m, caddr_t), len);
		cp += len;
		size -= len;
		*mp = m;
		mp = &m->m_next;
	}

	/*
	 *	Use cached route if the destination address is the same.
	 *	Otherwise, zero the route so that ip_output() will find a new one.
	 *	The use of the cached route decreases the total time to send a datagram
	 *	by as much as 40%.
	 */
	if (((struct sockaddr_in *) &cached_route.ro_dst)->sin_addr.s_addr
	    != ip_msg->ip.ip_dst.s_addr) {
		if (cached_route.ro_rt)
			RTFREE(cached_route.ro_rt);
		cached_route.ro_rt = 0;
	}

	/*
	 *	Send the mbuf chain.
	 *	We use the IP_FORWARDING option because
	 *	the user supplies a complete IP header.
	 *	Deallocation of the mbuf chain is handled by ip_output().
	 */
	if (ip_output(top, (struct mbuf *)0,
		      &cached_route, (IP_FORWARDING|IP_ALLOWBROADCAST)) != 0) {
		unix_release();
		return(FALSE);
	}
	unix_release();
	return(TRUE);
}

/*
 *	Data structures and associated routines
 *	to register IPC ports for incoming datagrams.
 *
 *	Listeners are organized into buckets by the (IP) protocol number
 *	for which they are listening. Within a bucket, the listeners
 *	are linked in a list that is maintained by a move-to-front strategy.
 *
 *	A zero entry in any of the source or destination
 *	fields indicates a wildcard that matches the corresponding field
 *	in any incoming datagram.
 *
 *	The lock field is used for mutual exclusion when adding listeners.
 *	Lookups do not require synchronization because the ipc_port field
 *	(used to tell whether a listener is present) is always updated last,
 *	in a single operation.
 *
 *	The routines to add and remove listeners are called by the user
 *	via a Matchmaker interface. Remove_listener() is also called by
 *	port_deallocate().
 */

struct lbucket {
	decl_simple_lock_data(,	lock)
	struct listener		*head;
};

struct lbucket listeners[256];		/* indexed by protocol */

struct listener {
	struct listener	*next;
	long		src_addr;
	long		dst_addr;
	unsigned short	src_port;
	unsigned short	dst_port;
	port_t		ipc_port;
};

#define LISTENER_MAX	100
#define LISTENER_CHUNK	1

zone_t listener_zone;

#define FOR_EACH_BUCKET(b)	for((b)=listeners; (b) < &listeners[256]; (b)++)
#define FOR_EACH_LISTENER(lp,b)	for((lp)=(b)->head; (lp) != 0; (lp)=(lp)->next)
#define LOCK_BUCKET(b, ipl)	{ ipl = splnet(); simple_lock(&(b)->lock); }
#define UNLOCK_BUCKET(b, ipl)	{ simple_unlock(&(b)->lock); splx(ipl); }

/*
 *	Register a listener.
 *	No checking is done to prevent conflict with existing listeners.
 */
/*ARGSUSED*/
#pragma CC_OPT_OFF
kern_return_t	netipc_listen(server_port, _src_addr, _dst_addr,
			      _src_port, _dst_port, _protocol, ipc_port)
	port_t		server_port;	/* unused */
	int		_src_addr;
	int		_dst_addr;
	int		_src_port;
	int		_dst_port;
	int		_protocol;
	port_t		ipc_port;
{
	register struct listener *lp;
	register struct lbucket *b;
	int ipl;

	long		src_addr = _src_addr;
	long		dst_addr = _dst_addr;
	unsigned short	src_port = _src_port;
	unsigned short	dst_port = _dst_port;
	u_char		protocol = _protocol;

#if	NeXT
	if (!(u.u_uid == 0))
		return(KERN_NO_ACCESS);
#else	NeXT
	if (!((u.u_uid == 0) || 
		groupmember(MACH_NET_GID_1) || 
		groupmember(MACH_NET_GID_2)))
		return(KERN_NO_ACCESS);
#endif	NeXT

	if (ipc_port == PORT_NULL)
		return(KERN_INVALID_ARGUMENT);
	lp = (struct listener *) zalloc(listener_zone);
	lp->src_addr = src_addr;
	lp->src_port = src_port;
	lp->dst_addr = dst_addr;
	lp->dst_port = dst_port;
	port_reference(lp->ipc_port = ipc_port);
	b = &listeners[protocol];
	LOCK_BUCKET(b, ipl);
	lp->next = b->head;
	b->head = lp;
	UNLOCK_BUCKET(b, ipl);
	port_object_set(ipc_port, PORT_OBJECT_NET, 0);
	return(KERN_SUCCESS);
}
#pragma CC_OPT_RESTORE

/*
 *	Remove all listeners with the specified port.
 */
/*ARGSUSED*/
kern_return_t	netipc_ignore(server_port, ipc_port)
	port_t		server_port;	/* unused */
	register port_t	ipc_port;
{
	register struct lbucket *b;
	register struct listener *prev, *lp;
	kern_return_t result = KERN_FAILURE;
	int ipl;

	if (ipc_port == PORT_NULL)
		return KERN_INVALID_ARGUMENT;
	FOR_EACH_BUCKET (b) {
		LOCK_BUCKET(b, ipl);
		prev = b->head;
		FOR_EACH_LISTENER (lp, b) {
			if (lp->ipc_port == ipc_port) {
				result = KERN_SUCCESS;
				if (lp == b->head) {
					b->head = lp->next;
					zfree(listener_zone, (vm_offset_t) lp);
					if ((prev = lp = b->head) == 0)
						break;
				} else {
					prev->next = lp->next;
					zfree(listener_zone, (vm_offset_t) lp);
					lp = prev;
				}
				port_release(ipc_port);
			}
			else {
				prev = lp;
			}
		}
		UNLOCK_BUCKET(b, ipl);
	}
	return(result);
}

/*
 *	Find a listener for the given source, destination, and protocol.
 */
port_t		find_listener(src_addr, src_port, dst_addr, dst_port, protocol)
	u_long		src_addr;
	u_short		src_port;
	u_long		dst_addr;
	u_short		dst_port;
	u_char		protocol;
{
	register struct lbucket *b;
	register struct listener *prev, *lp;

#define ADDR_MATCH(lp, addr)	((lp)->addr==INADDR_ANY || (lp)->addr==(addr))
#define PORT_MATCH(lp, port)	((lp)->port==0 || (lp)->port ==(port))

	b = &listeners[protocol];
	prev = b->head;
	FOR_EACH_LISTENER(lp, b) {
		/*
		 *	The following tests should be ordered
		 *	in increasing likelihood of success.
		 */
		if (PORT_MATCH(lp, dst_port) &&
		    PORT_MATCH(lp, src_port) &&
		    ADDR_MATCH(lp, src_addr) &&
		    ADDR_MATCH(lp, dst_addr)) {
			if (lp != b->head) {
				/* move to front */
				prev->next = lp->next;
				lp->next = b->head;
				b->head = lp;
			}
			return(lp->ipc_port);
		} else
			prev = lp;
	}
	return(PORT_NULL);
}

/*
 *	IP header plus source and destination ports.
 *	Common to both UDP datagrams and TCP segments.
 */
struct ip_plus_ports {
	struct ip	ip_header;
	u_short		src_port;
	u_short		dst_port;
};

/*
 *	Skeletal IPC message header for fast initialization.
 */
static msg_header_t ip_msg_template;

#define MACH_NET_MSG_SIZE_MAX	2048
zone_t	mach_net_kmsg_zone;

void		mach_net_init()
{
	register struct lbucket *b;
	
	listener_zone = zinit(
				sizeof(struct listener),
				LISTENER_MAX * sizeof(struct listener),
				LISTENER_CHUNK * sizeof(struct listener),
				FALSE,
				"net listener zone"
			);
	ip_msg_template.msg_simple = TRUE;
	ip_msg_template.msg_size = MACH_NET_MSG_SIZE_MAX - sizeof(struct kern_msg) + sizeof(msg_header_t);
	ip_msg_template.msg_type = MSG_TYPE_NORMAL;
	ip_msg_template.msg_local_port = PORT_NULL;
	ip_msg_template.msg_remote_port = PORT_NULL;
	ip_msg_template.msg_id = IP_MSG_ID;
	FOR_EACH_BUCKET (b) {
		simple_lock_init(&b->lock);
	}

	/*
	 * Create and allocate some space for our private kmsg zone.
	 */
#if	NeXT
	mach_net_kmsg_zone = zinit(MACH_NET_MSG_SIZE_MAX, 4 * MACH_NET_MSG_SIZE_MAX,
				MACH_NET_MSG_SIZE_MAX, FALSE, "mach_net messages");
	zcram(mach_net_kmsg_zone, kmem_alloc(kernel_map, 4 * MACH_NET_MSG_SIZE_MAX),
				4 * MACH_NET_MSG_SIZE_MAX);
#else	NeXT
	mach_net_kmsg_zone = zinit(MACH_NET_MSG_SIZE_MAX, 8 * MACH_NET_MSG_SIZE_MAX,
				MACH_NET_MSG_SIZE_MAX, FALSE, "mach_net messages");
	zcram(mach_net_kmsg_zone, kmem_alloc(kernel_map, 8 * MACH_NET_MSG_SIZE_MAX),
				8 * MACH_NET_MSG_SIZE_MAX);
#endif	NeXT
}

/*
 *	This routine is called from ipintr() (in netinet/ip_input.c)
 *	to filter incoming datagrams.
 *	Returns TRUE if a listener was found (or the packet was dropped),
 *	FALSE if the datagram should be passed on to socket code.
 *
 *	This routine is always called at splnet, and must return that way.
 *	However, it must drop interrupt level to spl0 to use any of the MACH_IPC
 *	routines (which do not block interrupts).
 */
boolean_t	receive_ip_datagram(mp)
	struct mbuf **mp;
{
	register struct mbuf		*m = *mp;
	register struct ip_plus_ports	*ipp;
	port_t				port;
	kern_msg_t			kmsgptr;
	register caddr_t		cp;
	int				space;

	/*
	 *	Get IP header and ports (for UDP or TCP) together in first mbuf.
	 *	Note: IP leaves IP header in first mbuf.
	 */
	ipp = mtod(m, struct ip_plus_ports *);
	if (ipp->ip_header.ip_hl > (sizeof (struct ip) >> 2))
		ip_stripoptions(&ipp->ip_header, (struct mbuf *)0);
	if (m->m_off > MMAXOFF || m->m_len < sizeof(struct ip_plus_ports)) {
		if ((*mp = m = m_pullup(m, sizeof(struct ip_plus_ports))) == 0)
			return(TRUE);
		ipp = mtod(m, struct ip_plus_ports *);
	}

	port = find_listener(
		ipp->ip_header.ip_src.s_addr, ipp->src_port,	/* source addr and port */
		ipp->ip_header.ip_dst.s_addr, ipp->dst_port,	/* dest addr and port */
		ipp->ip_header.ip_p);			/* protocol */

	if (port == PORT_NULL) {
		/*
		 *	No one is listening for this datagram.
		 *	Pass it on to the socket code.
		 */
		return(FALSE);
	}

	/*
	 *	Drop interrupt level to use MACH_IPC.
	 */
	(void) spl0();

	if ((kmsgptr = (kern_msg_t) zget(mach_net_kmsg_zone)) == KERN_MSG_NULL) {
		/*
		 *	Could not get a buffer - drop the datagram on the floor.
		printf("receive_ip_datagram: kern_msg_allocate returns null.\n");
		 */
		m_freem(m);

		/*
		 *	Raise interrupt level to return.
		 */
		(void) splnet();
		return(TRUE);
	}
	kmsgptr->home_zone = mach_net_kmsg_zone;

	ipc_event(current);
	ipc_event(ip_data_grams);

	/*
	 *	Undo changes to IP header made by ipintr()
	 *	before we were called. Unfortunately, the
	 *	type of service field is gone for good.
	 */
	ipp->ip_header.ip_len += (ipp->ip_header.ip_hl << 2);
	ipp->ip_header.ip_off >>= 3;

	/*
	 *	Copy mbuf chain to kmsg buffer.
	 */
	space = MACH_NET_MSG_SIZE_MAX - sizeof(struct kern_msg);
	cp = (caddr_t)(&kmsgptr->kmsg_header + 1);
	while (m && space) {
		register unsigned int len;

		len = MIN(m->m_len, space);
		bcopy(mtod(m, caddr_t), cp, len);
		cp += len;
		space -= len;
		m = m_free(m);
	}
	/*
	 *	Fill in message header and deliver it.
	 */
	kmsgptr->kmsg_header = ip_msg_template;
	kmsgptr->kmsg_header.msg_size -= space;
	kmsgptr->kmsg_header.msg_local_port = port;

	port_reference(port);
	(void) msg_queue(kmsgptr, SEND_ALWAYS, 0);

	/*
	 *	Raise interrupt level to return.
	 */
	(void) splnet();
	return(TRUE);
}

