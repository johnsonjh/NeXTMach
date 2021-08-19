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
 * $Log:	ipc_netport.c,v $
 *  1-Mar-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: Allocate only NETPORT_MSG_SIZE_MAX entries for the
 *	netport_kmsg_zone (24 * NETPORT_MSG_SIZE_MAX otherwise).
 *
 * Revision 2.9  89/03/09  20:12:11  rpd
 * 	More cleanup.
 * 
 * Revision 2.8  89/02/25  18:03:04  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.7  89/01/15  16:22:51  rpd
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  14:59:02  rpd]
 * 
 * Revision 2.6  88/10/11  10:16:09  rpd
 * 	Removed SEND_KERNEL option from msg_queue operations.
 * 	[88/10/09  08:42:56  rpd]
 * 
 * Revision 2.5  88/10/01  21:56:16  rpd
 * 	Replaced msg_remote_port with msg_local_port, and vice versa,
 * 	throughout, because now msg_local_port is the destination and
 * 	msg_remote_port the reply port in a copied-in message.
 * 	[88/10/01  21:28:05  rpd]
 * 
 * Revision 2.4  88/09/25  22:12:01  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:16:58  rpd]
 * 
 * Revision 2.3  88/08/25  18:42:30  mwyoung
 * 	Picked up fix from Rick.
 * 	[88/08/18  17:26:50  rpd]
 * 
 * Revision 2.2  88/08/06  18:17:17  rpd
 * Renamed from mach_ipc_netport.c.
 * Eliminated non-MACH_NP branch.
 * 
 * 18-Jan-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Created this module from mach_ipc_vmtp.c, by splitting off
 *	the VMTP-specific code into netport_vmtp.c.
 *
 * 14-Jan-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Removed duplicate definition of page_size.
 *	Fixed netport_queue to correctly place the server_eid in the
 *	reply port record before sending a request.
 *
 *  4-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	netisr thread changes: Removed all spl operations, removed spl
 *	fields from structures.  Added SEND_KERNEL option
 *	to msg_queue operations. (as per rds)
 *
 * 15-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 *  9-Jul-87  Rick Rashid (rfr) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 * File:	ipc_netport.c
 * Purpose:
 *	Primitive interface between IPC, network server process and
 *	transport modules.
 *	Provides fast transparent path to network for simple cases of
 *	message transmission.
 */

/*
 * XXX Possible problems.
 *
 * What happens if a port gets removed while netport_enter is executing.
 *
 * We should check to return from msg_queue when queuing a request, so
 * that we can refuse it if the port has moved.
 */
#import <mach_np.h>

#import <sys/types.h>
#import <kern/queue.h>
#import <kern/lock.h>
#import <sys/boolean.h>
#import <kern/zalloc.h>
#import <sys/message.h>
#import <kern/kern_port.h>
#import <kern/kern_msg.h>
#import <sys/kern_return.h>

#import <sys/msg_type.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/protosw.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/errno.h>
#import <sys/time.h>
#import <sys/kernel.h>

#import <net/if.h>
#import <net/route.h>

#import <netinet/in.h>
#import <netinet/in_pcb.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/ip_var.h>

#import <kern/ipc_netport.h>
#import <kern/ipc_kmesg.h>

#import <sys/param.h>		/* NBPG, CLBYTES, etc. */
#import <vm/vm_kern.h>		/* kernel_map */
#import <vm/vm_param.h>	/* page_size */

#import <kern/xpr.h>

#define Debugger(s)	kdb_kintr()
int			np_flags = 0;
void			ipc_set_external();		/* Forward */

/* 
 * NETPORT state for port
 */
#define NETPORT_STATE_NONE		0
#define NETPORT_STATE_REQ		1
#define NETPORT_STATE_RES		2

/*
 * Transport modules entry points.
 */
transport_sw_entry_t transport_switch[TR_MAX_ENTRY];
int	np_trmod = TR_TCP_ENTRY;

/*
 * Macros to call transport modules.
 */
#define transport_sendrequest(trmod,clid,kmsg,len,to,crypt)			\
	(transport_switch[(trmod)].sendrequest((clid),(kmsg),(len),		\
							(to),(crypt)))

#define transport_sendreply(trmod,trid,code,kmsg,len,crypt)			\
	(transport_switch[(trmod)].sendreply((trid),(code),(kmsg),(len),(crypt)))


struct netport_hash_entry {
	queue_chain_t			chain;
	int				(*msg_queue)();
	int				(*port_changed)();
	int				(*port_done)();
	decl_simple_lock_data(,		lock)
	network_port_t			netport;
	kern_port_t			port;
	short				ref_count;
	short				local;
	short				state;
	struct {	/* info for reply port on the client side */
		netaddr_t			dest;
		struct netport_hash_entry	*server_port;
		kern_msg_t			req_kmsg;
		int				current_seq_no;
	}				client;
	struct {	/* info for reply port on the server side */
		int				trmod;
		trid_t				trid;
		int				pending_seq_no;
	}				server;
};

typedef struct netport_hash_entry * netport_hash_t;

