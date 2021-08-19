/* 
 * Mach Operating System
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/memory_object.c
 *	Author:	Michael Wayne Young
 *
 *	External memory management interface control functions.
 */

/*
 * HISTORY
 * $Log:	memory_object.c,v $
 * Revision 2.7  88/11/14  15:06:02  gm0w
 * 	Oct-31-88 Mary Thompson
 * 	Turned off memory_object_debug for alpha release
 * 	[88/11/02  15:12:15  mrt]
 * 
 * Revision 2.6  88/10/18  03:43:15  mwyoung
 * 	Correct locking in memory_object_data_provided.
 * 	Remove pager_data_provided_inline code.
 * 	[88/10/18  00:49:16  mwyoung]
 * 	
 * 	Activate any page that we fill in memory_object_data_provided().
 * 	[88/10/15            mwyoung]
 * 	
 * 	No need to hold the vm_object_cache_lock() when changing the
 * 	caching attribute in memory_object_set_attributes.  Check for
 * 	bogus arguments.  Allow the "ready" attribute to be turned off.
 * 	[88/09/22            mwyoung]
 * 	
 * 	Protect against removal of the control port when issuing the
 * 	memory_object_lock_completed call.  Wait until that call
 * 	completes to lose the object reference.
 * 	[88/09/22            mwyoung]
 * 	
 * 	Remove pager_cache.
 * 	[88/09/18            mwyoung]
 * 	
 * 	Watch for a few more unusual pagein conditions.
 * 	[88/09/17            mwyoung]
 * 	
 * 	Take notice of abnormal behavior on the part of the default memory
 * 	manager.  [These abnormalities aren't necessarily errors, but are
 * 	indicative of strange client behavior.]
 * 	[88/09/12  22:33:33  mwyoung]
 * 
 * Revision 2.5  88/10/01  21:59:19  rpd
 * 	Changed FAST_PAGER_DATA to MACH_XP_FPD.
 * 	[88/09/29  01:09:33  rpd]
 * 	
 * 	Updated vm_move calls.
 * 	[88/09/28  17:06:12  rpd]
 * 
 * Revision 2.4  88/08/25  18:25:57  mwyoung
 * 	Corrected include file references.
 * 	[88/08/22            mwyoung]
 * 	
 * 	Allow multiple pages to be provided in one call to
 * 	memory_object_data_provided.
 * 	[88/08/11  18:53:28  mwyoung]
 * 
 * Revision 2.3  88/08/06  19:24:56  rpd
 * Eliminated use of kern/mach_ipc_defs.h.
 * 
 * Revision 2.2  88/07/17  19:31:53  mwyoung
 * *** empty log message ***
 * 
 * Revision 2.2.1.1  88/07/20  18:07:55  mwyoung
 * Distinguish "temporary" objects from "internal" ones.
 * 
 * Revision 2.1.1.2  88/07/11  13:18:39  mwyoung
 * See below.
 * 
 */

#import <mach_xp_fpd.h>

/*
 *	Interface dependencies:
 */

#import <kern/mach_types.h>	/* For pointer_t */

#import <sys/kern_return.h>
#import <vm/vm_object.h>
#import <vm/memory_object.h>
#import <sys/boolean.h>
#import <vm/vm_prot.h>

/*
 *	Implementation dependencies:
 */
#import <vm/vm_page.h>
#import <vm/vm_pageout.h>
#import <vm/pmap.h>		/* For copy_to_phys, pmap_clear_modify */
#import <kern/thread.h>		/* For current_thread() */

#if	!MACH_XP_FPD
#import <vm/vm_kern.h>		/* For kernel_map, vm_move */
#import <vm/vm_map.h>		/* For vm_map_pageable */
#import <kern/ipc_globals.h>
#endif	!MACH_XP_FPD

#import <kern/xpr.h>

int		memory_object_debug = 0;

/*
 */

memory_object_t	memory_manager_default = PORT_NULL;

/*
 *	Important note:
 *		All of these routines gain a reference to the
 *		object (first argument) as part of the automatic
 *		argument conversion. Explicit deallocation is necessary.
 */

