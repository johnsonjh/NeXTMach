/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_pager.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Paging space routine stubs.  Emulates a matchmaker-like interface
 *	for builtin pagers.
 *
 * HISTORY
 * 14-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Updated to use the latest vm_pageout_page() calling sequence.
 *
 *  8-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Correct handling of tasks that use the kernel's address space
 *	when using the FAST_PAGER_DATA optimization.
 *	
 *	When flushing a range of pages for which a request is pending,
 *	throw away the request (so that it is made again).  [This isn't
 *	necessary for the kernel, but probably is easier on the memory
 *	manager.]
 *
 * 24-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	Handle IO errors on paging (non-XP only).
 *
 *  4-Feb-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Remove old routines for cleaning/flushing/locking.
 *	Avoid absent pages during pager_lock_request() calls.
 *
 * 11-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Moved copy_user_to_physical to vm/vm_kern.c.
 *
 * 11-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Make PAGE_ASSERT_WAIT's non-interruptible (?).
 *
 *  4-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Fixed to conform to current interface.
 *	
 *	Condensed old history.  Relevant contributions:
 *		Take data directly from user space in
 *		 pager_data_provided [rfr].
 *		Basic external paging interface
 *		 implementation [mwyoung, bolosky].
 *		Handle device mapping [dbg].
 *		Original work [avie, dbg].
 */

#import <mach_xp.h>

#import <sys/kern_return.h>
#import <vm/vm_pager.h>
#import <vm/vm_page.h>
#import <vm/vm_prot.h>
#import <sys/boolean.h>
#if	MACH_XP
#import <kern/task.h>
#import <vm/vm_map.h>
#import <vm/vm_pageout.h>
#else	MACH_XP
#import <vm/vnode_pager.h>
#endif	MACH_XP

#import <vm/pmap.h>

#if	MACH_XP
#import <vm/vm_object.h>
#import <vm/vm_kern.h>		/* To get kernel_map, vm_move */
#import <vm/vm_page.h>
#import <kern/mach_types.h>
#import <kern/ipc_globals.h>
#endif	MACH_XP

#if	MACH_XP
vm_pager_t	vm_pager_default = PORT_NULL;
#endif	MACH_XP

void vm_pager_init()
{
}

/*
 *	Important note:
 *		All of these routines gain a reference to the
 *		object (first argument) as part of the automatic
 *		argument conversion. Explicit deallocation is necessary.
 */

kern_return_t pager_cache(object, should_cache)
	vm_object_t	object;
	boolean_t	should_cache;
{
	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	vm_object_cache_lock();
	vm_object_lock(object);
	object->can_persist = should_cache;
	vm_object_unlock(object);
	vm_object_cache_unlock();

	vm_object_deallocate(object);

	return(KERN_SUCCESS);
}

#if	MACH_XP

kern_return_t pager_data_provided(object, offset, data, data_cnt, lock_value)
	vm_object_t	object;
	vm_offset_t	offset;
	pointer_t	data;
	unsigned int	data_cnt;
	vm_prot_t	lock_value;
{
	kern_return_t	result = KERN_FAILURE;
	vm_page_t	m;

	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (data_cnt != PAGE_SIZE) {
		printf("pager_data_provided: bad data size\n");
		vm_object_deallocate(object);
#if	FAST_PAGER_DATA
#else	FAST_PAGER_DATA
		vm_deallocate(ipc_soft_map, data, data_cnt);
#endif	FAST_PAGER_DATA
		return(KERN_INVALID_ARGUMENT);
	}

	/*
	 *	Move in user's data, and wire it down
	 */
#if	FAST_PAGER_DATA
#else	FAST_PAGER_DATA
	if ((data = vm_move(ipc_soft_map, (vm_offset_t) data, ipc_kernel_map, data_cnt, TRUE)) == 0)
		panic("pager_data_provided: cannot move data to kernel_map");

	vm_map_pageable(ipc_kernel_map, trunc_page((vm_offset_t) data), round_page(((vm_offset_t) data + data_cnt)), FALSE);
#endif	FAST_PAGER_DATA

	/*
	 *	Find the page waiting for our data.  If there's no
	 *	page, see whether we can allocate one, but don't bother
	 *	waiting.
	 */

	vm_object_lock(object);
	if ((m = vm_page_lookup(object, offset - object->paging_offset)) == VM_PAGE_NULL) {
		if ((m = vm_page_alloc(object, offset - object->paging_offset)) != VM_PAGE_NULL) {
			m->busy = m->absent = TRUE;
			vm_page_activate(m);	/* Holding object lock to prevent pageout! */
		} else
			result = KERN_RESOURCE_SHORTAGE;
	}

	/*
	 *	Only overwrite pages which are "absent".
	 *	Maybe someday we'll allow pagers to overwrite live data,
	 *	but not yet.
	 */

	if ((m != VM_PAGE_NULL) && m->absent) {
		/*
		 *	Blast in the data
		 */
#if	FAST_PAGER_DATA
		if (vm_map_pmap(current_task()->map) != kernel_pmap)
			copy_user_to_physical_page(data, m, data_cnt);
		 else
#endif	FAST_PAGER_DATA
		copy_to_phys((vm_offset_t) data, VM_PAGE_TO_PHYS(m), data_cnt);
		pmap_clear_modify(VM_PAGE_TO_PHYS(m));
		m->busy = FALSE;
		m->absent = FALSE;
		m->page_lock = lock_value;
		m->unlock_request = VM_PROT_NONE;
		PAGE_WAKEUP(m);
		result = KERN_SUCCESS;
	}

	vm_object_unlock(object);
	
#if	FAST_PAGER_DATA
#else	FAST_PAGER_DATA
	vm_deallocate(ipc_kernel_map, data, data_cnt);
#endif	FAST_PAGER_DATA

	vm_object_deallocate(object);
	return(result);
}

