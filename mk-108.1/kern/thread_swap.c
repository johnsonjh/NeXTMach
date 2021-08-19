/*
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	thread_swap.c,v $
 * 20-Jul-90  Brian Pinkerton (bpinker) at NeXT
 *	Use new kernel stack routines to swap kernel stacks in and out.
 *
 *  2-Mar-90  Gregg Kellogg (gk) at NeXT
 *	MACH_OLD_VM_COPY: use old vm_map_pageable semantics.
 *	NeXT: use single thread for swapping
 *
 * Revision 2.10  89/12/23  13:51:07  rpd
 * 	Fixed thread_sleep call to use simple_lock_addr.
 * 
 * Revision 2.9  89/12/22  15:54:10  rpd
 * 	Add code to make thread_swappable work on any thread no matter
 * 	what its swap state is.
 * 	[89/11/28            dlb]
 * 
 * Revision 2.8  89/10/11  14:34:29  dlb
 * 	New calling sequence for vm_map_pageable.
 * 	[88/11/23            dlb]
 * 		
 * 	Convert to nested scan for processor logic.  Must scan all psets,
 * 	then all threads within each pset.
 * 	[88/10/26            dlb]
 * 
 * Revision 2.7  89/03/09  20:17:00  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  18:10:17  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.5  89/01/15  16:28:14  rpd
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  15:09:18  rpd]
 * 
 * Revision 2.4  88/10/27  10:50:40  rpd
 * 	Changed includes to the new style.  Removed extraneous semis
 * 	from the swapper_lock/swapper_unlock macros.
 * 	[88/10/26  14:49:09  rpd]
 * 
 * 15-Jun-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Fix improper handling of swapper_lock() in swapin_thread().
 *	Problem discovery and elegant recoding due to Richard Draves.
 *
 *  4-May-88  David Golub (dbg) at Carnegie-Mellon University
 *	Remove vax-specific code.
 *
 *  1-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	Logic change due to replacement of wait_time field in thread
 *	with sched_stamp.  Extra argument to thread_setrun().
 *
 * 25-Jan-88  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Notify pcb module that pcb is about to be unwired by calling
 *	pcb_synch(thread).
 *	
 * 21-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Fix lots more race conditions (thread_swapin called during
 *	swapout, thread_swapin called twice) by adding a swapper state
 *	machine to each thread.  Moved thread_swappable here from
 *	kern/thread.c.
 *
 * 12-Nov-87  David Golub (dbg) at Carnegie-Mellon University
 *	Fix race condition in thread_swapout: mark thread as swapped
 *	before swapping out its stack, so that an intervening wakeup
 *	will put it on the swapin list.
 *
 *  5-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Changed to new scheduling state machine.
 *
 * 15-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 *  5-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added check for THREAD_SWAPPABLE in swapout_scan().
 *
 * 14-Jul-87  David Golub (dbg) at Carnegie-Mellon University
 *	Truncate the starting address and round up the size given to
 *	vm_map_pageable, when wiring/unwiring kernel stacks.
 *	KERNEL_STACK_SIZE is not necessarily a multiple of page_size; if
 *	it isn't, forgetting to round the address and size to page
 *	boundaries results in panic.  Kmem_alloc and kmem_free, used in
 *	thread.c to allocate and free kernel stacks, already round to
 *	page boundaries.
 *
 * 26-Jun-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Add thread_swapout_allowed flag to make it easy to turn
 *	off swapping when debugging.
 *
 *  4-Jun-87  David Golub (dbg) at Carnegie-Mellon University
 *	Pass correct number of parameters to lock_init - initialize
 *	swap_lock as sleepable instead of calling lock_sleepable
 *	separately.
 *
 *  1-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Include vm_param.h to pick up KERNEL_STACK_SIZE definition.
 *
 * 20-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Lower ipl before calling thread_swapout().
 *
 * 19-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	Fix one race condition in this (not so buggy) version - since
 *	thread_swapin can be called from interrupts, must raise IPL when
 *	locking swapper_lock.
 *
 * 09-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created, based somewhat loosely on the earlier (which was a highly
 *	buggy, race condition filled version).
 *
 */
