/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 * 08-Mar-90  Brian Pinkerton at NeXT
 *	Removed redundant call to vm_object_cache_clear in doumount()
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	Sun Bugfixes: 1012835 - Fix panic when trying to mount an fs-type
 *				by number that is not configured into the
 *				kernel.
 *	NFS 4.0 Changes:  Removed dir.h
 *
 * 07-Jan-88  Avadis Tevanian (avie) at NeXT
 *	Make dounmount return error explicitly instead of in u.u_error.
 *
 * 23-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Flush mfs and object cache in unmount.
 *
 * 25-Aug-87  Peter King (king) at NeXT
 *	Ported from NFS distribution.
 */ 

/*	@(#)vfs.c	2.4 88/08/05 4.0NFSSRC SMI;  from SMI 2.26 87/01/27	*/

#import <mach_nbc.h>

#import <sys/param.h>
#import <sys/user.h>
#import <sys/uio.h>
#import <sys/file.h>
#import <sys/vfs.h>
#import <sys/socket.h>
#import <sys/systm.h>
#import <sys/mount.h>
#import <sys/pathname.h>
#import <sys/vnode.h>
#import <ufs/inode.h>
#import <sys/vfs_stat.h>
#import <sys/bootconf.h>

#import <kern/zalloc.h>
#import <vm/vm_param.h>

/*
 * vfs global data
 */
struct vnode *rootdir;			/* pointer to root vnode */

struct vfs *rootvfs;			/* pointer to root vfs. This is */
					/* also the head of the vfs list */
#if	NeXT
char	rootname[32];
#endif	NeXT
/*
 * Convert mode formats to vnode types
 */
enum vtype mftovt_tab[] = {
	VFIFO, VCHR, VDIR, VBLK, VREG, VLNK, VSOCK, VBAD
};

/*
 * System calls
 */

/*
 * Translate mount type numbers from old order to new order for backward
 * compatability with old mount command
 */
static int fixtype[] =
	{ MOUNT_UFS, MOUNT_NFS, MOUNT_PC, MOUNT_UFS, MOUNT_CFS};

/*
 * mount system call
 */
