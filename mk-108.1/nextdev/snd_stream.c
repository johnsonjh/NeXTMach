/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 * 25-Sep-90  Gregg Kellogg (gk) at NeXT
 *	Made pausing a stream work for streams that are linked.
 *
 * 28-Aug-90  Gregg Kellogg (gk) at NeXT
 *	When aborting a region deactivate pages starting from the page
 *	containing the rw_head, not the page after that.  This fixes
 *	a vm leak we've had for a long time.
 *
 *  1-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Allocate dma descriptors dynamically based on ndma field in stream.
 *	This value is initialized to 4.
 *
 * 29-May-90  Gregg Kellogg (gk) at NeXT, Inc.
 *	Fixed race condition in snd_split_region in which data could be
 *	lost if data needed to be copied between two different memory
 *	addresses.
 *
 *  2-Sep-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	Working around a VM bug.  If the memory object we're playing
 *	out of gets modified on us we need to cleanly abort the region.
 *	Check the return from pmap_resident_extract() to ensure that
 *	a non-zero page is returned.  If it's not, the abort the
 *	region.
 *
 * 24-Jul-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	Fixed a bug in the abort-region portion of the stream thread.
 *	After deactivating memory next_to_lock had been set to the end of
 *	the region, it should be set to the rw_head.
 *
 * 25-Jun-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	fixed a bug in which snd_stream_abort was returning a multiple
 *	reply to this RPC.
 *
 * 12-Jun-88  Gregg Kellogg (gk) at NeXT
 *	Cleaned up problems with stream thread synchronization
 *	and recognition of region termination.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <sound.h>
#if	NSOUND > 0

/*
 * High level raw sound device access.
 *
 * These routines implement the high level access needed for task level
 * access to the raw sound device.  This is used to bypass DSP access
 * and control system wide resources.
 *
 * At this level two queues exist for playing (recording into) regions of
 * virtual memory.
 *
 * Functions at this level:
 *	Enqueue regions for playing (recording)
 *	Pre-page active region(s) to maintain required high/low water marks
 *	Prime the low-level services when adding entries to an empty queue
 *	Complete regions when all portions of it are complete.
 */

#import <sys/param.h>
#import <sys/callout.h>
#import <sys/buf.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <vm/vm_page.h>
#import <nextdev/busvar.h>
#import <nextdev/snd_var.h>
#import <nextdev/snd_snd.h>
#import <nextdev/snd_dsp.h>

#import <kern/xpr.h>
#import <kern/kalloc.h>
#import <machine/spl.h>

#if	XPR_DEBUG
extern int sdbgflag;
#define snd_dbug(f) {if (sdbgflag&1)printf f; else XPR(XPR_SOUND, f);}
#define snd_dbug2(f) if (sdbgflag&2){if (sdbgflag&1)printf f; else XPR(XPR_SOUND, f);}
#else	XPR_DEBUG
#define snd_dbug(f)
#define snd_dbug2(f)
#endif	XPR_DEBUG

extern snd_var_t snd_var;
extern ll_snd_var_t ll_snd_var;
extern dsp_var_t dsp_var;

static void snd_stream_thread(void);
static boolean_t snd_desc_completion(snd_dma_desc_t *dd);
static boolean_t snd_stream_start(snd_queue_t *q, register snd_region_t *reg);
static void snd_deactivate_pages (
	vm_address_t start,
	vm_size_t len,
	boolean_t dirty,
	boolean_t deactivate);
static void snd_wire_page(vm_address_t page);
static snd_region_t *snd_reg_complete(snd_region_t *reg, snd_queue_t *q);
static snd_region_t *snd_split_region(snd_region_t *reg, snd_queue_t *q);

/*
 * Stream level initialization.
 */
void snd_stream_init(void)
{
	/*
	 * Initialize stream interface.
	 */
	queue_init(&snd_var.new_thread_q);
}

/*
 * Stream level reset.  Release all resources.
 */
void snd_stream_reset(void)
{
}

void snd_stream_queue_init(
	register snd_queue_t *q,
	caddr_t addr,
	boolean_t (*start)(snd_dma_desc_t *ddp, int direction, int rate))
{
	q->ptr = addr;
	queue_init(&q->reg_q);
	lock_init(&q->lock, TRUE);
	q->exclusive = q->enabled = q->active = q->nxfer = 0; 
	q->thread = 0;
	q->start = start;
	q->total_wired = q->paused  = 0;
	q->will_overflow = 0;
	q->ndma = 4;
	q->linked = 0;
}

void snd_stream_queue_reset(register snd_queue_t *q)
{
	snd_stream_abort(q, 0);

	/*
	 * Wait for the thread to finish.
	 */
	while(q->thread && current_thread()->wait_result != THREAD_INTERRUPTED)
	{
		assert_wait((int)q, TRUE);
		thread_block();
	}
}