/*
 *
 *	File:	kern/thread_swap.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1987, Avadis Tevanian, Jr. and Richard F. Rashid
 *
 *	Mach thread swapper:
 *		Find idle threads to swap, freeing up kernel stack resources
 *		at the expense of allowing them to execute.
 *
 *		Swap in threads that need to be run.  This is done here
 *		by the swapper thread since it cannot be done (in general)
 *		when the kernel tries to place a thread on a run queue.
 *
 *	Note: The act of swapping a thread in Mach does not mean that
 *	its memory gets forcibly swapped to secondary storage.  The memory
 *	for the task corresponding to a swapped thread is paged out
 *	through the normal paging mechanism.
 *
 */

#import <mach_old_vm_copy.h>

#import <kern/thread.h>
#import <kern/lock.h>
#import <vm/vm_map.h>
#import <vm/vm_kern.h>
#import <vm/vm_param.h>
#import <kern/sched_prim.h>
#import <kern/processor.h>
#import <kern/thread_swap.h>
#import <machine/machparam.h>		/* for splsched */

queue_head_t		swapin_queue;
decl_simple_lock_data(,	swapper_lock_data)

#define swapper_lock()		simple_lock(&swapper_lock_data)
#define swapper_unlock()	simple_unlock(&swapper_lock_data)

/*
 *	swapper_init: [exported]
 *
 *	Initialize the swapper module.
 */
void swapper_init()
{
	queue_init(&swapin_queue);
	simple_lock_init(&swapper_lock_data);
}

/*
 *	thread_swapin: [exported]
 *
 *	Place the specified thread in the list of threads to swapin.  It
 *	is assumed that the thread is locked, therefore we are at splsched.
 *	make_unswappable argument is used to ask swapin thread to make
 *	thread unswappable.
 */

void thread_swapin(thread, make_unswappable)
	thread_t	thread;
	boolean_t	make_unswappable;
{
	switch (thread->swap_state & TH_SW_STATE) {
	    case TH_SW_OUT:
		/*
		 *	Swapped out - queue for swapin thread
		 */
		thread->swap_state = TH_SW_COMING_IN;
		swapper_lock();
		enqueue_tail(&swapin_queue, (queue_entry_t) thread);
		swapper_unlock();
#if	NeXT
		thread_wakeup((int) &swapping_thread);
#else	NeXT
		thread_wakeup((int) &swapin_queue);
#endif	NeXT
		break;

	    case TH_SW_GOING_OUT:
		/*
		 *	Being swapped out - wait until swapped out,
		 *	then queue for swapin thread (in thread_swapout).
		 */
		thread->swap_state = TH_SW_WANT_IN;
		break;

	    case TH_SW_WANT_IN:
	    case TH_SW_COMING_IN:
		/*
		 *	Already queued for swapin thread, or being
		 *	swapped in
		 */
		break;

	    default:
		/*
		 *	Swapped in or unswappable
		 */
		panic("thread_swapin");
	}

	/*
	 *	Set make unswappable flag if asked to.  swapin thread
	 *	will make thread unswappable.
	 */
	if (make_unswappable)
		thread->swap_state |= TH_SW_MAKE_UNSWAPPABLE;
}

/*
 *	thread_doswapin:
 *
 *	Swapin the specified thread, if it should be runnable, then put
 *	it on a run queue.  No locks should be held on entry, as it is
 *	likely that this routine will sleep (waiting for page faults).
 */
void thread_doswapin(thread)
	thread_t	thread;
{
	register vm_offset_t	addr;
	register int		s;

	/*
	 *	Wire down the kernel stack.
	 */
#if	NeXT
	swapinStack(thread->kernel_stack);
#else	NeXT
	addr = thread->kernel_stack;
#if	MACH_OLD_VM_COPY
	(void) vm_map_pageable(kernel_map, trunc_page(addr),
			round_page(addr + KERNEL_STACK_SIZE),
			FALSE);
#else	MACH_OLD_VM_COPY
	(void) vm_map_pageable(kernel_map, trunc_page(addr),
			round_page(addr + KERNEL_STACK_SIZE),
			VM_PROT_READ|VM_PROT_WRITE);
#endif	MACH_OLD_VM_COPY
#endif	NeXT

	/*
	 *	Make unswappable and wake up waiting thread(s) if needed.
	 *	Place on run queue if appropriate.  
	 */
	s = splsched();
	thread_lock(thread);
	if (thread->swap_state & TH_SW_MAKE_UNSWAPPABLE) {
		thread->swap_state = TH_SW_UNSWAPPABLE;
		thread_wakeup(&thread->swap_state);
	}
	else {
		thread->swap_state = TH_SW_IN;
	}
	thread->state &= ~TH_SWAPPED;
	if (thread->state & TH_RUN)
		thread_setrun(thread, TRUE);
	thread_unlock(thread);
	(void) splx(s);
}

