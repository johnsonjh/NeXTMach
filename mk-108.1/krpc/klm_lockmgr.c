/* @(#)klm_lockmgr.c	2.1 88/05/24 4.0NFSSRC Copyr 1988 Sun Micro */

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * From SUN 1.16
 */

/*
 * Kernel<->Network Lock-Manager Interface
 *
 * File- and Record-locking requests are forwarded (via RPC) to a
 * Network Lock-Manager running on the local machine.  The protocol
 * for these transactions is defined in /usr/src/protocols/klm_prot.x
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/proc.h>
#import <sys/file.h>
#import <sys/stat.h>

/* files included by <rpc/rpc.h> */
#import <rpc/types.h>
#import <netinet/in.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>

#import <krpc/lockmgr.h>
#import <rpcsvc/klm_prot.h>
#import <net/if.h>
#import <nfs/nfs.h>
#import <nfs/nfs_clnt.h>
#import <nfs/rnode.h>

static struct sockaddr_in lm_sa;	/* talk to portmapper & lock-manager */

static talk_to_lockmgr();

extern int wakeup();

static int klm_debug = 0;

/* Define static parameters for run-time tuning */
static int backoff_timeout = 1;		/* time to wait on klm_denied_nolocks */
static int first_retry = 0;		/* first attempt if klm port# known */
static int first_timeout = 1;
static int normal_retry = 1;		/* attempts after new port# obtained */
static int normal_timeout = 1;
static int working_retry = 0;		/* attempts after klm_working */
static int working_timeout = 1;


/*
 * klm_lockctl - process a lock/unlock/test-lock request
 *
 * Calls (via RPC) the local lock manager to register the request.
 * Lock requests are cancelled if interrupted by signals.
 */
