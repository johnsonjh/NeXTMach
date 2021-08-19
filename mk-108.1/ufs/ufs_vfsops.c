/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 *  5-Jun-90  John Seamons (jks) at NeXT
 *	Moved "modify_now" label to detect write-protected roots when they are
 *	remounted.
 *
 * 27-Mar-90  John Seamons (jks) at NeXT
 *	Force u.u_error to zero before bwrite() so any previous error won't mask
 *	a write failure that should cause the disk to be mounted read-only.
 *	Should really figure out why u.u_error is non-zero to begin with.
 *
 * 08-Mar-90  Brian Pinkerton at NeXT
 *	Ifdef'ed out umount.  Nobody calls it.
 *
 * 28-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes
 *	Sun Bugfixes: 1011740 - remount of non-mounted filesystem caused
 *				panic.
 *
 * 24-May-89  John Seamons (jks) at NeXT
 *	In mountfs(), rewrite super block to detect if disk is write protected
 *	and mount read-only if it is.  This supports the write protect switch
 *	on optical disks.
 *
 * 22-Jan-89  John Seamons (jks) at NeXT
 *	Allow a sync to be restricted to a particular device.
 *	This allows a single optical disk volume to be sync'ed
 *	without disturbing other, potentially uninserted, volumes
 *	which will cause needless disk swapping.
 *
 */

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 2.45
 * @(#) Merged with ufs_mountc. 4.3 BSD 7.1 6/5/86
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/buf.h>
#import <sys/pathname.h>
#import <sys/vfs.h>
#import <sys/vfs_stat.h>
#import <sys/vnode.h>
#import <sys/file.h>
#import <sys/uio.h>
#import <sys/conf.h>
#import <ufs/fs.h>
#import <ufs/mount.h>
#import <ufs/inode.h>
#import <sys/mount.h>
#import <sys/reboot.h>
#import <nextdev/voldev.h>

#if	MACH
#import <vm/vm_kern.h>
#import <kern/zalloc.h>
extern struct zone *vfs_zone;
#endif	MACH

static int mountfs();
static int unmount1();
static int getmdev();

/*
 * ufs vfs operations.
 */
static int ufs_mount();
static int ufs_unmount();
static int ufs_root();
static int ufs_statfs();
static int ufs_sync();
static int ufs_vget();
static int ufs_mountroot();
static int ufs_badvfsop();

struct vfsops ufs_vfsops = {
	ufs_mount,
	ufs_unmount,
	ufs_root,
	ufs_statfs,
	ufs_sync,
	ufs_vget,
	ufs_mountroot,
#if	!MACH
	ufs_badvfsop,		/* XXX - swapvp */
#endif	!MACH
};

/*
 * Mount table.
 */
struct mount	*mounttab = NULL;

#if	NeXT
int	rootrw;		/* non-zero means mount root rw initially, else ro */
#endif	NeXT

/*
 * ufs_mount system call
 */
static
ufs_mount(vfsp, path, data)
	struct vfs *vfsp;
	char *path;
	caddr_t data;
{
	int error;
	dev_t dev;
	struct vnode *devvp;
	struct ufs_args args;

	/*
	 * Get arguments
	 */
	error = copyin(data, (caddr_t)&args, sizeof (struct ufs_args));
	if (error) {
		return (error);
	}
	if ((error = getmdev(args.fspec, &dev)) != 0)
		return (error);
	/*
	 * make a special (device) vnode for the filesystem
	 */
	devvp = bdevvp(dev);

	/*
	 * If the device is a tape, mount it read only
	 */
	if ((bdevsw[major(dev)].d_flags & B_TAPE) == B_TAPE)
		vfsp->vfs_flag |= VFS_RDONLY;

	/*
	 * Mount the filesystem.
	 */
	error = mountfs(&devvp, path, vfsp);
	if (error) {
		VN_RELE(devvp);
	}
	return (error);
}

/*
 * Called by vfs_mountroot when ufs is going to be mounted as root
 */
static int
ufs_mountroot(vfsp, vpp, name)
	struct vfs *vfsp;
	struct vnode **vpp;
	char *name;
{
	register struct fs *fsp;
	register int error;
	static int ufsrootdone = 0;
	extern dev_t getblockdev();

	if (ufsrootdone++) {
		return (EBUSY);
	}
	if (rootdev == NODEV) {
		return (ENOENT);
	}

	*vpp = bdevvp(rootdev);
#if	NeXT
	/*
	 * At boot, the root is mounted read-only.  If it fsck's
	 * clean during rc.boot, it's remounted read-write.
	 */
	if (! rootrw)
		vfsp->vfs_flag |= VFS_RDONLY;
#endif	NeXT
	error = mountfs(vpp, "/", vfsp);
	if (error) {
		VN_RELE(*vpp);
		*vpp = (struct vnode *)0;
		return (error);
	}
	error = vfs_add((struct vnode *)0, vfsp,
			(vfsp->vfs_flag & VFS_RDONLY) ? M_RDONLY : 0);
	if (error) {
		(void) unmount1(vfsp, 0);
		VN_RELE(*vpp);
		*vpp = (struct vnode *)0;
		return (error);
	}
	vfs_unlock(vfsp);
	fsp = ((struct mount *)(vfsp->vfs_data))->m_bufp->b_un.b_fs;
	inittodr(fsp->fs_time);
	return (0);
}

static int
mountfs(devvpp, path, vfsp)
	struct vnode **devvpp;
	char *path;
	struct vfs *vfsp;
{
	register struct fs *fsp;
	register struct mount *mp = 0;
	register struct buf *bp = 0;
	struct buf *tp = 0;
	int error;
	int blks;
	caddr_t space;
	int i;
	int size;
	u_int len;
	static int initdone = 0;
	int needclose = 0;

	if (!initdone) {
		ihinit();
		initdone = 1;
	}
	/*
	 * Open block device mounted on.
	 * When bio is fixed for vnodes this can all be vnode operations
	 */
	error = VOP_OPEN(devvpp,
	    (vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE, u.u_cred);
	if (error)
		return (error);
	needclose = 1;
	/*
	 * read in superblock
	 */
	tp = bread(*devvpp, SBLOCK, SBSIZE);
	if (tp->b_flags & B_ERROR) {
		goto out;
	}
	/*
	 * check for dev already mounted on
	 */
	for (mp = mounttab; mp != NULL; mp = mp->m_nxt) {
		if (mp->m_bufp != 0 && (*devvpp)->v_rdev == mp->m_dev) {
			if (vfsp->vfs_flag & VFS_REMOUNT) {
				bp = mp->m_bufp;
				goto modify_now;
			} else {
				mp = 0;
				error = EBUSY;
				needclose = 0;
				goto out;
			}
		}
	}
	vol_notify_cancel((*devvpp)->v_rdev);
	/*
	 * If this is a remount request, and we didn't spot the mounted
	 * device, then we're in error here....
	 */
	if (vfsp->vfs_flag & VFS_REMOUNT) {
		vfsp->vfs_flag &= ~VFS_REMOUNT;
		printf("mountfs: illegal remount request\n");
		error = EINVAL;
		goto out;
	}
	/*
	 * find empty mount table entry
	 */
	for (mp = mounttab; mp != NULL; mp = mp->m_nxt) {
		if (mp->m_bufp == 0)
			goto found;
	}
	/*
	 * get new mount table entry
	 */
#if	MACH
	mp = (struct mount *)kalloc(sizeof (struct mount));
	bzero((caddr_t)mp, sizeof (struct mount));
#else	MACH
	mp = (struct mount *)kmem_zalloc(sizeof (struct mount));
#endif	MACH
	if (mp == 0) {
		error = EMFILE;		/* needs translation */
		goto out;
	}
	mp->m_nxt = mounttab;
	mounttab = mp;
found:
	vfsp->vfs_data = (caddr_t)mp;
	mp->m_vfsp = vfsp;
	mp->m_bufp = tp;	/* just to reserve this slot */
	mp->m_dev = NODEV;
	mp->m_devvp = *devvpp;
	fsp = tp->b_un.b_fs;
	if (fsp->fs_magic != FS_MAGIC || fsp->fs_bsize > MAXBSIZE ||
	    fsp->fs_bsize < sizeof (struct fs)) {
		error = EINVAL;	/* also needs translation */
		goto out;
	}
	/*
	 * Copy the super block into a buffer in it's native size.
	 */
	bp = geteblk((int)fsp->fs_sbsize);
	mp->m_bufp = bp;
	bcopy((caddr_t)tp->b_un.b_addr, (caddr_t)bp->b_un.b_addr,
	   (u_int)fsp->fs_sbsize);
modify_now:
#if	NeXT
	/*
	 * For devices with read-only switch: force mount to be
	 * read-only if device is write-protected and not root.
	 */
	if ((vfsp->vfs_flag & VFS_RDONLY) == 0) {
		u.u_error = 0;		/* XXX */
		bwrite(tp);
		if (u.u_error == EROFS) {
			u.u_error = 0;
			if (*devvpp == rootvp)
				panic("Root device is physically write protected.  It must be writeable.");
			vfsp->vfs_flag |= VFS_RDONLY;
		}
	} else
		brelse(tp);
#else	NeXT
	brelse(tp);
#endif	NeXT
	tp = 0;
	fsp = bp->b_un.b_fs;
	/*
	 * Curently we only allow a remount to change from
	 * read-only to read-write.
	 */
	if (vfsp->vfs_flag & VFS_RDONLY) {
		if (vfsp->vfs_flag & VFS_REMOUNT) {
			printf ("mountfs: can't remount ro\n");
			error = EINVAL;
			goto out;
		}
#if	NeXT
		/*
		 * must clear mod flag if it was somehow left set
		 * on the disk image, else update panics
		 * since it assumes read-only file systems
		 * are never modified.
		 */
		fsp->fs_fmod = 0;
		/* FS state variable does not change */
#endif	NeXT
		fsp->fs_ronly = 1;
	} else {
#if	NeXT
		if (fsp->fs_state == FS_STATE_CLEAN)
			fsp->fs_state = FS_STATE_DIRTY;
		else
			fsp->fs_state = FS_STATE_CORRUPTED;
#endif	NeXT
		fsp->fs_fmod = 1;
		fsp->fs_ronly = 0;
		if (vfsp->vfs_flag & VFS_REMOUNT) {
#if	NeXT
			if (tp)
				brelse(tp);
			vfsp->vfs_flag &= ~VFS_REMOUNT;
			sbupdate(mp);	/* update FS state if necessary */
#else	NeXT
			brelse(tp);
			vfsp->vfs_flag &= ~VFS_REMOUNT;
#endif	NeXT
			return (0);
		}
	}
	vfsp->vfs_bsize = fsp->fs_bsize;
	/*
	 * Read in cyl group info
	 */
	blks = howmany(fsp->fs_cssize, fsp->fs_fsize);
#if	MACH
	space = (caddr_t)kalloc((vm_size_t)fsp->fs_cssize);
#else	MACH
	space = wmemall(vmemall, (int)fsp->fs_cssize);
#endif	MACH
	if (space == 0) {
		error = ENOMEM;
		goto out;
	}
	for (i = 0; i < blks; i += fsp->fs_frag) {
		size = fsp->fs_bsize;
		if (i + fsp->fs_frag > blks)
			size = (blks - i) * fsp->fs_fsize;
		tp = bread(mp->m_devvp, fsbtodb(fsp, fsp->fs_csaddr+i), size);
		if (tp->b_flags & B_ERROR) {
#if	MACH
			kfree(space, (vm_size_t)fsp->fs_cssize);
#else	MACH
			wmemfree(space, (int)fsp->fs_cssize);
#endif	MACH
			goto out;
		}
		bcopy((caddr_t)tp->b_un.b_addr, space, (u_int)size);
		fsp->fs_csp[fragstoblks(fsp, i)] = (struct csum *)space;
		space += size;
		brelse(tp);
		tp = 0;
	}
#if	NeXT
	if (!fsp->fs_ronly)
		sbupdate(mp);	/* update FS state if necessary */
#endif	NeXT
/* BEGIN CS_RPAUSE */
	/*
	 *  Enable first file system/inodes full console messages and calculate
	 *  low water pause/high water resume marks for fragments and inodes.
 	 *
	 *  Fragment water marks:
	 *  lo - (1 or minfree)% of total (but <= 100)
	 *  hi - 2* lo (but <= 200)
	 *
	 *  Inode water marks:
	 *  lo/hi - 1% of total (but <= 50)
	 */
	fsp->fs_flags  &= ~(FS_FNOSPC|FS_INOSPC);
	FS_FLOWAT(fsp) = ((fsp->fs_dsize * fsp->fs_minfree)/100);
	FS_FHIWAT(fsp) = FS_FLOWAT(fsp);
	if (FS_FHIWAT(fsp) > 100)
		FS_FHIWAT(fsp) += 100;
	else
		FS_FHIWAT(fsp) *= 2;
	FS_ILOWAT(fsp) = ((fsp->fs_ncg * fsp->fs_ipg)/100);
	if (FS_ILOWAT(fsp) > 50)
	    FS_ILOWAT(fsp) = 50;
	FS_IHIWAT(fsp) = FS_ILOWAT(fsp);
/* END CS_RPAUSE */
	mp->m_dev = mp->m_devvp->v_rdev;
	vfsp->vfs_fsid.val[0] = (long)mp->m_dev;
	vfsp->vfs_fsid.val[1] = MOUNT_UFS;
	(void) copystr(path, fsp->fs_fsmnt, sizeof (fsp->fs_fsmnt) - 1, &len);
	bzero(fsp->fs_fsmnt + len, sizeof (fsp->fs_fsmnt) - len);
	return (0);
out:
	if (error == 0)
		error = EIO;
	if (mp)
		mp->m_bufp = 0;
	if (bp)
		brelse(bp);
	if (tp)
		brelse(tp);
	if (needclose) {
		(void) VOP_CLOSE(*devvpp, (vfsp->vfs_flag & VFS_RDONLY) ?
		    FREAD : FREAD|FWRITE, 1, u.u_cred);
		binval(*devvpp);
	}
	return (error);
}

#if	!NeXT
/*
 * The System V Interface Definition requires a "umount" operation
 * which takes a device pathname as an argument.  This requires this
 * to be a system call.
 */
umount()
{
	struct a {
		char	*fspec;
	} *uap = (struct a *)u.u_ap;
	register struct mount *mp;
	dev_t dev;

	if (!suser())
		return;

	if ((u.u_error = getmdev(uap->fspec, &dev)) != 0)
		return;

	if ((mp = getmp(dev)) == NULL) {
		u.u_error = EINVAL;
		return;
	}

#if	MACH
#if	MACH_NBC
	mfs_cache_clear();		/* clear the MFS cache */
#endif	MACH_NBC
	vm_object_cache_clear();	/* clear the object cache */
#endif	MACH

	u.u_error = dounmount(mp->m_vfsp);
}
#endif	!NeXT


/*
 * vfs operations
 */
static int
ufs_unmount(vfsp)
	struct vfs *vfsp;
{

	return (unmount1(vfsp, 0));
}

static int
unmount1(vfsp, forcibly)
	register struct vfs *vfsp;
	int forcibly;
{
	dev_t dev;
	register struct mount *mp;
	register struct fs *fs;
	register int stillopen;
	int flag;

	mp = (struct mount *)vfsp->vfs_data;
	dev = mp->m_dev;
#ifdef QUOTA
	if ((stillopen = iflush(dev, mp->m_qinod)) < 0 && !forcibly)
#else
	if ((stillopen = iflush(dev)) < 0 && !forcibly)
#endif
		return (EBUSY);
	if (stillopen < 0)
		return (EBUSY);			/* XXX */
#ifdef QUOTA
	(void) closedq(mp);
	/*
	 * Here we have to iflush again to get rid of the quota inode.
	 * A drag, but it would be ugly to cheat, & this doesn't happen often
	 */
	(void) iflush(dev, (struct inode *)NULL);
#endif
	fs = mp->m_bufp->b_un.b_fs;
#if	MACH
#else	MACH
	wmemfree((caddr_t)fs->fs_csp[0], (int)fs->fs_cssize);
#endif	MACH
	flag = !fs->fs_ronly;
#if	NeXT
	if (flag) {
		if (fs->fs_state == FS_STATE_DIRTY) {
			fs->fs_state = FS_STATE_CLEAN;
			sbupdate(mp);
		}
	}
#endif	NeXT
#if	MACH
	/* after sbupdate above */
	kfree((caddr_t)fs->fs_csp[0], (vm_size_t)fs->fs_cssize);
#endif	MACH
	brelse(mp->m_bufp);
	mp->m_bufp = 0;
	mp->m_dev = 0;
	if (!stillopen) {
		register struct mount *mmp;

		(void) VOP_CLOSE(mp->m_devvp, flag, 1, u.u_cred);
		binval(mp->m_devvp);
		VN_RELE(mp->m_devvp);
		mp->m_devvp = (struct vnode *)0;
		if (mp == mounttab) {
			mounttab = mp->m_nxt;
		} else {
			for (mmp = mounttab; mmp != NULL; mmp = mmp->m_nxt) {
				if (mmp->m_nxt == mp) {
					mmp->m_nxt = mp->m_nxt;
				}
			}
		}
#if	MACH
		kfree((caddr_t)mp, sizeof (struct mount));
#else	MACH
		kmem_free((caddr_t)mp, sizeof (struct mount));
#endif	MACH
	}
	return (0);
}

/*
 * find root of ufs
 */
static int
ufs_root(vfsp, vpp)
	struct vfs *vfsp;
	struct vnode **vpp;
{
	register struct mount *mp;
	struct inode *ip;

	VFS_RECORD(vfsp, VS_ROOT, VS_CALL);

	mp = (struct mount *)vfsp->vfs_data;
	ip = iget(mp->m_dev, mp->m_bufp->b_un.b_fs, (ino_t)ROOTINO);
	if (ip == (struct inode *)0) {
		return (u.u_error);
	}
	IUNLOCK(ip);
	*vpp = ITOV(ip);
	return (0);
}

/*
 * Get file system statistics.
 */
static int
ufs_statfs(vfsp, sbp)
	register struct vfs *vfsp;
	struct statfs *sbp;
{
	register struct fs *fsp;

	VFS_RECORD(vfsp, VS_STATFS, VS_CALL);

	fsp = ((struct mount *)vfsp->vfs_data)->m_bufp->b_un.b_fs;
	if (fsp->fs_magic != FS_MAGIC)
		panic("ufs_statfs");
	sbp->f_bsize = fsp->fs_fsize;
	sbp->f_blocks = fsp->fs_dsize;
	sbp->f_bfree = fsp->fs_cstotal.cs_nbfree * fsp->fs_frag +
	    fsp->fs_cstotal.cs_nffree;
	/*
	 * avail = MAX(max_avail - used, 0)
	 */
	sbp->f_bavail = (fsp->fs_dsize * (100 - fsp->fs_minfree) / 100) -
	    (fsp->fs_dsize - sbp->f_bfree);
	/*
	 * inodes
	 */
	sbp->f_files =  fsp->fs_ncg * fsp->fs_ipg;
	sbp->f_ffree = fsp->fs_cstotal.cs_nifree;
#if	NeXT
	bcopy((caddr_t)&vfsp->vfs_fsid, (caddr_t)&sbp->f_fsid, 
			sizeof (fsid_t));
#else
/* FIXME : fs_id is not used yet
	bcopy((caddr_t)fsp->fs_id, (caddr_t)&sbp->f_fsid, sizeof (fsid_t));
*/
#endif	NeXT
	return (0);
}

/*
 * Flush any pending I/O to file system vfsp.
 * The update() routine will only flush *all* ufs files.
 */
/*ARGSUSED*/
static int
ufs_sync(vfsp)
	struct vfs *vfsp;
{

#if	NeXT
	update (NODEV, NODEV);
#else	NeXT
	update();
#endif	NeXT
	return (0);
}

sbupdate(mp)
	struct mount *mp;
{
	register struct fs *fs = mp->m_bufp->b_un.b_fs;
	register struct buf *bp;
	int blks;
	caddr_t space;
	int i, size;

	bp = getblk(mp->m_devvp, SBLOCK, (int)fs->fs_sbsize);
	bcopy((caddr_t)fs, bp->b_un.b_addr, (u_int)fs->fs_sbsize);
/* BEGIN CS_RPAUSE */
	/*
	 *  These fields are supposed to be zero in 4.2 super-blocks and are
	 *  currently maintained only internally so we can just zero them here
	 *  (in the outgoing copy) for now.  Perhaps someday it will make sense
	 *  to record some of them on disk...
	 */
	bp->b_un.b_fs->fs_flowat = 0;
	bp->b_un.b_fs->fs_fhiwat = 0;
	bp->b_un.b_fs->fs_ilowat = 0;
	bp->b_un.b_fs->fs_ihiwat = 0;
	bp->b_un.b_fs->fs_flags = 0;
/* END CS_RPAUSE */
	bwrite(bp);
	blks = howmany(fs->fs_cssize, fs->fs_fsize);
	space = (caddr_t)fs->fs_csp[0];
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		bp = getblk(mp->m_devvp, (daddr_t)fsbtodb(fs, fs->fs_csaddr+i),
		    size);
		bcopy(space, bp->b_un.b_addr, (u_int)size);
		space += size;
		bwrite(bp);
	}
}

/*
 * Common code for mount and umount.
 * Check that the user's argument is a reasonable
 * thing on which to mount, and return the device number if so.
 */
static int
getmdev(fspec, pdev)
	char *fspec;
	dev_t *pdev;
{
	register int error;
	struct vnode *vp;

	/*
	 * Get the device to be mounted
	 */
	error = lookupname(fspec, UIO_USERSPACE, FOLLOW_LINK,
	    (struct vnode **)0, &vp);
	if (error) {
		if (u.u_error == ENOENT)
			return (ENODEV);	/* needs translation */
		return (error);
	}
	if (vp->v_type != VBLK) {
		VN_RELE(vp);
		return (ENOTBLK);
	}
	*pdev = vp->v_rdev;
	VN_RELE(vp);
	if (major(*pdev) >= nblkdev)
		return (ENXIO);
	return (0);
}

static int
ufs_vget(vfsp, vpp, fidp)
	struct vfs *vfsp;
	struct vnode **vpp;
	struct fid *fidp;
{
	register struct ufid *ufid;
	register struct inode *ip;
	register struct mount *mp;

	mp = (struct mount *)vfsp->vfs_data;
	ufid = (struct ufid *)fidp;
	ip = iget(mp->m_dev, mp->m_bufp->b_un.b_fs, ufid->ufid_ino);
	if (ip == NULL) {
		*vpp = NULL;
		return (0);
	}
	if (ip->i_gen != ufid->ufid_gen) {
		idrop(ip);
		*vpp = NULL;
		return (0);
	}
	IUNLOCK(ip);
	*vpp = ITOV(ip);
	if ((ip->i_mode & ISVTX) && !(ip->i_mode & (IEXEC | IFDIR))) {
		(*vpp)->v_flag |= VISSWAP;
	}
	return (0);
}

static int
ufs_badvfsop()
{

	return (EINVAL);
}