smount()
{
	register struct a {
		char	*type;
		char	*dir;
		int	flags;
		caddr_t	data;
	} *uap = (struct a *)u.u_ap;
	struct pathname pn;
	struct vnode *vp;
#if	NeXT
	struct vnode	*pvp;	/* parent vnode */
	int didlock;
#endif	NeXT
	struct vfs *vfsp;
	struct vfssw *vs;
	extern struct vnodeops ufs_vnodeops;
	int saved_flag;

#if	NeXT
	/*
	 * Get vnode to be covered
	 *
	 * If this is a remount, can't ask for parent since we can
	 * be remounting the root and the root doesn't have a parent
	 * (and that causes an error and we don't get vp back...)
	 */
	pvp = (struct vnode *)0;
	u.u_error = lookupname(uap->dir, UIO_USERSPACE, FOLLOW_LINK,
	    (uap->flags & M_REMOUNT) ? (struct vnode **)0 : &pvp, &vp);
	if (u.u_error)
		return;

	if (vp) {
		if (pvp)
			VN_RELE(pvp);
		if (vp->v_vfsp->vfs_flag & VFS_NOSUB) {
			VN_RELE(vp);
			u.u_error = EINVAL;
			return;
		}
		dnlc_purge();
		/*
		 * This test is sort of bogus as you can cover up a
		 * directory by mounting at the next higher level spot
		 * in the directory tree.  If we want to keep this
		 * somewhat bogus test, then we will need to add
		 * a call to a routine which gets rid of any current
		 * pages / segmap mappings for this vnode.  This
		 * routine will need to exist for unmount anyway.
		 */
		if (vp->v_count != 1 && !(uap->flags & M_REMOUNT)) {
			VN_RELE(vp);
			u.u_error = EBUSY;
			return;
		}
		if (vp->v_type != VDIR) {
			VN_RELE(vp);
			u.u_error = ENOTDIR;
			return;
		}
		/*
		 * Check that this vnode is not the root of a mounted
		 * filesystem
		 */
		if ((vp->v_flag & VROOT) && !(uap->flags & M_REMOUNT)) {
			VN_RELE(vp);
			u.u_error = EBUSY;
			return;
		}
	} else {
		/*
		 * Technically, this can't happen.  Lookupname will return
		 * with an error if no vp and pvp is NULL, but just to
		 * be paranoid....
		 */
		if (uap->flags & M_REMOUNT) {
			if (pvp)	/* really paranoid */
				VN_RELE(pvp);
			u.u_error = ENOENT;
			return;
		}
		if (pvp->v_vfsp->vfs_flag & VFS_NOSUB) {
			VN_RELE(pvp);
			u.u_error = EINVAL;
			return;
		}
		uap->flags |= M_VIRTUAL;
		vp = pvp;
		pvp = NULL;
	}
#else	NeXT
	/*
	 * Must be super user
	 */
	if (!suser())
		return;
	/*
	 * Get vnode to be covered
	 */
	u.u_error = lookupname(uap->dir, UIO_USERSPACE, FOLLOW_LINK,
			       (struct vnode **)0, &vp);
	if (u.u_error)
		return;

	if (vp->v_vfsp->vfs_flag & VFS_NOSUB) {
		VN_RELE(vp);
		u.u_error = EINVAL;
		return;
	}
	dnlc_purge();
#ifdef XXX
	/*
	 * This test is sort of bogus as you can cover up a
	 * directory by mounting at the next higher level spot
	 * in the directory tree.  If we want to keep this
	 * somewhat bogus test, then we will need to add
	 * a call to a routine which gets rid of any current
	 * pages / segmap mappings for this vnode.  This
	 * routine will need to exist for unmount anyway.
	 */
	if (vp->v_count != 1) {
		VN_RELE(vp);
		u.u_error = EBUSY;
		return;
	}
#endif	XXX
	if (vp->v_type != VDIR) {
		VN_RELE(vp);
		u.u_error = ENOTDIR;
		return;
	}
#endif	NeXT

	if ((uap->flags & M_NEWTYPE) || (uap->flags & M_REMOUNT)) {
		u.u_error = pn_get(uap->type, UIO_USERSPACE, &pn);
		if (u.u_error) {
			VN_RELE(vp);
			return;
		}
		for (vs = vfssw; vs < vfsNVFS; vs++) {
			if (vs->vsw_name != 0 &&
			    strcmp(pn.pn_path, vs->vsw_name) == 0) {
				break;
			}
		}
		if (vs == vfsNVFS) {
			u.u_error = ENODEV;
			VN_RELE(vp);
			pn_free(&pn);
			return;
		}
		pn_free(&pn);
	} else if ((int)uap->type >= sizeof(fixtype)/sizeof(int) ||
	    (vs = &vfssw[fixtype[(int)uap->type]])->vsw_ops == 0) {
		u.u_error = ENODEV;
		VN_RELE(vp);
		return;
	}
#if	NeXT
	didlock = 0;
#endif	NeXT
	/*
	 * If this is a remount, we don't want to create a new VFS.
	 * Instead, we pass the existing one with a remount flag.
	 */
	if (uap->flags & M_REMOUNT) {
		for (vfsp = rootvfs;
		    vfsp != (struct vfs *)0; vfsp = vfsp->vfs_next) {
			if ((vp->v_vfsp == vfsp) && (vp->v_flag & VROOT) &&
			    (vp->v_vfsmountedhere == (struct vfs *)0))
				break;
		}
		if (vfsp == (struct vfs *)0) {
			u.u_error = ENOENT;
			VN_RELE(vp);
			return;
		}
		u.u_error = vfs_lock(vfsp);
		if (u.u_error) {
			VN_RELE(vp);
			return;
		}
		u.u_error = pn_get(uap->dir, UIO_USERSPACE, &pn);
		if (u.u_error) {
			VN_RELE(vp);
			vfs_unlock(vfsp);
			return;
		}
		/*
		 * Disallow making file systems read-only.
		 * Ignore other flags.
		 */
		if (uap->flags & M_RDONLY) {
			printf ("mount: can't remount ro\n");
			u.u_error = EINVAL;
			vfs_unlock(vfsp);
			VN_RELE(vp);
			pn_free(&pn);
			return;
		}
		saved_flag = vfsp->vfs_flag;
		vfsp->vfs_flag |= VFS_REMOUNT;
		vfsp->vfs_flag &= ~VFS_RDONLY;
#if	NeXT
		/*
		 * Since the mount has already occurred, this would
		 * lock the root of the mounted file system (rather
		 * than the covered inode); we don't care what happens
		 * to the root inode.  Since we hold the vfs lock,
		 * we don't have to worry about fs stuff getting
		 * smashed while we read in the superblock, etc.
		 */
#else	NeXT
		if (vp->v_op == &ufs_vnodeops)
			ILOCK(VTOI(vp));
#endif	NeXT
	} else {
		u.u_error = pn_get(uap->dir, UIO_USERSPACE, &pn);
		if (u.u_error) {
			VN_RELE(vp);
			return;
		}
		vfsp = (struct vfs *)kalloc(sizeof (struct vfs));
		VFS_INIT(vfsp, vs->vsw_ops, (caddr_t)0);
#if	NeXT
		if (uap->flags & M_VIRTUAL) {
			char		*cp;
			char		*ncp;
			extern char	*index();

			/*
			 * Save away final component of the path.
			 */
			cp = ncp = pn.pn_path;
			while(ncp = index(ncp, '/')) {
				cp = ++ncp;
			}
			strncpy(vfsp->vfs_name, cp, MAXNAMLEN);
			/*
			 * Don't lock inodes on virtual mounts.
			 * You don't have to: vfs_add doesn't sleep,
			 * and if lookuppn sleeps while searching the
			 * virtual list, it researches when it wakes-up.
			 * Besides, if you did, it would deadlock if
			 * the covered vnode and the dev share a common
			 * parent (since the parent directory is locked
			 * and needs to be searched to find the dev).
			 */
		} else {
			/*
			 * Mount the filesystem.
			 * Lock covered vnode (XXX this currently only works if
			 * it is type ufs)
			 */
			if (vp->v_op == &ufs_vnodeops) {
				ILOCK(VTOI(vp));
				didlock = 1;
			}
		}
		/*
		 * All user level mounts are NOSUID
		 */
		if (vfsp->vfs_uid != 0) {
			uap->flags |= M_NOSUID;
		}
#else	NeXT
		/*
		 * Mount the filesystem.
		 * Lock covered vnode (XXX this currently only works if
		 * it is type ufs)
		 */
		if (vp->v_op == &ufs_vnodeops)
			ILOCK(VTOI(vp));
#endif	NeXT
		u.u_error = vfs_add(vp, vfsp, uap->flags);
	}
	if (!u.u_error) {
		u.u_error = VFS_MOUNT(vfsp, pn.pn_path, uap->data);
	}
#if	NeXT
	if (didlock)
		IUNLOCK(VTOI(vp));
#else	NeXT
	if (vp->v_op == &ufs_vnodeops)
		IUNLOCK(VTOI(vp));
#endif	NeXT
	pn_free(&pn);
	if (!u.u_error) {
		vfs_unlock(vfsp);
		if (uap->flags & M_REMOUNT) {
			vfsp->vfs_flag &= ~VFS_REMOUNT;
			VN_RELE(vp);
		}
#ifdef VFSSTATS
		else {
			struct vfsstats *vs;

			vs = (struct vfsstats *)
			    kalloc(sizeof (struct vfsstats));
			bzero((caddr_t)vs, sizeof (struct vfsstats));
			vs->vs_time = time.tv_sec;
			vfsp->vfs_stats = (caddr_t)vs;
		}
#endif
	} else {
		/*
		 * There was an error, return filesystem to previous
		 * state if remount, otherwise clean up vfsp
		 */
		if (uap->flags & M_REMOUNT) {
			vfsp->vfs_flag = saved_flag;
			vfs_unlock(vfsp);
		} else {
			vfs_remove(vfsp);
			kfree(vfsp, sizeof (struct vfs));
		}
		VN_RELE(vp);
	}
}


