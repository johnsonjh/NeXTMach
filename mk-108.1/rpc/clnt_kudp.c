/*
 * HISTORY
 * 15-Mar-90  Morris Meyer (mmeyer) at NeXT
 *	Fix for case when nfs_pagein happens on behalf of a task without
 *	a proc structure (with its sigmask).
 *  
 *  2-Mar-90  Morris Meyer (mmeyer) at NeXT
 *	Changes in clntkudp_callit_addr() to set the signal mask.  NFS I/O
 *	requests were getting interrupted when SIGALRMs happened.
 *
 *  9-May-89  Peter King (king) at NeXT
 *	Sun Bugfixes: 10102533 - Make kernel rpc interruptable.
 *
 * 26-Jan-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes.
 *
 * 28-Oct-87  Peter King (king) at NeXT, Inc.
 *	Original Sun source, ported to Mach
 *
 */ 

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.59 88/02/16
 */


/*
 * clnt_kudp.c
 * Implements a kernel UPD/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/proc.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/mbuf.h>
#import <net/if.h>
#import <net/route.h>
#import <netinet/in.h>
#import <netinet/in_pcb.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/rpc_msg.h>

#import <machine/spl.h>

struct mbuf	*ku_recvfrom();
int		ckuwakeup();
extern int	wakeup();
static int	bindresvport();

enum clnt_stat	clntkudp_callit();
void		clntkudp_abort();
void		clntkudp_error();
bool_t		clntkudp_freeres();
bool_t		clntkudp_control();
void		clntkudp_destroy();

void		xdrmbuf_init();

/*
 * Operations vector for UDP/IP based RPC
 */
/* static */ struct clnt_ops udp_ops = {
	clntkudp_callit,	/* do rpc call */
	clntkudp_abort,		/* abort call */
	clntkudp_error,		/* return error status */
	clntkudp_freeres,	/* free results */
	clntkudp_destroy,	/* destroy rpc handle */
	clntkudp_control	/* the ioctl() of rpc */
};

/*
 * Private data per rpc handle.  This structure is allocated by
 * clntkudp_create, and freed by cku_destroy.
 */
struct cku_private {
	u_int			 cku_flags;	/* see below */
	CLIENT			 cku_client;	/* client handle */
	int			 cku_retrys;	/* request retrys */
	struct socket		*cku_sock;	/* open udp socket */
	struct sockaddr_in	 cku_addr;	/* remote address */
	struct rpc_err		 cku_err;	/* error status */
	XDR			 cku_outxdr;	/* xdr routine for output */
	XDR			 cku_inxdr;	/* xdr routine for input */
	u_int			 cku_outpos;	/* position of in output mbuf */
	char			*cku_outbuf;	/* output buffer */
	char			*cku_inbuf;	/* input buffer */
	struct mbuf		*cku_inmbuf;	/* input mbuf */
	struct ucred		*cku_cred;	/* credentials */
};

struct {
	int	rccalls;
	int	rcbadcalls;
	int	rcretrans;
	int	rcbadxids;
	int	rctimeouts;
	int	rcwaits;
	int	rcnewcreds;
	int	rcbadverfs;
} rcstat;


#define	ptoh(p)		(&((p)->cku_client))
#define	htop(h)		((struct cku_private *)((h)->cl_private))

/* cku_flags */
#define	CKU_TIMEDOUT	0x001
#define	CKU_BUSY	0x002
#define	CKU_WANTED	0x004
#define	CKU_BUFBUSY	0x008
#define	CKU_BUFWANTED	0x010
#define CKU_ONCE_ONLY	0x020
#if	NeXT
#define	CKU_INTERRUPTABLE	0x800
#endif	NeXT

/* Times to retry */
#define	RECVTRIES	2
#define	SNDTRIES	4

int	clntkudpxid;		/* transaction id used by all clients */

clntkudp_once(cl, flag)
	CLIENT  *cl;
{
	struct cku_private *p = (struct cku_private *)cl->cl_private;

	if (flag != 0) {
		p->cku_flags |= CKU_ONCE_ONLY;
	} else {
		p->cku_flags &= ~CKU_ONCE_ONLY;
	}
}

