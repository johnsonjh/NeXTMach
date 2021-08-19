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
 * $Log:	ipc_port.c,v $
 * 20-Feb-90  Gregg Kellogg (gk) at NeXT
 *	MACH_OLD_VM_COPY:	Port_names and port_set_status use ipc_soft_map
 *				instead of copy_maps.
 *				Vm_map_pageable's last arg is boolean, not
 *				access type.
 *
 * Revision 2.13  89/10/11  14:08:16  dlb
 * 	New calling sequence for vm_map_pageable.
 * 	[89/01/25            dlb]
 * 
 * Revision 2.12  89/10/10  10:54:11  mwyoung
 * 	Use new vm_map_copy technology.
 * 	[89/06/26  19:10:03  mwyoung]
 * 
 * Revision 2.11  89/05/01  17:00:05  rpd
 * 	Unimplemented xxx_port_select.  Removed port_set_select.
 * 	[89/05/01  14:36:38  rpd]
 * 
 * Revision 2.10  89/03/05  16:47:34  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK.
 * 	[89/02/16  14:00:19  rpd]
 * 
 * Revision 2.9  89/02/25  18:03:34  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.8  89/01/10  23:29:26  rpd
 * 	Added MACH_IPC_XXXHACK conditionals around the obsolete
 * 	xxx_port_* calls.  Also under MACH_IPC_XXXHACK, changed
 * 	PORT_RESERVED() to not reserve PORT_ENABLED.
 * 	[89/01/10  23:01:17  rpd]
 * 	
 * 	Fixed use of ipc_enabled; the locking wasn't right.
 * 	Implemented xxx_port_select by moving the key select code
 * 	into port_set_select_internal.  Fixed slight problem in
 * 	set status/select code that could potentially leak set structures.
 * 	Removed unused port_enable/port_disable/port_interpose code.
 * 	[89/01/10  13:30:45  rpd]
 * 
 * Revision 2.7  88/12/19  02:44:58  mwyoung
 * 	Remove lint.
 * 	[88/12/08            mwyoung]
 * 
 * Revision 2.6  88/10/11  10:17:00  rpd
 * 	Added port_set_backup.
 * 	[88/10/11  08:03:29  rpd]
 * 	
 * 	Added missing checks for a null task argument
 * 	to port_set_status and port_set_select.
 * 	[88/10/05  10:26:02  rpd]
 * 
 * Revision 2.5  88/09/25  22:12:41  rpd
 * 	Changed PORT_RESERVED definition; only PORT_ENABLED & PORT_NULL
 * 	are reserved now.
 * 	[88/09/24  18:01:21  rpd]
 * 	
 * 	Updated vm_move calls.
 * 	[88/09/20  16:12:03  rpd]
 * 	
 * 	Fixed bug in port_insert_send/port_insert_receive:
 * 	previously they didn't check for null ports.
 * 	[88/09/19  23:43:48  rpd]
 * 	
 * 	Fixed to define PORT_RESERVED predicate using PORT_RESERVED_CACHE.
 * 	[88/09/19  23:29:13  rpd]
 * 	
 * 	Added port_set_select.
 * 	[88/09/19  17:31:27  rpd]
 * 	
 * 	Changed to new-style includes.
 * 	[88/09/09  18:24:42  rpd]
 * 
 * Revision 2.4  88/07/29  03:19:24  rpd
 * Fixed port_insert_send and port_insert_receive to check for the case
 * that the target task already has rights for the port, and return
 * KERN_FAILURE in this case.
 * 
 * Revision 2.3  88/07/22  07:28:56  rpd
 * Removed global variable declarations.  Fixed port_set_add to
 * allow the port to already be in a set.  Use PORT_TYPE_IS_* macros.
 * 
 * Revision 2.2  88/07/20  16:33:06  rpd
 * Use KERN_NAME_EXISTS where appropriate.
 * Created from mach_ipc.c.
 * 
 */
/*
 * File:	ipc_port.c
 * Purpose:
 *	Exported port and port set functions.
 */

#import <mach_ipc_xxxhack.h>
#import <mach_old_vm_copy.h>

#import <sys/kern_return.h>
#import <sys/boolean.h>
#import <sys/port.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/kern_obj.h>
#import <kern/kern_port.h>
#import <kern/kern_set.h>
#import <kern/assert.h>
#import <kern/zalloc.h>
#import <kern/sched_prim_macros.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <kern/ipc_cache.h>
#import <kern/ipc_hash.h>
#import <kern/ipc_globals.h>
#import <kern/ipc_prims.h>
#import <vm/vm_kern.h>

#if	MACH_IPC_XXXHACK

#define PORT_RESERVED(name)			\
		(((name) == PORT_NULL) ||	\
		 ((name) == PORT_ENABLED))

#else	MACH_IPC_XXXHACK

#define PORT_RESERVED(name)	((name) == PORT_NULL)

#endif	MACH_IPC_XXXHACK

/*
 *	Routine:	port_translate [internal]
 *	Purpose:
 *		Translate from a task's local name for a port to the
 *		internal kernel data structure.  If the task doesn't
 *		have rights or the task isn't active or the port isn't
 *		active, returns FALSE.
 *	Conditions:
 *		Nothing locked before.  If successful, returns a locked
 *		port.  Doesn't increment the number of refs for the port.
 *		The port will/must have at least one ref, however.
 */
