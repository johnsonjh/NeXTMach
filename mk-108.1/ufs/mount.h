/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 **********************************************************************
 * HISTORY
 * 25-Jan-89  Peter King (king) at NeXT
 *	Cleaned out SUN_VFS compiler switches and old VICE hooks.
 *	NFS 4.0 Changes.  Removed mount table; made it a linked list.
 *
 * 27-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Moved to ../ufs from ../h.
 *
 *  7-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Merge VICE changes -- include vice.h and change to #if VICE.
 *
 *  2-Dec-86  Jay Kistler (jjk) at Carnegie-Mellon University
 *	VICE:  added fields to mount entries to allow determination
 *	of whether file system is handled by Venus or not.
 *
 **********************************************************************
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/* @(#)mount.h	2.2 88/06/09 4.0NFSSRC SMI;	from UCB 7.1 6/4/86	*/
/*				@(#) from SUN 2.10	*/


/*
 * Mount structure.
 * One allocated on every ufs mount.
 * Used to find the super block.
 */
struct	mount {
	struct vfs	*m_vfsp;	/* vfs structure for this filesystem */
	dev_t		m_dev;		/* device mounted */
	struct vnode	*m_devvp;	/* vnode for block device mounted */
	struct buf	*m_bufp;	/* pointer to superblock */
	struct inode	*m_qinod;	/* QUOTA: pointer to quota file */
	u_short		m_qflags;	/* QUOTA: filesystem flags */
	u_long		m_btimelimit;	/* QUOTA: block time limit */
	u_long		m_ftimelimit;	/* QUOTA: file time limit */
	struct mount	*m_nxt;		/* linked list of all mounts */
};
#ifdef KERNEL
extern struct	mount *mounttab;
/*
 * Convert vfs ptr to mount ptr.
 */
#define VFSTOM(VFSP)	((struct mount *)((VFSP)->vfs_data))

/*
 * Operations
 */
struct mount *getmp();
#endif
