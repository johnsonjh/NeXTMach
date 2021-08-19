/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 **********************************************************************
 * HISTORY
 * 28-Mar-90  Brian Pinkerton at NeXT
 *	Changed ufs_pagein() to read directly into a page when possible.
 *
 * 21-Feb-90  Morris Meyer (mmeyer) at NeXT
 *	Added an imark after the b{a,d}write in ufs_pageout().
 *
 *  9-May-89  Peter King (king) at NeXT
 *	Backed out NFS 4.0 directory changes.  ufs_readdir uses the
 *	old struct direct rather than the new struct dirent.
 *	Sun Bugfixes: 1011330 - getdents doesn't change access time.
 *		      1012527 - non-root user can set the sticky bit of
 *				an executable.
 *
 * 01-Mar-89  John Seamons (jks) at NeXT
 *	MACH_NBC: keep vp->vm_info->vnode_size up to date from ip->i_size
 *	if the mapped file system is being bypassed via the
 *	open flag O_NO_MFS (used by single disk copy).
 *
 * 25-Jan-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes: ufs_readdir nows uses new directory format.
 *
 * 20-Sep-88  Avadis Tevanian (avie) at NeXT
 *	Fast symbolic links support.  Creation on NeXT, detection on
 *	NeXT and Multimax.
 *
 **********************************************************************
 */