/*
 * Add this region to the list of regions to play.
 */ 
void snd_stream_enqueue_region(
	snd_region_t *reg,
	int index,
	boolean_t device,
	boolean_t preempt)
{
	register snd_queue_t *q = reg->sound_q;
	snd_region_t *oreg;
	register int s;

	/*
	 * Initialization.
	 */
	reg->was_aborted = reg->was_started = reg->is_complete =
		reg->did_overflow = reg->discont = reg->awaiting = 0;
	reg->first_locked = reg->next_to_lock =
		trunc_page(reg->addr);
	if (reg->first_locked)
		snd_dbug2(("snd_str_enq_reg: region starting at 0x%x, not 0\n",
			reg->addr));
	reg->rw_head = reg->rw_tail = reg->addr;
	reg->end = reg->addr + reg->size;

	/*
	 * If not specified, get dma size low- and high-water from the queue.
	 */
	if (reg->low_water == 0)
		reg->low_water = q->def_low_water;
	if (reg->high_water == 0)
		reg->high_water = q->def_high_water;
	if (!reg->dma_size)
		reg->dma_size = q->dmasize;

	/*
	 * Get a thread if we need one.
	 */
	if (q->thread == NULL) {
		/*
		 * Add region to queue of regions awaiting threads,
		 * it'll be picked up from there by some allocated
		 * thread (probably this one).
		 */
		s = spldsp();
		queue_enter(&snd_var.new_thread_q, q, snd_queue_t *, free_q);
		splx(s);
		q->thread = kernel_thread(snd_var.task, snd_stream_thread);
	}

	/*
	 * Add the region to the queue and tickle the queue thread
	 * to start getting it's pages in.
	 */
	snd_dbug(("snd_str_enq_reg: %x on q %x\n", reg, q));
	lock_write(&q->lock);
	if (!queue_empty(&q->reg_q)) {
		oreg = (snd_region_t *)queue_last(&q->reg_q);
		reg->discont = oreg->discont;
	}
	queue_enter(&q->reg_q, reg, snd_region_t *, link);
	lock_done(&q->lock);
	
	/*
	 * In case the guy is asleep, wake him up.
	 */
	q->work = 1;
	thread_wakeup((int)q);
}

/*
 * Stop everything enqueued from being played.  A completion message (with
 * byte count) is returned to the user.  If we're recording, send back
 * what's been recorded so far.
 */
void snd_stream_abort(snd_queue_t *q, int data_tag)
{
	register snd_region_t *reg;
	register int s;

	/*
	 * Find region being played.
	 */
	s = spldsp();

	for (  reg = (snd_region_t *)queue_first(&q->reg_q)
	     ; !queue_end(&q->reg_q, (queue_t)reg) && reg->is_complete
	     ; reg = (snd_region_t *)queue_next(&reg->link))
		;
	
	/*
	 * Set the "was_aborted" flag so that the queue thread will
	 * clean this stuff up.
	 */
	for (; !queue_end(&q->reg_q, (queue_t)reg)
	     ; reg = (snd_region_t *)queue_next(&reg->link))
	 	if (!data_tag || reg->reg_id == data_tag) {
			reg->was_aborted = 1;
			reg->awaiting = 1;
			snd_dbug(("snd_stream_abort: 0x%x, on q 0x%x\n",
				reg, q));
		}

	splx(s);
	
	q->work = 1;
	thread_wakeup((caddr_t)q);
}

/*
 * Pause everything enqueued from being played.  The low level routines
 * aren't sent any further updates.
 */
void snd_stream_pause(snd_queue_t *q)
{
	register snd_region_t *reg;
	register int s;

	s = spldsp();
	q->paused = 1;

	/*
	 * Linked queue has no regions.
	 */
	if (q->linked) {
		snd_link_pause(q);
		splx(s);
		return;
	}

	/*
	 * Issue pause messages.
	 */
	for ( reg = (snd_region_t *)queue_last(&q->reg_q)
	     ; !queue_end(&q->reg_q, (queue_t)reg) && !reg->is_complete
	     ; reg = (snd_region_t *)queue_prev(&reg->link))
	{
		/*
		 * Check to see if the guy wants a message.
		 */
		if (reg->paused) {
			splx(s);
			
			snd_dbug(("snd_stream_pause: 0x%x, on q 0x%x\n",
				reg, q));
			snd_reply_paused(reg->reply_port, reg->reg_id);

			s = spldsp();
		}
	}
	splx(s);
}

/*
 * Start sending updates to the low-level queues and re-start the device.
 */
