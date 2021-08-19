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
 * $Log:	ipc_kport.c,v $
 * Revision 2.13  89/12/22  16:28:05  rpd
 * 	Added port_lookup() call.
 * 	[89/11/24  15:05:37  rpd]
 * 
 * Revision 2.12  89/06/27  00:24:01  rpd
 * 	Added type argument to obj_alloc.
 * 	[89/06/26  23:51:44  rpd]
 * 
 * Revision 2.11  89/03/05  16:47:01  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK.
 * 	[89/02/16  13:46:51  rpd]
 * 
 * Revision 2.10  89/02/25  18:02:36  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.9  89/01/12  07:55:27  rpd
 * 	Moved ipc_statistics.h to mach_debug/.
 * 	[89/01/12  04:45:04  rpd]
 * 
 * Revision 2.8  88/12/20  13:51:59  rpd
 * 	Rewrote port_dealloc, fixing bug with ownership notifications.
 * 	They were getting send to the old owner instead of the receiver.
 * 	[88/12/09  13:37:16  rpd]
 * 
 * Revision 2.7  88/12/19  02:44:14  mwyoung
 * 	Removed lint.
 * 	[88/12/08            mwyoung]
 * 	
 * 	Added port_print().
 * 	[88/12/04            mwyoung]
 * 
 * Revision 2.6  88/11/23  16:38:51  rpd
 * 	Changed mach_ipc_debug to ipc_debug.
 * 	Added refcount debugging to the end of port_destroy.
 * 	[88/11/23  10:41:29  rpd]
 * 
 * Revision 2.5  88/10/18  03:20:25  mwyoung
 * 	Revert to using vm_object_destroy, as there are consistency
 * 	constraints on the use of the port object value maintained in the
 * 	vm_object module.
 * 	[88/09/16  18:46:27  mwyoung]
 * 
 * Revision 2.4  88/10/11  10:15:34  rpd
 * 	Modified for NOTIFY_PORT_DESTROYED implementation.
 * 	[88/10/11  07:59:42  rpd]
 * 	
 * 	Bump port_allocations statistic in port_alloc.
 * 	[88/10/10  07:56:09  rpd]
 * 	
 * 	When waking up a sleeping sender,
 * 	use SEND_SUCCESS instead of KERN_SUCCESS.
 * 	[88/10/09  16:04:54  rpd]
 * 	
 * 	Don't need sender_task to implement SEND_NOTIFY.
 * 	[88/10/06  07:53:20  rpd]
 * 	
 * 	Updated for new SEND_NOTIFY implementation.
 * 	[88/10/04  07:04:09  rpd]
 * 
 * Revision 2.3  88/08/25  18:15:22  mwyoung
 * 	Corrected include file references.
 * 	[88/08/22            mwyoung]
 * 	
 * 	Use memory_object_destroy() (with different arguments) instead of
 * 	vm_object_destroy().
 * 	[88/08/11  19:13:21  mwyoung]
 * 
 * Revision 2.2  88/08/06  18:16:10  rpd
 * Created.
 * 
 */
/*
 * File:	ipc_kport.c
 * Purpose:
 *	IPC port functions.
 */

#import <mach_ipc_xxxhack.h>
#import <mach_net.h>
#import <mach_np.h>
#import <mach_xp.h>

#import <sys/kern_return.h>
#import <kern/zalloc.h>
#import <kern/queue.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/msg_queue.h>
#import <kern/kern_port.h>
#import <sys/notify.h>
#import <kern/sched_prim_macros.h>
#import <kern/ipc_hash.h>
#import <kern/ipc_prims.h>
#import <kern/ipc_globals.h>
#import <kern/ipc_kmesg.h>
#import <kern/ipc_statistics.h>
#if	MACH_XP
#import <vm/vm_object.h>
#endif	MACH_XP

/*
 *	Routine:	port_reference [exported]
 *	Purpose:
 *		Acquire a reference to the port in question, preventing
 *		its destruction.  Also comes in macro form.
 */
void
port_reference(port)
	kern_port_t port;
{
	port_reference_macro(port);
}

/*
 *	Routine:	port_release [exported]
 *	Purpose:
 *		Release a port reference.
 *		Also comes in macro form.
 */
