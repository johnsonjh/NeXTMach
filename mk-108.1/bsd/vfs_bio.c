/* 
 * HISTORY
 * 29-Mar-90  Morris Meyer (mmeyer) at NeXT
 *	Corrected spl's to reflect NFSSRC 4.0.
 *
 * 28-Mar-90  Brian Pinkerton at NeXT
 *	Added support for private buffers so we can read directly into a page.
 *
 *  8-Mar-90  Morris Meyer (mmeyer) at NeXT
 *	Fix for the freeing free frag panic.
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Removed dir.h.  Added Peter King's 
 *      statistics gathering code.
 *
 * 22-Jan-89  John Seamons (jks) at NeXT
 *	Allow a sync to be restricted to a particular device.
 *	This allows a single optical disk volume to be sync'ed
 *	without disturbing other, potentially uninserted, volumes
 *	which will cause needless disk swapping.
 *
 * 12-Sep-88  Peter King (king) at NeXT
 *	Added support for private data.
 *
 * 12-Sep-88  Avadis Tevanian (avie) at NeXT
 *	spl6() -> splbio().
 *
 * 26-Oct-87  Peter King (king) at NeXT
 *	Original Sun source, upgraded to Mach
 */ 

#import <machine/spl.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/conf.h>
#import <sys/proc.h>
#import <sys/vm.h>
#import <sys/trace.h>
#import <sys/vnode.h>
#if	NeXT
#import <xpr_debug.h>
#import <kern/xpr.h>
#import <kern/thread.h>
#import <kern/task.h>

#import <kern/parallel.h>
#import <vm/vm_page.h>

struct bstats {
	int	n_bread;
	int	n_bread_hits;
	int	n_breada;
	int	n_breada_hits1;
	int	n_breada_hits2;
} bstats;
#endif	NeXT

/*
 * Read in (if necessary) the block and return a buffer pointer.
 */
struct buf *
bread(vp, blkno, size)
	struct vnode *vp;
	daddr_t blkno;
	int size;
{
	register struct buf *bp;

	bstats.n_bread++;
	if (size == 0)
		panic("bread: size 0");
	bp = getblk(vp, blkno, size);
	if (bp->b_flags&B_DONE) {
		trace(TR_BREADHIT, vp, blkno);
		bstats.n_bread_hits++;
		return (bp);
	}
	bp->b_flags |= B_READ;
	if (bp->b_bcount > bp->b_bufsize)
		panic("bread");
	VOP_STRATEGY(bp);
	trace(TR_BREADMISS, vp, blkno);
	u.u_ru.ru_inblock++;		/* pay for read */
	biowait(bp);
	return (bp);
}

/*
 * Read in the block, like bread, but also start I/O on the
 * read-ahead block (which is not allocated to the caller)
 */
struct buf *
breada(vp, blkno, size, rablkno, rabsize)
	struct vnode *vp;
	daddr_t blkno; int size;
	daddr_t rablkno; int rabsize;
{
	register struct buf *bp, *rabp;

	bstats.n_breada++;
	bp = NULL;
	/*
	 * If the block isn't in core, then allocate
	 * a buffer and initiate i/o (getblk checks
	 * for a cache hit).
	 */
	if (!incore(vp, blkno)) {
		bp = getblk(vp, blkno, size);
		if ((bp->b_flags&B_DONE) == 0) {
			bp->b_flags |= B_READ;
			if (bp->b_bcount > bp->b_bufsize)
				panic("breada");
			VOP_STRATEGY(bp);
			trace(TR_BREADMISS, vp, blkno);
			u.u_ru.ru_inblock++;		/* pay for read */
		} else {
			trace(TR_BREADHIT, vp, blkno);
			bstats.n_breada_hits1++;
		}
	}

	/*
	 * If there's a read-ahead block, start i/o
	 * on it also (as above).
	 */
	if (rablkno && !incore(vp, rablkno)) {
		rabp = getblk(vp, rablkno, rabsize);
		if (rabp->b_flags & B_DONE) {
			brelse(rabp);
			trace(TR_BREADHITRA, vp, blkno);
			bstats.n_breada_hits2++;
		} else {
			rabp->b_flags |= B_READ|B_ASYNC;
			if (rabp->b_bcount > rabp->b_bufsize)
				panic("breadrabp");
			VOP_STRATEGY(rabp);
			trace(TR_BREADMISSRA, vp, rablock);
			u.u_ru.ru_inblock++;		/* pay in advance */
		}
	}

	/*
	 * If block was in core, let bread get it.
	 * If block wasn't in core, then the read was started
	 * above, and just wait for it.
	 */
	if (bp == NULL)
		return (bread(vp, blkno, size));
	biowait(bp);
	return (bp);
}


