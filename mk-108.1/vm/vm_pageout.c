/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_pageout.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	The proverbial page-out daemon.
 *
 * HISTORY
 * 30-Sep-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Prevent lost wakeups in pageout daemon (vm_page_alloc's
 *	that fail do not wakeup the page daemon --- and even if
 *	they did you'd have to wait for a new fault before poking
 *	the pageout daemon).
 *
 * 11-Apr-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH_XP: Artificially wire down pages sent to the default pager.
 *	Make sure that vm_pageout_page is called with the page removed
 *	from pageout queues.
 *
 * 29-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Prevent double pageout of pages belonging to internal objects.
 *
 *  8-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH_XP: Add "initial" argument to vm_pageout_page().
 *
 * 29-Feb-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Use vm_object_pager_create() to create and bind a pager to an
 *	internal object.
 *
 * 24-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	Handle IO errors on paging (non-XP only).
 *
 * 18-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Removed "max_flush" argument from vm_pageout_page().
 *	Eliminated old history.
 *
 *  6-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Allow more parameters to be set before boot.
 *
 * 18-Nov-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH_XP: Changed error message handling in vm_pageout_page().
 *
 */

#import <mach_xp.h>

#import <vm/vm_page.h>
#import <vm/pmap.h>
#import <vm/vm_object.h>
#import <vm/vm_pageout.h>
#import <vm/vm_statistics.h>
#import <vm/vm_param.h>
#import <kern/thread.h>
#import <kern/ipc_globals.h>
#import <machine/spl.h>

#if	MACH_XP
#import <kern/ipc_globals.h>
#import <kern/mach_types.h>
#import <kern/pager.h>
#import <kern/pager_default.h>
#import <sys/kern_port.h>		/* For PORT_OBJECT_PAGING_REQUEST */
#endif	MACH_XP

int	vm_pages_needed;		/* Event on which pageout daemon sleeps */
int	vm_pageout_free_min = 0;	/* Stop pageout to wait for pagers at this free level */

int	vm_page_free_min_sanity = 128*1024;

#if	MACH_XP
int	vm_pageout_debug = 0;
int	vm_pageout_double_laundry_count = 0;

/*
 *	Routine:	vm_pageout_page
 *	Purpose:
 *		Causes the specified page to be written back to
 *		its pager.  The page in question is not freed here,
 *		but will be freed by the pager when it is done.
 *		The page will be written regardless whether it
 *		is marked as clean.
 *
 *		The "initial" argument specifies whether this
 *		data is an initialization only, and should use
 *		pager_data_initialize instead of pager_data_write.
 *
 *	In/out conditions:
 *		The page in question must not be on any pageout queues.
 *		The object to which it belongs must be locked.
 *	Implementation:
 *		Move this page to a completely new object.
 */
