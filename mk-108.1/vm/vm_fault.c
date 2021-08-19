/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm_fault.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Page fault handling module.
 *
 * HISTORY
 * 06-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Place pages that are copy-on-written (the originals) on
 *	the inactive list is all cases.
 *
 *  8-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Fix vm_pageout_page() calling sequence again.
 *
 * 26-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Eliminate vm_page_t->fake field references.
 *
 *
 * 24-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	Handle IO errors on paging (non-XP only).
 *
 *  4-Feb-88  David Black (dlb) at Carnegie-Mellon University
 *	Pass map entry to vm_fault_{wire,unwire} instead of range.
 *	Added vm_fault_wire_fast to optimize wire-down faults.
 *	Added paranoia check for protection change during a fault
 *	to wire a page.
 *
 *
 * 11-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Allow page faults (except those for wiring down memory) to be
 *	interrupted.
 *
 * 10-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Changed to use vm_map_verify() and vm_map_verify_done().
 *	
 *  1-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Fixed VM object leakage when finding a busy page in main loop.
 *	
 *	Removed old history.  Contributors so far are:
 *		avie, dbg, mwyoung, bolosky
 */
#import <mach_xp.h>

#define	USE_VERSIONS	MACH_XP

#import <sys/kern_return.h>
#import <sys/message.h>	/* for error codes */
#import <kern/thread.h>
#import <kern/sched_prim.h>
#import <vm/vm_map.h>
#import <vm/vm_object.h>
#import <vm/vm_page.h>
#import <vm/pmap.h>
#import <vm/vm_statistics.h>
#import <vm/vm_pageout.h>
#import <vm/vm_param.h>
#import <next/kernel_pmon.h>

/*
 *	vm_fault:
 *
 *	Handle a page fault occuring at the given address,
 *	requiring the given permissions, in the map specified.
 *	If successful, the page is inserted into the
 *	associated physical map.
 *
 *	NOTE: the given address should be truncated to the
 *	proper page address.
 *
 *	KERN_SUCCESS is returned if the page fault is handled; otherwise,
 *	a standard error specifying why the fault is fatal is returned.
 *
 *
 *	The map in question must be referenced, and remains so.
 *	Caller may hold no locks.
 */
kern_return_t vm_fault(map, vaddr, fault_type, change_wiring)
	vm_map_t	map;
	vm_offset_t	vaddr;
	vm_prot_t	fault_type;
	boolean_t	change_wiring;
{
	vm_object_t		first_object;
	vm_offset_t		first_offset;
#if	USE_VERSIONS
	vm_map_version_t	version;
#else	USE_VERSIONS
	vm_map_entry_t		entry;
#endif	USE_VERSIONS
	register vm_object_t	object;
	register vm_offset_t	offset;
	register vm_page_t	m;
	vm_page_t		first_m;
	vm_prot_t		prot;
	kern_return_t		result;
	boolean_t		wired;
	boolean_t		su;
#if	!USE_VERSIONS
	boolean_t		lookup_still_valid;
#endif	!USE_VERSIONS
	boolean_t		page_exists;
	vm_page_t		old_m;
	vm_object_t		next_object;

	pmon_log_event(PMON_SOURCE_VM, KP_VM_FAULT, vaddr, current_task(), 0);
	vm_stat.faults++;		/* needs lock XXX */
/*
 *	Recovery actions
 */
#if	MACH_XP
/* vm_page_free does a PAGE_WAKEUP anyway */
#define	FREE_PAGE(m) {					\
	vm_page_lock_queues();				\
	vm_page_free(m);				\
	vm_page_unlock_queues();			\
}
#else	MACH_XP
#define	FREE_PAGE(m)	{				\
	PAGE_WAKEUP(m);					\
	vm_page_lock_queues();				\
	vm_page_free(m);				\
	vm_page_unlock_queues();			\
}
#endif	MACH_XP

#define	RELEASE_PAGE(m)	{				\
	PAGE_WAKEUP(m);					\
	vm_page_lock_queues();				\
	vm_page_activate(m);				\
	vm_page_unlock_queues();			\
}

#if	USE_VERSIONS
#define	UNLOCK_MAP
#else	USE_VERSIONS
#define	UNLOCK_MAP	{				\
	if (lookup_still_valid) {			\
		vm_map_lookup_done(map, entry);		\
		lookup_still_valid = FALSE;		\
	}						\
}
#endif	USE_VERSIONS

#define	UNLOCK_THINGS	{				\
	object->paging_in_progress--;			\
	vm_object_unlock(object);			\
	if (object != first_object) {			\
		vm_object_lock(first_object);		\
		FREE_PAGE(first_m);			\
		first_object->paging_in_progress--;	\
		vm_object_unlock(first_object);		\
	}						\
	UNLOCK_MAP;					\
}

#define	UNLOCK_AND_DEALLOCATE	{			\
	UNLOCK_THINGS;					\
	vm_object_deallocate(first_object);		\
}

    RetryFault: ;

	/*
	 *	Find the backing store object and offset into
	 *	it to begin the search.
	 */

