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
 * $Log:	netport_tcp.c,v $
 * 28-Feb-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: use kernel_task in place of first_task.
 *
 * Revision 2.4  89/03/09  20:14:36  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:07:09  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  88/08/06  18:23:39  rpd
 * Moved from sys/mach_ipc_netport.h to kern/ipc_netport.h.
 * 
 * 24-Jan-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Fixed to byte-swap the TCP port number.
 *
 * 16-Jan-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 * File:	netport_tcp.c
 * Purpose:
 *	Front-end to the TCP system for the netport system
 *	implementing network IPC in the kernel.
 */

/*
 * This module is copied from the TCP tranport module of the user-state
 * network server, with only a minimum of changes.
 */

#import <sys/types.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <netinet/in.h>
#import <sys/kern_return.h>
#import <sys/port.h>
#import <kern/queue.h>
#import <kern/lock.h>
#import <kern/thread.h>
#import <kern/task.h>
#import <kern/ipc_netport.h>
#import <kern/kern_msg.h>
#import <kern/zalloc.h>
#import <sys/param.h>
#import <kern/xpr.h>
#import <vm/vm_param.h>

#ifndef	NULL
#define NULL	0
#endif	NULL

#if	NeXT
#define	Debugger(s)	panic(s)
#define queue_enter_first	queue_enter_head
#else	NeXT
#define	Debugger(s)	kdb_kintr()
#endif	NeXT

/*
 * Definitions for compatibility with the network server coding conventions
 * and debugging mechanism.
 */
#define PRIVATE		/**/
#define PUBLIC		/**/
#define EXPORT		/**/
#define BEGIN(name)	{
#define END		}
#define RETURN(val)	return (val)
#define RET		return

#define DEBUG0(a,b,c)			XPR(XPR_NPTCP,(c,0,0,0,0,0))
#define DEBUG1(a,b,c,p1)		XPR(XPR_NPTCP,(c,p1,0,0,0,0))
#define DEBUG2(a,b,c,p1,p2)		XPR(XPR_NPTCP,(c,p1,p2,0,0,0))
#define DEBUG3(a,b,c,p1,p2,p3)		XPR(XPR_NPTCP,(c,p1,p2,p3,0,0))
#define DEBUG4(a,b,c,p1,p2,p3,p4)	XPR(XPR_NPTCP,(c,p1,p2,p3,p4,0))
#define DEBUG5(a,b,c,p1,p2,p3,p4,p5)	XPR(XPR_NPTCP,(c,p1,p2,p3,p4,p5))
#define DEBUG6(a,b,c,p1,p2,p3,p4,p5,p6)	XPR(XPR_NPTCP,(c,p1,p2,p3,p4,p5))

#define INCSTAT(s)				/**/
#define msg		np_msg
#define errno		np_errno
#define ERROR(fmt)	{ 			\
	np_printf fmt;				\
	if (np_flags & NP_DEBUG)		\
		Debugger("NP");			\
}
static char		np_msg[200];
static int		np_errno;


#define mutex			slock
#define mutex_lock(l)		simple_lock(l)
#define mutex_unlock(l)		simple_unlock(l)
#define mutex_init(l)		simple_lock_init(l)

typedef int			kern_cond_t;
#define condition_init(c)	/**/
#define condition_wait(c,l)	{	\
	simple_unlock(l);		\
	sleep((caddr_t)c,PZERO+1);	\
	simple_lock(l);			\
}
#define condition_signal(c)	wakeup((caddr_t)c)

#if	NeXT
#else	NeXT
extern task_t	first_task;
#endif	NeXT

/*
 * Macros to derive a TCP connection ID from a trid obtained from a client 
 * and vice-versa.
 */
#define SET_TCPID(tcpid,trid)	{ (tcpid) = (trid).v1; }
#define SET_TRID(trid,tcpid)	{ (trid).v1 = (tcpid); }


/*
 * Transaction records.
 */
typedef struct tcp_trans {
	int			state;	/* see defines below */
	unsigned long		trid;
	int			client_id;
	kern_msg_t		kmsg;
	int			len;
	int			crypt_level;
/*	int			(*reply_proc)(); */
	queue_chain_t	transq;	/* list of pending/waiting transactions */
} tcp_trans_t, *tcp_trans_ptr_t;

#define TCP_TR_INVALID	0
#define TCP_TR_PENDING	1	/* awaiting a reply */
#define TCP_TR_WAITING	2	/* awaiting transmission */

zone_t	tcp_trans_zone;



/*
 * Forward declarations.
 */
void	np_tcp_conn_handler();


/*
 * TCP port to be used by the Mach netport service.
 */
#define TCP_NETMSG_PORT	2454


/*
 * Debugging flags.
 */
#define TCP_DBG_MAJOR	(0x1)	/* major events */
#define TCP_DBG_CRASH	(0x2)	/* host crashes */
#define TCP_DBG_VERBOSE	(0x4)	/* verbose output */

/*
 * Connection records.
 */
typedef	struct tcp_conn {
	int			state;	/* see defines below */
	struct socket		*sock;	/* socket structure */
	thread_t		th;	/* service thread */
	struct mutex		lock;	/* lock for this record */
	kern_cond_t		cond;	/* to wake up the service thread */
	netaddr_t		dest;	/* peer for current connection */
	queue_head_t	trans;	/* list of pending/waiting transactions */
	int			count;	/* number of pending/waiting trans */
	queue_chain_t	connq;	/* list of records */
	unsigned long		incarn;	/* incarnation number */
	tcp_ctl_t		ctlbuf;	/* for xmit control header */
} tcp_conn_t, *tcp_conn_ptr_t;

#define TCP_INVALID	0
#define TCP_FREE	1
#define TCP_CONNECTED	2
#define TCP_OPENING	3
#define TCP_CLOSING	4
#define TCP_CLOSED	5

/*
 * Static declarations.
 */
PRIVATE tcp_conn_t		conn_vec[32];	/* connection records */

