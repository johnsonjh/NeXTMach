/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 * 27-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes.
 */ 

#ifndef VFS_H
#define VFS_H

#ifndef	MAXNAMLEN
#define	MAXNAMLEN	255
#endif	MAXNAMLEN

/* @(#)vfs.h 2.2 88/05/25 4.0NFSSRC SMI; from vfs.h 2.15 87/01/14 SMI	*/

/*
 * File system identifier. Should be unique (at least per machine).
 */
typedef struct {
	long val[2];			/* file system id type */
} fsid_t;

/*
 * File identifier. Should be unique per filesystem on a single machine.
 */
#define	MAXFIDSZ	16
#define freefid(fidp) \
    kfree((caddr_t)(fidp), sizeof (struct fid) - MAXFIDSZ + (fidp)->fid_len)

struct fid {
	u_short		fid_len;		/* length of data in bytes */
	char		fid_data[MAXFIDSZ];	/* data (variable length) */
};

/*
 * Structure per mounted file system.
 * Each mounted file system has an array of
 * operations and an instance record.
 * The file systems are put on a singly linked list.
 */
struct vfs {
	struct vfs	*vfs_next;		/* next vfs in vfs list */
	struct vfsops	*vfs_op;		/* operations on vfs */
	struct vnode	*vfs_vnodecovered;	/* vnode we mounted on */
	int		vfs_flag;		/* flags */
	int		vfs_bsize;		/* native block size */
	fsid_t		vfs_fsid;		/* file system id */
	caddr_t 	vfs_stats;		/* filesystem statistics */
	char		vfs_name[MAXNAMLEN+1];	/* name if faking dir entry */
	struct vfs	*vfs_nextentry;		/* next fake entry in vnode */ 
	uid_t		vfs_uid;		/* user who mounted this */
	caddr_t		vfs_data;		/* private data */
};

/*
 * vfs flags.
 * VFS_MLOCK lock the vfs so that name lookup cannot proceed past the vfs.
 * This keeps the subtree stable during mounts and unmounts.
 */
#define VFS_RDONLY	0x01		/* read only vfs */
#define VFS_MLOCK	0x02		/* lock vfs so that subtree is stable */
#define VFS_MWAIT	0x04		/* someone is waiting for lock */
#define VFS_NOSUID	0x08		/* turn off set-uid on exec */
#if	NeXT
/*
 * For now, just overload NOSUID bit, since we don't want to change
 * mount command, etc.  When this gets its own bit, we'll have to
 * edit sys/mount.h to add M_NODEV and mntent.h to add MNTOPT_NODEV
 * along with hacking all the appropriate programs.
 */
#define VFS_NODEV	VFS_NOSUID	/* no access to blk and char dev */
#endif	NeXT
#define VFS_GRPID	0x10		/* Old BSD group-id on create */
#define VFS_NOSUB	0x20		/* No mounts allowed beneath this fs */
#define VFS_REMOUNT	0x40		/* modify mount otions only */
#define VFS_MULTI	0x80		/* Do multi-component lookup on files */

/*
 * Operations supported on virtual file system.
 */
struct vfsops {
	int	(*vfs_mount)();		/* mount file system */
	int	(*vfs_unmount)();	/* unmount file system */
	int	(*vfs_root)();		/* get root vnode */
	int	(*vfs_statfs)();	/* get fs statistics */
	int	(*vfs_sync)();		/* flush fs buffers */
	int	(*vfs_vget)();		/* get vnode from fid */
	int	(*vfs_mountroot)();	/* mount the root filesystem */
#ifndef MACH
	int	(*vfs_swapvp)();	/* return vnode for swap */
#endif  MACH
};

#define VFS_MOUNT(VFSP, PATH, DATA) \
				 (*(VFSP)->vfs_op->vfs_mount)(VFSP, PATH, DATA)
#define VFS_UNMOUNT(VFSP)	 (*(VFSP)->vfs_op->vfs_unmount)(VFSP)
#define VFS_ROOT(VFSP, VPP)	 (*(VFSP)->vfs_op->vfs_root)(VFSP,VPP)
#define VFS_STATFS(VFSP, SBP)	 (*(VFSP)->vfs_op->vfs_statfs)(VFSP,SBP)
#define VFS_SYNC(VFSP)		 (*(VFSP)->vfs_op->vfs_sync)(VFSP)
#define VFS_VGET(VFSP, VPP, FIDP) (*(VFSP)->vfs_op->vfs_vget)(VFSP, VPP, FIDP)
#define VFS_MOUNTROOT(VFSP, VPP, NM) \
				 (*(VFSP)->vfs_op->vfs_mountroot)(VFSP, VPP, NM)
#ifndef MACH
#define VFS_SWAPVP(VFSP, VPP, NM) (*(VFSP)->vfs_op->vfs_swapvp)(VFSP, VPP, NM)
#endif  MACH

/*
 * file system statistics
 */
struct statfs {
	long f_type;			/* type of info, zero for now */
	long f_bsize;			/* fundamental file system block size */
	long f_blocks;			/* total blocks in file system */
	long f_bfree;			/* free block in fs */
	long f_bavail;			/* free blocks avail to non-superuser */
	long f_files;			/* total file nodes in file system */
	long f_ffree;			/* free file nodes in fs */
	fsid_t f_fsid;			/* file system id */
	long f_spare[7];		/* spare for later */
};

#ifdef KERNEL
/*
 * Filesystem type switch table
 */
struct vfssw {
	char		*vsw_name;	/* type name string */
	struct vfsops	*vsw_ops;	/* filesystem operations vector */
};

/*
 * public operations
 */
extern void	vfs_mountroot();	/* mount the root */
extern int	vfs_add();		/* add a new vfs to mounted vfs list */
extern void	vfs_remove();		/* remove a vfs from mounted vfs list */
extern int	vfs_lock();		/* lock a vfs */
extern void	vfs_unlock();		/* unlock a vfs */
extern struct vfs *getvfs();		/* return vfs given fsid */
extern struct vfssw *getfstype();	/* find default filesystem type */
extern int vfs_getmajor();		/* get major device # for an fs type */
extern void vfs_putmajor();		/* free major device # for an fs type */
extern int vfs_getnum();		/* get device # for an fs type */
extern void vfs_putnum();		/* release device # for an fs type */

#define VFS_INIT(VFSP, OP, DATA)	{ \
	(VFSP)->vfs_next = (struct vfs *)0; \
	(VFSP)->vfs_op = (OP); \
	(VFSP)->vfs_flag = 0; \
	(VFSP)->vfs_stats = NULL; \
	(VFSP)->vfs_data = (DATA); \
	(VFSP)->vfs_nextentry = (struct vfs *)0; \
	(VFSP)->vfs_uid = u.u_uid; \
}
/*
 * globals
 */
extern struct vfs *rootvfs;		/* ptr to root vfs structure */
extern struct vfssw vfssw[];		/* table of filesystem types */
extern struct vfssw *vfsNVFS;		/* vfs switch table end marker */
#endif

#endif /* def VFS_H */