#if	NeXT
clntkudp_interruptable(cl, interruptable)
	CLIENT	*cl;
	bool_t	interruptable;
{
	struct cku_private *p = (struct cku_private *)cl->cl_private;

	if (interruptable) {
		p->cku_flags |= CKU_INTERRUPTABLE;
	} else {
		p->cku_flags &= ~CKU_INTERRUPTABLE;
	}
}

/*
 * Realloc the udp buffer with memory supplied by caller. 
 * Useful to avoid fragmentation problems.
 */
void
clntkudp_realloc(
		 CLIENT	*cl,
		 char *mem
		 )
{
	struct cku_private *p = (struct cku_private *)cl->cl_private;

	bcopy(p->cku_outbuf, mem, UDPMSGSIZE);
	kfree(p->cku_outbuf, UDPMSGSIZE);
	p->cku_outbuf = mem;
}
#endif	NeXT

static
noop()
{
}

static
buffree(p)
	struct cku_private *p;
{
	p->cku_flags &= ~CKU_BUFBUSY;
	if (p->cku_flags & CKU_BUFWANTED) {
		p->cku_flags &= ~CKU_BUFWANTED;
		wakeup((caddr_t)&p->cku_outbuf);
	}
}

/*
 * Create an rpc handle for a udp rpc connection.
 * Allocates space for the handle structure and the private data, and
 * opens a socket.  Note sockets and handles are one to one.
 */
CLIENT *
clntkudp_create(addr, pgm, vers, retrys, cred)
	struct sockaddr_in *addr;
	u_long pgm;
	u_long vers;
	int retrys;
	struct ucred *cred;
{
	register CLIENT *h;
	register struct cku_private *p;
	int error = 0;
	struct rpc_msg call_msg;
	struct mbuf *m, *mclgetx();
	extern int nfs_portmon;

	current_thread()->allocInProgress = TRUE;
	
#ifdef RPCDEBUG
	rpc_debug(4, "clntkudp_create(%X, %d, %d, %d\n",
	    addr->sin_addr.s_addr, pgm, vers, retrys);
#endif
#if	MACH
	p = (struct cku_private *)kalloc(sizeof *p);
	bzero((caddr_t)p, sizeof (*p));
#else	MACH
	p = (struct cku_private *)kmem_zalloc(sizeof *p);
#endif	MACH
	h = ptoh(p);

	if (!clntkudpxid) {
		clntkudpxid = time.tv_usec;
	}

	/* handle */
	h->cl_ops = &udp_ops;
	h->cl_private = (caddr_t) p;
	h->cl_auth = authkern_create();

	/* call message, just used to pre-serialize below */
	call_msg.rm_xid = 0;
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = pgm;
	call_msg.rm_call.cb_vers = vers;

	/* private */
	clntkudp_init(h, addr, retrys, cred);
#if	MACH
	p->cku_outbuf = (char *)kalloc((u_int)UDPMSGSIZE);
#else	MACH
	p->cku_outbuf = (char *)kmem_alloc((u_int)UDPMSGSIZE);
#endif	MACH
	m = mclgetx(noop, 0, p->cku_outbuf, UDPMSGSIZE, M_WAIT);
	if (m == NULL)
		goto bad;
	xdrmbuf_init(&p->cku_outxdr, m, XDR_ENCODE);

	/* pre-serialize call message header */
	if (! xdr_callhdr(&(p->cku_outxdr), &call_msg)) {
		printf("clntkudp_create - Fatal header serialization error.");
		(void) m_freem(m);
		goto bad;
	}
	p->cku_outpos = XDR_GETPOS(&(p->cku_outxdr));
	(void) m_free(m);

	/* open udp socket */
	error = socreate(AF_INET, &p->cku_sock, SOCK_DGRAM, IPPROTO_UDP);
	if (error) {
		printf("clntkudp_create: socket creation problem, %d", error);
		goto bad;
	}
	if (error = bindresvport(p->cku_sock)) {
		printf("clntkudp_create: socket bind problem, %d", error);
		goto bad;
	}
	
	current_thread()->allocInProgress = FALSE;
	
