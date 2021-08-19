/*
 **********************************************************************
 * HISTORY
 * 17-Jan-89  Peter King (king) at NeXT
 *	Original Sun source.  Ported to Mach
 **********************************************************************
 */

#ifndef lint
/* "@(#)spec_vfsops.c	2.3 88/08/05 4.0NFSSRC Copyr 1988 Sun Micro" */
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.12
 */


#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/bootconf.h>
#import <specfs/snode.h>
#import <sys/reboot.h>

extern	int spec_sync();	/* XXX - should be static */
/*
 * Used in spec_vnodeops.c
 */
int spec_badop();
static	int spec_mountroot();
static	int spec_swapvp();

struct vfsops spec_vfsops = {
	spec_badop,		/* mount */
	spec_badop,		/* unmount */
	spec_badop,		/* root */
	spec_badop,		/* statfs */
	spec_sync,
	spec_badop,		/* vget */
	spec_mountroot,
#if	!MACH
	spec_swapvp,
#endif	!MACH
};

int
spec_badop()
{

	panic("spec_badop");
}

/*
 * Run though all the snodes and force write back
 * of all dirty pages on the block devices.
 */
/*ARGSUSED*/
static int
spec_sync(vfsp)
	struct vfs *vfsp;
{
	static int spec_lock;
	register struct snode **spp, *sp;
	register struct vnode *vp;

	if (spec_lock)
		return (0);

	spec_lock++;
	for (spp = stable; spp < &stable[STABLESIZE]; spp++) {
		for (sp = *spp; sp != (struct snode *)NULL; sp = sp->s_next) {
			vp = STOV(sp);
			/*
			 * Don't bother sync'ing a vp if it
			 * is part of virtual swap device.
			 */
			if (((vp)->v_flag & VISSWAP) != 0)
				continue;
			if (vp->v_type == VBLK) {
#if	NeXT
				bflush(vp, NODEV, NODEV);
				/* start delayed writes */
#else	NeXT
				bflush(vp);	/* start delayed writes */
#endif	NeXT
			}
		}
	}
	spec_lock = 0;
	return (0);
}

/*ARGSUSED*/
static int
spec_mountroot(vfsp, vpp, name)
	struct vfs *vfsp;
	struct vnode **vpp;
	char *name;
{

	return (EINVAL);
}

#if	!MACH
/*ARGSUSED*/
static int
spec_swapvp(vfsp, vpp, name)
	struct vfs *vfsp;
	struct vnode **vpp;
	char *name;
{
	extern char *strcpy();
	extern struct vnodeops spec_vnodeops;
	dev_t dev;

	if (*name == '\0') {
		/*
		 * No swap name specified, use root dev partition "b"
		 * if it is a block device, otherwise fail.
		 * XXX - should look through device list or something here
		 * if root is not local.
		 */
		if (rootvp->v_op == &spec_vnodeops &&
		    (boothowto & RB_ASKNAME) == 0) {
			dev = makedev(major(rootvp->v_rdev), 1);
			(void) strcpy(name, rootfs.bo_name);
		} 
#ifdef sun
		else {
	extern dev_t getblockdev();
retry:
			if (!(dev = getblockdev("swap", name))) {
				return (ENODEV);
			}
			/*
			 * Check for swap on root device
			 */
			if (rootvp->v_op == &spec_vnodeops &&
			    dev == rootvp->v_rdev) {
				char resp[128];

				printf("Swapping on root device, ok? ");
				gets(resp);
				if (*resp != 'y' && *resp != 'Y') {
					goto retry;
				}
			}
		}
#endif
		name[3] = 'b';
	} else {
		if (name[2] == '\0') {
			/*
			 * Name doesn't include device number, default to
			 * device 0.
			 */
			name[2] = '0';
			name[3] = '\0';
		}
		if (name[3] == '\0') {
			/*
			 * name doesn't include partition, default to
			 * partition b.
			 */
			name[3] = 'b';
			name[4] = '\0';
		}
#ifdef sun
		if (!(dev = getblockdev("swap", name))) {
			return (ENODEV);
		}
#endif
	}
	*vpp = bdevvp(dev);
	return (0);
}
#endif	!MACH