PRIVATE queue_head_t	conn_lru;	/* LRU list of active conn */
PRIVATE int			conn_num;	/* number of active conn */
PRIVATE queue_head_t	conn_free;	/* list of free conn */
PRIVATE kern_cond_t	conn_cond;	/* to wake up listener */
PRIVATE int			conn_closing;	/* number of conn in TCP_CLOSING */
PRIVATE struct mutex		conn_lock;	/* lock for conn_lru & conn_free */


/*
 * Transport IDs are composed of 16 bits for the client side and 16 bits
 * for the server side. The client side is just a counter, to be matched
 * between the message and the transaction record. The server side is composed
 * of 8 bits of index of the connection record in the conn_vec array and
 * 8 bits of incarnation number for this connection record.
 *
 * We can afford not to protect the counter for client-side IDs with a lock,
 * because transaction records for one connection are protected by the lock
 * that connection, and they never move from one connection to another.
 *
 * XXX This is not completely foolproof if there is A LOT of traffic,
 * but it's cheap.
 */
PRIVATE unsigned long			trid_counter;
#define cptoix(cp)			(((cp) - conn_vec)/sizeof(tcp_conn_t))
#define ixtocp(id)			((tcp_conn_ptr_t)&(conn_vec[(id)]))
#define TRID_SET_CLIENT(trid)		{ trid = (trid_counter++) & 0xffff; }
#define TRID_GET_CLIENT(trid,cl)	{ (cl) = (trid) & 0xffff; }
#define TRID_SET_SERVER(trid,sv)	{ (trid) |= \
					(cptoix(sv) << 24) | ((sv)->incarn << 16);}
#define TRID_GET_SERVER(trid,sv)	{ (sv) = ixtocp((trid) >> 24); \
		if ((((trid) >> 16) & 0xff) != (sv)->incarn) (sv) = NULL; }



/*
 * Limits on connected sockets.
 */
#define TCP_CONN_STEADY		6	/* steady-state max [6] */
#define TCP_CONN_OPENING	8	/* max open/opening [8] */
#define TCP_CONN_MAX		10	/* absolute maximum [10] */


/*
 * Zone for kmsg's to be used for incoming messages.
 */
extern zone_t	netport_kmsg_zone;
#define DATA_SIZE_MAX 				\
		(NETPORT_MSG_SIZE_MAX		\
		- sizeof(struct KMsg)		\
		+ sizeof(tcp_ctl_t)		\
		+ sizeof(ipc_network_hdr_t)	\
		+ sizeof(msg_header_t))



/*
 * Macro for transmission of a simple control message.
 *
 * cp->lock must be held throughout.
 */
#define tcp_xmit_control(cp,ctlcode,a_trid,a_code,ret) {	\
	int	b_len;						\
								\
	(cp)->ctlbuf.ctl = htonl(ctlcode);			\
	(cp)->ctlbuf.trid = htonl(a_trid);			\
	(cp)->ctlbuf.code = htonl(a_code);			\
	(cp)->ctlbuf.size = 0;					\
	(cp)->ctlbuf.crypt_level = 0;				\
	b_len = sizeof(tcp_ctl_t);				\
	ret = mach_tcp_send(PORT_NULL,(cp)->sock,		\
				&((cp)->ctlbuf),&b_len,0);	\
	INCSTAT(tcp_send);					\
	DEBUG6(TCP_DBG_VERBOSE,0,2803,cp,ctlcode,a_trid,	\
					a_code,ret,errno);	\
}

/*
 * Macro for transmission of data.
 *
 * cp->lock must be held throughout.
 */
#define tcp_xmit_data(cp,ctlcode,a_trid,a_code,a_kmsg,a_len,a_crypt,ret) {	\
	int		b_len;						\
									\
	if (a_kmsg) {							\
		(a_kmsg)->tcp_ctl.ctl = htonl(ctlcode);			\
		(a_kmsg)->tcp_ctl.trid = htonl(a_trid);			\
		(a_kmsg)->tcp_ctl.code = htonl(a_code);			\
		(a_kmsg)->tcp_ctl.size = htonl(a_len);			\
		(a_kmsg)->tcp_ctl.crypt_level = htonl(a_crypt);		\
									\
		DEBUG6(TCP_DBG_VERBOSE,0,2800,cp,ctlcode,a_trid,	\
					a_code,&a_kmsg,a_crypt);	\
									\
		/*							\
		 * XXX Worry about data encryption.			\
		 */							\
									\
		/*							\
		 * Send everything in one pass.				\
		 */							\
		b_len = sizeof(tcp_ctl_t) + (a_len);			\
		ret = mach_tcp_send(PORT_NULL,(cp)->sock,		\
					&((a_kmsg)->tcp_ctl),&b_len,0);	\
		INCSTAT(tcp_send);					\
		DEBUG3(TCP_DBG_VERBOSE,0,2801,b_len,ret,errno);	\
	} else {							\
		tcp_xmit_control((cp),(ctlcode),(a_trid),(a_code),(ret));	\
	}								\
}



/*
 * np_printf --
 *
 * Special version of printf to avoid using sprintf in ERROR.
 */
np_printf(msg,fmt,p1,p2,p3,p4,p5,p6)
	char	*msg;
	char	*fmt;
	int	p1;
	int	p2;
	int	p3;
	int	p4;
	int	p5;
	int	p6;
{
	printf(fmt,p1,p2,p3,p4,p5,p6);
	printf("\n");
}



/*
 * np_tcp_init_conn --
 *
 * Allocate and initialize a new TCP connection record.
 *
 * Parameters:
 *
 * Results:
 *
 * pointer to the new record.
 *
 * Side effects:
 *
 * Starts a new thread to handle the connection.
 *
 * Note:
 *
 * conn_lock must be acquired before calling this routine.
 * It is held throughout its execution.
 */