#import <mach_nbc.h>
#import <xpr_debug.h>

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 2.125
 * @(#) from Berkeley 7.1 6/5/86 (sys_inode.c, ufs_syscalls.c)
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/vfs.h>
#import <sys/vfs_stat.h>
#import <sys/vnode.h>
#import <sys/proc.h>
#import <sys/file.h>
#import <sys/uio.h>
#import <sys/conf.h>
#import <sys/kernel.h>
#import <sys/stat.h>
#import <ufs/fs.h>
#import <ufs/inode.h>
#import <ufs/mount.h>
#import <ufs/fsdir.h>
#ifdef QUOTA
#import <ufs/quota.h>
#endif
#if	NeXT
#else	NeXT
#import <sys/dirent.h>
#endif	NeXT

#import <specfs/fifo.h>	/* this defines PIPE_BUF for ufs_getattr() */
#import <krpc/lockmgr.h>

#if	MACH
#import <kern/xpr.h>
#import <kern/kalloc.h>
#import <kern/mfs.h>

#import <kern/parallel.h>
#import <vm/vm_page.h>
#endif	MACH

#define ISVDEV(t) ((t == VCHR) || (t == VBLK) || (t == VFIFO) || (t == VSTR))

static int rwip();
static int chown1();

static	int ufs_open();
static	int ufs_close();
static	int ufs_rdwr();
static	int ufs_ioctl();
static	int ufs_select();
static	int ufs_getattr();
static	int ufs_setattr();
static	int ufs_access();
static	int ufs_lookup();
static	int ufs_create();
static	int ufs_remove();
static	int ufs_link();
static	int ufs_rename();
static	int ufs_mkdir();
static	int ufs_rmdir();
static	int ufs_readdir();
static	int ufs_symlink();
static	int ufs_readlink();
static	int ufs_fsync();
static	int ufs_inactive();
static	int ufs_bmap();
static	int ufs_strategy();
static	int ufs_bread();
static	int ufs_brelse();
static	int ufs_lockctl();
static	int ufs_fid();
static  int ufs_cmp();
static  int ufs_realvp();
static	int ufs_badop();
#if	MACH
static pager_return_t ufs_pagein();
static pager_return_t ufs_pageout();
extern int ufs_nlinks();
#endif	MACH

struct vnodeops ufs_vnodeops = {
	ufs_open,
	ufs_close,
	ufs_rdwr,
	ufs_ioctl,
	ufs_select,
	ufs_getattr,
	ufs_setattr,
	ufs_access,
	ufs_lookup,
	ufs_create,
	ufs_remove,
	ufs_link,
	ufs_rename,
	ufs_mkdir,
	ufs_rmdir,
	ufs_readdir,
	ufs_symlink,
	ufs_readlink,
	ufs_fsync,
	ufs_inactive,
	ufs_bmap,
	ufs_strategy,
	ufs_bread,
	ufs_brelse,
	ufs_lockctl,
	ufs_fid,
	ufs_badop,		/* dump */
	ufs_cmp,
	ufs_realvp,
#if	MACH
	ufs_pagein,
	ufs_pageout,
	ufs_nlinks,
#endif	MACH
};

int ufsDirectRead,ufsBufferRead;

/*ARGSUSED*/
static int
ufs_open(vpp, flag, cred)
	struct vnode **vpp;
	int flag;
	struct ucred *cred;
{

	VFS_RECORD((*vpp)->v_vfsp, VS_OPEN, VS_CALL);
	return (0);
}

/*ARGSUSED*/
static int
ufs_close(vp, flag, count, cred)
	struct vnode *vp;
	int flag;
	int count;
	struct ucred *cred;
{
	struct inode *ip;
	
	VFS_RECORD(vp->v_vfsp, VS_CLOSE, VS_CALL);
	
#if	NeXT
	/*
	 *  We need to ensure that the file we're closing is flushed from the
	 *  buffer cache (i.e. completely flushed to disk).  Also, we need to
	 *  mark the modification time NOW, and turn off IUPD (data has changed)
	 *  to ensure that a subsequent flush of the INODE (not the data) doesn't
	 *  change the data modification time. 
	 */
	ip = VTOI(vp);
	
	bflush(vp, NODEV, NODEV);	/* start delayed writes */
	IMARK(ip);
	ip->i_flag &= ~IUPD;
#endif	NeXT

	return (0);
}

/*
 * read or write a vnode
 */
/*ARGSUSED*/
static int
ufs_rdwr(vp, uiop, rw, ioflag, cred)
	struct vnode *vp;
	struct uio *uiop;
	enum uio_rw rw;
	int ioflag;
	struct ucred *cred;
{
	register struct inode *ip;
	int error;
	int didlock;

	/*
	 *  flush the file from the mfs cache before we lock the inode
	 */
	if (rw == UIO_WRITE && vp->vm_info->pager != vm_pager_null)
		vnode_uncache(vp);

	ip = VTOI(vp);
	if  ((ip->i_mode & IFMT) == IFREG) {
		didlock = 1;
		ILOCK(ip);
		if ((ioflag & IO_APPEND) != 0 && (rw == UIO_WRITE)) {
			/*
			 * In append mode start at end of file after locking it.
			 */
			uiop->uio_offset = ip->i_size;
		}
	} else {
		didlock = 0;
	}

	error = rwip(ip, uiop, rw, ioflag);
	ITIMES(ip);

	if (didlock)
		IUNLOCK(ip);

	return (error);
}

/*
 * Don't cache write blocks to files with the sticky bit set.
 * Used to keep swap files from blowing the page cache on a server.
 */
int stickyhack = 1;

/*
 * rwip does the real work of read or write requests for ufs.
 */
static int
rwip(ip, uio, rw, ioflag)
	register struct inode *ip;
	register struct uio *uio;
	enum uio_rw rw;
	int ioflag;
{
	struct vnode *devvp;
	struct buf *bp;
	struct fs *fs;
	daddr_t lbn, bn;
	register int n, on, type;
	int size;
	long bsize;
	extern int mem_no;
	int error = 0;
	int iupdat_flag = 0;
	u_short execmask = (IEXEC | (IEXEC >> 3) | (IEXEC >> 6));

	if (rw != UIO_READ && rw != UIO_WRITE)
		panic("rwip");
	type = ip->i_mode&IFMT;
	if (type != IFREG && type != IFDIR && type != IFLNK)
		panic("rwip type");
	if ( ((int)uio->uio_offset < 0 ||
	      ((int)uio->uio_offset + (int)uio->uio_resid) < 0))
		return (EINVAL);
	if (uio->uio_resid == 0)
		return (0);

	if (rw == UIO_WRITE) {
		if (type == IFREG && uio->uio_offset + uio->uio_resid >
		    u.u_rlimit[RLIMIT_FSIZE].rlim_cur) {
			psignal(u.u_procp, SIGXFSZ);
			return (EFBIG);
		}
	} else {
		ip->i_flag |= IACC;
	}
	devvp = ip->i_devvp;
	fs = ip->i_fs;
	bsize = fs->fs_bsize;
	u.u_error = 0;
	do {
		lbn = uio->uio_offset / bsize;
		on = uio->uio_offset % bsize;
		n = MIN((unsigned)(bsize - on), uio->uio_resid);
		if (rw == UIO_READ) {
			int diff = ip->i_size - uio->uio_offset;

			VFS_RECORD(ITOV(ip)->v_vfsp, VS_READ, VS_CALL);
			if (diff <= 0) {
				error = 0;
				goto out;
			}
			if (diff < n)
				n = diff;
		} else {
			VFS_RECORD(ITOV(ip)->v_vfsp, VS_WRITE, VS_CALL);
		}
		bn = fsbtodb(fs,
		    bmap(ip, lbn, rw == UIO_WRITE ? B_WRITE: B_READ,
		      (int)(on+n), (ioflag & IO_SYNC ? &iupdat_flag : 0) ));
		if (u.u_error || rw == UIO_WRITE && (long)bn<0) {
			error = u.u_error;
			goto out;
		}
		if (rw == UIO_WRITE && (uio->uio_offset + n > ip->i_size) &&
		   (type == IFDIR || type == IFREG || type == IFLNK)) {
			ip->i_size = uio->uio_offset + n;
#if	MACH_NBC
			/*
			 *  The mapped file system (mfs) caches the file length
			 *  in vp->vm_info->vnode_size and pretty much ignores
			 *  ip->i_size.  If we happen to bypass the mfs
			 *  routines by setting the open flag O_NO_MFS
			 *  (single disk copy does this) then we need to keep 
			 *  vnode_size up to date.  We also need to invalidate
			 *  the mfs file cache during a bypass, but that's
			 *  another story..
			 */
			if (ip->i_size > ITOV(ip)->vm_info->vnode_size)
				ITOV(ip)->vm_info->vnode_size = ip->i_size;
#endif	MACH_NBC
			if (ioflag & IO_SYNC) {
				iupdat_flag = 1;
			}
		}
		size = blksize(fs, ip, lbn);
		if (rw == UIO_READ) {
			if ((long)bn<0) {
				bp = geteblk(size);
				clrbuf(bp);
			} else if (ip->i_lastr + 1 == lbn)
				bp = breada(devvp, bn, size, rablock, rasize);
			else
				bp = bread(devvp, bn, size);
			ip->i_lastr = lbn;
		} else {
#if	MACH
#else	MACH
			int i, count;
			extern struct cmap *mfind();

			count = howmany(size, DEV_BSIZE);
			for (i = 0; i < count; i += CLBYTES/DEV_BSIZE)
				if (mfind(devvp, (daddr_t)(bn + i)))
					munhash(devvp, (daddr_t)(bn + i));
#endif	MACH
			if (n == bsize) 
				bp = getblk(devvp, bn, size);
			else
				bp = bread(devvp, bn, size);
		}
		n = MIN(n, bp->b_bcount - bp->b_resid);
		if (bp->b_flags & B_ERROR) {
			error = EIO;
			brelse(bp);
			goto out;
		}
		u.u_error = uiomove(bp->b_un.b_addr+on, n, rw, uio);
		if ((ioflag & IO_SYNC) && (ip->i_mode & ISVTX) && stickyhack
		  && (ip->i_mode & execmask) == 0)
		      bp->b_flags |= B_NOCACHE;
		if (rw == UIO_READ) {
			if (n + on == bsize || uio->uio_offset == ip->i_size)
				bp->b_flags |= B_AGE;
			brelse(bp);
		} else {
			if ((ioflag & IO_SYNC) || (ip->i_mode&IFMT) == IFDIR)
				bwrite(bp);
			else if (n + on == bsize) {
				bp->b_flags |= B_AGE;
				bawrite(bp);
			} else
				bdwrite(bp);
			ip->i_flag |= IUPD|ICHG;
			if (u.u_ruid != 0)
				ip->i_mode &= ~(ISUID|ISGID);
		}
	} while (u.u_error == 0 && uio->uio_resid > 0 && n != 0);
	if (iupdat_flag) {
		iupdat(ip, 1);
	}
	if (error == 0)				/* XXX */
		error = u.u_error;		/* XXX */
out:
	return (error);
}

/*ARGSUSED*/
static int
ufs_ioctl(vp, com, data, flag, cred)
	struct vnode *vp;
	int com;
	caddr_t data;
	int flag;
	struct ucred *cred;
{

	VFS_RECORD(vp->v_vfsp, VS_IOCTL, VS_CALL);
	return (EINVAL);
}

/*ARGSUSED*/
static int
ufs_select(vp, which, cred)
	struct vnode *vp;
	int which;
	struct ucred *cred;
{

	VFS_RECORD(vp->v_vfsp, VS_SELECT, VS_CALL);
	return (EINVAL);
}

/*ARGSUSED*/
static int
ufs_getattr(vp, vap, cred)
	struct vnode *vp;
	register struct vattr *vap;
	struct ucred *cred;
{
	register struct inode *ip;

	VFS_RECORD(vp->v_vfsp, VS_GETATTR, VS_CALL);

	ip = VTOI(vp);
	/*
	 * Mark correct time in inode.
	 */
	ITIMES(ip);
	/*
	 * Copy from inode table.
	 */
	vap->va_type = IFTOVT(ip->i_mode);
	vap->va_mode = ip->i_mode;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_fsid = ip->i_dev;
	vap->va_nodeid = ip->i_number;
	vap->va_nlink = ip->i_nlink;
#if	MACH_NBC
	if (vp->v_type == VREG) {
		vap->va_size = vp->vm_info->vnode_size;
	} else
#endif	MACH_NBC
	vap->va_size = ip->i_size;
	vap->va_atime.tv_sec = ip->i_atime;
	vap->va_atime.tv_usec = 0;
	vap->va_mtime.tv_sec = ip->i_mtime;
	vap->va_mtime.tv_usec = 0;
	vap->va_ctime.tv_sec = ip->i_ctime;
	vap->va_ctime.tv_usec = 0;
	vap->va_rdev = ip->i_rdev;
	vap->va_blocks = btosb(dbtob(ip->i_blocks));

	switch(ip->i_mode & IFMT) {
#define OLDCODE
#ifdef OLDCODE
	case IFBLK:
		vap->va_blocksize = BLKDEV_IOSIZE;
		break;

	case IFCHR:
		vap->va_blocksize = MAXBSIZE;
		break;
#else
	case IFBLK:				/* was BLKDEV_IOSIZE */
	case IFCHR:
		vap->va_blocksize = MAXBSIZE;
		break;
#endif OLDCODE

	default:
		vap->va_blocksize = vp->v_vfsp->vfs_bsize;
		break;
	}
	return (0);
}

static int
ufs_setattr(vp, vap, cred)
	register struct vnode *vp;
	register struct vattr *vap;
	struct ucred *cred;
{
	register struct inode *ip;
	int chtime = 0;
	int error = 0;

	VFS_RECORD(vp->v_vfsp, VS_SETATTR, VS_CALL);

	/*
	 * Cannot set these attributes
	 */
	if ((vap->va_nlink != -1) || (vap->va_blocksize != -1) ||
	    (vap->va_rdev != -1) || (vap->va_blocks != -1) ||
	    (vap->va_fsid != -1) || (vap->va_nodeid != -1) ||
	    ((int)vap->va_type != -1)) {
		return (EINVAL);
	}

	ip = VTOI(vp);
	ilock(ip);
	/*
	 * Change file access modes.  Must be owner or su.
	 */
	if (vap->va_mode != (u_short)-1) {
		error = OWNER(cred, ip);
		if (error)
			goto out;
		ip->i_mode &= IFMT;
		ip->i_mode |= vap->va_mode & ~IFMT;
		if (cred->cr_uid != 0) {
			if ((ip->i_mode & IFMT) != IFDIR)
				ip->i_mode &= ~ISVTX;
			if (!groupmember(ip->i_gid))
				ip->i_mode &= ~ISGID;
		}
		ip->i_flag |= ICHG;
	}
	/*
	 * To change file ownership, must be su.
	 * To change group ownership, must be su or owner and in target group.
	 * This is now enforced in chown1() below.
	 */
	if ((vap->va_uid != (uid_t)-1) || (vap->va_gid != (gid_t)-1)) {
		error = chown1(ip, vap->va_uid, vap->va_gid);
		if (error)
			goto out;
	}
	/*
	 * Truncate file.  Must have write permission and not be a directory.
	 */
	if (vap->va_size != (u_long)-1) {
		if ((ip->i_mode & IFMT) == IFDIR) {
			error = EISDIR;
			goto out;
		}
		if ((error = iaccess(ip, IWRITE)) != 0) {
			goto out;
		}
		if ((error = itrunc(ip, vap->va_size)) != 0) {
			goto out;
		}
	}
#if	MACH_NBC
	/*
	 *	Sync out all blocks to prevent delayed writes from
	 *	changing the modified time later.  Need to unlock
	 *	the inode so the pager can page out the pages... this
	 *	is a bit hokey but we'll fix it when we fix
	 *	the rest of the filesystem.
	 */
	iunlock(ip);
	(void) mfs_fsync(vp);
	ilock(ip);
#endif	MACH_NBC
	/*
	 * Change file access or modified times.
	 */
	if (vap->va_atime.tv_sec != -1) {
		error = OWNER(cred, ip);
		if (error)
			goto out;
		ip->i_atime = vap->va_atime.tv_sec;
		chtime++;
	}
	if (vap->va_mtime.tv_sec != -1) {
		error = OWNER(cred, ip);
		if (error)
			goto out;
		ip->i_mtime = vap->va_mtime.tv_sec;
		chtime++;
	}
	if (chtime) {
		ip->i_ctime = time.tv_sec;
		ip->i_flag |= IMOD;
	}
out:
	iupdat(ip, 1);			/* XXX - should be async for perf */
	iunlock(ip);
	return (error);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
chown1(ip, uid, gid)
	register struct inode *ip;
	register uid_t uid;
	register gid_t gid;
{
#ifdef QUOTA
	register long change;
#endif

	if (uid == (uid_t) -1)
		uid = ip->i_uid;
	if (gid == (gid_t) -1)
		gid = ip->i_gid;

	/*
	 * If:
	 *    1) not the owner of the file, or
	 *    2) trying to change the owner of the file, or
	 *    3) trying to change the group of the file to a group not in the
	 *	 process' group set,
	 * then must be super-user.
	 * Check super-user last, and use "suser", so that the accounting
	 * file's "used super-user privileges" flag is properly set.
	 */
	if ((u.u_uid != uid || uid != ip->i_uid || !groupmember(gid)) &&
	    !suser())
		return (EPERM);

#ifdef QUOTA
	if (ip->i_uid == uid)		/* this just speeds things a little */
		change = 0;
	else
		change = ip->i_blocks;
	(void) chkdq(ip, -change, 1);
	(void) chkiq(VFSTOM(ip->i_vnode.v_vfsp), ip, ip->i_uid, 1);
	dqrele(ip->i_dquot);
#endif
	ip->i_uid = uid;
	ip->i_gid = gid;
	ip->i_flag |= ICHG;
	if (u.u_uid != 0)
		ip->i_mode &= ~(ISUID|ISGID);
#ifdef QUOTA
	ip->i_dquot = getinoquota(ip);
	(void) chkdq(ip, change, 1);
	(void) chkiq(VFSTOM(ip->i_vnode.v_vfsp), (struct inode *)NULL, uid, 1);
#endif
	return (0);
}

/*ARGSUSED*/
static int
ufs_access(vp, mode, cred)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
{
	register struct inode *ip;
	int error;

	VFS_RECORD(vp->v_vfsp, VS_ACCESS, VS_CALL);

	ip = VTOI(vp);
	ILOCK(ip);
	error = iaccess(ip, mode);
	iunlock(ip);
	return (error);
}

/*ARGSUSED*/
static int
ufs_readlink(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	register struct inode *ip;
	register int error;

	VFS_RECORD(vp->v_vfsp, VS_READLINK, VS_CALL);

	if (vp->v_type != VLNK)
		return (EINVAL);
	ip = VTOI(vp);
#if	defined(NeXT) || defined(MULTIMAX)
	if (ip->i_icflags & IC_FASTLINK) {
		error = uiomove(ip->i_symlink, ip->i_size, UIO_READ, uiop);
	}
	else
#endif	defined(NeXT) || defined(MULTIMAX)
	error = rwip(ip, uiop, UIO_READ, 0);
	ITIMES(ip);
	return (error);
}

/*ARGSUSED*/
static int
ufs_fsync(vp, cred)
	struct vnode *vp;
	struct ucred *cred;
{
	register struct inode *ip;

	VFS_RECORD(vp->v_vfsp, VS_FSYNC, VS_CALL);

	ip = VTOI(vp);
	ilock(ip);
	syncip(ip);			/* do synchronous writes */
	iunlock(ip);

	return (0);
}

/*ARGSUSED*/
static int
ufs_inactive(vp, cred)
	struct vnode *vp;
	struct ucred *cred;
{

	VFS_RECORD(vp->v_vfsp, VS_INACTIVE, VS_CALL);

	iinactive(VTOI(vp));
	return (0);
}

/*
 * Unix file system operations having to do with directory manipulation.
 */
/*ARGSUSED*/
static int
ufs_lookup(dvp, nm, vpp, cred, pnp, flags)
	struct vnode *dvp;
	char *nm;
	struct vnode **vpp;
	struct ucred *cred;
	struct pathname *pnp;
	int flags;
{
	register struct inode *ip;
	struct inode *xip;
	register int error;

	VFS_RECORD(dvp->v_vfsp, VS_LOOKUP, VS_CALL);

	ip = VTOI(dvp);
	error = dirlook(ip, nm, &xip);
	ITIMES(ip);
	if (error == 0) {
		ip = xip;
		*vpp = ITOV(ip);
		if ((ip->i_mode & ISVTX) && !(ip->i_mode & (IEXEC | IFDIR)) &&
		    stickyhack) {
			(*vpp)->v_flag |= VISSWAP;
		}
		ITIMES(ip);
		iunlock(ip);
		/*
		 * If vnode is a device return special vnode instead
		 */
		if (ISVDEV((*vpp)->v_type)) {
			struct vnode *newvp;

			newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type);
			VN_RELE(*vpp);
			*vpp = newvp;
		}
	}
	return (error);
}

static int
ufs_create(dvp, nm, vap, exclusive, mode, vpp, cred)
	struct vnode *dvp;
	char *nm;
	struct vattr *vap;
	enum vcexcl exclusive;
	int mode;
	struct vnode **vpp;
	struct ucred *cred;
{
	register int error;
	register struct inode *ip;
	struct inode *xip;

	VFS_RECORD(dvp->v_vfsp, VS_CREATE, VS_CALL);

	/*
	 * Can't create directories - use ufs_mkdir instead.
	 */
	if (vap->va_type == VDIR)
		return (EISDIR);
	xip = (struct inode *)0;
	ip = VTOI(dvp);

	/* Must be super-user to set sticky bit */
	if (cred->cr_uid != 0)
		vap->va_mode &= ~VSVTX;

	error = direnter(ip, nm, DE_CREATE, (struct inode *)0,
	    (struct inode *)0, vap, &xip);
	ITIMES(ip);
	ip = xip;
	/*
	 * If file exists and this is a nonexclusive create,
	 * check for not directory and access permissions.
	 * If create/read-only an existing directory, allow it.
	 */
	if (error == EEXIST) {
		if (exclusive == NONEXCL) {
			if (((ip->i_mode & IFMT) == IFDIR) && (mode & IWRITE)) {
				error = EISDIR;
			} else if (mode) {
				error = iaccess(ip, mode);
			} else {
				error = 0;
			}
		}
		if (error) {
			iput(ip);
		} else if (((ip->i_mode&IFMT) == IFREG) && (vap->va_size == 0)){
			/*
			 * Truncate regular files, if required
			 */
			(void) itrunc(ip, (u_long)0);
		}
	}
	if (error) {
		return (error);
	}
	*vpp = ITOV(ip);
	ITIMES(ip);
	iunlock(ip);
	/*
	 * If vnode is a device return special vnode instead
	 */
	if (ISVDEV((*vpp)->v_type)) {
		struct vnode *newvp;

		newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type);
		VN_RELE(*vpp);
		*vpp = newvp;
	}

	if (vap != (struct vattr *)0) {
		(void) VOP_GETATTR(*vpp, vap, cred);
	}
	return (error);
}