void snd_stream_resume(snd_queue_t *q)
{
	register snd_region_t *reg;
	register int s;

	s = spldsp();

	q->paused = 0;

	/*
	 * Linked queue has no regions.
	 */
	if (q->linked) {
		snd_link_resume(q);
		splx(s);
		return;
	}

	/*
	 * Issue resume messages.
	 */
	for ( reg = (snd_region_t *)queue_last(&q->reg_q)
	     ; !queue_end(&q->reg_q, (queue_t)reg) && !reg->is_complete
	     ; reg = (snd_region_t *)queue_prev(&reg->link))
	{
		/*
		 * Check to see if the guy wants a message.
		 */
		if (reg->resumed) {
			splx(s);
			
			snd_dbug(("snd_stream_resume: 0x%x, on q 0x%x\n",
				reg, q));
			snd_reply_resumed(reg->reply_port, reg->reg_id);

			s = spldsp();
		}
	}
	splx(s);

	/*
	 * Get the thread back to work
	 */
	q->work = 1;;
	thread_wakeup((caddr_t)q);
}

/*
 * Return and already recorded data
 */
kern_return_t snd_stream_await (
	snd_queue_t	*q,
	int		data_tag)
{
	register snd_region_t *reg;
	register int s;

	s = spldsp();

	/*
	 * Find the region in question.
	 */
	for ( reg = (snd_region_t *)queue_first(&q->reg_q)
	     ; !queue_end(&q->reg_q, (queue_t)reg)
	     ; reg = (snd_region_t *)queue_next(&reg->link))
	{
		/*
		 * Check to see if the guy wants a message.
		 */
		if (reg->is_complete)
			continue;
		if (data_tag && reg->reg_id != data_tag)
			continue;
		break;
	}
	
	if (queue_end(&q->reg_q, (queue_t)reg)) {
		splx(s);
		
		/*
		 * No such region, or it's already been completed.
		 * Return an error.
		 */
		return SND_SEARCH;
	}

	snd_dbug(("snd_stream_await: 0x%x, on q 0x%x\n", reg, q));
	reg->awaiting = 1;

	splx(s);

	/*
	 * Get the thread back to work
	 */
	q->work = 1;;
	thread_wakeup((caddr_t)q);
	return KERN_SUCCESS;
}

/*
 * Per-queue thread.
 * Functions:
 *	Lock regions pages in memory to satisfy requirements.
 *	Start up dma activity using lower level routines.
 *	Free chunks of memory when some number are complete.
 *	Free region when complete.
 *	Return completion, nsamples when region completes.
 *	Detect underflow/overflow from lower levels and issue message.
 */
static void snd_stream_thread(void)
{
#if	DEBUG
	int foo[100];
	snd_stream_thread1();
}

snd_stream_thread1()
{
	int foo[100];
	snd_stream_thread2();
}