#if	USE_VERSIONS
	if ((result = vm_map_lookup(&map, vaddr, fault_type, &version,
#else	USE_VERSIONS
	if ((result = vm_map_lookup(&map, vaddr, fault_type, &entry,
#endif	USE_VERSIONS
			&first_object, &first_offset,
			&prot, &wired, &su)) != KERN_SUCCESS) {
		return(result);
	}
#if	!USE_VERSIONS
	lookup_still_valid = TRUE;
#endif	!USE_VERSIONS

	if (wired)
		fault_type = prot;

	first_m = VM_PAGE_NULL;

   	/*
	 *	Make a reference to this object to
	 *	prevent its disposal while we are messing with
	 *	it.  Once we have the reference, the map is free
	 *	to be diddled.  Since objects reference their
	 *	shadows (and copies), they will stay around as well.
	 */

#if	!USE_VERSIONS
	vm_object_lock(first_object);
#endif	!USE_VERSIONS

	first_object->ref_count++;
	first_object->paging_in_progress++;

	/*
	 *	INVARIANTS (through entire routine):
	 *
	 *	1)	At all times, we must either have the object
	 *		lock or a busy page in some object to prevent
	 *		some other thread from trying to bring in
	 *		the same page.
	 *
	 *		Note that we cannot hold any locks during the
	 *		pager access or when waiting for memory, so
	 *		we use a busy page then.
	 *
	 *		Note also that we aren't as concerned about
	 *		more than one thead attempting to pager_data_unlock
	 *		the same page at once, so we don't hold the page
	 *		as busy then, but do record the highest unlock
	 *		value so far.  [Unlock requests may also be delivered
	 *		out of order.]
	 *
	 *	2)	Once we have a busy page, we must remove it from
	 *		the pageout queues, so that the pageout daemon
	 *		will not grab it away.
	 *
	 *	3)	To prevent another thread from racing us down the
	 *		shadow chain and entering a new page in the top
	 *		object before we do, we must keep a busy page in
	 *		the top object while following the shadow chain.
	 *
	 *	4)	We must increment paging_in_progress on any object
	 *		for which we have a busy page, to prevent
	 *		vm_object_collapse from removing the busy page
	 *		without our noticing.
	 */

	/*
	 *	Search for the page at object/offset.
	 */

	object = first_object;
	offset = first_offset;

	/*
	 *	See whether this page is resident
	 */

	while (TRUE) {
		m = vm_page_lookup(object, offset);
		if (m != VM_PAGE_NULL) {
			/*
			 *	If the page is in error, give up now.
			 */

			if (m->error) {
				FREE_PAGE(m);
				UNLOCK_AND_DEALLOCATE;
				return(KERN_MEMORY_ERROR);
			}

			/*
			 *	If the page is being brought in,
			 *	wait for it and then retry.
			 */
			if (m->busy) {
				kern_return_t	wait_result;
				PAGE_ASSERT_WAIT(m, !change_wiring);
				UNLOCK_MAP;				
				vm_object_unlock(object);
				thread_block();
				wait_result = current_thread()->wait_result;
				vm_object_lock(object);
				if (wait_result == THREAD_RESTART) {
					UNLOCK_AND_DEALLOCATE;
					goto RetryFault;
				}
				if (wait_result != THREAD_AWAKENED) {
					UNLOCK_AND_DEALLOCATE;
					return(KERN_SUCCESS);
				}
				continue;
			}

			/*
			 *	If the page isn't busy, but is absent,
			 *	then it was deemed "unavailable".
			 */

			if (m->absent) {
				/*
				 * Remove the non-existent page (unless it's
				 * in the top object) and move on down to the
				 * next object (if there is one).
				 */
				offset += object->shadow_offset;
				next_object = object->shadow;
				if (next_object == VM_OBJECT_NULL) {
					/*
					 * Absent page at bottom of shadow
					 * chain; zero fill the page we left
					 * busy in the first object, and flush
					 * the absent page.
					 */
					if (object != first_object) {
						m->busy = m->absent = FALSE;
						FREE_PAGE(m);
						object->paging_in_progress--;
						vm_object_unlock(object);
						object = first_object;
						offset = first_offset;
						m = first_m;
						vm_object_lock(object);
					}
					first_m = VM_PAGE_NULL;
					vm_page_zero_fill(m);
					vm_stat.zero_fill_count++;
					m->absent = FALSE;
				} else {
					if (object != first_object) {
						object->paging_in_progress--;
						FREE_PAGE(m);
					} else {
						first_m = m;
						m->absent = FALSE;
					}
					vm_object_lock(next_object);
					vm_object_unlock(object);
					object = next_object;
					object->paging_in_progress++;
					continue;
				}
			}

			/*
			 *	If the desired access to this page has
			 *	been locked out, request that it be unlocked.
			 */

			if (fault_type & m->page_lock) {
				kern_return_t	wait_result;

				if ((fault_type & m->unlock_request) != fault_type) {
					vm_prot_t	new_unlock_request;
					kern_return_t	rc;
					
					new_unlock_request = m->unlock_request =
						(fault_type | m->unlock_request);
					vm_object_unlock(object);
					if ((rc = pager_data_unlock(
						object->pager,
						object->pager_request,
						offset + object->paging_offset,
						PAGE_SIZE,
						new_unlock_request))
					     != KERN_SUCCESS) {
					     	printf("vm_fault: pager_data_unlock failed\n");
						vm_object_lock(object);
						UNLOCK_AND_DEALLOCATE;
						return((rc == SEND_INTERRUPTED) ?
							KERN_SUCCESS :
							KERN_MEMORY_ERROR);
					}
					vm_object_lock(object);
					continue;
				}

				PAGE_ASSERT_WAIT(m, !change_wiring);
				UNLOCK_THINGS;
				thread_block();
				wait_result = current_thread()->wait_result;
				vm_object_deallocate(first_object);
				if (wait_result != THREAD_AWAKENED)
					return(KERN_SUCCESS);
				goto RetryFault;
			}

			/*
			 *	Remove the page from the pageout daemon's
			 *	reach while we play with it.
			 */

			vm_page_lock_queues();
			if (m->inactive) {
				queue_remove(&vm_page_queue_inactive, m,
						vm_page_t, pageq);
				m->inactive = FALSE;
				vm_page_inactive_count--;
				vm_stat.reactivations++;
				pmon_log_event(PMON_SOURCE_VM, KP_VM_REACTIVE,
					       vaddr, current_task(), 0);
			} 

			if (m->active) {
				queue_remove(&vm_page_queue_active, m,
						vm_page_t, pageq);
				m->active = FALSE;
				vm_page_active_count--;
			}
#if	NeXT
			if (m->free) {
				/*
				 * We only get here on reactivation of a free page,
				 * vm_page_alloc takes care of this for us in the
				 * typical case.
				 */
				queue_remove(&vm_page_queue_free, m,
						vm_page_t, pageq);
				m->free = FALSE;
				vm_page_free_count--;
				vm_stat.reactivations++;
				pmon_log_event(PMON_SOURCE_VM, KP_VM_FREE_REACTIVE,
					       vaddr, current_task(), 0);
			}
#endif	NeXT
			vm_page_unlock_queues();

			/*
			 *	Mark page busy for other threads.
			 */
			m->busy = TRUE;
			m->absent = FALSE;
			break;
		}

		if (((object->pager != vm_pager_null) &&
				(!change_wiring || wired))
		    || (object == first_object)) {

			/*
			 *	Allocate a new page for this object/offset
			 *	pair.
			 */

			m = vm_page_alloc(object, offset);

			if (m == VM_PAGE_NULL) {
				UNLOCK_AND_DEALLOCATE;
				VM_WAIT;
				goto RetryFault;
			}
		}

		if ((object->pager != vm_pager_null) &&
				(!change_wiring || wired)) {
#if	MACH_XP
			kern_return_t	rc;
#else	MACH_XP
			pager_return_t	rc;
#endif	MACH_XP

			/*
			 *	Now that we have a busy page, we can
			 *	release the object lock.
			 */
			vm_object_unlock(object);

			/*
			 *	Call the pager to retrieve the data, if any,
			 *	after releasing the lock on the map.
			 */
			UNLOCK_MAP;

#if	MACH_XP
			m->absent = TRUE;

			vm_stat.pageins++;
			if ((rc = pager_data_request(object->pager, 
				object->pager_request,
				m->offset + object->paging_offset, 
				PAGE_SIZE, fault_type)) != KERN_SUCCESS) {
				if (rc != SEND_INTERRUPTED)
					printf("%s(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) failed, %d\n",
						"pager_data_request",
						object->pager,
						object->pager_request,
						m->offset + object->paging_offset, 
						PAGE_SIZE, fault_type, rc);
				vm_object_lock(object);
				FREE_PAGE(m);
				UNLOCK_AND_DEALLOCATE;
				return((rc == SEND_INTERRUPTED) ?
					KERN_SUCCESS : KERN_MEMORY_ERROR);
			}
			
			/*
			 * Retry with same object/offset, since new data may
			 * be in a different page (ie, m is meaningless at
			 * this point).
			 */
			vm_object_lock(object);
			continue;
#else	MACH_XP
			pmon_log_event(PMON_SOURCE_VM, KP_VM_PAGEIN_START,
					vaddr, current_task(), 0);
			rc = vm_pager_get(object->pager, m);
			pmon_log_event(PMON_SOURCE_VM, KP_VM_PAGEIN_DONE,
					vaddr, current_task(), 0);
			if (rc == PAGER_SUCCESS) {

				/*
				 *	Found the page.
				 *	Leave it busy while we play with it.
				 */
				vm_object_lock(object);

				/*
				 *	Relookup in case pager changed page.
				 *	Pager is responsible for disposition
				 *	of old page if moved.
				 */
				m = vm_page_lookup(object, offset);

				vm_stat.pageins++;
				pmap_clear_modify(VM_PAGE_TO_PHYS(m));
				break;
			}
			if (rc == PAGER_ERROR) {
				/*
				 *	Pager had the page, but could not
				 *	read it.  Return error to stop caller.
				 */
				vm_object_lock(object);
				FREE_PAGE(m);
				UNLOCK_AND_DEALLOCATE;
				return(KERN_MEMORY_ERROR);
			}

			/*
			 *	Remove the bogus page (which does not
			 *	exist at this object/offset); before
			 *	doing so, we must get back our object
			 *	lock to preserve our invariant.
			 *
			 *	Also wake up any other thread that may want
			 *	to bring in this page.
			 *
			 *	If this is the top-level object, we must
			 *	leave the busy page to prevent another
			 *	thread from rushing past us, and inserting
			 *	the page in that object at the same time
			 *	that we are.
			 */

			vm_object_lock(object);
			if (object != first_object) {
				FREE_PAGE(m);
			}
#endif	MACH_XP
		}

		/*
		 * For the XP system, the only case in which we get here is if
		 * object has no pager (or unwiring).  If the pager doesn't
		 * have the page this is handled in the m->absent case above
		 * (and if you change things here you should look above).
		 */
		if (object == first_object)
			first_m = m;

		/*
		 *	Move on to the next object.  Lock the next
		 *	object before unlocking the current one.
		 */

		offset += object->shadow_offset;
		next_object = object->shadow;
		if (next_object == VM_OBJECT_NULL) {
			/*
			 *	If there's no object left, fill the page
			 *	in the top object with zeros.
			 */
			if (object != first_object) {
				object->paging_in_progress--;
				vm_object_unlock(object);

				object = first_object;
				offset = first_offset;
				m = first_m;
				vm_object_lock(object);
			}
			first_m = VM_PAGE_NULL;

			vm_page_zero_fill(m);
			vm_stat.zero_fill_count++;
			m->absent = FALSE;
			break;
		}
		else {
			vm_object_lock(next_object);
			if (object != first_object)
				object->paging_in_progress--;
			vm_object_unlock(object);
			object = next_object;
			object->paging_in_progress++;
		}
	}

	if (m->absent || m->active || m->inactive || !m->busy)
		panic("vm_fault: absent or active or inactive or not busy after main loop");

	/*
	 *	PAGE HAS BEEN FOUND.
	 *	[Loop invariant still holds -- the object lock
	 *	is held.]
	 */

	old_m = m;	/* save page that would be copied */

	/*
	 *	If the page is being written, but isn't
	 *	already owned by the top-level object,
	 *	we have to copy it into a new page owned
	 *	by the top-level object.
	 */

	if (object != first_object) {
	    	/*
		 *	We only really need to copy if we
		 *	want to write it.
		 */

	    	if (fault_type & VM_PROT_WRITE) {

			/*
			 *	If we try to collapse first_object at this
			 *	point, we may deadlock when we try to get
			 *	the lock on an intermediate object (since we
			 *	have the bottom object locked).  We can't
			 *	unlock the bottom object, because the page
			 *	we found may move (by collapse) if we do.
			 *
			 *	Instead, we first copy the page.  Then, when
			 *	we have no more use for the bottom object,
			 *	we unlock it and try to collapse.
			 *
			 *	Note that we copy the page even if we didn't
			 *	need to... that's the breaks.
			 */

		    	/*
			 *	We already have an empty page in
			 *	first_object - use it.
			 */

			vm_page_copy(m, first_m);
			first_m->absent = FALSE;

			/*
			 *	If another map is truly sharing this
			 *	page with us, we have to flush all
			 *	uses of the original page, since we
			 *	can't distinguish those which want the
			 *	original from those which need the
			 *	new copy.
			 */

			vm_page_lock_queues();
			/*
			 *	We first activate the page, then deactivate
			 *	it since vm_page_deactivate will only
			 *	deactivate active pages.
			 */
			vm_page_activate(m);
			vm_page_deactivate(m);
			if (!su)
				pmap_remove_all(VM_PAGE_TO_PHYS(m));
			vm_page_unlock_queues();

			/*
			 *	We no longer need the old page or object.
			 */
			PAGE_WAKEUP(m);
			object->paging_in_progress--;
			vm_object_unlock(object);

			/*
			 *	Only use the new page below...
			 */

			vm_stat.cow_faults++;
			m = first_m;
			object = first_object;
			offset = first_offset;

			/*
			 *	Now that we've gotten the copy out of the
			 *	way, let's try to collapse the top object.
			 */
			vm_object_lock(object);
			/*
			 *	But we have to play ugly games with
			 *	paging_in_progress to do that...
			 */
			object->paging_in_progress--;
			vm_object_collapse(object);
			object->paging_in_progress++;
		}
		else {
		    	prot &= (~VM_PROT_WRITE);
			m->copy_on_write = TRUE;
		}
	}

	if (m->active || m->inactive)
		panic("vm_fault: active or inactive before copy object handling");

	/*
	 *	If the page is being written, but hasn't been
	 *	copied to the copy-object, we have to copy it there.
	 */
    RetryCopy:
	if (first_object->copy != VM_OBJECT_NULL) {
		vm_object_t		copy_object = first_object->copy;
		vm_offset_t		copy_offset;
		vm_page_t		copy_m;

		/*
		 *	We only need to copy if we want to write it.
		 */
		if ((fault_type & VM_PROT_WRITE) == 0) {
			prot &= ~VM_PROT_WRITE;
			m->copy_on_write = TRUE;
		}
		else {
			/*
			 *	Try to get the lock on the copy_object.
			 */
			if (!vm_object_lock_try(copy_object)) {
				vm_object_unlock(object);
				/* should spin a bit here... */
				vm_object_lock(object);
				goto RetryCopy;
			}

			/*
			 *	Make another reference to the copy-object,
			 *	to keep it from disappearing during the
			 *	copy.
			 */
			copy_object->ref_count++;

			/*
			 *	Does the page exist in the copy?
			 */
			copy_offset = first_offset
				- copy_object->shadow_offset;
			copy_m = vm_page_lookup(copy_object, copy_offset);
			if (page_exists = (copy_m != VM_PAGE_NULL)) {
				if (copy_m->busy) {
					kern_return_t	wait_result;

					/*
					 *	If the page is being brought
					 *	in, wait for it and then retry.
					 */
					PAGE_ASSERT_WAIT(copy_m, !change_wiring);
					RELEASE_PAGE(m);
					copy_object->ref_count--;
					vm_object_unlock(copy_object);
					UNLOCK_THINGS;
					thread_block();
					wait_result = current_thread()->wait_result;
					vm_object_deallocate(first_object);
							/* may block */
					if (wait_result != THREAD_AWAKENED)
						return(KERN_SUCCESS);
					goto RetryFault;
				}
			}

#if	MACH_XP
			 else {
				/*
				 *	Allocate a page for the copy
				 */
				copy_m = vm_page_alloc(copy_object,
								copy_offset);
				if (copy_m == VM_PAGE_NULL) {
					/*
					 *	Wait for a page, then retry.
					 */
					RELEASE_PAGE(m);
					copy_object->ref_count--;
					vm_object_unlock(copy_object);
					UNLOCK_AND_DEALLOCATE;
					VM_WAIT;
					goto RetryFault;
				}

				/*
				 *	Must copy page into copy-object.
				 */

				vm_page_copy(m, copy_m);
				m->copy_on_write = FALSE;
				copy_m->absent = FALSE;
				
				/*
				 *	If the old page was in use by any users
				 *	of the copy-object, it must be removed
				 *	from all pmaps.  (We can't know which
				 *	pmaps use it.)
				 */

				vm_page_lock_queues();
				pmap_remove_all(VM_PAGE_TO_PHYS(old_m));
				copy_m->clean = FALSE;
				vm_page_unlock_queues();

				/*
				 *	If there's a pager, then immediately
				 *	page out this page, using the "initialize"
				 *	option.  Else, we use the copy.
				 */

			 	if (copy_object->pager == vm_pager_null) {
					vm_page_lock_queues();
					vm_page_activate(copy_m);
					vm_page_unlock_queues();
					PAGE_WAKEUP(copy_m);
				} else {
					/*
					 *	Prepare the page for pageout:
					 *
					 *	Since it was just allocated,
					 *	it is not on a pageout queue, but
					 *	it is busy.
					 */

					copy_m->busy = FALSE;

					/*
					 *	Unlock everything except the
					 *	copy_object itself.
					 */

					vm_object_unlock(object);
					UNLOCK_MAP;

					vm_pageout_page(copy_m, TRUE);

					/*
					 *	Since the pageout may have
					 *	temporarily dropped the
					 *	copy_object's lock, we
					 *	check whether we'll have
					 *	to deallocate the hard way.
					 */

					if ((copy_object->shadow != object) ||
					    (copy_object->ref_count == 1)) {
						vm_object_unlock(copy_object);
						vm_object_deallocate(copy_object);
						vm_object_lock(object);
						goto RetryCopy;
					}
					
					/*
					 *	Pick back up the old object's
					 *	lock.  [It is safe to do so,
					 *	since it must be deeper in the
					 *	object tree.]
					 */
					
					vm_object_lock(object);
				}
			}
#if	defined(lint) || defined(hc)
			if (++page_exists != 0)
				panic("lint");
#endif	defined(lint) || defined(hc)
#else	MACH_XP
			/*
			 *	If the page is not in memory (in the object)
			 *	and the object has a pager, we have to check
			 *	if the pager has the data in secondary
			 *	storage.
			 */
			if (!page_exists) {

				/*
				 *	If we don't allocate a (blank) page
				 *	here... another thread could try
				 *	to page it in, allocate a page, and
				 *	then block on the busy page in its
				 *	shadow (first_object).  Then we'd
				 *	trip over the busy page after we
				 *	found that the copy_object's pager
				 *	doesn't have the page...
				 */
				copy_m = vm_page_alloc(copy_object,
								copy_offset);
				if (copy_m == VM_PAGE_NULL) {
					/*
					 *	Wait for a page, then retry.
					 */
					RELEASE_PAGE(m);
					copy_object->ref_count--;
					vm_object_unlock(copy_object);
					UNLOCK_AND_DEALLOCATE;
					VM_WAIT;
					goto RetryFault;
				}

			 	if (copy_object->pager != vm_pager_null) {
					vm_object_unlock(object);
					vm_object_unlock(copy_object);
					UNLOCK_MAP;

					page_exists = vm_pager_has_page(
							copy_object->pager,
							(copy_offset + copy_object->paging_offset));

					vm_object_lock(copy_object);

					/*
					 * Since the map is unlocked, someone
					 * else could have copied this object
					 * and put a different copy_object
					 * between the two.  Or, the last
					 * reference to the copy-object (other
					 * than the one we have) may have
					 * disappeared - if that has happened,
					 * we don't need to make the copy.
					 */
					if (copy_object->shadow != object ||
					    copy_object->ref_count == 1) {
						/*
						 *	Gaah... start over!
						 */
						FREE_PAGE(copy_m);
						vm_object_unlock(copy_object);
						vm_object_deallocate(copy_object);
							/* may block */
						vm_object_lock(object);
						goto RetryCopy;
					}
					vm_object_lock(object);

					if (page_exists) {
						/*
						 *	We didn't need the page
						 */
						FREE_PAGE(copy_m);
					}
				}
			}
			if (!page_exists) {
				/*
				 *	Must copy page into copy-object.
				 */
				vm_page_copy(m, copy_m);
				copy_m->absent = FALSE;

				/*
				 * Things to remember:
				 * 1. The copied page must be marked 'dirty'
				 *    so it will be paged out to the copy
				 *    object.
				 * 2. If the old page was in use by any users
				 *    of the copy-object, it must be removed
				 *    from all pmaps.  (We can't know which
				 *    pmaps use it.)
				 */
				vm_page_lock_queues();
				pmap_remove_all(VM_PAGE_TO_PHYS(old_m));
				copy_m->clean = FALSE;
				vm_page_activate(copy_m);	/* XXX */
				vm_page_unlock_queues();

				PAGE_WAKEUP(copy_m);
			}
#endif	MACH_XP
			/*
			 *	The reference count on copy_object must be
			 *	at least 2: one for our extra reference,
			 *	and at least one from the outside world
			 *	(we checked that when we last locked
			 *	copy_object).
			 */
			copy_object->ref_count--;
			vm_object_unlock(copy_object);
			m->copy_on_write = FALSE;
		}
	}

	if (m->active || m->inactive)
		panic("vm_fault: active or inactive before retrying lookup");

	/*
	 *	We must verify that the maps have not changed
	 *	since our last lookup.
	 */

#if	USE_VERSIONS
	vm_object_unlock(object);
	while (!vm_map_verify(map, &version)) {
		vm_object_t	retry_object;
		vm_offset_t	retry_offset;
		vm_prot_t	retry_prot;

		/*
		 *	To avoid trying to write_lock the map while another
		 *	thread has it read_locked (in vm_map_pageable), we
		 *	do not try for write permission.  If the page is
		 *	still writable, we will get write permission.  If it
		 *	is not, or has been marked needs_copy, we enter the
		 *	mapping without write permission, and will merely
		 *	take another fault.
		 */
		result = vm_map_lookup(&map, vaddr,
				fault_type & ~VM_PROT_WRITE, &version,
				&retry_object, &retry_offset, &retry_prot,
				&wired, &su);

		if (result != KERN_SUCCESS) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			return(result);
		}

		vm_object_unlock(retry_object);
		vm_object_lock(object);

		if ((retry_object != first_object) ||
				(retry_offset != first_offset)) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			goto RetryFault;
		}

		/*
		 *	Check whether the protection has changed or the object
		 *	has been copied while we left the map unlocked.
		 *	Changing from read to write permission is OK - we leave
		 *	the page write-protected, and catch the write fault.
		 *	Changing from write to read permission means that we
		 *	can't mark the page write-enabled after all.
		 */
		prot &= retry_prot;
		if (m->copy_on_write)
			prot &= ~VM_PROT_WRITE;

		vm_object_unlock(object);
	}
	vm_object_lock(object);
#else	USE_VERSIONS

	if (!lookup_still_valid) {
		vm_object_t	retry_object;
		vm_offset_t	retry_offset;
		vm_prot_t	retry_prot;

		/*
		 *	Since map entries may be pageable, make sure we can
		 *	take a page fault on them.
		 */
		vm_object_unlock(object);

		/*
		 *	To avoid trying to write_lock the map while another
		 *	thread has it read_locked (in vm_map_pageable), we
		 *	do not try for write permission.  If the page is
		 *	still writable, we will get write permission.  If it
		 *	is not, or has been marked needs_copy, we enter the
		 *	mapping without write permission, and will merely
		 *	take another fault.
		 */

		result = vm_map_lookup(&map, vaddr,
				fault_type & ~VM_PROT_WRITE, &entry,
				&retry_object, &retry_offset, &retry_prot,
				&wired, &su);
		vm_object_lock(object);

		/*
		 *	If we don't need the page any longer, put it on the
		 *	active list (the easiest thing to do here).  If no
		 *	one needs it, pageout will grab it eventually.
		 */

		if (result != KERN_SUCCESS) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			return(result);
		}

		lookup_still_valid = TRUE;

		if ((retry_object != first_object) ||
				(retry_offset != first_offset)) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			goto RetryFault;
		}

		/*
		 *	Check whether the protection has changed or the object
		 *	has been copied while we left the map unlocked.
		 *	Changing from read to write permission is OK - we leave
		 *	the page write-protected, and catch the write fault.
		 *	Changing from write to read permission means that we
		 *	can't mark the page write-enabled after all.
		 */
		prot &= retry_prot;
		if (m->copy_on_write)
			prot &= ~VM_PROT_WRITE;
		/*
		 *	Can't catch write fault if page is to be wired.  This
		 *	should never happen because caller holds a read lock
		 *	on the map.
		 */
		if (wired && (prot != fault_type)) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			goto RetryFault;
		}
	}
