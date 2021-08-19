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
 * $Log:	zalloc.c,v $
 * 21-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Removed the previous fix.  zget_space can return memory crossing
 *	a page boundary again.
 *
 * 24-May-90  Gregg Kellogg (gk) at NeXT
 *	Changed zget_space to always return memory contained on the same
 *	page.  This leads to greater fragmentation, but assures that any
 *	memory retrieved for DMA will be properly contained.
 *
 *  4-May-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: protect all simple_locks with splsched(), as zget can be called
 *	from an interrupt handler (via kernel_thread_noblock()).
 *
 * 20-Feb-90  Gregg Kellogg (gk) at NeXT
 *	port_names is conditional on MACH_OLD_VM_COPY in using copy_maps
 *	or old style stuff.
 *	Put back in zone alignment stuff.
 *
 * Revision 2.13  89/10/11  14:35:59  dlb
 * 	Changes suggested by boykin@multimax to avoid duplicate memory
 * 	allocation in zalloc and die a little more gracefully when the
 * 	zone_map runs out of memory.
 * 	[88/09/06            dlb]
 * 
 * Revision 2.12  89/10/10  10:55:16  mwyoung
 * 	Use new vm_map_copy technology.
 * 	[89/10/02            mwyoung]
 * 
 * Revision 2.11  89/05/30  10:38:40  rvb
 * 	Make zalloc storage pointers external, so they can be initialized from
 * 	the outside.
 * 	[89/05/30  08:28:14  rvb]
 * 
 * Revision 2.10  89/05/11  14:41:30  gm0w
 * 	Keep all zones on a list that host_zone_info can traverse.
 * 	This fixes a bug in host_zone_info: it would try to lock
 * 	uninitialized zones.  Fixed zinit to round elem_size up
 * 	to a multiple of four.  This prevents zget_space from handing
 * 	out improperly aligned objects.
 * 	[89/05/08  21:34:17  rpd]
 * 
 * Revision 2.9  89/05/06  15:47:11  rpd
 * 	From jsb: Added missing argument to kmem_free in zget_space.
 * 
 * Revision 2.8  89/05/06  02:57:35  rpd
 * 	Added host_zone_info (under MACH_DEBUG).
 * 	Fixed zget to increase cur_size when the space comes from zget_space.
 * 	Use MACRO_BEGIN/MACRO_END, decl_simple_lock_data where appropriate.
 * 	[89/05/06  02:43:29  rpd]
 * 
 * Revision 2.7  89/04/18  16:43:20  mwyoung
 * 	Document zget_space.  Eliminate MACH_XP conditional.
 * 	[89/03/26            mwyoung]
 * 	Make handling of zero allocation size unconditional.  Clean up
 * 	allocation code.
 * 	[89/03/16            mwyoung]
 * 
 * Revision 2.6  89/03/15  15:04:46  gm0w
 * 	Picked up code from rfr to allocate data from non pageable zones
 * 	from a single pool.
 * 	[89/03/09            mrt]
 * 
 * Revision 2.5  89/03/09  20:17:50  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:11:15  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.3  89/01/18  00:50:51  jsb
 * 	Vnode support: interpret allocation size of zero in zinit as meaning
 * 	PAGE_SIZE.
 * 	[89/01/17  20:57:39  jsb]
 * 
 * Revision 2.2  88/12/19  02:48:41  mwyoung
 * 	Fix include file references.
 * 	[88/12/19  00:33:03  mwyoung]
 * 	
 * 	Add and use zone_ignore_overflow.
 * 	[88/12/14            mwyoung]
 * 
 *  8-Jan-88  Rick Rashid (rfr) at Carnegie-Mellon University
 *	Made pageable zones really pageable.  Turned spin locks to sleep
 *	locks for pageable zones.
 *
 * 30-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 * 20-Oct-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Allocate zone memory from a separate kernel submap, to avoid
 *	sleeping with the kernel_map locked.
 *
 *  1-Oct-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added zchange().
 *
 * 30-Sep-87  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Deleted the printf() in zinit() which is called when zinit is
 *	creating a pageable zone.
 *
 * 12-Sep-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Modified to use list of elements instead of queues.  Actually,
 *	this package now uses macros defined in zalloc.h which define
 *	the list semantics.
 *
 * 30-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Update zone's cur_size field when it is crammed (zcram).
 *
 * 23-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Only define zfree if there is no macro version.
 *
 * 17-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	De-linted.
 *
 * 12-Feb-87  Robert Sansom (rds) at Carnegie Mellon University
 *	Added zget - no waiting version of zalloc.
 *
 * 22-Jan-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 12-Jan-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Eliminated use of the old interlocked queuing package;
 *	random other cleanups.
 *
 *  9-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */
/*
 *	File:	kern/zalloc.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Zone-based memory allocator.  A zone is a collection of fixed size
 *	data blocks for which quick allocation/deallocation is possible.
 */

#import <mach_debug.h>
#import <mach_old_vm_copy.h>

#import <vm/vm_param.h>
#import <kern/zalloc.h>
#import <kern/macro_help.h>
#import <vm/vm_kern.h>
#import <kern/assert.h>
#if	NeXT
#import <sys/printf.h>
#import <next/spl.h>
#import <next/malloc_debug.h>
#endif	NeXT

zone_t		zone_zone;	/* this is the zone containing other zones */
int zone_alignment = 3;

boolean_t	zone_ignore_overflow = FALSE;

extern vm_offset_t	zdata;
extern vm_size_t	zdata_size;

vm_map_t	zone_map = VM_MAP_NULL;

#if	NeXT
#define lock_zone(zone)					\
MACRO_BEGIN						\
	if (zone->pageable) { 				\
		lock_write(&zone->complex_lock);	\
	} else {					\
		s = splsched();	/* NeXT */		\
		simple_lock(&zone->lock);		\
	}						\
MACRO_END
#else	NeXT
#define lock_zone(zone)					\
MACRO_BEGIN						\
	if (zone->pageable) { 				\
		lock_write(&zone->complex_lock);	\
	} else {					\
		simple_lock(&zone->lock);		\
	}						\
MACRO_END
#endif	NeXT

#if	NeXT
#define unlock_zone(zone)				\
MACRO_BEGIN						\
	if (zone->pageable) { 				\
		lock_done(&zone->complex_lock);		\
	} else {					\
		simple_unlock(&zone->lock);		\
		splx(s);				\
	}						\
MACRO_END
#else	NeXT
#define unlock_zone(zone)				\
MACRO_BEGIN						\
	if (zone->pageable) { 				\
		lock_done(&zone->complex_lock);		\
	} else {					\
		simple_unlock(&zone->lock);		\
	}						\
MACRO_END
#endif	NeXT

#define lock_zone_init(zone)				\
MACRO_BEGIN						\
	if (zone->pageable) { 				\
		lock_init(&zone->complex_lock, TRUE);	\
	} else {					\
		simple_lock_init(&zone->lock);		\
	}						\
MACRO_END

/*
 *	Protects the static variables inside zget_space.
 */
decl_simple_lock_data(,	zget_space_lock)

/*
 *	Protects first_zone, last_zone, num_zones,
 *	and the next_zone field of zones.
 */
decl_simple_lock_data(,	all_zones_lock)
zone_t			first_zone;
zone_t			*last_zone;
int			num_zones;

/*
 *	zinit initializes a new zone.  The zone data structures themselves
 *	are stored in a zone, which is initially a static structure that
 *	is initialized by zone_init.
 */
zone_t zinit(size, max, alloc, pageable, name)
	vm_size_t	size;		/* the size of an element */
	vm_size_t	max;		/* maximum memory to use */
	vm_size_t	alloc;		/* allocation size */
	boolean_t	pageable;	/* is this zone pageable? */
	char		*name;		/* a name for the zone */
{
	register zone_t		z;

	if (zone_zone == ZONE_NULL)
		z = (zone_t) zdata;
	else if ((z = (zone_t) zalloc(zone_zone)) == ZONE_NULL)
		return(ZONE_NULL);

 	if (alloc == 0)
		alloc = PAGE_SIZE;

#if	NeXT
	/*
	 *	Round off all the parameters appropriately.
	 *	NOTE: max is not rounded off so that pre-allocated
	 *	zones (like kernel map entries) can be crammed and will
	 *	never try to expand.
	 */

	if (max < (alloc = round_page(alloc)))
		max = alloc;
#else	NeXT
	/*
	 *	Round off all the parameters appropriately.
	 */

	if ((max = round_page(max)) < (alloc = round_page(alloc)))
		max = alloc;
#endif	NeXT

	z->free_elements = 0;
	z->cur_size = 0;
	z->max_size = max;
	z->elem_size = (size + zone_alignment) & ~zone_alignment;
	z->alloc_size = alloc;
	z->pageable = pageable;
	z->zone_name = name;
	z->count = 0;
	z->doing_alloc = FALSE;
	z->exhaustible = z->sleepable = FALSE;
	lock_zone_init(z);

	/*
	 *	Add the zone to the all-zones list.
	 */

	z->next_zone = ZONE_NULL;
	simple_lock(&all_zones_lock);
	*last_zone = z;
	last_zone = &z->next_zone;
	num_zones++;
	simple_unlock(&all_zones_lock);

	return(z);
}