/*ARGSUSED*/
static int
ufs_remove(vp, nm, cred)
	struct vnode *vp;
	char *nm;
	struct ucred *cred;
{
	register int error;
	register struct inode *ip;

	VFS_RECORD(vp->v_vfsp, VS_REMOVE, VS_CALL);

	ip = VTOI(vp);
	error = dirremove(ip, nm, (struct inode *)0, 0);
	ITIMES(ip);
	return (error);
}

/*
 * Link a file or a directory
 * If source is a directory, must be superuser.
 */
/*ARGSUSED*/
static int
ufs_link(vp, tdvp, tnm, cred)
	struct vnode *vp;
	register struct vnode *tdvp;
	char *tnm;
	struct ucred *cred;
{
	register struct inode *sip;
	register int error;
	struct vnode *realvp;

	if (VOP_REALVP(vp, &realvp) == 0) {
		vp = realvp;
	}

	VFS_RECORD(vp->v_vfsp, VS_LINK, VS_CALL);

	sip = VTOI(vp);
	if (((sip->i_mode & IFMT) == IFDIR) && !suser()) {
		return (EPERM);
	}
	error = direnter(VTOI(tdvp), tnm, DE_LINK,
	    (struct inode *)0, sip, (struct vattr *)0, (struct inode **)0);
	ITIMES(sip);
	ITIMES(VTOI(tdvp));
	return (error);
}

