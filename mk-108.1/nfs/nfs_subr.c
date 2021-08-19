/*
 * HISTORY
 *  5-Nov-90  Bradley Taylor (btaylor) at NeXT
 *	Make MAXCLIENTS a soft limit (3) and increase to 6 for netbooted
 *	clients. Also in netboot case, preallocate client handles to avoid
 *	allocation on pageout and make UDP buffers contiguous to avoid 
 *	fragmentation.
 *
 *  8-Mar-90  Morris Meyer (mmeyer) at NeXT
 *	Change to correctly set the rpcerr status if a RPC_INTR occurs.
 *	Cast botch in vattr_to_sattr().
 *	Better hashing function for rtablehash().
 *	Fix in rinval() that was causing VN_RELE's to be lost.
 *
 *  9-May-89  Peter King (king) at NeXT
 *	Sun Bugfixes: km06    -	Fix variable type error.
 *		      1012533 -	Use the new RPC_INTR return from
 *				clnt_kudp_callit() instead of calling
 *				interrupted() to detect an interrupted
 *				NFS request.
 *		      1012926 - Fix logic in rfscall: if an operation on a
 *				soft-mounted filesystem is
 *				interrupted, return status is
 *				uninitialized.
 *		      1017983 - Made the counter "newnum" unsigned.
 *		      1018301 - Fix randomization on newname().
 *
 * 20-Dec-88  Peter King (king) at NeXT
 *	NFS 4.0 Changes: Reworked rnode cache.
 *
 * 28-Oct-87  Peter King (king) at NeXT, Inc.
 *	Original Sun source, ported to Mach.
 */ 

/*	 @(#)nfs_subr.c	1.7 88/08/09 NFSSRC4.0 from 2.84 88/02/08 SMI	*/

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#import <secure_nfs.h>
#import <nfsmeas.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <kern/mfs.h>
#import <sys/kernel.h>
#import <sys/buf.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/proc.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/stat.h>
#import <sys/uio.h>
#import <sys/trace.h>
#import <kern/thread.h>
#import <vm/vm_pager.h>
#import <net/if.h>
#import <netinet/in.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>

#ifdef	TRACE
#ifndef	NFSSERVER
#define	NFSSERVER
#import <nfs/nfs.h>
#undef	NFSSERVER
#else	NFSSERVER
#import <nfs/nfs.h>
#endif	NFSSERVER
#else	TRACE
#import <nfs/nfs.h>
#endif	TRACE

#import <nfs/nfs_clnt.h>
#import <nfs/rnode.h>
#import <nfs/nfs_meas.h>

#import <machine/spl.h>

#ifdef NFSDEBUG
extern int nfsdebug;
#endif

extern struct vnodeops nfs_vnodeops;
static struct rnode *rfind();
static int rp_addhash();

extern int nrnode;		/* max rnodes to alloc, set in machdep.c */

/*
 * Client side utilities
 */

/*
 * client side statistics
 */
struct {
	int	nclsleeps;		/* client handle waits */
	int	nclgets;		/* client handle gets */
	int	ncalls;			/* client requests */
	int	nbadcalls;		/* rpc failures */
	int	reqs[32];		/* count of each request */
} clstat;

int cltoomany = 0;

#if	NeXT
/*
 * The absolute maximum number of clients
 */
#define MAXCLIENTS_SLOTS	6

/*
 * Padding used to reduce absolute maximum down to a soft limit. 
 * The soft limit will be raised in the case of netbooted clients
 * which need a lot of prealloced client handles to avoid pageout
 * allocation problems.
 */
#define MAXCLIENTS_PADDING	3

/*
 * Use a variable instead of #define so that we
 * can increase MAXCLIENTS (up to a certain point) if necessary.
 */
unsigned MAXCLIENTS =		MAXCLIENTS_SLOTS - MAXCLIENTS_PADDING;

/*
 * Temporary define for purposes of allocation by compiler.
 * Will be #undefed a bit later. 
 * National Code Readability Project - We do our part!
 */
#define MAXCLIENTS	MAXCLIENTS_SLOTS