/*
 * Sync system call - sync all vfs types
 */
sync()
{
	register struct vfssw *vfsswp;

#if	MACH_NBC
	mfs_sync();
#endif	MACH_NBC
	for (vfsswp = vfssw; vfsswp < vfsNVFS; vfsswp++) {
		if (vfsswp->vsw_ops == (struct vfsops *)NULL)
			continue;		/* not configured in */
		/*
		 * Call the vfs sync operation with a NULL
		 * pointer to force all vfs's of the given
		 * type to be flushed back to main storage.
		 */
		(*vfsswp->vsw_ops->vfs_sync)((struct vfs *)NULL);
	}
}

/*
 * get filesystem statistics
 */
statfs()
{
	struct a {
		char *path;
		struct statfs *buf;
	} *uap = (struct a *)u.u_ap;
	struct vnode *vp;

	u.u_error = lookupname(uap->path, UIO_USERSPACE, FOLLOW_LINK,
	    (struct vnode **)0, &vp);
	if (u.u_error)
		return;
	cstatfs(vp->v_vfsp, uap->buf);
	VN_RELE(vp);
}

fstatfs()
{
	struct a {
		int fd;
		struct statfs *buf;
	} *uap = (struct a *)u.u_ap;
	struct file *fp;

	u.u_error = getvnodefp(uap->fd, &fp);
	if (u.u_error == 0)
		cstatfs(((struct vnode *)fp->f_data)->v_vfsp, uap->buf);
}