kern_return_t pager_data_provided_inline(object, offset, data, lock_value)
	vm_object_t	object;
	vm_offset_t	offset;
	vm_page_data_t	data;
	vm_prot_t	lock_value;
{
	kern_return_t	result = KERN_FAILURE;
	vm_page_t	m;

	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (PAGE_SIZE != 4096) {
		printf("pager_data_provided_inline: wrong page size\n");
		vm_object_deallocate(object);
		return(KERN_INVALID_ARGUMENT);
	}

	/*
	 *	Find the page waiting for our data.  If there's no
	 *	page, see whether we can allocate one, but don't bother
	 *	waiting.
	 */

	vm_object_lock(object);
	if ((m = vm_page_lookup(object, offset - object->paging_offset)) == VM_PAGE_NULL) {
		if ((m = vm_page_alloc(object, offset - object->paging_offset)) != VM_PAGE_NULL) {
			m->busy = m->absent = TRUE;
			vm_page_activate(m);	/* Holding object lock to prevent pageout! */
		} else
			result = KERN_RESOURCE_SHORTAGE;
	}

	/*
	 *	Only overwrite pages which are "absent".
	 *	Maybe someday we'll allow pagers to overwrite live data,
	 *	but not yet.
	 */

	if ((m != VM_PAGE_NULL) && m->absent) {
		/*
		 *	Blast in the data
		 */

		copy_to_phys(data, VM_PAGE_TO_PHYS(m), PAGE_SIZE);
		pmap_clear_modify(VM_PAGE_TO_PHYS(m));
		m->busy = FALSE;
		m->absent = FALSE;
		m->page_lock = lock_value;
		PAGE_WAKEUP(m);
		result = KERN_SUCCESS;
	}

	vm_object_unlock(object);
	
	vm_object_deallocate(object);
	return(result);
}

kern_return_t pager_data_unavailable(object, offset, size)
	vm_object_t	object;
	vm_offset_t	offset;
	vm_size_t	size;
{
	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (size != round_page(size))
		return(KERN_INVALID_ARGUMENT);

	vm_object_lock(object);
	while (size != 0) {
		register vm_page_t m;

		while (((m = vm_page_lookup(object, offset)) != VM_PAGE_NULL)) {
			if (m->absent) {
				m->busy = FALSE;
				PAGE_WAKEUP(m);
				break;
			}

			PAGE_ASSERT_WAIT(m, FALSE);
			vm_object_unlock(object);
			thread_block();
			vm_object_lock(object);
		}
		size -= PAGE_SIZE;
		offset += PAGE_SIZE;
	 }
	vm_object_unlock(object);

	vm_object_deallocate(object);
	return(KERN_SUCCESS);
}

