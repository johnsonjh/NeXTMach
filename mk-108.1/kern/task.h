/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	task.h,v $
 * 19-Apr-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: Added current_task_EXTERNAL() and current_map_EXTERNAL()
 *	procedure declarations.  If KERNEL_FEATURES is defined,
 *	current_task() is defined to be current_task_EXTERNAL() and
 *	current_map() is defined to be current_map_EXTERNAL().  Otherwise
 *	current_map() is a macro.  Note that current_task() is defined in
 *	thread.h
 *
 * 18-Feb-90  Gregg Kellogg (gk) at NeXT
 *	NeXT uses proc pointer, rather than index so that the table
 *	doesn't need to be contigious.
 *	NeXT: added in kernel_privilege for hardware access.
 *
 * Revision 2.14  89/10/11  14:29:55  dlb
 * 	XXX need separate lock for thread_list that can only be held at
 * 	XXX splsched() to solve nasty problem with signals.
 * 	[89/08/15            dlb]
 * 
 * 	Add task_array_t definition.
 * 	Add priority field.
 * 	Add task_assign(), task_assign_default() decls.
 * 	Add assign_active field.
 * 	Delete all_tasks list.
 * 	Added processor_set, may_assign fields.
 * 	Delete user_suspend_count field.
 * 
 * Revision 2.13  89/06/27  00:24:58  rpd
 * 	Added task_tself.
 * 	[89/06/26  23:56:18  rpd]
 * 
 * Revision 2.12  89/05/01  17:02:11  rpd
 * 	Moved reply_port to the thread structure.
 * 	Put the translation cache under MACH_IPC_TCACHE,
 * 	and made it bigger (8 lines for now, numbers needed).
 * 	[89/05/01  14:06:59  rpd]
 * 
 * Revision 2.11  89/03/09  20:16:16  rpd
 * 	More cleanup.
 * 
 * Revision 2.10  89/02/25  18:09:28  gm0w
 * 	Kernel code cleanup.
 * 	Made IPC_XXXHACK stuff unconditionally
 * 	defined, so that structure can be used outside the kernel.
 * 	[89/02/15            mrt]
 * 
 * Revision 2.9  89/02/07  01:04:46  mwyoung
 * Relocated from sys/task.h
 * 
 * Revision 2.8  89/01/15  16:35:02  rpd
 * 	Use decl_simple_lock_data.
 * 	Fixed all_tasks, all_tasks_lock to be extern.
 * 	[89/01/15  15:19:00  rpd]
 * 
 * Revision 2.7  89/01/10  23:32:49  rpd
 * 	Made the ipc_enabled field conditional on MACH_IPC_XXXHACK.
 * 	[89/01/10  23:11:38  rpd]
 * 
 * Revision 2.6  88/09/25  22:16:41  rpd
 * 	Changed port_cache fields/definitions to obj_cache.
 * 	[88/09/24  18:13:13  rpd]
 * 
 * Revision 2.5  88/08/24  02:46:30  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:24:13  mwyoung]
 * 
 * Revision 2.4  88/07/20  21:07:49  rpd
 * Added ipc_task_lock/ipc_task_unlock definitions.
 * Changes for port sets.
 * Add ipc_next_name field, used for assigning local port names.
 * 
 * Revision 2.3  88/07/17  18:56:33  mwyoung
 * Cleaned up.  Replaced task_t->kernel_only with
 * task_t->kernel_ipc_space, task_t->kernel_vm_space, and
 * task_t->ipc_privilege, to prevent overloading errors.
 * 
 * Remove current_task() declaration.
 * Eliminate paging_task.
 * 
 * Revision 2.2.1.2  88/06/26  00:45:49  rpd
 * Changes for port sets.
 * 
 * Revision 2.2.1.1  88/06/23  23:32:38  rpd
 * Add ipc_next_name field, used for assigning local port names.
 * 
 * 21-Jun-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Cleaned up.  Replaced task_t->kernel_only with
 *	task_t->kernel_ipc_space, task_t->kernel_vm_space, and
 *	task_t->ipc_privilege, to prevent overloading errors.
 *
 * 19-Apr-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Remove current_task() declaration.
 *	Eliminate paging_task.
 *
 * 18-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Removed task_data (now is per_thread).  Added
 *	task_bootstrap_port.  Added new routine declarations.
 *	Removed wake_active (unused).  Added fields to accumulate
 *	user and system time for terminated threads.
 *
 *  19-Feb-88 Douglas Orr (dorr) at Carnegie-Mellon University
 *	Change emulation bit mask into vector of routine  addrs
 *
 *  27-Jan-87 Douglas Orr (dorr) at Carnegie-Mellon University
 *	Add support for user space syscall emulation (bit mask
 *	of enabled user space syscalls and user space emulation
 *	routine).
 *
 *  3-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Change port cache account for per-task port names.
 *	Should move IPC stuff to a separate file :-).
 *	Add reply port for use by kernel-internal tasks.
 *
 *  2-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Added active field.
 *
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminate conditionals, flush history.
 */
/*
 *	File:	task.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	This file contains the structure definitions for tasks.
 *
 */

