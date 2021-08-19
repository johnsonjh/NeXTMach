/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_kern.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Kernel memory management.
 *
 * HISTORY
 * $Log:	vm_kern.c,v $
 * Revision 2.7  88/10/18  03:44:37  mwyoung
 * 	Give up in kmem_alloc_wait if the request is larger than the
 * 	(sub)map can ever accomodate.
 * 	[88/09/13            mwyoung]
 * 
 * Revision 2.6  88/10/11  10:26:37  rpd
 * 	Added special check for zero size to vm_move.
 * 	[88/10/07  14:26:34  rpd]
 * 
 * Revision 2.5  88/10/01  21:59:58  rpd
 * 	Changed FAST_PAGER_DATA to MACH_XP_FPD.
 * 	[88/09/29  01:11:09  rpd]
 * 
 * Revision 2.4  88/09/25  22:17:08  rpd
 * 	Changed vm_move; dst_map can no longer be VM_MAP_NULL.
 * 	[88/09/24  18:16:39  rpd]
 * 	
 * 	Changed vm_move to return an explicit error indication,
 * 	and pass back the destination address in an argument.
 * 	[88/09/20  16:14:13  rpd]
 * 
 * Revision 2.3  88/08/25  18:27:05  mwyoung
 * 	Include files for FAST_PAGER_DATA.
 * 	[88/08/16  00:36:39  mwyoung]
 * 
 * 06-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Call vm_page_wire for pages in kmem_mb_alloc.  (Keeps total
 *	page count sane).
 *
 * 16-Apr-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	FAST_PAGER_DATA: De-linted.
 *
 * 28-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	In kmem_alloc, preallocate pages for non-XP configurations as
 *	well, to prevent getting non-zero pages that had previously been
 *	paged out.
 *
 * 23-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Make v_to_p_zone non-pageable, as it is commonly used by the
 *	inode pager.
 *
 * 11-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Moved copy_user_to_physical here.  Eliminated old history.
 *
 * 30-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Changed kmem_free to use vm_offset_t for address.
 *
 */

#import <mach_xp_fpd.h>
#import <kern/assert.h>
#import <sys/kern_return.h>
#import <sys/types.h>

#import <vm/vm_kern.h>
#import <vm/vm_map.h>
#import <vm/vm_page.h>
#import <vm/vm_pageout.h>
#import <vm/vm_param.h>

#if	NeXT
#import <next/malloc_debug.h>
#endif	NeXT
/*
 *	kmem_alloc_pageable:
 *
 *	Allocate pageable memory to the kernel's address map.
 *	map must be "kernel_map" below.
 */

vm_offset_t kmem_alloc_pageable(map, size)
	vm_map_t		map;
	register vm_size_t	size;
{
	vm_offset_t		addr;
	register kern_return_t	result;

#if	0
	if (map != kernel_map)
		panic("kmem_alloc_pageable: not called with kernel_map");
#endif	0

	size = round_page(size);

	addr = vm_map_min(map);
	result = vm_map_find(map, VM_OBJECT_NULL, (vm_offset_t) 0,
				&addr, size, TRUE);
	if (result != KERN_SUCCESS) {
		return(0);
	}

	return(addr);
}

/*
 *	Allocate wired-down memory in the kernel's address map
 *	or a submap.
 */