klm_lockctl(lh, ld, cmd, cred, clid)
	register lockhandle_t *lh;
	register struct flock *ld;
	int cmd;
	struct ucred *cred;
	int clid;
{
	register int	error;
	klm_lockargs	args;
	klm_testrply	reply;
	u_long		xdrproc;
	xdrproc_t	xdrargs;
	xdrproc_t	xdrreply;

	/* initialize sockaddr_in used to talk to local processes */
	if (lm_sa.sin_port == 0) {
#ifdef notdef
		struct ifnet	*ifp;

		if ((ifp = if_ifwithafup(AF_INET)) == (struct ifnet *)NULL) {
			panic("klm_lockctl: no inet address");
		}
		lm_sa = *(struct sockaddr_in *) &(ifp->if_addr);
#else
		lm_sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		lm_sa.sin_family = AF_INET;
#endif
	}

	args.block = FALSE;
	args.exclusive = FALSE;
	args.alock.fh.n_bytes = (char *)&lh->lh_id;
	args.alock.fh.n_len = sizeof (lh->lh_id);
	args.alock.server_name = lh->lh_servername;
	args.alock.pid = clid;
	args.alock.l_offset = ld->l_start;
	args.alock.l_len = ld->l_len;
	xdrproc = KLM_LOCK;
	xdrargs = (xdrproc_t)xdr_klm_lockargs;
	xdrreply = (xdrproc_t)xdr_klm_stat;

	/* now modify the lock argument structure for specific cases */
	switch (ld->l_type) {
	case F_WRLCK:
		args.exclusive = TRUE;
		break;
	case F_UNLCK:
		xdrproc = KLM_UNLOCK;
		xdrargs = (xdrproc_t)xdr_klm_unlockargs;
		break;
	}

	switch (cmd) {
	case F_SETLKW:
		args.block = TRUE;
		break;
	case F_GETLK:
		xdrproc = KLM_TEST;
		xdrargs = (xdrproc_t)xdr_klm_testargs;
		xdrreply = (xdrproc_t)xdr_klm_testrply;
		break;
	}

requestloop:
	/* send the request out to the local lock-manager and wait for reply */
	error = talk_to_lockmgr(xdrproc,xdrargs, &args, xdrreply, &reply, cred);
	if (error == ENOLCK) {
		goto ereturn;	/* no way the request could have gotten out */
	}

	/*
	 * The only other possible return values are:
	 *   klm_granted  |  klm_denied  | klm_denied_nolocks |  EINTR
	 */
	switch (xdrproc) {
	case KLM_LOCK:
		switch (error) {
		case klm_granted:
			error = 0;		/* got the requested lock */
			goto ereturn;
		case klm_denied:
			if (args.block) {
				printf("klm_lockmgr: blocking lock denied?!\n");
				goto requestloop;	/* loop forever */
			}
			error = EACCES;		/* EAGAIN?? */
			goto ereturn;
		case klm_denied_nolocks:
			error = ENOLCK;		/* no resources available?! */
			goto ereturn;
		case EINTR:
			if (args.block)
				goto cancel;	/* cancel blocking locks */
			else
				goto requestloop;	/* loop forever */
		}

	case KLM_UNLOCK:
		switch (error) {
		case klm_granted:
			error = 0;
			goto ereturn;
		case klm_denied:
			printf("klm_lockmgr: unlock denied?!\n");
			error = EINVAL;
			goto ereturn;
		case klm_denied_nolocks:
			goto nolocks_wait;	/* back off; loop forever */
		case EINTR:
			goto requestloop;	/* loop forever */
		}

	case KLM_TEST:
		switch (error) {
		case klm_granted:
			ld->l_type = F_UNLCK;	/* mark lock available */
			error = 0;
			goto ereturn;
		case klm_denied:
			ld->l_type = (reply.klm_testrply_u.holder.exclusive) ?
			    F_WRLCK : F_RDLCK;
			ld->l_start = reply.klm_testrply_u.holder.l_offset;
			ld->l_len = reply.klm_testrply_u.holder.l_len;
			ld->l_pid = reply.klm_testrply_u.holder.svid;
			error = 0;
			goto ereturn;
		case klm_denied_nolocks:
			goto nolocks_wait;	/* back off; loop forever */
		case EINTR:
			/* may want to take a longjmp here */
			goto requestloop;	/* loop forever */
		}
	}

/*NOTREACHED*/
nolocks_wait:
	timeout(wakeup, (caddr_t)&lm_sa, (backoff_timeout * hz));
	(void) sleep((caddr_t)&lm_sa, PZERO|PCATCH);
	untimeout(wakeup, (caddr_t)&lm_sa);
	goto requestloop;	/* now try again */

cancel:
	/*
	 * If we get here, a signal interrupted a rqst that must be cancelled.
	 * Change the procedure number to KLM_CANCEL and reissue the exact same
	 * request.  Use the results to decide what return value to give.
	 */
	xdrproc = KLM_CANCEL;
	error = talk_to_lockmgr(xdrproc,xdrargs, &args, xdrreply, &reply, cred);
	switch (error) {
	case klm_granted:
		error = 0;		/* lock granted */
		goto ereturn;
	case klm_denied:
		/* may want to take a longjmp here */
		error = EINTR;
		goto ereturn;
	case EINTR:
		goto cancel;		/* ignore signals til cancel succeeds */

	case klm_denied_nolocks:
		error = ENOLCK;		/* no resources available?! */
		goto ereturn;
	case ENOLCK:
		printf("klm_lockctl: ENOLCK on KLM_CANCEL request\n");
		goto ereturn;
	}
/*NOTREACHED*/
ereturn:
	return(error);
}


/*
 * Send the given request to the local lock-manager.
 * If timeout or error, go back to the portmapper to check the port number.
 * This routine loops forever until one of the following occurs:
 *	1) A legitimate (not 'klm_working') reply is returned (returns 'stat').
 *
 *	2) A signal occurs (returns EINTR).  In this case, at least one try
 *	   has been made to do the RPC; this protects against jamming the
 *	   CPU if a KLM_CANCEL request has yet to go out.
 *
 *	3) A drastic error occurs (e.g., the local lock-manager has never
 *	   been activated OR cannot create a client-handle) (returns ENOLCK).
 */
