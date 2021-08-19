/*
 * HISTORY
 *  9-May-89  Peter King (king) at NeXT
 *	Sun Bugfixes: 1014231 - Blocks associated with certain devices do
 *				not get flushed properly when the device
 *				is closed.
 *
 * 17-Dec-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes: Support for clone/indirect snodes.
 *
 *  9-Mar-88  John Seamons (jks) at NeXT
 *	Attach vm_info structure to snode.
 *
 * 18-Jan-88  Gregg Kellogg (gk) at Next, Inc.
 *	STREAMS: added support for SVR2.1 streams and VSTR vnode type.
 *
 * 27-Oct-87  Peter King (king) at NeXT, Inc.
 *	Original Sun source, ported to Mach.
 *
 **********************************************************************
 */ 

#import <sun_lock.h>

#ifndef lint
/* static char sccsid[] = 	"@(#)spec_vnodeops.c	2.4 88/08/05 4.0NFSSRC Copyr 1988 Sun Micro"; */
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.66
 */

#if	NeXT
#import <xpr_debug.h>
#endif	NeXT

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/kernel.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/uio.h>
#import <sys/conf.h>
#import <sys/file.h>
#import <specfs/snode.h>
#if	MACH
#import <kern/mfs.h>
#endif	MACH
#if	STREAMS
#import <sys/stream.h>
#endif	STREAMS

#import <krpc/lockmgr.h>

static int spec_open();
static int spec_close();
static int spec_rdwr();
static int spec_ioctl();
static int spec_select();
static int spec_inactive();
static int spec_strategy();
static int spec_noop();
static int spec_dump();
static int spec_cmp();
/*
 * Used directly in fifo_vnodeops
 */
int spec_setattr();
int spec_getattr();
int spec_access();
int spec_link();
#if	SUN_LOCK
int spec_lockctl();
#endif	SUN_LOCK
int spec_fsync();
int spec_fid();
int spec_realvp();
/*
 * Declared in spec_vfsops.c
 */
int spec_badop();


struct vnodeops spec_vnodeops = {
	spec_open,
	spec_close,
	spec_rdwr,
	spec_ioctl,
	spec_select,
	spec_getattr,
	spec_setattr,
	spec_access,
	spec_noop,	/* lookup */
	spec_noop,	/* create */
	spec_noop,	/* remove */
	spec_link,
	spec_noop,	/* rename */
	spec_noop,	/* mkdir */
	spec_noop,	/* rmdir */
	spec_noop,	/* readdir */
	spec_noop,	/* symlink */
	spec_noop,	/* readlink */
	spec_fsync,
	spec_inactive,
	spec_badop,	/* bmap */
	spec_strategy,	/* strategy */
	spec_badop,	/* bread */
	spec_badop,	/* brelse */
#if	SUN_LOCK
	spec_lockctl,
#else	SUN_LOCK
	spec_badop,
#endif	SUN_LOCK
	spec_fid,
	spec_dump,
	spec_cmp,
	spec_realvp,
#if	MACH
	spec_badop,	/* pagein */
	spec_badop,	/* pageout */
	spec_badop,	/* nlinks */
#endif	MACH
};

/*
 * open a special file (device)
 * Some weird stuff here having to do with clone and indirect devices:
 * When a file lookup operation happens (e.g. ufs_lookup) and the vnode has
 * type VDEV specvp() is used to return a spec vnode instead.  Then when
 * the VOP_OPEN routine is called, we get control here.  When we do the
 * device open routine there are several possible strange results:
 * 1) An indirect device will return the error EAGAIN on open and return
 *    a new dev number.  We have to make that into a spec vnode and call
 *    open on it again.
 * 2) The clone device driver will return the error EEXIST and return a
 *    new dev number.  As above, we build a new vnode and call open again,
 *    explicitly asking the open routine to do a clone open.
 * 3) A clone device will return a new dev number on open but no error.
 *    In this case we just make a new spec vnode out of the new dev number
 *    and return that.
 * The last two cases differ in that the decision to clone arises outside
 * of the target device in 2) and from within in 3).
 *
 * TODO: extend case 2) to apply to all character devices, not just streams
 * devices.
 */
/*ARGSUSED*/
static int
spec_open(vpp, flag, cred)
	struct vnode **vpp;
	int flag;
	struct ucred *cred;
{
	register struct snode *sp;
	dev_t dev;
	dev_t newdev;
	register int error;

	sp = VTOS(*vpp);