PRIVATE tcp_conn_ptr_t np_tcp_init_conn()
BEGIN("np_tcp_init_conn")
	tcp_conn_ptr_t	cp;
	int		i;
	char		name[40];

	/*
	 * Find an unused connection record in the conn_vec array.
	 * We could have used the global memory allocator for that,
	 * but since there are few connection records, why bother...
	 *
	 * conn_lock guarantees mutual exclusion.
	 */
	cp = NULL;
	for (i = 0; i < 32; i++) {
		if (conn_vec[i].state == TCP_INVALID) {
			cp = &conn_vec[i];
			break;
		}
	}
	if (cp == NULL) {
		panic("The TCP module cannot allocate a new connection record");
	}

	cp->state = TCP_FREE;
	cp->sock = 0;
	cp->count = 0;
	cp->dest = 0;
	mutex_init(&cp->lock);
	mutex_lock(&cp->lock);
	condition_init(&cp->cond);
	queue_init(&cp->trans);
	cp->th = NULL;
#if	NeXT
	(void) kernel_thread(kernel_task,np_tcp_conn_handler);
#else	NeX T
	(void) kernel_thread(first_task,np_tcp_conn_handler);
#endif	NeXT
/*	sprintf(name,"np_tcp_conn_handler(0x%x)",cp); */

	DEBUG2(TCP_DBG_MAJOR,0,2805,cp,cp->th);

	mutex_unlock(&cp->lock);

	RETURN(cp);
END



/*
 * np_tcp_close_conn --
 *
 * Arrange to close down one TCP connection as soon as possible.
 *
 * Parameters:
 *
 * Results:
 *
 * Side effects:
 *
 * Note:
 *
 * conn_lock must be acquired before calling this routine.
 * It is held throughout its execution.
 */
PRIVATE void np_tcp_close_conn()
BEGIN("np_tcp_close_conn")
	tcp_conn_ptr_t		first;
	tcp_conn_ptr_t		cp;
	kern_return_t		ret;

	/*
	 * Look for an old connection to recycle.
	 */
	first = (tcp_conn_ptr_t)queue_first(&conn_lru);
	cp = (tcp_conn_ptr_t)queue_last(&conn_lru);
	while (cp != first) {
		if (cp->count == 0) {
			mutex_lock(&cp->lock);
			if ((cp->count == 0) && (cp->state == TCP_CONNECTED)) {
				break;
			} else {
				mutex_unlock(&cp->lock);
			}
		}
		cp = (tcp_conn_ptr_t)queue_prev(&cp->connq);
	}
	if (cp == first) {
		/*
		 * We are over-committed. We will try again
		 * to close something at the next request or
		 * reply.
		 *
		 * XXX We could also set a timer to kill someone at
		 * random, to give new clients a chance.
		 */
		DEBUG2(TCP_DBG_MAJOR,0,2838,conn_num,conn_closing);
	} else {
		/*
		 * Close this unused connection.
		 */
		DEBUG4(TCP_DBG_MAJOR,0,2839,cp,cp->dest,conn_num,conn_closing);
		cp->state = TCP_CLOSING;
		conn_closing++;
		tcp_xmit_control(cp,TCP_CTL_CLOSEREQ,0,0,ret);
		mutex_unlock(&cp->lock);
	}

	RET;
END



/*
 * netport_tcp_sendrequest --
 *
 * Send a request through the TCP interface.
 *
 * Parameters:
 *
 *	client_id	: an identifier assigned by the client to this transaction
 *	kmsg		: the data to be sent
 *	len		: the length of the data in kmsg
 *	to		: the destination of the request
 *	crypt_level	: whether the data should be encrypted
 *
 * Results:
 *
 *	TR_SUCCESS or a specific failure code.
 *
 * Side effects:
 *
 * Design:
 *
 * Note:
 *
 */