#endif	USE_VERSIONS

	/*
	 * (the various bits we're fiddling with here are locked by
	 * the object's lock)
	 */

	/* XXX This distorts the meaning of the copy_on_write bit */

	if (prot & VM_PROT_WRITE)
		m->copy_on_write = FALSE;

	/*
	 *	It's critically important that a wired-down page be faulted
	 *	only once in each map for which it is wired.
	 */

	if (m->active || m->inactive)
		panic("vm_fault: active or inactive before pmap_enter");

	vm_object_unlock(object);

	/*
	 *	Put this page into the physical map.
	 *	We had to do the unlock above because pmap_enter
	 *	may cause other faults.   We don't put the
	 *	page back on the active queue until later so
	 *	that the page-out daemon won't find us (yet).
	 */

	pmap_enter(map->pmap, vaddr, VM_PAGE_TO_PHYS(m), 
			prot & ~(m->page_lock), wired);

	/*
	 *	If the page is not wired down, then put it where the
	 *	pageout daemon can find it.
	 */
	vm_object_lock(object);
	vm_page_lock_queues();
	if (change_wiring) {
		if (wired)
			vm_page_wire(m);
		else
			vm_page_unwire(m);
	}
	else
		vm_page_activate(m);
	vm_page_unlock_queues();

	/*
	 *	Unlock everything, and return
	 */

