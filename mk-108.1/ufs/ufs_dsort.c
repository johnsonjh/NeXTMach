/* 
 **********************************************************************
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 **********************************************************************
 *
 * HISTORY
 * 28-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Prioritized disksort.
 *
 * 10-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Added support for procedural access to disk queue.
 */ 
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ufs_dsort.c	7.1 (Berkeley) 6/5/86
 */

/*
 * Seek sort for disks.  We depend on the driver
 * which calls us using b_sort_key as the current cylinder number.
 *
 * The argument dp structure holds a b_actf activity chain pointer
 * on which we keep two queues, sorted in ascending cylinder order.
 * The first queue holds those requests which are positioned after
 * the current cylinder (in the first request); the second holds
 * requests which came in after their cylinder number was passed.
 * Thus we implement a one way scan, retracting after reaching the
 * end of the drive to the first request on the second queue,
 * at which time it becomes the first queue.
 *
 * A one-way scan is natural because of the way UNIX read-ahead
 * blocks are allocated.
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/buf.h>
#if	NeXT
#import <sys/disksort.h>
#import <kern/assert.h>
#import <kern/kalloc.h>
#import <next/spl.h>
#import <kern/xpr.h>
#import <kern/sched.h>
#import <kern/thread.h>
#endif	NeXT

#if	NeXT
#else	NeXT
disksort(dp, bp)
	register struct buf *dp, *bp;
{
	register struct buf *ap;

	/*
	 * If nothing on the activity queue, then
	 * we become the only thing.
	 */
	ap = dp->b_actf;
	if(ap == NULL) {
		dp->b_actf = bp;
		dp->b_actl = bp;
		bp->av_forw = NULL;
		return;
	}
	/*
	 * If we lie after the first (currently active)
	 * request, then we must locate the second request list
	 * and add ourselves to it.
	 */
	if (bp->b_sort_key < ap->b_sort_key) {
		while (ap->av_forw) {
			/*
			 * Check for an ``inversion'' in the
			 * normally ascending cylinder numbers,
			 * indicating the start of the second request list.
			 */
			if (ap->av_forw->b_sort_key < ap->b_sort_key) {
				/*
				 * Search the second request list
				 * for the first request at a larger
				 * cylinder number.  We go before that;
				 * if there is no such request, we go at end.
				 */
				do {
					if (bp->b_sort_key <
						   ap->av_forw->b_sort_key)
						goto insert;
					ap = ap->av_forw;
				} while (ap->av_forw);
				goto insert;		/* after last */
			}
			ap = ap->av_forw;
		}
		/*
		 * No inversions... we will go after the last, and
		 * be the first request in the second request list.
		 */
		goto insert;
	}
	/*
	 * Request is at/after the current request...
	 * sort in the first request list.
	 */
	while (ap->av_forw) {
		/*
		 * We want to go after the current request
		 * if there is an inversion after it (i.e. it is
		 * the end of the first request list), or if
		 * the next request is a larger cylinder than our request.
		 */
		if (ap->av_forw->b_sort_key < ap->b_sort_key ||
		    bp->b_sort_key < ap->av_forw->b_sort_key)
			goto insert;
		ap = ap->av_forw;
	}
	/*
	 * Neither a second list nor a larger
	 * request... we go at the end of the first list,
	 * which is the same as the end of the whole schebang.
	 */
insert:
	bp->av_forw = ap->av_forw;
	ap->av_forw = bp;
	if (ap == dp->b_actl)
		dp->b_actl = bp;
}
#endif	NeXT

#if	NeXT
ds_call_t ds_call;

#ifdef	notdef
#define ds_dbg(a)	XPR(XPR_SCSI, a)
#else	notdef
#define ds_dbg(a)
#endif	notdef
#define DS_NBUCKET	20


/*
 * Check to see if there's a loaded disksort.  Call it if we've just
 * noticed that it's been added, and remove it if it's on it's way out.
 */