EXPORT int netport_tcp_sendrequest(client_id,kmsg,len,to,crypt_level)
int		client_id;
kern_msg_t	kmsg;
int		len;
netaddr_t	to;
int		crypt_level;
BEGIN("netport_tcp_sendrequest")
	tcp_conn_ptr_t		first;
	tcp_conn_ptr_t		cp;
	tcp_trans_ptr_t		tp;
	kern_return_t		ret;

	mutex_lock(&conn_lock);
	DEBUG4(TCP_DBG_VERBOSE,0,2837,to,client_id,conn_num,conn_closing);
	INCSTAT(tcp_requests_sent);

	/*
	 * Find an open connection to the destination.
	 */
	first = (tcp_conn_ptr_t)queue_first(&conn_lru);
	cp = first;
	while (!queue_end(&conn_lru,(queue_entry_t)cp)) {
		if (cp->dest == to) {
			break;
		}
		cp = (tcp_conn_ptr_t)queue_next(&cp->connq);
	}

	if (queue_end(&conn_lru,(queue_entry_t)cp)) {
		/*
		 * Could not find an open connection.
		 */
		if (conn_num < TCP_CONN_OPENING) {
			/*
			 * Immediately start a new connection.
			 */
			if (queue_empty(&conn_free)) {
				/*
				 * Initialize a new connection record.
				 */
				cp = np_tcp_init_conn();
			} else {
				cp = (tcp_conn_ptr_t)queue_first(&conn_free);
				queue_remove(&conn_free,cp,
							tcp_conn_ptr_t,connq);
			}
			mutex_lock(&cp->lock);
			DEBUG2(TCP_DBG_MAJOR,0,2840,cp,to);
			queue_enter_first(&conn_lru,cp,tcp_conn_ptr_t,connq);
			conn_num++;
			cp->dest = to;
			cp->state = TCP_OPENING;
			cp->count = 1;
#ifdef	notdef
			/*
			 * This is done when placing cp on the free list.
			 */
			queue_init(&cp->trans);
#endif	notdef
			condition_signal(&cp->cond);
			mutex_unlock(&cp->lock);
			if ((conn_num - conn_closing) > TCP_CONN_STEADY) {
				np_tcp_close_conn();
			}
			mutex_unlock(&conn_lock);
		} else {
			/*
			 * We are over-committed. Tell the caller to wait.
			 */
			DEBUG0(TCP_DBG_MAJOR,0,2841);
			if ((conn_num - conn_closing) > TCP_CONN_STEADY) {
				np_tcp_close_conn();
			}
			mutex_unlock(&conn_lock);
			RETURN(TR_OVERLOAD);
		}
	} else {
		/*
		 * Found an open connection. Use it!
		 */
		DEBUG2(TCP_DBG_VERBOSE,0,2842,cp,cp->dest);
		if (cp != first) {
			/*
			 * Place the record at the head of the queue.
			 */
			queue_remove(&conn_lru,cp,tcp_conn_ptr_t,connq);
			queue_enter_first(&conn_lru,cp,tcp_conn_ptr_t,connq);
		}
		if ((conn_num - conn_closing) > TCP_CONN_STEADY) {
			np_tcp_close_conn();
		}
		mutex_lock(&cp->lock);
		cp->count++;
		mutex_unlock(&conn_lock);
	}

	/*
	 * At this point, we have a lock on a connection record for the
	 * right destination. See if we can transmit the data.
	 */

	/*
	 * Link the transaction record in the connection record.
	 */
	ZALLOC(tcp_trans_zone,tp,tcp_trans_ptr_t);
	if (tp == NULL) {
		panic("netport_tcp_sendrequest: cannot get a transaction record");
	}
	tp->client_id = client_id;
	TRID_SET_CLIENT(tp->trid);

	DEBUG4(TCP_DBG_VERBOSE,0,2843,cp,cp->state,tp,tp->trid);

	if (cp->state == TCP_FREE) {
		panic("TCP module trying to transmit on a free connection");
	}

	if (cp->state == TCP_CONNECTED) {
		/*
		 * Send all the data on the socket.
		 */
		tp->state = TCP_TR_PENDING;
		tcp_xmit_data(cp,TCP_CTL_REQUEST,tp->trid,0,kmsg,len,crypt_level,ret);
		if (ret != KERN_SUCCESS) {
			/*
			 * Something went wrong. Most probably, the client is dead.
			 */
			DEBUG2(TCP_DBG_CRASH,0,2844,cp->dest,errno);
			cp->count--;
			mutex_unlock(&cp->lock);
			ZFREE(tcp_trans_zone,tp);
			RETURN(TR_FAILURE);
		}
	} else {
		tp->state = TCP_TR_WAITING;
		tp->kmsg = kmsg;
		tp->len = len;
		tp->crypt_level = crypt_level;
	}
	queue_enter(&cp->trans,tp,tcp_trans_ptr_t,transq);
	mutex_unlock(&cp->lock);

	RETURN(TR_SUCCESS);
END



/*
 * netport_tcp_sendreply --
 *
 * Send a response through the TCP interface.
 *
 * Parameters:
 *
 *	trid		: transport-level ID for a previous operation on this
 *			  transaction
 *	code		: a return code to be passed to the client.
 *	kmsg		: the data to be sent
 *	len		: the length of the data in kmsg
 *	crypt_level	: whether the data should be encrypted
 *
 * Results:
 *
 *	TR_SUCCESS or a specific failure code.
 *
 * Side effects:
 *
 * Design:
 *
 * Note:
 *
 */
EXPORT int netport_tcp_sendreply(trid,code,kmsg,len,crypt_level)
trid_t		trid;
int		code;
kern_msg_t	kmsg;
int		len;
int		crypt_level;
BEGIN("netport_tcp_sendreply")
	tcp_conn_ptr_t	cp;
	kern_return_t	ret;
	int		tcpid;

	SET_TCPID(tcpid,trid);
	TRID_GET_SERVER(tcpid,cp);

	/*
	 * If the client has died, the connection record may
	 * already have been reused, and we may be sending this reply
	 * to the wrong machine. This should be detected by the 
	 * incarnation number in the trid.
	 */
	if (cp == NULL) {
		DEBUG1(TCP_DBG_CRASH,0,2847,tcpid);
		RETURN(TR_FAILURE);
	}

	mutex_lock(&cp->lock);

	DEBUG4(TCP_DBG_VERBOSE,0,2845,tcpid,cp,cp->dest,cp->state);
	INCSTAT(tcp_replies_sent);

	if (cp->state != TCP_CONNECTED) {
		/*
		 * The client has died or the connection has just
		 * been dropped. Drop the reply.
		 */
		mutex_unlock(&cp->lock);
		RETURN(TR_FAILURE);
	}

	cp->count--;
	tcp_xmit_data(cp,TCP_CTL_REPLY,tcpid,code,kmsg,len,crypt_level,ret);

	if (ret != KERN_SUCCESS) {
		/*
		 * Something went wrong. Most probably, the client is dead.
		 */
		DEBUG2(TCP_DBG_CRASH,0,2846,cp->dest,errno);
		mutex_unlock(&cp->lock);
		RETURN(TR_FAILURE);
	}

	mutex_unlock(&cp->lock);

	/*
	 * Update the LRU list of active connections and check for
	 * excess connections.
	 */
	mutex_lock(&conn_lock);
	if (cp != (tcp_conn_ptr_t)queue_first(&conn_lru)) {
		/*
		 * Place the record at the head of the queue.
		 */
		queue_remove(&conn_lru,cp,tcp_conn_ptr_t,connq);
		queue_enter_first(&conn_lru,cp,tcp_conn_ptr_t,connq);
	}
	if ((conn_num - conn_closing) > TCP_CONN_STEADY) {
		np_tcp_close_conn();
	}
	mutex_unlock(&conn_lock);

	RETURN(TR_SUCCESS);
END