snd_stream_thread2()
{
#endif	DEBUG
	snd_queue_t *q;
	snd_region_t *stale_reg, *active_reg, *wire_reg;
	register snd_dma_desc_t *ddp;
	int desc_size;
	register int i;

	/*
	 * Find out which queue we represent.
	 */
	ASSERT(!queue_empty(&snd_var.new_thread_q));
	queue_remove_first(&snd_var.new_thread_q, q, snd_queue_t *, free_q);
	ASSERT(q->thread == current_thread());

	/*
	 * Allocate a set of dma_desc's for us to use.
	 */
	queue_init(&q->free_q);
	for (i = q->ndma - 1; i >=0; i--) {
		ddp = kalloc(sizeof *ddp);
		snd_dbug2(("snd_s_t: alloc ddp 0x%x\n", ddp));
		ddp->hdr.dh_drvarg = (int)ddp;
		queue_enter(&q->free_q, ddp, snd_dma_desc_t *, link);
		ddp->free = (int (*)()) snd_desc_completion;
	}

	/*
	 * Loop locking pages, sending them to the device, and dequeuing
	 * freed pages.
	 */
	lock_write(&q->lock);
	stale_reg = (snd_region_t *)queue_first(&q->reg_q);
	wire_reg = (snd_region_t *)queue_first(&q->reg_q);
	while (!queue_empty(&q->reg_q)) {
		q->work = 0;
		ASSERT(!(stale_reg->first_locked%PAGE_SIZE));
		ASSERT(!(wire_reg->first_locked%PAGE_SIZE));
		ASSERT(!(stale_reg->next_to_lock%PAGE_SIZE));
		ASSERT(!(wire_reg->next_to_lock%PAGE_SIZE));
		ASSERT(!(q->total_wired%PAGE_SIZE));

		/*
		 * Free any pages in this region up until the rw_tail
		 */
		if (   ((int)stale_reg->rw_tail - (int)stale_reg->first_locked)
		    >= (int)PAGE_SIZE)
		{
			register int len;

			ASSERT(stale_reg->rw_tail > stale_reg->first_locked);
			len = trunc_page(stale_reg->rw_tail)
				- stale_reg->first_locked;
			ASSERT(len > 0);
			snd_dbug2(("snd_s_t: deactivate %d bytes from %x"
				  " in region %x\n", len,
				  stale_reg->first_locked, stale_reg));
			snd_deactivate_pages(stale_reg->first_locked, len,
				stale_reg->direction == SND_DIR_RECORD,
				stale_reg->deactivate);
			stale_reg->first_locked += len;
			q->total_wired -= len;
			ASSERT(q->total_wired >= 0);
		}

		/*
		 * Check for regions requiring some message be sent on them.
		 */
		if (stale_reg->did_overflow) {
			stale_reg->did_overflow = 0;
			snd_dbug(("snd_s_t: overflow in region %x\n",
				  stale_reg));
			if (stale_reg->overflow)
				snd_reply_overflow(stale_reg->reply_port,
					stale_reg->reg_id);
		}
		if (stale_reg->was_started && stale_reg->started) {
			stale_reg->started = 0;
			
			snd_dbug(("snd_s_t: started region %x\n",
				  stale_reg));
			(void) snd_reply_started(stale_reg->reply_port,
				stale_reg->reg_id);
		}

		if (stale_reg->was_started && stale_reg->awaiting) {
			stale_reg = snd_split_region(stale_reg, q);
			wire_reg = stale_reg;
		}

		/*
		 * Clean up aborted regions.
		 */
		if (stale_reg->was_aborted) {
			snd_region_t *reg;
			int len;

			/*
			 * Keep from playing/recording any more in this reg.
			 */
			stale_reg->discont = TRUE;

			/*
			 * Free the un-used portion of the region and
			 * adjust the region's size so that we'll complete.
			 */
			ASSERT(stale_reg->rw_head <= stale_reg->end);

			/*
			 * Deactiveate unused wired pages.
			 */
			len = stale_reg->next_to_lock
				- round_page(stale_reg->rw_head);
			if (len > 0) {
				snd_deactivate_pages(
					round_page(stale_reg->rw_head), len,
					FALSE, TRUE);
				q->total_wired -= len;
				ASSERT(q->total_wired >= 0);
				stale_reg->next_to_lock =
					round_page(stale_reg->rw_head);
			}

			ASSERT(stale_reg->rw_tail <= stale_reg->rw_head);
			ASSERT(stale_reg->addr <= stale_reg->rw_tail);
			ASSERT(   stale_reg->rw_head <= stale_reg->next_to_lock
			       || stale_reg->rw_head == stale_reg->addr);
			ASSERT(    stale_reg->next_to_lock
				<= round_page(stale_reg->end));
			if (stale_reg->next_to_lock < stale_reg->end) {
				snd_dbug2(("s_s_t: (ntl < end) "
					"deallocate 0x%x bytes at 0x%x\n",
					  stale_reg->end
					- stale_reg->next_to_lock,
					stale_reg->next_to_lock));
				(void) vm_deallocate(snd_var.task_map,
				    stale_reg->next_to_lock,
				    stale_reg->end - stale_reg->next_to_lock);
				stale_reg->end = stale_reg->next_to_lock;
			}
			stale_reg->first_locked = stale_reg->next_to_lock;
			i = spldsp();
			if (stale_reg->rw_tail == stale_reg->rw_head) {
				/*
				 * We're done, clean up an go on to the
				 * completion.
				 */
				stale_reg->is_complete = 1;
			} else {
				/*
				 * We'll finish when the last dma descriptor
				 * is returned in the completion routine.
				 */
				;
			}
			stale_reg->end = stale_reg->rw_head;
			stale_reg->size = stale_reg->end - stale_reg->addr;
			stale_reg->completed = 0;
			stale_reg->was_aborted = 0;
			/*
			 * Set the discont flag so that streams
			 * being enqueued while this guy's waiting
			 * to complete will have their discont flag set
			 * so that they'll keep the device from walking
			 * into the next region without underflowing this
			 * region.
			 */
			stale_reg->discont = 1;

			/*
			 * If we haven't started up the next region,
			 * force an underflow so that the next region
			 * will start from zero, not where the last
			 * (aborted) stream left off.
			 */
			reg = (snd_region_t *)queue_next(&stale_reg->link);
			if (   !queue_end(&q->reg_q, (queue_t)reg)
			    && !reg->started)
			    	reg->discont = 1;
			splx(i);
			snd_dbug(("snd_s_t: aborted region %x%s\n",
				stale_reg, stale_reg->aborted ?" (send)":""));
			if (stale_reg->aborted)
				snd_reply_aborted(stale_reg->reply_port,
					stale_reg->reg_id);
			continue;
		}

		/*
		 * Region finished, issue completion message and remove
		 * the memory and region structures.
		 */
		if (   stale_reg->is_complete
		    && stale_reg->first_locked >= trunc_page(stale_reg->end))
		{
			stale_reg = snd_reg_complete(stale_reg, q);
			wire_reg = stale_reg;

			lock_done(&q->lock);
			lock_write(&q->lock);
			continue;
		}

		/*
		 * If the lower level isn't active and we have enough
		 * locked down to start sound up, do so.
		 */
		if (   !q->active
		    && !q->paused
		    && !q->will_overflow
		    && !queue_empty(&q->free_q)
		    && (       (  stale_reg->next_to_lock
			        - trunc_page(stale_reg->rw_head))
			    >= stale_reg->low_water
			|| stale_reg->next_to_lock >= stale_reg->end))
		{
			stale_reg->discont = 0;
			(void) snd_stream_start(q, stale_reg);
		}

		/*
		 * If it's time to start wiring down the next region, move
		 * there. (region wired && !last region in queue)
		 */
		if (   (   wire_reg->next_to_lock >= wire_reg->end
			|| wire_reg->was_aborted)
		    && !queue_end(&q->reg_q, queue_next(&wire_reg->link)))
		{
			wire_reg = (snd_region_t *)queue_next(&wire_reg->link);
			snd_dbug2(("snd_s_t: wire reg now %x\n", wire_reg));
		}
			
		/*
		 * If this region is below it's high water mark, but
		 * not all locked down continue locking it.
		 */
		if (   !wire_reg->was_aborted
		    && wire_reg->next_to_lock < wire_reg->end
		    && q->total_wired < wire_reg->high_water)
		{
			ASSERT(!(wire_reg->next_to_lock%PAGE_SIZE));
			snd_dbug2(("snd_s_t: wire %d bytes at %x in reg %x\n",
				  PAGE_SIZE, wire_reg->next_to_lock,
				  wire_reg));
			snd_wire_page(wire_reg->next_to_lock);
			wire_reg->next_to_lock += PAGE_SIZE;

			/*
			 * Make sure that our low_water mark doesn't
			 * exceed the number of pages left in the region.
			 */
			if (  wire_reg->next_to_lock + wire_reg->low_water
			    > wire_reg->end)
			    	wire_reg->low_water -= PAGE_SIZE;

			ASSERT(q->total_wired >= 0);
			q->total_wired += PAGE_SIZE;

			lock_done(&q->lock);
			lock_write(&q->lock);
			continue;
		} else {
			snd_dbug2(("snd_s_t: don't wire%s%s%s\n",
				wire_reg->was_aborted
					? " wire_reg->was_aborted" : "",
				wire_reg->next_to_lock >= wire_reg->end
					? " next_to_lock >= end" : "",
				q->total_wired >= wire_reg->high_water
					? " total_wired >= high_water" : ""));
		}

		lock_done(&q->lock);

		/*
		 * Sleep until our low-water mark is hit or or something
		 * else wakes us up.
		 * FIXME: wicked race here.
		 */
		i = splsched();
		if (!q->work) {
			assert_wait((int)q, TRUE);
			thread_block();
		}
		splx(i);

		lock_write(&q->lock);
	}

	/*
	 * Free ddp's.
	 */
	while (!queue_empty(&q->free_q)) {
		queue_remove_first(&q->free_q, ddp, snd_dma_desc_t *, link);
		snd_dbug2(("snd_s_t: free ddp 0x%x\n", ddp));
		kfree(ddp, sizeof(*ddp));
	}

	/*
	 * Empty queue, terminate this thread.
	 */
	q->thread = NULL;

	snd_dbug2(("snd_s_t: terminate\n"));
	lock_done(&q->lock);
	thread_wakeup((int)q);		// wakeup anyone waiting for us.
	thread_terminate(current_thread());
	thread_halt_self();
	ASSERT(0);
}