/*
 *  Start a read on a vnode block.  Instead of trying to grab a buffer from the
 *  buffer pool, we use a private buffer header, and attach the physical memory
 *  directly to the buffer.  We mark this buffer private, and enter it in the
 *  BUFHASH data structure.  Others reading the same block will see the buffer,
 *  wait for IO to complete, then retry with their own buffer.
 */
vnStartRead(vp, page, blkno, size, bp)
    struct vnode *vp;
    vm_page_t page;
    daddr_t blkno;
    int size;
    struct buf *bp;
{
    struct buf *hashHeader;
    
    /* Start by filling in the buffer structure for the read */
    bp->b_vp = 0;
    bsetvp(bp, vp);
    bp->b_dev = vp->v_rdev;
    bp->b_flags = B_READ | B_PRIVATE;
    bp->b_blkno = blkno;
    bp->b_bufsize = page_size;
    bp->b_bcount = bp->b_bufsize;
    bp->b_error = 0;
    bp->b_resid = 0;
    bp->b_un.b_addr = (caddr_t) VM_PAGE_TO_PHYS(page);
    bp->b_rtpri = RTPRI_NONE;
    
    u.u_ru.ru_inblock++;		/* pay for read */
    
    hashHeader = BUFHASH(vp, blkno);
    binshash(bp, hashHeader);

    VOP_STRATEGY(bp);			/* start IO on first page */
}


/*
 *  Clean up a private buffer.  We just remove it from the BUFHASH chain, and
 *  release its vnode.
 */
vnCleanBuffer(bp)
    struct buf *bp;
{
    /* make sure nobody else has grabbed the buffer */
    assert(bp->b_flags == B_PRIVATE|B_READ|B_DONE);
    
    bremhash(bp);
    brelvp(bp);
}


/*
 *  Start read-ahead (though the buffer cache) on rablk with the given size.
 */
vnReadAhead(vp, rablkno, rabsize)
	struct vnode *vp;
	daddr_t rablkno; int rabsize;
{
    register struct buf *bp, *rabp;

    /*
     * If there's a read-ahead block, start i/o
     * on it also (as above).
     */
    if (rablkno && !incore(vp, rablkno)) {
	    rabp = getblk(vp, rablkno, rabsize);
	    if (rabp->b_flags & B_DONE) {
		    brelse(rabp);
		    trace(TR_BREADHITRA, vp, blkno);
	    } else {
		    rabp->b_flags |= B_READ|B_ASYNC;
		    if (rabp->b_bcount > rabp->b_bufsize)
			    panic("breadrabp");
		    VOP_STRATEGY(rabp);
		    trace(TR_BREADMISSRA, vp, rablock);
		    u.u_ru.ru_inblock++;		/* pay in advance */
	    }
    }
}


/*
 *  breadDirect
 *
 *  Purpose: read a page out of a vnode directly into a page.  If the page is
 *           resident in the buffer cache, copy the data out of the buffer
 *	     cache.  In any case, we start read ahead.
 *
 *	     Currently the read-ahead block ends up in the buffer cache, because
 *	     there is no page in which to place it.  Eventually, though, we will
 *	     read directly into some physical page for the read-ahead block.
 *
 *  Returns: KERN_SUCCESS if success, KERN_FAILURE if no success.
 *
 *  NB: This won't work on a multi-processor right now.  To ensure that it does
 *      work, we need to lock the buffer cache while we check to see if a block
 *      is present and while we put it on the queue.  Ultimately, a different
 *	data structure would be nice.
 *
 */