	return (h);

bad:
	current_thread()->allocInProgress = FALSE;
#if	MACH
	kfree((caddr_t)p->cku_outbuf, (u_int)UDPMSGSIZE);
	kfree((caddr_t)p, sizeof (struct cku_private));
#else	MACH
	kmem_free((caddr_t)p->cku_outbuf, (u_int)UDPMSGSIZE);
	kmem_free((caddr_t)p, sizeof (struct cku_private));
#endif	MACH
#ifdef RPCDEBUG
	rpc_debug(4, "create failed\n");
#endif
	
	return ((CLIENT *)NULL);
}

clntkudp_init(h, addr, retrys, cred)
	CLIENT *h;
	struct sockaddr_in *addr;
	int retrys;
	struct ucred *cred;
{
	struct cku_private *p = htop(h);

	p->cku_retrys = retrys;
	p->cku_addr = *addr;
	p->cku_cred = cred;
	p->cku_flags &= (CKU_BUFBUSY | CKU_BUFWANTED);
}

/*
 * Time out back off function. tim is in hz
 */
#define MAXTIMO	(60 * hz)
#define backoff(tim)	((((tim) << 1) > MAXTIMO) ? MAXTIMO : ((tim) << 1))

/*
 * Call remote procedure.
 * Most of the work of rpc is done here.  We serialize what is left
 * of the header (some was pre-serialized in the handle), serialize
 * the arguments, and send it off.  We wait for a reply or a time out.
 * Timeout causes an immediate return, other packet problems may cause
 * a retry on the receive.  When a good packet is received we deserialize
 * it, and check verification.  A bad reply code will cause one retry
 * with full (longhand) credentials.
 */
enum clnt_stat
clntkudp_callit_addr(h, procnum, xdr_args, argsp, xdr_results, resultsp, wait,
		sin)
	register CLIENT	*h;
	u_long		procnum;
	xdrproc_t	xdr_args;
	caddr_t		argsp;
	xdrproc_t	xdr_results;
	caddr_t		resultsp;
	struct timeval	wait;
	struct sockaddr_in *sin;
{
	register struct cku_private *p = htop(h);
	register XDR		   *xdrs;
	register struct socket	   *so = p->cku_sock;
	int			   rtries;
	int			   stries = p->cku_retrys;
	int			   s;
	struct ucred		   *tmpcred;
	struct mbuf		   *m;
	int timohz;
	u_long xid;
	u_int rempos = 0;
	int refreshes = 2;	/* number of times to refresh credential */
	int interrupted;        /* return from sleep() */
	int smask;              /* saved signal mask */
	struct proc *pp = u.u_procp;

#ifdef RPCDEBUG
	rpc_debug(4, "cku_callit\n");
#endif
	rcstat.rccalls++;

	while (p->cku_flags & CKU_BUSY) {
		rcstat.rcwaits++;
		p->cku_flags |= CKU_WANTED;
		(void) sleep((caddr_t)h, PZERO-2);
	}
	p->cku_flags |= CKU_BUSY;

	if ((p->cku_flags & CKU_ONCE_ONLY) != 0)
		stries = 1;

	/*
	 * Set credentials into the u structure
	 */
#if	NeXT
	u_cred_lock();
#endif	NeXT
	tmpcred = u.u_cred;
	u.u_cred = p->cku_cred;

	xid = clntkudpxid++;

	/*
	 * This is dumb but easy: keep the time out in units of hz
	 * so it is easy to call timeout and modify the value.
	 */
	timohz = wait.tv_sec * hz + (wait.tv_usec * hz) / 1000000;

call_again:

	/*
	 * Wait til buffer gets freed then make a type 2 mbuf point at it
	 * The buffree routine clears CKU_BUFBUSY and does a wakeup when
	 * the mbuf gets freed.
	 */
	s = splimp();
	while (p->cku_flags & CKU_BUFBUSY) {
		p->cku_flags |= CKU_BUFWANTED;
		/*
		 * This is a kludge to avoid deadlock in the case of a
		 * loop-back call.  The client can block wainting for
		 * the server to free the mbuf while the server is blocked
		 * waiting for the client to free the reply mbuf.  Avoid this
		 * by flushing the input queue every once in a while while
		 * we are waiting.
		 */
		timeout(wakeup, (caddr_t)&p->cku_outbuf, hz);
		(void) sleep((caddr_t)&p->cku_outbuf, PZERO-3);
		sbflush(&so->so_rcv);
	}
	p->cku_flags |= CKU_BUFBUSY;
	(void) splx(s);
	m = mclgetx(buffree, (int)p, p->cku_outbuf, UDPMSGSIZE, M_WAIT);
	if (m == NULL) {
		p->cku_err.re_status = RPC_SYSTEMERROR;
		p->cku_err.re_errno = ENOBUFS;
		buffree(p);
		goto done;
	}