void		vm_pageout_page(m, initial)
	vm_page_t	m;
	boolean_t	initial;
{
	vm_object_t	new_object;
	vm_object_t	old_object = m->object;
	vm_offset_t	paging_offset = m->offset + old_object->paging_offset;
	kern_return_t	rc;
	auto
	struct vm_page	holding_page;

	if (m->busy)
		panic("vm_pageout_page: busy page");

	/*
	 *	In order to create a new object, we must unlock
	 *	the old object; before doing so, we mark this page
	 *	busy (to prevent pagein) and increment paging_in_progress
	 *	(to prevent object termination).
	 */

	old_object->paging_in_progress++;

	/*
	 *	Create a place-holder page where the old one was, to prevent
	 *	anyone from attempting to page in this page while we've unlocked.
	 */

	vm_page_remove(m);
	vm_page_init(&holding_page, old_object, paging_offset, (vm_offset_t) 0);
	holding_page.fictitious = TRUE;

	vm_object_unlock(old_object);

	/*
	 *	Allocate a new object into which we can put the page.
	 *
	 *	If the old object was internal, then we shouldn't
	 *	ever page this out.
	 */

	new_object = vm_object_allocate(PAGE_SIZE);
	vm_object_lock(new_object);

	/*
	 *	Move this page into to the new object
	 */

	vm_page_insert(m, new_object, 0);

	/*
	 *	Put the old page on the pageout queues; if a bogus
	 *	user-supplied pager fails to release the page, it will
	 *	get paged out again to the default pager.
	 */

	vm_page_lock_queues();
	vm_stat.pageouts++;
	if (old_object->internal)
		m->wire_count++;
	 else
		vm_page_activate(m);
	vm_page_unlock_queues();

	/*
	 *	Mark the page as in the laundry, so we can detect
	 *	pages which aren't released, and so we can decide
	 *	when to stop the pageout scan.
	 */

	if (m->laundry) {
		if (vm_pageout_debug)
			printf("vm_pageout_page: double laundry page, object 0x%x offset 0x%x page 0x%x physical 0x%x!\n",
			       old_object, paging_offset, m, VM_PAGE_TO_PHYS(m));
		vm_pageout_double_laundry_count++;
	}
	 else {
		vm_page_laundry_count++;
		m->laundry = TRUE;
	}
	
	/*
	 *	Since IPC operations may block, we drop locks now.
	 *	[The placeholder page is busy, and we still have paging_in_progress
	 *	incremented.]
	 */

	vm_object_unlock(new_object);

	/*
	 *	Write the data to its pager.
	 *	Note that the data is passed by naming the new object,
	 *	not a virtual address; the pager interface has been;
	 *	manipulated to use the "internal memory" data type.
	 *	[The object reference from its allocation is donated
	 *	to the eventual recipient.]
	 */

	if ((rc = (initial ? pager_data_initialize : pager_data_write)
			(old_object->pager,
			 old_object->pager_request,
			 paging_offset, (pointer_t) new_object, PAGE_SIZE))
					!= KERN_SUCCESS) {
		if (vm_pageout_debug)
			printf("vm_pageout_page: pager_data_write failed, %d, page 0x%x (should panic).\n", rc, m);
		vm_page_free(m);
		vm_object_deallocate(new_object);
	}

	/*
	 *	Pick back up the old object lock to clean up
	 *	and to meet exit conditions.
	 */

	vm_object_lock(old_object);
	
	/*
	 *	Free the placeholder page to permit pageins to continue
	 *	[Don't need to hold queues lock, since this page should
	 *	never get on the pageout queues!]
	 */
	vm_page_free(&holding_page);

	if (--old_object->paging_in_progress == 0)
		thread_wakeup((int) old_object);
}

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
void		vm_pageout_scan()
{
	register vm_page_t	m;
	register int		page_shortage;
	register int		pages_moved;
	register int		s;

	/*
	 *	Only continue when we want more pages to be "free"
	 */

	s = splimp();
	simple_lock(&vm_page_queue_free_lock);
	if (vm_page_free_count < vm_page_free_target) {
		simple_unlock(&vm_page_queue_free_lock);
		splx(s);

		swapout_threads();

		/*
		 *	Be sure the pmap system is updated so
		 *	we can scan the inactive queue.
		 */

		pmap_update();
	} else {
		simple_unlock(&vm_page_queue_free_lock);
		splx(s);
	}

	/*
	 *	Start scanning the inactive queue for pages we can free.
	 *	We keep scanning until we have enough free pages or
	 *	we have scanned through the entire queue.  If we
	 *	encounter dirty pages, we start cleaning them.
	 *
	 *	NOTE:	The page queues lock is not held at loop entry,
	 *		but *is* held upon loop exit.
	 */

	pages_moved = 0;

	while (TRUE) {
		register vm_object_t	object;

		vm_page_lock_queues();

		m = (vm_page_t) queue_first(&vm_page_queue_inactive);
		if (queue_end(&vm_page_queue_inactive, (queue_entry_t) m))
			break;

		s = splimp();
		simple_lock(&vm_page_queue_free_lock);
		if ((((vm_page_free_count + vm_page_laundry_count) >= vm_page_free_target)) && (pages_moved > 0)) {
			simple_unlock(&vm_page_queue_free_lock);
			splx(s);
			break;
		}

		if ((vm_page_free_count < vm_pageout_free_min) && (vm_page_laundry_count > 0)) {
			vm_page_laundry_count--;
			simple_unlock(&vm_page_queue_free_lock);
			splx(s);
			break;
		}

		simple_unlock(&vm_page_queue_free_lock);
		splx(s);

		object = m->object;

		/*
		 *	If we don't have to clean it, and it's being used,
		 *	merely reactivate.
		 *	XXX mwyoung wonders why we care about m->clean here?
		 */

		if (m->clean && pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			vm_page_activate(m);
			vm_stat.reactivations++;
			vm_page_unlock_queues();
			continue;
		}

		/*
		 *	Try to lock object; since we've got the
		 *	page queues lock, we can only try for this one.
		 */
		if (!vm_object_lock_try(object)) {
			/*
			 *	Move page to end and continue.
			 */
			queue_remove(&vm_page_queue_inactive, m, vm_page_t, pageq);
			queue_enter(&vm_page_queue_inactive, m,	vm_page_t, pageq);
			vm_page_unlock_queues();
			continue;
		}

		/*
		 *	If it's clean, we can merely free the page.
		 */

		if (m->clean) {
			pmap_remove_all(VM_PAGE_TO_PHYS(m));
			vm_page_free(m);
			pages_moved++;
			vm_object_unlock(object);
			vm_page_unlock_queues();
			continue;
		}

		/*
		 *	Remove the page from the inactive list.
		 */

		queue_remove(&vm_page_queue_inactive, m, vm_page_t, pageq);
		vm_page_inactive_count--;
		m->inactive = FALSE;
		m->busy = TRUE;

		vm_page_unlock_queues();

		/*
		 *	Do a wakeup here in case the following
		 *	operations block.
		 */

		thread_wakeup((int) &vm_page_free_count);

		/*
		 *	If there is no pager for the page, create
		 *	one and hand it to the default pager.
		 *	[First try to collapse, so we don't make
		 *	a pager unnecessarily.]
		 */

		vm_object_collapse(object);

		if (object->pager == vm_pager_null) {
			vm_object_pager_create(object);
			vm_page_lock_queues();
			vm_page_activate(m);
			vm_page_unlock_queues();

			PAGE_WAKEUP(m);
			vm_object_unlock(object);
			continue;
		}

		PAGE_WAKEUP(m);

		pmap_remove_all(VM_PAGE_TO_PHYS(m));
		vm_pageout_page(m, FALSE);
		vm_object_unlock(object);
		pages_moved++;
	}

	/*
	 *	Compute the page shortage.  If we are still very low on memory
	 *	be sure that we will move a minimal amount of pages from active
	 *	to inactive.
	 */

	page_shortage = vm_page_inactive_target - vm_page_inactive_count;

	if ((page_shortage <= 0) && (pages_moved == 0))
		page_shortage = 1;

	while (page_shortage > 0) {
		/*
		 *	Move some more pages from active to inactive.
		 */

		if (queue_empty(&vm_page_queue_active)) {
			break;
		}
		m = (vm_page_t) queue_first(&vm_page_queue_active);
		vm_page_deactivate(m);
		page_shortage--;
	}

	vm_page_unlock_queues();
}