int
breadDirect(vp, page, blkno, blksize, numBytes, rablkno, rasize, error)
	struct vnode *vp;			/* vnode from which to read page */
	vm_page_t page;				/* page in which to place data */
	daddr_t blkno;				/* block in vnode to read */
	int blksize;				/* num of bytes to actually read */
	int numBytes;				/* num of bytes to copy to page */
	daddr_t rablkno;			/* readahead block number */
	int rasize;				/* readahead block size */
	int *error;
{
	struct buf myBuf;
	struct buf *bp;

	myBuf.b_vp = (struct vnode *) 0;
	
	if (incore(vp, blkno)) {
		bp = breada(vp, blkno, blksize, rablkno, rasize);
		if ((bp->b_flags & B_ERROR) == 0) {
		    copy_to_phys(bp->b_un.b_addr, VM_PAGE_TO_PHYS(page), numBytes);
		    *error = 0;
		    brelse(bp);
		    return blksize - bp->b_resid;
		}
		brelse(bp);
		*error = bp->b_error;
		return 0;
	}
	
	/*
	 *  get the first block on the disk queue, start read ahead if desired,
	 *  and wait for first block.
	 */
	vnStartRead(vp, page, blkno, blksize, &myBuf);
	
	/*
	 *  Start read ahead, then wait for the initial read
	 */
	vnReadAhead(vp, rablkno, rasize);

	biowait(&myBuf);
	
	if (myBuf.b_flags & B_ERROR) {
		*error = myBuf.b_error;
		numBytes = blksize - myBuf.b_resid;
		vnCleanBuffer(&myBuf);
		return numBytes;
	} else {
		vnCleanBuffer(&myBuf);
		*error = 0;
		return blksize - myBuf.b_resid;
	}
}


/*
 * Write the buffer, waiting for completion.
 * Then release the buffer.
 */
bwrite(bp)
	register struct buf *bp;
{
	register flag;

	flag = bp->b_flags;
	bp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI);
	if ((flag&B_DELWRI) == 0)
		u.u_ru.ru_oublock++;		/* noone paid yet */
	trace(TR_BWRITE, bp->b_vp, bp->b_blkno);
	if (bp->b_bcount > bp->b_bufsize)
		panic("bwrite");
	VOP_STRATEGY(bp);

	/*
	 * If the write was not synchronous, then await i/o completion.
	 * If the write was "delayed", then we put the buffer on
	 * the q of blocks awaiting i/o completion status.
	 */
	if ((flag&B_ASYNC) == 0) {
		biowait(bp);
		brelse(bp);
	} else if (flag & B_DELWRI)
		bp->b_flags |= B_AGE;
}

/*
 * Release the buffer, marking it so that if it is grabbed
 * for another purpose it will be written out before being
 * given up (e.g. when writing a partial block where it is
 * assumed that another write for the same block will soon follow).
 * This can't be done for magtape, since writes must be done
 * in the same order as requested.
 */
bdwrite(bp)
	register struct buf *bp;
{

	if ((bp->b_flags&B_DELWRI) == 0)
		u.u_ru.ru_oublock++;		/* noone paid yet */
#ifdef notdef
	/*
	 * This does not work for buffers associated with
	 * vnodes that are remote - they have no dev.
	 * Besides, we don't use bio with tapes, so rather
	 * than develop a fix, we just ifdef this out for now.
	 */
	if (bdevsw[major(bp->b_dev)].d_flags & B_TAPE)
		bawrite(bp);
	else {
		bp->b_flags |= B_DELWRI | B_DONE;
		brelse(bp);
	}
#endif
	bp->b_flags |= B_DELWRI | B_DONE;
	brelse(bp);
}