/*
 * Rename a file or directory.
 * We are given the vnode and entry string of the source and the
 * vnode and entry string of the place we want to move the source to
 * (the target). The essential operation is:
 *	unlink(target);
 *	link(source, target);
 *	unlink(source);
 * but "atomically".  Can't do full commit without saving state in the inode
 * on disk, which isn't feasible at this time.  Best we can do is always
 * guarantee that the TARGET exists.
 */
/*ARGSUSED*/
static int
ufs_rename(sdvp, snm, tdvp, tnm, cred)
	struct vnode *sdvp;		/* old (source) parent vnode */
	char *snm;			/* old (source) entry name */
	struct vnode *tdvp;		/* new (target) parent vnode */
	char *tnm;			/* new (target) entry name */
	struct ucred *cred;
{
	struct inode *sip;		/* source inode */
	register struct inode *sdp;	/* old (source) parent inode */
	register struct inode *tdp;	/* new (target) parent inode */
	register int error;

	VFS_RECORD(sdvp->v_vfsp, VS_RENAME, VS_CALL);

	sdp = VTOI(sdvp);
	tdp = VTOI(tdvp);
	/*
	 * Make sure we can delete the source entry.
	 */
	error = iaccess(sdp, IWRITE);
	if (error) {
		return (error);
	}
	/*
	 * Look up inode of file we're supposed to rename.
	 */
	error = dirlook(sdp, snm, &sip);
	if (error) {
		return (error);
	}