typedef struct {
	queue_head_t		head;
	decl_simple_lock_data(, lock)
} netport_hash_bucket_t;

/*
 * Size of network port hash table
 */
#define NETPORT_HASH_COUNT	(1 << 6)

#define netport_hash(netport) \
		(((netport.np_puid.np_uid_low) & (NETPORT_HASH_COUNT-1)))

netport_hash_bucket_t	NP_table[NETPORT_HASH_COUNT];

/*
 * Zones for handling netport records and network messages.
 */
zone_t	netport_hash_zone;
zone_t	netport_kmsg_zone;


#define KERNEL_NET_ABLE(kmsgptr) 				\
	((kmsgptr->kmsg_header.msg_simple) &&			\
	 (!(kmsgptr->kmsg_header.msg_type & 			\
		(MSG_TYPE_ENCRYPTED || MSG_TYPE_CAMELOT))))

#define RPC_REQUEST(kmsgptr)					\
	(kmsgptr->kmsg_header.msg_type & MSG_TYPE_RPC)		

#define entry_lock(entry) 	simple_lock(&entry->lock);

#define entry_unlock(entry) 	simple_unlock(&entry->lock);

#define netport_reference(entry) {			\
	simple_lock(&entry->port->port_data_lock);	\
	entry->ref_count++;				\
	simple_unlock(&entry->port->port_data_lock);	\
}

#define netport_release(entry) {				\
	simple_lock(&entry->port->port_data_lock);		\
	if (--entry->ref_count <= 0) { 				\
		kern_port_t port = entry->port;			\
		simple_unlock(&port->port_data_lock);		\
		port_release(port);				\
		ZFREE(netport_hash_zone, entry);		\
	} else {						\
		simple_unlock(&entry->port->port_data_lock);	\
	}							\
}

#define EXTERNAL_ENTRY(lp, lp_entry) {					     \
	if (lp == PORT_NULL) {				   	    	     \
		lp_entry = NULL;					     \
	} else {							     \
		simple_lock(&((kern_port_t)lp)->port_data_lock);	     \
		lp_entry = (netport_hash_t)((kern_port_t)lp)->port_external;	     \
		if (lp_entry != NULL) 					     \
			lp_entry->ref_count++;				     \
		simple_unlock(&((kern_port_t)lp)->port_data_lock);	     \
	}								     \
}

#undef  NPORT_EQUAL
#define NPORT_EQUAL(nport1, nport2) 					    \
	(!bcmp((caddr_t)&nport1, (caddr_t)&nport2, sizeof(network_port_t)))
	
kern_return_t
netport_init(ServPort)
{
	int 			i;
	static	boolean_t	done = FALSE;

	/*
	 * Avoid re-initializing when restarting the network server.
	 */
	if (done) {
		return KERN_SUCCESS;
	}
	done = TRUE;

	netport_hash_zone = zinit(sizeof(struct netport_hash_entry), 
			512 * 1024, page_size, FALSE, "netport translations");
	for (i = 0; i< NETPORT_HASH_COUNT; i++) {
		simple_lock_init(&(&NP_table[i])->lock);
		queue_init(&(&NP_table[i])->head);
	}

	netport_kmsg_zone = zinit(NETPORT_MSG_SIZE_MAX, 
				    24 * NETPORT_MSG_SIZE_MAX,
				    NETPORT_MSG_SIZE_MAX, FALSE, 
				    "netport messages");

	zcram(netport_kmsg_zone, 
#if	NeXT
	      kmem_alloc(kernel_map, 1 * NETPORT_MSG_SIZE_MAX, FALSE),
   	      round_page(1*NETPORT_MSG_SIZE_MAX));
#else	NeXT
	      kmem_alloc(kernel_map, 24 * NETPORT_MSG_SIZE_MAX, FALSE),
   	      2*NETPORT_MSG_SIZE_MAX);
#endif	NeXT

	netport_tcp_init();

	return KERN_SUCCESS;
}

netport_hash_t
netport_lookup(netport) 
register
network_port_ptr_t	netport;
{
	register
	netport_hash_bucket_t *	bucket;
	register
	netport_hash_t 		entry;

	bucket  = &NP_table[netport_hash((*netport))];
	bucket_lock(bucket);
 
	entry = (netport_hash_t) queue_first(&bucket->head); 
	while (!queue_end(&bucket->head, (queue_entry_t) entry)) { 
		if (NPORT_EQUAL((*netport), entry->netport)) {
			netport_reference(entry);
			bucket_unlock(bucket);
 			return(entry);
		} 
		entry = (netport_hash_t) queue_next(&entry->chain); 
	} 
	bucket_unlock(bucket);
	return (NULL);
}

/*
 * We accepted the request but cannot process the response.
 * This terminates the connection and puts the requestor
 * in receive state.
 */
void
netport_abort_response(trmod,trid)
	int	trmod;
	trid_t	trid;
{
	XPR(XPR_NP,("netport_abort_response(trmod=%d,trid=%d)",trmod,trid));
	transport_sendreply(trmod,trid,IPC_ABORT_REPLY,0,0,0);
}