/*
 * Routine called (at interrupt level) by lower level dma completion
 * to "free" the dma descriptor.
 * (called at splsched)
 */
static boolean_t snd_desc_completion(snd_dma_desc_t *ddp)
{
	register snd_region_t *reg = (snd_region_t *)ddp->ptr;
	register snd_queue_t *q = reg->sound_q;
	int do_wakeup = 0, rval = 0;

	snd_dbug(("snd_desc_c: finished ddp %x addr %x\n", ddp, ddp->vaddr));

	/*
	 * Update pointer in region.
	 */
	q->active = 1;
	ASSERT(ddp->vaddr == reg->rw_tail);
	
	if (reg->rw_tail == reg->addr) {
		snd_dbug(("snd_desc_c: reg %x started\n", reg));
		reg->was_started = 1;
		do_wakeup++;
	}

	reg->rw_tail += ddp->size;

	/*
	 * Signal special conditions for waking up thread.
	 */
	if (reg->rw_tail == reg->end) {
		snd_dbug(("snd_desc_c: reg %x complete\n", reg));
		reg->is_complete = 1;
		reg->did_overflow = 0;
		do_wakeup++;
	}

	/*
	 * If we overflowed, then we don't have any dma descriptors
	 * in the device, so we can arbitrarily reset rw_tail
	 * to rw_head.  This helps when we're trying to shut down
	 * a region (abort) and we've set rw_head to the end and
	 * we're just waiting for dma to complete.
	 */
	if (ddp->flag&SDD_OVERFLOW) {
		snd_dbug(("snd_desc_c: reg %x overflow %s(complete)\n", reg,
			reg->is_complete ? "" : "!"));
		q->will_overflow = 0;
		if (!reg->is_complete) {
			reg->did_overflow++;
			q->active = 0;
			do_wakeup++;
		}
	}

	/*
	 * If someone's waiting for the data, get the thread going.
	 */
	if (reg->awaiting)
		do_wakeup++;

	queue_enter(&q->free_q, ddp, snd_dma_desc_t *, link);

	/*
	 * Get another page for dma.
	 */
	if (reg->rw_head == reg->end) {
		boolean_t old_complete = reg->is_complete;
		snd_dbug(("snd_desc_c: (head == end == 0x%x tail 0x%x, ",
			  reg->rw_head, reg->rw_tail));
		reg = (snd_region_t *)queue_next(&reg->link);
		if (queue_end(&q->reg_q, (queue_t)reg)) {
			q->active = 0;
			q->will_overflow = !(ddp->flag&SDD_OVERFLOW);
			do_wakeup++;
			snd_dbug(("reg empty\n"));
			goto done;
		} else if (old_complete && reg->rw_head >= reg->next_to_lock) {
			q->active = FALSE;
			snd_dbug(("reg %x (!active)\n", reg));
		} else
			snd_dbug(("reg %x (active)\n", reg));
	}

	/*
	 * See if we need an underrun.
	 */
	if (!q->active || q->paused || reg->discont || reg->was_aborted) {
		/*
		 * Don't start another transfer.
		 */
		q->active = 0;
		rval = (ddp->flag&SDD_OVERFLOW);
		snd_dbug2(("snd_desc_c: %s%s%s%s -> done\n",
			!q->active ? "!q->active" : "",
			q->paused ? " q->paused" : "",
			reg->discont ? " reg->discont" : "",
			reg->was_aborted ? " reg->was_aborted" : ""));
		goto done;
	}

	/*
	 * See if we can get another page out of the region.
	 */
	q->will_overflow = 0;
	rval = snd_stream_start(q, reg);

	/*
	 * Flow control thread
	 */
	if (   (   reg->next_to_lock-round_page(reg->rw_head) <= reg->low_water
		&& reg->next_to_lock != round_page(reg->end))
	    || reg->rw_tail - reg->first_locked >= 4*PAGE_SIZE)
		do_wakeup++;

	/*
	 * Wake up the thread if this is the last desc in the region,
	 * or the number of bytes locked in the region falls under the
	 * low-water mark.
	 */
     done:
	 if (do_wakeup) {
		q->work = 1;
		if (curipl() > IPLSCHED)
			softint_sched(CALLOUT_PRI_SOFTINT1, thread_wakeup,
				(int)q);
		else
			thread_wakeup((int)q);
	}
	return rval;
}