#if	USE_VERSIONS
	vm_map_verify_done(map, &version);
#endif	USE_VERSIONS
	PAGE_WAKEUP(m);
	UNLOCK_AND_DEALLOCATE;

	return(KERN_SUCCESS);

}

kern_return_t	vm_fault_wire_fast();

/*
 *	vm_fault_wire:
 *
 *	Wire down a range of virtual addresses in a map.
 */
void vm_fault_wire(map, entry)
	vm_map_t	map;
	vm_map_entry_t	entry;
{

	register vm_offset_t	va;
	register pmap_t		pmap;
	register vm_offset_t	end_addr = entry->end;

	pmap = vm_map_pmap(map);

	/*
	 *	Inform the physical mapping system that the
	 *	range of addresses may not fault, so that
	 *	page tables and such can be locked down as well.
	 */

	pmap_pageable(pmap, entry->start, end_addr, FALSE);

	/*
	 *	We simulate a fault to get the page and enter it
	 *	in the physical map.
	 */

	for (va = entry->start; va < end_addr; va += PAGE_SIZE) {
		if (vm_fault_wire_fast(map, va, entry) != KERN_SUCCESS)
			(void) vm_fault(map, va, VM_PROT_NONE, TRUE);
	}
}


/*
 *	vm_fault_unwire:
 *
 *	Unwire a range of virtual addresses in a map.
 */