/*
 * Release the buffer, start I/O on it, but don't wait for completion.
 */
bawrite(bp)
	register struct buf *bp;
{

	bp->b_flags |= B_ASYNC;
	bwrite(bp);
}

/*
 * Release the buffer, with no I/O implied.
 */
brelse(bp)
	register struct buf *bp;
{
	register struct buf *flist;
	register s;

	/*
	 * If someone's waiting for the buffer, or
	 * is waiting for a buffer wake 'em up.
	 */
	if (bp->b_flags&B_WANTED)
		wakeup((caddr_t)bp);
	if (bfreelist[0].b_flags&B_WANTED) {
		bfreelist[0].b_flags &= ~B_WANTED;
		wakeup((caddr_t)bfreelist);
	}
	if (bp->b_flags & B_NOCACHE) {
		bp->b_flags |= B_INVAL;
	}
	if (bp->b_flags&B_ERROR)
		if (bp->b_flags & B_LOCKED)
			bp->b_flags &= ~B_ERROR;	/* try again later */
		else
			brelvp(bp);

	/*
	 * Stick the buffer back on a free list.
	 */
	s = splhigh();
	if (bp->b_bufsize <= 0) {
		/* block has no buffer ... put at front of unused buffer list */
		flist = &bfreelist[BQ_EMPTY];
		binsheadfree(bp, flist);
	} else if (bp->b_flags & (B_ERROR|B_INVAL)) {
		/* block has no info ... put at front of most free list */
		flist = &bfreelist[BQ_AGE];
		binsheadfree(bp, flist);
	} else {
		if (bp->b_flags & B_LOCKED)
			flist = &bfreelist[BQ_LOCKED];
/*		else if (bp->b_flags & B_AGE)
			flist = &bfreelist[BQ_AGE];*/
		else
			flist = &bfreelist[BQ_LRU];
		binstailfree(bp, flist);
	}
	bp->b_flags &= ~(B_WANTED|B_BUSY|B_ASYNC|B_AGE|B_NOCACHE);
	(void) splx(s);
}

/*
 * See if the block is associated with some buffer
 * (mainly to avoid getting hung up on a wait in breada)
 */
incore(vp, blkno)
	struct vnode *vp;
	daddr_t blkno;
{
	register struct buf *bp;
	register struct buf *dp;

	dp = BUFHASH(vp, blkno);
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw)
		if (bp->b_blkno == blkno && bp->b_vp == vp &&
		    (bp->b_flags & B_INVAL) == 0)
			return (1);
	return (0);
}

struct buf *
baddr(vp, blkno, size)
	struct vnode *vp;
	daddr_t blkno;
	int size;
{

	if (incore(vp, blkno))
		return (bread(vp, blkno, size));
	return (0);
}

/*
 * Assign a buffer for the given block.  If the appropriate
 * block is already associated, return it; otherwise search
 * for the oldest non-busy buffer and reassign it.
 *
 * We use splx here because this routine may be called
 * on the interrupt stack during a dump, and we don't
 * want to lower the ipl back to 0.
 */