#else	NeXT
#define MAXCLIENTS	6
#endif	NeXT
struct chtab {
	int	ch_timesused;
	bool_t	ch_inuse;
	CLIENT	*ch_client;
} chtable[MAXCLIENTS];

#if	SECURE_NFS
u_int authdes_win = (60*60);	/* one hour -- should be mount option */

struct desauthent {
	struct mntinfo *da_mi;
	uid_t da_uid;
	short da_inuse;
	AUTH *da_auth;
} desauthtab[MAXCLIENTS];
int nextdesvictim;
#endif	SECURE_NFS

struct unixauthent {
	short ua_inuse;
	AUTH *ua_auth;
} unixauthtab[MAXCLIENTS];
int nextunixvictim;

#if	NeXT
/* 
 * Let MAXCLIENTS variable take effect now
 */
#undef MAXCLIENTS
#endif	NeXT

AUTH *
authget(mi, cr)
	struct mntinfo *mi;
	struct ucred *cr;
{
	int i;
	AUTH *auth;
	register struct unixauthent *ua;
#if	SECURE_NFS
	register struct desauthent *da;
#endif	SECURE_NFS
	int authflavor;
	struct ucred *savecred;

	authflavor = mi->mi_authflavor;
	for (;;) switch (authflavor) {
	case AUTH_NONE:
		/*
		 * XXX: should do real AUTH_NONE, instead of AUTH_UNIX
		 */
	case AUTH_UNIX:
		i = MAXCLIENTS;
		do {
			ua = &unixauthtab[nextunixvictim++];
			nextunixvictim %= MAXCLIENTS;
		} while (ua->ua_inuse && --i > 0);

		if (ua->ua_inuse) {
			/* overflow of unix auths */
			return authkern_create();
		}

		if (ua->ua_auth == NULL) {
			ua->ua_auth = authkern_create();
		}
		ua->ua_inuse = 1;
		return (ua->ua_auth);

#if	SECURE_NFS
	case AUTH_DES:
		for (da = desauthtab; da < &desauthtab[MAXCLIENTS]; da++) {
			if (da->da_mi == mi && da->da_uid == cr->cr_uid &&
			    !da->da_inuse && da->da_auth != NULL) {
				da->da_inuse = 1;
				return (da->da_auth);
			}
		}

		savecred = u.u_cred;
		u.u_cred = cr;
		auth = authdes_create(mi->mi_netname, authdes_win,
			(struct sockaddr *)&mi->mi_addr, (des_block *)NULL);
		u.u_cred = savecred;

		if (auth == NULL) {
			printf("authget: authdes_create failure\n");
			authflavor = AUTH_UNIX;
			continue;
		}

		i = MAXCLIENTS;
		do {
			da = &desauthtab[nextdesvictim++];
			nextdesvictim %= MAXCLIENTS;
		} while (da->da_inuse && --i > 0);

		if (da->da_inuse) {
			/* overflow of des auths */
			return (auth);
		}

		if (da->da_auth != NULL) {
			auth_destroy(da->da_auth);	/* should reuse!!! */
		}

		da->da_auth = auth;
		da->da_inuse = 1;
		da->da_uid = cr->cr_uid;
		da->da_mi = mi;
		return (da->da_auth);
#endif	SECURE_NFS

	default:
		/*
		 * auth create must have failed, try AUTH_NONE
		 * (this relies on AUTH_NONE never failing)
		 */
		printf("authget: unknown authflavor %d\n", authflavor);
		authflavor = AUTH_NONE;
	}
}

authfree(auth)
	AUTH *auth;
{
	register struct unixauthent *ua;
#if	SECURE_NFS
	register struct desauthent *da;
#endif	SECURE_NFS

	switch (auth->ah_cred.oa_flavor) {
	case AUTH_NONE: /* XXX: do real AUTH_NONE */
	case AUTH_UNIX:
		for (ua = unixauthtab; ua < &unixauthtab[MAXCLIENTS]; ua++) {
			if (ua->ua_auth == auth) {
				ua->ua_inuse = 0;
				return;
			}
		}
		auth_destroy(auth);	/* was overflow */
		break;
#if	SECURE_NFS
	case AUTH_DES:
		for (da = desauthtab; da < &desauthtab[MAXCLIENTS]; da++) {
			if (da->da_auth == auth) {
				da->da_inuse = 0;
				return;
			}
		}
		auth_destroy(auth);	/* was overflow */
		break;
#endif	SECURE_NFS
	default:
		printf("authfree: unknown authflavor %d\n", auth->ah_cred.oa_flavor);
		break;
	}
}


