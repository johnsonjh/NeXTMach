/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	task.c,v $
 * Revision 2.14  90/07/03  16:38:11  mrt
 * 	Return actual task priority in task_info.
 * 	[90/06/18            dlb]
 * 
 * 19-Apr-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: Added current_task_EXTERNAL() and current_map_EXTERNAL()
 *	procedure definitions.
 *
 * 22-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Make sure proc structure is initialized to zero so that we don't
 *	de-reference a bogus pointer for a non-unix task.
 *
 * 18-Feb-90  Gregg Kellogg (gk) at NeXT
 *	NeXT:	added in kernel_privilege for hardware access.
 *		Make kernel_task a real task.
 *		Use kernel_task instead of first_task.
 *	MACH_OLD_VM_COPY: use old vm_map_pageable semantics.
 *	Removed MACH_EMULATION conditional code.
 *
 * Revision 2.13  89/12/22  15:53:32  rpd
 * 	Redirect task assignment to default_pset if target pset is dead.
 * 	[89/12/15            dlb]
 * 
 * Revision 2.12  89/10/11  14:29:07  dlb
 * 	Init thread_list_lock in task_create, and use it in
 * 	       task_terminate.  This is to deal with a messy signal problem.
 * 	Initialize task priority in task_create().  Add task_priority.
 * 	task_halt() reassigns threads to default_pset.
 * 	Check for termination of current thread or task in task_terminate.
 * 	Use thread_force_terminate in task_terminate.
 * 	Add assign_active logic to avoid wakeups in task_assign.
 * 	New calling sequence for vm_map_pageable.
 * 	Convert to processor set logic.
 * 	Deleted obsolete xxx routines and user_suspend_count field.
 * 
 * Revision 2.11  89/06/27  00:24:48  rpd
 * 	Added support for TASK_KERNEL_PORT (using task_tself).
 * 	[89/06/26  23:55:57  rpd]
 * 
 * Revision 2.10  89/05/06  02:57:13  rpd
 * 	Purged <kern/task_statistics.h>.  Purged xxx_task_* routines.
 * 	[89/05/05  20:42:07  rpd]
 * 
 * Revision 2.9  89/02/25  18:09:19  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.8  89/01/15  16:27:25  rpd
 * 	Added all_tasks, all_tasks_lock definitions.
 * 	[89/01/15  15:07:32  rpd]
 * 
 * Revision 2.7  88/12/19  02:47:46  mwyoung
 * 	Remove lint.
 * 	[88/12/09            mwyoung]
 * 
 * Revision 2.6  88/10/11  10:21:38  rpd
 * 	Changed includes to the new style.
 * 	Rewrote task_threads; the old version could return
 * 	an inconsistent picture of the task.
 * 	[88/10/05  10:28:13  rpd]
 * 
 * Revision 2.5  88/08/06  18:25:53  rpd
 * Changed to use ipc_task_lock/ipc_task_unlock macros.
 * Eliminated use of kern/mach_ipc_defs.h.
 * Enable kernel_task for IPC access.  (See hack in task_by_unix_pid to
 * allow a user to get the kernel_task's port.)
 * Made kernel_task's ref_count > 0, so that task_reference/task_deallocate
 * works on it.  (Previously the task_deallocate would try to destroy it.)
 * 
 * Revision 2.4  88/07/20  16:40:17  rpd
 * Removed task_ports (replaced by port_names).
 * Didn't leave xxx form, because it wasn't implemented.
 * 
 * Revision 2.3  88/07/17  17:55:52  mwyoung
 * Split up uses of task.kernel_only field.  Condensed history.
 * 
 * Revision 2.2.1.1  88/06/28  20:46:20  mwyoung
 * Split up uses of task.kernel_only field.  Condensed history.
 * 
 * 21-Jun-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Split up uses of task.kernel_only field.  Condensed history.
 *
 * 07-Apr-88  John Seamons (jks) at NeXT
 *	removed TASK_CHUNK for NeXT
 *
 * 27-Jan-88  Douglas Orr (dorr) at Carnegie-Mellon University
 *	Init user space library structures.
 *
 * 21-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Task_create no longer returns the data port.  Task_status and
 *	task_set_notify are obsolete (use task_{get,set}_special_port).
 *
 * 17-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Added new task interfaces: task_suspend, task_resume,
 *	task_info, task_get_special_port, task_set_special_port.
 *	Old interfaces remain (temporarily) for binary
 *	compatibility, prefixed with 'xxx_'.
 *
 *  3-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Implemented better task termination based on task active field:
 *		1.  task_terminate sets active field to false.
 *		2.  All but the most simple task operations check the
 *			active field and abort if it is false.
 *		3.  task_{hold, dowait, release} now return kern_return_t's.
 *		4.  task_dowait has a second parameter to ignore active
 *			field if called from task_terminate.
 *	Task terminate acquires extra reference to current thread before
 *	terminating it (see thread_terminate()).
 *
 *  6-Aug-87  David Golub (dbg) at Carnegie-Mellon University
 *	Moved ipc_task_terminate to task_terminate, to shut down other
 *	threads that are manipulating the task via its task_port.
 *	Changed task_terminate to terminate all threads in the task.
 *
 * 29-Jul-87  David Golub (dbg) at Carnegie-Mellon University
 *	Fix task_suspend not to hold the task if the task has been
 *	resumed.  Change task_hold/task_wait so that if the current
 *	thread is in the task, it is not held until after all of the
 *	other threads in the task have stopped.  Make task_terminate be
 *	able to terminate the current task.
 */
/*
 *	File:	kern/task.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub,
 *		David Black
 *
 *	Task management primitives implementation.
 */

#import <mach_host.h>
#import <mach_old_vm_copy.h>

#import <kern/mach_param.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/zalloc.h>
#import <machine/vm_types.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>		/* for kernel_map */
#import <kern/ipc_tt.h>
#import <kern/ipc_globals.h>	/* for ipc_kernel_map */
#import <sys/task_info.h>
#import <sys/task_special_ports.h>
#import <kern/processor.h>
#import <kern/sched_prim.h>	/* for thread_wakeup */

#import <machine/machparam.h>	/* for splsched */

task_t	kernel_task;
zone_t	task_zone;

extern zone_t	u_zone;		/* UNIX */

void task_init()
{
	task_zone = zinit(
			sizeof(struct task),
			TASK_MAX * sizeof(struct task),
#if	NeXT
			0,
#else	NeXT
			TASK_CHUNK * sizeof(struct task),
#endif	NeXT
			FALSE, "tasks");

#if	NeXT
	(void) task_create(TASK_NULL, FALSE, &kernel_task);
	(void) vm_map_deallocate(kernel_task->map);
	kernel_task->kernel_privilege = TRUE;
	kernel_task->kernel_ipc_space = TRUE;
	kernel_task->kernel_vm_space = TRUE;
#else	NeXT
	kernel_task = (task_t) zalloc(task_zone);
	kernel_task->ref_count = 1;
	simple_lock_init(&kernel_task->lock);
#endif	NeXT
	kernel_task->map = kernel_map;
	ipc_task_init(kernel_task, TASK_NULL);
	ipc_task_enable(kernel_task);
}

kern_return_t task_create(parent_task, inherit_memory, child_task)
	task_t		parent_task;
	boolean_t	inherit_memory;
	task_t		*child_task;		/* OUT */
{
	register task_t	new_task;
	register processor_set_t	pset;

	new_task = (task_t) zalloc(task_zone);
	if (new_task == TASK_NULL) {
		panic("task_create: no memory for task structure");
	}

	new_task->u_address = (struct utask *) zalloc(u_zone);	/* UNIX */
#if	NeXT
	utask_zero(new_task);
#endif	NeXT
	new_task->ref_count = 1;

#if	NeXT
	if (parent_task == kernel_task)
		inherit_memory = FALSE;
#endif	NeXT

	if (inherit_memory)
		new_task->map = vm_map_fork(parent_task->map);
	else
		new_task->map = vm_map_create(pmap_create(0),
					round_page(VM_MIN_ADDRESS),
					trunc_page(VM_MAX_ADDRESS), TRUE);

	simple_lock_init(&new_task->lock);
	queue_init(&new_task->thread_list);
	simple_lock_init(&new_task->thread_list_lock);
	new_task->suspend_count = 0;
	new_task->active = TRUE;
	new_task->user_stop_count = 0;
	new_task->thread_count = 0;
	*child_task = new_task;

	new_task->kernel_ipc_space = FALSE;
	new_task->kernel_vm_space = FALSE;
	ipc_task_init(new_task, parent_task);

	new_task->total_user_time.seconds = 0;
	new_task->total_user_time.microseconds = 0;
	new_task->total_system_time.seconds = 0;
	new_task->total_system_time.microseconds = 0;

	if (parent_task != TASK_NULL) {
#if	NeXT
		new_task->kernel_privilege = parent_task->kernel_privilege;
#endif	NeXT
		task_lock(parent_task);
		pset = parent_task->processor_set;
		if (!pset->active)
			pset = &default_pset;
		new_task->priority = parent_task->priority;
	}
	else {
#if	NeXT
		new_task->kernel_privilege = FALSE;
#endif	NeXT
		pset = &default_pset;
		new_task->priority = BASEPRI_USER;
	}
	pset_lock(pset);
	pset_add_task(pset, new_task);
	pset_unlock(pset);
	if (parent_task != TASK_NULL)
		task_unlock(parent_task);

	new_task->may_assign = TRUE;
	new_task->assign_active = FALSE;

	ipc_task_enable(new_task);
	return(KERN_SUCCESS);
}

/*
 *	task_deallocate:
 *
 *	Give up a reference to the specified task and destroy it if there
 *	are no other references left.  It is assumed that the current thread
 *	is never in this task.
 */
void task_deallocate(task)
	register task_t	task;
{
	register int c;

	if (task == TASK_NULL)
		return;

	task_lock(task);
	c = --(task->ref_count);
	task_unlock(task);
	if (c != 0) {
		return;
	}
	else {
		register processor_set_t pset;

		pset = task->processor_set;
		pset_lock(pset);
		pset_remove_task(pset,task);
		pset_unlock(pset);
		pset_deallocate(pset);
		vm_map_deallocate(task->map);
#if	NeXT
		utask_free(task->u_address);
#else	NeXT
		zfree(u_zone, (vm_offset_t) task->u_address);
#endif	NeXT
		zfree(task_zone, (vm_offset_t) task);
	}
}

void task_reference(task)
	register task_t	task;
{
	if (task == TASK_NULL)
		return;

	task_lock(task);
	task->ref_count++;
	task_unlock(task);
}

/*
 *	task_terminate:
 *
 *	Terminate the specified task.  See comments on thread_terminate
 *	(kern/thread.c) about problems with terminating the "current task."
 */
kern_return_t task_terminate(task)
	register task_t	task;
{
	register thread_t	thread, cur_thread;
	register queue_head_t	*list;
	register task_t		cur_task;
	int			s;

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	list = &task->thread_list;
	cur_task = current_task();
	cur_thread = current_thread();

	/*
	 *	Deactivate task so that it can't be terminated again,
	 *	and so lengthy operations in progress will abort.
	 *
	 *	If the current thread is in this task, remove it from
	 *	the task's thread list to keep the thread-termination
	 *	loop simple.
	 */
	if (task == cur_task) {
		task_lock(task);
		if (!task->active) {
			/*
			 *	Task is already being terminated.
			 */
			task_unlock(task);
			return(KERN_FAILURE);
		}
		/*
		 *	Make sure current thread is not being terminated.
		 */
		s = splsched();
		simple_lock(&task->thread_list_lock);
		thread_lock(cur_thread);
		if (!cur_thread->active) {
			thread_unlock(cur_thread);
			simple_unlock(&task->thread_list_lock);
			(void) splx(s);
			task_unlock(task);
			thread_terminate(cur_thread);
			return(KERN_FAILURE);
		}
		task->active = FALSE;
		queue_remove(list, cur_thread, thread_t, thread_list);
		thread_unlock(cur_thread);
		simple_unlock(&task->thread_list_lock);
		(void) splx(s);
		task_unlock(task);

		/*
		 *	Shut down this thread's ipc now because it must
		 *	be left alone to terminate the task.
		 */
		ipc_thread_disable(cur_thread);
		ipc_thread_terminate(cur_thread);
	}
	else {
		/*
		 *	Lock both current and victim task to check for
		 *	potential deadlock.
		 */
		if ((int)task < (int)cur_task) {
			task_lock(task);
			task_lock(cur_task);
		}
		else {
			task_lock(cur_task);
			task_lock(task);
		}
		/*
		 *	Check if current thread or task is being terminated.
		 */
		s = splsched();
		thread_lock(cur_thread);
		if ((!cur_task->active) ||(!cur_thread->active)) {
			/*
			 * Current task or thread is being terminated.
			 */
			thread_unlock(cur_thread);
			(void) splx(s);
			task_unlock(task);
			task_unlock(cur_task);
			thread_terminate(cur_thread);
			return(KERN_FAILURE);
		}
		thread_unlock(cur_thread);
		(void) splx(s);
		task_unlock(cur_task);

		if (!task->active) {
			/*
			 *	Task is already being terminated.
			 */
			task_unlock(task);
			return(KERN_FAILURE);
		}
		task->active = FALSE;
		task_unlock(task);
	}

	/*
	 *	Prevent further execution of the task.  ipc_task_disable
	 *	prevents further task operations via the task port.
	 *	If this is the current task, the current thread will
	 *	be left running.
	 */
	ipc_task_disable(task);
	(void) task_hold(task);
	(void) task_dowait(task,TRUE);			/* may block */

	/*
	 *	Terminate each thread in the task.
	 *
	 *	The task_port is closed down, so no more thread_create
	 *	operations can be done.  Thread_force_terminate closes the
	 *	thread port for each thread; when that is done, the
	 *	thread will eventually disappear.  Thus the loop will
	 *	terminate.  Call thread_force_terminate instead of
	 *	thread_terminate to avoid deadlock checks.
	 */
	task_lock(task);
	while (!queue_empty(list)) {
		thread = (thread_t) queue_first(list);
		task_unlock(task);
		thread_force_terminate(thread);
		task_lock(task);
	}
	task_unlock(task);

	/*
	 *	Shut down IPC.
	 */
	ipc_task_terminate(task);

	/*
	 *	Deallocate the task's reference to itself.
	 */
	task_deallocate(task);

	/*
	 *	If the current thread is in this task, it has not yet
	 *	been terminated (since it was removed from the task's
	 *	thread-list).  Put it back in the thread list (for
	 *	completeness), and terminate it.  Since it holds the
	 *	last reference to the task, terminating it will deallocate
	 *	the task.
	 */
	if (cur_thread->task == task) {
		task_lock(task);
		s = splsched();
		simple_lock(&task->thread_list_lock);
		queue_enter(list, cur_thread, thread_t, thread_list);
		simple_unlock(&task->thread_list_lock);
		(void) splx(s);
		task_unlock(task);
		(void) thread_terminate(cur_thread);
	}

	return(KERN_SUCCESS);
}

/*
 *	task_hold:
 *
 *	Suspend execution of the specified task.
 *	This is a recursive-style suspension of the task, a count of
 *	suspends is maintained.
 */
kern_return_t task_hold(task)
	register task_t	task;
{
	register queue_head_t	*list;
	register thread_t	thread, cur_thread;

	cur_thread = current_thread();

	task_lock(task);
	if (!task->active) {
		task_unlock(task);
		return(KERN_FAILURE);
	}

	task->suspend_count++;

	/*
	 *	Iterate through all the threads and hold them.
	 *	Do not hold the current thread if it is within the
	 *	task.
	 */
	list = &task->thread_list;
	thread = (thread_t) queue_first(list);
	while (!queue_end(list, (queue_entry_t) thread)) {
		if (thread != cur_thread)
			thread_hold(thread);
		thread = (thread_t) queue_next(&thread->thread_list);
	}
	task_unlock(task);
	return(KERN_SUCCESS);
}

/*
 *	task_dowait:
 *
 *	Wait until the task has really been suspended (all of the threads
 *	are stopped).  Skip the current thread if it is within the task.
 *
 *	If task is deactivated while waiting, return a failure code unless
 *	must_wait is true.
 */
kern_return_t task_dowait(task, must_wait)
	register task_t	task;
	boolean_t must_wait;
{
	register queue_head_t	*list;
	register thread_t	thread, cur_thread, prev_thread;
	register kern_return_t	ret = KERN_SUCCESS;

	/*
	 *	Iterate through all the threads.
	 *	While waiting for each thread, we gain a reference to it
	 *	to prevent it from going away on us.  This guarantees
	 *	that the "next" thread in the list will be a valid thread.
	 *
	 *	We depend on the fact that if threads are created while
	 *	we are looping through the threads, they will be held
	 *	automatically.  We don't care about threads that get
	 *	deallocated along the way (the reference prevents it
	 *	from happening to the thread we are working with).
	 *
	 *	If the current thread is in the affected task, it is skipped.
	 *
	 *	If the task is deactivated before we're done, and we don't
	 *	have to wait for it (must_wait is FALSE), just bail out.
	 */
	cur_thread = current_thread();

	list = &task->thread_list;
	prev_thread = THREAD_NULL;
	task_lock(task);
	thread = (thread_t) queue_first(list);
	while (!queue_end(list, (queue_entry_t) thread)) {
		if (!(task->active) && !(must_wait)) {
			ret = KERN_FAILURE;
			break;
		}
		if (thread != cur_thread) {
			thread_reference(thread);
			task_unlock(task);
			if (prev_thread != THREAD_NULL)
				thread_deallocate(prev_thread);
							/* may block */
			(void) thread_dowait(thread, TRUE);  /* may block */
			prev_thread = thread;
			task_lock(task);
		}
		thread = (thread_t) queue_next(&thread->thread_list);
	}
	task_unlock(task);
	if (prev_thread != THREAD_NULL)
		thread_deallocate(prev_thread);		/* may block */
	return(ret);	
}

kern_return_t task_release(task)
	register task_t	task;
{
	register queue_head_t	*list;
	register thread_t	thread, next;

	task_lock(task);
	if (!task->active) {
		task_unlock(task);
		return(KERN_FAILURE);
	}

	task->suspend_count--;

	/*
	 *	Iterate through all the threads and release them
	 */
	list = &task->thread_list;
	thread = (thread_t) queue_first(list);
	while (!queue_end(list, (queue_entry_t) thread)) {
		next = (thread_t) queue_next(&thread->thread_list);
		thread_release(thread);
		thread = next;
	}
	task_unlock(task);
	return(KERN_SUCCESS);
}

/*
 *	task_halt:
 *
 *	Halt all threads in the task.  Do not halt the current thread if
 *	it is within the task.
 *
 *	Only called from exit().
 */
kern_return_t task_halt(task)
	register task_t	task;
{
	register queue_head_t	*list;
	register thread_t	thread, cur_thread, prev_thread;
	register kern_return_t	ret = KERN_SUCCESS;

	/*
	 *	Iterate through all the threads.
	 *	While waiting for each thread, we gain a reference to it
	 *	to prevent it from going away on us.  This guarantees
	 *	that the "next" thread in the list will be a valid thread.
	 *
	 *	If the current thread is in the affected task, it is skipped.
	 */
	cur_thread = current_thread();

	list = &task->thread_list;
	prev_thread = THREAD_NULL;
	task_lock(task);
	thread = (thread_t) queue_first(list);
	while (!queue_end(list, (queue_entry_t) thread)) {
		if (thread != cur_thread) {
			thread_reference(thread);
			task_unlock(task);
			if (prev_thread != THREAD_NULL)
			    thread_deallocate(prev_thread); /* may block */
#if	MACH_HOST
			thread_freeze(thread);
			if (thread->processor_set != &default_pset)
			    thread_doassign(thread, &default_pset, FALSE);
#endif	MACH_HOST
			thread_halt(thread, TRUE); /* may block */
#if	MACH_HOST
			thread_unfreeze(thread);
#endif	MACH_HOST
			prev_thread = thread;
			task_lock(task);
		}
		thread = (thread_t) queue_next(&thread->thread_list);
	}
	task_unlock(task);
	if (prev_thread != THREAD_NULL)
		thread_deallocate(prev_thread);		/* may block */
	return(ret);	
}

kern_return_t task_threads(task, thread_list, count)
	task_t		task;
	thread_array_t	*thread_list;
	unsigned int	*count;
{
	unsigned int actual;	/* this many threads */
	thread_t thread;
	port_t *threads;
	int i;

	vm_size_t size;
	vm_offset_t addr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	size = 0; addr = 0;

	for (;;) {
		vm_size_t size_needed;

		task_lock(task);
		if (!task->active) {
			task_unlock(task);
			return KERN_FAILURE;
		}

		actual = task->thread_count;

		/* do we have the memory we need? */

		size_needed = actual * sizeof(port_t);
		if (size_needed <= size)
			break;

		/* unlock the task and allocate more memory */
		task_unlock(task);

		if (size != 0)
			(void) kmem_free(ipc_kernel_map, addr, size);

		size = round_page(2 * size_needed);

		/* allocate memory non-pageable, so don't fault
		   while holding locks */

		(void) vm_allocate(ipc_kernel_map, &addr, size, TRUE);
#if	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map, addr, addr + size,
			FALSE);
#else	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map, addr, addr + size,
			VM_PROT_READ|VM_PROT_WRITE);
#endif	MACH_OLD_VM_COPY
	}

	/* OK, have memory and the task is locked & active */

	threads = (port_t *) addr;

	for (i = 0, thread = (thread_t) queue_first(&task->thread_list);
	     i < actual;
	     i++, thread = (thread_t) queue_next(&thread->thread_list))
		threads[i] = convert_thread_to_port(thread);
	assert(queue_end(&task->thread_list, (queue_entry_t) thread));

	/* can unlock task now that we've got the thread ports */
	task_unlock(task);

	if (actual == 0) {
		/* no threads, so return null pointer and deallocate memory */
		*thread_list = 0;
		*count = 0;

		if (size != 0)
			(void) kmem_free(ipc_kernel_map, addr, size);
	} else {
		vm_size_t size_used;

		*thread_list = threads;
		*count = actual;

		size_used = round_page(actual * sizeof(thread_t));

		/* finished touching it, so make the memory pageable */
#if	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map,
				       addr, addr + size_used, TRUE);