struct buf *
getblk(vp, blkno, size)
	struct vnode *vp;
	daddr_t blkno;
	int size;
{
	register struct buf *bp, *dp;
	int s;

	if (vp == (struct vnode *)0) {
		printf("vp=0x%x, blkno=0x%x, size=0x%x\n", vp, blkno, size);
		panic("getblk: Illegal vnode pointer");
	}
	if ((unsigned)blkno >= 1 << (sizeof(int)*NBBY-DEV_BSHIFT))    /* XXX */
		blkno = 1 << ((sizeof(int)*NBBY-DEV_BSHIFT) + 1);
	/*
	 * Search the cache for the block.  If we hit, but
	 * the buffer is in use for i/o, then we wait until
	 * the i/o has completed.
	 */
	dp = BUFHASH(vp, blkno);
loop:
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		if (bp->b_blkno != blkno || bp->b_vp != vp ||
		    bp->b_flags&B_INVAL)
			continue;
		s = splhigh();
		if (bp->b_flags&B_BUSY) {
			bp->b_flags |= B_WANTED;
			(void) sleep((caddr_t)bp, PRIBIO+1);
			(void) splx(s);
			goto loop;
		}
		(void) splx(s);
		notavail(bp);
		if (bp->b_bcount != size && brealloc(bp, size) == 0)
			goto loop;
		bp->b_flags |= B_CACHE;
		return (bp);
	}
	bp = getnewbuf();
	bfree(bp);
	bremhash(bp);
	bsetvp(bp, vp);
	bp->b_dev = vp->v_rdev;
	bp->b_blkno = blkno;
	bp->b_error = 0;
	bp->b_resid = 0;
	binshash(bp, dp);
	if (brealloc(bp, size) == 0)
		goto loop;
	return (bp);
}

/*
 * get an empty block,
 * not assigned to any particular device
 */
struct buf *
geteblk(size)
	int size;
{
	register struct buf *bp, *flist;

	if (size > MAXBSIZE)
		panic("geteblk: size too big");
loop:
	bp = getnewbuf();
	bp->b_flags |= B_INVAL;
	bfree(bp);
	bremhash(bp);
	flist = &bfreelist[BQ_AGE];
	brelvp(bp);
	bp->b_error = 0;
	bp->b_resid = 0;
	binshash(bp, flist);
	if (brealloc(bp, size) == 0)
		goto loop;
	return (bp);
}

/*
 * Allocate space associated with a buffer.
 * If can't get space, buffer is released
 */
brealloc(bp, size)
	register struct buf *bp;
	int size;
{
	daddr_t start, last;
	register struct buf *ep;
	struct buf *dp;
	int s;

	/*
	 * First need to make sure that all overlaping previous I/O
	 * is dispatched with.
	 */
	if (size == bp->b_bcount)
		return (1);
#if	NeXT
/*
 * Note the following if statement has been moved BEFORE the size < check
 * This causes delayed block writes to NOT happen when the requested
 * size to write out is less than a full buffer's worth. In other words
 * this looses data blocks. This bug is present in the 4.3 BSD & 4.3 Tahoe
 * file system code and was causing the freeing free frag bug.
 */
	if (bp->b_flags & B_DELWRI) {
		bwrite(bp);
		return (0);
	}
  	if (size < bp->b_bcount) { 
		if (bp->b_flags & B_LOCKED)
			panic("brealloc");
		return (allocbuf(bp, size));
	}
#else		
	if (size < bp->b_bcount) { 
		if (bp->b_flags & B_DELWRI) {
			bwrite(bp);
			return (0);
		}
		if (bp->b_flags & B_LOCKED)
			panic("brealloc");
		return (allocbuf(bp, size));
	}
#endif	NeXT
	bp->b_flags &= ~B_DONE;
	if (bp->b_vp == (struct vnode *) 0)
		return (allocbuf(bp, size));

	/*
	 * Search cache for any buffers that overlap the one that we
	 * are trying to allocate. Overlapping buffers must be marked
	 * invalid, after being written out if they are dirty. (indicated
	 * by B_DELWRI) A disk block must be mapped by at most one buffer
	 * at any point in time. Care must be taken to avoid deadlocking
	 * when two buffer are trying to get the same set of disk blocks.
	 */
	start = bp->b_blkno;
	last = start + btodb(size) - 1;
	dp = BUFHASH(bp->b_vp, bp->b_blkno);
loop:
	for (ep = dp->b_forw; ep != dp; ep = ep->b_forw) {
		if (ep == bp || ep->b_vp != bp->b_vp || (ep->b_flags&B_INVAL))
			continue;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
		    ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		s = splhigh();
		if (ep->b_flags&B_BUSY) {
			ep->b_flags |= B_WANTED;
			(void) sleep((caddr_t)ep, PRIBIO+1);
			(void) splx(s);
			goto loop;
		}
		(void) splx(s);
		notavail(ep);
		if (ep->b_flags & B_DELWRI) {
			bwrite(ep);
			goto loop;
		}
		ep->b_flags |= B_INVAL;
		brelse(ep);
	}
	return (allocbuf(bp, size));
}
  