CLIENT *
clget(mi, cred)
	struct mntinfo *mi;
	struct ucred *cred;
{
	register struct chtab *ch;
	int retrans;
	CLIENT *client;

	/*
	 * If soft mount and server is down just try once
	 */
	if (!mi->mi_hard && mi->mi_down) {
		retrans = 1;
	} else {
		retrans = mi->mi_retrans;
	}

	/*
	 * Find an unused handle or create one
	 */
	clstat.nclgets++;
	for (ch = chtable; ch < &chtable[MAXCLIENTS]; ch++) {
		if (!ch->ch_inuse) {
			ch->ch_inuse = TRUE;
			if (ch->ch_client == NULL) {
				ch->ch_client =
				    clntkudp_create(&mi->mi_addr,
				    NFS_PROGRAM, NFS_VERSION,
				    retrans, cred);
				if (ch->ch_client == NULL) {
					panic("clget: null client");
				}
				auth_destroy(ch->ch_client->cl_auth); /* XXX */
			} else {
				clntkudp_init(ch->ch_client,
				    &mi->mi_addr, retrans, cred);
			}
			ch->ch_client->cl_auth = authget(mi, cred);
			if (ch->ch_client->cl_auth == NULL) {
				panic("clget: null auth");
			}
			ch->ch_timesused++;
#if	NeXT
			/*
			 * If interruptable mount, make the client
			 * interruptable.
			 */
			if (mi->mi_hard && mi->mi_int) {
				clntkudp_interruptable(ch->ch_client, TRUE);
			}
#endif	NeXT
			return (ch->ch_client);
		}
	}

	/*
	 * If we got here there are no available handles
	 * To avoid deadlock, don't wait, but just grab another
	 */
	cltoomany++;
	client = clntkudp_create(&mi->mi_addr, NFS_PROGRAM, NFS_VERSION,
				retrans, cred);
	if (client == NULL)
		panic("clget: null client");
	auth_destroy(client->cl_auth);	 /* XXX */
	client->cl_auth = authget(mi, cred);
	if (client->cl_auth == NULL)
		panic("clget: null auth");
#if	NeXT
	/*
	 * If interruptable mount, make the client
	 * interruptable.
	 */
	if (mi->mi_hard && mi->mi_int) {
		clntkudp_interruptable(client, TRUE);
	}
#endif	NeXT
	return (client);
}

#ifdef	NeXT
/*
 * Netbooting has special needs. On pageout, we must avoid allocating
 * memory, so here we prealloc the client handles to avoid pageout
 * allocation problems. We also increase the number of client handles
 * to decrease the likelihood that we will run out, causing us to allocate
 * more. Finally, we allocate our own contiguous UDPMSGSIZE buffers, to 
 * avoid kalloc fragmentation.
 */
void
nfs_netboot_prealloc(
		     struct mntinfo *mi
		     )
{
	CLIENT *clients[MAXCLIENTS_SLOTS];
	int i;
	char *mem;

	/*
	 * Raise client handle limit to maximum
	 */
	MAXCLIENTS += MAXCLIENTS_PADDING;

	/*
	 * Insure all the slotted client handles get allocated 
	 */
	for (i = 0; i < MAXCLIENTS; i++) {
		clients[i] = clget(mi, u.u_cred);
	}
	
	/*
	 * This doesn't really free the handles, just marks them
	 * ready for reuse
	 */
	for (i = 0; i < MAXCLIENTS; i++) {
		if (clients[i] != NULL) {
			clfree(clients[i]);
		}
	}

	/*
	 * Now, allocate contiguous memory and realloc each client handle
	 * with it to avoid fragmentation.
	 */
	mem = kalloc(MAXCLIENTS * UDPMSGSIZE);
	for (i = 0; i < MAXCLIENTS; i++) {
		clntkudp_realloc(chtable[i].ch_client, mem);
		mem += UDPMSGSIZE;
	}
}
#endif