kern_return_t	pager_lock_request(object, offset, size, should_clean, should_flush, prot, reply_to)
	register
	vm_object_t	object;
	register
	vm_offset_t	offset;
	register
	vm_size_t	size;
	boolean_t	should_clean;
	boolean_t	should_flush;
	vm_prot_t	prot;
	port_t		reply_to;
{
	vm_offset_t	original_offset = offset;
	vm_size_t	original_size = size;

	if (object == VM_OBJECT_NULL)
		return(KERN_FAILURE);

	size = round_page(size);

	vm_object_lock(object);

	if (atop(size) > object->resident_page_count) {
		/* XXX
		 *	Should search differently!
		 *	Must be careful to preserve ordering appearance.
		 */;
	}
		
	for (; size != 0; size -= PAGE_SIZE, offset += PAGE_SIZE) {
		register vm_page_t m;

		while ((m = vm_page_lookup(object, offset)) != VM_PAGE_NULL) {
			if (m->busy) {
				if (m->absent) {
					if (should_flush)
						VM_PAGE_FREE(m);
					break;
				}

				PAGE_ASSERT_WAIT(m, FALSE);
				vm_object_unlock(object);
				thread_block();
				vm_object_lock(object);
				continue;
			}

			/*
			 *	Check for cleaning and flushing before worrying
			 *	about locking
			 */

			vm_page_lock_queues();
			if (m->wire_count != 0) {
				/* XXX Must do something about this */
			}

			if ((should_clean || should_flush) && m->active)
				vm_page_deactivate(m);

			if (should_clean && !m->clean) {
				if (m->inactive) {
					queue_remove(&vm_page_queue_inactive, m, vm_page_t, pageq);
					m->inactive = FALSE;
					vm_page_inactive_count--;
				} else if (m->active)
					panic("pager_lock_request: page still active");

				vm_page_unlock_queues();
				pmap_remove_all(VM_PAGE_TO_PHYS(m));
				pmap_update();
				vm_pageout_page(m, FALSE);
				break;
			}

			if (should_flush) {
				pmap_remove_all(VM_PAGE_TO_PHYS(m));
				pmap_update();
				vm_page_free(m);
				vm_page_unlock_queues();
				break;
			}
			vm_page_unlock_queues();

			/*
			 *	If we are decreasing permission, do it now;
			 *	else, let the fault handler take care of it.
			 */

			if ((m->page_lock ^ prot) & prot)
				pmap_page_protect(VM_PAGE_TO_PHYS(m), prot ^ VM_PROT_ALL);
			else
				PAGE_WAKEUP(m);

			m->page_lock = prot;
			m->unlock_request = VM_PROT_NONE;
			break;
		}
	}
	vm_object_unlock(object);

	vm_object_deallocate(object);
	if (reply_to != PORT_NULL)
		(void) pager_lock_completed(reply_to, object->pager_request, original_offset, original_size);

	return(KERN_SUCCESS);
}

#else	MACH_XP

extern
boolean_t	vm_page_zero_fill();

pager_return_t vm_pager_get(pager, m)
	vm_pager_t	pager;
	vm_page_t	m;
{
	if (pager == vm_pager_null) {
		(void) vm_page_zero_fill(m);
		return(PAGER_SUCCESS);
	}
	if (pager->is_device)
		return(device_pagein(m));
	return(vnode_pagein(m));
}

pager_return_t vm_pager_put(pager, m)
	vm_pager_t	pager;
	vm_page_t	m;
{
	if (pager == vm_pager_null)
		panic("vm_pager_put: null pager");
	if (pager->is_device)
		return(device_pageout(m));
	return(vnode_pageout(m));
}

boolean_t vm_pager_deallocate(pager)
	vm_pager_t	pager;
{
	if (pager == vm_pager_null)
		panic("vm_pager_deallocate: null pager");
	if (pager->is_device)
		return(device_dealloc(pager));
	return(vnode_dealloc(pager));
}

vm_pager_t vm_pager_allocate(size)
	vm_size_t	size;
{
	return((vm_pager_t)vnode_alloc(size));
}

boolean_t vm_pager_has_page(pager, offset)
	vm_pager_t	pager;
	vm_offset_t	offset;
{
	if ((pager == vm_pager_null) || (pager->is_device))
		panic("vm_pager_has_page");
	return(vnode_has_page(pager,offset));
}

#define	DUMMY(x)	kern_return_t x() { printf("x: not implemented\n"); return(KERN_FAILURE); }
DUMMY(pager_data_provided)
DUMMY(pager_data_provided_inline)
DUMMY(pager_data_unavailable)
DUMMY(pager_data_lock)
DUMMY(pager_clean_request)
DUMMY(pager_flush_request)
DUMMY(pager_lock_request)

DUMMY(pager_data_unlock)
#undef	DUMMY

#endif	MACH_XP