/*
 * Start up as many buffers as can be sent down to the device.
 */
static boolean_t snd_stream_start(snd_queue_t *q, register snd_region_t *reg)
{
	register int s, size;
	register snd_dma_desc_t *ddp;
	int rval = FALSE;

	snd_dbug2(("snd_s_start: q 0x%x, reg 0x%x\n", q, reg));

	s = spldsp();

	while (    reg->rw_head < reg->next_to_lock
		&& reg->rw_head < reg->end
		&& !queue_empty(&q->free_q))
	{
		rval = TRUE;
		queue_remove_first(&q->free_q, ddp, snd_dma_desc_t *, link);

		/*
		 * Size to keep on page boundary.
		 */
		size = round_page(reg->rw_head+1) - reg->rw_head;
		if (size > reg->dma_size)
			size = reg->dma_size;
		if (reg->rw_head + size > reg->end)
			size = reg->end - reg->rw_head;

		ddp->vaddr = reg->rw_head;
		ddp->size = size;
		ddp->flag = 0;
		ddp->ptr = (caddr_t)reg;
		reg->rw_head += size;
		ddp->hdr.dh_start = (char *)pmap_resident_extract(
					snd_var.task_map->pmap,
					ddp->vaddr);
		if (!ddp->hdr.dh_start) {
			/*
			 * If the mapping failed, then our underlying
			 * object has been screwed up.  Abort playing
			 * the region.
			 * FIXME: this is a VM bug, it shouldn't be
			 * fixed here.
			 */
			snd_dbug(("snd_s_start: aborting region\n"));
			reg->was_aborted = TRUE;
			q->active = FALSE;
			reg->rw_head -= size;
			queue_enter(&q->free_q, ddp, snd_dma_desc_t *, link);
			rval = FALSE;
			q->will_overflow = 1;
			break;
		}

		ASSERT((int)ddp->hdr.dh_start / PAGE_SIZE);
		ddp->hdr.dh_stop = ddp->hdr.dh_start + ddp->size;
		snd_dbug(("snd_s_start: start vaddr %x paddr %x size %d\n",
			  ddp->vaddr, ddp->hdr.dh_start, size));
		if ((*q->start)(ddp, reg->direction, reg->rate) == 0) {
			/*
			 * Sound didn't start up.
			 */
			snd_dbug(("snd_s_start: no start -> will_ovrflow\n"));
			reg->rw_head -= size;
			queue_enter(&q->free_q, ddp, snd_dma_desc_t *, link);
			rval = FALSE;
			q->will_overflow = 1;
			break;
		}
		splx(s);
		s = spldsp();
	}
	snd_dbug2(("snd_s_start: exit%s%s%s\n",
		(reg->rw_head >= reg->next_to_lock)
			? " rw_head >= next_to_lock" : "",
		(reg->rw_head == reg->end) ? " rw_head == end" : "",
		queue_empty(&q->free_q) ? " queue_empty" : ""));
	splx(s);

	return rval;
}