static void disksort_check_default(ds_queue_t *dsq)
{
	/*
	 * If a special mechanism's been set up, use it.
	 * Otherwise, call disksort.
	 */
	if (ds_call.available && !dsq->indirect) {
		/*
		 * Wait for the queue to empty before using
		 * the loaded version.
		 */
		if (queue_empty(&dsq->activeq)) {
			dsq->indirect = TRUE;
			
			/*
			 * Allocate data needed by this loaded disksort.
			 */
			(*ds_call.alloc)(dsq);
		}
	} else if (   !ds_call.available
		   && dsq->indirect
		   && !(*ds_call.first)(dsq))
	{
		/*
		 * The queue's empty and the interface is going away,
		 * go ahead and use the default version.
		 */
		dsq->indirect = FALSE;
		
		/*
		 * Deallocate data allocated by this loaded disksort.
		 */
		(*ds_call.free)(dsq);
	}
}

/*
 * Allocate a bucket.
 */
static inline ds_bucket_t *bucket_alloc(ds_queue_t *dsq)
{
	ds_bucket_t *new;

	if (dsq->navail == 0)
		return NULL;

	dsq->navail--;
	new = kalloc(sizeof *new);

	new->pri = 0;
	new->head = new->tail = NULL;
	new->sort_key = 0;

	return new;
}

/*
 * Free a bucket.
 */
static inline void bucket_free(ds_queue_t *dsq, ds_bucket_t *dsb)
{
	kfree(dsb, sizeof *dsb);
	dsq->navail++;
}

/*
 * Return the buffer pointer of last buffer having this sort_key or lower
 * or the last buffer before the insertion point
 * or NULL if insertion is at head of bucket's queue.
 */
static struct buf *insertion_point(ds_bucket_t *dsb, int sort_key)
{
	struct buf *bp = dsb->head;

	ASSERT(bp);

	/*
	 * If sort_key is greater than the head of the queue look for
	 * it's place in the first part of the queue.
	 */
	if (bp->b_sort_key < sort_key) {
		for (; bp->av_forw; bp = bp->av_forw) {
			if (bp->b_sort_key > bp->av_forw->b_sort_key)
				break;	// found end of queue
			if (bp->av_forw->b_sort_key > sort_key)
				break;	// found insertion point
		}
		
		/*
		 * Insertion point follows bp.
		 */
		ds_dbg(("bucket insertion after 0x%x\n", bp));
		return bp;
	}

	/*
	 * Look for the insertion point in the second part of the queue.
	 */
	if (bp->b_sort_key >= dsb->sort_key) {
		/*
		 * Goto the end of the first part of the queue.
		 */
		for (; bp->av_forw; bp = bp->av_forw)
			if (bp->b_sort_key > bp->av_forw->b_sort_key)
				break;
	} else {
		/*
		 * bp is the head of the second part of the queue.
		 * See if sort_key comes before this.
		 */
		if (bp->b_sort_key > sort_key) {
			/*
			 * Insertion point is at front of queue.
			 */
			ds_dbg(("no insertion point\n"));
			return NULL;
		}
	}

	/*
	 * bp->av_forw is the head of the second part of the queue.
	 */

	for (; bp->av_forw; bp = bp->av_forw)
		if (bp->av_forw->b_sort_key > sort_key)
			break;

	ds_dbg(("bucket insertion after 0x%x\n", bp));

	return bp;
}

/*
 * Sort this buffer into the bucket.
 * Bucket is sorted by it's sort_key.
 */
static void add_buf (
	ds_queue_t	*dsq,
	ds_bucket_t	*dsb,
	struct buf	*bp,
	int		busy)
{
	struct buf *ap;

	ASSERT(dsb->head);
	ds_dbg(("add buf 0x%x to bucket 0x% pri %d\n", bp, dsb, dsb->pri));

	if (   bp->b_sort_key > dsb->sort_key
	    && (   bp->b_sort_key < dsb->head->b_sort_key
		|| dsb->head->b_sort_key < dsb->sort_key))
	{
		ds_dbg(("buf placed at head of queue (1st part)"));
		bp->av_forw = dsb->head;
		dsb->head = bp;
		return;
	}

	ap = insertion_point(dsb, bp->b_sort_key);

	if (!ap) {
		ds_dbg(("buf placed at head of queue (2nd part)\n"));
		/*
		 * Put the request at the head of the queue.
		 */
		bp->av_forw = dsb->head;
		dsb->head = bp;
	} else {
		ds_dbg(("buf placed behind 0x%x\n", ap));
		bp->av_forw = ap->av_forw;
		ap->av_forw = bp;
		if (dsb->tail == ap)
			dsb->tail = bp;
	}
}