/*
 * np_tcp_conn_handler_open --
 *
 * Handler for one connection - opening phase.
 *
 * Parameters:
 *
 * cp: pointer to the connection record.
 *
 * Results:
 *
 * TRUE if the connection was successfully opened, FALSE otherwise.
 *
 * Side effects:
 *
 * Transactions waiting in the connection record are initiated.
 *
 * Note:
 *
 * cp->lock must be locked on entry. It is also locked on exit, but
 * it may be unlocked during the execution of this procedure.
 */
PRIVATE boolean_t np_tcp_conn_handler_open(cp)
	tcp_conn_ptr_t	cp;
BEGIN("np_tcp_conn_handler_open")
	tcp_trans_ptr_t		tp;
	struct socket		*cs;
	struct sockaddr_in	sname;
/*	netaddr_t		peeraddr; */
	kern_return_t		ret;

	sname.sin_family = AF_INET;
	sname.sin_port = htons(TCP_NETMSG_PORT);
	sname.sin_addr.s_addr = (u_long)(cp->dest);
/*	peeraddr = cp->dest; */

	/*
	 * Unlock the record while we are waiting for the connection
	 * to be established.
	 */
	mutex_unlock(&cp->lock);

	mutex_lock(&conn_lock);
	ret = mach_tcp_socket(PORT_NULL,&cs);
	mutex_unlock(&conn_lock);
	if (ret != KERN_SUCCESS) {
		ERROR((msg,"np_tcp_conn_handler.socket failed: errno=%d",errno));
		panic("tcp");
	}

	if (np_flags & NP_SODEBUG) {
		cs->so_options |= SO_DEBUG;
	}

	ret = mach_tcp_connect(PORT_NULL,cs,&sname,sizeof(struct sockaddr_in));
	if (ret != KERN_SUCCESS) {
		DEBUG2(TCP_DBG_CRASH,0,2815,0,errno);
		mutex_lock(&cp->lock);
		RETURN(FALSE);
	}
	INCSTAT(tcp_connect);

	mutex_lock(&cp->lock);
	cp->sock = cs;
	cp->state = TCP_CONNECTED;
	DEBUG3(TCP_DBG_VERBOSE,0,2816,cp,cs,0);

	/*
	 * Look for transactions waiting to be transmitted.
	 */
	tp = (tcp_trans_ptr_t)queue_first(&cp->trans);
	while (!queue_end(&cp->trans,(queue_entry_t)tp)) {
		DEBUG2(TCP_DBG_VERBOSE,0,2817,tp,tp->state);
		if (tp->state == TCP_TR_WAITING) {
			tp->state = TCP_TR_PENDING;
			tcp_xmit_data(cp,TCP_CTL_REQUEST,tp->trid,0,
					tp->kmsg,tp->len,tp->crypt_level,ret);
			if (ret != KERN_SUCCESS) {
				RETURN(FALSE);
			}
		}
		tp = (tcp_trans_ptr_t)queue_next(&tp->transq);
	}

	RETURN(TRUE);
END



/*
 * np_tcp_conn_handler_active --
 *
 * Handler for one connection - active phase.
 *
 * Parameters:
 *
 * cp: pointer to the connection record.
 *
 * Results:
 *
 * Exits when the connection should be closed.
 *
 * Note:
 *
 * For now, the data received on the connection is only kept until the
 * higher-level handler procedure (disp_in_request or reply_proc) returns.
 * This allows the use of a data buffer on the stack.
 *
 */
PRIVATE void np_tcp_conn_handler_active(cp)
	tcp_conn_ptr_t	cp;