/*
 * Find a buffer which is available for use.
 * Select something from a free list.
 * Preference is to AGE list, then LRU list.
 */
struct buf *
getnewbuf()
{
	register struct buf *bp, *dp;
	int s;

loop:
	s = splhigh();
	for (dp = &bfreelist[BQ_AGE]; dp > bfreelist; dp--)
		if (dp->av_forw != dp)
			break;
	if (dp == bfreelist) {		/* no free blocks */
		dp->b_flags |= B_WANTED;
		(void) sleep((caddr_t)dp, PRIBIO+1);
		(void) splx(s);
		goto loop;
	}
	(void) splx(s);
	bp = dp->av_forw;
	notavail(bp);
	if (bp->b_flags & B_DELWRI) {
		bp->b_flags |= B_ASYNC;
		bwrite(bp);
		goto loop;
	}
	brelvp(bp);
	trace(TR_BRELSE, bp->b_vp, bp->b_blkno);
	bp->b_flags = B_BUSY;
	return (bp);
}

#if	NeXT
/*
 * Return a count of the number of buffers which are available for use.
 */
getnewbuf_count()
{
	register struct buf *bp, *dp;
	int s, count = 0;

	s = splbio();
	for (dp = &bfreelist[BQ_AGE]; dp > bfreelist; dp--) {
		bp = dp->av_forw;
		while (bp != dp) {
			bp = bp->av_forw;
			count++;
		}
	}
	(void) splx(s);
	return (count);
}
#endif	NeXT

/*
 * Wait for I/O completion on the buffer; return errors
 * to the user.
 */
biowait(bp)
	register struct buf *bp;
{
	int s;

	s = splhigh();
	while ((bp->b_flags&B_DONE)==0)
		(void) sleep((caddr_t)bp, PRIBIO);
	(void) splx(s);
	if (u.u_error == 0)			/* XXX */
		u.u_error = geterror(bp);
}

/*
 * Mark I/O complete on a buffer.
 * If someone should be called, e.g. the pageout
 * daemon, do so.  Otherwise, wake up anyone
 * waiting for it.
 */
biodone(bp)
	register struct buf *bp;
{

	if (bp->b_flags & B_DONE)
		panic("dup biodone");
	bp->b_flags |= B_DONE;
	if (bp->b_flags & B_CALL) {
		bp->b_flags &= ~B_CALL;
		(*bp->b_iodone)(bp);
		return;
	}
	if (bp->b_flags&B_ASYNC)
		brelse(bp);
	else {
		bp->b_flags &= ~B_WANTED;
		wakeup((caddr_t)bp);
	}
}

/*
 * Insure that no part of a specified block is in an incore buffer.
 */
blkflush(vp, blkno, size)
	struct vnode *vp;
	daddr_t blkno;
	u_long size;
{
	register struct buf *ep;
	struct buf *dp;
	daddr_t start, last;
	int s;

	start = blkno;
	last = start + btodb(size) - 1;
	dp = BUFHASH(vp, blkno);
loop:
	for (ep = dp->b_forw; ep != dp; ep = ep->b_forw) {
		if (ep->b_vp != vp || (ep->b_flags&B_INVAL))
			continue;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
		    ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		s = splhigh();
		if (ep->b_flags&B_BUSY) {
			ep->b_flags |= B_WANTED;
			(void) sleep((caddr_t)ep, PRIBIO+1);
			(void) splx(s);
			goto loop;
		}
		if (ep->b_flags & B_DELWRI) {
			(void) splx(s);
			notavail(ep);
			bwrite(ep);
			goto loop;
		}
		(void) splx(s);
	}
}