	iunlock(sip);			/* unlock inode (it's held) */

#if	NeXT
	/*
	 * If source directory is "sticky", then user must own the directory,
	 * or the entry in it, else he may not rename it (unless he's root).
	 */
	if ((sdp->i_mode & ISVTX) && u.u_uid != 0 &&
	    u.u_uid != sdp->i_uid && sip->i_uid != u.u_uid) {
	    	error = EPERM;
		goto out;
	}
#endif	NeXT

	/*
	 * Check for renaming '.' or '..' or alias of '.'
	 */
	if ((strcmp(snm, ".") == 0) || (strcmp(snm, "..") == 0) ||
	    (sdp == sip)) {
		error = EINVAL;
		goto out;
	}
	/*
	 * Link source to the target.
	 */
	error = direnter(tdp, tnm, DE_RENAME,
	    sdp, sip, (struct vattr *)0, (struct inode **)0);
	if (error) {
		/*
		 * ESAME isn't really an error; it indicates that the
		 * operation should not be done because the source and target
		 * are the same file, but that no error should be reported.
		 */
		if (error == ESAME)
			error = 0;
		goto out;
	}

	/*
	 * Unlink the source.
	 * Remove the source entry.  Dirremove checks that the entry
	 * still reflects sip, and returns an error if it doesn't.
	 * If the entry has changed just forget about it.
	 * Release the source inode.
	 */
	error = dirremove(sdp, snm, sip, 0);
	if (error == ENOENT) {
		error = 0;
	} else if (error) {
		goto out;
	}

out:
	ITIMES(sdp);
	ITIMES(tdp);
	irele(sip);
	return (error);
}

/*ARGSUSED*/
static int
ufs_mkdir(dvp, nm, vap, vpp, cred)
	struct vnode *dvp;
	char *nm;
	register struct vattr *vap;
	struct vnode **vpp;
	struct ucred *cred;
{
	register struct inode *ip;
	struct inode *xip;
	register int error;

	VFS_RECORD(dvp->v_vfsp, VS_MKDIR, VS_CALL);

	ip = VTOI(dvp);
	error =
	    direnter(ip, nm, DE_CREATE,
		(struct inode *)0, (struct inode *)0, vap, &xip);
	ITIMES(ip);
	if (error == 0) {
		ip = xip;
		*vpp = ITOV(ip);
		ITIMES(ip);
		iunlock(ip);
	} else if (error == EEXIST) {
		iput(xip);
	}
	return (error);
}

/*ARGSUSED*/
static int
ufs_rmdir(vp, nm, cred)
	struct vnode *vp;
	char *nm;
	struct ucred *cred;
{
	register struct inode *ip;
	register int error;

	VFS_RECORD(vp->v_vfsp, VS_RMDIR, VS_CALL);

	ip = VTOI(vp);
	error = dirremove(ip, nm, (struct inode *)0, 1);
	ITIMES(ip);
	return (error);
}

/*ARGSUSED*/
static int
ufs_readdir(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
#if	NeXT
#else	NeXT
	register struct iovec *iovp;
	register struct inode *ip;
	register struct direct *idp;
	register struct dirent *odp;
	register int incount;
	register int outcount = 0;
	register u_int offset;
	register u_int count, bytes_read;
	struct iovec t_iovec;
	struct uio t_uio;
	caddr_t outbuf;
	caddr_t inbuf;
	u_int bufsize;
	int error = 0;
	static caddr_t *dirbufp = NULL;
#if	MACH
#else	MACH
	caddr_t kmem_alloc();
#endif	MACH

	VFS_RECORD(vp->v_vfsp, VS_READDIR, VS_CALL);

	ip = VTOI(vp);
	iovp = uiop->uio_iov;
	count = iovp->iov_len;

	/*
	 * Get space to change directory entries into fs independent format.
	 * Also, get space to buffer the data returned by rwip.
	 */
	bufsize = count + sizeof (struct dirent);
#if	MACH
	outbuf = kalloc(bufsize * 2);
#else	MACH
	outbuf = kmem_alloc(bufsize * 2);
#endif	MACH
	odp = (struct dirent *)outbuf;
	iovp = &t_iovec;
	inbuf = (caddr_t)((u_int)outbuf + bufsize);

	/* Force offset to be valid (to guard against bogus lseek() values) */
	offset = uiop->uio_offset & ~(DIRBLKSIZ - 1);

nextblk:
	/*
	 * Setup a temporary uio structure to handle the directory information
	 * before reformatting.
	 */
	iovp->iov_len = count;
	iovp->iov_base = inbuf;
	t_uio.uio_iov = iovp;
	t_uio.uio_iovcnt = 1;
	t_uio.uio_segflg = UIO_SYSSPACE;
	t_uio.uio_offset = offset;
	t_uio.uio_fmode = uiop->uio_fmode;
	t_uio.uio_resid = uiop->uio_resid;

	if (error = rwip(ip, &t_uio, UIO_READ, 0))
		goto out;
	bytes_read = uiop->uio_resid - t_uio.uio_resid;

	incount = 0;
	idp = (struct direct *)inbuf;

	/* Transform to file-system independent format */
	while (incount < bytes_read) {
		extern char *strcpy();

		/* skip empty entries */
		if (idp->d_ino != 0 && offset >= uiop->uio_offset) {
			odp->d_fileno = idp->d_ino;
			odp->d_namlen = idp->d_namlen;
			(void) strcpy(odp->d_name, idp->d_name);
			odp->d_reclen = DIRSIZ(odp);
			odp->d_off = offset + idp->d_reclen;
			outcount += odp->d_reclen;
			/* Got as many bytes as requested, quit */
			if (outcount > count) {
				outcount -= odp->d_reclen;
 				break;
			}
			odp = (struct dirent *)((int)odp + odp->d_reclen);
		}
		incount += idp->d_reclen;
		offset += idp->d_reclen;
		idp = (struct direct *)((int)idp + idp->d_reclen);
	}

        /* Read whole block, but got no entries, read another if not eof */
        if (offset < ip->i_size && outcount == 0)
                goto nextblk;
 
	/* Copy out the entry data */
	if (error = uiomove(outbuf, outcount, UIO_READ, uiop))
		goto out;

	uiop->uio_offset = offset;
	ip->i_flag |= IACC;
out:
	ITIMES(ip);
#if	MACH
	kfree(outbuf, (bufsize * 2));
#else	MACH
	kmem_free(outbuf, (bufsize * 2));
#endif	MACH
	return (error);
}