BEGIN("np_tcp_conn_handler_active")
	struct socket		*cs;
	netaddr_t		peeraddr;
	tcp_trans_ptr_t		tp;
	kern_return_t		ret;
	kern_msg_t		kmsg;
	kern_msg_t		new_kmsg;
	int			len;
	caddr_t			bufp;		/* current location in data */
	int			buf_count;	/* data available in buf */
	int			buf_free;	/* free space in buf */
	int			data_size;
	unsigned long		trid;
	trid_t			trid_cl;
	int			s;

	peeraddr = cp->dest;	/* OK not to lock at this point */
	cs = cp->sock;
	new_kmsg = NULL;

	/*
	 * Enter the recv loop.
	 */
	for (;;) {

		/*
		 * Get a fresh kmsg for a receive buffer.
		 */
		if (new_kmsg == NULL) {
			ZALLOC(netport_kmsg_zone,kmsg,kern_msg_t);
			if (kmsg == NULL) {
				panic("netport out of kmsgs");
			}
			kmsg->home_zone = netport_kmsg_zone;
			bufp = (caddr_t)&(kmsg->tcp_ctl);
			buf_count = 0;
			buf_free = DATA_SIZE_MAX;
		} else {
			/*
			 * There is already some data obtained
			 * in the previous pass.
			 */
			kmsg = new_kmsg;
		}
		
		/*
		 * Get at least a tcp control header in the
		 * buffer.
		 */
		while (buf_count < sizeof(tcp_ctl_t)) {
			len = buf_free;
			ret = mach_tcp_recv(PORT_NULL,cs,bufp,&len,0);
			if ((ret != KERN_SUCCESS) || (len <= 0)) {
				ZFREE(netport_kmsg_zone,kmsg);
				RET;
			}
			INCSTAT(tcp_recv);
			DEBUG2(TCP_DBG_VERBOSE,0,2820,ret,peeraddr);
			buf_count += len;
			buf_free -= len;
			bufp += len;
		}

		/*
		 * Do all the required byte-swapping (Sigh!).
		 */
		kmsg->tcp_ctl.ctl		= ntohl(kmsg->tcp_ctl.ctl);
		kmsg->tcp_ctl.trid		= ntohl(kmsg->tcp_ctl.trid);
		kmsg->tcp_ctl.code		= ntohl(kmsg->tcp_ctl.code);
		kmsg->tcp_ctl.size		= ntohl(kmsg->tcp_ctl.size);
		kmsg->tcp_ctl.crypt_level	= ntohl(kmsg->tcp_ctl.crypt_level);

		/*
		 * Read any user data.
		 * Advance the current data pointer.
		 */
		buf_count -= sizeof(tcp_ctl_t);
		data_size = kmsg->tcp_ctl.size;
		if (data_size > (buf_count + buf_free)) {
			ERROR((msg,"Netport: size too big from 0x%x\n", peeraddr));
			ZFREE(netport_kmsg_zone,kmsg);
			RET;
		}
		while (buf_count < data_size) {
			len = buf_free;
			ret = mach_tcp_recv(PORT_NULL,cs,bufp,&len,0);
			if ((ret != KERN_SUCCESS) || (len <= 0)) {
				ZFREE(netport_kmsg_zone,kmsg);
				RET;
			}
			INCSTAT(tcp_recv);
			buf_count += len;
			buf_free -= len;
			bufp += len;
		}

		/*
		 * If we received more data than we asked for,
		 * transfer the excess in a new kmsg.
		 */
		if (buf_count > data_size) {
			ZALLOC(netport_kmsg_zone,new_kmsg,kern_msg_t);
			if (new_kmsg == NULL) {
				panic("netport out of kmsgs");
			}
			new_kmsg->home_zone = netport_kmsg_zone;
			bcopy(((caddr_t)&(kmsg->netmsg_hdr)) + data_size,
						(caddr_t)&(new_kmsg->tcp_ctl), 
						buf_count - data_size);
			buf_count -= data_size;
			bufp = (caddr_t)(&(new_kmsg->tcp_ctl)) + buf_count;
			buf_free = DATA_SIZE_MAX - buf_count;
		} else {
			new_kmsg = NULL;
		}

		/*
		 * XXX Worry about encryption.
		 */

		/*
		 * Now process the message.
		 */
		DEBUG1(TCP_DBG_VERBOSE,0,2826,kmsg->tcp_ctl.ctl);
		switch(kmsg->tcp_ctl.ctl) {
			case TCP_CTL_REQUEST:
				INCSTAT(tcp_requests_rcvd);
				mutex_lock(&cp->lock);
				cp->count++;
				if (cp->state == TCP_CLOSING) {
					cp->state = TCP_CONNECTED;
					mutex_unlock(&cp->lock);
					mutex_lock(&conn_lock);
					conn_closing--;
					mutex_unlock(&conn_lock);
				} else {
					mutex_unlock(&cp->lock);
				}
				trid = kmsg->tcp_ctl.trid;
				TRID_SET_SERVER(trid,cp);
				SET_TRID(trid_cl,trid);
				(void) netport_handle_rq(TR_TCP_ENTRY,trid_cl,
						kmsg,data_size,peeraddr,
						kmsg->tcp_ctl.crypt_level,FALSE);
				/*
				 * The kmsg will be destroyed by netmsg_input_rq.
				 */
#ifdef	notdef
				if (disp_ret != DISP_WILL_REPLY) {
					mutex_lock(&cp->lock);
					DEBUG3(TCP_DBG_VERBOSE,0,2827,peeraddr,
								trid,disp_ret);
					tcp_xmit_control(cp,TCP_CTL_REPLY,trid,
								disp_ret,ret);
					cp->count--;
					mutex_unlock(&cp->lock);
					if (ret != KERN_SUCCESS) {
						RET;
					}
				}
#endif	notdef
				break;

			case TCP_CTL_REPLY:
				INCSTAT(tcp_replies_rcvd);
				mutex_lock(&cp->lock);
				if (cp->state == TCP_CLOSING) {
					cp->state = TCP_CONNECTED;
					mutex_unlock(&cp->lock);
					mutex_lock(&conn_lock);
					conn_closing--;
					mutex_unlock(&conn_lock);
					mutex_lock(&cp->lock);
				}
				/*
				 * Find the transaction record.
				 */
				TRID_GET_CLIENT(kmsg->tcp_ctl.trid,trid);
				tp = (tcp_trans_ptr_t)queue_first(&cp->trans);
				while (!queue_end(&cp->trans,(queue_entry_t)tp)) {
					if (tp->trid == trid) {
						break;
					}
					tp = (tcp_trans_ptr_t)queue_next(&tp->transq);
				}
				if (queue_end(&cp->trans,(queue_entry_t)tp)) {
					ERROR((msg,
"np_tcp_conn_handler_active: cannot find the transaction record for a reply"));
					mutex_unlock(&cp->lock);
					ZFREE(netport_kmsg_zone,kmsg);
				} else {
					queue_remove(&cp->trans,tp,
							tcp_trans_ptr_t,transq);
					cp->count--;
					mutex_unlock(&cp->lock);
					DEBUG1(TCP_DBG_VERBOSE,0,2828,tp);
					netport_handle_rp(tp->client_id,
						kmsg->tcp_ctl.code,kmsg,data_size);
					/*
					 * The kmsg will be destroyed by
					 * the reply_proc.
					 */
					ZFREE(tcp_trans_zone,tp);
				}
				break;

			case TCP_CTL_CLOSEREQ:
				mutex_lock(&cp->lock);
				if (cp->count == 0) {
					/*
					 * Send CLOSEREP.
					 */
					DEBUG1(TCP_DBG_MAJOR,0,2829,cp->dest);
					tcp_xmit_control(cp,TCP_CTL_CLOSEREP,
									0,0,ret);
					if (cp->state != TCP_CLOSING) {
						cp->state = TCP_CLOSED;
					}
					mutex_unlock(&cp->lock);
					ZFREE(netport_kmsg_zone,kmsg);
					RET;
				} else {
					/*
					 * We have some data in
					 * transit. Nothing more
					 * should be needed.
					 */
					DEBUG2(TCP_DBG_MAJOR,0,2830,cp->dest,
									cp->count);
					cp->state = TCP_CONNECTED;
					mutex_unlock(&cp->lock);
					ZFREE(netport_kmsg_zone,kmsg);
				}
				break;

			case TCP_CTL_CLOSEREP:
				mutex_lock(&cp->lock);
				DEBUG1(TCP_DBG_MAJOR,0,2831,cp->dest);
				/*
				 * cp->state can only be TCP_CLOSING:
				 *
				 * We have sent a CLOSEREQ, and set the
				 * state to TCP_CLOSING then. If the state
				 * has changed since then, it must be because
				 * we have received data. But this data can only
				 * be a request, because we had nothing going on
				 * when we sent the CLOSEREQ. This CLOSEREQ must
				 * arrive at the other end before our reply
				 * because TCP does not reorder messages. But
				 * then the CLOSEREQ will be rejected because
				 * of the pending transaction.
				 */
				mutex_unlock(&cp->lock);
				ZFREE(netport_kmsg_zone,kmsg);
				RET;

			default:
				ERROR((msg,
		"np_tcp_conn_handler_active: received an unknown ctl code: %d",
								kmsg->tcp_ctl.ctl));
				ZFREE(netport_kmsg_zone,kmsg);
				break;
		}
	}

