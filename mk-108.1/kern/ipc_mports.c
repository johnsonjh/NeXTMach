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
 * $Log:	ipc_mports.c,v $
 * Revision 2.4  89/02/25  18:02:51  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.3  88/09/25  22:11:36  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:15:10  rpd]
 * 
 * Revision 2.2  88/07/22  07:25:52  rpd
 * Created for mach_ports_* calls.
 * 
 */
/*
 * File:	ipc_mports.c
 * Purpose:
 *	Code for mach_ports_* calls.
 */

#import <sys/kern_return.h>
#import <sys/port.h>
#import <kern/task.h>
#import <kern/kern_port.h>
#import <vm/vm_param.h>
#import <kern/ipc_globals.h>
#import <kern/ipc_mports.h>

/*
 *	Routine:	mach_ports_register [exported, user]
 *	Purpose:
 *		Record the given list of ports in the target_task,
 *		to be passed down to its children.
 *
 *		Provides a means of getting valuable system-wide
 *		ports to future generations without using initial
 *		messages or other conventions that non-Mach applications
 *		might well ignore.
 *
 *		Children do not automatically acquire rights to these
 *		ports -- they must use mach_ports_lookup to pick them up.
 */
kern_return_t
mach_ports_register(target_task, init_port_set, init_port_set_Cnt)
	task_t target_task;
	port_array_t init_port_set;
	unsigned int init_port_set_Cnt;
{
	port_t list[TASK_PORT_REGISTER_MAX];
	int i;
	kern_return_t result = KERN_INVALID_ARGUMENT;

	/* Verify that there weren't too many passed to us. */

	if (init_port_set_Cnt <= TASK_PORT_REGISTER_MAX) {

		/* Copy them into a local list, to avoid faulting. */

		for (i = 0; i < init_port_set_Cnt; i++)
			list[i] = init_port_set[i];
		for (; i < TASK_PORT_REGISTER_MAX; i++)
			list[i] = PORT_NULL;

		/* Mark each port as registered by this task. */

		ipc_task_lock(target_task);
		if (target_task->ipc_active) {
			for (i = 0; i < TASK_PORT_REGISTER_MAX; i++) {
				if (target_task->ipc_ports_registered[i] !=
						PORT_NULL)
					port_release((kern_port_t) target_task->
						ipc_ports_registered[i]);
				if (list[i] != PORT_NULL)
					port_reference((kern_port_t) list[i]);
				target_task->ipc_ports_registered[i] =
					list[i];
			}
			result = KERN_SUCCESS;
		}
		ipc_task_unlock(target_task);
	}

	return result;
}

/*
 *	Routine:	mach_ports_lookup [exported, user]
 *	Purpose:
 *		Retrieve the ports for the target task that were
 *		established by mach_ports_register (or by inheritance
 *		of those registered in an ancestor, if no mach_ports_register
 *		has been done explicitly on this task).
 */
kern_return_t
mach_ports_lookup(target_task, init_port_set, init_port_set_Cnt)
	task_t target_task;
	port_array_t *init_port_set;
	unsigned int *init_port_set_Cnt;
{
	kern_return_t result;
	port_t list[TASK_PORT_REGISTER_MAX];
	vm_size_t list_size = sizeof(list);

	*init_port_set = (port_array_t) 0;
	*init_port_set_Cnt = 0;

	result = vm_allocate(ipc_kernel_map,
			     (vm_offset_t *) init_port_set, list_size,
			     TRUE);
	if (result == KERN_SUCCESS) {
		int i;

		/*
		 *	Copy the registered port list out right away
		 *	to avoid faulting while holding the lock.
		 */

		ipc_task_lock(target_task);
			for (i = 0; i < TASK_PORT_REGISTER_MAX; i++)
				if ((list[i] = target_task->ipc_active ?
					target_task->ipc_ports_registered[i] :
					PORT_NULL) != PORT_NULL)
						port_reference((kern_port_t) list[i]);
		ipc_task_unlock(target_task);

		/* Copy the ports into the array to return. */

		for (i = 0; i < TASK_PORT_REGISTER_MAX; i++)
			(*init_port_set)[i] = list[i];
		*init_port_set_Cnt = TASK_PORT_REGISTER_MAX;
	}

	/*
	 *	Note that this may return a barely-usable error code
	 *	if one of the vm_* calls fail.
	 */

	return result;
}


