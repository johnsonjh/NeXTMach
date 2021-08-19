/*	@(#)snode.h	2.3 88/06/20 4.0NFSSRC SMI;	*/
/*      @(#)snode.h 1.13 87/02/22 SMI      */

#ifndef _specfs_snode_h
#define _specfs_snode_h
 
/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

/*
 * HISTORY
 *  4-Jan-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes: Better support for multiple snodes pointing
 *			 at the same device without cache aliasing
 *			 problems.
 */

/*
 * The SNODE represents a special file in any filesystem.  There is
 * one snode for each active special file.  Filesystems which support
 * special files use specvp(vp, dev) to convert a normal vnode to a
 * special vnode in the ops create, mkdir, and lookup.
 *
 * To handle having multiple snode's which represent the same
 * underlying block device vnode without cache aliasing problems,
 * the s_bdevvp is used to point to the "common" vnode used for
 * caching data.  If an snode is created internally by the kernel,
 * then the s_realvp field is NULL and s_bdevvp points to s_vnode.
 * The other snodes which are created as a result of a lookup of a
 * device in a file system have s_realvp pointing to the vp which
 * represents the device in the file system while the s_bdevvp points
 * into the "common" vnode for the block device in another snode.
 */


struct snode {
	struct	snode *s_next;		/* must be first */
	struct	vnode s_vnode;		/* vnode associated with this snode */
	struct	vnode *s_realvp;	/* vnode for the fs entry (if any) */
	struct	vnode *s_bdevvp;	/* blk device vnode (for caching) */
	u_short	s_flag;			/* flags, see below */
	dev_t	s_dev;			/* device the snode represents */
	daddr_t	s_lastr;		/* last read (read-ahead) */
	daddr_t	s_size;			/* block device size in bytes */
	struct timeval  s_atime;	/* time of last access */
	struct timeval  s_mtime;	/* time of last modification */
	struct timeval  s_ctime;	/* time of last attributes change */
	int	s_count;		/* count of opened references */
};

/* flags */
#define SLOCKED		0x01		/* snode is locked */
#define SUPD		0x02		/* update device access time */
#define SACC		0x04		/* update device modification time */
#define	SCLOSING	0x08		/* device is being closed */
#define SWANT		0x10		/* some process waiting on lock */
#define SCHG		0x40		/* update device change time */

/*
 * Convert between vnode and snode
 */
#define	VTOS(vp)	((struct snode *)((vp)->v_data))
#define	STOV(sp)	(&(sp)->s_vnode)

#ifdef KERNEL
/*
 * Lock and unlock snodes.
 */
#define SNLOCK(sp) { \
	while ((sp)->s_flag & SLOCKED) { \
		(sp)->s_flag |= SWANT; \
		(void) sleep((caddr_t)(sp), PINOD); \
	} \
	(sp)->s_flag |= SLOCKED; \
}

#define SNUNLOCK(sp) { \
	(sp)->s_flag &= ~SLOCKED; \
	if ((sp)->s_flag & SWANT) { \
		(sp)->s_flag &= ~SWANT; \
		wakeup((caddr_t)(sp)); \
	} \
}

/*
 * Construct a spec vnode for a given device that shadows a particular
 * "real" vnode.
 */
struct vnode *specvp();

/*
 * Construct a spec vnode for a given device that shadows nothing.
 */
struct vnode *makespecvp();

/*
 * Find any other spec vnode that refers to the same device as another vnode.
 */
struct vnode *other_specvp();

/*
 * Snode lookup stuff.
 * These routines maintain a table of snodes hashed by dev so
 * that the snode for an dev can be found if it already exists.
 * NOTE: STABLESIZE must be a power of 2 for STABLEHASH to work!
 */

#define	STABLESIZE	16
#define	STABLEHASH(dev)	((major(dev) + minor(dev)) & (STABLESIZE - 1))
extern struct snode *stable[];

extern struct vnodeops spec_vnodeops;
#endif KERNEL

#endif !_specfs_snode_h