END



/*
 * np_tcp_conn_handler_close --
 *
 * Handler for one connection - closing phase.
 *
 * Parameters:
 *
 * cp: pointer to the connection record.
 *
 * Results:
 *
 * none.
 *
 * Note:
 *
 */
PRIVATE void np_tcp_conn_handler_close(cp)
	tcp_conn_ptr_t	cp;
BEGIN("np_tcp_conn_handler_close")
	tcp_trans_ptr_t		tp;
	int			s;

	/*
	 * Some transactions might be initiated after the active phase exits
	 * and before this phase starts. Hopefully, they will be stopped by
	 * the TCP_CLOSING or TCP_CLOSED states, or the send will fail.
	 */
	mutex_lock(&conn_lock);
	mutex_lock(&cp->lock);
	mach_tcp_close(PORT_NULL,cp->sock);
	INCSTAT(tcp_close);
	if (cp->state == TCP_CLOSING) {
		conn_closing--;
	}
	cp->state = TCP_FREE;

	/*
	 * Go down the list of waiting/pending transactions
	 * and abort them.
	 * The client is of course free to retry them later.
	 */
	while (!queue_empty(&cp->trans)) {
		tp = (tcp_trans_ptr_t)queue_first(&cp->trans);
		DEBUG3(TCP_DBG_VERBOSE,0,2834,tp,tp->state,tp->client_id);
		if (tp->state == TCP_TR_WAITING) {
			netport_handle_rp(tp->client_id,TR_SEND_FAILURE,0,0);
		} else {
			netport_handle_rp(tp->client_id,TR_FAILURE,0,0);
		}
		queue_remove(&cp->trans,tp,tcp_trans_ptr_t,transq);
		ZFREE(tcp_trans_zone,tp);
	}
	queue_init(&cp->trans);
	cp->count = 0;
	queue_remove(&conn_lru,cp,tcp_conn_ptr_t,connq);
	queue_enter(&conn_free,cp,tcp_conn_ptr_t,connq);
	mutex_unlock(&cp->lock);
	conn_num--;
	DEBUG1(TCP_DBG_MAJOR,0,2835,conn_num);
	if (conn_num == (TCP_CONN_MAX - 1)) {
		/*
		 * OK to start accepting connections again.
		 */
		DEBUG0(TCP_DBG_MAJOR,0,2836);
		condition_signal(&conn_cond);
	}
	mutex_unlock(&conn_lock);

	RET;
END



/*
 * np_tcp_conn_handler --
 *
 * Handler for one connection.
 *
 * Parameters:
 *
 * Results:
 *
 *	Should never exit.
 *
 * Note:
 *
 * The first thing the thread must do is locate the connection record which
 * it is to service. This is guaranteed to succeed because there are exactly
 * as many threads as there are valid connection records.
 *
 * For clarity, this code is split into three different procedures handling
 * the opening, active and closing phases of the life of the connection.
 *
 */
PRIVATE void np_tcp_conn_handler()
BEGIN("np_tcp_conn_handler")
	tcp_conn_ptr_t	cp;
	int		i;
	boolean_t	active;

	/*
	 * Find the connection record.
	 */
	mutex_lock(&conn_lock);
	cp = NULL;
	for (i = 0; i < 32; i++) {
		if (conn_vec[i].state != TCP_INVALID) {
			cp = &conn_vec[i];
			if (cp->th == NULL) {
				cp->th = current_thread();
				break;
			} else {
				cp = NULL;
			}
		}
	}
	mutex_unlock(&conn_lock);
	if (cp == NULL) {
		panic("TCP connection handler cannot find a connection record");
	}

	/*
	 * Service loop.
	 */
	for (;;) {
		/*
		 * First wait to be activated.
		 */
		mutex_lock(&cp->lock);
		while(cp->state == TCP_FREE) {
			DEBUG0(TCP_DBG_VERBOSE,0,2811);
			condition_wait(&cp->cond,&cp->lock);
		}

		/*
		 * At this point, the state is either TCP_OPENING (local open)
		 * or TCP_CONNECTED (remote open).
		 */
		DEBUG3(TCP_DBG_VERBOSE,0,2812,cp,cp->state,cp->dest);

		if (cp->state == TCP_OPENING) {
			/*
			 * Open a new connection.
			 */
			active = np_tcp_conn_handler_open(cp);
		} else {
			active = TRUE;
		}
		cp->incarn = (cp->incarn++) & 0xff;
		mutex_unlock(&cp->lock);

		if (active) {
			DEBUG3(TCP_DBG_MAJOR,0,2813,cp,cp->sock,cp->dest);
			np_tcp_conn_handler_active(cp);
			DEBUG3(TCP_DBG_MAJOR,0,2814,cp,cp->sock,cp->dest);
		}

		/*
		 * Close the connection.
		 */
		np_tcp_conn_handler_close(cp);
	}
END



/*
 * np_tcp_listener --
 *
 * Handler for the listener socket.
 *
 * Parameters:
 *
 * Results:
 *
 *	Should never exit.
 *
 * Note:
 *
 */