boolean_t
port_translate(task, port, kport)
	task_t task;
	port_name_t port;
	kern_port_t *kport;
{
	ipc_task_lock(task);

	if (task->ipc_active) {
		register port_hash_t entry = obj_entry_lookup(task, port);

		if ((entry != PORT_HASH_NULL) &&
		    PORT_TYPE_IS_PORT(entry->type)) {
			register kern_port_t kp = (kern_port_t) entry->obj;

			port_lock(kp);
			if (kp->port_in_use) {
				ipc_task_unlock(task);
				*kport = kp;
				return TRUE;
			}
			port_unlock(kp);
		}
	}

	ipc_task_unlock(task);
	return FALSE;
}

/*
 *	Routine:	set_translate [internal]
 *	Purpose:
 *		Translate from a task's local name for a set to the
 *		internal kernel data structure.  If the task doesn't
 *		have rights or the task isn't active, returns FALSE.
 *	Conditions:
 *		Nothing locked before.  If successful, returns a
 *		locked set.  Doesn't increment the number of refs for
 *		the set.  The set will/must have at least one ref,
 *		however.
 */
boolean_t
set_translate(task, name, kset)
	task_t task;
	port_set_name_t name;
	kern_set_t *kset;
{
	ipc_task_lock(task);

	if (task->ipc_active) {
		register port_hash_t entry = obj_entry_lookup(task, name);

		if ((entry != PORT_HASH_NULL) &&
		    PORT_TYPE_IS_SET(entry->type)) {
			register kern_set_t set = (kern_set_t) entry->obj;

			set_lock(set);
			if (set->set_in_use) {
				ipc_task_unlock(task);
				*kset = set;
				return TRUE;
			}
			set_unlock(set);
		}
	}

	ipc_task_unlock(task);
	return FALSE;
}

#if	MACH_IPC_XXXHACK

/*
 *	Routine:	xxx_port_allocate [exported, user, obsolete]
 *	Purpose:
 *		Allocate a new port, giving all rights to "task".
 *
 *		Returns in "my_port" the global name of the port;
 *		a reference to that port is provided (intended for
 *		a reply message, or for a kernel data structure).
 *	Conditions:
 *		No locks held on entry or exit.
 */
kern_return_t
xxx_port_allocate(task, my_port)
	task_t task;
	port_t *my_port;
{
	kern_port_t port;
	kern_return_t kr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	kr = port_alloc(task, &port);
	if (kr == KERN_SUCCESS) {
		port->port_references++; /* for the reply message */
		port_unlock(port);
		*my_port = (port_t) port;
	}

	return kr;
}

/*
 *	Routine:	xxx_port_deallocate [exported, user, obsolete]
 *	Purpose:
 *		Delete port rights to "port" from this "task".
 *	Conditions:
 *		No locks on entry or exit.
 *	Side effects:
 *		Port may be "destroyed" or rights moved.
 */
kern_return_t
xxx_port_deallocate(task, port)
	register task_t task;
	port_t port;
{
	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;
	return port_dealloc(task, (kern_port_t) port);
}

/*
 *	Routine:	xxx_port_enable [exported, user, obsolete]
 *	Purpose:
 *		Add "my_port" to the set of enabled ports for "task".
 *
 *		If "my_port" is PORT_NULL, it succeeds.  If "my_port"
 *		is already in the distinguished enabled set, the call
 *		succeeds.  If "my_port" is in some other set, it
 *		fails.  If "task" doesn't have receive rights for
 *		"my_port", it fails.
 *	Conditions:
 *		No locks held on entry or exit.
 */
kern_return_t
xxx_port_enable(task, my_port)
	task_t task;
	port_name_t my_port;
{
	kern_port_t port;
	kern_set_t set;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;
	if (my_port == PORT_NULL)
		return KERN_SUCCESS;

	/* Grab the enabled set for this task. */

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	set = task->ipc_enabled;
	assert(set != KERN_SET_NULL);
	set_reference(set);
	ipc_task_unlock(task);

	/* Check out the port. */

	port = (kern_port_t) my_port;
	port_lock(port);

	if (!port->port_in_use) {
		port_unlock(port);
		set_release(set);
		return KERN_INVALID_ARGUMENT;
	}

	if (port->port_receiver != task) {
		port_unlock(port);
		set_release(set);
		return KERN_NOT_RECEIVER;
	}

	if (port->port_set == set) {
		port_unlock(port);
		set_release(set);
		return KERN_SUCCESS;
	}

	if (port->port_set != KERN_SET_NULL) {
		port_unlock(port);
		set_release(set);
		return KERN_ALREADY_IN_SET;
	}

	/* OK, the port looks fine.  Lock the set too and check it. */

	set_lock(set);
	set->set_references--;

	if (!set->set_in_use) {
		set_check_unlock(set);
		port_unlock(port);
		return KERN_INVALID_ARGUMENT;
	}

	set_add_member(set, port);
	set_unlock(set);
	port_unlock(port);

	return KERN_SUCCESS;
}

/*
 *	Routine:	xxx_port_disable [exported, user, obsolete]
 *	Purpose:
 *		Remove "my_port" from the set of enabled ports for "task".
 *	Conditions:
 *		No locks held on entry or exit.
 */
kern_return_t
xxx_port_disable(task, my_port)
	task_t task;
	port_t my_port;
{
	kern_port_t port;
	kern_set_t set;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;
	if (my_port == PORT_NULL)
		return KERN_SUCCESS;

	port = (kern_port_t) my_port;
	port_lock(port);

	if (!port->port_in_use) {
		port_unlock(port);
		return KERN_INVALID_ARGUMENT;
	}

	if (port->port_receiver != task) {
		port_unlock(port);
		return KERN_NOT_RECEIVER;
	}

	set = port->port_set;
	if (set == KERN_SET_NULL) {
		port_unlock(port);
		return KERN_SUCCESS;
	}

	set_lock(set);
	set_remove_member(set, port);
	set_check_unlock(set);
	port_check_unlock(port);
	return KERN_SUCCESS;
}