/*
 *	thread_swapout:
 *
 *	Swap out the specified thread (unwire its kernel stack).
 *	The thread must already be marked as 'swapping out'.
 */
void thread_swapout(thread)
	thread_t	thread;
{
	register vm_offset_t	addr;
	register boolean_t	make_unswappable;
	int			s;

	/*
	 *	Thread is marked as swapped before we swap it out; if
	 *	it is awakened while we are swapping it out, it will
	 *	be put on the swapin list.
	 */

	/*
	 * Notify the pcb module that it must update any
 	 * hardware state associated with this thread.
	 */
	pcb_synch(thread);
	
	/*
	 *	Unwire the kernel stack.
	 */
#if	NeXT
	swapoutStack(thread->kernel_stack);
#else	NeXT
	addr = thread->kernel_stack;
#if	MACH_OLD_VM_COPY
	(void) vm_map_pageable(kernel_map, trunc_page(addr),
			round_page(addr + KERNEL_STACK_SIZE), TRUE);
#else	MACH_OLD_VM_COPY
	(void) vm_map_pageable(kernel_map, trunc_page(addr),
			round_page(addr + KERNEL_STACK_SIZE), VM_PROT_NONE);
#endif	MACH_OLD_VM_COPY
#endif	NeXT

	pmap_collect(vm_map_pmap(thread->task->map));

	s = splsched();
	thread_lock(thread);
	switch (thread->swap_state & TH_SW_STATE) {
	    case TH_SW_GOING_OUT:
		thread->swap_state = TH_SW_OUT;
		break;

	    case TH_SW_WANT_IN:
		make_unswappable = thread->swap_state & TH_SW_MAKE_UNSWAPPABLE;
		thread->swap_state = TH_SW_OUT;
		thread_swapin(thread, make_unswappable);
		break;

	    default:
		panic("thread_swapout");
	}
	thread_unlock(thread);
	splx(s);
}

#if	!NeXT
/*
 *	swapin_thread: [exported]
 *
 *	This procedure executes as a kernel thread.  Threads that need to
 *	be swapped in are swapped in by this thread.
 */
void swapin_thread()
{
	while (TRUE) {
		register thread_t	thread;
		register int		s;

		s = splsched();
		swapper_lock();

		while ((thread = (thread_t) dequeue_head(&swapin_queue))
							== THREAD_NULL) {
			assert_wait((int) &swapin_queue, FALSE);
			swapper_unlock();
			splx(s);
			thread_block();
			s = splsched();
			swapper_lock();
		}

		swapper_unlock();
		splx(s);

		thread_doswapin(thread);
	}
}
#endif	!NeXT

boolean_t	thread_swapout_allowed = TRUE;
#if	NeXT
boolean_t	perform_swapout_scan = FALSE;
#endif	NeXT

int	thread_swap_tick = 0;
int	last_swap_tick = 0;

#define MAX_SWAP_RATE	60

/*
 *	swapout_threads: [exported]
 *
 *	This procedure is called periodically by the pageout daemon.  It
 *	determines if it should scan for threads to swap and starts that
 *	scan if appropriate.  (Algorithm is like that of old package)
 */
void swapout_threads()
{
	if (thread_swapout_allowed &&
	    (thread_swap_tick > (last_swap_tick + MAX_SWAP_RATE))) {
#if	NeXT
		perform_swapout_scan = TRUE;
		thread_wakeup((int) &swapping_thread);	/* poke swapper */
#else	NeXT
		last_swap_tick = thread_swap_tick;
#endif	NeXT
		thread_wakeup((int) &thread_swap_tick);	/* poke swapper */
	}
}

/*
 *	swapout_scan:
 *
 *	Scan the list of all threads looking for threads to swap.
 */