PRIVATE void np_tcp_listener()
BEGIN("np_tcp_listener")
	struct socket		*s;
	struct socket		*newsock;
	kern_return_t		ret;
	struct sockaddr_in	sname;
	int			snamelen;
	tcp_conn_ptr_t		cp;

	/*
	 * First create the listener socket.
	 */
	mutex_lock(&conn_lock);
	ret = mach_tcp_socket(PORT_NULL,&s);
	mutex_unlock(&conn_lock);
	if (ret != KERN_SUCCESS) {
		ERROR((msg,"np_tcp_listener.socket failed: errno=%d",errno));
		panic("tcp");
	}
	sname.sin_family = AF_INET;
	sname.sin_port = htons(TCP_NETMSG_PORT);
	sname.sin_addr.s_addr = INADDR_ANY;
	ret = mach_tcp_bind(PORT_NULL,s,&sname,sizeof(struct sockaddr_in));
	if (ret != KERN_SUCCESS) {
		ERROR((msg,"np_tcp_listener.bind failed: errno=%d",errno));
		panic("tcp");
	}
	ret = mach_tcp_listen(PORT_NULL,s,2);
	if (ret != KERN_SUCCESS) {
		ERROR((msg,"np_tcp_listener.listen failed: errno=%d",errno));
		panic("tcp");
	}
	DEBUG1(TCP_DBG_VERBOSE,0,2806,s);

	/*
	 * Loop forever accepting connections.
	 */
	for (;;) {
		mutex_lock(&conn_lock);
		while (conn_num >= TCP_CONN_MAX) {
			DEBUG1(TCP_DBG_VERBOSE,0,2810,conn_num);
			condition_wait(&conn_cond,&conn_lock);
		}

		mutex_unlock(&conn_lock);
		DEBUG0(TCP_DBG_VERBOSE,0,2807);
		snamelen = sizeof(struct sockaddr_in);
		ret = mach_tcp_accept(PORT_NULL,s,&sname,&snamelen,&newsock);
		if (ret != KERN_SUCCESS) {
			ERROR((msg,
				"np_tcp_listener.accept failed: errno=%d",errno));
			continue;
		}
		INCSTAT(tcp_accept);
		DEBUG0(TCP_DBG_VERBOSE,0,2808);

		if (np_flags & NP_SODEBUG) {
			newsock->so_options |= SO_DEBUG;
		}

		mutex_lock(&conn_lock);
		if (queue_empty(&conn_free)) {
			/*
			 * Initialize a new connection record.
			 */
			cp = np_tcp_init_conn();
		} else {
			cp = (tcp_conn_ptr_t)queue_first(&conn_free);
			queue_remove(&conn_free,cp,tcp_conn_ptr_t,connq);
		}
		mutex_lock(&cp->lock);
		DEBUG4(TCP_DBG_MAJOR,0,2809,ret,cp,
					sname.sin_addr.s_addr,sname.sin_port);
		queue_enter_first(&conn_lru,cp,tcp_conn_ptr_t,connq);
		conn_num++;
		cp->sock = newsock;
		cp->dest = (netaddr_t)(sname.sin_addr.s_addr);
		cp->state = TCP_CONNECTED;
		cp->count = 0;
#ifdef	notdef
		/*
		 * This is done when placing cp on the free list.
		 */
		queue_init(&cp->trans);
#endif	notdef
		condition_signal(&cp->cond);
		mutex_unlock(&cp->lock);
		if ((conn_num - conn_closing) > TCP_CONN_STEADY) {
			np_tcp_close_conn();
		}
		mutex_unlock(&conn_lock);
	}

END



/*
 * netport_tcp_init --
 *
 * Initialises the TCP transport protocol.
 *
 * Parameters:
 *
 * Results:
 *
 *	FALSE : we failed to initialise the TCP transport protocol.
 *	TRUE  : we were successful.
 *
 * Side effects:
 *
 *	Initialises the TCP protocol entry point in the switch array.
 *	Allocates the listener port and creates a thread to listen to the network.
 *
 */
EXPORT boolean_t netport_tcp_init()
BEGIN("netport_tcp_init")
	int		i;
	tcp_conn_ptr_t	cp;

	/*
	 * Initialize the set of connection records and the lists.
	 */
	for (i = 0; i < 32; i++) {
		conn_vec[i].state = TCP_INVALID;
		conn_vec[i].incarn = 0;
	}
	mutex_init(&conn_lock);
	mutex_lock(&conn_lock);
	condition_init(&conn_cond);
	queue_init(&conn_lru);
	queue_init(&conn_free);
	conn_num = 0;
	conn_closing = 0;
	trid_counter = 10;

	/*
	 * Create a first connection record (just a test).
	 */
	cp = np_tcp_init_conn();
	queue_enter(&conn_free,cp,tcp_conn_ptr_t,connq);

	/*
	 * Set up the entry in the transport switch.
	 */
	transport_switch[TR_TCP_ENTRY].sendrequest = netport_tcp_sendrequest;
	transport_switch[TR_TCP_ENTRY].sendreply = netport_tcp_sendreply;

	/*
	 * Initialize the zone for transaction records.
	 */
	tcp_trans_zone = zinit(sizeof(tcp_trans_t), 64 * 1024, page_size, FALSE, 
					    "netport TCP transaction records");

	/*
	 * Initialize the TCP interface.
	 */
	mach_tcp_init(PORT_NULL,NULL);

	/*
	 * Start the listener.
	 */
#if	NeXT
	(void) kernel_thread(kernel_task,np_tcp_listener);
#else	NeX T
	(void) kernel_thread(first_task,np_tcp_listener);
#endif	NeXT

	/*
	 * Get the show on the road...
	 */
	DEBUG0(TCP_DBG_MAJOR,0,2804);
	mutex_unlock(&conn_lock);
	RETURN(TRUE);

END





