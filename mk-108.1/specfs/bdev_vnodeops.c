/*
 * HISTORY
 *  9-Mar-88 John Seamons (jks) at NeXT
 *	Attach vm_info structure to dev_vnode.
 *
 * 27-Oct-87  Peter King (king) at NeXT, Inc.
 * 	Original Sun source, ported to Mach.
 *
 **********************************************************************
 */ 

/* @(#)bdev_vnodeops.c	1.3 87/06/30 3.2/4.3NFSSRC */
#ifndef lint
/* static  char sccsid[] = "@(#)bdev_vnodeops.c 1.1 86/09/25"; */
#endif

/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */


#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/conf.h>
#import <sys/buf.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <kern/mfs.h>

#if	MACH
#import <kern/kalloc.h>
#endif	MACH

/*
 * Convert a block dev into a vnode pointer suitable for bio.
 */

struct dev_vnode {
	struct vnode dv_vnode;
	struct dev_vnode *dv_link;
} *dev_vnode_headp;

bdev_badop()
{

	panic("bdev_badop");
}

/*ARGSUSED*/
int
bdev_open(vpp, flag, cred)
	struct vnode **vpp;
	int flag;
	struct ucred *cred;
{

	return ((*bdevsw[major((*vpp)->v_rdev)].d_open)((*vpp)->v_rdev, flag));
}

/*ARGSUSED*/
int
bdev_close(vp, flag, cred)
	struct vnode *vp;
	int flag;
	struct ucred *cred;
{

	/*
	 * On last close of a block device (that isn't mounted)
	 * we must invalidate any in core blocks, so that
	 * we can, for instance, change floppy disks.
	 */
#if	NeXT
	bflush(vp, NODEV, NODEV);
#else	NeXT
	bflush(vp);
#endif	NeXT
	binval(vp);
	return ((*bdevsw[major(vp->v_rdev)].d_close)(vp->v_rdev, flag));
}

/*
 * For now, the only value we actually return is size.
 */
/*ARGSUSED*/
int
bdev_getattr(vp, vap, cred)
	struct vnode *vp;
	register struct vattr *vap;
	struct ucred *cred;
{
	int (*size)();

	bzero((caddr_t) vap, sizeof (struct vattr));
	size = bdevsw[major(vp->v_rdev)].d_psize;
	if (size) {
		vap->va_size = (*size)(vp->v_rdev) * DEV_BSIZE;
	}
	return (0);
}

/*ARGSUSED*/
int
bdev_inactive(vp)
	struct vnode *vp;
{

	/* could free the vnode here */
	return (0);
}

int
bdev_strategy(bp)
	struct buf *bp;
{

	(*bdevsw[major(bp->b_vp->v_rdev)].d_strategy)(bp);
	return (0);
}

struct vnodeops dev_vnode_ops = {
	bdev_open,
	bdev_close,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_getattr,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_inactive,
	bdev_badop,
	bdev_strategy,
	bdev_badop,
	bdev_badop,
	bdev_badop,
	bdev_badop,
#if	MACH
	bdev_badop,			/* pagein */
	bdev_badop,			/* pageout */
	bdev_badop,			/* nlinks */
#endif	MACH
};

/*
 * Convert a block device into a special purpose vnode for bio
 */
struct vnode *
bdevvp(dev)
	dev_t dev;
{
	register struct dev_vnode *dvp;
	register struct dev_vnode *endvp;

	endvp = (struct dev_vnode *)0;
	for (dvp = dev_vnode_headp; dvp; dvp = dvp->dv_link) {
		if (dvp->dv_vnode.v_rdev == dev) {
			VN_HOLD(&dvp->dv_vnode);
			return (&dvp->dv_vnode);
		}
		endvp = dvp;
	}
	dvp = (struct dev_vnode *)
#if	MACH
		kalloc((u_int)sizeof (struct dev_vnode));
#else	MACH
		kmem_alloc((u_int)sizeof (struct dev_vnode));
#endif	MACH
	bzero((caddr_t)dvp, sizeof (struct dev_vnode));
	dvp->dv_vnode.v_count = 1;
	dvp->dv_vnode.v_op = &dev_vnode_ops;
	dvp->dv_vnode.v_rdev = dev;
#if	MACH
/*	vm_info_init(&dvp->dv_vnode);*/
/* no vm_info struct */
#endif	MACH
	if (endvp != (struct dev_vnode *)0) {
		endvp->dv_link = dvp;
	} else {
		dev_vnode_headp = dvp;
	}
	return (&dvp->dv_vnode);
}