#else	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map,
				       addr, addr + size_used, VM_PROT_NONE);
#endif	MACH_OLD_VM_COPY

		/* free any unused memory */
		if (size_used != size)
			(void) kmem_free(ipc_kernel_map,
					 addr + size_used, size - size_used);
	}

	return KERN_SUCCESS;
}

kern_return_t task_suspend(task)
	register task_t	task;
{
	register boolean_t	hold;

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	hold = FALSE;
	task_lock(task);
	if ((task->user_stop_count)++ == 0)
		hold = TRUE;
	task_unlock(task);

	/*
	 *	If the stop count was positive, the task is
	 *	already stopped and we can exit.
	 */
	if (!hold) {
		return (KERN_SUCCESS);
	}

	/*
	 *	Hold all of the threads in the task, and wait for
	 *	them to stop.  If the current thread is within
	 *	this task, hold it separately so that all of the
	 *	other threads can stop first.
	 */

	if (task_hold(task) != KERN_SUCCESS)
		return(KERN_FAILURE);

	if (task_dowait(task, FALSE) != KERN_SUCCESS)
		return(KERN_FAILURE);

	if (current_task() == task) {
		thread_hold(current_thread());
		(void) thread_dowait(current_thread(), TRUE);
	}

	return(KERN_SUCCESS);
}