/*
 * Make sure all write-behind blocks
 * associated with vp (or the whole cache if vp == 0)
 * are flushed out.
 * (from sync)
 */
#if	NeXT
bflush(vp, dev, mask)
	struct vnode *vp;
	dev_t dev, mask;
#else	NeXT
bflush(vp)
	struct vnode *vp;
#endif	NeXT
{
	register struct buf *bp;
	register struct buf *flist;
	int s;

loop:
	s = splhigh();
	for (flist = bfreelist; flist < &bfreelist[BQ_EMPTY]; flist++)
	for (bp = flist->av_forw; bp != flist; bp = bp->av_forw) {
#if	NeXT
		if (dev != NODEV && dev != (bp->b_dev & mask))
			continue;
#endif	NeXT
		if ((bp->b_flags & B_DELWRI) == 0)
			continue;
		if (vp == bp->b_vp || vp == (struct vnode *) 0) {
			bp->b_flags |= B_ASYNC;
			notavail(bp);
			(void) splx(s);
			bwrite(bp);
			goto loop;
		}
	}
	(void) splx(s);
}

/*
 * Invalidate blocks associated with vp which are on the freelist.
 * Make sure all write-behind blocks associated with vp are flushed out.
 */
binvalfree(vp)
	struct vnode *vp;
{
	register struct buf *bp;
	register struct buf *flist;
	int s;

loop:
	s = splhigh();
	for (flist = bfreelist; flist < &bfreelist[BQ_EMPTY]; flist++)
	for (bp = flist->av_forw; bp != flist; bp = bp->av_forw) {
		if (vp == bp->b_vp || vp == (struct vnode *) 0) {
			if (bp->b_flags & B_DELWRI) {
				bp->b_flags |= B_ASYNC;
				notavail(bp);
				(void) splx(s);
				bwrite(bp);
			} else {
				bp->b_flags |= B_INVAL;
				brelvp(bp);
				(void) splx(s);
			}
			goto loop;
		}
	}
	(void) splx(s);
}

/*
 * Pick up the device's error number and pass it to the user;
 * if there is an error but the number is 0 set a generalized
 * code.  Actually the latter is always true because devices
 * don't yet return specific errors.
 */
geterror(bp)
	register struct buf *bp;
{
	int error = 0;

	if (bp->b_flags&B_ERROR)
		if ((error = bp->b_error)==0)
			return (EIO);
	return (error);
}

/*
 * Invalidate in core blocks belonging to closed or umounted filesystem
 *
 * This is not nicely done at all - the buffer ought to be removed from the
 * hash chains & have its dev/blkno fields clobbered, but unfortunately we
 * can't do that here, as it is quite possible that the block is still
 * being used for i/o. Eventually, all disc drivers should be forced to
 * have a close routine, which ought ensure that the queue is empty, then
 * properly flush the queues. Until that happy day, this suffices for
 * correctness.						... kre
 * This routine assumes that all the buffers have been written.
 */
binval(vp)
	struct vnode *vp;
{
	register struct buf *bp;
	register struct bufhd *hp;
#define dp ((struct buf *)hp)

loop:
	for (hp = bufhash; hp < &bufhash[BUFHSZ]; hp++)
		for (bp = dp->b_forw; bp != dp; bp = bp->b_forw)
			if (bp->b_vp == vp && (bp->b_flags & B_INVAL) == 0) {
				bp->b_flags |= B_INVAL;
				brelvp(bp);
				goto loop;
			}
}

bsetvp(bp, vp)
	struct buf *bp;
	struct vnode *vp;
{
	if (bp->b_vp) {
		brelvp(bp);
	}
	VN_HOLD(vp);
	bp->b_vp = vp;
}

brelvp(bp)
	struct buf *bp;
{
	struct vnode *vp;

	if (bp->b_vp == (struct vnode *) 0) {
		return;
	}
	vp = bp->b_vp;		/* save vp because VN_RELE may sleep */
	bp->b_vp = (struct vnode *) 0;
	VN_RELE(vp);
}








