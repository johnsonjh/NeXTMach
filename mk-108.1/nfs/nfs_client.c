/*	@(#)nfs_client.c	2.4 88/08/04 NFSSRC4.0 from 1.5 88/02/08 SMI 	*/

/*
 * HISTORY
 *   4-Dec-89  Morris Meyer (mmeyer) at NeXT
 *	Added compatibility fix to keep the major device returned in
 *	the vnode attributes (which turns into stat buffer for stat())
 *	as 0xff.
 *
 *   9-May-89  Peter King (king) at NeXT
 *	Sun Bugfixes: 1011512 - Don't cache attributes if VNOCACHE set in
 *				vnode.
 *
 *  19-Dec-88  Peter King (king) at NeXT
 *	Original Sun NFS 4.0 source.
 *	Ported to Mach.  Flush the vnode_cache in nfs_purge_caches.
 */

#if	MACH
#import <mach_nbc.h>
#endif	MACH

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/time.h>
#import <sys/kernel.h>
#import <sys/vnode.h>
#import <sys/vfs.h>
#import <sys/vfs_stat.h>
#import <sys/errno.h>
#import <sys/buf.h>
#import <sys/stat.h>
#import <sys/ucred.h>
#import <kern/mfs.h>
#import <kern/thread.h>
#import <rpc/types.h>
#import <netinet/in.h>		/* XXX this is for sockaddr_in! */
#import <nfs/nfs.h>
#import <nfs/nfs_clnt.h>
#import <nfs/rnode.h>

#ifdef NFSDEBUG
extern int nfsdebug;
#endif

/*
 * Attributes caching:
 *
 * Attributes are cached in the rnode in struct vattr form.
 * There is a time associated with the cached attributes (r_attrtime)
 * which tells whether the attributes are valid. The time is initialized
 * to the difference between current time and the modify time of the vnode
 * when new attributes are cached. This allows the attributes for
 * files that have changed recently to be timed out sooner than for files
 * that have not changed for a long time. There are minimum and maximum
 * timeout values that can be set per mount point.
 */

/*
 * Validate caches by checking cached attributes. If they have timed out
 * get the attributes from the server and compare mtimes. If mtimes are
 * different purge all caches for this vnode.
 */
nfs_validate_caches(vp, cred)
	struct vnode *vp;
	struct ucred *cred;
{
	struct vattr va;

	return (nfsgetattr(vp, &va, cred));
}

nfs_invalidate_caches(vp)
	struct vnode *vp;
{
	struct rnode *rp;

	rp = vtor(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "nfs_invalidate_caches: rnode %x\n", rp);
#endif
	/* FIXME: Is this correct? */
	vnode_uncache(vp);
	mfs_invalidate(vp);
	PURGE_ATTRCACHE(vp);
	dnlc_purge_vp(vp);
	binvalfree(vp);
}

nfs_purge_caches(vp)
	struct vnode *vp;
{
	struct rnode *rp;

	rp = vtor(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "nfs_purge_caches: rnode %x\n", rp);
#endif
#if	MACH
	/* FIXME: Is this correct? */
	vnode_uncache(vp);
#endif	MACH
#if	NeXT
	sync_vp(vp);
#else	NeXT
	rlock(rp);
	sync_vp(vp);
	runlock(rp);
#endif	NeXT
	PURGE_ATTRCACHE(vp);
	dnlc_purge_vp(vp);
	binvalfree(vp);
}

int
nfs_cache_check(vp, mtime)
	struct vnode *vp;
	struct timeval mtime;
{
	if (!CACHE_VALID(vtor(vp), mtime)) {
		nfs_purge_caches(vp);
	}
}

/*
 * Set attributes cache for given vnode using nfsattr.
 */
nfs_attrcache(vp, na)
	struct vnode *vp;
	struct nfsfattr *na;
{

	if ((vp->v_flag & VNOCACHE) || vtomi(vp)->mi_noac) {
		return;
	}
	nattr_to_vattr(vp, na, &vtor(vp)->r_attr);
	set_attrcache_time(vp);
}