clfree(cl)
	CLIENT *cl;
{
	register struct chtab *ch;

	authfree(cl->cl_auth);
	cl->cl_auth = NULL;
	for (ch = chtable; ch < &chtable[MAXCLIENTS]; ch++) {
		if (ch->ch_client == cl) {
			ch->ch_inuse = FALSE;
			return;
		}
	}
	/* destroy any extra allocated above MAXCLIENTS */
	CLNT_DESTROY(cl);
}

char *rfsnames[] = {
	"null", "getattr", "setattr", "unused", "lookup", "readlink", "read",
	"unused", "write", "create", "remove", "rename", "link", "symlink",
	"mkdir", "rmdir", "readdir", "fsstat" };
static short timeo_shift[] = {
	0, 0, 1, 0, 0, 1, 1,
	0, 2, 2, 2, 2, 2, 2,
	2, 2, 1, 0 };

/*
 * Back off for retransmission timeout, MAXTIMO is in 10ths of a sec
 */
#define MAXTIMO	300
#define backoff(tim)	((((tim) << 2) > MAXTIMO) ? MAXTIMO : ((tim) << 2))

int
rfscall(mi, which, xdrargs, argsp, xdrres, resp, cred)
	register struct mntinfo *mi;
	int	 which;
	xdrproc_t xdrargs;
	caddr_t	argsp;
	xdrproc_t xdrres;
	caddr_t	resp;
	struct ucred *cred;
{
	CLIENT *client;
	register enum clnt_stat status;
	struct rpc_err rpcerr;
	struct timeval wait;
	struct ucred *newcred;
	int timeo;
	int user_told;
	bool_t tryagain;
#if	NNFSMEAS
	extern int clntkudpxid;
	int xid;
#endif	NNFSMEAS

#ifdef NFSDEBUG
	dprint(nfsdebug, 6, "rfscall: %x, %d, %x, %x, %x, %x\n",
	    mi, which, xdrargs, argsp, xdrres, resp);
#endif
	clstat.ncalls++;
	clstat.reqs[which]++;

	rpcerr.re_errno = 0;
	rpcerr.re_status = RPC_SUCCESS;
	newcred = NULL;
	timeo = mi->mi_timeo << timeo_shift[which];
	user_told = 0;
retry:
	client = clget(mi, cred);
	if (which == RFS_CREATE) {
		clntkudp_once (client, 1);
	}

#if	NNFSMEAS
	xid = clntkudpxid;
	nfsmeas_begin (which, xid, NFSMEAS_CLIENT);
#endif	NNFSMEAS
	/*
	 * If hard mounted fs, retry call forever unless hard error occurs
	 */
	do {
		tryagain = FALSE;

		wait.tv_sec = timeo / 10;
		wait.tv_usec = 100000 * (timeo % 10);
		status = CLNT_CALL(client, which, xdrargs, argsp,
		    xdrres, resp, wait);
		switch (status) {
		case RPC_SUCCESS:
			break;

		/*
		 * Unrecoverable errors: give up immediately
		 */
		case RPC_AUTHERROR:
		case RPC_CANTENCODEARGS:
		case RPC_CANTDECODERES:
		case RPC_VERSMISMATCH:
		case RPC_PROGVERSMISMATCH:
		case RPC_CANTDECODEARGS:
			break;

		default:
			if (status == RPC_INTR) {
				tryagain = (bool_t)(mi->mi_hard && !mi->mi_int);
				if (tryagain) 
					continue;
				rpcerr.re_status = RPC_INTR;
				rpcerr.re_errno = EINTR;
			} else 
				tryagain = (bool_t)mi->mi_hard; 

			if (tryagain) {
				timeo = backoff(timeo);
				if (!mi->mi_printed) {
					mi->mi_printed = 1;
	printf("NFS server %s not responding still trying\n", mi->mi_hostname);
				}
				if (!user_told && u.u_ttyp) {
					user_told = 1;
	uprintf("NFS server %s not responding still trying\n", mi->mi_hostname);
				}
			}
		}
	} while (tryagain);
	clntkudp_once(client, 0);