cstatfs(vfsp, ubuf)
	struct vfs *vfsp;
	struct statfs *ubuf;
{
	struct statfs sb;

	bzero((caddr_t)&sb, sizeof (sb));
	u.u_error = VFS_STATFS(vfsp, &sb);
	if (u.u_error)
		return;
	u.u_error = copyout((caddr_t)&sb, (caddr_t)ubuf, sizeof (sb));
}

/*
 * Unmount system call.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
unmount()
{
	struct a {
		char	*pathp;
	} *uap = (struct a *)u.u_ap;
	struct vnode *fsrootvp;
	register struct vfs *vfsp;

#if	NeXT
#else	NeXT
	if (!suser())
		return;
#endif	NeXT
	/*
	 * lookup root of fs
	 */
	u.u_error = lookupname(uap->pathp, UIO_USERSPACE, FOLLOW_LINK,
	    (struct vnode **)0, &fsrootvp);
	if (u.u_error)
		return;
	/*
	 * make sure this is a root
	 */
	if ((fsrootvp->v_flag & VROOT) == 0) {
		u.u_error = EINVAL;
		VN_RELE(fsrootvp);
		return;
	}
	/*
	 * get vfs
	 */
	vfsp = fsrootvp->v_vfsp;
	VN_RELE(fsrootvp);
#if	NeXT
	/*
	 * Only do the unmount if this is the user that mounted the
	 * filesystem or the super user.
	 */
	if ((u.u_uid != vfsp->vfs_uid) && !suser()) {
		u.u_error = EPERM;
		return;
	}
#endif	NeXT
	/*
	 * Do the unmount.
	 */
#if	MACH
#if	MACH_NBC
	mfs_cache_clear();		/* clear the MFS cache */
#endif	MACH_NBC
	vm_object_cache_clear();	/* clear the object cache */
#endif	MACH
	u.u_error = dounmount(vfsp);
}

/*
 * XXX Subroutine so the old 4.2/S5-style
 * "umount" call can use this code as well.
 */