vm_offset_t kmem_alloc(map, size)
	register vm_map_t	map;
	register vm_size_t	size;
{
	vm_offset_t		addr;
	register kern_return_t	result;
	register vm_offset_t	offset;
	extern vm_object_t	kernel_object;
	vm_offset_t		i;

	size = round_page(size);

	/*
	 *	Use the kernel object for wired-down kernel pages.
	 *	Assume that no region of the kernel object is
	 *	referenced more than once.
	 */

	addr = vm_map_min(map);
	result = vm_map_find(map, VM_OBJECT_NULL, (vm_offset_t) 0,
				 &addr, size, TRUE);
	if (result != KERN_SUCCESS) {
		return(0);
	}

	/*
	 *	Since we didn't know where the new region would
	 *	start, we couldn't supply the correct offset into
	 *	the kernel object.  Re-allocate that address
	 *	region with the correct offset.
	 */

	offset = addr - VM_MIN_KERNEL_ADDRESS;
	vm_object_reference(kernel_object);

	vm_map_lock(map);
	vm_map_delete(map, addr, addr + size);
	vm_map_insert(map, kernel_object, offset, addr, addr + size);
	vm_map_unlock(map);

	/*
	 *	Guarantee that there are pages already in this object
	 *	before calling vm_map_pageable.  This is to prevent the
	 *	following scenario:
	 *
	 *		1) Threads have swapped out, so that there is a
	 *		   pager for the kernel_object.
	 *		2) The kmsg zone is empty, and so we are kmem_allocing
	 *		   a new page for it.
	 *		3) vm_map_pageable calls vm_fault; there is no page,
	 *		   but there is a pager, so we call
	 *		   pager_data_request.  But the kmsg zone is empty,
	 *		   so we must kmem_alloc.
	 *		4) goto 1
	 *		5) Even if the kmsg zone is not empty: when we get
	 *		   the data back from the pager, it will be (very
	 *		   stale) non-zero data.  kmem_alloc is defined to
	 *		   return zero-filled memory.
	 *
	 *	We're intentionally not activating the pages we allocate
	 *	to prevent a race with page-out.  vm_map_pageable will wire
	 *	the pages.
	 */

	vm_object_lock(kernel_object);
	for (i = 0 ; i < size; i+= PAGE_SIZE) {
		vm_page_t	mem;

		while ((mem = vm_page_alloc(kernel_object, offset+i))
			    == VM_PAGE_NULL) {
			vm_object_unlock(kernel_object);
			VM_WAIT;
			vm_object_lock(kernel_object);
		}
		vm_page_zero_fill(mem);
		mem->busy = FALSE;
	}
	vm_object_unlock(kernel_object);
		
	/*
	 *	And finally, mark the data as non-pageable.
	 */

	(void) vm_map_pageable(map, (vm_offset_t) addr, addr + size, FALSE);

#if	defined(DEBUG) && NeXT
	malloc_debug ((void *)addr, getpc(), size, 
			MTYPE_KMEM_ALLOC, ALLOC_TYPE_ALLOC);
#endif	defined(DEBUG) && NeXT
	return(addr);
}

/*
 *	kmem_free:
 *
 *	Release a region of kernel virtual memory allocated
 *	with kmem_alloc, and return the physical pages
 *	associated with that region.
 */
void kmem_free(map, addr, size)
	vm_map_t		map;
	register vm_offset_t	addr;
	vm_size_t		size;
{
	(void) vm_map_remove(map, trunc_page(addr), round_page(addr + size));

#if	defined(DEBUG) && NeXT
	malloc_debug ((void *)addr, getpc(), size, 
			MTYPE_KMEM_ALLOC, ALLOC_TYPE_FREE);
#endif	defined(DEBUG) && NeXT
}

/*
 *	kmem_suballoc:
 *
 *	Allocates a map to manage a subrange
 *	of the kernel virtual address space.
 *
 *	Arguments are as follows:
 *
 *	parent		Map to take range from
 *	size		Size of range to find
 *	min, max	Returned endpoints of map
 *	pageable	Can the region be paged
 */
vm_map_t kmem_suballoc(parent, min, max, size, pageable)
	register vm_map_t	parent;
	vm_offset_t		*min, *max;
	register vm_size_t	size;
	boolean_t		pageable;
{
	register kern_return_t	ret;
	vm_map_t	result;

	size = round_page(size);

	*min = (vm_offset_t) vm_map_min(parent);
	ret = vm_map_find(parent, VM_OBJECT_NULL, (vm_offset_t) 0,
				min, size, TRUE);
	if (ret != KERN_SUCCESS) {
		printf("kmem_suballoc: bad status return of %d.\n", ret);
		panic("kmem_suballoc");
	}
	*max = *min + size;
	pmap_reference(vm_map_pmap(parent));
	result = vm_map_create(vm_map_pmap(parent), *min, *max, pageable);
	if (result == VM_MAP_NULL)
		panic("kmem_suballoc: cannot create submap");
	if ((ret = vm_map_submap(parent, *min, *max, result)) != KERN_SUCCESS)
		panic("kmem_suballoc: unable to change range to submap");
	return(result);
}