/*
 *	Routine:	xxx_port_set_backlog [exported, user, obsolete]
 *	Purpose:
 *		Change the queueing backlog on "my_port" to "backlog";
 *		the specified "task" must be the current receiver.
 *	Conditions:
 *		No locks on entry or exit.
 *	Side effects:
 *		May wake up blocked senders if backlog is increased.
 */
kern_return_t
xxx_port_set_backlog(task, my_port, backlog)
	task_t task;
	port_t my_port;
	int backlog;
{
	register kern_port_t kp;
	register kern_return_t result;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;
	if (my_port == PORT_NULL)
		return KERN_SUCCESS;

	kp = (kern_port_t) my_port;
	port_lock(kp);

	if (!kp->port_in_use || (task != kp->port_receiver))
		result = KERN_NOT_RECEIVER;
	else if ((backlog <= 0) ||
		 (backlog > PORT_BACKLOG_MAX))
		result = KERN_INVALID_ARGUMENT;
	else {
	 	register int new_space;

		/* Wake up senders to meet the new backlog. */

		for (new_space = (backlog - kp->port_backlog); 
		     new_space > 0; new_space--) {
			register thread_t sender_to_wake;	

			if (queue_empty(&kp->port_blocked_threads))
				break;

			queue_remove_first(&kp->port_blocked_threads, 
					   sender_to_wake, thread_t, 
					   ipc_wait_queue);
			sender_to_wake->ipc_state = KERN_SUCCESS;
			thread_go(sender_to_wake);
		}
			
	 	kp->port_backlog = backlog;
		result = KERN_SUCCESS;
	}

	port_unlock(kp);
	return result;
}

/*
 *	Routine:	xxx_port_status [exported, user, obsolete]
 *	Purpose:
 *		Returns statistics related to "my_port", as seen by "task".
 *		Only the receiver for a given port will see true message
 *		counts.
 *	Conditions:
 *		No locks on entry or exit.
 */
kern_return_t
xxx_port_status(task, my_port, enabled, num_msgs, backlog,
		ownership, receive_rights)
	task_t task;
	port_t my_port;
	boolean_t *enabled;
	int *num_msgs;
	int *backlog;
	boolean_t *ownership;
	boolean_t *receive_rights;
{
	register kern_port_t kp;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	if (my_port == PORT_NULL) {
		*enabled = FALSE;
		*num_msgs = 0;
		*backlog = 0;
		*ownership = FALSE;
		*receive_rights = FALSE;
		return KERN_SUCCESS;
	}

	kp = (kern_port_t) my_port;
	port_lock(kp);

	*ownership = (kp->port_in_use && (task == kp->port_owner));
	if (*receive_rights = (kp->port_in_use && 
					(task == kp->port_receiver))) {
	 	*enabled = (kp->port_set != KERN_SET_NULL);
		*num_msgs = kp->port_message_count;
		*backlog = kp->port_backlog;
	} else {
		*enabled = FALSE;
		*num_msgs = -1;
		*backlog = 0;
	}

	port_unlock(kp);
	return KERN_SUCCESS;
}

/*
 *	Routine:	xxx_port_select [exported, user, obsolete]
 *	Purpose:
 *		Return a list of ports which have messages available.
 *		Only the enabled ports for this "task" are considered.
 *	Conditions:
 *		No locks on entry or exit.
 */
kern_return_t
xxx_port_select(task, members, membersCnt)
	task_t task;
	port_name_array_t *members;
	unsigned int *membersCnt;
{
	uprintf("xxx_port_select not implemented");
	return KERN_FAILURE;
}

#endif	MACH_IPC_XXXHACK

/*
 *	Routine:	port_names [exported, user]
 *	Purpose:
 *		Retrieve all the names in the task's port name space.
 *		As a (major) convenience, return port type information.
 *		The port name space includes port sets.
 *	Conditions:
 *		No locks held on entry or exit.
 */
kern_return_t
port_names(task, port_names_result, port_namesCnt, port_types, port_typesCnt)
	task_t task;
	port_name_array_t *port_names_result;
	unsigned int *port_namesCnt;
	port_type_array_t *port_types;
	unsigned int *port_typesCnt;
{
	unsigned int actual;	/* this many names */
	unsigned int space;	/* space for this many names */
	port_name_t *names;
	port_type_t *types;
	kern_return_t kr = KERN_SUCCESS;

	vm_size_t size;
	vm_offset_t addr1, addr2;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	/* safe simplifying assumption */
	assert_static(sizeof(port_name_t) == sizeof(port_type_t));

	/* initial guess for amount of memory to allocate */
	size = PAGE_SIZE;

	for (;;) {
		register port_hash_t entry;

		/* allocate memory non-pageable, so don't fault
		   while holding locks */
		(void) vm_allocate(ipc_kernel_map, &addr1, size, TRUE);
#if	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map,
				       addr1, addr1 + size, FALSE);
		(void) vm_allocate(ipc_kernel_map, &addr2, size, TRUE);
		(void) vm_map_pageable(ipc_kernel_map,
				       addr2, addr2 + size, FALSE);
#else	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map, addr1, addr1 + size,
				VM_PROT_READ|VM_PROT_WRITE);
		(void) vm_allocate(ipc_kernel_map, &addr2, size, TRUE);
		(void) vm_map_pageable(ipc_kernel_map, addr2, addr2 + size,
				VM_PROT_READ|VM_PROT_WRITE);
