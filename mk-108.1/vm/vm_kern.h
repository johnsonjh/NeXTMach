/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_kern.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Kernel memory management definitions.
 *
 * HISTORY
 * $Log:	vm_kern.h,v $
 * Revision 2.3  88/09/25  22:17:22  rpd
 * 	Changes includes to the new style.
 * 	Updated vm_move declaration.
 * 	[88/09/20  16:15:48  rpd]
 * 
 * 17-Aug-87  David Golub (dbg) at Carnegie-Mellon University
 *	Added kmem_alloc_wait, kmem_free_wakeup, kernel_pageable_map.
 *
 * 15-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Coalesced includes, flushed MACH_ACC.
 *
 * 11-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Removed kmem_terminate and sh_map.  Note: definitions of
 *	mb_map, et. al. do not belong here.
 *
 * 14-Oct-85  Michael Wayne Young (mwyoung) at Carnegie-Mellon University
 *	Created header file.
 */

#import <sys/kern_return.h>
#import <sys/types.h>
#import <vm/vm_map.h>

void		kmem_init();
vm_offset_t	kmem_alloc();
vm_offset_t	kmem_alloc_pageable();
void		kmem_free();
vm_map_t	kmem_suballoc();

kern_return_t	vm_move();

vm_offset_t	kmem_alloc_wait();
void		kmem_free_wakeup();

vm_offset_t	kmem_mb_alloc();

vm_map_t	kernel_map;
vm_map_t	kernel_pageable_map;
vm_map_t	mb_map;