/*
 * We cannot accept the request.  This terminates the
 * connection and causes the requestor to resend.
 */
void
netport_abort_request(trmod,trid,flush)
	int		trmod;
	trid_t		trid;
	boolean_t	flush;		/* should the client flush its entry? */
{
	int	abort = (flush ? IPC_ABORT_REQUEST_FLUSH : IPC_ABORT_REQUEST);

	XPR(XPR_NP,("netport_abort_request(trmod=%d,trid=%d,flush=%d)",trmod,trid,flush));
	transport_sendreply(trmod,trid,abort,0,0,0);
}

/*
 * The client discovered that the transaction should be aborted.
 * Cause the server to send a dummy response.
 *
 * Note: lp_entry must be locked on entry and is unlocked on exit.
 *  	 Also, the entry is released.
 */
void
netport_abort_client(lp_entry)
	netport_hash_t	lp_entry;
{
	struct KMsg		kmsg;
	netaddr_t		dest;

	dest = lp_entry->client.dest;
	XPR(XPR_NP,("netport_abort_client(lp_entry=0x%x) - dest=0x%x",lp_entry,dest));
	kmsg.netmsg_hdr.disp_hdr.disp_type = htons(DISP_IPC_ABORT);
	kmsg.netmsg_hdr.disp_hdr.src_format = htons(CONF_OWN_FORMAT);
	kmsg.netmsg_hdr.ipc_seq_no = lp_entry->client.current_seq_no;
	kmsg.netmsg_hdr.local_port  = lp_entry->netport;

	/*
	 * Should prevent another ABORT_CLIENT to be
	 * sent when there is already one pending for the
	 * same client, but this is hard because we must
	 * wait for the dummy response to clean up everything.
	 */
#if	0
	/* INCORRECT */
	if (lp_entry->state & NETPORT_STATE_REQ) {
		netport_release(lp_entry->server_port);
	}
	lp_entry->state		&= ~NETPORT_STATE_REQ;	
#endif	0

	entry_unlock(lp_entry);
	netport_release(lp_entry);

	(void) transport_sendrequest(np_trmod,0,&kmsg,
					sizeof(ipc_network_hdr_t),dest,0);
}

/*
 * MACH IPC interface routines: netport_queue, netport_changed, netport_done.
 * 
 * These routines are called by the MACH IPC facility whenever a port
 * record with a netport attached has a message queued to it, is changed
 * or is destroyed.
 */