	/*
	 * Do open protocol for special type.
	 */
	dev = sp->s_dev;

	switch ((*vpp)->v_type) {

	case VCHR:
	case VSTR:
		newdev = dev;
		error = 0;
		for (;;) {
			register struct vnode *nvp;

			dev = newdev;
			if ((u_int)major(dev) >= nchrdev)
				return (ENXIO);

			while (isclosing(dev, (*vpp)->v_type))
				(void) sleep((caddr_t)sp, PSLEP);

#if	STREAMS
			if ((*vpp)->v_type == VSTR || cdevsw[major(dev)].d_str)
			{
				(*vpp)->v_type = VSTR;
				error = str_open(dev, flag, *vpp, &newdev);
			} else
#endif	STREAMS
				error = (*cdevsw[major(dev)].d_open)(dev,
								     flag,
								     &newdev);

			/*
			 * If this is an indirect device or a forced clone,
			 * we need to do the open again.  In both cases,
			 * we insist that newdev differ from dev, to help
			 * avoid infinite regress.
			 */
			if (newdev == dev ||
			    (error != 0 && error != EAGAIN && error != EEXIST))
				break;

			/*
			 * Allocate new snode with new device.  Release old
			 * snode. Set vpp to point to new one.  This snode will
			 * go away when the last reference to it goes away.
			 * Warning: if you stat this, and try to match it with
			 * a name in the filesystem you will fail, unless you
			 * had previously put names in that match.
			 */
			nvp = specvp(*vpp, newdev, VCHR);
			sp = VTOS(nvp);
			VN_RELE(*vpp);
			*vpp = nvp;
		}
		break;

	case VFIFO:
		printf("spec_open: got a VFIFO???\n");
		/* fall through to... */

	case VSOCK:
		error = EOPNOTSUPP;
		break;
	case VBLK:
		/*
		 * The block device sizing was already done in specvp().
		 * However, we still need to verify that we can open the
		 * block device here (since specvp was called as part of a
		 * "lookup", not an "open", and e.g. "stat"ing a block special
		 * file with an illegal major device number should be legal).
		 */
		if ((u_int)major(dev) >= nblkdev)
			error = ENXIO;
		else
			error = (*bdevsw[major(dev)].d_open)(dev, flag);
		break;

	default:
		error = 0;
		break;
	}
	if (error == 0)
		sp->s_count++;		/* one more open reference */
	return (error);
}

/*ARGSUSED*/
static int
spec_close(vp, flag, count, cred)
	struct vnode *vp;
	int flag;
	int count;
	struct ucred *cred;
{
	register struct snode *sp;
	dev_t dev;

	if (count > 1)
		return (0);

	/*
	 * setjmp in case close is interrupted
	 */
	if (setjmp(&u.u_qsave)) {
		sp = VTOS(vp);	/* recompute - I don't trust setjmp/longjmp */
		sp->s_flag &= ~SCLOSING;
		wakeup((caddr_t)sp);
		return (EINTR);
	}

	sp = VTOS(vp);
	sp->s_count--;		/* one fewer open reference */

	/*
	 * Only call the close routine when the last open
	 * reference through any [s,v]node goes away.
	 */
	if (stillopen(sp->s_dev, vp->v_type))
		return (0);

	dev = sp->s_dev;

	switch (vp->v_type) {

	case VCHR:
		(void) (*cdevsw[major(dev)].d_close)(dev, flag);
		break;

#if	STREAMS
	case VSTR:
		str_close(dev, flag, vp);
		return(0);
#endif	STREAMS

	case VBLK:
		/*
		 * On last close of a block device (that isn't mounted)
		 * we must invalidate any in core blocks so that
		 * we can, for instance, change floppy disks.
		 */
#if	NeXT
		bflush(sp->s_bdevvp, NODEV, NODEV);
#else	NeXT
		bflush(sp->s_bdevvp);
#endif	NeXT
		binval(sp->s_bdevvp);
		(void) (*bdevsw[major(dev)].d_close)(dev, flag);
		break;

	case VFIFO:
		printf("spec_close: got a VFIFO???\n");
		break;
	}

	return (0);
}

/*
 * read or write a spec vnode
 */
/*ARGSUSED*/
static int
spec_rdwr(vp, uiop, rw, ioflag, cred)
	struct vnode *vp;
	register struct uio *uiop;
	enum uio_rw rw;
	int ioflag;
	struct ucred *cred;
{
	register struct snode *sp;
	struct vnode *blkvp;
	dev_t dev;
	struct buf *bp;
	daddr_t lbn, bn;
	register int n, on;
	int size;
	u_int bdevsize;
	int error;
	extern int mem_no;

