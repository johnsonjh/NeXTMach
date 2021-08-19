/*
 * HISTORY
 *  9-May-89  Peter King (king) at NeXT
 *	Sun Bugfixes: 1015438 - Removed redundant VN_HOLD(sp->s_bdevvp)
 *		      1012586 - Correctly handle case when size() returns -1.
 *
 * 16-Jan-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes: Support for indirect and clone snodes.
 *
 * 27-Oct-87  Peter King (king) at NeXT, Inc.
 *	Original Sun source, ported to Mach.
 *
 **********************************************************************
 */ 

/* "@(#)spec_subr.c	2.2 88/06/13 4.0NFSSRC Copyr 1988 Sun Micro"; */

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.25
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/conf.h>
#import <specfs/snode.h>
#if	MACH
#import <kern/mfs.h>
#import <kern/kalloc.h>
#import <kern/xpr.h>
#endif	MACH

extern struct vnodeops spec_vnodeops;
struct vnode *slookup();

static ssave();

/*
 * Find an appropriate snode.
 */
static struct snode *sfind();

/*
 * Returns a special vnode for the given dev.  The vnode is the
 * one which is "common" to all the snodes which represent the
 * same block device.
 */
struct vnode *
bdevvp(dev)
	dev_t dev;
{

	return (specvp((struct vnode *)NULL, dev, VBLK));
}

/*
 * Return a shadow special vnode for the given dev.
 * If no snode exists for this dev create one and put it
 * in a table hashed by dev,realvp.  If the snode for
 * this dev is already in the table return it (ref count is
 * incremented by sfind).  The snode will be flushed from the
 * table when spec_inactive calls sunsave.
 *
 * vp will be NULL if this is a block device that is
 * to be shared via the s_bdevvp field in the snodes.
 */
struct vnode *
specvp(vp, dev, type)
	struct vnode *vp;
	dev_t dev;
	enum vtype type;
{
	register struct snode *sp;
	extern struct snode *fifosp();
	struct vattr va;

#if	NeXT
XPR(XPR_FS, ("specvp entered: thread = 0x%x dev = %d,%d\n", current_thread(), dev>>8, dev&0xff));
#endif	NeXT

	if ((sp = sfind(dev, vp, type)) == NULL) {
		if (vp && vp->v_type == VFIFO) {
			sp = fifosp(vp);
		} else {
#if	MACH
			sp = (struct snode *)kalloc((u_int)sizeof (*sp));
			bzero((caddr_t)sp, sizeof (*sp));
#else	MACH
			sp = (struct snode *)kmem_zalloc(sizeof (*sp));
#endif	MACH
			STOV(sp)->v_op = &spec_vnodeops;

			/* init the times in the snode to those in the vnode */
			if (vp && VOP_GETATTR(vp, &va, u.u_cred) == 0) {
				sp->s_atime = va.va_atime;
				sp->s_mtime = va.va_mtime;
				sp->s_ctime = va.va_ctime;
			}
		}
		sp->s_realvp = vp;
		sp->s_dev = dev;
		STOV(sp)->v_rdev = dev;
		STOV(sp)->v_count = 1;
		STOV(sp)->v_data = (caddr_t)sp;
		if (vp != (struct vnode *)NULL) {
			VN_HOLD(vp);
			STOV(sp)->v_type = vp->v_type;
			STOV(sp)->v_vfsp = vp->v_vfsp;
			if (vp->v_type == VBLK) {
				sp->s_bdevvp = bdevvp(dev);
				sp->s_size = VTOS(sp->s_bdevvp)->s_size;
			}
		} else {
			/* must be a `real' block device */
			int (*size)();
			long rsize;

			STOV(sp)->v_type = VBLK;
			STOV(sp)->v_vfsp = NULL;
			sp->s_bdevvp = STOV(sp);
			if ((major(dev) < nblkdev) &&
			    (size = bdevsw[major(dev)].d_psize)) {
				rsize = (*size)(dev);
				if (rsize == -1)        /* did size fail? */
					sp->s_size = 0;	
				else
					sp->s_size = dbtob(rsize);
			} else {
				sp->s_size = 0;
			}
		}
		ssave(sp);
	}
	return (STOV(sp));
}

/*
 * Return a special vnode for the given dev; no vnode is supplied
 * for it to shadow.
 * If no snode exists for this dev (with a NULL realvp), create one
 * and put it in a table hashed by dev, NULL.  If the snode for
 * this dev is already in the table return it (ref count is
 * incremented by sfind).  The snode will be flushed from the
 * table when spec_inactive calls sunsave.
 */