msg_return_t	
netport_queue(kmsgptr, option, send_timeout)
	register
	kern_msg_t	kmsgptr;
	msg_option_t	option;
	int		send_timeout;
{
	register
	netport_hash_t		lp_entry, rp_entry;
	kern_return_t		ret = SEND_KERNEL_REFUSED;
	netaddr_t		dest;
	int			ipc_seq_no;
	int			trmod;
	trid_t			trid;

	XPR(XPR_NP,("netport_queue(kmsgptr=0x%x): entered",kmsgptr));
	/*
	 * Check the remote (destination) port.
	 * We cannot allow an RPC request to a port waiting for
	 * an RPC response, so just fold it into a simple IPC.
	 */
	EXTERNAL_ENTRY(kmsgptr->kmsg_header.msg_local_port,rp_entry);
	if (rp_entry == NULL) {
		XPR(XPR_NP,("netport_queue(kmsgptr=0x%x): cannot find the remote port",kmsgptr));
		return (SEND_KERNEL_REFUSED);
	}
	entry_lock(rp_entry);
	if (rp_entry->state & NETPORT_STATE_RES) {
	    	kmsgptr->kmsg_header.msg_type &= ~MSG_TYPE_RPC;
	}
	dest = rp_entry->netport.np_receiver;
	entry_unlock(rp_entry);

	/* 
         * We are either sending out a request or handling a response.
	 */
	if (RPC_REQUEST(kmsgptr)) {
		/* 
		 * We are initiating a request.
		 * There must be no requests outstanding for the local port.
		 * We mark the local port as being in request state,
		 * translate the message and invoke the request.
		 */

		/*
		 * Can we handle this message?
		 */
		if (!KERNEL_NET_ABLE(kmsgptr)) {
			netport_release(rp_entry);
			XPR(XPR_NP,("netport_queue(REQ,kmsgptr=0x%x): not NET_ABLE",kmsgptr));
			return (SEND_KERNEL_REFUSED);
		}

		/*
		 * Check to see that we know about the local port.
		 */
		EXTERNAL_ENTRY(kmsgptr->kmsg_header.msg_remote_port, lp_entry);
		if (lp_entry == NULL) {
			netport_release(rp_entry);
			XPR(XPR_NP,("netport_queue(REQ,kmsgptr=0x%x): cannot find the local port",kmsgptr));
			return (SEND_KERNEL_REFUSED);
		}

		XPR(XPR_NP,("netport_queue(REQ,kmsgptr=0x%x): lp_entry=0x%x",kmsgptr,lp_entry));
		/*
		 * The reply port had better be local.
		 */
		entry_lock(lp_entry);
		if (!lp_entry->local) {
			entry_unlock(lp_entry);
			XPR(XPR_NP,("netport_queue(REQ,kmsgptr=0x%x): local port not local",kmsgptr));
			goto netport_queue_done;
		}

		/*
		 * Now, make sure that we are not already waiting on
		 * the response to a request with this local port.
		 */
		if (lp_entry->state & NETPORT_STATE_REQ) {
			XPR(XPR_NP,("netport_queue(REQ,kmsgptr=0x%x): already waiting for a response",kmsgptr));
			netport_abort_client(lp_entry);
					/* unlocks and releases lp_entry */
			netport_release(rp_entry);
			return (SEND_KERNEL_REFUSED);
		}

		/*
		 * The message is OK. Get ready to transmit it.
		 */
		lp_entry->state		       |= NETPORT_STATE_REQ;
		lp_entry->client.dest		= dest;
		lp_entry->client.server_port 	= rp_entry;
		lp_entry->client.req_kmsg	= kmsgptr;
		ipc_seq_no = ++lp_entry->client.current_seq_no;
		kmsgptr->netmsg_hdr.local_port  = lp_entry->netport;
		entry_unlock(lp_entry);

		entry_lock(rp_entry);
		kmsgptr->netmsg_hdr.remote_port = rp_entry->netport;
		entry_unlock(rp_entry);

		kmsgptr->netmsg_hdr.disp_hdr.disp_type 	= htons(DISP_IPC_MSG);
		kmsgptr->netmsg_hdr.disp_hdr.src_format = 
						       htons(CONF_OWN_FORMAT);

		kmsgptr->netmsg_hdr.inline_size =
						kmsgptr->kmsg_header.msg_size;
		kmsgptr->netmsg_hdr.info = IPC_INFO_SIMPLE | IPC_INFO_RPC;

		kmsgptr->netmsg_hdr.ipc_seq_no 	= ipc_seq_no;
		kmsgptr->netmsg_hdr.npd_size 	= 0;
		kmsgptr->netmsg_hdr.ool_size 	= 0;
		kmsgptr->netmsg_hdr.ool_num 	= 0;
	
		XPR(XPR_NP,("netport_queue(REQ,kmsgptr=0x%x): transmitting, lp_entry=0x%x",kmsgptr,lp_entry));
		ret = transport_sendrequest(np_trmod,(long)lp_entry,
									kmsgptr,
		(sizeof (ipc_network_hdr_t)) + kmsgptr->kmsg_header.msg_size,
									dest,0);

		if ((ret != TR_SUCCESS)) {
			entry_lock(lp_entry);
			lp_entry->state &= ~NETPORT_STATE_REQ;
			entry_unlock(lp_entry);
			ret = SEND_KERNEL_REFUSED;
			XPR(XPR_NP,("netport_queue(REQ,kmsgptr=0x%x): transport failed, ret=%d",kmsgptr,ret));
			goto netport_queue_done;
		} else {
			ret = KERN_SUCCESS;
		}

		/*
		 * We need one extra reference for the netport
		 * stored in the csr and the one stored in the
		 * other netport.
		 */
		netport_reference(lp_entry);
		netport_reference(rp_entry);
	} else {
		XPR(XPR_NP,("netport_queue(kmsgptr=0x%x): handling a response",kmsgptr));
		entry_lock(rp_entry);
		if (rp_entry->state & NETPORT_STATE_RES) {
			trmod = rp_entry->server.trmod;
			trid = rp_entry->server.trid;
			rp_entry->state &= ~NETPORT_STATE_RES;
			kmsgptr->netmsg_hdr.remote_port = rp_entry->netport;
			entry_unlock(rp_entry);

			EXTERNAL_ENTRY(kmsgptr->kmsg_header.msg_remote_port, 
								lp_entry);
			/*
			 * Can we handle this message?
			 *
			 * XXX Note: there is nothing wrong with a null
			 *     local port in the response.
			 */
			if ((lp_entry == NULL)) {
				XPR(XPR_NP,("netport_queue(RES,kmsgptr=0x%x): no local port",kmsgptr));
				netport_abort_response(trmod,trid);
				netport_release(rp_entry);
				return (SEND_KERNEL_REFUSED);
			} 

			if (!KERNEL_NET_ABLE(kmsgptr)) {
				XPR(XPR_NP,("netport_queue(RES,kmsgptr=0x%x): not NET_ABLE",kmsgptr));
				netport_abort_response(trmod,trid);
				goto netport_queue_done;
			}

			kmsgptr->netmsg_hdr.disp_hdr.disp_type 	= 
						       htons(DISP_IPC_MSG);
			kmsgptr->netmsg_hdr.disp_hdr.src_format = 
						       htons(CONF_OWN_FORMAT);

			entry_lock(lp_entry);
			kmsgptr->netmsg_hdr.local_port  = lp_entry->netport;
			entry_unlock(lp_entry);

			kmsgptr->netmsg_hdr.inline_size =
						kmsgptr->kmsg_header.msg_size;
			kmsgptr->netmsg_hdr.info = IPC_INFO_SIMPLE;

			kmsgptr->netmsg_hdr.ipc_seq_no 	= 0;
			kmsgptr->netmsg_hdr.npd_size 	= 0;
			kmsgptr->netmsg_hdr.ool_size 	= 0;
			kmsgptr->netmsg_hdr.ool_num 	= 0;
	
			XPR(XPR_NP,("netport_queue(RES,kmsgptr=0x%x): transmitting, trid=%d",kmsgptr,trid));
			transport_sendreply(trmod,trid,IPC_SUCCESS,kmsgptr,
		(sizeof (ipc_network_hdr_t)) + kmsgptr->kmsg_header.msg_size, 0);

			kern_msg_destroy(kmsgptr);
			ret = KERN_SUCCESS;
		} else {
			entry_unlock(rp_entry);
			netport_release(rp_entry);
			XPR(XPR_NP,("netport_queue(RES,kmsgptr=0x%x): not waiting for a response",kmsgptr));
			return (SEND_KERNEL_REFUSED);
		}
	}

netport_queue_done:
	netport_release(rp_entry);
	netport_release(lp_entry);
	XPR(XPR_NP,("netport_queue(kmsgptr=0x%x): exiting",kmsgptr));
	return (ret);
}