kern_return_t memory_object_data_provided(object, offset, data, data_cnt, lock_value)
	vm_object_t	object;
	vm_offset_t	offset;
	pointer_t	data;
	unsigned int	data_cnt;
	vm_prot_t	lock_value;
{
	kern_return_t	result = KERN_FAILURE;
	vm_page_t	m;
	pointer_t	original_data = data;
	unsigned int	original_data_cnt = data_cnt;
	pointer_t	kernel_data = data;

	XPR((1 << 30), ("memory_object_data_provided, object 0x%x, offset 0x%x",
				object, offset, 0, 0));

	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	for (;data_cnt >= PAGE_SIZE; data_cnt -= PAGE_SIZE, data += PAGE_SIZE) {
		/*
		 *	Move in user's data, and wire it down
		 */
#if	MACH_XP_FPD
		kernel_data = data;
#else	MACH_XP_FPD
		if (vm_move(ipc_soft_map, (vm_offset_t) data, ipc_kernel_map,
			    PAGE_SIZE, FALSE, &kernel_data) != KERN_SUCCESS)
			panic("memory_object_data_provided: cannot move data to kernel_map");

		vm_map_pageable(ipc_kernel_map, trunc_page((vm_offset_t) kernel_data), round_page(((vm_offset_t) kernel_data + PAGE_SIZE)), FALSE);
#endif	MACH_XP_FPD

		/*
		 *	Find the page waiting for our data.  If there's no
		 *	page, see whether we can allocate one, but don't bother
		 *	waiting.
		 */

		vm_object_lock(object);
		if ((memory_object_debug & 0x2) && (object->ref_count <= 1))
			printf("memory_object_data_provided: supplying data to a dead object");

		if ((m = vm_page_lookup(object, offset - object->paging_offset)) == VM_PAGE_NULL) {
			if (memory_object_debug & 0x4) {
				printf("memory_object_data_provided: object providing spurious data");
				printf("; object = 0x%x, offset = 0x%x\n", object, (offset - object->paging_offset));
			}
			if ((m = vm_page_alloc(object, offset - object->paging_offset)) != VM_PAGE_NULL) {
				m->busy = m->absent = TRUE;
			} else
				result = KERN_RESOURCE_SHORTAGE;
		}

		/*
		 *	Only overwrite pages that are "absent".
		 *	Maybe someday we'll allow memory managers
		 *	to overwrite live data,	but not yet.
		 */

		if ((m != VM_PAGE_NULL) && m->absent) {
			/*
			 *	Blast in the data
			 */
#if	MACH_XP_FPD
			if (vm_map_pmap(current_task()->map) != kernel_pmap)
				copy_user_to_physical_page(data, m, data_cnt);
			 else
#endif	MACH_XP_FPD
			copy_to_phys((vm_offset_t) kernel_data, VM_PAGE_TO_PHYS(m), data_cnt);
			pmap_clear_modify(VM_PAGE_TO_PHYS(m));
			m->busy = FALSE;
			m->absent = FALSE;
			m->page_lock = lock_value;
			m->unlock_request = VM_PROT_NONE;
			PAGE_WAKEUP(m);
			result = KERN_SUCCESS;

			vm_page_lock_queues();
			if (!m->active)
				vm_page_activate(m);
			vm_page_unlock_queues();
		}

		vm_object_unlock(object);
	
#if	MACH_XP_FPD
#else	MACH_XP_FPD
		vm_deallocate(ipc_kernel_map, kernel_data, PAGE_SIZE);
#endif	MACH_XP_FPD
	}

#if	MACH_XP_FPD
#else	MACH_XP_FPD
	if (original_data_cnt != 0)
		vm_deallocate(ipc_soft_map, original_data, original_data_cnt);
#endif	MACH_XP_FPD

	if (data_cnt != 0)
		uprintf("memory_object_data_provided: partial page discarded\n");

	vm_object_deallocate(object);
	return(result);
}

kern_return_t pager_data_provided_inline(object, offset, data, lock_value)
	vm_object_t	object;
	vm_offset_t	offset;
	vm_page_data_t	data;
	vm_prot_t	lock_value;
{
#ifdef	lint
	offset++; data[0]++; lock_value++;
#endif	lint

	uprintf("pager_data_provided_inline: no longer supported -- use");
	uprintf(" memory_object_data_provided instead... trust us\n");
	
	vm_object_deallocate(object);
	return(KERN_FAILURE);
}

