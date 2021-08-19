/* 
 **********************************************************************
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 **********************************************************************
 * HISTORY
 * 22-Jun-90  Doug Mitchell at NeXT
 *	Added blocksize argument to physio(); allowed physio to kernel space.
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Removed dir.h
 *	Cleaned out SUN_VFS compiler switches.
 *
 * 16-Oct-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Correct IBM-RT SCSI disk conditionals.
 *	[ V5.1(XF19) ]
 *
 * 27-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Changed devices to vnodes.  NOTE: This has not been tested
 *		 because NeXT uses MACH_VM.
 *
 * 24-Aug-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Move vsunlock in physio out from under the splbio().  This can cause
 *	a deadlock if someone else has started to do a unwire, has the
 *	vm_page_queue_lock and is now doing a shoot, while we are at spl
 *	and trying to get the vm_page_queue_lock, are cpus_active and
 *	are not taking interrupts.
 *
 * 30-May-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Installed latest fix from IBM to constrain physical disk I/O
 *	length for SCSI drives (this should really be done in a per-
 *	driver routine).
 *	[ V5.1(XF11) ]
 *
 *  7-Jul-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Removed unnecessary include of pte.h
 *
 * 26-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 18-Dec-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	Upgraded from 4.1BSD.  Carried over change below:
 *
 * 17-Jul-84  Fil Alleva (faa) at Carnegie-Mellon University
 *	NDS:  Added code in physio() to preserve SPHYSIO bit in
 *	users process flags so that the DSC device will work correctly
 *	with other devices.
 *
 **********************************************************************
 */
/*	@(#)vm_swp.c	2.2 88/06/17 4.0NFSSRC SMI;	from UCB 7.1 6/5/86	*/
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/conf.h>
#import <sys/proc.h>
#import <sys/vm.h>
#import <sys/trace.h>
#import <sys/uio.h>
#import <sys/vnode.h>

#import <machine/spl.h>

#if	MACH
#else	MACH

/*
 * Swap IO headers -
 * They contain the necessary information for the swap I/O.
 * At any given time, a swap header can be in three
 * different lists. When free it is in the free list, 
 * when allocated and the I/O queued, it is on the swap 
 * device list, and finally, if the operation was a dirty 
 * page push, when the I/O completes, it is inserted 
 * in a list of cleaned pages to be processed by the pageout daemon.
 */
struct	buf *swbuf;

/*
 * swap I/O -
 *
 * If the flag indicates a dirty page push initiated
 * by the pageout daemon, we map the page into the i th
 * virtual page of process 2 (the daemon itself) where i is
 * the index of the swap header that has been allocated.
 * We simply initialize the header and queue the I/O but
 * do not wait for completion. When the I/O completes,
 * iodone() will link the header to a list of cleaned
 * pages to be processed by the pageout daemon.
 */
swap(p, dblkno, addr, nbytes, rdflg, flag, vp, pfcent)
	struct proc *p;
	swblk_t dblkno;
	caddr_t addr;
	int nbytes, rdflg, flag;
	struct vnode *vp;
	u_int pfcent;
{
	register struct buf *bp;
	register u_int c;
	int p2dp;
	register struct pte *dpte, *vpte;
	int s;
	extern swdone();
	int error = 0;

	s = splbio();
	while (bswlist.av_forw == NULL) {
		bswlist.b_flags |= B_WANTED;
		(void) sleep((caddr_t)&bswlist, PSWP+1);
	}
	bp = bswlist.av_forw;
	bswlist.av_forw = bp->av_forw;
	splx(s);

	bp->b_flags = B_BUSY | B_PHYS | rdflg | flag;
	if ((bp->b_flags & (B_DIRTY|B_PGIN)) == 0)
		if (rdflg == B_READ)
			sum.v_pswpin += btoc(nbytes);
		else
			sum.v_pswpout += btoc(nbytes);
	bp->b_proc = p;
	if (flag & B_DIRTY) {
		p2dp = ((bp - swbuf) * CLSIZE) * KLMAX;
		dpte = dptopte(&proc[2], p2dp);
		vpte = vtopte(p, btop(addr));
		for (c = 0; c < nbytes; c += NBPG) {
			if (vpte->pg_pfnum == 0 || vpte->pg_fod)
				panic("swap bad pte");
			*dpte++ = *vpte++;
		}
		bp->b_un.b_addr = (caddr_t)ctob(dptov(&proc[2], p2dp));
		bp->b_flags |= B_CALL;
		bp->b_iodone = swdone;
		bp->b_pfcent = pfcent;
	} else
		bp->b_un.b_addr = addr;
	while (nbytes > 0) {
		bp->b_bcount = nbytes;
		minphys(bp);
                bp->b_blkno = dblkno;
                bp->b_dev = vp->v_rdev;
                bsetvp(bp, vp);
		c = bp->b_bcount;
#ifdef TRACE
		trace(TR_SWAPIO, vp, bp->b_blkno);
#endif
                physstrat(bp, vp->v_op->vn_strategy, PSWP);
		if (flag & B_DIRTY) {
			if (c < nbytes)
				panic("big push");
			return (error);
		}
		bp->b_un.b_addr += c;
		bp->b_flags &= ~B_DONE;
		if (bp->b_flags & B_ERROR) {
			if ((flag & (B_UAREA|B_PAGET)) || rdflg == B_WRITE)
				panic("hard IO err in swap");
			swkill(p, "swap: read error from swap device");
			error = EIO;
		}
		nbytes -= c;
		dblkno += btodb(c);
	}
	s = splbio();
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_PAGET|B_UAREA|B_DIRTY);
	bp->av_forw = bswlist.av_forw;
	bswlist.av_forw = bp;
	if (bswlist.b_flags & B_WANTED) {
		bswlist.b_flags &= ~B_WANTED;
		wakeup((caddr_t)&bswlist);
		wakeup((caddr_t)&proc[2]);
	}
	splx(s);
	return (error);
}