	if (status != RPC_SUCCESS) {
		clstat.nbadcalls++;
		mi->mi_down = 1;
		if (status != RPC_INTR) {
#if	NeXT
			rpcerr.re_status = status;
			rpcerr.re_errno = EINVAL;
#endif	NeXT
			CLNT_GETERR(client, &rpcerr);
			printf("NFS %s failed for server %s: %s\n",
				rfsnames[which], mi->mi_hostname,
				clnt_sperrno(status));
			if (u.u_ttyp) {
				uprintf("NFS %s failed for server %s: %s\n",
					rfsnames[which], mi->mi_hostname,
					clnt_sperrno(status));
			}
		}
	} else if (resp && *(int *)resp == EACCES &&
	    newcred == NULL && cred->cr_uid == 0 && cred->cr_ruid != 0) {
		/*
		 * Boy is this a kludge!  If the reply status is EACCES
		 * it may be because we are root (no root net access).
		 * Check the real uid, if it isn't root make that
		 * the uid instead and retry the call.
		 */
		newcred = crdup(cred);
		cred = newcred;
		cred->cr_uid = cred->cr_ruid;
		clfree(client);
		goto retry;
	} else if (mi->mi_hard) {
		if (mi->mi_printed) {
			printf("NFS server %s ok\n", mi->mi_hostname);
			mi->mi_printed = 0;
		}
		if (user_told) {
			uprintf("NFS server %s ok\n", mi->mi_hostname);
		}
	} else {
		mi->mi_down = 0;
	}

	clfree(client);
#ifdef NFSDEBUG
	dprint(nfsdebug, 7, "rfscall: returning %d\n", rpcerr.re_errno);
#endif
	if (newcred) {
		crfree(newcred);
	}
	/*
	 *      This should never happen, but we suspect something is
	 *      garbaging packets, so ...
	 */
	if (rpcerr.re_status != RPC_SUCCESS && rpcerr.re_errno == 0) {
		printf("rfscall:  re_status %d, re_errno 0\n", rpcerr.re_status);
		panic("rfscall");
		/* NOTREACHED */
	}
#if	NNFSMEAS
	nfsmeas_end (which, xid, NFSMEAS_CLIENT);
#endif	NNFSMEAS
	return (rpcerr.re_errno);
}

vattr_to_sattr(vap, sa)
	register struct vattr *vap;
	register struct nfssattr *sa;
{

	sa->sa_mode = vap->va_mode;
#if	NeXT
	sa->sa_uid = (uid_t)vap->va_uid;
	sa->sa_gid = (gid_t)vap->va_gid;
#else
	sa->sa_uid = (int)vap->va_uid;
	sa->sa_gid = (int)vap->va_gid;
#endif	NeXT
	sa->sa_size = vap->va_size;
	sa->sa_atime.tv_sec  = vap->va_atime.tv_sec;
	sa->sa_atime.tv_usec = vap->va_atime.tv_usec;
	sa->sa_mtime.tv_sec  = vap->va_mtime.tv_sec;
	sa->sa_mtime.tv_usec = vap->va_mtime.tv_usec;
}

setdiropargs(da, nm, dvp)
	struct nfsdiropargs *da;
	char *nm;
	struct vnode *dvp;
{

	bcopy((caddr_t)vtofh(dvp),(caddr_t)&da->da_fhandle,sizeof(fhandle_t));
	da->da_name = nm;
}

int
setdirgid(dvp)
	struct vnode *dvp;
{

	/*
	 * To determine the expected group-id of the created file:
	 *  1)	If the filesystem was not mounted with the Old-BSD-compatible
	 *	GRPID option, and the directory's set-gid bit is clear,
	 *	then use the process's gid.
	 *  2)	Otherwise, set the group-id to the gid of the parent directory.
	 */
	if (!(dvp->v_vfsp->vfs_flag & VFS_GRPID) &&
	    !(vtor(dvp)->r_attr.va_mode & VSGID))
		return ((int)u.u_gid);
	else
		return ((int)vtor(dvp)->r_attr.va_gid);
}