dounmount(vfsp)
	register struct vfs *vfsp;
{
	register struct vnode *coveredvp;
	int	error;

	/*
	 * Get covered vnode.
	 */
	coveredvp = vfsp->vfs_vnodecovered;
	/*
	 * lock vnode to maintain fs status quo during unmount
	 */
	error = vfs_lock(vfsp);
	if (error)
#if	DEBUG
		if (vfsp == rootvfs)
			panic("can't get root lock");
#else	DEBUG
		return(error);
#endif	DEBUG

	dnlc_purge();	/* remove dnlc entries for this file sys */
	VFS_SYNC(vfsp);

	error = VFS_UNMOUNT(vfsp);
	if (error) {
#if	DEBUG
		if (vfsp == rootvfs)
			panic("can't unmount root");
#endif	DEBUG
		vfs_unlock(vfsp);
	} else {
#if	NeXT
		if (coveredvp) {	/* root has no covered vp
					   and is not removed from list */
#endif	NeXT
		VN_RELE(coveredvp);
		vfs_remove(vfsp);
#if	NeXT
		}
#endif	NeXT
#ifdef VFSSTATS
		if (vfsp->vfs_stats) {
			kfree((caddr_t)vfsp->vfs_stats,
			    sizeof (struct vfsstats));
		}
#endif
		kfree(vfsp, sizeof (struct vfs));
	}
	return(error);
}

/*
 * External routines
 */

/*
 * vfssw_lookup is used to locate the vfssw of a particular type.
 */
struct vfssw *
vfssw_lookup(type)
	char *type;
{
	struct vfssw *vsw;

	for (vsw = vfssw; vsw < vfsNVFS; vsw++) {
		if (strcmp(type, vsw->vsw_name) == 0)
			return (vsw);
	}
	return (NULL);
}

/*
 * vfs_mountroot is called by main (init_main.c) to
 * mount the root filesystem.
 */
void
vfs_mountroot()
{
	register int error;
	struct vfssw *vsw;
	extern char *strcpy();

#if	MACH
	rootvfs = (struct vfs *)kalloc(sizeof (struct vfs));
	vsw = vfssw_lookup(rootfs.bo_fstype);
#else	MACH
	rootvfs = (struct vfs *)kmem_alloc(sizeof (struct vfs));
	vsw = &vfssw[1];
	strcpy(rootfs.bo_fstype,"4.3");
#endif	MACH
	if (vsw) {
		VFS_INIT(rootvfs, vsw->vsw_ops, (caddr_t)0);
		error = VFS_MOUNTROOT(rootvfs, &rootvp, rootfs.bo_name);
	} else {
		/*
		 * Step through the filesystem types til we find one that
		 * will mount a root filesystem.  If error panic.
		 */
		for (vsw = vfssw; vsw < vfsNVFS; vsw++) {
			if (vsw->vsw_ops) {
				VFS_INIT(rootvfs, vsw->vsw_ops, (caddr_t)0);
				error = VFS_MOUNTROOT(rootvfs, &rootvp,
				    rootfs.bo_name);
				if (!error) {
					break;
				}
			}
		}
	}
	if (error) {
		panic("vfs_mountroot: cannot mount root");
	}
#ifdef VFSSTATS
	{
		struct vfsstats *vs;

#if	MACH
		vs = (struct vfsstats *)kalloc(sizeof (struct vfsstats));
		bzero(vs, sizeof (struct vfsstats));
#else	MACH
		vs = (struct vfsstats *)kmem_alloc(sizeof (struct vfsstats));
#endif	MACH
		vs->vs_time = time.tv_sec;
		rootvfs->vfs_stats = (caddr_t)vs;
	}
#endif
	/*
	 * Get vnode for '/'.
	 * Setup rootdir, u.u_rdir and u.u_cdir to point to it.
	 * These are used by lookuppn so that it knows where
	 * to start from '/' or '.'.
	 */
	error = VFS_ROOT(rootvfs, &rootdir);
	if (error)
		panic("vfs_mountroot: cannot find root vnode");
	u.u_cdir = rootdir;
	VN_HOLD(u.u_cdir);
	u.u_rdir = NULL;
#if	NeXT
	if (rootname[0]) {
		struct vnode *newroot;

		if (!lookupname(rootname, UIO_SYSSPACE, 1, 0, &newroot)) {
			rootdir = newroot;
			VN_RELE(u.u_cdir);	/* from VN_HOLD */
			VN_RELE(u.u_cdir);	/* from VFS_ROOT */
			u.u_cdir = rootdir;
			VN_HOLD(rootdir);
		}
	}
#endif	NeXT
	rootfs.bo_vp = rootvp;
	(void) strcpy(rootfs.bo_fstype, vsw->vsw_name);
	rootfs.bo_size = 0;
	rootfs.bo_flags = BO_VALID;
}