/*
 *	vm_move:
 *
 *	Move memory from source to destination map, possibly deallocating
 *	the source map reference to the memory.
 *
 *	Parameters are as follows:
 *
 *	src_map		Source address map
 *	src_addr	Address within source map
 *	dst_map		Destination address map
 *	num_bytes	Amount of data (in bytes) to copy/move
 *	src_dealloc	Should source be removed after copy?
 *
 *	Assumes the src and dst maps are not already locked.
 *
 *	If successful, returns destination address in dst_addr.
 */
kern_return_t vm_move(src_map,src_addr,dst_map,num_bytes,src_dealloc,dst_addr)
	vm_map_t		src_map;
	register vm_offset_t	src_addr;
	register vm_map_t	dst_map;
	vm_offset_t		num_bytes;
	boolean_t		src_dealloc;
	vm_offset_t		*dst_addr;
{
	register vm_offset_t	src_start;	/* Beginning of region */
	register vm_size_t	src_size;	/* Size of rounded region */
	vm_offset_t		dst_start;	/* destination address */
	register kern_return_t	result;

	assert(dst_map != VM_MAP_NULL);

	if (num_bytes == 0) {
		*dst_addr = 0;
		return KERN_SUCCESS;
	}

	/*
	 *	Page-align the source region
	 */

	src_start = trunc_page(src_addr);
	src_size = round_page(src_addr + num_bytes) - src_start;

	/*
	 *	Allocate a place to put the copy
	 */

	dst_start = (vm_offset_t) 0;
	result = vm_allocate(dst_map, &dst_start, src_size, TRUE);
	if (result == KERN_SUCCESS) {
		/*
		 *	Perform the copy, asking for deallocation if desired
		 */
		result = vm_map_copy(dst_map, src_map, dst_start, src_size,
				     src_start, FALSE, src_dealloc);

		/*
		 *	Return the destination address corresponding to
		 *	the source address given (rather than the front
		 *	of the newly-allocated page).
		 */

		if (result == KERN_SUCCESS)
			*dst_addr = dst_start + (src_addr - src_start);
	}

	return(result);
}

#if	MACH_XP
#if	MACH_XP_FPD
#import <kern/lock.h>
#import <kern/zalloc.h>

lock_data_t	v_to_p_lock_data;
lock_t		v_to_p_lock = &v_to_p_lock_data;
zone_t		v_to_p_zone;
boolean_t	v_to_p_initialized = FALSE;


struct 		v_to_p_entry {
		struct v_to_p_entry * 	next;
		vm_offset_t		v;
} * v_to_p_list;

#define	V_TO_P_NULL	((struct v_to_p_entry *)0)

/*
 *	Routine:	copy_user_to_physical_page
 *
 * 	Purpose:	
 *		Copy a virtual page which may be either kernel or
 *		user and may be currently paged out, to a physical
 *		page. 
 *	
 *	Algorithm:
 *		Allocate a kernel virtual address using kmem_alloc_pageable
 *		and pmap_enter and remove the target page m to that address.
 *		For speed, previous kmem_alloc_pageable results are kept
 *		around in a linked list to allow quick reuse.  
 *
 *	Notes:
 *		This routine probably should not be here but in another
 *		file.  Currently, thought, it is only used by the
 *		external pager.
 */