/*
 * Allocate a new bucket and insert it following the specified bucket.
 */
static ds_bucket_t *add_new(ds_queue_t *dsq, ds_bucket_t *dsb, struct buf *bp)
{
	ds_bucket_t *new;

	new = bucket_alloc(dsq);
	ASSERT(new);
	new->head = new->tail = bp;
	bp->av_forw = NULL;

	if (queue_end(&dsq->activeq, (queue_t)dsb)) {
		ds_dbg(("buf 0x%x in new bucket 0x%x at queue head\n",
			bp, new));
		queue_enter_first(&dsq->activeq, new, ds_bucket_t *, link);
	} else if (queue_end(&dsq->activeq, dsb->link.next)) {
		ds_dbg(("buf 0x%x in new bucket 0x%x at queue tail\n",
			bp, new));
		queue_enter(&dsq->activeq, new, ds_bucket_t *, link);
	} else {
		ds_dbg(("buf 0x%x in new bucket 0x%x after bucket 0x%x\n",
			bp, new, dsb));
		new->link.prev = (queue_t)dsb;
		new->link.next = dsb->link.next;
		dsb->link.next = (queue_t)new;
		((ds_bucket_t *)(new->link.next))->link.prev = (queue_t)new;
	}
	return new;
}

/*
 * Reorder an already ordered bucket based on sort_key.
 */
static void reorder_bucket(ds_bucket_t *dsb, int sort_key)
{
	struct buf *bp = dsb->head;

	ASSERT(dsb->head);

	if ((bp = insertion_point(dsb, sort_key-1)) && bp->av_forw) {
		/*
		 * Make the buffer after bp (if any) be the head
		 * of the bucket's queue.
		 */
		ds_dbg(("bucket reordered, head now 0x%x\n", bp->av_forw));
		dsb->tail->av_forw = dsb->head;
		dsb->tail = bp;
		dsb->head = bp->av_forw;
		bp->av_forw = NULL;
	}
	dsb->sort_key = sort_key;
}

/*
 * Add this request to the queue.
 * Requests are placed in buckets.  Each bucket has a different priority.
 */