/*
 * vfs_add is called by a specific filesystem's mount routine to add
 * the new vfs into the vfs list and to cover the mounted on vnode.
 * The vfs is also locked so that lookuppn will not venture into the
 * covered vnodes subtree. coveredvp is zero if this is the root.
#if	NeXT
 * mflag will have M_VIRTUAL set if we are supposed to add this to the
 * coveredvp's vfsentries list rather than mounting on top of it.
#endif	NeXT
 */
int
vfs_add(coveredvp, vfsp, mflag)
	register struct vnode *coveredvp;
	register struct vfs *vfsp;
	int mflag;
{
	register int error;
	struct vfs *evfsp;

	error = vfs_lock(vfsp);
	if (error)
		return (error);
	if (coveredvp != (struct vnode *)0) {
		/*
		 * Return EBUSY if the covered vp is already mounted on.
		 */
		if (coveredvp->v_vfsmountedhere != (struct vfs *)0) {
			vfs_unlock(vfsp);
			return (EBUSY);
		}
#if	NeXT
		if (mflag & M_VIRTUAL) {
			/*
			 * Add this vfs to the parent's fake entry
			 * list so lookuppn (vfs_lookup.c) can work
			 * its way into the new filesystem.
			 */
			vfsp->vfs_nextentry = coveredvp->v_vfsentries;
			coveredvp->v_vfsentries = vfsp;
			microtime(&coveredvp->v_vfstime);
		} else {
			coveredvp->v_vfsmountedhere = vfsp;
		}
#endif	NeXT
		/*
		 * Put the new vfs on the vfs list after root.
		 * Point the covered vnode at the new vfs so lookuppn
		 * (vfs_lookup.c) can work its way into the new file system.
		 */
		vfsp->vfs_next = rootvfs->vfs_next;
		rootvfs->vfs_next = vfsp;
#if	NeXT
#else	NeXT
		coveredvp->v_vfsmountedhere = vfsp;
#endif	NeXT
	} else {
		/*
		 * This is the root of the whole world.
		 */
		rootvfs = vfsp;
		vfsp->vfs_next = (struct vfs *)0;
	}
	vfsp->vfs_vnodecovered = coveredvp;
	if (mflag & M_RDONLY) {
		vfsp->vfs_flag |= VFS_RDONLY;
	} else {
		vfsp->vfs_flag &= ~VFS_RDONLY;
	}
	if (mflag & M_NOSUID) {
		vfsp->vfs_flag |= VFS_NOSUID;
	} else {
		vfsp->vfs_flag &= ~VFS_NOSUID;
	}
#if	NeXT && VFS_NOSUID != VFS_NODEV
	if (mflag & M_NODEV) {
		vfsp->vfs_flag |= VFS_NODEV;
	} else {
		vfsp->vfs_flag &= ~VFS_NODEV;
	}
#endif	NeXT && VFS_NOSUID != VFS_NODEV
	if (mflag & M_GRPID) {
		vfsp->vfs_flag |= VFS_GRPID;
	} else {
		vfsp->vfs_flag &= ~VFS_GRPID;
	}
	if (mflag & M_NOSUB) {
		vfsp->vfs_flag |= VFS_NOSUB;
	} else {
		vfsp->vfs_flag &= ~VFS_NOSUB;
	}
	vfsp->vfs_flag &= ~VFS_MULTI;
	return (0);
}

/*
 * Remove a vfs from the vfs list, and destory pointers to it.
 * Should be called by filesystem implementation after it determines
 * that an unmount is legal but before it destroys the vfs.
 */