void vm_fault_unwire(map, entry)
	vm_map_t	map;
	vm_map_entry_t	entry;
{

	register vm_offset_t	va, pa;
	register pmap_t		pmap;
	register vm_offset_t	end_addr = entry->end;

	pmap = vm_map_pmap(map);

	/*
	 *	Since the pages are wired down, we must be able to
	 *	get their mappings from the physical map system.
	 */

	vm_page_lock_queues();

	for (va = entry->start; va < end_addr; va += PAGE_SIZE) {
		pa = pmap_extract(pmap, va);
		if (pa == (vm_offset_t) 0) {
			panic("unwire: page not in pmap");
		}
		pmap_change_wiring(pmap, va, FALSE);
		vm_page_unwire(PHYS_TO_VM_PAGE(pa));
	}
	vm_page_unlock_queues();

	/*
	 *	Inform the physical mapping system that the range
	 *	of addresses may fault, so that page tables and
	 *	such may be unwired themselves.
	 */

	pmap_pageable(pmap, entry->start, end_addr, TRUE);

}

/*
 *	Routine:
 *		vm_fault_copy_entry
 *	Function:
 *		Copy all of the pages from a wired-down map entry to another.
 *
 *	In/out conditions:
 *		The source and destination maps must be locked for write.
 *		The source map entry must be wired down (or be a sharing map
 *		entry corresponding to a main map entry that is wired down).
 */