/*
 *	Cram the given memory into the specified zone.
 */
void zcram(zone, newmem, size)
	register zone_t		zone;
	vm_offset_t		newmem;
	vm_size_t		size;
{
	register vm_size_t	elem_size;
#if	NeXT
	int s;
#endif	NeXT

	assert(newmem != (vm_offset_t) 0);
	elem_size = zone->elem_size;

	lock_zone(zone);
	zone->cur_size += size;
	while (size >= elem_size) {
		ADD_TO_ZONE(zone, newmem);
		zone->count++;	/* compensate for ADD_TO_ZONE */
		size -= elem_size;
		newmem += elem_size;
	}
	unlock_zone(zone);
}

/*
 * Contiguous space allocator for non-paged zones. Allocates "size" amount
 * of memory from zone_map.
 */
vm_offset_t zalloc_next_space = 0;
vm_offset_t zalloc_end_of_space = 0;

static vm_offset_t zget_space(size)
	vm_offset_t size;
{
	vm_offset_t	new_space = 0;
	vm_offset_t	result;
	vm_size_t	space_to_add;
#if	NeXT
	int s = splsched();
#endif	NeXT

	simple_lock(&zget_space_lock);
	while ((zalloc_next_space + size) > zalloc_end_of_space) {
		/*
		 *	Add at least one page to allocation area.
		 */

		space_to_add = (PAGE_SIZE < size) ? 
						round_page(size) : PAGE_SIZE;

		if (new_space == 0) {
			/*
			 *	Memory cannot be wired down while holding
			 *	any locks that the pageout daemon might
			 *	need to free up pages.  [Making the zget_space
			 *	lock a complex lock does not help in this
			 *	regard.]
			 *
			 *	Unlock and allocate memory.  Because several
			 *	threads might try to do this at once, don't
			 *	use the memory before checking for available
			 *	space again.
			 */

			simple_unlock(&zget_space_lock);
#if	NeXT
			splx(s);
#endif	NeXT

			new_space = kmem_alloc(zone_map, space_to_add);
			if (new_space == 0)
				return(0);
#if	NeXT
			s = splsched();
#endif	NeXT
			simple_lock(&zget_space_lock);
			continue;
		}

		
		/*
	  	 *	Memory was allocated in a previous iteration.
		 *
		 *	Check whether the new region is contiguous
		 *	with the old one.
		 */

		if (new_space != zalloc_end_of_space) {
			/*
			 *	Throw away the remainder of the
			 *	old space, and start a new one.
			 */
			zalloc_next_space = new_space;
		}

		zalloc_end_of_space = new_space + space_to_add;

		new_space = 0;
	}
	result = zalloc_next_space;
	zalloc_next_space += size;		
	simple_unlock(&zget_space_lock);
#if	NeXT
	splx(s);
#endif	NeXT

	if (new_space != 0)
		kmem_free(zone_map, new_space, space_to_add);

	return(result);
}

/*
 *	Initialize the "zone of zones" which uses fixed memory allocated
 *	earlier in memory initialization.  zone_bootstrap is called
 *	before zone_init.
 */
void zone_bootstrap()
{
	simple_lock_init(&zget_space_lock);

	simple_lock_init(&all_zones_lock);
	first_zone = ZONE_NULL;
	last_zone = &first_zone;
	num_zones = 0;

	zone_zone = ZONE_NULL;
	zone_zone = zinit(sizeof(struct zone), sizeof(struct zone), 0,
					FALSE, "zones");

	zcram(zone_zone, (vm_offset_t)(zone_zone + 1),
	      zdata_size - sizeof *zone_zone);
}