/*
 * Set attributes cache for given vnode using vnode attributes.
 */
nfs_attrcache_va(vp, va)
	struct vnode *vp;
	struct vattr *va;
{

	if ((vp->v_flag & VNOCACHE) || vtomi(vp)->mi_noac) {
		return;
	}
	vtor(vp)->r_attr = *va;
	vp->v_type = va->va_type;
	set_attrcache_time(vp);
}

set_attrcache_time(vp)
	struct vnode *vp;
{
	struct rnode *rp;
	int delta;

	rp = vtor(vp);
	rp->r_attrtime = time;
	/*
	 * Delta is the number of seconds that we will cache
	 * attributes of the file.  It is based on the number of seconds
	 * since the last change (i.e. files that changed recently
	 * are likely to change soon), but there is a minimum and
	 * a maximum for regular files and for directories.
	 */
	delta = (time.tv_sec - rp->r_attr.va_mtime.tv_sec) >> 4;
	if (vp->v_type == VDIR) {
		if (delta < vtomi(vp)->mi_acdirmin) {
			delta = vtomi(vp)->mi_acdirmin;
		} else if (delta > vtomi(vp)->mi_acdirmax) {
			delta = vtomi(vp)->mi_acdirmax;
		}
	} else {
		if (delta < vtomi(vp)->mi_acregmin) {
			delta = vtomi(vp)->mi_acregmin;
		} else if (delta > vtomi(vp)->mi_acregmax) {
			delta = vtomi(vp)->mi_acregmax;
		}
	}
	rp->r_attrtime.tv_sec += delta;
}

/*
 * Fill in attribute from the cache. If valid return 1 otherwise 0;
 */
int
nfs_getattr_cache(vp, vap)
	struct vnode *vp;
	struct vattr *vap;
{
	struct rnode *rp;

	rp = vtor(vp);
	if (timercmp(&time, &rp->r_attrtime, <)) {
		/*
		 * Cached attributes are valid
		 */
		*vap = rp->r_attr;
#ifdef NeXT
		/*
		 * This is a kludge to make programs that use
		 * dev from stat to tell file systems apart
		 * happy.  we kludge up a dev from the mount
		 * number and an arbitrary major number 255.
		 *
		 * I'm including the following compatiblility kludge
		 * to make the 1.0 Workspace happy.  It has 255 hard
		 * coded as the network major device number.
		 *	Morris Meyer,  December 4, 1989
		 */
		vap->va_fsid = 0xff00 | vtomi(vp)->mi_mntno;
#endif NeXT
#if	MACH_NBC
		/*
		 * Make sure size represents MFS vnode size.
		 */
		if (vap->va_size < vp->vm_info->vnode_size &&
		    (vp->vm_info->dirty || (rp->r_flags & RDIRTY)))
			vap->va_size = vp->vm_info->vnode_size;
#endif	MACH_NBC
		return (1);
	}
	return (0);
}

/*
 * Get attributes over-the-wire.
 * Return 0 if successful, otherwise error.
 */
int
nfs_getattr_otw(vp, vap, cred)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
{
	int error;
	struct nfsattrstat *ns;

#if	MACH
	ns = (struct nfsattrstat *)kalloc(sizeof (*ns));
#else	MACH
	ns = (struct nfsattrstat *)kmem_alloc(sizeof (*ns));
#endif	MACH
	error = rfscall(vtomi(vp), RFS_GETATTR, xdr_fhandle,
	    (caddr_t)vtofh(vp), xdr_attrstat, (caddr_t)ns, cred);
	if (error == 0) {
		error = geterrno(ns->ns_status);
		if (error == 0) {
			nattr_to_vattr(vp, &ns->ns_attr, vap);
#ifdef NeXT
			/*
			 * This is a kludge to make programs that use
			 * dev from stat to tell file systems apart
			 * happy.  we kludge up a dev from the mount
			 * number and an arbitrary major number 255.
			 *
			 * I'm including the following compatiblility kludge
			 * to make the 1.0 Workspace happy.  It has 255 hard
			 * coded as the network major device number.
			 *	Morris Meyer,  December 4, 1989
			 */
			vap->va_fsid = 0xff00 | vtomi(vp)->mi_mntno;
#endif NeXT
		} else {
			PURGE_STALE_FH(error, vp);
		}
	}
#if	MACH
	kfree((caddr_t)ns, sizeof (*ns));
#else	MACH
	kmem_free((caddr_t)ns, sizeof (*ns));
#endif	MACH
	return (error);
}