kern_return_t task_resume(task)
	register task_t	task;
{
	register boolean_t	release;

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	release = FALSE;
	task_lock(task);
	if (task->user_stop_count > 0) {
		if (--(task->user_stop_count) == 0)
	    		release = TRUE;
	}
	else {
		task_unlock(task);
		return(KERN_FAILURE);
	}
	task_unlock(task);

	/*
	 *	Release the task if necessary.
	 */
	if (release)
		return(task_release(task));

	return(KERN_SUCCESS);
}

kern_return_t task_info(task, flavor, task_info_out, task_info_count)
	task_t			task;
	int			flavor;
	task_info_t		task_info_out;	/* pointer to OUT array */
	unsigned int		*task_info_count;	/* IN/OUT */
{
	task_basic_info_t	basic_info;
	vm_map_t		map;
#if	NeXT
#else	NeXT
	extern task_t		first_task;	/* kernel task */
#endif	NeXT

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Only one flavor of information returned at the moment.
	 */
	if (flavor != TASK_BASIC_INFO ||
	    *task_info_count < TASK_BASIC_INFO_COUNT) {
		return(KERN_INVALID_ARGUMENT);
	}

	basic_info = (task_basic_info_t) task_info_out;

#if	NeXT
	map = task->map;
#else	NeXT
	map = (task == first_task) ? kernel_map : task->map;
#endif	NeXT

	basic_info->virtual_size  = map->size;
#if	NeXT
	if (map->pmap != PMAP_NULL)
		basic_info->resident_size = pmap_resident_count(map->pmap)
					   * PAGE_SIZE;
	else
		basic_info->resident_size = 0;

	basic_info->base_priority = task->priority;
#else	NeXT
	basic_info->resident_size = pmap_resident_count(map->pmap)
					   * PAGE_SIZE;
	basic_info->base_priority =
		(task == first_task) ? BASEPRI_SYSTEM : BASEPRI_USER;
				/* may change later XXX */
#endif	NeXT
	task_lock(task);
	basic_info->base_priority = task->priority;
	basic_info->suspend_count = task->user_stop_count;
	basic_info->user_time.seconds
				= task->total_user_time.seconds;
	basic_info->user_time.microseconds
				= task->total_user_time.microseconds;
	basic_info->system_time.seconds
				= task->total_system_time.seconds;
	basic_info->system_time.microseconds 
				= task->total_system_time.microseconds;
	task_unlock(task);

	*task_info_count = TASK_BASIC_INFO_COUNT;

	return(KERN_SUCCESS);
}