struct vnode *
makespecvp(dev, type)
	dev_t dev;
	enum vtype type;
{
	register struct snode *sp;

#if	NeXT
tryagain:
#endif	NeXT
	if ((sp = sfind(dev, (struct vnode *)NULL, type)) == NULL) {
#if	MACH
		sp = (struct snode *)kalloc(sizeof (*sp));
		bzero((caddr_t)sp, sizeof (*sp));
#else	MACH
		sp = (struct snode *)kmem_zalloc(sizeof (*sp));
#endif	MACH
		STOV(sp)->v_op = &spec_vnodeops;
		STOV(sp)->v_type = type;
		if (type == VBLK) {
			sp->s_bdevvp = bdevvp(dev);
		}
		sp->s_realvp = NULL;
		sp->s_dev = dev;
		STOV(sp)->v_rdev = dev;
		STOV(sp)->v_count = 1;
		STOV(sp)->v_data = (caddr_t)sp;
		STOV(sp)->v_vfsp = NULL;
#if	MACH
/*		vm_info_init(STOV(sp)); */
/* no vm_info struct */
#endif	MACH
		ssave(sp);
#if	NeXT
	} else if (sp->s_flag & SLOCKED) {
			/* This snode is being deactivated */
			sp->s_flag |= SWANT;
			(void) sleep((caddr_t)(sp), PINOD);
			goto tryagain;
#endif	NeXT
	}
	return (STOV(sp));
}

/*
 * Snode lookup stuff.
 * These routines maintain a table of snodes hashed by dev so
 * that the snode for an dev can be found if it already exists.
 */

struct snode *stable[STABLESIZE];

/*
 * Put a snode in the table
 */
static
ssave(sp)
	struct snode *sp;
{

	sp->s_next = stable[STABLEHASH(sp->s_dev)];
	stable[STABLEHASH(sp->s_dev)] = sp;
}

/*
 * Remove a snode from the hash table.
 * The realvp is not released here because spec_inactive() still
 * needs it to do a spec_fsync().
 */
sunsave(sp)
	struct snode *sp;
{
	struct snode *st;
	struct snode *stprev = NULL;

	st = stable[STABLEHASH(sp->s_dev)];
	while (st != NULL) {
		if (st == sp) {
			if (stprev == NULL) {
				stable[STABLEHASH(sp->s_dev)] = st->s_next;
			} else {
				stprev->s_next = st->s_next;
			}
			break;
		}
		stprev = st;
		st = st->s_next;
	}
}

/*
 * Check to see how many open references there are in the snode table for
 * a given device of a given type; if there are any, return 1, otherwise
 * return 0.
 */
int
stillopen(dev, type)
	register dev_t dev;
	register enum vtype type;
{
	register struct snode *st;
	register int count;

	count = 0;
	for (st = stable[STABLEHASH(dev)]; st != NULL; st = st->s_next) {
		if (st->s_dev == dev && STOV(st)->v_type == type)
			count += st->s_count;
	}
	return (count != 0);
}

/*
 * Check to see whether a given device of a given type is currently being
 * closed; if so, return 1, otherwise return 0.
 */
int
isclosing(dev, type)
	register dev_t dev;
	register enum vtype type;
{
	register struct snode *st;

	for (st = stable[STABLEHASH(dev)]; st != NULL; st = st->s_next) {
		if (st->s_dev == dev && STOV(st)->v_type == type
		    && (st->s_flag & SCLOSING))
			return (1);
	}
	return (0);
}

/*
 * Check to see if there is an snode in the table referring to a given device
 * other than the one the vnode provided is associated with.  If so, return
 * it.
 */
struct vnode *
other_specvp(vp)
	register struct vnode *vp;
{
	struct snode *sp;
	register dev_t dev;
	register struct snode *st;
	register struct vnode *nvp;

	sp = VTOS(vp);
	dev = sp->s_dev;
	st = stable[STABLEHASH(dev)];
	while (st != NULL) {
		if (st->s_dev == dev && (nvp = STOV(st)) != vp
		    && nvp->v_type == vp->v_type)
			return (nvp);
		st = st->s_next;
	}
	return (NULL);
}

/*
 * Lookup a snode by type and dev; return a pointer to the vnode in that snode.
 */
struct vnode *
slookup(type, dev)
	enum vtype type;
	dev_t dev;
{
	register struct snode *st;
	register struct vnode *nvp;

	st = stable[STABLEHASH(dev)];
	while (st != NULL) {
		if (st->s_dev == dev) { 
			nvp = STOV(st);
			if (nvp->v_type == type) {
				VN_HOLD(nvp);
				return (nvp);
			}
		}
		st = st->s_next;
	}
	return (NULL);
}

/*
 * Lookup a snode by <dev, vp, type>
 */
static struct snode *
sfind(dev, vp, type)
	dev_t dev;
	struct vnode *vp;
	enum vtype type;
{
	register struct snode *st;

	st = stable[STABLEHASH(dev)];
	while (st != NULL) {
		if ((st->s_dev == dev) && STOV(st)->v_type == type &&
		    ((st->s_realvp && vp && VN_CMP(st->s_realvp, vp))
		      || (st->s_realvp == NULL && vp == NULL))) {
			VN_HOLD(STOV(st));
			return (st);
		}
		st = st->s_next;
	}
	return (NULL);
}

/*
 * Mark the accessed, updated, or changed times in an snode
 * with the current (unique) time
 */
smark(sp, flag)
	register struct snode *sp;
	register int flag;
{
	struct timeval ut;

#if	NeXT
	microtime(&ut);
#else	NeXT
	uniqtime(&ut);
#endif	NeXT
	sp->s_flag |= flag;
	if (flag & SACC)
		sp->s_atime = ut;
	if (flag & SUPD)
		sp->s_mtime = ut;
	if (flag & SCHG) {
		sp->s_ctime = ut;
	}
}