void swapout_scan()
{
	register thread_t	thread, prev_thread;
	processor_set_t		pset, prev_pset;
	register int		s;

	prev_thread = THREAD_NULL;
	prev_pset = PROCESSOR_SET_NULL;
	simple_lock(&all_psets_lock);
	pset = (processor_set_t) queue_first(&all_psets);
	while (!queue_end(&all_psets, (queue_entry_t) pset)) {
		pset_lock(pset);
		thread = (thread_t) queue_first(&pset->threads);
		while (!queue_end(&pset->threads, (queue_entry_t) thread)) {
			s = splsched();
			thread_lock(thread);
			if ((thread->state & (TH_RUN | TH_SWAPPED)) == 0 &&
			    thread->swap_state == TH_SW_IN &&
			    thread->interruptible &&
			    (sched_tick - thread->sched_stamp > 10)) {
				thread->state |= TH_SWAPPED;
				thread->swap_state = TH_SW_GOING_OUT;
				thread->ref_count++;
				thread_unlock(thread);
				(void) splx(s);
				pset->ref_count++;
				pset_unlock(pset);
				simple_unlock(&all_psets_lock);
				thread_swapout(thread);		/* swap it */

				if (prev_thread != THREAD_NULL)
					thread_deallocate(prev_thread);
				if (prev_pset != PROCESSOR_SET_NULL)
					pset_deallocate(prev_pset);

				prev_thread = thread;
				prev_pset = pset;
				simple_lock(&all_psets_lock);
				pset_lock(pset);
				s = splsched();
				thread_lock(thread);
			}
			thread_unlock(thread);
			splx(s);
			thread = (thread_t) queue_next(&thread->pset_threads);
		}
		pset_unlock(pset);
		pset = (processor_set_t) queue_next(&pset->all_psets);
	}
	simple_unlock(&all_psets_lock);

	if (prev_thread != THREAD_NULL)
		thread_deallocate(prev_thread);
	if (prev_pset != PROCESSOR_SET_NULL)
		pset_deallocate(prev_pset);
}

#if	!NeXT
/*
 *	swapout_thread: [exported]
 *
 *	Executes as a separate kernel thread.  This thread is periodically
 *	woken up.  When this happens, it initiates the scan for threads
 *	to swap.
 */
void swapout_thread()
{
	(void) spl0();
	while (TRUE) {
		assert_wait((int) &thread_swap_tick, FALSE);
		thread_block();
		swapout_scan();
	}
}
#endif	!NeXT

/*
 *	Mark a thread as swappable or unswappable.  May be called at
 *	any time.  No longer assumes thread is swapped in.
 */
void thread_swappable(thread, is_swappable)
	thread_t	thread;
	boolean_t	is_swappable;
{
	int	s = splsched();
	thread_lock(thread);
	if (is_swappable) {
	    if (thread->swap_state == TH_SW_UNSWAPPABLE)
		thread->swap_state = TH_SW_IN;
	}
	else {
	    switch(thread->swap_state) {
		case TH_SW_UNSWAPPABLE:
		    break;

		case TH_SW_IN:
		    thread->swap_state = TH_SW_UNSWAPPABLE;
		    break;

		default:
		    do {
			thread_swapin(thread, TRUE);
			thread_sleep(&thread->swap_state,
				     simple_lock_addr(thread->lock),
				     FALSE);
			thread_lock(thread);
		    } while (thread->swap_state != TH_SW_UNSWAPPABLE);
		    break;
	    }
	}
	thread_unlock(thread);
	(void) splx(s);
}

#if	NeXT
/*
 *	swapping_thread: [exported]
 *
 *	This procedure executes as a kernel thread.
 *	It provides the combined function of swapping threads in and out.
 */
void swapping_thread()
{
	while (TRUE) {
		register thread_t	thread;
		register int		s;

		s = splsched();
		swapper_lock();

		thread = (thread_t) dequeue_head(&swapin_queue);
		while ((thread == THREAD_NULL) && (!perform_swapout_scan)) {
			assert_wait((int) &swapping_thread, FALSE);
			swapper_unlock();
			splx(s);
			thread_block();
			s = splsched();
			swapper_lock();
			thread = (thread_t) dequeue_head(&swapin_queue);
		}

		swapper_unlock();
		splx(s);

		if (thread != THREAD_NULL)
			thread_doswapin(thread);

		if (perform_swapout_scan) {
			swapout_scan();
			perform_swapout_scan = FALSE;
		}
	}
}
#endif	NeXT