	sp = VTOS(vp);
	dev = (dev_t)sp->s_dev;
	if (rw != UIO_READ && rw != UIO_WRITE)
		panic("spec_rdwr");
	if (rw == UIO_READ && uiop->uio_resid == 0)
		return (0);
	if ((uiop->uio_offset < 0 ||
	    (uiop->uio_offset + uiop->uio_resid) < 0) &&
	    !(vp->v_type == VCHR && mem_no == major(dev))) {
		return (EINVAL);
	}

	if (rw == UIO_READ)
		smark(sp, SACC);

	if (vp->v_type == VCHR) {
		if (rw == UIO_READ) {
			error = (*cdevsw[major(dev)].d_read)(dev, uiop);
		} else {
			smark(sp, SUPD|SCHG);
			error = (*cdevsw[major(dev)].d_write)(dev, uiop);
		}
		return (error);
#if	STREAMS
	} else if (vp->v_type == VSTR) {
		if (rw == UIO_READ) {
			error = str_read(dev, uiop, vp);
		} else {
			smark(sp, SUPD|SCHG);
			error = str_write(dev, uiop, vp);
		}
		return(error);
#endif	STREAMS
	}

	if (vp->v_type != VBLK)
		return (EOPNOTSUPP);

	if (uiop->uio_resid == 0)
		return (0);

	error = 0;
	blkvp = sp->s_bdevvp;
	bdevsize = MAXBSIZE;
	do {
		lbn = uiop->uio_offset / bdevsize;
		on = uiop->uio_offset % bdevsize;
		n = MIN((MAXBSIZE - on), uiop->uio_resid);
		bn = lbn * (bdevsize/DEV_BSIZE);
		rablock = bn + (bdevsize/DEV_BSIZE);
		rasize = size = bdevsize;
		if (rw == UIO_READ) {
			if ((long)bn<0) {
				bp = geteblk(size);
				clrbuf(bp);
			} else if (sp->s_lastr + 1 == lbn)
				bp = breada(blkvp, bn, size, rablock,
					rasize);
			else
				bp = bread(blkvp, bn, size);
			sp->s_lastr = lbn;
		} else {
#if	MACH
#else	MACH
			int i, count;
			extern struct cmap *mfind();

			count = howmany(size, DEV_BSIZE);
			for (i = 0; i < count; i += CLBYTES/DEV_BSIZE)
				if (mfind(blkvp, (daddr_t)(bn + i)))
					munhash(blkvp, (daddr_t)(bn + i));
#endif	MACH
			if (n == bdevsize) 
				bp = getblk(blkvp, bn, size);
			else
				bp = bread(blkvp, bn, size);
		}
		n = MIN(n, bp->b_bcount - bp->b_resid);
		if (bp->b_flags & B_ERROR) {
			error = EIO;
			brelse(bp);
			goto bad;
		}
		error = uiomove(bp->b_un.b_addr+on, n, rw, uiop);
		if (rw == UIO_READ) {
			if (n + on == bdevsize)
				bp->b_flags |= B_AGE;
			brelse(bp);
		} else {
			if (ioflag & IO_SYNC)
				bwrite(bp);
			else if (n + on == bdevsize) {
				bp->b_flags |= B_AGE;
				bawrite(bp);
			} else
				bdwrite(bp);
			smark(sp, SUPD|SCHG);
		}
	} while (error == 0 && uiop->uio_resid > 0 && n != 0);
bad:
	return (error);
}

/*ARGSUSED*/
static int
spec_ioctl(vp, com, data, flag, cred)
	struct vnode *vp;
	int com;
	caddr_t data;
	int flag;
	struct ucred *cred;
{
	register struct snode *sp;

#if	STREAMS
	if (vp->v_type == VSTR)
		return(str_ioctl(vp, com, data));
#endif	STREAMS
	sp = VTOS(vp);
	if (vp->v_type != VCHR)
		panic("spec_ioctl");
	return ((*cdevsw[major(sp->s_dev)].d_ioctl)
		(sp->s_dev, com, data, flag));
}