void zone_init()
{
	vm_offset_t	zone_min;
	vm_offset_t	zone_max;

#if	NeXT
	zone_map = kmem_suballoc(kernel_map, &zone_min, &zone_max,
				 2*1024*1024 /*XXX*/, FALSE);
#else	NeXT
	zone_map = kmem_suballoc(kernel_map, &zone_min, &zone_max,
				 8*1024*1024 /*XXX*/, FALSE);
#endif	NEXT
}
	

/*
 *	zalloc returns an element from the specified zone.
 */
vm_offset_t zalloc(zone)
	register zone_t	zone;
{
	register vm_offset_t	addr;
#if	NeXT
	int s;
#endif	NeXT

	assert(zone != ZONE_NULL);

	lock_zone(zone);
	REMOVE_FROM_ZONE(zone, addr, vm_offset_t);
	while (addr == 0) {
		/*
 		 *	If nothing was there, try to get more
		 */
		if (zone->doing_alloc) {
			/*
			 *	Someone is allocating memory for this zone.
			 *	Wait for it to show up, then try again.
			 */
			assert_wait((int)&zone->doing_alloc, TRUE);
			/* XXX say wakeup needed */
			unlock_zone(zone);
			thread_block();
			lock_zone(zone);
		}
		else {
			if ((zone->cur_size + (zone->pageable ?
				zone->alloc_size : zone->elem_size)) >
			    zone->max_size) {
				if (zone->exhaustible)
					break;

				if (!zone_ignore_overflow) {
					printf("zone \"%s\" empty.\n",
						zone->zone_name);
					panic("zalloc");
				}
			}

			if (zone->pageable)
				zone->doing_alloc = TRUE;
			unlock_zone(zone);

			if (zone->pageable) {
				zcram(zone, kmem_alloc_pageable(zone_map, 
 						        zone->alloc_size), 
				      zone->alloc_size);
				lock_zone(zone);
				zone->doing_alloc = FALSE; 
				/* XXX check before doing this */
				thread_wakeup((int)&zone->doing_alloc);

				REMOVE_FROM_ZONE(zone, addr, vm_offset_t);
			} else {
				addr = zget_space(zone->elem_size);
				if (addr == 0)
					panic("zalloc");

				lock_zone(zone);
				zone->count++;
				zone->cur_size += zone->elem_size;
				unlock_zone(zone);
				return(addr);
			}
		}
	}

	unlock_zone(zone);
#if	defined(DEBUG) && NeXT
	malloc_debug ((void *)addr, getpc(), zone->elem_size,
		 MTYPE_ZALLOC, ALLOC_TYPE_ALLOC);
#endif	defined(DEBUG) && NeXT
	return(addr);
}

/*
 *	zget returns an element from the specified zone
 *	and immediately returns nothing if there is nothing there.
 *
 *	This form should be used when you can not block (like when
 *	processing an interrupt).
 */
vm_offset_t zget(zone)
	register zone_t	zone;
{
	register vm_offset_t	addr;
#if	NeXT
	int s;
#endif	NeXT

	assert(zone != ZONE_NULL);

	lock_zone(zone);
	REMOVE_FROM_ZONE(zone, addr, vm_offset_t);
	unlock_zone(zone);

#if	defined(DEBUG) && NeXT
	malloc_debug ((void *)addr, getpc(), zone->elem_size,
		 MTYPE_ZALLOC, ALLOC_TYPE_ALLOC);
#endif	defined(DEBUG) && NeXT
	return(addr);
}

void zfree(zone, elem)
	register zone_t	zone;
	vm_offset_t	elem;
{
#if	NeXT
	int s;
#endif	NeXT

	assert(elem);
	lock_zone(zone);
	ADD_TO_ZONE(zone, elem);
	unlock_zone(zone);

#if	defined(DEBUG) && NeXT
	malloc_debug ((void *)elem, getpc(), zone->elem_size,
		 MTYPE_ZALLOC, ALLOC_TYPE_FREE);
#endif	defined(DEBUG) && NeXT
}

void zchange(zone, pageable, sleepable, exhaustible)
	zone_t		zone;
	boolean_t	pageable;
	boolean_t	sleepable;
	boolean_t	exhaustible;
{
	zone->pageable = pageable;
	zone->sleepable = sleepable;
	zone->exhaustible = exhaustible;
}