static
talk_to_lockmgr(xdrproc, xdrargs, args, xdrreply, reply, cred)
	u_long xdrproc;
	xdrproc_t xdrargs;
	klm_lockargs *args;
	xdrproc_t xdrreply;
	klm_testrply *reply;
	struct ucred *cred;
{
	register CLIENT *client;
	struct timeval tmo;
	register int error;

	/* set up a client handle to talk to the local lock manager */
	client = clntkudp_create(&lm_sa, (u_long)KLM_PROG, (u_long)KLM_VERS,
	    first_retry, cred);
	if (client == (CLIENT *) NULL) {
		return(ENOLCK);
	}
	tmo.tv_sec = first_timeout;
	tmo.tv_usec = 0;

	/*
	 * If cached port number, go right to CLNT_CALL().
	 * This works because timeouts go back to the portmapper to
	 * refresh the port number.
	 */
	if (lm_sa.sin_port != 0) {
		goto retryloop;		/* skip first portmapper query */
	}

	for (;;) {
remaploop:
		/* go get the port number from the portmapper...
		 * if return 1, signal was received before portmapper answered;
		 * if return -1, the lock-manager is not registered
		 * else, got a port number
		 */
		switch (getport_loop(&lm_sa,
		    (u_long)KLM_PROG, (u_long)KLM_VERS, (u_long)KLM_PROTO)) {
		case 1:
			error = EINTR;		/* signal interrupted things */
			goto out;

		case -1:
			uprintf("fcntl: Local lock-manager not registered\n");
			error = ENOLCK;
			goto out;
		}

		/*
		 * If a signal occurred, pop back out to the higher
		 * level to decide what action to take.  If we just
		 * got a port number from the portmapper, the next
		 * call into this subroutine will jump to retryloop.
		 */
		if (ISSIG(u.u_procp)) {
			error = EINTR;
			goto out;
		}

		/* reset the lock-manager client handle */
		(void) clntkudp_init(client, &lm_sa, normal_retry, cred);
		tmo.tv_sec = normal_timeout;

retryloop:
		/* retry the request until completion, timeout, or error */
		for (;;) {
			error = (int) CLNT_CALL(client, xdrproc, xdrargs,
			    (caddr_t)args, xdrreply, (caddr_t)reply, tmo);

			if (klm_debug)
				printf(
				    "klm: pid:%d cmd:%d [%d,%d]  stat:%d/%d\n",
				    args->alock.pid,
				    (int) xdrproc,
				    args->alock.l_offset,
				    args->alock.l_len,
				    error, (int) reply->stat);

			switch (error) {
			case RPC_SUCCESS:
				error = (int) reply->stat;
				if (error == (int) klm_working) {
					if (ISSIG(u.u_procp)) {
						error = EINTR;
						goto out;
					}
					/* lock-mgr is up...can wait longer */
					(void) clntkudp_init(client, &lm_sa,
					    working_retry, cred);
					tmo.tv_sec = working_timeout;
					continue;	/* retry */
				}
				goto out;	/* got a legitimate answer */

			case RPC_TIMEDOUT:
				goto remaploop;	/* ask for port# again */

			default:
				printf("lock-manager: RPC error: %s\n",
				    clnt_sperrno((enum clnt_stat) error));

				/* on RPC error, wait a bit and try again */
				timeout(wakeup, (caddr_t)&lm_sa,
				    (normal_timeout * hz));
				error = sleep((caddr_t)&lm_sa, PZERO|PCATCH);
				untimeout(wakeup, (caddr_t)&lm_sa);
				if (error) {
				    error = EINTR;
				    goto out;
				}
				goto remaploop;	/* ask for port# again */
	    
			} /*switch*/

		} /*for*/	/* loop until timeout, error, or completion */
	} /*for*/		/* loop until signal or completion */

out:
	AUTH_DESTROY(client->cl_auth);	/* drop the authenticator */
	CLNT_DESTROY(client);		/* drop the client handle */
	return(error);
}