	xdrs = &p->cku_outxdr;
	/*
	 * The transaction id is the first thing in the
	 * preserialized output buffer.
	 */
	(*(u_long *)(p->cku_outbuf)) = xid;

	xdrmbuf_init(xdrs, m, XDR_ENCODE);

	if (rempos != 0) {
		XDR_SETPOS(xdrs, rempos);
	} else {
		/*
		 * Serialize dynamic stuff into the output buffer.
		 */
		XDR_SETPOS(xdrs, p->cku_outpos);
		if ((! XDR_PUTLONG(xdrs, (long *)&procnum)) ||
		    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
		    (! (*xdr_args)(xdrs, argsp))) {
			p->cku_err.re_status = RPC_CANTENCODEARGS;
			p->cku_err.re_errno = EIO;
			(void) m_freem(m);
			goto done;
		}
		rempos = XDR_GETPOS(xdrs);
	}
	m->m_len = rempos;

	if ((p->cku_err.re_errno = ku_sendto_mbuf(so, m, &p->cku_addr)) != 0) {
		p->cku_err.re_status = RPC_CANTSEND;
		goto done;
	}

	for (rtries = RECVTRIES; rtries; rtries--) {
		s = splnet();
		while (so->so_rcv.sb_cc == 0) {
			/*
			 * Set timeout then wait for input, timeout
			 * or interrupt.
			 */
#ifdef RPCDEBUG
			rpc_debug(3, "callit: waiting %d\n", timohz);
#endif
			timeout(ckuwakeup, (caddr_t)p, timohz);
			so->so_rcv.sb_flags |= SB_WAIT;
#if	NeXT
			/*
			 * Juggling the sigmask is completely bogus.
			 * Also, not every routine that calls us
			 * deals well with the RPC_INTR return val.
			 * The client must be explicitly set interruptable
			 * before this is called.
			 */
			if (pp && (p->cku_flags & CKU_INTERRUPTABLE)) {
				smask = pp->p_sigmask;
				pp->p_sigmask |= ~(sigmask(SIGHUP) |
					/*sigmask(SIGINT) |*/ sigmask(SIGQUIT) |
					sigmask(SIGTERM) | sigmask(SIGCONT));
				interrupted = sleep((caddr_t)&so->so_rcv.sb_cc,
						    PZERO+1 | PCATCH);
				pp->p_sigmask = smask;
			} else {
				interrupted = 0;
				(void) sleep((caddr_t)&so->so_rcv.sb_cc,
					     PRIBIO);
			}
#else	NeXT
			smask = pp->p_sigmask;
			pp->p_sigmask |= ~(sigmask(SIGHUP) |
				sigmask(SIGINT) | sigmask(SIGQUIT) |
				sigmask(SIGTERM));

			interrupted = sleep((caddr_t)&so->so_rcv.sb_cc,
				PZERO+1 | PCATCH);
			pp->p_sigmask = smask;
#endif	NeXT
			untimeout(ckuwakeup, (caddr_t)p);
			if (interrupted) {
				(void) splx(s);
				p->cku_err.re_status = RPC_INTR;
				p->cku_err.re_errno = EINTR;
				goto done;
			}
			if (p->cku_flags & CKU_TIMEDOUT) {
				p->cku_flags &= ~CKU_TIMEDOUT;
				(void) splx(s);
				p->cku_err.re_status = RPC_TIMEDOUT;
				p->cku_err.re_errno = ETIMEDOUT;
				rcstat.rctimeouts++;
				goto done;
			}
		}

		if (so->so_error) {
			so->so_error = 0;
			(void) splx(s);
			continue;
		}

		{
			/*
			 * Declare this variable here to have smaller
			 * demand for stack space in this procedure.
			 */
			struct sockaddr_in from;

			p->cku_inmbuf = ku_recvfrom(so, &from);
			if (sin) {
				*sin = from;
			}
		}

		(void) splx(s);
		if (p->cku_inmbuf == NULL) {
			continue;
		}
		p->cku_inbuf = mtod(p->cku_inmbuf, char *);

		if (p->cku_inmbuf->m_len < sizeof (u_long)) {
			m_freem(p->cku_inmbuf);
			continue;
		}
		/*
		 * If reply transaction id matches id sent
		 * we have a good packet.
		 */
		if (*((u_long *)(p->cku_inbuf))
		    != *((u_long *)(p->cku_outbuf))) {
			rcstat.rcbadxids++;
			m_freem(p->cku_inmbuf);
			continue;
		}
		/*
		 * Flush the rest of the stuff on the input queue
		 * for the socket.
		 */
		s = splnet();
		sbflush(&so->so_rcv);
		(void) splx(s);
		break;
	}