copy_user_to_physical_page(v, m, data_cnt) 
	vm_offset_t	v;	
	vm_page_t	m;
	int		data_cnt;
{
	struct v_to_p_entry * 	allocated_entry;
	vm_offset_t		allocated_addr;

	/*
	 * Lock access to virtual address list.
	 */
	lock_write(v_to_p_lock);
	if (!v_to_p_initialized) {
		/*
		 * Create a zone for v_to_p_entries.  Setup list.
		 */		
		v_to_p_zone = zinit(sizeof(struct v_to_p_entry),
				sizeof(struct v_to_p_entry), 
				sizeof(struct v_to_p_entry)*1024,
				FALSE, "v_to_p_zone");
		v_to_p_initialized = TRUE;
		v_to_p_list = V_TO_P_NULL;
	}

	while (v_to_p_list == V_TO_P_NULL) {
		/*
		 * Allocate a new entry from zone.
		 */
		v_to_p_list = (struct v_to_p_entry *)zalloc(v_to_p_zone);
		if (v_to_p_list == V_TO_P_NULL) {
			/*
			 * Give someone a chance to return entry to pool.
			 */
			lock_write_done(v_to_p_lock);
			thread_block();
			lock_write(v_to_p_lock);
			continue;
		}
		v_to_p_list->next = V_TO_P_NULL;
		v_to_p_list->v = kmem_alloc_pageable(kernel_map, PAGE_SIZE);
	}

	allocated_entry = v_to_p_list;
	v_to_p_list = v_to_p_list->next;
	lock_write_done(v_to_p_lock);

	allocated_addr = allocated_entry->v;
	pmap_enter(kernel_pmap, allocated_addr, VM_PAGE_TO_PHYS(m),
				VM_PROT_READ|VM_PROT_WRITE, TRUE);
	copyin((caddr_t) v, (caddr_t) allocated_addr, (unsigned int) data_cnt);
	pmap_remove(kernel_pmap, allocated_addr, allocated_addr+PAGE_SIZE);

	lock_write(v_to_p_lock);
	allocated_entry->next = v_to_p_list;
	v_to_p_list = allocated_entry;
	lock_done(v_to_p_lock);
}
#endif	MACH_XP_FPD
#endif	MACH_XP

/*
 *	Special hack for allocation in mb_map.  Can never wait for pages
 *	(or anything else) in mb_map.
 */
vm_offset_t kmem_mb_alloc(map, size)
	register vm_map_t	map;
	vm_size_t		size;
{
	vm_object_t		object;
	register vm_map_entry_t	entry;
	vm_offset_t		addr;
	register int		npgs;
	register vm_page_t	m;
	register vm_offset_t	vaddr, offset, cur_off;

	/*
	 *	Only do this on the mb_map.
	 */
	if (map != mb_map)
		panic("You fool!");

	size = round_page(size);

	vm_map_lock(map);
	entry = map->header.next;
	if (entry == &map->header) {
		/*
		 *	Map is empty.  Do things normally the first time...
		 *	this will allocate the entry and the object to use.
		 */
		vm_map_unlock(map);
		addr = vm_map_min(map);
		if (vm_map_find(map, VM_OBJECT_NULL, (vm_offset_t) 0,
			&addr, size, TRUE) != KERN_SUCCESS)
			return (0);
		(void) vm_map_pageable(map, addr, addr + size, FALSE);
		return(addr);
	}
	/*
	 *	Map already has an entry.  We must be extending it.
	 */
	if (!(entry == map->header.prev &&
	      entry->is_a_map == FALSE &&
	      entry->start == vm_map_min(map) &&
	      entry->max_protection == VM_PROT_DEFAULT &&
	      entry->protection == VM_PROT_DEFAULT &&
	      entry->inheritance == VM_INHERIT_DEFAULT &&
	      entry->wired_count != 0)) {
		/*
		 *	Someone's not playing by the rules...
		 */
		panic("mb_map abused even more than usual");
	}

	/*
	 *	Make sure there's enough room in map to extend entry.
	 */

	if (vm_map_max(map) - size < entry->end) {
		vm_map_unlock(map);
		return(0);
	}

	/*
	 *	extend the entry
	 */
	object = entry->object.vm_object;
	offset = (entry->end - entry->start) + entry->offset;
	addr   = entry->end;
	entry->end += size;

	/*
	 *	Since we may not have enough memory, and we may not
	 *	block, we first allocate all the memory up front, pulling
	 *	it off the active queue to prevent pageout.  We then can
	 *	either enter the pages, or free whatever we tried to get.
	 */

	vm_object_lock(object);
	cur_off = offset;
	npgs = atop(size);
	while (npgs) {
		m = vm_page_alloc(object, cur_off);
		if (m == VM_PAGE_NULL) {
			/*
			 *	Not enough pages, and we can't
			 *	wait, so free everything up.
			 */
			while (cur_off > offset) {
				cur_off -= PAGE_SIZE;
				m = vm_page_lookup(object, cur_off);
				/*
				 *	Don't have to lock the queues here
				 *	because we know that the pages are
				 *	not on any queues.
				 */
				vm_page_free(m);
			}
			vm_object_unlock(object);

			/*
			 *	Shrink the map entry back to its old size.
			 */
			entry->end -= size;
			vm_map_unlock(map);
			return(0);
		}

		/*
		 *	We want zero-filled memory
		 */

		vm_page_zero_fill(m);

		/*
		 *	Since no other process can see these pages, we don't
		 *	have to bother with the busy bit.
		 */

		m->busy = FALSE;

		npgs--;
		cur_off += PAGE_SIZE;
	}

	vm_object_unlock(object);

	/*
	 *	Map entry is already marked non-pageable.
	 *	Loop thru pages, entering them in the pmap.
	 *	(We can't add them to the wired count without
	 *	wrapping the vm_page_queue_lock in splimp...)
	 */
	vaddr = addr;
	cur_off = offset;
	while (vaddr < entry->end) {
		vm_object_lock(object);
		m = vm_page_lookup(object, cur_off);
		vm_page_wire(m);
		vm_object_unlock(object);
		pmap_enter(map->pmap, vaddr, VM_PAGE_TO_PHYS(m),
			entry->protection, TRUE);
		vaddr += PAGE_SIZE;
		cur_off += PAGE_SIZE;
	}
	vm_map_unlock(map);

	return(addr);
}