void
netport_done(port) 
	kern_port_t	port;
{
	netport_hash_t	entry;
	network_port_t	netport;
	
	EXTERNAL_ENTRY(port, entry);			/* 1st reference */
	XPR(XPR_NP,("netport_done(port=0x%x,entry=0x%x)",port,entry));
	if (entry != NULL) {
		entry_lock(entry);
		netport = entry->netport;
		if (entry->state & NETPORT_STATE_REQ) {
			netport_abort_client(entry);	/* release 1st */
		} else {
			if (entry->state & NETPORT_STATE_RES) {
				netport_abort_response(entry->server.trmod,
							entry->server.trid);
				entry->state &= ~NETPORT_STATE_RES;
			}
			entry_unlock(entry);
			netport_release(entry);		/* release 1st */
		}
		(void) netport_remove(PORT_NULL, netport);
	}
}

void
netport_changed(port) 
	kern_port_t	port;
{
	netport_hash_t	entry;

	EXTERNAL_ENTRY(port, entry);
	XPR(XPR_NP,("netport_changed(port=0x%x,entry=0x%x)",port,entry));
	if (entry != NULL) {
		entry_lock(entry);
		if (entry->state & NETPORT_STATE_REQ) {
			netport_abort_client(entry);
		} else {
			if (entry->state & NETPORT_STATE_RES) {
				netport_abort_response(entry->server.trmod,
							entry->server.trid);
				entry->state &= ~NETPORT_STATE_RES;
			}
			entry_unlock(entry);
			netport_release(entry);
		}
	}
}

/*
 * Interface to network interrupt handler.
 *
 * Whenever a netport message arrives from the network
 * one of those routines is called.
 * Incoming requests are handled either by forwarding
 * an IPC message to the appropriate port or by responding
 * to the client saying that the request cannot be processed.
 * Responses are matched to a waiting netport record
 * and reflected to the client.
 */