#endif	MACH_OLD_VM_COPY

		names = (port_name_t *) addr1;
		types = (port_type_t *) addr2;
		space = size / sizeof(port_name_t);
		actual = 0;

		ipc_task_lock(task);
		if (!task->ipc_active) {
			ipc_task_unlock(task);
			kr = KERN_INVALID_ARGUMENT;
			break;
		}

		/* teensy-tiny inefficiency here, if we exactly fill space
		   and then there are some dead objects.  we will abort
		   and get more memory when it isn't necessary. */

		entry = (port_hash_t) queue_first(&task->ipc_translations);
		while ((actual < space) &&
		       !queue_end(&task->ipc_translations,
				  (queue_entry_t) entry)) {
			register kern_obj_t obj = entry->obj;

			obj_lock(obj);
			if (obj->obj_in_use) {
				names[actual] = entry->local_name;
				types[actual] = entry->type;
				actual++;
			}
			obj_unlock(obj);

			entry = (port_hash_t) queue_next(&entry->task_chain);
		}

		/* we are done if we made it all the way through the list */
		if (queue_end(&task->ipc_translations,
			      (queue_entry_t) entry)) {
			ipc_task_unlock(task);
			break;
		}

		/* otherwise finish counting number of translations.
		   don't bother checking to be sure they are alive. */
		do {
			actual++;
			entry = (port_hash_t) queue_next(&entry->task_chain);
		} while (!queue_end(&task->ipc_translations,
				    (queue_entry_t) entry));

		ipc_task_unlock(task);

		/* free current memory blocks */
		(void) kmem_free(ipc_kernel_map, addr1, size);
		(void) kmem_free(ipc_kernel_map, addr2, size);

		/* go for another try, allowing for expansion */
		size = round_page(2 * actual * sizeof(port_name_t));
	}

	if (actual == 0) {
		/* no members, so return null pointer and deallocate memory */
		*port_names_result = 0;
		*port_types = 0;
		*port_namesCnt = *port_typesCnt = 0;

		(void) kmem_free(ipc_kernel_map, addr1, size);
		(void) kmem_free(ipc_kernel_map, addr2, size);
	} else {
		vm_size_t size_used;

		*port_namesCnt = *port_typesCnt = actual;

		size_used = round_page(actual * sizeof(port_name_t));

#if	MACH_OLD_VM_COPY
		/* finished touching it, so make the memory pageable */
		(void) vm_map_pageable(ipc_kernel_map,
				       addr1, addr1 + size_used, TRUE);
		(void) vm_map_pageable(ipc_kernel_map,
				       addr2, addr2 + size_used, TRUE);

		/* the memory needs to be in ipc_soft_map */
		(void) vm_move(ipc_kernel_map, addr1,
			       ipc_soft_map, size_used, TRUE,
			       (vm_offset_t *) port_names_result);
		(void) vm_move(ipc_kernel_map, addr2,
			       ipc_soft_map, size_used, TRUE,
			       (vm_offset_t *) port_types);
#else	MACH_OLD_VM_COPY
		/* finished touching it, so make the memory pageable */
		(void) vm_map_pageable(ipc_kernel_map,
			       addr1, addr1 + size_used, VM_PROT_NONE);
		(void) vm_map_pageable(ipc_kernel_map,
			       addr2, addr2 + size_used, VM_PROT_NONE);

		/* the memory needs to be in copied-in form */
		(void) vm_map_copyin(
				ipc_kernel_map,
				addr1,
				size_used,
				TRUE,
				(vm_map_copy_t *)port_names_result);
		(void) vm_map_copyin(
				ipc_kernel_map,
				addr2,
				size_used,
				TRUE,
				(vm_map_copy_t *)port_types);
#endif	MACH_OLD_VM_COPY

		/* free any unused memory */
		if (size_used != size) {
			(void) kmem_free(ipc_kernel_map,
					 addr1 + size_used, size - size_used);
			(void) kmem_free(ipc_kernel_map,
					 addr2 + size_used, size - size_used);
		}
	}

	return kr;
}

/*
 *	Routine:	port_type [exported, user]
 *	Purpose:
 *		Return type of the capability named.
 *	Conditions:
 *		No locks held on entry or exit.
 */
kern_return_t
port_type(task, port_name, port_type_result)
	task_t task;
	port_name_t port_name;
	port_type_t *port_type_result;
{
	register port_hash_t entry;
	register kern_obj_t obj;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	entry = obj_entry_lookup(task, port_name);
	if (entry == PORT_HASH_NULL) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	obj = entry->obj;
	obj_lock(obj);

	/* now that object is locked, can unlock task and
	   the entry is still guaranteed to remain stable. */
	ipc_task_unlock(task);

	if (!obj->obj_in_use) {
		obj_unlock(obj);
		return KERN_INVALID_ARGUMENT;
	}

	*port_type_result = entry->type;
	obj_unlock(obj);

	return KERN_SUCCESS;
}

/*
 *	Routine:	port_rename [exported, user]
 *	Purpose:
 *		Change the name of a capability.
 *		The new name can't be in use.
 *	Conditions:
 *		No locks held on entry or exit.
 */