/*
 * Unwire the range of pages, placing them direcly on the inactive
 * queue if the option is given.  Pages are marked dirty or clean explicity
 * based upon parameter.
 */
static void snd_deactivate_pages (
	vm_address_t start,
	vm_size_t len,
	boolean_t dirty,
	boolean_t deactivate)
{
	vm_page_t m;

	ASSERT(!(start%PAGE_SIZE));
	for (;len; len -= PAGE_SIZE, start += PAGE_SIZE) {
		/*
		 * Make sure that the page is marked dirty if we
		 * recorded into it.
		 */
		vm_page_lock_queues();
		m = PHYS_TO_VM_PAGE(pmap_resident_extract(
				snd_var.task_map->pmap, start));

		/*
		 * FIXME: because of a bug in the vm copy stuff,
		 * our mapping might get lost if the file that's being
		 * played is modified.  Therefore, it's possible that
		 * m is NULL.
		 */
		if (m) {
			m->clean = !dirty;
			vm_page_unlock_queues();
		} else {
			vm_page_unlock_queues();
			continue;
		}

		/*
		 * Unlock the page.
		 */
		(void) vm_fault(snd_var.task_map, start, VM_PROT_NONE, TRUE);

		if (deactivate) {
			/*
			 * Get the physical address of this page and place it
			 * on the inactive queue.
			 */
			vm_page_lock_queues();
			m = PHYS_TO_VM_PAGE(pmap_extract(
							snd_var.task_map->pmap,
							start));
			if (m)
				vm_page_deactivate(m);
			vm_page_unlock_queues();
		}
	}
}

/*
 * Fault the page in and wire it down into memory.
 */
static void snd_wire_page(vm_address_t page)
{
	vm_map_entry_t		entry;

	/*
	 * Get the map entry for this page and change it so that pages
	 * get wired from vm_fault().
	 */
	ASSERT(!(page%PAGE_SIZE));
	vm_map_lock(snd_var.task_map);
	if (!vm_map_lookup_entry(snd_var.task_map, page, &entry))
		panic("snd_wire_page: addr not in map!");
	entry->wired_count++;
	vm_map_unlock(snd_var.task_map);

	/*
	 * Fault the page in and lock it.
	 */
	(void) vm_fault(snd_var.task_map, page, VM_PROT_NONE, TRUE);

	vm_map_lock(snd_var.task_map);
	entry->wired_count--;
	vm_map_unlock(snd_var.task_map);
}

/*
 * Cleanup a region that has been copleted.
 * Returns the next region.
 */
static snd_region_t *snd_reg_complete(snd_region_t *reg, snd_queue_t *q)
{

	ASSERT(reg->rw_head == reg->rw_tail);
    
	snd_dbug(("snd_reg_complete: finished region %x", reg));
    
	/*
	 * First make sure that the last page is unlocked.
	 */
	if (reg->first_locked != reg->next_to_lock) {
		snd_deactivate_pages(reg->first_locked, PAGE_SIZE,
		    reg->direction == SND_DIR_RECORD, reg->deactivate);
		q->total_wired -= PAGE_SIZE;
	}
    
	if (reg->direction == SND_DIR_RECORD) {
		/*
		 * Send a message containing the contents
		 * of the region.
		 */
		if (reg->size) {
			snd_dbug((" (send recorded data)"));
			(void) snd_reply_recorded_data(reg->reply_port,
						       reg->reg_id, reg->addr,
						       reg->size, FALSE);
		}
	} else {
		if (reg->size) {
			snd_dbug2((" (deallocate 0x%x bytes at 0x%x)",
				reg->size, reg->addr));
			(void) vm_deallocate(snd_var.task_map, reg->addr,
				reg->size);
		}
		if (reg->completed) {
			snd_dbug((" (send completed)"));
			(void) snd_reply_completed(reg->reply_port,
						   reg->reg_id);
		}
	}
	
	/*
	 * Dequeue the region.
	 */
	queue_remove_first(&q->reg_q, reg, snd_region_t *, link);
    
	/*
	 * Deallocate the region structure.
	 */
	kfree((caddr_t)reg, sizeof(snd_region_t));

	snd_dbug((" next region %x\n",
		queue_empty(&q->reg_q) ? 0 : queue_first(&q->reg_q)));

    
	return (snd_region_t *)queue_first(&q->reg_q);
}