/*
 *	kmem_alloc_wait
 *
 *	Allocates pageable memory from a sub-map of the kernel.  If the submap
 *	has no room, the caller sleeps waiting for more memory in the submap.
 *
 */
vm_offset_t kmem_alloc_wait(map, size)
	vm_map_t	map;
	vm_size_t	size;
{
	vm_offset_t	addr;
	kern_return_t	result;

	size = round_page(size);

	do {
		/*
		 *	To make this work for more than one map,
		 *	use the map's lock to lock out sleepers/wakers.
		 *	Unfortunately, vm_map_find also grabs the map lock.
		 */
		vm_map_lock(map);
		lock_set_recursive(&map->lock);

		addr = vm_map_min(map);
		result = vm_map_find(map, VM_OBJECT_NULL, (vm_offset_t) 0,
				&addr, size, TRUE);

		lock_clear_recursive(&map->lock);
		if (result != KERN_SUCCESS) {

			if ( (vm_map_max(map) - vm_map_min(map)) < size ) {
				vm_map_unlock(map);
				return(0);
			}

			assert_wait((int)map, TRUE);
			vm_map_unlock(map);
			thread_block();
		}
		else {
			vm_map_unlock(map);
		}

	} while (result != KERN_SUCCESS);

	return(addr);
}

/*
 *	kmem_free_wakeup
 *
 *	Returns memory to a submap of the kernel, and wakes up any threads
 *	waiting for memory in that map.
 */
void	kmem_free_wakeup(map, addr, size)
	vm_map_t	map;
	vm_offset_t	addr;
	vm_size_t	size;
{
	vm_map_lock(map);
	(void) vm_map_delete(map, trunc_page(addr), round_page(addr + size));
	thread_wakeup((int)map);
	vm_map_unlock(map);
}

/*
 *	kmem_init:
 *
 *	Initialize the kernel's virtual memory map, taking
 *	into account all memory allocated up to this time.
 */
void kmem_init(start, end)
	vm_offset_t	start;
	vm_offset_t	end;
{
	vm_offset_t	addr;
	extern vm_map_t	kernel_map;

	kernel_map = vm_map_create(pmap_kernel(), VM_MIN_KERNEL_ADDRESS, end,
				FALSE);

	addr = VM_MIN_KERNEL_ADDRESS;
	(void) vm_map_find(kernel_map, VM_OBJECT_NULL, (vm_offset_t) 0,
				&addr, (start - VM_MIN_KERNEL_ADDRESS),
				FALSE);

#if	MACH_XP
#if	MACH_XP_FPD
	v_to_p_initialized = FALSE;
	lock_init(v_to_p_lock, TRUE);
#endif	MACH_XP_FPD
#endif	MACH_XP
}