void vm_fault_copy_entry(dst_map, src_map, dst_entry, src_entry)
	vm_map_t	dst_map;
	vm_map_t	src_map;
	vm_map_entry_t	dst_entry;
	vm_map_entry_t	src_entry;
{

	vm_object_t	dst_object;
	vm_object_t	src_object;
	vm_offset_t	dst_offset;
	vm_offset_t	src_offset;
	vm_prot_t	prot;
	vm_offset_t	vaddr;
	vm_page_t	dst_m;
	vm_page_t	src_m;

#ifdef	lint
	src_map++;
#endif	lint

	src_object = src_entry->object.vm_object;
	src_offset = src_entry->offset;

	/*
	 *	Create the top-level object for the destination entry.
	 *	(Doesn't actually shadow anything - we copy the pages
	 *	directly.)
	 */
	dst_object = vm_object_allocate(
			(vm_size_t) (dst_entry->end - dst_entry->start));

	dst_entry->object.vm_object = dst_object;
	dst_entry->offset = 0;

	prot  = dst_entry->max_protection;

	/*
	 *	Loop through all of the pages in the entry's range, copying
	 *	each one from the source object (it should be there) to the
	 *	destination object.
	 */
	for (vaddr = dst_entry->start, dst_offset = 0;
	     vaddr < dst_entry->end;
	     vaddr += PAGE_SIZE, dst_offset += PAGE_SIZE) {

		/*
		 *	Allocate a page in the destination object
		 */
		vm_object_lock(dst_object);
		do {
			dst_m = vm_page_alloc(dst_object, dst_offset);
			if (dst_m == VM_PAGE_NULL) {
				vm_object_unlock(dst_object);
				VM_WAIT;
				vm_object_lock(dst_object);
			}
		} while (dst_m == VM_PAGE_NULL);

		/*
		 *	Find the page in the source object, and copy it in.
		 *	(Because the source is wired down, the page will be
		 *	in memory.)
		 */
		vm_object_lock(src_object);
		src_m = vm_page_lookup(src_object, dst_offset + src_offset);
		if (src_m == VM_PAGE_NULL)
			panic("vm_fault_copy_wired: page missing");

		vm_page_copy(src_m, dst_m);

		/*
		 *	Enter it in the pmap...
		 */
		vm_object_unlock(src_object);
		vm_object_unlock(dst_object);

		pmap_enter(dst_map->pmap, vaddr, VM_PAGE_TO_PHYS(dst_m),
				prot, FALSE);

		/*
		 *	Mark it no longer busy, and put it on the active list.
		 */
		vm_object_lock(dst_object);
		vm_page_lock_queues();
		vm_page_activate(dst_m);
		vm_page_unlock_queues();
		PAGE_WAKEUP(dst_m);
		vm_object_unlock(dst_object);
	}

}