#if	MACH_DEBUG
#import <sys/kern_return.h>
#import <machine/vm_types.h>
#import <kern/zone_info.h>
#import <kern/ipc_globals.h>
#import <vm/vm_user.h>
#import <vm/vm_map.h>

kern_return_t host_zone_info(task, names, namesCnt, info, infoCnt)
	task_t		task;
	zone_name_array_t *names;
	unsigned int	*namesCnt;
	zone_info_array_t *info;
	unsigned int	*infoCnt;
{
	int		max_zones;
	vm_offset_t	addr1, addr2;
	vm_size_t	size1, size2;
	zone_t		z;
	zone_name_t	*zn;
	zone_info_t	*zi;
	int		i;
	kern_return_t	kr;
#if	NeXT
	int		s;
#endif	NeXT

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	/*
	 *	We assume that zones aren't freed once allocated.
	 *	We won't pick up any zones that are allocated later.
	 */

#if	NeXT
	s = splsched();
#endif	NeXT

	simple_lock(&all_zones_lock);
	max_zones = num_zones;
	z = first_zone;
	simple_unlock(&all_zones_lock);
#if	NeXT
	splx(s);
#endif	NeXT

	assert(max_zones >= 0);
	*namesCnt = max_zones;
	*infoCnt = max_zones;

	if (max_zones == 0) {
		*names = 0;
		*info = 0;
		return KERN_SUCCESS;
	}

	size1 = round_page(max_zones * sizeof(zone_name_t));
	size2 = round_page(max_zones * sizeof(zone_info_t));

	/*
	 *	Allocate memory in the ipc_kernel_map, because
	 *	we need to touch it, and then move it to the ipc_soft_map
	 *	(where the IPC code expects to find it) when we're done.
	 *
	 *	Because we don't touch the memory with any locks held,
	 *	it can be left pageable.
	 */

	kr = vm_allocate(ipc_kernel_map, &addr1, size1, TRUE);
	if (kr != KERN_SUCCESS)
		panic("host_zone_info: vm_allocate");

	kr = vm_allocate(ipc_kernel_map, &addr2, size2, TRUE);
	if (kr != KERN_SUCCESS)
		panic("host_zone_info: vm_allocate");

	zn = (zone_name_t *) addr1;
	zi = (zone_info_t *) addr2;

	for (i = 0; i < max_zones; i++, zn++, zi++) {
		struct zone zcopy;

		assert(z != ZONE_NULL);

		lock_zone(z);
		zcopy = *z;
		unlock_zone(z);

#if	NeXT
		s = splsched();
#endif	NeXT
		simple_lock(&all_zones_lock);
		z = z->next_zone;
		simple_unlock(&all_zones_lock);
#if	NeXT
		splx(s);
#endif	NeXT

		/* assuming here the name data is static */
		(void) strncpy(zn->name, zcopy.zone_name, sizeof zn->name);

		zi->count = zcopy.count;
		zi->cur_size = zcopy.cur_size;
		zi->max_size = zcopy.max_size;
		zi->elem_size = zcopy.elem_size;
		zi->alloc_size = zcopy.alloc_size;
		zi->pageable = zcopy.pageable;
		zi->sleepable = zcopy.sleepable;
		zi->exhaustible = zcopy.exhaustible;
	}

	/*
	 *	Move memory to ipc_soft_map, and free unused memory.
	 */
#if	MACH_OLD_VM_COPY
	(void) vm_move(ipc_kernel_map, addr1,
			ipc_soft_map, size1, TRUE,
			(vm_offset_t *) names);
	(void) vm_move(ipc_kernel_map, addr2,
			ipc_soft_map, size2, TRUE,
			(vm_offset_t *) info);
#else	MACH_OLD_VM_COPY
	kr = vm_map_copyin(ipc_kernel_map, addr1,
		     size1, TRUE,
		     (vm_map_copy_t *) names);
	if (kr != KERN_SUCCESS)
		panic("host_zone_info: vm_map_copyin");

	kr = vm_map_copyin(ipc_kernel_map, addr2,
		     size2, TRUE,
		     (vm_map_copy_t *) info);
	if (kr != KERN_SUCCESS)
		panic("host_zone_info: vm_map_copyin");

#endif	MACH_OLD_VM_COPY
	return KERN_SUCCESS;
}
#endif	MACH_DEBUG