static void ds_enter_common(ds_queue_t *dsq, struct buf *bp)
{
	ds_bucket_t *new, *cur = NULL;
	int pri = bp->b_rtpri;
	int s;

	ds_dbg(("enter buf 0x%x in queue 0x%x with pri %d\n", bp, dsq, pri));
	s = spldma();
	simple_lock(&dsq->lock);

	if (queue_empty(&dsq->activeq)) {
		ASSERT(!dsq->busy);
		new = add_new(dsq, (ds_bucket_t *)&dsq->activeq, bp);
		new->pri = pri;
		new->sort_key = dsq->last_loc;
		simple_unlock(&dsq->lock);
		splx(s);
		return;
	}

	/*
	 * Search for a bucket with this priority and insert the buffer
	 * in there if it exists.
	 */
	cur = (ds_bucket_t *)queue_first(&dsq->activeq);
	if (dsq->busy)
		/*
		 * Ensure that the sort_key for this buffer is always
		 * the sort_key of the busy buffer.
		 */
		cur->sort_key = cur->head->b_sort_key;

	if (dsq->busy && cur->pri < pri && dsq->navail > 0) {
		ds_bucket_t *new;
		struct buf *ap;

		/*
		 * Make the active buffer the same priority as this buffer
		 * and put it in it's own bucket at the head of the queue.
		 */
		if (cur->head != cur->tail) {
			ap = cur->head;
			cur->head = ap->av_forw;
			new = add_new(dsq, (ds_bucket_t *)&dsq->activeq, ap);
			new->pri = pri;
			new->sort_key = ap->b_sort_key;
			cur = new;
		} else
			cur->pri = pri;
	}

	/*
	 * Find the bucket to insert this request to, or the next lower
	 * priority bucket if none exists.
	 */
	while (!queue_end((queue_t)cur, &dsq->activeq) && cur->pri > pri)
		cur = (ds_bucket_t *)queue_next(&cur->link);

	/*
	 * If we've reached the end of the queue and haven't found the right
	 * bucket and can't allocate a bucket, use the last bucket in
	 * the queue.
	 */
	if (queue_end(&dsq->activeq, (queue_t)cur) && dsq->navail == 0) {
		cur = (ds_bucket_t *)queue_last(&dsq->activeq);
		ds_dbg(("adding to last bucket 0x%x (no buckets available)\n",
			cur));
		add_buf(dsq, cur, bp, dsq->busy);
		simple_unlock(&dsq->lock);
		splx(s);
		return;
	}

	/*
	 * If there's no more available buckets, add the request to this
	 * bucket.
	 */
	if (dsq->navail == 0) {
		ds_dbg(("adding to bucket 0x%x (no buckets available)\n",
			cur));
		add_buf(dsq, cur, bp, dsq->busy);
		goto reorder;
	}
	
	if (queue_end(&dsq->activeq, (queue_t)cur)) {
		/*
		 * We got to the end of the queue without finding
		 * a bucket of equal or lower priority than this request.
		 * Add a new bucket to the end of the queue.
		 */
		cur = (ds_bucket_t *)queue_last(&dsq->activeq);
	}

	if (cur->pri < pri) {
		new = add_new(dsq, (ds_bucket_t *)queue_prev(&cur->link), bp);
		ds_dbg(("new bucket 0x%x before bucket 0x%x\n",
			new, cur));
		new->pri = pri;

		/*
		 * Get the sort_key for this bucket from the tail of the
		 * previous bucket (if it exists) or from the last location.
		 */
		cur = (ds_bucket_t *)queue_prev(&new->link);
		if (queue_end(&dsq->activeq, (queue_t)cur))
			new->sort_key = dsq->last_loc;
		else
			new->sort_key = cur->tail->b_sort_key;

		cur = new;
	} else if (cur->pri == pri) {
		/*
		 * Add this buffer to this bucket.
		 */
		add_buf(dsq, cur, bp, dsq->busy);
	} else /* cur->pri > pri */ {
		/*
		 * Add a new bucket after this bucket.
		 */
		new = add_new(dsq, cur, bp);
		ds_dbg(("new bucket 0x%x after bucket 0x%x\n",
			new, cur));
		new->pri = pri;
		new->sort_key = cur->tail->b_sort_key;
	}

    reorder:
	if (!queue_end(&dsq->activeq, (queue_t)cur)) {
		ds_bucket_t *next = (ds_bucket_t *)queue_next(&cur->link);

	    	while (!queue_end(&dsq->activeq, (queue_t)next)) {
			reorder_bucket(next, cur->tail->b_sort_key);
			cur = next;
			next = (ds_bucket_t *)queue_next(&next->link);
		}
	}

	simple_unlock(&dsq->lock);
	splx(s);
}

void disksort_enter(ds_queue_t *dsq, struct buf *bp)
{
	disksort_check_default(dsq);

	if (dsq->indirect)
		(*ds_call.enter)(dsq, bp);
	else {
		/*
		 * Pull the guy's prioirty out of the current thread.
		 */
		bp->b_rtpri = current_thread()->priority;
		ds_enter_common(dsq, bp);
	}
}

/*
 * Add this request to the head of the queue.  If the queue is busy, then
 * add it immediately behind the first element in the active bucket, otherwise
 * allocate a new bucket and add it in there.
 */
void disksort_enter_head(ds_queue_t *dsq, struct buf *bp)
{
	disksort_check_default(dsq);

	if (dsq->indirect)
		(*ds_call.enter_head)(dsq, bp);
	else {
		bp->b_rtpri = NRQS-1;	// Maximum priority
		ds_enter_common(dsq, bp);
	}
}

/*
 * Enter this request at the end of the queue.  If there's a bucket
 * available, allocate for the request and add it to the tail of the queue.
 * Otherwise, add it to the last element of the queue.
 */