task_t	pageout_task;

#else	MACH_XP
/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
vm_pageout_scan()
{
	register vm_page_t	m;
	register int		page_shortage;
	register int		s;
	register int		pages_freed;
#if	NeXT
	boolean_t		free_pages;
#endif	NeXT

	/*
	 *	Only continue when we want more pages to be "free"
	 */

	s = splimp();
	simple_lock(&vm_page_queue_free_lock);
#if	NeXT
	free_pages = FALSE;
	if (vm_page_free_count < vm_page_free_min) {
		free_pages = TRUE;
#else	NeXT
	if (vm_page_free_count < vm_page_free_target) {
#endif	NeXT
		/*
		 *	See whether the physical mapping system
		 *	knows of any pages which are not being used.
		 */
		 
		simple_unlock(&vm_page_queue_free_lock);
		splx(s);
		swapout_threads();

		/*
		 *	And be sure the pmap system is updated so
		 *	we can scan the inactive queue.
		 */

		pmap_update();
	}
	else {
		simple_unlock(&vm_page_queue_free_lock);
		splx(s);
	}

	/*
	 *	Acquire the resident page system lock,
	 *	as we may be changing what's resident quite a bit.
	 */
	vm_page_lock_queues();

	/*
	 *	Start scanning the inactive queue for pages we can free.
	 *	We keep scanning until we have enough free pages or
	 *	we have scanned through the entire queue.  If we
	 *	encounter dirty pages, we start cleaning them.
	 */

	pages_freed = 0;
	m = (vm_page_t) queue_first(&vm_page_queue_inactive);
#if	NeXT
	while (free_pages && !queue_end(&vm_page_queue_inactive, (queue_entry_t) m)) {
#else	NeXT
	while (!queue_end(&vm_page_queue_inactive, (queue_entry_t) m)) {
#endif	NeXT
		vm_page_t	next;

		s = splimp();
		simple_lock(&vm_page_queue_free_lock);
		if (vm_page_free_count >= vm_page_free_target) {
			simple_unlock(&vm_page_queue_free_lock);
			splx(s);
			break;
		}
		simple_unlock(&vm_page_queue_free_lock);
		splx(s);

		if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			next = (vm_page_t) queue_next(&m->pageq);
			vm_page_activate(m);
			vm_stat.reactivations++;
			m = next;
			continue;
		}
		if (m->clean) {
			register vm_object_t	object;

			next = (vm_page_t) queue_next(&m->pageq);
			object = m->object;
			if (!vm_object_lock_try(object)) {
				/*
				 *	Can't lock object -
				 *	skip page.
				 */
				m = next;
				continue;
			}
#if	NeXT
			m->busy = TRUE;
			vm_page_unlock_queues();
#endif	NeXT
			pmap_remove_all(VM_PAGE_TO_PHYS(m));
#if	NeXT
			vm_page_lock_queues();
			m->busy = FALSE;
			PAGE_WAKEUP(m);
			
			next = (vm_page_t) queue_next(&m->pageq);
			vm_page_addfree(m);
#else	NeXT
			vm_page_free(m);	/* will dequeue */
#endif	NeXT
			pages_freed++;
			vm_object_unlock(object);
			m = next;
		}
		else {
			/*
			 *	If a page is dirty, then it is either
			 *	being washed (but not yet cleaned)
			 *	or it is still in the laundry.  If it is
			 *	still in the laundry, then we start the
			 *	cleaning operation.
			 */

			if (m->laundry) {
				/*
				 *	Clean the page and remove it from the
				 *	laundry.
				 *
				 *	We set the busy bit to cause
				 *	potential page faults on this page to
				 *	block.
				 *
				 *	And we set pageout-in-progress to keep
				 *	the object from disappearing during
				 *	pageout.  This guarantees that the
				 *	page won't move from the inactive
				 *	queue.  (However, any other page on
				 *	the inactive queue may move!)
				 */

				register vm_object_t	object;
				register vm_pager_t	pager;
				boolean_t	pageout_succeeded;

				object = m->object;
				if (!vm_object_lock_try(object)) {
					/*
					 *	Skip page if we can't lock
					 *	its object
					 */
					m = (vm_page_t) queue_next(&m->pageq);
					continue;
				}

#if	!NeXT
				pmap_remove_all(VM_PAGE_TO_PHYS(m));
#endif	!NeXT
				m->busy = TRUE;
				vm_stat.pageouts++;

				/*
				 *	Try to collapse the object before
				 *	making a pager for it.  We must
				 *	unlock the page queues first.
				 */
				vm_page_unlock_queues();

#if	NeXT
				/*
				 *  Moved this call from inside the queue lock to
				 *  prevent the following scenario: remove_all->
				 *  pmap_collapse->kmem_free->vm_page_lock_queues
				 */
				pmap_remove_all(VM_PAGE_TO_PHYS(m));
#endif	NeXT

				vm_object_collapse(object);

				object->paging_in_progress++;
				vm_object_unlock(object);


				/*
				 *	Do a wakeup here in case the following
				 *	operations block.
				 */
				thread_wakeup((int) &vm_page_free_count);

				/*
				 *	If there is no pager for the page,
				 *	use the default pager.  If there's
				 *	no place to put the page at the
				 *	moment, leave it in the laundry and
				 *	hope that there will be paging space
				 *	later.
				 */

				if ((pager = object->pager) == vm_pager_null) {
					pager = (vm_pager_t)vm_pager_allocate(
							object->size);
					if (pager != vm_pager_null) {
						vm_object_setpager(object,
							pager, 0, FALSE);
					}
				}

				pageout_succeeded = FALSE;
				if (pager != vm_pager_null) {
				    if (vm_pager_put(pager, m) == PAGER_SUCCESS) {
					pageout_succeeded = TRUE;
				    }
				}

				vm_object_lock(object);
				vm_page_lock_queues();
				/*
				 *	If page couldn't be paged out, then
				 *	reactivate the page so it doesn't
				 *	clog the inactive list.  (We will try
				 *	paging out it again later).
				 */
				next = (vm_page_t) queue_next(&m->pageq);
				if (pageout_succeeded)
					m->laundry = FALSE;
				else
					vm_page_activate(m);

				pmap_clear_reference(VM_PAGE_TO_PHYS(m));
				m->busy = FALSE;
				PAGE_WAKEUP(m);

				object->paging_in_progress--;
				thread_wakeup((int) object);
				vm_object_unlock(object);
				m = next;
			}
			else
				m = (vm_page_t) queue_next(&m->pageq);
		}
	}
	
	/*
	 *	Compute the page shortage.  If we are still very low on memory
	 *	be sure that we will move a minimal amount of pages from active
	 *	to inactive.
	 */

	page_shortage = vm_page_inactive_target - vm_page_inactive_count;
	page_shortage -= vm_page_free_count;

	if ((page_shortage <= 0) && (pages_freed == 0))
		page_shortage = 1;

	while (page_shortage > 0) {
		/*
		 *	Move some more pages from active to inactive.
		 */

		if (queue_empty(&vm_page_queue_active)) {
			break;
		}
		m = (vm_page_t) queue_first(&vm_page_queue_active);
		vm_page_deactivate(m);
		page_shortage--;
	}

	vm_page_unlock_queues();
}
#endif	MACH_XP

/*
 *	vm_pageout is the high level pageout daemon.
 */

void vm_pageout()
{
#if	MACH_XP
	pageout_task = current_task();
	pageout_task->kernel_vm_space = TRUE;
	pageout_task->kernel_ipc_space = TRUE;
	pageout_task->ipc_privilege = TRUE;

	port_reference(vm_pager_default);
	port_copyout(kernel_task, &vm_pager_default, MSG_TYPE_PORT);
#endif	MACH_XP
	current_thread()->vm_privilege = TRUE;

	(void) spl0();

	/*
	 *	Initialize some paging parameters.
	 */

	if (vm_page_free_min == 0) {
//		vm_page_free_min = vm_page_free_count / 20;
		vm_page_free_min = vm_page_free_count / 50;
		if (vm_page_free_min < 3)
			vm_page_free_min = 3;

		if (vm_page_free_min*PAGE_SIZE > vm_page_free_min_sanity)
			vm_page_free_min = vm_page_free_min_sanity/PAGE_SIZE;
	}

	if (vm_page_free_reserved == 0) {
//		if ((vm_page_free_reserved = vm_page_free_min / 4) < 3)
			vm_page_free_reserved = 3;
	}
	if (vm_pageout_free_min == 0) {
		if ((vm_pageout_free_min = vm_page_free_reserved / 2) > 10)
			vm_pageout_free_min = 10;
	}

	if (vm_page_free_target == 0)
//		vm_page_free_target = (vm_page_free_min * 4) / 3;
		vm_page_free_target = vm_page_free_min * 4;

	if (vm_page_inactive_target == 0)
		vm_page_inactive_target = vm_page_free_count / 3;

	if (vm_page_free_target <= vm_page_free_min)
		vm_page_free_target = vm_page_free_min + 1;

	if (vm_page_inactive_target <= vm_page_free_target)
		vm_page_inactive_target = vm_page_free_target + 1;

	/*
	 *	The pageout daemon is never done, so loop
	 *	forever.
	 */

	simple_lock(&vm_pages_needed_lock);
	while (TRUE) {
#if	NeXT
		if ((vm_page_free_count >= vm_page_free_min) &&
		    ((vm_page_free_count >= vm_page_free_target) ||
		     (vm_page_inactive_count > vm_page_inactive_target)))
#else	NeXT
		if ((vm_page_free_count > vm_page_free_min) &&
		    ((vm_page_free_count >= vm_page_free_target) ||
		     (vm_page_inactive_count > vm_page_inactive_target)))
#endif	NeXT
			thread_sleep((int) &vm_pages_needed,
				     &vm_pages_needed_lock, FALSE);
		else
			simple_unlock(&vm_pages_needed_lock);
		vm_pageout_scan();
		simple_lock(&vm_pages_needed_lock);
		thread_wakeup((int) &vm_page_free_count);
	}
}