void
vfs_remove(vfsp)
register struct vfs *vfsp;
{
	register struct vfs *tvfsp;
	register struct vnode *vp;
#if	NeXT
	register struct vfs **vfspp;
#endif	NeXT

	/*
	 * can't unmount root. Should never happen, because fs will be busy.
	 */
	if (vfsp == rootvfs)
		panic("vfs_remove: unmounting root");
	for (tvfsp = rootvfs;
	    tvfsp != (struct vfs *)0; tvfsp = tvfsp->vfs_next) {
		if (tvfsp->vfs_next == vfsp) {
			/*
			 * remove vfs from list, unmount covered vp.
			 */
			tvfsp->vfs_next = vfsp->vfs_next;
			vp = vfsp->vfs_vnodecovered;
#if	NeXT
			/*
			 * If we don't have something mounted here, then
			 * it must be a fake entry.
			 */
			if (vp->v_vfsmountedhere == (struct vfs *)0) {
				for (vfspp = &vp->v_vfsentries;
				     *vfspp;
				     vfspp = &((*vfspp)->vfs_nextentry)) {
					if (*vfspp == vfsp)
						break;
				}
				if (*vfspp != vfsp) {
					panic("vfs_remove: can't find vfs in entries");
				}
				*vfspp = vfsp->vfs_nextentry;
				microtime(&vp->v_vfstime);
			} else
#endif	NeXT
			vp->v_vfsmountedhere = (struct vfs *)0;
			/*
			 * release lock and wakeup anybody waiting
			 */
			vfs_unlock(vfsp);
			return;
		}
	}
	/*
	 * can't find vfs to remove
	 */
	panic("vfs_remove: vfs not found");
}

/*
 * Lock a filesystem to prevent access to it while mounting and unmounting.
 * Returns error if already locked.
 * XXX This totally inadequate for unmount right now - srk
 */
int
vfs_lock(vfsp)
	register struct vfs *vfsp;
{

	if (vfsp->vfs_flag & VFS_MLOCK)
		return (EBUSY);
	vfsp->vfs_flag |= VFS_MLOCK;
	return (0);
}

/*
 * Unlock a locked filesystem.
 * Panics if not locked
 */
void
vfs_unlock(vfsp)
	register struct vfs *vfsp;
{

	if ((vfsp->vfs_flag & VFS_MLOCK) == 0)
		panic("vfs_unlock");
	vfsp->vfs_flag &= ~VFS_MLOCK;
	/*
	 * Wake anybody waiting for the lock to clear
	 */
	if (vfsp->vfs_flag & VFS_MWAIT) {
		vfsp->vfs_flag &= ~VFS_MWAIT;
		wakeup((caddr_t)vfsp);
	}
}

struct vfs *
getvfs(fsid)
	fsid_t *fsid;
{
	register struct vfs *vfsp;

	for (vfsp = rootvfs; vfsp; vfsp = vfsp->vfs_next) {
		if (vfsp->vfs_fsid.val[0] == fsid->val[0] &&
		    vfsp->vfs_fsid.val[1] == fsid->val[1]) {
			break;
		}
	}
	return (vfsp);
}

/*
 * Take a file system ID of the sort that appears in a "struct vattr"
 * and find the VFS that has that file system ID.  This is done by
 * finding the root directory for each VFS, finding its attributes,
 * and comparing its file system ID with the given file system ID;
 * if we have a match, we've found the right VFS.  It's slow, but it
 * works, and it's only used for the System V "ustat" call which is
 * a crock anyway ("statfs" is better).
 */
int
vafsidtovfs(vafsid, vfspp)
	long vafsid;
	struct vfs **vfspp;
{
	register struct vfs *vfsp;
	struct vnode *rootvn;		/* pointer to root vnode of vfs */
	struct vattr vattr;
	register int error;

	for (vfsp = rootvfs; vfsp != (struct vfs *)0; vfsp = vfsp->vfs_next) {
		error = VFS_ROOT(vfsp, &rootvn);
		if (error)
			return (error);
		error = VOP_GETATTR(rootvn, &vattr, u.u_cred);
		if (error)
			return (error);
		if (vafsid == vattr.va_fsid) {
			*vfspp = vfsp;
			return (0);
		}
	}
	return (EINVAL);			/* not found */
}