	if (rtries == 0) {
		p->cku_err.re_status = RPC_CANTRECV;
		p->cku_err.re_errno = EIO;
		goto done;
	}

	/*
	 * Process reply
	 */

	xdrs = &(p->cku_inxdr);
	xdrmbuf_init(xdrs, p->cku_inmbuf, XDR_DECODE);

	{
		/*
		 * Declare this variable here to have smaller
		 * demand for stack space in this procedure.
		 */
		struct rpc_msg		   reply_msg;

		reply_msg.acpted_rply.ar_verf = _null_auth;
		reply_msg.acpted_rply.ar_results.where = resultsp;
		reply_msg.acpted_rply.ar_results.proc = xdr_results;

		/*
		 * Decode and validate the response.
		 */
		if (xdr_replymsg(xdrs, &reply_msg)) {
			_seterr_reply(&reply_msg, &(p->cku_err));

			if (p->cku_err.re_status == RPC_SUCCESS) {
				/*
				 * Reply is good, check auth.
				 */
				if (! AUTH_VALIDATE(h->cl_auth,
				    &reply_msg.acpted_rply.ar_verf)) {
					p->cku_err.re_status = RPC_AUTHERROR;
					p->cku_err.re_why = AUTH_INVALIDRESP;
					rcstat.rcbadverfs++;
				}
				if (reply_msg.acpted_rply.ar_verf.oa_base !=
				    NULL) {
					/* free auth handle */
					xdrs->x_op = XDR_FREE;
					(void) xdr_opaque_auth(xdrs,
					    &(reply_msg.acpted_rply.ar_verf));
				}
			} else {
				/*
				 * Maybe our credential needs refreshed
				 */
				if (refreshes > 0 && AUTH_REFRESH(h->cl_auth)) {
					refreshes--;
					rcstat.rcnewcreds++;
					rempos = 0;
				}
			}
		} else {
			p->cku_err.re_status = RPC_CANTDECODERES;
			p->cku_err.re_errno = EIO;
		}
	}

	m_freem(p->cku_inmbuf);

#ifdef RPCDEBUG
	rpc_debug(4, "cku_callit done\n");
#endif
done:
	if ((p->cku_err.re_status != RPC_SUCCESS) &&
	    (p->cku_err.re_status != RPC_INTR) &&
	    (p->cku_err.re_status != RPC_CANTENCODEARGS) &&
	    (--stries > 0)) {
		rcstat.rcretrans++;
		timohz = backoff(timohz);
		if (p->cku_err.re_status == RPC_SYSTEMERROR ||
		    p->cku_err.re_status == RPC_CANTSEND) {
			/*
			 * Errors due to lack o resources, wait a bit
			 * and try again.
			 */
			(void) sleep((caddr_t)&lbolt, PZERO-4);
		}
		goto call_again;
	}
	u.u_cred = tmpcred;
#if	NeXT
	u_cred_unlock();
#endif	NeXT
	/*
	 * Insure that buffer is not busy prior to releasing client handle.
	 */
	s = splimp();
	while (p->cku_flags & CKU_BUFBUSY) {
		p->cku_flags |= CKU_BUFWANTED;
		timeout(wakeup, (caddr_t)&p->cku_outbuf, hz);
		(void) sleep((caddr_t)&p->cku_outbuf, PZERO-3);
		sbflush(&so->so_rcv);
	}
	(void) splx(s);