void disksort_enter_tail(ds_queue_t *dsq, struct buf *bp)
{
	disksort_check_default(dsq);

	if (dsq->indirect)
		(*ds_call.enter)(dsq, bp);
	else {
		bp->b_rtpri = 0;	// Minimum priority
		ds_enter_common(dsq, bp);
	}
}

struct buf *disksort_first(ds_queue_t *dsq)
{
	ds_bucket_t *dsb;
	struct buf *bp;
	int s;

	disksort_check_default(dsq);

	/*
	 * If a special mechanism's been set up, use it.
	 * Otherwise, just return the first element in the queue.
	 */
	if (dsq->indirect)
		return (*ds_call.first)(dsq);

	s = spldma();
	simple_lock(&dsq->lock);
	if (queue_empty(&dsq->activeq)) {
		simple_unlock(&dsq->lock);
		splx(s);
		return NULL;
	}

	dsb = (ds_bucket_t *)queue_first(&dsq->activeq);
	bp = dsb->head;
	
	simple_unlock(&dsq->lock);
	splx(s);

	return bp;
}

struct buf *disksort_remove(ds_queue_t *dsq, struct buf *bp)
{
	ds_bucket_t *dsb;
	struct buf *cp;
	int s;

	disksort_check_default(dsq);

	/*
	 * If a special mechanism's been set up, use it.
	 */
	if (dsq->indirect)
		return (*ds_call.remove)(dsq, bp);

	s = spldma();
	simple_lock(&dsq->lock);

	if (queue_empty(&dsq->activeq)) {
		simple_unlock(&dsq->lock);
		splx(s);
		return NULL;
	}

	for (  dsb = (ds_bucket_t *)queue_first(&dsq->activeq)
	     ; !queue_end(&dsq->activeq, (queue_t)dsb)
	     ; dsb = (ds_bucket_t *)queue_next(&dsb->link))
	{
		cp = dsb->head;

		/*
		 * Check to see if it's the head of the bucket we're freeing.
		 */
		if (cp == bp) {
			dsb->head = cp->av_forw;

			/*
			 * If this is the first bucket, then the
			 * queue isn't busy anymore.
			 */
			if (   dsb
			    == (ds_bucket_t *)queue_first(&dsq->activeq))
			{
				dsq->busy = 0;
				dsq->last_loc = cp->b_sort_key;
			}
		} else {
			while (cp->av_forw && cp->av_forw != bp)
				cp = cp->av_forw;
			if (cp->av_forw == NULL)
				continue;
			cp->av_forw = bp->av_forw;
			if (cp->av_forw == NULL)
				dsb->tail = cp;
			cp = bp;
		}
		break;
	}

	/*
	 * If the queue isn't busy anymore we can check to go on to
	 * the next bucket.
	 */
	dsb = (ds_bucket_t *)queue_first(&dsq->activeq);
	for (  dsb = (ds_bucket_t *)queue_first(&dsq->activeq)
	     ; !dsq->busy && !queue_empty(&dsq->activeq) && dsb->head == NULL
	     ; dsb = (ds_bucket_t *)queue_first(&dsq->activeq))
	{
		/*
		 * Free up this bucket and resort the next bucket
		 * based on the current head location.
		 */
		queue_remove(&dsq->activeq, dsb, ds_bucket_t *, link);
		bucket_free(dsq, dsb);
	}

	simple_unlock(&dsq->lock);
	splx(s);

	return cp;
}

/*
 * Initialize anything that needs doing in the queue.
 */
void disksort_init(ds_queue_t *dsq)
{
	int i;

	bzero(dsq, sizeof(*dsq));
	disksort_check_default(dsq);
	queue_init(&dsq->activeq);
	dsq->navail = DS_NBUCKET;
	simple_lock_init(&dsq->lock);
}

/*
 * Free any storage that's been allocated.
 */
void disksort_free(ds_queue_t *dsq)
{
	if (dsq->indirect) {
		/*
		 * The queue's empty and the interface is going away,
		 * go ahead and use the default version.
		 */
		dsq->indirect = FALSE;

		/*
		 * Deallocate data allocated by this loaded disksort.
		 */
		if (ds_call.available)
			(*ds_call.free)(dsq);
	}
}
#endif	NeXT