/*
 * Old form of the ufs_readdir op. Returns directory entries directly
 * from the disk in the 4.2 structure instead of the new sys/dirent.h
 * structure. This routine is called directly by the old getdirentries
 * system call when it discovers it is dealing with a ufs filesystem.
 * The reason for this mess is to avoid large performance penalties
 * that occur during conversion from the old format to the new and
 * back again.
 */

/*ARGSUSED*/
int
old_ufs_readdir(vp, uiop, cred)
	struct vnode *vp;
	register struct uio *uiop;
	struct ucred *cred;
{
#endif	NeXT
	register struct iovec *iovp;
	register unsigned count;
	register struct inode *ip;
	int error;

	ip = VTOI(vp);
	iovp = uiop->uio_iov;
	count = iovp->iov_len;
	if ((uiop->uio_iovcnt != 1) || (count < DIRBLKSIZ) ||
	    (uiop->uio_offset & (DIRBLKSIZ -1)))
		return (EINVAL);
	count &= ~(DIRBLKSIZ - 1);
	uiop->uio_resid -= iovp->iov_len - count;
	iovp->iov_len = count;
	error = rwip(ip, uiop, UIO_READ, 0);
	ITIMES(ip);
	return (error);
}

int dosymlink = 1;

/*ARGSUSED*/
static int
ufs_symlink(dvp, lnm, vap, tnm, cred)
	register struct vnode *dvp;
	char *lnm;
	struct vattr *vap;
	char *tnm;
	struct ucred *cred;
{
	struct inode *ip;
	register int error;
	int	len;

	VFS_RECORD(dvp->v_vfsp, VS_SYMLINK, VS_MISS);

	ip = (struct inode *)0;
	vap->va_type = VLNK;
	vap->va_rdev = 0;
	error = direnter(VTOI(dvp), lnm, DE_CREATE,
	    (struct inode *)0, (struct inode *)0, vap, &ip);
	if (error == 0) {
#if	defined(NeXT)
		len = strlen(tnm);
		if ((len < MAX_FASTLINK_SIZE) && (dosymlink)) {
			bcopy(tnm, ip->i_symlink, len);
			ip->i_icflags |= IC_FASTLINK;
			ip->i_size = 
#if	MACH_NBC
			ITOV(ip)->vm_info->vnode_size = len;	/* for stat */
#endif	MACH_NBC
			ip->i_flag |= (IUPD|ICHG);
		}
		else
			error = rdwri(UIO_WRITE, ip, tnm, len,
				      (off_t)0, UIO_SYSSPACE, (int *)0);
#else	defined(NeXT)
		error = rdwri(UIO_WRITE, ip, tnm, strlen(tnm),
		    (off_t)0, UIO_SYSSPACE, (int *)0);
#endif	defined(NeXT)
		iput(ip);
	} else if (error == EEXIST) {
		iput(ip);
	}
	ITIMES(VTOI(dvp));
	return (error);
}

/*
 * Ufs specific routine used to do ufs io.
 */
int
rdwri(rw, ip, base, len, offset, seg, aresid)
	enum uio_rw rw;
	struct inode *ip;
	caddr_t base;
	int len;
	off_t offset;
	int seg;
	int *aresid;
{
	struct uio auio;
	struct iovec aiov;
	register int error;

	aiov.iov_base = base;
	aiov.iov_len = len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = offset;

#if	MACH
	auio.uio_segflg = seg;
#else	MACH
	auio.uio_seg = seg;
#endif	MACH
	auio.uio_resid = len;
	error = ufs_rdwr(ITOV(ip), &auio, rw, 0, u.u_cred);
	if (aresid) {
		*aresid = auio.uio_resid;
	} else if (auio.uio_resid) {
		error = EIO;
	}
	return (error);
}

static int
ufs_bmap(vp, lbn, vpp, bnp)
	struct vnode *vp;
	daddr_t lbn;
	struct vnode **vpp;
	daddr_t *bnp;
{
	register struct inode *ip;

	VFS_RECORD(vp1->v_vfsp, VS_BMAP, VS_CALL);
	ip = VTOI(vp);
	if (vpp)
		*vpp = ip->i_devvp;
	if (bnp)
		*bnp = fsbtodb(ip->i_fs, bmap(ip, lbn, B_READ, 0, 0));
	return (0);
}

/*
 * read a logical block and return it in a buffer
 */
/*ARGSUSED*/
static int
ufs_bread(vp, lbn, bpp, sizep)
	struct vnode *vp;
	daddr_t lbn;
	struct buf **bpp;
	long *sizep;
{
	register struct inode *ip;
	register struct buf *bp;
	register daddr_t bn;
	register int size;

	VFS_RECORD(vp1->v_vfsp, VS_BREAD, VS_CALL);
	ip = VTOI(vp);
	size = blksize(ip->i_fs, ip, lbn);
	bn = fsbtodb(ip->i_fs, bmap(ip, lbn, B_READ, 0, 0));
	if ((long)bn < 0) {
		bp = geteblk(size);
		clrbuf(bp);
	} else if (ip->i_lastr + 1 == lbn) {
		bp = breada(ip->i_devvp, bn, size, rablock, rasize);
	} else {
		bp = bread(ip->i_devvp, bn, size);
	}
	ip->i_lastr = lbn;
	ip->i_flag |= IACC;
	IMARK(ip);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (EIO);
	} else {
		*bpp = bp;
		return (0);
	}
}

