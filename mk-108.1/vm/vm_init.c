/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_init.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Initialize the Virtual Memory subsystem.
 *
 * HISTORY
 * 20-Mar-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: move kallocinit here from init_main.
 *
 * 22-Apr-88  David Black (dlb) at Carnegie-Mellon University
 *	Add call to vm_user_init for non-XP systems.
 *
 * 18-Nov-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Split up zone startup into bootstrap and later initialization.
 *
 *  8-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Call both vm_pager_init() and inode_pager_init().
 *	Rename vm_page_init() to vm_page_startup().
 *
 * 18-Mar-87  John Seamons (jks) at NeXT
 *	NeXT: added support for non-contiguous physical memory regions.
 *
 * 10-Mar-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH_XP: Change vm_pager_init to inode_pager_init.
 *
 * 18-Sep-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */

#import <mach_xp.h>

#import <machine/vm_types.h>
#import <kern/lock.h>
#import <vm/vm_object.h>
#import <vm/vm_map.h>
#import <vm/vm_page.h>
#import <vm/vm_kern.h>
#import <vm/vnode_pager.h>

/*
 *	vm_init initializes the virtual memory system.
 *	This is done only by the first cpu up.
 *
 *	The start and end address of physical memory is passed in.
 */

void vm_mem_init()
{
#if	NeXT
#else	NeXT
	extern vm_offset_t	avail_start, avail_end;
#endif	NeXT
	extern vm_offset_t	virtual_avail, virtual_end;

	/*
	 *	Initializes resident memory structures.
	 *	From here on, all physical memory is accounted for,
	 *	and we use only virtual addresses.
	 */

#if	NeXT
	virtual_avail = vm_page_startup(mem_region, num_regions, virtual_avail);
#else	NeXT
	virtual_avail = vm_page_startup(avail_start, avail_end, virtual_avail);
#endif	NeXT

	/*
	 *	Initialize other VM packages
	 */

	zone_bootstrap();
	vm_object_init();
	vm_map_init();
	kmem_init(virtual_avail, virtual_end);
#if	NeXT
	pmap_init(mem_region, num_regions);
#else	NeXT
	pmap_init(avail_start, avail_end);
#endif	NeXT
	zone_init();
	kallocinit();
	vm_pager_init();

#if	MACH_XP
#else	MACH_XP
	vm_user_init();
#endif	MACH_XP
}