	p->cku_flags &= ~CKU_BUSY;
	if (p->cku_flags & CKU_WANTED) {
		p->cku_flags &= ~CKU_WANTED;
		wakeup((caddr_t)h);
	}
	if (p->cku_err.re_status != RPC_SUCCESS) {
		rcstat.rcbadcalls++;
	}
	return (p->cku_err.re_status);
}

enum clnt_stat
clntkudp_callit(h, procnum, xdr_args, argsp, xdr_results, resultsp, wait)
	register CLIENT *h;
	u_long		procnum;
	xdrproc_t	xdr_args;
	caddr_t		argsp;
	xdrproc_t	xdr_results;
	caddr_t		resultsp;
	struct timeval	wait;
{
	return (clntkudp_callit_addr(h, procnum, xdr_args, argsp, xdr_results,
		resultsp, wait, (struct sockaddr_in *)0));
}

/*
 * Wake up client waiting for a reply.
 */
ckuwakeup(p)
	register struct cku_private *p;
{
	register struct socket	*so = p->cku_sock;

#ifdef RPCDEBUG
	rpc_debug(4, "cku_timeout\n");
#endif
	p->cku_flags |= CKU_TIMEDOUT;
	sbwakeup(&so->so_rcv);
}

/*
 * Return error info on this handle.
 */
void
clntkudp_error(h, err)
	CLIENT *h;
	struct rpc_err *err;
{
	register struct cku_private *p = htop(h);

	*err = p->cku_err;
}

/* static */ bool_t
clntkudp_freeres(cl, xdr_res, res_ptr)
	CLIENT *cl;
	xdrproc_t xdr_res;
	caddr_t res_ptr;
{
	register struct cku_private *p = (struct cku_private *)cl->cl_private;
	register XDR *xdrs = &(p->cku_outxdr);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_res)(xdrs, res_ptr));
}

void
clntkudp_abort()
{
}

bool_t
clntkudp_control()
{
	return (FALSE);
}


/*
 * Destroy rpc handle.
 * Frees the space used for output buffer, private data, and handle
 * structure, and closes the socket for this handle.
 */
void
clntkudp_destroy(h)
	CLIENT *h;
{
	register struct cku_private *p = htop(h);

#ifdef RPCDEBUG
	rpc_debug(4, "cku_destroy %x\n", h);
#endif
	(void) soclose(p->cku_sock);
#if	MACH
	kfree((caddr_t)p->cku_outbuf, (u_int)UDPMSGSIZE);
	kfree((caddr_t)p, sizeof (*p));
#else	MACH
	kmem_free((caddr_t)p->cku_outbuf, (u_int)UDPMSGSIZE);
	kmem_free((caddr_t)p, sizeof (*p));
#endif	MACH
}

/*
 * try to bind to a reserved port
 */
bindresvport(so)
	struct socket *so;
{
	struct sockaddr_in *sin;
	struct mbuf *m;
	u_short i;
	int error;
	struct ucred *tmpcred;
	struct ucred *savecred;

#	define MAX_PRIV	(IPPORT_RESERVED-1)
#	define MIN_PRIV	(IPPORT_RESERVED/2)

	m = m_get(M_WAIT, MT_SONAME);
	if (m == NULL) {
		printf("bindresvport: couldn't alloc mbuf");
		return (ENOBUFS);
	}

	sin = mtod(m, struct sockaddr_in *);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY;
	m->m_len = sizeof (struct sockaddr_in);

	/*
	 * Only root can bind to a privileged port number, so
	 * temporarily change the uid to 0 to do the bind.
	 */
	tmpcred = crdup(u.u_cred);
	savecred = u.u_cred;
	u.u_cred = tmpcred;
	u.u_uid = 0;
	error = EADDRINUSE;
	for (i = MAX_PRIV; error == EADDRINUSE && i >= MIN_PRIV; i--) {
		sin->sin_port = htons(i);
		error = sobind(so, m);
	}
	(void) m_freem(m);
	u.u_cred = savecred;
	crfree(tmpcred);
	return (error);
}