/*ARGSUSED*/
static int
spec_select(vp, which, cred)
	struct vnode *vp;
	int which;
	struct ucred *cred;
{
	register struct snode *sp;

#if	STREAMS
	if (vp->v_type == VSTR)
		return(str_select(vp, which));
#endif	STREAMS
	sp = VTOS(vp);
	if (vp->v_type != VCHR)
		panic("spec_select");
	return ((*cdevsw[major(sp->s_dev)].d_select)(sp->s_dev, which));
}

static int
spec_inactive(vp, cred)
	struct vnode *vp;
	struct ucred *cred;
{
	struct snode *sp;

	sp = VTOS(vp);
        /* must sunsave() first to prevent a race when spec_fsync() sleeps */
	sunsave(sp);
 
	if (sp->s_realvp)
		(void) spec_fsync(vp, cred);

	/* now free the realvp (no longer done by sunsave()) */
	if (sp->s_realvp) {
		VN_RELE(sp->s_realvp);
		sp->s_realvp = NULL;
		if (sp->s_bdevvp)
			VN_RELE(sp->s_bdevvp);
	}

#if	MACH
	kfree((caddr_t)sp, sizeof (*sp));
#else	MACH
	kmem_free((caddr_t)sp, sizeof (*sp));
#endif	MACH
	return (0);
}

static int
spec_getattr(vp, vap, cred)
	struct vnode *vp;
	register struct vattr *vap;
	struct ucred *cred;
{
	int error;
	register struct snode *sp;
	register struct vnode *realvp;
	enum vtype type;

	sp = VTOS(vp);
	if ((realvp = sp->s_realvp) == NULL) {
		/*
		 * No real vnode behind this one.
		 * Set the device size from snode.
		 * Set times to the present.
		 * Set blocksize based on type in the unreal vnode.
		 */
		bzero((caddr_t)vap, sizeof (*vap));
		vap->va_size = sp->s_size;
		vap->va_atime = time;
		vap->va_mtime = time;
		vap->va_ctime = time;
		type = vp->v_type;
	} else {
		error = VOP_GETATTR(realvp, vap, cred);
		if (error != 0)
			return (error);
		/* set current times from snode, even if older than vnode */
		vap->va_atime = sp->s_atime;
		vap->va_mtime = sp->s_mtime;
		vap->va_ctime = sp->s_ctime;
		type = vap->va_type;
	}

	/* set device-dependent blocksizes */
	switch (type) {
#define OLDCODE
#ifdef OLDCODE
	case VBLK:
		vap->va_blocksize = BLKDEV_IOSIZE;
		break;

	case VCHR:
		vap->va_blocksize = MAXBSIZE;
		break;
#else
	case VBLK:			/* was BLKDEV_IOSIZE	*/
	case VCHR:
		vap->va_blocksize = MAXBSIZE;
		break;
#endif OLDCODE
#if	STREAMS
	case VSTR:
		vap->va_blocksize=str_block_size(str_block_maxclass());
		break;
#endif	STREAMS
	}
	return (0);
}

int
spec_setattr(vp, vap, cred)
	struct vnode *vp;
	register struct vattr *vap;
	struct ucred *cred;
{
	register struct snode *sp;
	register struct vnode *realvp;
	int error;
	register int chtime = 0;

	sp = VTOS(vp);
	if ((realvp = sp->s_realvp) == NULL)
		error = 0;			/* no real vnode to update */
	else {
#if	NeXT
		/*
		 * The real vnode doesn't have any blocks associated
		 * so it's a big mistake to try and change it's size
		 */
		vap->va_size = (u_long) -1;
#endif	NeXT
		error = VOP_SETATTR(realvp, vap, cred);
	}
	if (error == 0) {
		/* if times were changed, update snode */
		if (vap->va_mtime.tv_sec != -1) {
			sp->s_mtime = vap->va_mtime;
			chtime++;
		}
		if (vap->va_atime.tv_sec != -1) {
			sp->s_atime = vap->va_atime;
			chtime++;
		}
		if (chtime)
			sp->s_ctime = time;
	}
	return (error);
}

int
spec_access(vp, mode, cred)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
{
	register struct vnode *realvp;

	if ((realvp = VTOS(vp)->s_realvp) != NULL)
		return (VOP_ACCESS(realvp, mode, cred));
	else
		return (0);	/* allow all access */
}

int
spec_link(vp, tdvp, tnm, cred)
	struct vnode *vp;
	struct vnode *tdvp;
	char *tnm;
	struct ucred *cred;
{
	register struct vnode *realvp;