/*
 *	vm_fault_wire_fast:
 *
 *	Handle common case of a wire down page fault at the given address.
 *	If successful, the page is inserted into the associated physical map.
 *	The map entry is passed in to avoid the overhead of a map lookup.
 *
 *	NOTE: the given address should be truncated to the
 *	proper page address.
 *
 *	KERN_SUCCESS is returned if the page fault is handled; otherwise,
 *	a standard error specifying why the fault is fatal is returned.
 *
 *	The map in question must be referenced, and remains so.
 *	Caller has a read lock on the map.
 *
 *	This is a stripped version of vm_fault() for wiring pages.  Anything
 *	other than the common case will return KERN_FAILURE, and the caller
 *	is expected to call vm_fault().
 */
kern_return_t vm_fault_wire_fast(map, va, entry)
	vm_map_t	map;
	vm_offset_t	va;
	vm_map_entry_t	entry;
{
	vm_object_t		object;
	vm_offset_t		offset;
	register vm_page_t	m;
	vm_prot_t		prot;

	vm_stat.faults++;		/* needs lock XXX */
/*
 *	Recovery actions
 */

#undef	RELEASE_PAGE
#define	RELEASE_PAGE(m)	{				\
	PAGE_WAKEUP(m);					\
	vm_page_lock_queues();				\
	vm_page_unwire(m);				\
	vm_page_unlock_queues();			\
}


