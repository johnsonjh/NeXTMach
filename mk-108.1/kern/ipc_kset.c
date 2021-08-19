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
 * $Log:	ipc_kset.c,v $
 * Revision 2.8  89/06/27  00:24:08  rpd
 * 	Added type argument to obj_alloc.
 * 	[89/06/26  23:52:11  rpd]
 * 
 * Revision 2.7  89/02/25  18:02:44  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.6  88/12/19  02:44:28  mwyoung
 * 	Removed lint.
 * 	[88/12/08            mwyoung]
 * 	
 * 	Fixed two bugs in set_add_member.  It didn't hand-off messages
 * 	to blocked receivers.  (Thanks Avie!)  It neglected to PSIGNAL
 * 	when not waking up a blocked receiver.
 * 	[88/11/26  21:30:23  rpd]
 * 
 * Revision 2.5  88/11/23  16:39:13  rpd
 * 	Changed mach_ipc_debug to ipc_debug.
 * 	[88/11/23  10:42:20  rpd]
 * 
 * Revision 2.4  88/10/01  21:55:39  rpd
 * 	Changed port_remove_member to use msg_local_port for a
 * 	message's destination port.
 * 	[88/10/01  21:25:53  rpd]
 * 
 * Revision 2.3  88/09/25  22:11:24  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:14:24  rpd]
 * 
 * Revision 2.2  88/08/06  18:16:40  rpd
 * Created.
 * 
 */
/*
 * File:	ipc_kset.c
 * Purpose:
 *	IPC port set functions.
 */

#import <sys/kern_return.h>
#import <kern/zalloc.h>
#import <kern/queue.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/msg_queue.h>
#import <kern/kern_set.h>
#import <kern/kern_port.h>
#import <kern/sched_prim_macros.h>
#import <kern/ipc_hash.h>
#import <kern/ipc_prims.h>
#import <kern/ipc_globals.h>
#import <kern/ipc_signal.h>

/*
 *	Routine:	set_reference [exported]
 *	Purpose:
 *		Acquire a reference to the set in question, preventing
 *		its destruction.  Also comes in macro form.
 */
void
set_reference(set)
	kern_set_t set;
{
	set_reference_macro(set);
}

/*
 *	Routine:	set_release [exported]
 *	Purpose:
 *		Release a set reference.
 *		Also comes in macro form.
 */
void
set_release(set)
	kern_set_t set;
{
	set_release_macro(set);
}

/*
 *	Routine:	set_alloc [internal]
 *	Purpose:
 *		Allocates and initializes a set data structure.  Allocates
 *		and initializes a translation record for the set.
 *	Conditions:
 *		Nothing locked.  Returns the set locked, with no ref
 *		for the caller.
 */
kern_return_t
set_alloc(task, setp)
	task_t task;
	kern_set_t *setp;
{
	kern_return_t kr;
	port_hash_t entry;

	/* probably should have a reserved zone for ipc_privilege tasks */

	kr = obj_alloc(task, set_zone, PORT_TYPE_SET, &entry);
	if (kr == KERN_SUCCESS) {
		register kern_set_t set = (kern_set_t) entry->obj;

		set->set_local_name = entry->local_name;
		set->set_owner = task;

		msg_queue_init(&set->set_messages);
		queue_init(&set->set_members);
		set->set_traversal = KERN_PORT_NULL;

		*setp = set;
	}

	return kr;
}

/*
 *	Routine:	set_destroy
 *	Purpose:
 *		Destroys the set.  Any ports in the set are removed.
 *		Removes the owner's translation for the set.
 *	Conditions:
 *		The task and the set are locked.  The caller shouldn't
 *		provide a ref for the set.  The task is unlocked.  The
 *		set is unlocked (& probably deallocated) upon return.
 */
void
set_destroy(task, set, entry)
	task_t task;
	kern_set_t set;
	port_hash_t entry;
{
	assert(entry->type == PORT_TYPE_SET);
	assert(entry->local_name == set->set_local_name);
	assert(set->set_owner == task);

	assert(set->set_in_use);
	set->set_in_use = FALSE;

	/* remove and deallocate the translation */

	obj_entry_remove(task, &set->set_obj, entry);
	obj_entry_dealloc(&set->set_obj, entry);
	assert(queue_empty(&set->set_translations));

	ipc_task_unlock(task);

	/* take a reference, so we can unlock the set safely */
	set->set_references++;

	/* remove all members of the set */

	while (!queue_empty(&set->set_members)) {
		kern_port_t port = (kern_port_t)
			queue_first(&set->set_members);

		/* need to get lock on port, but have set locked.  so, indicate
		   our desire with set_traversal */
		set->set_traversal = port;

		set_unlock(set);
		/* port is still valid here, because either set has a ref for
		   the port or set_remove_member gave us a ref */
		port_lock(port);
		set_lock(set);

		/* if set_remove_member took the port out of the set, it gave
		   us a ref for the port and cleared set_traversal */
		if (set->set_traversal == KERN_PORT_NULL) {
			port->port_references--;
			assert(port->port_set != set);
		} else {
			set->set_traversal = KERN_PORT_NULL;
			set_remove_member(set, port);
		}

		port_check_unlock(port);
	}

	msg_queue_changed(&set->set_messages, RCV_INVALID_PORT);

	set->set_references--;	/* throw away the ref we took above */
	if (ipc_debug & IPC_DEBUG_SET_REFS)
		if (set->set_references != 0)
			printf("set_destroy: refs = %d, zone = %#8x\n",
			       set->set_references, set->set_home_zone);
	set_check_unlock(set);
}