void
port_release(port)
	kern_port_t port;
{
	port_release_macro(port);
}

/*
 *	Routine:	port_alloc [internal]
 *	Purpose:
 *		Allocate a new port, giving all rights to "task".
 *		Allocates and initializes the translation.
 *		Returns a locked port, with no ref for the caller.
 *	Conditions:
 *		No locks held on entry.  Port locked on exit.
 */
kern_return_t
port_alloc(task, portp)
	task_t task;
	kern_port_t *portp;
{
	kern_return_t kr;
	zone_t my_zone;
	port_hash_t entry;

	/* note we are inspecting ipc_privilege without having the task locked.
	   this should be OK, assuming the value of ipc_privilege doesn't
	   change after the task is initialized. */
	my_zone = task->ipc_privilege ? port_zone_reserved : port_zone;

	kr = obj_alloc(task, my_zone, PORT_TYPE_RECEIVE_OWN, &entry);
	if (kr == KERN_SUCCESS) {
		register kern_port_t port = (kern_port_t) entry->obj;

		port->port_receiver_name = entry->local_name;
		port->port_receiver = task;
#if	MACH_IPC_XXXHACK
		port->port_owner = task;
#endif	MACH_IPC_XXXHACK
		port->port_backup = KERN_PORT_NULL;

		port->port_message_count = 0;
		port->port_backlog = PORT_BACKLOG_DEFAULT;
		msg_queue_init(&port->port_messages);
		queue_init(&port->port_blocked_threads);

		port->port_set = KERN_SET_NULL;
		/* don't have to initialize port_brothers */

		port->port_object.kp_type = PORT_OBJECT_NONE;

#if	MACH_NP
		port->port_external = (int **) 0;
#endif	MACH_NP

		*portp = port;
		ipc_event(port_allocations);
	}

	return kr;
}

/*
 *	Routine:	port_destroy
 *	Purpose:
 *		Shut down a port; called after both receiver and owner die.
 *		Destroys all queued messages and port rights associated with
 *		the port, sending death messages as appropriate.  References
 *		to this port held by the kernel or in other messages in
 *		transit may still remain.
 *	Conditions:
 *		The port must be locked on entry, and have one spare
 *		reference; the reference and the lock will both be released.
 */
void
port_destroy(kp)
	register kern_port_t kp;
{
	register kern_msg_t kmsg;
	register queue_t dead_queue;
	kern_port_t backup;
	thread_t thread_to_wake;

	/* Check that both receive & ownership rights are dead. */

	assert(kp->port_in_use);
	assert(kp->port_receiver == TASK_NULL);
#if	MACH_IPC_XXXHACK
	assert(kp->port_owner == TASK_NULL);
#endif	MACH_IPC_XXXHACK

	/* First check for a backup port.  */

	backup = kp->port_backup;
	if (backup != KERN_PORT_NULL) {
		/* Clear the backup port, and transfer kp's ref to ourself. */
		kp->port_backup = KERN_PORT_NULL;

		/* Need to send kp away in a notification.
		   The notification will get the receive/owner rights.
		   We transfer our ref to the notification. */

		kp->port_receiver = ipc_soft_task;
#if	MACH_IPC_XXXHACK
		kp->port_owner = ipc_soft_task;
#endif	MACH_IPC_XXXHACK
		port_unlock(kp);

		send_complex_notification(backup, NOTIFY_PORT_DESTROYED,
					  MSG_TYPE_PORT_ALL, kp);

		port_release(backup);
		return;
	}

	/* Mark the port as no longer in use. */

	kp->port_in_use = FALSE;
	port_unlock(kp);

	/* Throw away all of the messages. */

	dead_queue = &kp->port_messages.messages;
	while ((kmsg = (kern_msg_t) dequeue_head(dead_queue)) !=
	    KERN_MSG_NULL) {
		register port_hash_t entry;

		assert((kern_port_t) kmsg->kmsg_header.msg_local_port == kp);

		/* Must have port locked to check msg-accepted flag. */
		port_lock(kp);

		entry = kmsg->sender_entry;
		if (entry != PORT_HASH_NULL) {
			assert(entry->obj == (kern_obj_t) kp);
			assert(entry->kmsg == kmsg);

			/* don't need to have the task locked,
			   because the kmsg field is special. */
			entry->kmsg = KERN_MSG_NULL;
		}

		port_unlock(kp);

		kern_msg_destroy(kmsg);
	}

	/* Poke all sleeping senders. */

	while (!queue_empty(&kp->port_blocked_threads)) {
		queue_remove_first(&kp->port_blocked_threads, thread_to_wake, 
				   thread_t, ipc_wait_queue);
		thread_to_wake->ipc_state = SEND_SUCCESS;
		thread_go(thread_to_wake);
	}

#if	MACH_NP
	if (kp->port_external != (int **)0) {
		((void (*)())(kp->port_external[PORT_DEAD_OFF]))(kp);
	}
#endif	MACH_NP

	/* Eliminate send rights from all tasks. */
	port_lock(kp);
	obj_destroy_rights((kern_obj_t) kp);

#if	MACH_NET || MACH_XP
	port_unlock(kp);
	switch (kp->port_object.kp_type) {
#if	MACH_NET
		case PORT_OBJECT_NET:
			netipc_ignore(PORT_NULL, (port_t) kp);
			break;
#endif	MACH_NET
#if	MACH_XP
		case PORT_OBJECT_PAGER:
			vm_object_destroy((port_t) kp);
			break;
#endif	MACH_XP
	}
	port_lock(kp);
#endif	MACH_NET || MACH_XP

	/* Release the reference provided by the caller. */

	kp->port_references--;
	if (ipc_debug & IPC_DEBUG_PORT_REFS)
		if (kp->port_references != 0)
			printf("port_destroy: refs = %d, zone = %08x\n",
			       kp->port_references, kp->port_home_zone);
	port_check_unlock(kp);
}