void
netport_handle_rq(trmod,trid,kmsgptr,len,from,crypt_level,broadcast)
	int			trmod;
	trid_t			trid;
	kern_msg_t		kmsgptr;
	int			len;
	netaddr_t		from;
	int			crypt_level;
	boolean_t		broadcast;
{
	register
	netport_hash_t		rp_entry;
	register
	netport_hash_t		lp_entry;
	kern_port_t		rp;
	kern_port_t		lp;
	register
	ipc_netmsg_hdr_t	*net_in_msg;

	if ((kmsgptr == 0) || (len == 0)) {
		panic("netport_handle_rq: no message");
	}

	/*
	 * Find out if the incoming request is acceptable.
	 */
	net_in_msg = (ipc_netmsg_hdr_t *)&(kmsgptr->netmsg_hdr);

	if (net_in_msg->disp_hdr.src_format != htons(CONF_OWN_FORMAT)) {
		netport_abort_request(trmod,trid,TRUE);
		ZFREE(netport_kmsg_zone, kmsgptr);
		XPR(XPR_NP,("netport_handle_rq(kmsgptr=0x%x): bad format",kmsgptr));
		return;
	}

	/*
	 * Check for a request from the client side of a transaction
	 * to abort that transaction (on the server side).
	 * In that case, we must generate a dummy reply and return.
	 *
	 * Note: we must be prepared to deal with cases where the
	 * transaction has already finished for other reasons.
	 * The present code is not totally correct: it should
	 * consider the possibility that the abort request arrives
	 * at the server before the transaction it should abort. XXX
	 */
	if (net_in_msg->disp_hdr.disp_type == DISP_IPC_ABORT) {
		register
		netport_hash_t	lp_entry;

		lp_entry = netport_lookup(&net_in_msg->local_port);
		if (lp_entry) {
			entry_lock(lp_entry);
			if ((lp_entry->state & NETPORT_STATE_RES) &&
					(lp_entry->server.pending_seq_no == 
						net_in_msg->ipc_seq_no)) {
				netport_abort_response(lp_entry->server.trmod,
							lp_entry->server.trid);
				lp_entry->state &= ~NETPORT_STATE_RES;
			}
			entry_unlock(lp_entry);
			netport_release(lp_entry);
		}
		transport_sendreply(trmod,trid,0,0,0,0);
		ZFREE(netport_kmsg_zone, kmsgptr);
		XPR(XPR_NP,("netport_handle_rq(kmsgptr=0x%x): IPC_ABORT",kmsgptr));
		return;
	}

	rp_entry = netport_lookup(&net_in_msg->remote_port);
	if (rp_entry == NULL) {
		XPR(XPR_NP,("netport_handle_rq(kmsgptr=0x%x): cannot find remote port",kmsgptr));
		netport_abort_request(trmod,trid,FALSE);
		ZFREE(netport_kmsg_zone, kmsgptr);
		return;
	} else {
		entry_lock(rp_entry);
		if (!rp_entry->local) {
			entry_unlock(rp_entry);
			netport_release(rp_entry);
			XPR(XPR_NP,("netport_handle_rq(kmsgptr=0x%x): remote port not local",kmsgptr));
			netport_abort_request(trmod,trid,FALSE);
			ZFREE(netport_kmsg_zone, kmsgptr);
			return;
		}
		rp = rp_entry->port;
		entry_unlock(rp_entry);
	}

	lp_entry = netport_lookup(&net_in_msg->local_port);
	if (lp_entry == NULL) {
		netport_release(rp_entry);
		XPR(XPR_NP,("netport_handle_rq(kmsgptr=0x%x): cannot find local port",kmsgptr));
		netport_abort_request(trmod,trid,FALSE);
		ZFREE(netport_kmsg_zone, kmsgptr);
		return;
	}

	entry_lock(lp_entry);
	lp = lp_entry->port;
	if (net_in_msg->info & IPC_INFO_RPC) {
		/*
		 * Watch out! We have not exchanged remote and local ports
		 * anywhere, so on the server the local port is really the
		 * client port.
		 */
		lp_entry->state = NETPORT_STATE_RES;
		lp_entry->server.trmod = trmod;
		lp_entry->server.trid = trid;
		lp_entry->server.pending_seq_no = net_in_msg->ipc_seq_no;
		XPR(XPR_NP,("netport_handle_rq(kmsgptr=0x%x): doing RPC",kmsgptr));
	} else {
		entry_unlock(lp_entry);
		netport_release(rp_entry);
		netport_release(lp_entry);
		XPR(XPR_NP,("netport_handle_rq(kmsgptr=0x%x): not RPC",kmsgptr));
		netport_abort_request(trmod,trid,FALSE);
		ZFREE(netport_kmsg_zone, kmsgptr);
		return;
	}
	entry_unlock(lp_entry);

	kmsgptr->kmsg_header.msg_remote_port  = (port_t) lp;
	port_reference(lp);

	kmsgptr->kmsg_header.msg_local_port = (port_t) rp;
	port_reference(rp);

	/*
	 * Make sure we do not accidentaly forward an RPC
	 * with another RPC.
	 */
	kmsgptr->kmsg_header.msg_type &= ~MSG_TYPE_RPC;
	(void) msg_queue(kmsgptr, SEND_ALWAYS, 0);

	netport_release(lp_entry);
	netport_release(rp_entry);
}