/*
 *	Routine:	set_add_member
 *	Purpose:
 *		Move a port into a port set.
 *	Conditions:
 *		Both the port and set are locked throughout.
 *		The port and set are both active.  The port isn't
 *		already in a set.  The owner of the port set is also
 *		receiver for the port.  The port and set both gain an
 *		additional reference.
 */
void
set_add_member(set, port)
	kern_set_t set;
	kern_port_t port;
{
	register queue_t old_queue, new_queue;
	register kern_msg_t kmsg;
	register queue_t bqueue;

	assert(port->port_in_use);
	assert(set->set_in_use);
	assert(port->port_set == KERN_SET_NULL);
	assert(port->port_receiver == set->set_owner);

	port->port_set = set;
	queue_enter(&set->set_members, port, kern_port_t, port_brothers);
	port->port_references++;
	set->set_references++;

	msg_queue_lock(&port->port_messages);
	msg_queue_lock(&set->set_messages);

	old_queue = &port->port_messages.messages;
	new_queue = &set->set_messages.messages;
	bqueue = &set->set_messages.blocked_threads;

	while ((queue_t) (kmsg = (kern_msg_t) old_queue->next) != old_queue) {
		register thread_t t;

		/* Fast dequeue of the kmsg relies on having
		   queue_head at the front of the kmsg. */

		(old_queue->next = kmsg->queue_head.next)->prev = old_queue;

		/* We are effectively doing a msg_queue to a port in a set,
		   so must PSIGNAL just as msg_queue does. */

		PSIGNAL(port->port_receiver,
			kmsg->kmsg_header.msg_type & MSG_TYPE_EMERGENCY);

		/* Now we want to add kmsg to new_queue.  However, first
		   we must check for possible blocked receivers. */

		while ((queue_t) (t = (thread_t) bqueue->next) != bqueue) {
			/* Dequeue and wake the sleeping receiver. */
			queue_remove(bqueue, t, thread_t, ipc_wait_queue);
			thread_go(t);

			/* Check if the receiver can handle our message. */
			if (kmsg->kmsg_header.msg_size <= t->ipc_data.msize) {
				t->ipc_state = RCV_SUCCESS;
				t->ipc_data.kmsg = kmsg;

				goto next_kmsg;
			}

			t->ipc_state = RCV_TOO_LARGE;
			t->ipc_data.msize = kmsg->kmsg_header.msg_size;
		}

		/* Couldn't find a receiver, just enqueue the message. */

		enqueue(new_queue, (queue_entry_t) kmsg);

	      next_kmsg:;
	}

	msg_queue_unlock(&set->set_messages);
	msg_queue_unlock(&port->port_messages);

	msg_queue_changed(&port->port_messages, RCV_PORT_CHANGE);
}

/*
 *	Routine:	set_remove_member
 *	Purpose:
 *		Removes a port from a port set.
 *	Conditions:
 *		The port is in the specified set.  Both the port and
 *		the set are locked throughout.  The set and the port
 *		don't have to be active.  They both lose a reference.
 */
void
set_remove_member(set, port)
	kern_set_t set;
	kern_port_t port;
{
	kern_msg_t next_msg;
	queue_t old_queue, new_queue;

	assert(set == port->port_set);

	port->port_set = KERN_SET_NULL;
	queue_remove(&set->set_members, port, kern_port_t, port_brothers);
	port->port_references--;
	set->set_references--;

	if (set->set_traversal == port) {
		/* this means set_destroy is trying to destroy the set, and
		   is trying to lock the port.  tell it what's happening and
		   give it a ref for the port. */
		set->set_traversal = KERN_PORT_NULL;
		port->port_references++;
	}

	msg_queue_lock(&port->port_messages);
	msg_queue_lock(&set->set_messages);

	old_queue = &set->set_messages.messages;
	new_queue = &port->port_messages.messages;

	next_msg = (kern_msg_t) queue_first(old_queue);
	while (!queue_end(old_queue, (queue_entry_t) next_msg)) {
		kern_msg_t this = next_msg;
		next_msg = (kern_msg_t) queue_next((queue_entry_t) next_msg);
		if (this->kmsg_header.msg_local_port == (port_t) port) {
			remqueue(old_queue, (queue_entry_t) this);
			enqueue(new_queue, (queue_entry_t) this);
		}
	}

	assert(queue_empty(&port->port_messages.blocked_threads));

	msg_queue_unlock(&set->set_messages);
	msg_queue_unlock(&port->port_messages);
}