#ifndef	_KERN_TASK_H_
#define _KERN_TASK_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_ipc_xxxhack.h>
#import <mach_ipc_tcache.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/boolean.h>
#import <sys/port.h>
#import <sys/time_value.h>
#import <kern/lock.h>
#import <kern/queue.h>
#import <kern/mach_param.h>
#import <kern/kern_obj.h>
#import <kern/kern_set.h>
#import <kern/processor.h>
#import <vm/vm_map.h>

struct task {
	/* Synchronization/destruction information */
	decl_simple_lock_data(,lock)	/* Task's lock */
	int		ref_count;	/* Number of references to me */
	boolean_t	active;		/* Task has not been terminated */

	/* Miscellaneous */
	vm_map_t	map;		/* Address space description */
	queue_chain_t	pset_tasks;	/* list of tasks assigned to pset */
	int		suspend_count;	/* Internal scheduling only */

	/* Thread information */
	queue_head_t	thread_list;	/* list of threads */
	int		thread_count;	/* number of threads */
	decl_simple_lock_data(,thread_list_lock) /* XXX thread_list lock */
	processor_set_t	processor_set;	/* processor set for new threads */
	boolean_t	may_assign;	/* can assigned pset be changed? */
	boolean_t	assign_active;	/* waiting for may_assign */

	/* Garbage */
	struct utask	*u_address;
#if	NeXT
	struct proc	*proc;		/* corresponding process */
#else	NeXT
	int		proc_index;	/* corresponding process, by index */
#endif	NeXT

	/* User-visible scheduling information */
	int		user_stop_count;	/* outstanding stops */
	int		priority;		/* for new threads */

	/* Information for kernel-internal tasks */
#if	NeXT
	boolean_t	kernel_privilege; /* Is a kernel task */
#endif	NeXT
	boolean_t	kernel_ipc_space; /* Uses kernel's port names? */
	boolean_t	kernel_vm_space; /* Uses kernel's pmap? */

	/* Statistics */
	time_value_t	total_user_time;
				/* total user time for dead threads */
	time_value_t	total_system_time;
				/* total system time for dead threads */

	/* Special ports */
	port_t		task_self;	/* Port representing the task */
	port_t		task_tself;	/* What the task thinks is task_self */
	port_t		task_notify;	/* Where notifications get sent */
	port_t		exception_port;	/* Where exceptions are sent */
	port_t		bootstrap_port;	/* Port passed on for task startup */

	/* IPC structures */
	boolean_t	ipc_privilege;	/* Can use kernel resource pools? */
	decl_simple_lock_data(,ipc_translation_lock)
	queue_head_t	ipc_translations; /* Per-task port naming */
	boolean_t	ipc_active;	/* Can IPC rights be added? */
	port_name_t	ipc_next_name;	/* Next local name to use */
#if	MACH_IPC_XXXHACK
	kern_set_t	ipc_enabled;	/* Port set for PORT_ENABLED */
#endif	MACH_IPC_XXXHACK

#if	MACH_IPC_TCACHE
#define OBJ_CACHE_MAX		010	/* Number of cache lines */
#define OBJ_CACHE_MASK		007	/* Mask for name->line */

	struct {
		port_name_t	name;
		kern_obj_t	object;
	}		obj_cache[OBJ_CACHE_MAX];
					/* Fast object translation cache */
#endif	MACH_IPC_TCACHE

	/* IPC compatibility garbage */
	boolean_t	ipc_intr_msg;	/* Send signal upon message arrival? */
	port_t		ipc_ports_registered[TASK_PORT_REGISTER_MAX];
};

typedef struct task *task_t;

#define TASK_NULL	((task_t) 0)

typedef	port_t	*task_array_t;

#define task_lock(task)		simple_lock(&(task)->lock)
#define task_unlock(task)	simple_unlock(&(task)->lock)

#define ipc_task_lock(t)	simple_lock(&(t)->ipc_translation_lock)
#define ipc_task_unlock(t)	simple_unlock(&(t)->ipc_translation_lock)

/*
 *	Exported routines/macros
 */

extern kern_return_t	task_create();
extern kern_return_t	task_terminate();
extern kern_return_t	task_suspend();
extern kern_return_t	task_resume();
extern kern_return_t	task_threads();
extern kern_return_t	task_ports();
extern kern_return_t	task_info();
extern kern_return_t	task_get_special_port();
extern kern_return_t	task_set_special_port();
extern kern_return_t	task_assign();
extern kern_return_t	task_assign_default();

/*
 *	Internal only routines
 */

extern void		task_init();
extern void		task_reference();
extern void		task_deallocate();
extern kern_return_t	task_hold();
extern kern_return_t	task_dowait();
extern kern_return_t	task_release();
extern kern_return_t	task_halt();

extern kern_return_t	task_suspend_nowait();

extern task_t	kernel_task;

#if	NeXT
extern task_t		current_task_EXTERNAL();
/*
 * Loadable servers need to be able to find their task map.
 */
extern vm_map_t	current_map_EXTERNAL();

#if	defined(KERNEL) && defined(KERNEL_FEATURES)
/*
 * Loadable servers need a procedure to access the current task pointer.
 */
#define current_task()	current_task_EXTERNAL()
#define current_map()	current_map_EXTERNAL()
#else
#define current_map()	current_task()->map
#endif	defined(KERNEL) && defined(KERNEL_FEATURES)
#endif	NeXT

#endif	_KERN_TASK_H_