void
netport_handle_rp(clid,code,kmsgptr,len)
	int			clid;
	int			code;
	kern_msg_t		kmsgptr;
	int			len;
{
	register
	netport_hash_t		rp_entry;
	netport_hash_t		lp_entry;
	kern_port_t		rp;
	kern_port_t		lp;
	kern_msg_t		req_kmsg;
	ipc_netmsg_hdr_t *	net_in_msg;

	/*
	 * Find the waiting netport, and extract the relevant data.
	 */
	if ((rp_entry = (netport_hash_t)clid) == NULL) {
		/*
		 * A null client ID is probably from the response
		 * to a client abort. In any case, there is nothing
		 * we can do but ignore it.
		 */
		if (kmsgptr) {
			ZFREE(netport_kmsg_zone, kmsgptr);
		}
		return;
	}

	XPR(XPR_NP,("netport_handle_rp(kmsgptr=0x%x): entered, rp_entry=0x%x",kmsgptr,rp_entry));
	/*
	 * We have the netport. 
	 * There is already a netport_reference done when
	 * the netport was stored in the csr, just take it over.
	 */
	entry_lock(rp_entry);
	if (!(rp_entry->state & NETPORT_STATE_REQ)) {
		/*
		 * Ignore if not waiting for a response.
		 */
		entry_unlock(rp_entry);
		netport_release(rp_entry);
		if (kmsgptr) {
			ZFREE(netport_kmsg_zone, kmsgptr);
		}
		XPR(XPR_NP,("netport_handle_rp(kmsgptr=0x%x): not waiting for response",kmsgptr));
		return;
	}
	rp_entry->state &= ~NETPORT_STATE_REQ;

	/*
	 * Get the probable local port entry. We take over
	 * the netport_reference made when this field was set.
	 */
	lp_entry = rp_entry->client.server_port;
	req_kmsg = rp_entry->client.req_kmsg;
	rp	 = rp_entry->port;

	/* XXX BEGIN PARANOIA */
	rp_entry->client.server_port = NULL;
	rp_entry->client.req_kmsg = NULL;
	/* XXX END PARANOIA */

	entry_unlock(rp_entry);

	/*
	 * Worry about error responses and aborts.
	 */
	XPR(XPR_NP,("netport_handle_rp(kmsgptr=0x%x): code=%d",kmsgptr,code));
	switch(code) {
		case IPC_SUCCESS:
			break;

		case TR_FAILURE:
		case TR_SEND_FAILURE:
		{
			/*
			 * Safer to disable further requests
			 * through the same channel.
			 */
			network_port_t		lp_net;

			entry_lock(lp_entry);
			lp_net = lp_entry->netport;
			entry_unlock(lp_entry);
			(void)netport_remove(PORT_NULL, lp_net);
			/*
			 * In the case of a transport failure, there
			 * may not be a message at all.
			 */
			if (kmsgptr) {
				ZFREE(netport_kmsg_zone, kmsgptr);
			}
			goto netport_redo_request;
		}

		case IPC_ABORT_REQUEST:
			ZFREE(netport_kmsg_zone, kmsgptr);
			goto netport_redo_request;

		case IPC_ABORT_REQUEST_FLUSH:
		{
			 /*
			  * Request aborted with flush.
			  * Probably wrong machine type.
			  * Flush network mapping.
			  */
			network_port_t	rp_net;

			entry_lock(rp_entry);
			rp_net = rp_entry->netport;
			entry_unlock(rp_entry);
			netport_remove(PORT_NULL, rp_net);
			ZFREE(netport_kmsg_zone, kmsgptr);
			goto netport_redo_request;
		}

		case IPC_ABORT_REPLY:
			ZFREE(netport_kmsg_zone, kmsgptr);
			goto netport_dealloc_request;

		default:
			printf("netport_handle_rp: unexpected code: %d\n",code);
			if (np_flags & NP_DEBUG)
				Debugger("NP");
			if (kmsgptr) {
				ZFREE(netport_kmsg_zone, kmsgptr);
			}
			goto netport_redo_request;
	}

	/*
	 * At this point, we are sure that we have an IPC response,
	 * and that the request has been accepted.
	 */

	net_in_msg = (ipc_netmsg_hdr_t *)&(kmsgptr->netmsg_hdr);

	/*
	 * XXX Sanity check: is the byte order OK?
	 */
	if (net_in_msg->disp_hdr.src_format != htons(CONF_OWN_FORMAT)) {
		printf("Got a response with a wrong format\n");
		if (np_flags & NP_DEBUG)
			Debugger("NP");
		/*
		 * There is nothing we can do but ignore it.
		 */
		ZFREE(netport_kmsg_zone, kmsgptr);
		XPR(XPR_NP,("netport_handle_rp(kmsgptr=0x%x): bad format",kmsgptr));
		goto netport_dealloc_request;
	}

	/*
	 * XXX Sanity check: is the destination port what we think it is?
	 */
	entry_lock(rp_entry);
	if (!(NPORT_EQUAL(rp_entry->netport,net_in_msg->remote_port))) {
		printf("Got a response with an unknown destination port\n");
		if (np_flags & NP_DEBUG)
			Debugger("NP");
		/*
		 * There is nothing we can do but ignore it.
		 */
		entry_unlock(rp_entry);
		ZFREE(netport_kmsg_zone, kmsgptr);
		XPR(XPR_NP,("netport_handle_rp(kmsgptr=0x%x): bad destination port",kmsgptr));
		goto netport_dealloc_request;
	}
	entry_unlock(rp_entry);

	/*
	 * Let's see if we got lucky, and the local port is the old
	 * server port. Otherwise, fix it!
	 */
	entry_lock(lp_entry);
	if (NPORT_EQUAL(lp_entry->netport,net_in_msg->local_port)) {
		lp = lp_entry->port;
		entry_unlock(lp_entry);
	} else {
		entry_unlock(lp_entry);
		netport_release(lp_entry);
		lp_entry = netport_lookup(&net_in_msg->local_port);
		if (lp_entry == NULL) {
			lp = KERN_PORT_NULL;
		} else {
			entry_lock(lp_entry);
			lp = lp_entry->port;
			entry_unlock(lp_entry);
		}
	}

	kmsgptr->kmsg_header.msg_remote_port  = (port_t) lp;
	port_reference(lp);

	kmsgptr->kmsg_header.msg_local_port = (port_t) rp;
	port_reference(rp);

	XPR(XPR_NP,("netport_handle_rp(kmsgptr=0x%x): delivering the msg",kmsgptr));
	(void) msg_queue(kmsgptr, SEND_ALWAYS, 0);

netport_dealloc_request:
	/*
	 * The request was accepted. Deallocate the local copy.
	 */
	kern_msg_destroy(req_kmsg);
	netport_release(rp_entry);
	if (lp_entry != NULL) {
		netport_release(lp_entry);
	}
	return;

netport_redo_request:
	/*
	 * The request was not accepted. Reflect it to the network server.
	 *
	 * XXX With the present mach_ipc module, this will call netport_queue
	 * again.
	 */
	XPR(XPR_NP,("netport_handle_rp(kmsgptr=0x%x): redoing the request",kmsgptr));
	msg_queue(req_kmsg, SEND_ALWAYS, 0);

	netport_release(rp_entry);
	netport_release(lp_entry);
}