kern_return_t
port_rename(task, old_name, new_name)
	task_t task;
	port_name_t old_name;
	port_name_t new_name;
{
	register port_hash_t entry;
	register kern_obj_t obj;

	if ((task == TASK_NULL) || PORT_RESERVED(new_name))
		return KERN_INVALID_ARGUMENT;

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	/* check if new_name is already in use */
	if (task_check_name(task, new_name)) {
		ipc_task_unlock(task);
		return KERN_NAME_EXISTS;
	}

	entry = obj_entry_lookup(task, old_name);
	if (entry == PORT_HASH_NULL) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	obj = entry->obj;
	obj_lock(obj);

	if (!obj->obj_in_use) {
		obj_unlock(obj);
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	/* OK, the task is locked, the object is locked,
	   both are alive and kicking, and new_name is free. */

	obj_entry_change(task, obj, entry, old_name, new_name);
	ipc_task_unlock(task);
	/* Can't modify entry now that task is unlocked. */

	/* may have more to do, depending on rights held */

	switch (entry->type) {
#if	MACH_IPC_XXXHACK
	    case PORT_TYPE_RECEIVE:
#endif	MACH_IPC_XXXHACK
	    case PORT_TYPE_RECEIVE_OWN: {
		register kern_port_t port = (kern_port_t) obj;
		register kern_set_t set = port->port_set;

		assert(port->port_receiver == task);
		if (set != KERN_SET_NULL) {
			/* if port is in a set, it must be locked when
			   port_receiver_name is changed. */
			set_lock(set);
			port->port_receiver_name = new_name;
			set_unlock(set);
		} else
			port->port_receiver_name = new_name;
		break;
	    }

	    case PORT_TYPE_SET: {
		register kern_set_t set = (kern_set_t) obj;

		assert(set->set_owner == task);
		set->set_local_name = new_name;
		break;
	    }

	    default:
		break;
	}

	obj_unlock(obj);
	return KERN_SUCCESS;
}

/*
 *	Routine:	port_allocate [exported, user]
 *	Purpose:
 *		Allocate a new port, giving all rights to "task".
 *
 *		Returns in "port_name" the task's local name for the port.
 *		Doesn't return a reference to the port.
 *	Conditions:
 *		No locks held on entry or exit.
 */
kern_return_t
port_allocate(task, port_name)
	task_t task;
	port_name_t *port_name;
{
	kern_port_t port;
	kern_return_t kr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	kr = port_alloc(task, &port);
	if (kr == KERN_SUCCESS) {
		*port_name = port->port_receiver_name;
		port_unlock(port);
	}

	return kr;
}

/*
 *	Routine:	port_deallocate [exported, user]
 *	Purpose:
 *		Delete port rights to "port" from this "task".
 *	Conditions:
 *		No locks on entry or exit.
 *	Side effects:
 *		Port may be "destroyed" or rights moved.
 */
kern_return_t
port_deallocate(task, port_name)
	task_t task;
	port_name_t port_name;
{
	kern_port_t port;
	kern_return_t kr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	if (!port_translate(task, port_name, &port))
		return KERN_INVALID_ARGUMENT;
	/* port is locked */

	/* take a ref, so the port doesn't disappear */
	port->port_references++;
	port_unlock(port);

	kr = port_dealloc(task, port);

	port_release(port);
	return kr;
}

/*
 *	Routine:	port_set_backlog [exported, user]
 *	Purpose:
 *		Change the queueing backlog on "port_name" to "backlog";
 *		the specified "task" must be the current receiver.
 *	Conditions:
 *		No locks on entry or exit.
 *	Side effects:
 *		May wake up blocked senders if backlog is increased.
 */
kern_return_t
port_set_backlog(task, port_name, backlog)
	task_t task;
	port_name_t port_name;
	int backlog;
{
	kern_port_t port;
	kern_return_t kr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	if (!port_translate(task, port_name, &port))
		return KERN_INVALID_ARGUMENT;
	/* port is locked */

	if (task != port->port_receiver)
		kr = KERN_NOT_RECEIVER;
	else if ((backlog <= 0) || (backlog > PORT_BACKLOG_MAX))
		kr = KERN_INVALID_ARGUMENT;
	else {
	 	register int new_space;

		/* Wake up senders to meet the new backlog. */

		for (new_space = (backlog - port->port_backlog); 
		     new_space > 0; new_space--) {
			register thread_t sender_to_wake;	

			if (queue_empty(&port->port_blocked_threads))
				break;

			queue_remove_first(&port->port_blocked_threads, 
					   sender_to_wake, thread_t, 
					   ipc_wait_queue);
			sender_to_wake->ipc_state = KERN_SUCCESS;
			thread_go(sender_to_wake);
		}

	 	port->port_backlog = backlog;
		kr = KERN_SUCCESS;
	}

	port_unlock(port);
	return kr;
}

/*
 *	Routine:	port_set_backup [exported, user]
 *	Purpose:
 *		Changes the backup port for the the named port.
 *		The specified "task" must be the current receiver.
 *		Returns the old backup port, if any.
 *	Conditions:
 *		No locks on entry or exit.
 */
kern_return_t
port_set_backup(task, port_name, backup, previous)
	task_t task;
	port_name_t port_name;
	kern_port_t backup;
	kern_port_t *previous;
{
	kern_port_t port;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	/* take a ref for the backup before locking target port */
	if (backup != KERN_PORT_NULL)
		port_reference(backup);

	if (!port_translate(task, port_name, &port)) {
		if (backup != KERN_PORT_NULL)
			port_release(backup);
		return KERN_INVALID_ARGUMENT;
	}
	/* port is locked */

	if (task != port->port_receiver) {
		port_unlock(port);
		if (backup != KERN_PORT_NULL)
			port_release(backup);
		return KERN_NOT_RECEIVER;
	}

	/* Transfer port's ref for the existing backup to the reply
	   message.  Transfer the ref we took above for the new backup
	   to the target port. */

	*previous = port->port_backup;
	port->port_backup = backup;

	port_unlock(port);

	return KERN_SUCCESS;
}

/*
 *	Routine:	port_status [exported, user]
 *	Purpose:
 *		Returns statistics related to "port_name", as seen by "task".
 *		Only the receiver for a given port will see true message
 *		counts.
 *	Conditions:
 *		No locks on entry or exit.
 */
kern_return_t
port_status(task, port_name, enabled, num_msgs, backlog,
	       ownership, receive_rights)
	task_t task;
	port_name_t port_name;
	port_name_t *enabled;
	int *num_msgs;
	int *backlog;
	boolean_t *ownership;
	boolean_t *receive_rights;
{
	kern_port_t port;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	if (!port_translate(task, port_name, &port))
		return KERN_INVALID_ARGUMENT;
	/* port is locked */

	if (*receive_rights = (task == port->port_receiver)) {
		kern_set_t set = port->port_set;
		port_name_t name = PORT_NULL;

		if (set != KERN_SET_NULL) {
			set_lock(set);
			if (set->set_in_use)
				name = set->set_local_name;
			/* not necessary to hasten removal if set is dying */
			set_unlock(set);
		}

		*enabled = name;
		*num_msgs = port->port_message_count;
		*backlog = port->port_backlog;
	} else {
		*enabled = PORT_NULL;
		*num_msgs = -1;
		*backlog = 0;
	}

#if	MACH_IPC_XXXHACK
	*ownership = (task == port->port_owner);
#else	MACH_IPC_XXXHACK
	*ownership = *receive_rights;
#endif	MACH_IPC_XXXHACK

	port_unlock(port);
	return KERN_SUCCESS;
}

/*
 *	Routine:	port_set_allocate [exported, user]
 *	Purpose:
 *		Create a new port set, give rights to task, and
 *		return task's local name for the set.
 */
kern_return_t
port_set_allocate(task, set_name)
	task_t task;
	port_set_name_t *set_name;
{
	kern_set_t set;
	kern_return_t kr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	kr = set_alloc(task, &set);
	if (kr == KERN_SUCCESS) {
		/* we g'tee to return the task's first local name for
		   the set even in the face of random renaming */
		*set_name = set->set_local_name;
		set_unlock(set);
	}
	return kr;
}

/*
 *	Routine:	port_set_deallocate [exported, user]
 *	Purpose:
 *		Destroys the task's port set.  If there are any
 *		receive rights in the set, they are removed.
 */
kern_return_t
port_set_deallocate(task, set_name)
	task_t task;
	port_set_name_t set_name;
{
	kern_set_t set;
	port_hash_t entry;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	entry = obj_entry_lookup(task, set_name);
	if ((entry == PORT_HASH_NULL) ||
	    !PORT_TYPE_IS_SET(entry->type)) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	set = (kern_set_t) entry->obj;
	set_lock(set);
	if (!set->set_in_use) {
		set_unlock(set);
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	/* will unlock task and set */
	set_destroy(task, set, entry);
	return KERN_SUCCESS;
}

/*
 *	Routine:	port_set_add [exported, user]
 *	Purpose:
 *		Moves receive rights into the port set.  The rights
 *		can't already be in a (live) port set.
 */
kern_return_t
port_set_add(task, set_name, port_name)
	task_t task;
	port_set_name_t set_name;
	port_name_t port_name;
{
	kern_port_t port;
	kern_set_t oset, nset;
	port_hash_t entry;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	entry = obj_entry_lookup(task, port_name);
	if ((entry == PORT_HASH_NULL) ||
	    !PORT_TYPE_IS_PORT(entry->type)) {
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	port = (kern_port_t) entry->obj;
	port_lock(port);
	if (!port->port_in_use) {
		port_unlock(port);
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	if (port->port_receiver != task) {
		port_unlock(port);
		ipc_task_unlock(task);
		return KERN_NOT_RECEIVER;
	}

	entry = obj_entry_lookup(task, set_name);
	if ((entry == PORT_HASH_NULL) ||
	    !PORT_TYPE_IS_SET(entry->type)) {
		port_unlock(port);
		ipc_task_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	oset = port->port_set;
	nset = (kern_set_t) entry->obj;

	/* Need to remove the port from any set it may be in.
	   However, we want this to be atomic, so that at no
	   point is the port not in a set, according to any
	   port_status or port_set_status calls.  Must be
	   especially careful that any errors leave the port
	   unchanged.

	   To make an atomic switch, need to have port and
	   both sets locked.  Must be careful of locking order
	   to get to this situation. */

	if (oset == KERN_SET_NULL) {
		/* No problem here, just do the set_add_member */

		set_lock(nset);

		/* Okay, now that port & nset are locked can unlock task */
		ipc_task_unlock(task);

		if (!nset->set_in_use) {
			set_unlock(nset);
			port_unlock(port);
			return KERN_INVALID_ARGUMENT;
		}

		set_add_member(nset, port);

		set_unlock(nset);
		port_unlock(port);
	} else if (oset == nset) {
		/* Must check that set is active, to report error properly. */

		set_lock(nset);

		/* Okay, now that port & nset are locked can unlock task */
		ipc_task_unlock(task);

		if (!nset->set_in_use) {
			set_unlock(nset);
			port_unlock(port);
			return KERN_INVALID_ARGUMENT;
		}

		set_unlock(nset);
		port_unlock(port);
	} else {
		/* Need to lock the sets in order, and make atomic switch */

		if (oset < nset) {
			set_lock(oset);
			set_lock(nset);
		} else {
			set_lock(nset);
			set_lock(oset);
		}

		/* Okay, now that port & nset are locked can unlock task */
		ipc_task_unlock(task);

		if (!nset->set_in_use) {
			set_unlock(nset);
			set_unlock(oset);
			port_unlock(port);
			return KERN_INVALID_ARGUMENT;
		}

		/* set_remove_member is kosher even if oset is dying */
		set_remove_member(oset, port);
		set_add_member(nset, port);

		set_unlock(nset);
		set_check_unlock(oset);
		port_unlock(port);
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	port_set_remove [exported, user]
 *	Purpose:
 *		Removes the receive rights from the set they are in.
 */
kern_return_t
port_set_remove(task, port_name)
	task_t task;
	port_name_t port_name;
{
	kern_port_t port;
	kern_set_t set;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	if (!port_translate(task, port_name, &port))
		return KERN_INVALID_ARGUMENT;
	/* port is locked */

	if (port->port_receiver != task) {
		port_unlock(port);
		return KERN_NOT_RECEIVER;
	}

	set = port->port_set;
	if (set == KERN_SET_NULL) {
		port_unlock(port);
		return KERN_NOT_IN_SET;
	}

	set_lock(set);
	if (!set->set_in_use) {
		/* could use set_remove_member, but not necessary */
		set_unlock(set);
		port_unlock(port);
		return KERN_NOT_IN_SET;
	}

	set_remove_member(set, port);
	set_check_unlock(set);
	port_check_unlock(port);
	return KERN_SUCCESS;
}

/*
 *	Routine:	port_set_status [exported, user]
 *	Purpose:
 *		Retrieve list of members of a port set.
 *	Conditions:
 *		No locks on entry or exit.
 */
kern_return_t
port_set_status(task, set_name, members, membersCnt)
	task_t task;
	port_set_name_t set_name;
	port_name_array_t *members;
	unsigned int *membersCnt;
{
	kern_set_t set;
	unsigned int actual;	/* this many members */
	unsigned int space;	/* space for this many members */
	port_name_t *names;
	kern_return_t kr = KERN_SUCCESS;

	vm_size_t size;
	vm_offset_t addr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	if (!set_translate(task, set_name, &set))
		return KERN_INVALID_ARGUMENT;
	/* set is locked */

	/* ensure set doesn't go away while it is unlocked */
	set->set_references++;
	set_unlock(set);

	/* initial guess for amount of memory to allocate */
	size = page_size;

	for (;;) {
		register kern_port_t port;

		/* allocate memory non-pageable, so don't fault
		   while holding locks */
		(void) vm_allocate(ipc_kernel_map, &addr, size, TRUE);
#if	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map,
				       addr, addr + size, FALSE);
#else	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map, addr, addr + size,
				VM_PROT_READ|VM_PROT_WRITE);
#endif	MACH_OLD_VM_COPY

		names = (port_name_t *) addr;
		space = size / sizeof(port_name_t);
		actual = 0;

		/* because set was unlocked, it might be invalid now */
		set_lock(set);
		if (!set->set_in_use) {
			kr = KERN_INVALID_ARGUMENT;
			break;
		}

		/* we don't have to lock the ports to access port_receiver
		   and port_receiver_name.  port_receiver can't change
		   while the port is in a set, and port_receiver_name
		   is only changed while the set is also locked. */

		for (port = (kern_port_t) queue_first(&set->set_members);
		     (actual < space) &&
		     !queue_end(&set->set_members, (queue_entry_t) port);
		     actual++,
		     port = (kern_port_t) queue_next(&port->port_brothers)) {
			assert(port->port_receiver == task);
			names[actual] = port->port_receiver_name;
		}

		/* we are done if we made it all the way through the list */
		if (queue_end(&set->set_members, (queue_entry_t) port))
			break;

		/* otherwise finish counting current set size */
		do {
			actual++;
			port = (kern_port_t) queue_next(&port->port_brothers);
		} while (!queue_end(&set->set_members, (queue_entry_t) port));

		set_unlock(set);

		/* free current memory block */
		(void) kmem_free(ipc_kernel_map, addr, size);

		/* go for another try, allowing for expansion */
		size = round_page(2 * actual * sizeof(port_name_t));
	}

	/* release the ref we took above */
	set->set_references--;
	set_check_unlock(set);

	if (actual == 0) {
		/* no members, so return null pointer and deallocate memory */
		*members = 0;
		*membersCnt = 0;

		(void) kmem_free(ipc_kernel_map, addr, size);
	} else {
		vm_size_t size_used;

		*membersCnt = actual;

		size_used = round_page(actual * sizeof(port_name_t));

#if	MACH_OLD_VM_COPY
		/* finished touching it, so make the memory pageable */
		(void) vm_map_pageable(ipc_kernel_map,
				       addr, addr + size_used, TRUE);

		/* the memory needs to be in ipc_soft_map */
		(void) vm_move(ipc_kernel_map, addr,
			       ipc_soft_map, size_used, TRUE,
			       (vm_offset_t *) members);

#else	MACH_OLD_VM_COPY
		/* finished touching it, so make the memory pageable */
		(void) vm_map_pageable(ipc_kernel_map,
				       addr, addr + size_used, VM_PROT_NONE);

		/* the memory needs to be in copied-in form */
		(void) vm_map_copyin(ipc_kernel_map, addr, size_used, TRUE,
			       (vm_map_copy_t *) members);
#endif	MACH_OLD_VM_COPY

		/* free any unused memory */
		if (size_used != size)
			(void) kmem_free(ipc_kernel_map,
					 addr + size_used, size - size_used);
	}

	return kr;
}

/*
 *	Routine:	port_insert_send [exported, user]
 *	Purpose:
 *		Inserts send rights to a port into a task,
 *		at a given name.  The name must not be in use.
 *	Conditions:
 *		Nothing locked.
 */
kern_return_t
port_insert_send(task, my_port, his_name)
	task_t task;
	port_t my_port;
	port_name_t his_name;
{
	register kern_port_t port = (kern_port_t) my_port;
	register port_hash_t entry;

	if ((task == TASK_NULL) ||
	    (my_port == PORT_NULL) ||
	    PORT_RESERVED(his_name))
		return KERN_INVALID_ARGUMENT;

	/* allocate a new translation entry before locking anything */
	ZALLOC(port_hash_zone, entry, port_hash_t);
	assert(entry != PORT_HASH_NULL);

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		ZFREE(port_hash_zone, (vm_offset_t) entry);
		return KERN_INVALID_ARGUMENT;
	}

	/* check if his_name is already in use */
	if (task_check_name(task, his_name)) {
		ipc_task_unlock(task);
		ZFREE(port_hash_zone, (vm_offset_t) entry);
		return KERN_NAME_EXISTS;
	}

	/* check if the task already has rights for the port */
	if (task_check_rights(task, &port->port_obj)) {
		ipc_task_unlock(task);
		ZFREE(port_hash_zone, (vm_offset_t) entry);
		return KERN_FAILURE;
	}

	port_lock(port);
	if (!port->port_in_use) {
		port_unlock(port);
		ipc_task_unlock(task);
		ZFREE(port_hash_zone, (vm_offset_t) entry);
		return KERN_INVALID_ARGUMENT;
	}

	/* OK, the task is locked, the port is locked, both are alive,
	   the new name is not in use, and we have an entry allocated. */

	obj_entry_insert(entry, task, &port->port_obj,
			 his_name, PORT_TYPE_SEND);

	port_unlock(port);
	ipc_task_unlock(task);
	return KERN_SUCCESS;
}

/*
 *	Routine:	port_extract_send [exported, user]
 *	Purpose:
 *		Extracts send rights from "task"'s "his_name" port.
 *		The task is left with no rights for the port.
 *	Conditions:
 *		Nothing locked.
 */
kern_return_t
port_extract_send(task, his_name, his_port)
	task_t task;
	port_name_t his_name;
	port_t *his_port;
{
	if ((task == TASK_NULL) || (his_name == PORT_NULL))
		return KERN_INVALID_ARGUMENT;

	if (object_copyin(task, his_name, MSG_TYPE_PORT,
			  TRUE, (kern_obj_t *) his_port))
		return KERN_SUCCESS;
	else
		return KERN_INVALID_ARGUMENT;
}

/*
 *	Routine:	port_insert_receive [exported, user]
 *	Purpose:
 *		Inserts receive/ownership rights to a port into a task,
 *		at a given name.
 *	Conditions:
 *		Nothing locked.
 */
kern_return_t
port_insert_receive(task, my_port, his_name)
	task_t task;
	port_t my_port;
	port_name_t his_name;
{
	register kern_port_t port = (kern_port_t) my_port;
	register port_hash_t entry;

	if ((task == TASK_NULL) ||
	    (my_port == PORT_NULL) ||
	    PORT_RESERVED(his_name))
		return KERN_INVALID_ARGUMENT;

	/* allocate a new translation entry before locking anything */
	ZALLOC(port_hash_zone, entry, port_hash_t);
	assert(entry != PORT_HASH_NULL);

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		ZFREE(port_hash_zone, (vm_offset_t) entry);
		return KERN_INVALID_ARGUMENT;
	}

	/* check if his_name is already in use */
	if (task_check_name(task, his_name)) {
		ipc_task_unlock(task);
		ZFREE(port_hash_zone, (vm_offset_t) entry);
		return KERN_NAME_EXISTS;
	}

	/* check if the task already has rights for the port */
	if (task_check_rights(task, &port->port_obj)) {
		ipc_task_unlock(task);
		ZFREE(port_hash_zone, (vm_offset_t) entry);
		return KERN_FAILURE;
	}

	port_lock(port);
	/* The port must be alive, because receive & ownership rights
	   are in the message. */
	assert(port->port_in_use);

	/* OK, the task is locked, the port is locked, both are alive,
	   the new name is not in use, and we have an entry allocated. */

	obj_entry_insert(entry, task, &port->port_obj,
			 his_name, PORT_TYPE_RECEIVE_OWN);

	/* Move receive & ownership rights to the task. */
	port_copyout_receive_own(task, port, his_name);

	port_unlock(port);
	ipc_task_unlock(task);
	return KERN_SUCCESS;
}

/*
 *	Routine:	port_extract_receive [exported, user]
 *	Purpose:
 *		Extracts receive/ownership rights
 *		from "task"'s "his_name" port.
 *
 *		The task is left with no rights for the port.
 *	Conditions:
 *		Nothing locked.
 */
kern_return_t
port_extract_receive(task, his_name, his_port)
	task_t task;
	port_name_t his_name;
	port_t *his_port;
{
	if ((task == TASK_NULL) || (his_name == PORT_NULL))
		return KERN_INVALID_ARGUMENT;

	if (object_copyin(task, his_name, MSG_TYPE_PORT_ALL,
			  TRUE, (kern_obj_t *) his_port))
		return KERN_SUCCESS;
	else
		return KERN_INVALID_ARGUMENT;
}