/*
 * release a block returned by ufs_bread
 */
/*ARGSUSED*/
static int
ufs_brelse(vp, bp)
	struct vnode *vp;
	struct buf *bp;
{
	VFS_RECORD(vp1->v_vfsp, VS_BRELSE, VS_CALL);
	bp->b_flags |= B_AGE;
	bp->b_resid = 0;
	brelse(bp);
}

static int
ufs_cmp(vp1, vp2)
	struct vnode *vp1, *vp2;
{

	VFS_RECORD(vp1->v_vfsp, VS_CMP, VS_CALL);
	return (vp1 == vp2);
}

/*ARGSUSED*/
static int
ufs_realvp(vp, vpp)
	struct vnode *vp;
	struct vnode **vpp;
{

	VFS_RECORD(vp->v_vfsp, VS_REALVP, VS_CALL);
	return (EINVAL);
}

static int
ufs_badop()
{
	panic("ufs_badop");
}

#if	SUN_LOCK
/*
 * Record-locking requests are passed to the local Lock-Manager daemon.
 */
static int
ufs_lockctl(vp, ld, cmd, cred, clid)
	struct vnode *vp;
	struct flock *ld;
	int cmd;
	struct ucred *cred;
	int clid;
{
	lockhandle_t lh;
	struct fid *fidp;

	VFS_RECORD(vp->v_vfsp, VS_LOCKCTL, VS_CALL);

	/* Convert vnode into lockhandle-id. This is awfully like makefh(). */
	if (VOP_FID(vp, &fidp) || fidp == NULL) {
		return (EINVAL);
	}
	bzero((caddr_t)&lh.lh_id, sizeof (lh.lh_id));	/* clear extra bytes */
	lh.lh_fsid.val[0] = vp->v_vfsp->vfs_fsid.val[0];
	lh.lh_fsid.val[1] = vp->v_vfsp->vfs_fsid.val[1];
	lh.lh_fid.fid_len = fidp->fid_len;
	bcopy(fidp->fid_data, lh.lh_fid.fid_data, fidp->fid_len);
	freefid(fidp);

	/* Add in vnode and server and call to common code */
	lh.lh_vp = vp;
	lh.lh_servername = hostname;
	return (klm_lockctl(&lh, ld, cmd, cred, clid));
}
#else	SUN_LOCK
int
ufs_lockctl()
{
    return(EINVAL);
}
#endif	SUN_LOCK

static int
ufs_fid(vp, fidpp)
	struct vnode *vp;
	struct fid **fidpp;
{
	register struct ufid *ufid;

	VFS_RECORD(vp->v_vfsp, VS_FID, VS_CALL);

#if	MACH
	ufid = (struct ufid *)kalloc(sizeof (struct ufid));
	bzero((caddr_t)ufid, sizeof (struct ufid));
#else	MACH
	ufid = (struct ufid *)kmem_zalloc(sizeof (struct ufid));
#endif	MACH
	ufid->ufid_len = sizeof(struct ufid) - (sizeof(struct fid) - MAXFIDSZ);
	ufid->ufid_ino = VTOI(vp)->i_number;
	ufid->ufid_gen = VTOI(vp)->i_gen;
	*fidpp = (struct fid *)ufid;
	return (0);
}

static int
ufs_strategy(bp)
	register struct buf *bp;
{
	int resid;

	VFS_RECORD(bp->b_vp->v_vfsp, VFS_STRATEGY, VS_CALL);

	if ((bp->b_flags & B_READ) == B_READ) {
		bp->b_error = rdwri(UIO_READ, VTOI(bp->b_vp), bp->b_un.b_addr,
			(int) bp->b_bcount, (off_t) bp->b_blkno * DEV_BSIZE,
			UIO_SYSSPACE, &resid);
	} else {
		bp->b_error = rdwri(UIO_WRITE, VTOI(bp->b_vp), bp->b_un.b_addr,
			(int)bp->b_bcount, (off_t) bp->b_blkno * DEV_BSIZE,
			UIO_SYSSPACE, &resid);
	}
	bp->b_resid = resid;
	if (bp->b_error) {
		bp->b_flags |= B_ERROR;
	}
	iodone(bp);

}

#if	MACH