	if ((realvp = VTOS(vp)->s_realvp) != NULL)
		return (VOP_LINK(realvp, tdvp, tnm, cred));
	else
		return (ENOENT);	/* can't link to something non-existent */
}

/*
 * In order to sync out the snode times without multi-client problems,
 * make sure the times written out are never earlier than the times
 * already set in the vnode.
 */
int
spec_fsync(vp, cred)
	struct vnode *vp;
	struct ucred *cred;
{
	register int error;
	register struct snode *sp;
	register struct vnode *realvp;
	struct vattr *vatmp;
	struct vattr *vap;

	sp = VTOS(vp);
	/*
	 * If times didn't change on a non-block
	 * special file, don't flush anything.
	 */
	if ((sp->s_flag & (SACC|SUPD|SCHG)) == 0 && vp->v_type != VBLK)
		return (0);

	/*
	 * If no real vnode to update, don't flush anything
	 */
	if ((realvp = sp->s_realvp) == NULL)
		return (0);

#if	MACH
	vatmp = (struct vattr *)kalloc(sizeof (*vatmp));
#else	MACH
	vatmp = (struct vattr *)kmem_alloc(sizeof (*vatmp));
#endif	MACH
	error = VOP_GETATTR(sp->s_realvp, vatmp, cred);
	if (error == 0) {
#if	MACH
		vap = (struct vattr *)kalloc(sizeof (*vap));
#else	MACH
		vap = (struct vattr *)kmem_alloc((u_int)sizeof (*vap));
#endif	MACH
		vattr_null(vap);
		vap->va_atime = timercmp(&vatmp->va_atime, &sp->s_atime, >) ?
		    vatmp->va_atime : sp->s_atime;
		vap->va_mtime = timercmp(&vatmp->va_mtime, &sp->s_mtime, >) ?
		    vatmp->va_mtime : sp->s_mtime;
		VOP_SETATTR(realvp, vap, cred);
#if	MACH
		kfree((caddr_t)vap, sizeof (*vap));
#else	MACH
		kmem_free((caddr_t)vap, sizeof (*vap));
#endif	MACH
	}
#if	MACH
	kfree((caddr_t)vatmp, sizeof (*vatmp));
#else	MACH
	kmem_free((caddr_t)vatmp, sizeof (*vatmp));
#endif	MACH
	(void) VOP_FSYNC(realvp, cred);
	return (0);
}

static int
spec_dump(vp, addr, bn, count)
	struct vnode *vp;
	caddr_t addr;
	int bn;
	int count;
{

	return ((*bdevsw[major(vp->v_rdev)].d_dump)
	    (vp->v_rdev, addr, bn, count));
}

static int
spec_noop()
{

	return (EINVAL);
}

#if	SUN_LOCK
/*
 * Record-locking requests are passed back to the real vnode handler.
 */
int
spec_lockctl(vp, ld, cmd, cred, clid)
	struct vnode *vp;
	struct flock *ld;
	int cmd;
	struct ucred *cred;
	int clid;
{
	register struct vnode *realvp;

	if ((realvp = VTOS(vp)->s_realvp) != NULL)
		return (VOP_LOCKCTL(realvp, ld, cmd, cred, clid));
	else
		return (EINVAL);	/* can't lock this, it doesn't exist */
}
#endif	SUN_LOCK

int
spec_fid(vp, fidpp)
	struct vnode *vp;
	struct fid **fidpp;
{
	register struct vnode *realvp;

	if ((realvp = VTOS(vp)->s_realvp) != NULL)
		return (VOP_FID(realvp, fidpp));
	else
		return (EINVAL);	/* you lose */
}

static int
spec_cmp(vp1, vp2)
	struct vnode *vp1, *vp2;
{

	return (vp1 == vp2);
}

int
spec_realvp(vp, vpp)
	struct vnode *vp;
	struct vnode **vpp;
{
	extern struct vnodeops fifo_vnodeops;
	struct vnode *rvp;

	if (vp &&
	    (vp->v_op == &spec_vnodeops || vp->v_op == &fifo_vnodeops)) {
		vp = VTOS(vp)->s_realvp;
	}
	if (vp && VOP_REALVP(vp, &rvp) == 0) {
		vp = rvp;
	}
	*vpp = vp;
	return (0);
}

static int
spec_strategy(bp)
	struct buf *bp;
{
	(*bdevsw[major(bp->b_vp->v_rdev)].d_strategy)(bp);
	return (0);
}