/*
 * Return either cached ot remote attributes. If get remote attr
 * use them to check and invalidate caches, then cache the new attributes.
 */
int
nfsgetattr(vp, vap, cred)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
{
	int error;

	if (nfs_getattr_cache(vp, vap)) {
		/*
		 * got cached attributes, we're done.
		 */
		return (0);
	}
	error = nfs_getattr_otw(vp, vap, cred);
	if (error == 0) {
		nfs_cache_check(vp, vap->va_mtime);
		nfs_attrcache_va(vp, vap);
	}
	return (error);
}

nattr_to_vattr(vp, na, vap)
	register struct vnode *vp;
	register struct nfsfattr *na;
	register struct vattr *vap;
{
	struct rnode *rp;

	rp = vtor(vp);
	vap->va_type = (enum vtype)na->na_type;
	vap->va_mode = na->na_mode;
	vap->va_uid = na->na_uid;
	vap->va_gid = na->na_gid;
	vap->va_fsid = 0x0ffff &
 	 (long)makedev(vfs_fixedmajor(vp->v_vfsp), 0x0ff & vtomi(vp)->mi_mntno);
	vap->va_nodeid = na->na_nodeid;
	vap->va_nlink = na->na_nlink;
#if	MACH_NBC
	/* FIXME: Is this correct? */
	/* It should check vm_info->dirty somewhere */
	if (na->na_size < vp->vm_info->vnode_size) {
		vap->va_size = vp->vm_info->vnode_size;
	} else {
		vap->va_size = na->na_size;
	}
	if (rp->r_size < vap->va_size || ((rp->r_flags & RDIRTY) == 0)){
		rp->r_size = vap->va_size;
	}
#else	MACH_NBC
	if (rp->r_size < na->na_size || ((rp->r_flags & RDIRTY) == 0)){
		rp->r_size = vap->va_size = na->na_size;
	} else {
		vap->va_size = rp->r_size;
	}
#endif	MACH_NBC
	vap->va_atime.tv_sec  = na->na_atime.tv_sec;
	vap->va_atime.tv_usec = na->na_atime.tv_usec;
	vap->va_mtime.tv_sec  = na->na_mtime.tv_sec;
	vap->va_mtime.tv_usec = na->na_mtime.tv_usec;
	vap->va_ctime.tv_sec  = na->na_ctime.tv_sec;
	vap->va_ctime.tv_usec = na->na_ctime.tv_usec;
	vap->va_rdev = na->na_rdev;
	vap->va_blocks = na->na_blocks;
	switch(na->na_type) {

	case NFBLK:
		vap->va_blocksize = BLKDEV_IOSIZE;
		break;

	case NFCHR:
		vap->va_blocksize = MAXBSIZE;
		break;

	default:
		vap->va_blocksize = na->na_blocksize;
		break;
	}
	/*
	 * This bit of ugliness is a *TEMPORARY* hack to preserve the
	 * over-the-wire protocols for named-pipe vnodes.  It remaps the
	 * special over-the-wire type to the VFIFO type. (see note in nfs.h)
	 *
	 * BUYER BEWARE:
	 *  If you are porting the NFS to a non-SUN server, you probably
	 *  don't want to include the following block of code.  The
	 *  over-the-wire special file types will be changing with the
	 *  NFS Protocol Revision.
	 */
	if (NA_ISFIFO(na)) {
		vap->va_type = VFIFO;
		vap->va_mode = (vap->va_mode & ~S_IFMT) | S_IFIFO;
		vap->va_rdev = 0;
		vap->va_blocksize = na->na_blocksize;
	}
}