#undef	UNLOCK_THINGS
#define	UNLOCK_THINGS	{				\
	object->paging_in_progress--;			\
	vm_object_unlock(object);			\
}

#undef	UNLOCK_AND_DEALLOCATE
#define	UNLOCK_AND_DEALLOCATE	{			\
	UNLOCK_THINGS;					\
	vm_object_deallocate(object);			\
}
/*
 *	Give up and have caller do things the hard way.
 */

#define	GIVE_UP {					\
	UNLOCK_AND_DEALLOCATE;				\
	return(KERN_FAILURE);				\
}


	/*
	 *	If this entry is not directly to a vm_object, bail out.
	 */
	if ((entry->is_a_map) || (entry->is_sub_map))
		return(KERN_FAILURE);

	/*
	 *	Find the backing store object and offset into it.
	 */

	object = entry->object.vm_object;
	offset = va - entry->start + entry->offset;
	prot = entry->protection;

   	/*
	 *	Make a reference to this object to prevent its
	 *	disposal while we are messing with it.
	 */

	vm_object_lock(object);
	object->ref_count++;
	object->paging_in_progress++;

	/*
	 *	INVARIANTS (through entire routine):
	 *
	 *	1)	At all times, we must either have the object
	 *		lock or a busy page in some object to prevent
	 *		some other thread from trying to bring in
	 *		the same page.
	 *
	 *	2)	Once we have a busy page, we must remove it from
	 *		the pageout queues, so that the pageout daemon
	 *		will not grab it away.
	 *
	 */

	/*
	 *	Look for page in top-level object.  If it's not there or
	 *	there's something going on, give up.
	 */
	m = vm_page_lookup(object, offset);
	if ((m == VM_PAGE_NULL) || (m->busy) || (m->absent) ||
	    (prot & m->page_lock)) {
		GIVE_UP;
	}

	/*
	 *	Wire the page down now.  All bail outs beyond this
	 *	point must unwire the page.  
	 */

	vm_page_lock_queues();
	vm_page_wire(m);
	vm_page_unlock_queues();

	/*
	 *	Mark page busy for other threads.
	 */
	m->busy = TRUE;
	m->absent = FALSE;

	/*
	 *	Give up if the page is being written and there's a copy object
	 */
	if (object->copy != VM_OBJECT_NULL) {
		if ((prot & VM_PROT_WRITE) == 0) {
			m->copy_on_write = TRUE;
		}
		else {
			RELEASE_PAGE(m);
			GIVE_UP;
		}
	}

	/*
	 * (the various bits we're fiddling with here are locked by
	 * the object's lock)
	 */

	/* XXX This distorts the meaning of the copy_on_write bit */

	if (prot & VM_PROT_WRITE)
		m->copy_on_write = FALSE;

	/*
	 *	Put this page into the physical map.
	 *	We have to unlock the object because pmap_enter
	 *	may cause other faults.   
	 */
	vm_object_unlock(object);

	pmap_enter(map->pmap, va, VM_PAGE_TO_PHYS(m), prot , TRUE);

	/*
	 *	Must relock object so that paging_in_progress can be cleared.
	 */
	vm_object_lock(object);

	/*
	 *	Unlock everything, and return
	 */

	PAGE_WAKEUP(m);
	UNLOCK_AND_DEALLOCATE;

	return(KERN_SUCCESS);

}