/*
 * The policies and routines below guarantee that a unique device number
 * (as returned by the stat() system call) is associated with each mounted
 * filesystem and they must be adhered to by all filesystem types.
 * 	Local filesystems (i.e., those with associated Unix devices)
 * do not use the routines below.  Their device number is the device number
 * of the Unix device associated with the filesystem. The range
 * 0x0000 - 0x7fff is reserved for filesystems of this type.
 * 	Non-local filesystems use the range 0x8000-0xffff. For the major
 * device number, filesystem types which only require one major device number
 * for all mounts use their reserved number which is 0x80 + the index of
 * their filesystem type in the filesystem type table (vfs_conf.c). This
 * number may be obtained by calling the routine vfs_fixedmajor(). Filesystem
 * types requiring more than one major device number should obtain these
 * numbers via calls to vfs_getmajor() and release them via calls to
 * vfs_putmajor(). Minor device numbers are under the control of
 * individual filesystem types. Any filesystem types that wishes may
 * allocate and de-allocate minor device numbers using the routines
 * vfs_getnum() and vfs_putnum() and its own private minor device number map.
 */

#define LOGBBY 3
#define MAJOR_MIN 128
#define MAJOR_MAX 255
#define NUM_MAJOR (MAJOR_MAX - MAJOR_MIN + 1)
#define MAJOR_MAPSIZE (NUM_MAJOR/NBBY)
static char devmap[MAJOR_MAPSIZE];    /* Bitmap storing used major device #s */

/*
 * Return the next available major device number from the range 128-255.
 * If a free number is available it is returned. Otherwise the reserved
 * number for the filesystem type is returned.
 */
int
vfs_getmajor(vfsp)
	struct vfs *vfsp;
{
	register int i;

	/* Load the filesystem-reserved numbers */
	for (i = 0; i < (vfsNVFS - vfssw); i++)
		devmap[i >> LOGBBY] |= (1 << (i - ((i >> LOGBBY) << LOGBBY)));

	/* Get the first avalaible number */
	i = vfs_getnum(devmap, MAJOR_MAPSIZE);

	/* If none are available, return the reserved # for this fs type */
	if (i == -1)
		i = vfs_fixedmajor(vfsp);
	else
		i += MAJOR_MIN;
	return (i);
}

/*
 * Return the reserved major device number for this filesystem type
 * defined as its position in the filesystem type table.
 */
int
vfs_fixedmajor(vfsp)
	struct vfs* vfsp;
{
	register struct vfssw *vs;

	for (vs = vfssw; vs < vfsNVFS; vs++) {
		if (vs->vsw_ops == vfsp->vfs_op)
			break;
	}
	return ((vs - vfssw) + MAJOR_MIN);
}

/*
 * Free the major number "num". "num" must have been allocated by a
 * call to vfs_getmajor().
 */
void
vfs_putmajor(vfsp, num)
	struct vfs *vfsp;
	register int num;
{

	num -= MAJOR_MIN;
	if (vfsp->vfs_op == vfssw[num].vsw_ops)
		return;
	vfs_putnum(devmap, num);
}

/*
 * Set and return the first free position from the bitmap "map".
 * Return -1 if no position found.
 */
int
vfs_getnum(map, mapsize)
	register char *map;
	int mapsize;
{
	register int i;
	register char *mp;

	for (mp = map; mp < &map[mapsize]; mp++) {
		if (*mp != (char)0xff) {
			for (i=0; i < NBBY; i++) {
				if (!((*mp >> i) & 0x1)) {
					*mp |= (1 << i);
					return ((mp - map) * NBBY  + i);
				}
			}
		}
	}
	return (-1);
}

/*
 * Clear the designated position "n" in bitmap "map".
 */
void
vfs_putnum(map, n)
	register char *map;
	int n;
{

	if (n >= 0)
		map[n >> LOGBBY] &= ~(1 << (n - ((n >> LOGBBY) << LOGBBY)));
}