/*
 *	Special version of task_suspend that doesn't wait.
 *	Called only from interrupt level (U*X psignal).
 *	Will go away when signal code becomes sane.
 */
kern_return_t task_suspend_nowait(task)
	register task_t	task;
{
	register boolean_t	hold;

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	hold = FALSE;
	task_lock(task);
	if ((task->user_stop_count)++ == 0)
		hold = TRUE;
	task_unlock(task);

	/*
	 *	If the stop count was positive, the task is
	 *	already stopped and we can exit.
	 */
	if (!hold) {
		return (KERN_SUCCESS);
	}

	/*
	 *	Hold all of the threads in the task.
	 *	If the current thread is within
	 *	this task, hold it separately so that all of the
	 *	other threads can stop first.
	 */

	if (task_hold(task) != KERN_SUCCESS)
		return(KERN_FAILURE);

	if (current_task() == task) {
		thread_hold(current_thread());
	}

	return(KERN_SUCCESS);
}

kern_return_t task_get_special_port(task, which_port, port)
	task_t		task;
	int		which_port;
	port_t		*port;
{
	port_t		*portp;

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	switch (which_port) {
	    case TASK_KERNEL_PORT:
		portp = &task->task_tself;
		break;
	    case TASK_NOTIFY_PORT:
		portp = &task->task_notify;
		break;
	    case TASK_EXCEPTION_PORT:
		portp = &task->exception_port;
		break;
	    case TASK_BOOTSTRAP_PORT:
		portp = &task->bootstrap_port;
		break;
	    default:
		return(KERN_INVALID_ARGUMENT);
	}

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		return(KERN_FAILURE);
	}
	
	if ((*port = *portp) != PORT_NULL) {
		port_reference(*portp);
	}
	ipc_task_unlock(task);

	return(KERN_SUCCESS);
}

