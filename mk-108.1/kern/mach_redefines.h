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
 * $Log:	mach_redefines.h,v $
 * 14-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Don't define out support for task_create/task_terminate.
 *
 * Revision 2.5  89/03/09  20:13:56  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:06:11  gm0w
 * 	Kernel code cleanup.
 * 	Put entire file under #indef KERNEL.
 * 	[89/02/15            mrt]
 * 
 *  2-Jul-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Include "vm/vm_pager.h" to get types for
 *	vm_allocate_with_pager's dummy.
 *
 * 16-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	Undefine vm_allocate_with_pager if not running XP system.
 *
 * 21-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Created.  Temporary file to disable task_create and
 *	task_terminate until they can operate independently
 *	of BSD code.
 */

#ifndef	_KERN_MACH_REDEFINES_H_
#define _KERN_MACH_REDEFINES_H_

#if	!NeXT
#define task_create	task_create_not_implemented
/*ARGSUSED*/
kern_return_t task_create(parent_task, inherit_memory, child_task)
	task_t		parent_task;
	boolean_t	inherit_memory;
	task_t		*child_task;
{
	uprintf("task_create is not implemented yet\n");
	return(KERN_FAILURE);
}

#define task_terminate	task_terminate_not_implemented
/*ARGSUSED*/
kern_return_t task_terminate(task)
	task_t		task;
{
	uprintf("task_terminate is not implemented yet\n");
	return(KERN_FAILURE);
}
#endif	!NeXT

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_xp.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#if	!MACH_XP
#import <vm/memory_object.h>

#define vm_allocate_with_pager	vm_allocate_with_pager_not_implemented
/*ARGSUSED*/
kern_return_t vm_allocate_with_pager(map, addr, size, find_space, pager,
		pager_offset)
	vm_map_t		map;
	vm_offset_t		*addr;
	vm_size_t		size;
	boolean_t		find_space;
	memory_object_t		pager;
	vm_offset_t		pager_offset;
{
	uprintf("vm_allocate_with_pager is not implemented in this kernel\n");
	return(KERN_FAILURE);
}
#endif	!MACH_XP

#if	MACH_XP
#import <vm/vm_object.h>
#import <vm/memory_object.h>
#import <sys/boolean.h>

#define pager_cache	xxx_pager_cache
kern_return_t pager_cache(object, should_cache)
	vm_object_t	object;
	boolean_t	should_cache;
{
	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	return(memory_object_set_attributes(object, TRUE, should_cache, MEMORY_OBJECT_COPY_NONE));
}
#endif	MACH_XP

#endif	_KERN_MACH_REDEFINES_H_