/*
 * External IPC interface routines: netport_enter, netport_remove
 *
 * These routines can be called by network server process to associate
 * a port with a network port identifier and to remove the relationship.
 */
kern_return_t
netport_enter(ServPort, netport, port, local)
kern_port_t	ServPort;
network_port_t	netport;
kern_port_t	port;
boolean_t	local;
{
	netport_hash_bucket_t *	bucket;
	netport_hash_t	entry;
	network_port_t	old_netport;
	int		old_seq_no = 0;

	if (port == PORT_NULL) return (KERN_FAILURE);

	/*
	 * Remove any existing entry for this port.
	 */
 	EXTERNAL_ENTRY(port, entry);
	XPR(XPR_NP,("netport_enter(port=0x%x,entry=0x%x)",port,entry));
	if (entry != NULL) {
		entry_lock(entry);
		old_netport = entry->netport;
		/*
		 * Preserve the current sequence number if we are
		 * only changing an existing entry.
		 */
		if (NPORT_EQUAL(old_netport, netport)) {
			old_seq_no = entry->client.current_seq_no;
		}
		entry_unlock(entry);
		(void) netport_remove(PORT_NULL, old_netport);
		netport_release(entry);
	}

	bucket  = &NP_table[netport_hash(netport)];
	bucket_lock(bucket);
 
	entry = (netport_hash_t) queue_first(&bucket->head); 
	while (!queue_end(&bucket->head, (queue_entry_t) entry)) { 
		if (NPORT_EQUAL(netport, entry->netport)) {
			/* Port already registered! */
			bucket_unlock(bucket);
			return (KERN_FAILURE);
		} 
		entry = (netport_hash_t) queue_next(&entry->chain); 
	} 
	entry			= (netport_hash_t)zalloc(netport_hash_zone);
	entry->netport 		= netport;
	entry->port    		= port;
	port_reference(port);
  	entry->ref_count	= 1;
	entry->local		= local;
	if (local) {
		/* If local, we don't worry about queueing,
		 * unless, of course, a NETPORT RPC is in progress.
	   	 */
		entry->msg_queue	= NULL;
	} else {
		entry->msg_queue	= (int (*)())netport_queue;
	}
	entry->port_changed 		= (int (*)())netport_changed;
	entry->port_done		= (int (*)())netport_done;
	entry->state			= NETPORT_STATE_NONE;
	entry->client.current_seq_no 	= old_seq_no;
	simple_lock_init(&entry->lock);
	queue_enter(&bucket->head, entry, netport_hash_t, chain);  
	/*
	 * Keep the lock on the bucket a little while longer,
	 * to make sure the port and entry stay valid here.
	 */
	ipc_set_external(port, entry);
	bucket_unlock(bucket);
	return (KERN_SUCCESS);
 }

kern_return_t
netport_remove(ServPort, netport)
	kern_port_t	ServPort;
	network_port_t	netport;
{
	netport_hash_bucket_t *	bucket;
	netport_hash_t		entry;

	bucket  = &NP_table[netport_hash(netport)];
	bucket_lock(bucket);
 
	entry = (netport_hash_t) queue_first(&bucket->head); 
	while (!queue_end(&bucket->head, (queue_entry_t) entry)) { 
		if (NPORT_EQUAL(netport, entry->netport)) {
			queue_remove(&bucket->head, entry, 
					netport_hash_t, chain);
			bucket_unlock(bucket);
			/*
			 * XXX Could somebody remove the entry after
			 * accessing it through the port record?
			 * -> No, everybody must do netport_remove.
			 */
			ipc_set_external(entry->port, NULL);
			entry_lock(entry);
			if (entry->state & NETPORT_STATE_REQ) {
				netport_abort_client(entry);	/* release 1st */
			} else {
				if (entry->state & NETPORT_STATE_RES) {
					netport_abort_response(entry->server.trmod,
							entry->server.trid);
					entry->state &= ~NETPORT_STATE_RES;
				}
				entry_unlock(entry);
				netport_release(entry);		/* release 1st */
			}
			XPR(XPR_NP,("netport_remove: entry=0x%x",entry));
			return(KERN_SUCCESS);
		} 
		entry = (netport_hash_t) queue_next(&entry->chain); 
	} 
	bucket_unlock(bucket);
	return (KERN_SUCCESS);
}

/*
 *	Routine:	ipc_set_external [exported]
 *	Purpose:
 *		Sets external port data pointer.
 *	Conditions:
 *		No locks held on entry or exit.
 */
void
ipc_set_external(port, external) 
	kern_port_t port;
	int **external;
{
	port_lock(port);
	port->port_external = external;
	port_unlock(port);
}