kern_return_t task_set_special_port(task, which_port, port)
	task_t		task;
	int		which_port;
	port_t		port;
{
	port_t		*portp;
	port_t		old_port;

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	switch (which_port) {
	    case TASK_KERNEL_PORT:
		portp = &task->task_tself;
		break;
	    case TASK_NOTIFY_PORT:
		portp = &task->task_notify;
		break;
	    case TASK_EXCEPTION_PORT:
		portp = &task->exception_port;
		break;
	    case TASK_BOOTSTRAP_PORT:
		portp = &task->bootstrap_port;
		break;
	    default:
		return(KERN_INVALID_ARGUMENT);
	}

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		return(KERN_FAILURE);
	}
	
	old_port = *portp;
	if ((*portp = port) != PORT_NULL)
		port_reference(port);

	ipc_task_unlock(task);

	if (old_port != PORT_NULL)
		port_release(old_port);

	return(KERN_SUCCESS);
}

#if	MACH_HOST
/*
 *	task_assign:
 *
 *	Change the assigned processor set for the task
 */
kern_return_t
task_assign(task, new_pset, assign_threads)
task_t	task;
processor_set_t	new_pset;
boolean_t	assign_threads;
{
	kern_return_t		ret = KERN_SUCCESS;
	register thread_t	thread, prev_thread;
	register queue_head_t	*list;
	register processor_set_t	pset;

	if (task == TASK_NULL || new_pset == PROCESSOR_SET_NULL) {
		return(KERN_INVALID_ARGUMENT);
	}

	task_lock(task);

	/*
	 *	If may_assign is false, task is already being assigned,
	 *	wait for that to finish.
	 */
	while (task->may_assign == FALSE) {
		task->assign_active = TRUE;
		assert_wait(&task->assign_active, TRUE);
		task_unlock(task);
		thread_block();
		task_lock(task);
	}

	/*
	 *	Do assignment.  If new pset is dead, redirect to default.
	 */
	pset = task->processor_set;
	pset_lock(pset);
	pset_remove_task(pset,task);
	pset_unlock(pset);
	pset_deallocate(pset);

	pset_lock(new_pset);
	if (!new_pset->active) {
	    pset_unlock(new_pset);
	    new_pset = &default_pset;
	    pset_lock(new_pset);
	}
	pset_add_task(new_pset,task);
	pset_unlock(new_pset);

	if (assign_threads == FALSE) {
		task_unlock(task);
		return(KERN_SUCCESS);
	}

	/*
	 *	Now freeze this assignment while we get the threads
	 *	to catch up to it.
	 */
	task->may_assign = FALSE;

	/*
	 *	If current thread is in task, freeze its assignment.
	 */
	if (current_thread()->task == task) {
		task_unlock(task);
		thread_freeze(current_thread());
		task_lock(task);
	}

	/*
	 *	Iterate down the thread list reassigning all the threads.
	 *	New threads pick up task's new processor set automatically.
	 *	Do current thread last because new pset may be empty.
	 */
	list = &task->thread_list;
	prev_thread = THREAD_NULL;
	thread = (thread_t) queue_first(list);
	while (!queue_end(list, (queue_entry_t) thread)) {
		if (!(task->active)) {
			ret = KERN_FAILURE;
			break;
		}
		if (thread != current_thread()) {
			thread_reference(thread);
			task_unlock(task);
			if (prev_thread != THREAD_NULL)
			    thread_deallocate(prev_thread); /* may block */
			thread_assign(thread,new_pset);	    /* may block */
			prev_thread = thread;
			task_lock(task);
		}
		thread = (thread_t) queue_next(&thread->thread_list);
	}

	/*
	 *	Done, wakeup anyone waiting for us.
	 */
	task->may_assign = TRUE;
	if (task->assign_active) {
		task->assign_active = FALSE;
		thread_wakeup((int)&task->may_assign);
	}
	task_unlock(task);
	if (prev_thread != THREAD_NULL)
		thread_deallocate(prev_thread);		/* may block */

	/*
	 *	Finish assignment of current thread.
	 */
	if (current_thread()->task == task)
		thread_doassign(current_thread(), new_pset, TRUE);

	return(ret);
}
#else	MACH_HOST
/*
 *	task_assign:
 *
 *	Change the assigned processor set for the task
 */