kern_return_t memory_object_data_error(object, offset, size, error_value)
	vm_object_t	object;
	vm_offset_t	offset;
	vm_size_t	size;
	kern_return_t	error_value;
{
	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (size != round_page(size))
		return(KERN_INVALID_ARGUMENT);

#if	defined(hc) || defined(lint)
	/* Error value is ignored at this time */
	error_value++;
#endif	defined(hc) || defined(lint)

	vm_object_lock(object);
	while (size != 0) {
		register vm_page_t m;

		while (((m = vm_page_lookup(object, offset)) != VM_PAGE_NULL)) {
			if (m->absent) {
				m->busy = FALSE;
				m->absent = FALSE;
				m->error = TRUE;
				PAGE_WAKEUP(m);
			}

			if (!m->busy)
				break;

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

kern_return_t memory_object_data_unavailable(object, offset, size)
	vm_object_t	object;
	vm_offset_t	offset;
	vm_size_t	size;
{
	XPR((1 << 30), ("memory_object_data_unavailable, object 0x%x, offset 0x%x",
				object, offset, 0, 0));

	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (size != round_page(size))
		return(KERN_INVALID_ARGUMENT);

	if (!object->temporary) {
		uprintf("memory_object_data_unavailable: called on user-defined object -- converted to memory_object_data_error\n");
		return(memory_object_data_error(object, offset, size, KERN_SUCCESS));
	}

	vm_object_lock(object);
	while (size != 0) {
		register vm_page_t m;

		while (((m = vm_page_lookup(object, offset)) != VM_PAGE_NULL)) {
			if (m->absent) {
				m->busy = FALSE;
				PAGE_WAKEUP(m);
			}

			if (!m->busy)
				break;

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

kern_return_t	memory_object_lock_request(object, offset, size, should_clean, should_flush, prot, reply_to)
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
			if (m->busy && m->absent) {
				if (should_flush)
					VM_PAGE_FREE(m);
				break;
			}

			if (m->busy || (m->wire_count != 0)) {
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

			/*
			 *	XXX Optimize these sequences
			 */

			if ((should_clean || should_flush) && m->active)
				vm_page_deactivate(m);

			if (should_flush) {
				pmap_remove_all(VM_PAGE_TO_PHYS(m));
				m->clean &= !pmap_is_modified(VM_PAGE_TO_PHYS(m));
				pmap_update();
			}

			if (should_clean && !m->clean) {
				if (m->inactive) {
					queue_remove(&vm_page_queue_inactive, m, vm_page_t, pageq);
					m->inactive = FALSE;
					vm_page_inactive_count--;
				} else if (m->active)
					panic("memory_object_lock_request: page still active");

				vm_page_unlock_queues();
				if (!should_flush) {
					pmap_remove_all(VM_PAGE_TO_PHYS(m));
					pmap_update();
				}
				vm_pageout_page(m, FALSE);
				break;
			}

			if (should_flush) {
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

	if (reply_to != PORT_NULL) {
		/*
		 *	Prevent the control port from being destroyed
		 *	while we're making the completed call.
		 */

		vm_object_paging_begin(object);
		vm_object_unlock(object);

		(void) memory_object_lock_completed(reply_to, object->pager_request, original_offset, original_size);

		vm_object_lock(object);
		vm_object_paging_end(object);
	}

	vm_object_unlock(object);
	vm_object_deallocate(object);
	return(KERN_SUCCESS);
}

kern_return_t	memory_object_set_attributes(object, object_ready,
						may_cache, copy_strategy)
	vm_object_t	object;
	boolean_t	object_ready;
	boolean_t	may_cache;
	memory_object_copy_strategy_t copy_strategy;
{
	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Verify the attributes of importance
	 */

	switch(copy_strategy) {
		case MEMORY_OBJECT_COPY_NONE:
		case MEMORY_OBJECT_COPY_CALL:
		case MEMORY_OBJECT_COPY_DELAY:
			break;
		default:
			vm_object_deallocate(object);
			return(KERN_INVALID_ARGUMENT);
	}

	vm_object_lock(object);

	/*
	 *	Wake up anyone waiting for the ready attribute
	 *	to become asserted.
	 */

	if (object_ready && !object->pager_ready)
		thread_wakeup((int) &object->pager);

	/*
	 *	Copy the attributes
	 */

	object->can_persist = may_cache;
	object->pager_ready = object_ready;
	object->copy_strategy = copy_strategy;

	vm_object_unlock(object);

	vm_object_deallocate(object);

	return(KERN_SUCCESS);
}