u_int
setdirmode(dvp, om)
	struct vnode *dvp;
	u_int om;
{

	/*
	 * Modify the expected mode (om) so that the set-gid bit matches
	 * that of the parent directory (dvp).
	 */
	om &= ~VSGID;
	if (vtor(dvp)->r_attr.va_mode & VSGID)
		om |= VSGID;
	return (om);
}

struct rnode *rpfreelist = NULL;
int rreuse, rnew, ractive, rreactive, rnfree, rnhash, rnpages;

#if	MACH
#import <kern/zalloc.h>
zone_t	rnode_zone = ZONE_NULL;
#endif	MACH

/*
 * Return a vnode for the given fhandle.
 * If no rnode exists for this fhandle create one and put it
 * in a table hashed by fh_fsid and fs_fid.  If the rnode for
 * this fhandle is already in the table return it (ref count is
 * incremented by rfind.  The rnode will be flushed from the
 * table when nfs_inactive calls runsave.
 */
struct vnode *
makenfsnode(fh, attr, vfsp)
	fhandle_t *fh;
	struct nfsfattr *attr;
	struct vfs *vfsp;
{
	register struct vnode *vp;
	register struct rnode *rp;
	struct vm_info *vitmp;
	char newnode = 0;

	if ((rp = rfind(fh, vfsp)) == NULL) {
		if (rpfreelist && rnew >= nrnode) {
			rp = rpfreelist;
			rpfreelist = rpfreelist->r_freef;
			rm_free(rp);
			rp_rmhash(rp);
			rinactive(rp);
			rreuse++;
		} else {
#if	MACH
			if (rnode_zone == ZONE_NULL)
				rnode_zone = zinit(sizeof(*rp),
					10000*sizeof(*rp),
					0, FALSE, "rnode structures");
			rp = (struct rnode *)zalloc(rnode_zone);
			rtov(rp)->vm_info = VM_INFO_NULL;
			vm_info_init(rtov(rp));
#else	MACH
			rp = (struct rnode *)kmem_alloc(sizeof(*rp));
#endif	MACH
			rnew++;
		}
#if	MACH
		vp = rtov(rp);
		vitmp = vp->vm_info;
		bzero((caddr_t)rp, sizeof (*rp));
		vp->vm_info = vitmp;
#if	MACH_NBC
		/*
		 *	Flush the vnode from the file map cache.
		 */
		mfs_uncache(vp);
#endif	MACH_NBC
		vp->vm_info->pager = vm_pager_null;
		vp->vm_info->vnode_size = rp->r_size;
#else	MACH
		bzero((caddr_t)rp, sizeof (*rp));
#endif	MACH
		bcopy((caddr_t)fh,(caddr_t)&rp->r_fh,sizeof(*fh));
		vp->v_count = 1;
		vp->v_op = &nfs_vnodeops;
		if (attr) {
			vp->v_type = n2v_type(attr);
			vp->v_rdev = n2v_rdev(attr);
		}
		vp->v_data = (caddr_t)rp;
		vp->v_vfsp = vfsp;
		rp_addhash(rp);
		((struct mntinfo *)(vfsp->vfs_data))->mi_refct++;
		newnode++;
	}
	vp = rtov(rp);
	if (attr) {
		if (!newnode) {
			nfs_cache_check(vp, attr->na_mtime);
		}
		nfs_attrcache(vp, attr);
	}
	return (vp);
}

/*
 * Rnode lookup stuff.
 * These routines maintain a table of rnodes hashed by fhandle so
 * that the rnode for an fhandle can be found if it already exists.
 * NOTE: RTABLESIZE must be a power of 2 for rtablehash to work!
 */

#define	BACK	0
#define	FRONT	1

#define	RTABLESIZE	64

#if	NeXT
/*
 * File handle internal representation
 *
 * file system id (fsid)	=> Bytes  0-7
 * Length of data (Bytes 10-19) => Bytes  8-9
 * inode number		=> Bytes 10-13
 * generation number		=> Bytes 14-17
 * Unused        		=> Bytes 18-19
 *
 * Length of data (Bytes 22-31)	=> Bytes 20-21
 * exported fs inode number	=> Bytes 22-25
 * exported fs generation number=> Bytes 26-29
 * Unused                       => Bytes 30-31
 */