/*
 * Split the given region into two parts, making the first complete
 * if appropriate.  If the split occures in the middle of a page then
 * we need to make a vm_copy of one of them so that the overlapping
 * page isn't deallocated.
 */
static snd_region_t *snd_split_region(snd_region_t *reg, snd_queue_t *q)
{
	int s;
	vm_address_t reg_addr;
	vm_size_t new_size;
	snd_region_t *new_reg = (snd_region_t *)kalloc(sizeof(*new_reg));

	s = spldsp();
	reg->awaiting = 0;
	*new_reg = *reg;
	reg->was_started = 0;
	reg->aborted = 0;	// already getting an aborted message
	reg_addr = reg->addr;

	/*
	 * Update the current region before we allow interrupts.
	 */
	new_size = reg->rw_tail - reg->addr;
	reg->addr += new_size;
	reg->size = reg->end - reg->addr;
	reg->first_locked = trunc_page(reg->addr);
	snd_dbug2(("snd_split_stream: new_size 0x%x reg_size 0x%x\n",
		new_size, reg->size));

	/*
	 * If the rw_tail convinently is on a page boundary then it's a pretty
	 * simple matter to fork the vm range and wait for completion on the
	 * first portion.  Otherwise we need to create a new vm map to be able
	 * to send off. 
	 */
	if (reg->rw_tail%PAGE_SIZE) {
		vm_size_t copy_size, dealloc_size;
		vm_address_t new_addr;

		/*
		 * We have to do this with interrupts on.  After the new map
		 * has been created we again need to check for completion of
		 * the current region.
		 */
		copy_size = round_page(reg->rw_tail) - trunc_page(reg_addr);
		dealloc_size = trunc_page(reg->rw_tail) -
			trunc_page(reg_addr);

		splx(s);
		(void) vm_allocate(snd_var.task_map, &new_addr, copy_size,
			TRUE);
		(void) vm_copy(snd_var.task_map, trunc_page(reg_addr),
			       copy_size, new_addr);
		if (reg->is_complete) {
			/*
			 * Too late, the current region is already complete.
			 */
			snd_dbug2(("snd_split_region: (is_complete)"
				"deallocate 0x%x bytes at 0x%x\n",
				copy_size, new_addr));
			vm_deallocate(snd_var.task_map, new_addr,
				      copy_size);
			kfree(new_reg, sizeof(*new_reg));
			reg->addr = reg_addr;
			reg->size = reg->end - reg->addr;
			return reg;
		} else if (new_size >= PAGE_SIZE) {
			snd_dbug2(("snd_split_region: (new_size >= PAGE_SIZE)"
				"deallocate 0x%x bytes at 0x%x\n",
				dealloc_size, trunc_page(reg_addr)));
			vm_deallocate(snd_var.task_map, trunc_page(reg_addr),
				dealloc_size);
		}
		s = spldsp();
		new_reg->addr = new_addr + reg_addr%PAGE_SIZE;
		new_reg->size = new_size;
		new_reg->end = new_reg->addr + new_size;
		new_reg->next_to_lock = new_reg->first_locked =
			round_page(new_reg->end);
	} else {
		/*
		 * Simple split of the region.
		 */

		new_reg->end = reg->rw_tail;
		new_reg->size = new_reg->end - new_reg->addr;
		new_reg->first_locked = new_reg->next_to_lock = new_reg->end;
	}


	new_reg->rw_head = new_reg->rw_tail = new_reg->end;
	new_reg->is_complete = 1;
	new_reg->link.next = (queue_t)reg;
	reg->link.prev = (queue_t)new_reg;
	if (queue_end(&q->reg_q, new_reg->link.prev))
		q->reg_q.next = (queue_t)new_reg;
	else
		((snd_region_t *)new_reg->link.prev)->link.next
			= (queue_t)new_reg;

	snd_dbug(("snd_split_region: new reg %x (0x%x to 0x%x)",
		  new_reg, new_reg->addr, new_reg->end));
	snd_dbug((", next reg 0x%x (0x%x to 0x%x)\n",
		  reg, reg->addr, reg->end));
	ASSERT(new_reg->end%PAGE_SIZE == reg->addr%PAGE_SIZE);
	ASSERT(new_reg->size == new_reg->end - new_reg->addr);
	ASSERT(reg->size == reg->end - reg->addr);

	splx(s);
	return (new_reg);
}

#endif	NSOUND > 0