/*
 * Put a buffer on the clean list after I/O is done.
 * Called from biodone.
 */
swdone(bp)
	register struct buf *bp;
{
	register int s;

	if (bp->b_flags & B_ERROR)
		panic("IO err in push");
	s = splbio();
	bp->av_forw = bclnlist;
	cnt.v_pgout++;
	cnt.v_pgpgout += bp->b_bcount / NBPG;
	bclnlist = bp;
	if (bswlist.b_flags & B_WANTED)
		wakeup((caddr_t)&proc[2]);
	splx(s);
}

/*
 * If rout == 0 then killed on swap error, else
 * rout is the name of the routine where we ran out of
 * swap space.
 */
swkill(p, rout)
	struct proc *p;
	char *rout;
{

	printf("pid %d: %s\n", p->p_pid, rout);
	uprintf("sorry, pid %d was killed in %s\n", p->p_pid, rout);
	/*
	 * To be sure no looping (e.g. in vmsched trying to
	 * swap out) mark process locked in core (as though
	 * done by user) after killing it so noone will try
	 * to swap it out.
	 */
	psignal(p, SIGKILL);
	p->p_flag |= SULOCK;
}

#endif	MACH
/*
 * Raw I/O. The arguments are
 *	The strategy routine for the device
 *	A buffer, which will always be a special buffer
 *	  header owned exclusively by the device for this purpose
 *	The device number
 *	Read/write flag
 * 	Block size
 * Essentially all the work is computing physical addresses and
 * validating them.
 * If the user has the proper access privileges, the process is
 * marked 'delayed unlock' and the pages involved in the I/O are
 * faulted and locked. After the completion of the I/O, the above pages
 * are unlocked.
 */
physio(strat, bp, dev, rw, mincnt, uio, blocksize)
	int (*strat)(); 
	register struct buf *bp;
	dev_t dev;
	int rw;
	unsigned (*mincnt)();
	struct uio *uio;
	int blocksize;		/* normally DEV_BSIZE */
{
	register struct iovec *iov;
	register int c;
	char *a;
	int s, error = 0;

nextiov:
	if (uio->uio_iovcnt == 0)
		return (0);
	iov = uio->uio_iov;
	if(uio->uio_segflg != UIO_SYSSPACE) {
		if (useracc(iov->iov_base, (u_int)iov->iov_len,
		    rw==B_READ?B_WRITE:B_READ) == NULL)
			return (EFAULT);
	}
	s = splbio();
	while (bp->b_flags&B_BUSY) {
		bp->b_flags |= B_WANTED;
		sleep((caddr_t)bp, PRIBIO+1);
	}
	splx(s);
	bp->b_error = 0;
	bp->b_proc = u.u_procp;
	bp->b_un.b_addr = iov->iov_base;
	while (iov->iov_len > 0) {
		bp->b_flags = B_BUSY | B_PHYS | rw;
		bp->b_dev = dev;
		if(blocksize == DEV_BSIZE)
			bp->b_blkno = btodb(uio->uio_offset);
		else
			bp->b_blkno = uio->uio_offset / blocksize;
		bp->b_bcount = iov->iov_len;
		(*mincnt)(bp);
		c = bp->b_bcount;
		if(uio->uio_segflg == UIO_SYSSPACE)
			bp->b_flags |= B_KERNSPACE;
		else {
			u.u_procp->p_flag |= SPHYSIO;
			vslock(a = bp->b_un.b_addr, c);
		}
		physstrat(bp, strat, PRIBIO);
#if	MACH
#else	MACH
		(void) splbio();
#endif	MACH
		if(uio->uio_segflg != UIO_SYSSPACE) {
			vsunlock(a, c, rw);
			u.u_procp->p_flag &= ~SPHYSIO;
		}
#if	MACH
		(void) splbio();
#else	MACH
#endif	MACH
		if (bp->b_flags&B_WANTED)
			wakeup((caddr_t)bp);
		splx(s);
		c -= bp->b_resid;
		bp->b_un.b_addr += c;
		iov->iov_len -= c;
		uio->uio_resid -= c;
		uio->uio_offset += c;
		/* temp kludge for tape drives */
		if (bp->b_resid || (bp->b_flags&B_ERROR))
			break;
	}
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS);
	error = geterror(bp);
	/* temp kludge for tape drives */
	if (bp->b_resid || error)
		return (error);
	uio->uio_iov++;
	uio->uio_iovcnt--;
	goto nextiov;
}

#define	MAXPHYS	(63 * 1024)

unsigned
minphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > MAXPHYS)
		bp->b_bcount = MAXPHYS;
}