#define rtablehash(fh) \
    ((fh->fh_data[0] ^ fh->fh_data[1] ^ fh->fh_data[2] ^ fh->fh_data[3] ^ \
      fh->fh_data[4] ^ fh->fh_data[5] ^ fh->fh_data[6] ^ fh->fh_data[7] ^ \
      fh->fh_data[10] ^ fh->fh_data[11] ^ fh->fh_data[12] ^ fh->fh_data[13] ^ \
      fh->fh_data[14] ^ fh->fh_data[15] ^ fh->fh_data[16] ^ fh->fh_data[17]) \
      & (RTABLESIZE-1))
#else
#define	rtablehash(fh) \
    ((fh->fh_data[2] ^ fh->fh_data[5] ^ fh->fh_data[15]) & (RTABLESIZE-1))
#endif	NeXT

struct rnode *rtable[RTABLESIZE];

/*
 * Put a rnode in the hash table
 */
static
rp_addhash(rp)
	struct rnode *rp;
{

	rp->r_hash = rtable[rtablehash(rtofh(rp))];
	rtable[rtablehash(rtofh(rp))] = rp;
	rnhash++;
}

/*
 * Remove a rnode from the hash table
 */
rp_rmhash(rp)
	struct rnode *rp;
{
	register struct rnode *rt;
	register struct rnode *rtprev = NULL;

	rt = rtable[rtablehash(rtofh(rp))];
	while (rt != NULL) {
		if (rt == rp) {
			if (rtprev == NULL) {
				rtable[rtablehash(rtofh(rp))] = rt->r_hash;
			} else {
				rtprev->r_hash = rt->r_hash;
			}
			rnhash--;
			return;
		}
		rtprev = rt;
		rt = rt->r_hash;
	}
}

/*
 * Add an rnode to the front of the free list
 */
static
add_free(rp, front)
	register struct rnode *rp;
	int front;
{
	if (rp->r_freef != NULL) {
		return;
	}
	if (rpfreelist == NULL) {
		rp->r_freef = rp;
		rp->r_freeb = rp;
		rpfreelist = rp;
	} else {
		rp->r_freef = rpfreelist;
		rp->r_freeb = rpfreelist->r_freeb;
		rpfreelist->r_freeb->r_freef = rp;
		rpfreelist->r_freeb = rp;
		if (front) {
			rpfreelist = rp;
		}
	}
	rnfree++;
}

/*
 * Remove an rnode from the free list
 */
rm_free(rp)
	register struct rnode *rp;
{
	if (rp->r_freef == NULL) {
		return;
	}
	if (rp->r_freef == rp) {
		rpfreelist = NULL;
	} else {
		if (rp == rpfreelist) {
			rpfreelist = rp->r_freef;
		}
		rp->r_freeb->r_freef = rp->r_freef;
		rp->r_freef->r_freeb = rp->r_freeb;
	}
	rp->r_freef = rp->r_freeb = NULL;
	rnfree--;
}

/*
 * free resource for rnode
 */
rinactive(rp)
	struct rnode *rp;
{
	if (rp->r_cred) {
		crfree(rp->r_cred);
		rp->r_cred = NULL;
	}
}

/*
 * Put an rnode on the free list.
 * The rnode has already been removed from the hash table.
 * If there are no pages on the vnode remove inactivate it,
 * otherwise put it back in the hash table so it can be reused
 * and the vnode pages don't go away.
 */
rfree(rp)
	register struct rnode *rp;
{
	((struct mntinfo *)rtov(rp)->v_vfsp->vfs_data)->mi_refct--;
	rinactive(rp);
	add_free(rp, FRONT);
}

/*
 * Lookup a rnode by fhandle.
 */
static struct rnode *
rfind(fh, vfsp)
	fhandle_t *fh;
	struct vfs *vfsp;
{
	register struct vnode *vp;
	register struct rnode *rt;