pager_return_t
ufs_pagein(vp, m, f_offset)
	struct vnode *vp;
	vm_page_t	m;
	vm_offset_t	f_offset;	/* byte offset within file
	 * block */
{
	register struct inode *ip = VTOI(vp);
	vm_offset_t	p_offset;	/* byte offset within physical page */
	struct vnode	*devvp;
	register struct fs	*fs;
	daddr_t		lbn, bn;
	int		size;
	long		bsize;
	int		csize, on, n, save_error, err;
	u_long		diff;
	struct buf	*bp;
	int		numBytes, error;


	XPR(XPR_FS,("ufs_pagein entered vp=0x%x\n",vp));

	/*
	 *	Get the inode and the offset within it to read from.
	 */
	ILOCK(ip);
	ip->i_flag |= IACC;

	p_offset = 0;

	devvp = ip->i_devvp;
	fs = ip->i_fs;
	bsize = fs->fs_bsize;
	csize = PAGE_SIZE;

	/*
	 * Be sure that data not in the file is zero filled.  The
	 * easiest way to do this is to zero the entire page now.
	 */

	if (ip->i_size < (f_offset + csize)) {
		vm_page_zero_fill(m);
	}

	/*
	 *	Read from the inode until we've filled the page.
	 */
	do {
		/*
		 *	Find block and offset within it for our data.
		 */
		lbn = lblkno(fs, f_offset);	/* logical block number */
		on = blkoff(fs, f_offset);	/* byte offset within block */

		/*
		 *	Find the size we can read - don't go beyond the
		 *	end of a block.
		 */
		n = MIN((unsigned)(bsize - on), csize);
		diff = ip->i_size - f_offset;
		if (ip->i_size <= f_offset) {
			if (p_offset == 0) {
				/*
				 * entire block beyond end of file -
				 * doesn't exist
				 */
				IUNLOCK(ip);
			XPR(XPR_FS,("ufs_pagein exiting ABSENT vp=0x%x\n",vp));
				return(PAGER_ABSENT);
			}
			/*
			 * block partially there - zero the rest of it
			 */
			break;
		}
		if (diff < n)
			n = diff;

		/*
		 *	Read the index to find the disk block to read
		 *	from.  If there is no block, report that we don't
		 *	have this data.
		 *
		 *	!!! Assumes that:
		 *		1) Any offset will be on a fragment boundary
		 *		2) The inode has whole page
		 */
		save_error = u.u_error;
		u.u_error = 0;
		/* changes u.u_error! */
		bn = fsbtodb(fs, bmap(ip, lbn, B_READ, (int)(on+n), 0));
		err = u.u_error;
		u.u_error = save_error;

		if (err) {
			vp->vm_info->error = err;
			printf("IO error on pagein: error = %d.\n",err);
			IUNLOCK(ip);
			XPR(XPR_FS,("ufs_pagein exiting ERROR vp=0x%x\n",vp));
			return(PAGER_ERROR);
		}

		if ((long)bn < 0) {
			IUNLOCK(ip);
			XPR(XPR_FS,("ufs_pagein exiting ABSENT vp=0x%x\n",vp));
			return(PAGER_ABSENT);
		}

		size = blksize(fs, ip, lbn);

		/*
		 *	Read the block through the buffer pool, trying
		 *	to use the physical memory if possible.  The
		 *	only reason that we might have to copy out of
		 * 	the buffer pool is if the block was already
		 *	cached for some reason.
		 *
		 */
		if (size == page_size && p_offset == 0 && on == 0) {
		
			if (ip->i_lastr + 1 != lbn) {	/* should we read ahead? */
				rablock = 0;
				rasize = 0;
			}

			error = 0;
			numBytes = breadDirect(devvp, m, bn, size, n,
						rablock, rasize, &error);
						
			ip->i_lastr = lbn;
						
			if (error != 0) {
			    vp->vm_info->error = error;
			    IUNLOCK(ip);
			    printf("IO error on pagein (breadDirect)\n");
			    XPR(XPR_FS,("ufs_pagein exiting ERROR vp=0x%x\n",vp));
			    return(PAGER_ERROR);
			}
			
			n = MIN(n, numBytes);
			
		} else {

			if ((ip->i_lastr + 1) == lbn)
				bp = breada(devvp, bn, size, rablock, rasize);
			else
				bp = bread(devvp, bn, size);
			ip->i_lastr = lbn;
	
			n = MIN(n, size - bp->b_resid);
			if (bp->b_flags & B_ERROR) {
				vp->vm_info->error = bp->b_error;
				brelse(bp);
				printf("IO error on pagein (bread)\n");
				IUNLOCK(ip);
				XPR(XPR_FS,("ufs_pagein exiting ERROR vp=0x%x\n",vp));
				return(PAGER_ERROR);
			}
	
			copy_to_phys(bp->b_un.b_addr+on,
					VM_PAGE_TO_PHYS(m) + p_offset,
					n);
	
			/* if we read entire block, throw it away now */
	
			if (n == bsize)
				bp->b_flags |= B_NOCACHE;
			brelse(bp);
		}

		/*
		 *	Account for how much we've read this time
		 *	around.
		 */
		csize -= n;
		p_offset += n;
		f_offset += n;

	} while (csize > 0 && n != 0);

	IUNLOCK(ip);
	XPR(XPR_FS,("ufs_pagein exiting vp=0x%x\n",vp));
	return(PAGER_SUCCESS);
}

pager_return_t
ufs_pageout(vp, addr, csize, f_offset)
	struct vnode *vp;
	vm_offset_t addr;
	vm_size_t csize;
	vm_offset_t f_offset;
{
	register struct inode	*ip = VTOI(vp);
	vm_offset_t	p_offset;	/* byte offset within physical page */
	struct vnode	*devvp;
	register struct fs	*fs;
	daddr_t		lbn, bn;
	int		size;
	long		bsize;
	int		on, n, save_error, err;
	struct buf	*bp;

	XPR(XPR_FS,("ufs_pageout entered vp=0x%x\n",vp));

	ILOCK(ip);
	p_offset = 0;
	devvp = ip->i_devvp;
	fs = ip->i_fs;
	bsize = fs->fs_bsize;

	do {
		lbn = lblkno(fs, f_offset);	/* logical block number */
		on = blkoff(fs, f_offset);	/* byte offset within block */

		n = MIN((unsigned)(bsize - on), csize);

		save_error = u.u_error;
		u.u_error = 0;
		/* changes u.u_error! */
		bn = fsbtodb(fs, bmap(ip, lbn, B_WRITE | B_XXX,
				      (int)(on+n), 0));
		err = u.u_error;
		u.u_error = save_error;

		if (err || (long) bn < 0) {
			vp->vm_info->error = err;
			printf("IO error on pageout: error = %d.\n",err);
			IUNLOCK(ip);
			XPR(XPR_FS,("ufs_pageout exiting ERROR vp=0x%x\n",vp));
			return(PAGER_ERROR);
		}

		if (f_offset + n > ip->i_size) {
			ip->i_size = f_offset + n;
			XPR(XPR_VM_OBJECT, ("inode extended to %d bytes\n",
				     ip->i_size));
		}

		size = blksize(fs, ip, lbn);

		if (n == bsize)
			bp = getblk(devvp, bn, size);
		else
			bp = bread(devvp, bn, size);

		n = MIN(n, size - bp->b_resid);
		if (bp->b_flags & B_ERROR) {
			vp->vm_info->error = bp->b_error;
			brelse(bp);
			printf("IO error on pageout (bread)\n");
			IUNLOCK(ip);
			XPR(XPR_FS,("ufs_pageout exiting ERROR vp=0x%x\n",vp));
			return(PAGER_ERROR);
		}

		copy_from_phys(addr + p_offset, bp->b_un.b_addr+on, n);

		csize -= n;
		p_offset += n;
		f_offset += n;

		if (n + on == bsize) {
			bp->b_flags |= B_NOCACHE;
			bawrite(bp);
		}
		else
			bdwrite(bp);
		ip->i_flag |= (IUPD|ICHG);
		IMARK(ip);
	} while (csize != 0 && n != 0);

	IUNLOCK(ip);
	XPR(XPR_FS,("ufs_pageout exiting vp=0x%x\n",vp));
	return(PAGER_SUCCESS);
}

int
ufs_nlinks(vp, l)
    struct vnode	*vp;
    int			*l;
{
    *l = VTOI(vp)->i_nlink;
    return (0);
}
#endif	MACH