kern_return_t
task_assign(task, new_pset, assign_threads)
task_t	task;
processor_set_t	new_pset;
boolean_t	assign_threads;
{
#ifdef	lint
	task++; new_pset++; assign_threads++;
#endif	lint
	return(KERN_FAILURE);
}
#endif	MACH_HOST
	

/*
 *	task_assign_default:
 *
 *	Version of task_assign to assign to default processor set.
 */
kern_return_t
task_assign_default(task, assign_threads)
task_t		task;
boolean_t	assign_threads;
{
    return (task_assign(task, &default_pset, assign_threads));
}

/*
 *	task_get_assignment
 *
 *	Return name of processor set that task is assigned to.
 */
kern_return_t task_get_assignment(task, pset)
task_t	task;
processor_set_t	*pset;
{
	if (!task->active)
		return(KERN_FAILURE);

	*pset = task->processor_set;
	return(KERN_SUCCESS);
}

/*
 *	task_priority
 *
 *	Set priority of task; used only for newly created threads.
 *	Optionally change priorities of threads.
 */
kern_return_t
task_priority(task, priority, change_threads)
task_t		task;
int		priority;
boolean_t	change_threads;
{
	kern_return_t	ret = KERN_SUCCESS;

	if (task == TASK_NULL || invalid_pri(priority))
		return(KERN_INVALID_ARGUMENT);

	task_lock(task);
	task->priority = priority;

	if (change_threads) {
		register thread_t	thread;
		register queue_head_t	*list;

		list = &task->thread_list;
		thread = (thread_t) queue_first(list);
		while (!queue_end(list, (queue_entry_t) thread)) {

			if (thread_priority(thread, priority, FALSE)
				!= KERN_SUCCESS)
					ret = KERN_FAILURE;
			thread = (thread_t) queue_next(&thread->thread_list);
		}
	}

	task_unlock(task);
	return(ret);
}

#if	NeXT
task_t current_task_EXTERNAL()
{
	return current_task();
}

/*
 * Loadable servers need to be able to find their task map.
 */
vm_map_t current_map_EXTERNAL()
{
	return current_task()->map;
}
#endif	NeXT