	rt = rtable[rtablehash(fh)];
	while (rt != NULL) {
		vp = rtov(rt);
		if (bcmp((caddr_t)rtofh(rt), (caddr_t)fh, sizeof (*fh)) == 0 &&
		    vfsp == vp->v_vfsp) {
			VN_HOLD(vp);
			if (vp->v_count == 1) {
				/*
				 * reactivating a free rnode, up vfs ref count
				 * and remove rnode from free list.
				 */
				rm_free(rt);
				((struct mntinfo *)
				 (vp->v_vfsp->vfs_data))->mi_refct++;
				rreactive++;
			} else {
				ractive++;
			}
			rm_free(rt);
			return (rt);
		}
		rt = rt->r_hash;
	}
	return (NULL);
}


/*
 * Invalidate all vnodes for this vfs.
 *
 * NOTE: We assume on entry the vnodes have already been written via 
 * rflush().  We are called by nfs_unmount() to release the name cache
 * entries associated with this vnode and to purge the buffer cache of
 * pages associated with the vnode. 
 *
 * NOTE2: The goto within the inner loop is REQUIRED because the rnode 
 * address may eside anywhere on the hash chain and the loop invariant 
 * is violated when a rnode release occurs.
 */
/*ARGSUSED*/
rinval(vfsp)
	struct vfs *vfsp;
{
	register struct rnode **rpp, *rp;
	struct vnode *vp;

	for (rpp = rtable; rpp < &rtable[RTABLESIZE]; rpp++) {
#if	NeXT
restart:
#endif	NeXT
		for (rp = *rpp;rp != (struct rnode *)NULL;rp = rp->r_hash) {
			vp = rtov(rp);
#if	NeXT
			if (vp->v_vfsp == vfsp) {
				rp_rmhash(rp);
				VN_HOLD(vp);
				binvalfree(vp);
				dnlc_purge_vp(vp);

				if (vp->v_count > 1) {
					/*
					 * optimize no rnode release case 
					 */
					VN_RELE(vp);
				}
				else {
					/*
					 * loop invariant is invalidated 
					 * so restart
					 */
					VN_RELE(vp);
					goto restart;
				}
			}
#else
			if (rtov(rp)->v_vfsp == vfsp) {
				rp_rmhash(rp);
				VN_HOLD(rtov(rp));
				binvalfree(rtov(rp));
				dnlc_purge_vp(rtov(rp));
				VN_RELE(rtov(rp));
			}
#endif	NeXT
		}
	}
}

/*
 * Flush all vnodes in this (or every) vfs.
 * Used by nfs_sync and by nfs_unmount.
 */
rflush(vfsp)
	struct vfs *vfsp;
{
	register struct rnode **rpp, *rp;
	register struct vnode *vp;

	for (rpp = rtable; rpp < &rtable[RTABLESIZE]; rpp++) {
		for (rp = *rpp; rp != (struct rnode *)NULL; rp = rp->r_hash) {
			vp = rtov(rp);
			/*
			 * Don't bother sync'ing a vp if it
			 * is part of virtual swap device or
			 * if VFS is read-only
			 */
			if ((vp->v_flag & VISSWAP) ||
			    (vp->v_vfsp->vfs_flag & VFS_RDONLY) != 0)
				continue;
			if (vfsp == (struct vfs *)NULL || vp->v_vfsp == vfsp) {
				sync_vp(vp); 
			}
		}
	}
}

#define	PREFIXLEN	4
static char prefix[PREFIXLEN + 1] = ".nfs";

char *
newname()
{
	char *news;
	register char *s1, *s2;
	int id;
	static unsigned int newnum;

#if	MACH
	news = (char *)kalloc((u_int)NFS_MAXNAMLEN);
#else	MACH
	news = (char *)kmem_alloc((u_int)NFS_MAXNAMLEN);
#endif	MACH
	for (s1 = news, s2 = prefix; s2 < &prefix[PREFIXLEN]; ) {
		*s1++ = *s2++;
	}
	if (newnum == 0) {
		newnum = time.tv_sec & 0xffff;
	}
	id = newnum++;
	while (id) {
		*s1++ = "0123456789ABCDEF"[id & 0x0f];
		id = id >> 4;
	}
	*s1 = '\0';
	return (news);
}

rlock(rp)
	register struct rnode *rp;
{

	RLOCK(rp);
}

runlock(rp)
	register struct rnode *rp;
{

	RUNLOCK(rp);
}