/*
 *	Routine:	port_dealloc [exported, internal]
 *	Purpose:
 *		Delete port rights to "kp" from this "task".
 *	Conditions:
 *		No locks on entry or exit.  Assumes task is valid.
 *	Side effects:
 *		Port may be "destroyed" or rights moved.
 */
kern_return_t
port_dealloc(task, kp)
	register task_t task;
	register kern_port_t kp;
{
	register port_hash_t entry;
	port_type_t pt;
#if	MACH_IPC_XXXHACK
	task_t otask;		/* other task involved */
	port_name_t name;
#endif	MACH_IPC_XXXHACK

	assert(task != TASK_NULL);

	if (kp == PORT_NULL)
		return KERN_SUCCESS;

	ipc_task_lock(task);

	/* Look for the port right. */

	entry = obj_entry_find(task, (kern_obj_t) kp);
	if (entry == PORT_HASH_NULL) {
		ipc_task_unlock(task);
		return KERN_SUCCESS;
	}

	/* Found it... remove the translation. */

	pt = entry->type;
	port_lock(kp);
	obj_entry_destroy(task, (kern_obj_t) kp, entry);
	ipc_task_unlock(task);

	/* If we hold special rights, there's more to do.
	   At this point, the port is still locked and
	   our caller has his ref for the port. */

	switch (pt) {
	    case PORT_TYPE_SEND:
		assert(kp->port_receiver != task);
#if	MACH_IPC_XXXHACK
		assert(kp->port_owner != task);
#endif	MACH_IPC_XXXHACK

		/* Not receiver or owner; nothing to do. */

		port_unlock(kp);
		break;

#if	MACH_IPC_XXXHACK
	    case PORT_TYPE_RECEIVE:
		assert(kp->port_receiver == task);
		assert(kp->port_owner != task);

		otask = kp->port_owner;

		if (otask == TASK_NULL) {
			/* We are receiver and ownership is dead.
			   Destroy the port. */

			port_clear_receiver(kp, TASK_NULL);
			kp->port_references++; /* for port_destroy */
			port_destroy(kp);
		} else if (otask == ipc_soft_task) {
			/* We are receiver and ownership is in transit.
			   Mark receiver as dead; notification will get
			   sent when ownership is picked up. */

			port_clear_receiver(kp, TASK_NULL);
			port_unlock(kp);
		} else {
			/* We are receiver and there is a real owner.
			   Send him a notification. */

			port_clear_receiver(kp, ipc_soft_task);
			kp->port_references++; /* for object_copyout */
			port_unlock(kp);
			object_copyout(otask, (kern_obj_t) kp,
				       MSG_TYPE_PORT_RECEIVE, &name);
			send_notification(otask, NOTIFY_RECEIVE_RIGHTS, name);
		}
		break;

	    case PORT_TYPE_OWN:
		assert(kp->port_receiver != task);
		assert(kp->port_owner == task);

		otask = kp->port_receiver;

		if (otask == TASK_NULL) {
			/* We are owner and receive right is dead.
			   Destroy the port. */

			kp->port_owner = TASK_NULL;
			kp->port_references++; /* for port_destroy */
			port_destroy(kp);
		} else if (otask == ipc_soft_task) {
			/* We are owner and receive right is in transit.
			   Mark owner as dead; notification will get
			   sent when receive right is picked up. */

			kp->port_owner = TASK_NULL;
			port_unlock(kp);
		} else {
			/* We are owner and there is a real receiver.
			   Send him a notification. */

			kp->port_owner = ipc_soft_task;
			kp->port_references++; /* for object_copyout */
			port_unlock(kp);
			object_copyout(otask, (kern_obj_t) kp,
				       MSG_TYPE_PORT_OWNERSHIP, &name);
			send_notification(otask, NOTIFY_OWNERSHIP_RIGHTS, name);
		}
		break;
#endif	MACH_IPC_XXXHACK

	    case PORT_TYPE_RECEIVE_OWN:
		assert(kp->port_receiver == task);
#if	MACH_IPC_XXXHACK
		assert(kp->port_owner == task);
#endif	MACH_IPC_XXXHACK

		/* We are the receiver and owner.
		   Destroy the port. */

#if	MACH_IPC_XXXHACK
		kp->port_owner = TASK_NULL;
#endif	MACH_IPC_XXXHACK
		port_clear_receiver(kp, TASK_NULL);

		kp->port_references++; /* for port_destroy */
		port_destroy(kp);
		break;

	    default:
		panic("port_dealloc: strange translation type");
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	port_lookup [external]
 *	Purpose:
 *		Converts a task/name pair to a port.
 *		If successful, the caller has a ref for the object.
 *
 *		Returns KERN_PORT_NULL for failure.
 *	Conditions:
 *		Nothing locked.
 */
kern_port_t
port_lookup(task, name)
	register task_t task;
	port_t name;
{
	register kern_obj_t obj = KERN_OBJ_NULL;
	register port_hash_t entry;

	assert(task != TASK_NULL);

	ipc_task_lock(task);
	if (!task->ipc_active)
		goto exit;

	entry = obj_entry_lookup(task, name);
	if (entry == PORT_HASH_NULL)
		goto exit;

	if (!PORT_TYPE_IS_PORT(entry->type))
		goto exit;

	obj = entry->obj;
	obj_reference(obj);

    exit:
	ipc_task_unlock(task);
	return (kern_port_t) obj;
}

#if	DEBUG
/*
 *	Routine:	port_print
 */
void		port_print(kp)
	register
	kern_port_t	kp;
{
	extern int indent;

	printf("port 0x%x\n", kp);

	indent += 2;
	  ipc_obj_print(&kp->port_obj);
	  iprintf("receiver = 0x%x", kp->port_receiver);
	  printf(",receiver_name = 0x%x", kp->port_receiver_name);
#if	MACH_IPC_XXXHACK
	  printf(",owner = 0x%x\n", kp->port_owner);
#endif	MACH_IPC_XXXHACK
	  iprintf("set = 0x%x", kp->port_set);
	  printf(",backup = 0x%x", kp->port_backup);
	  printf(",backlog = 0x%x\n", kp->port_backlog);
	  iprintf("message count = %d, queue = %x\n", kp->port_message_count,
	  				&kp->port_messages);
	  iprintf("object = 0x%x (type %d)\n",
		kp->port_object.kp_object,
		kp->port_object.kp_type);
#if	MACH_NP
	  iprintf("external = 0x%x\n", kp->port_external);
#endif	MACH_NP
	  indent -=2;
}
#endif	DEBUG

