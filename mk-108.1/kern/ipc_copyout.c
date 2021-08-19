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
 * $Log:	ipc_copyout.c,v $
 * 18-May-90  Gregg Kellogg (gk) at NeXT
 *	Added object_copyout_to_kernel and modified msg_copyout to use it
 *	for copyouts to kern_ipc_space tasks ala msg_copyin.
 *
 * 20-Feb-90  Gregg Kellogg (gk) at NeXT
 *	msg_copyout is conditional on MACH_OLD_VM_COPY in using copy_maps
 *	or old style stuff.
 *
 * Revision 2.14  89/12/22  16:27:54  rpd
 * 	Print a warning message if vm_map_find fails.
 * 	[89/11/28  21:35:31  rpd]
 * 
 * Revision 2.13  89/10/10  10:53:49  mwyoung
 * 	Use vm_map_copyin instead of vm_move.
 * 	[89/08/01  14:46:54  mwyoung]
 * 
 * Revision 2.12  89/05/01  16:59:32  rpd
 * 	Updated for the new translation cache organization.
 * 	Removed object_copyout_cache.
 * 	[89/05/01  14:35:13  rpd]
 * 
 * Revision 2.11  89/03/05  16:46:16  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK.
 * 	[89/02/16  13:45:06  rpd]
 * 
 * Revision 2.10  89/02/25  18:01:53  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.9  89/01/12  07:54:47  rpd
 * 	Moved ipc_statistics.h to mach_debug/.
 * 	[89/01/12  04:44:30  rpd]
 * 
 * Revision 2.8  89/01/10  23:28:17  rpd
 * 	Removed port_copyout.
 * 	[89/01/09  14:45:18  rpd]
 * 
 * Revision 2.7  88/12/19  02:43:49  mwyoung
 * 	Remove lint.
 * 	[88/12/08            mwyoung]
 * 
 * Revision 2.6  88/10/11  10:13:24  rpd
 * 	Fixed bug in object_copyout_cache: entry type wasn't right
 * 	if the task already had more than send rights for the port.
 * 	Fixed bug in msg_copyout: didn't check that the task was still
 * 	the receiver for the dest port before using port_receiver_name.
 * 	[88/10/10  07:55:09  rpd]
 * 	
 * 	Removed user_buffer_size argument from msg_copyout.
 * 	[88/10/09  08:41:49  rpd]
 * 	
 * 	Slight cleanup of msg_copyout.
 * 	[88/10/07  15:46:39  rpd]
 * 
 * Revision 2.5  88/10/01  21:54:45  rpd
 * 	No longer swap msg_local_port & msg_remote_port in msg_copyout.
 * 	[88/10/01  21:24:01  rpd]
 * 
 * Revision 2.4  88/09/25  22:10:27  rpd
 * 	Added object_copyout_cache.  Revamped & specialized
 * 	msg_copyout and msg_destroy.  Updated for new translation cache.
 * 	[88/09/24  17:57:28  rpd]
 * 	
 * 	Renamed object_delete as object_destroy.
 * 	Added msg_destroy.  For the moment, it is a copy
 * 	of msg_copyout.
 * 	[88/09/21  00:44:43  rpd]
 * 	
 * 	Updated vm_move calls.
 * 	[88/09/20  16:10:44  rpd]
 * 
 * Revision 2.3  88/08/25  18:14:23  mwyoung
 * 	Corrected include file references.
 * 	[88/08/22            mwyoung]
 * 	
 * 	In msg_copyout, always advance past out-of-line data, even if it's
 * 	not being vm_moved.
 * 	[88/08/15  02:29:47  mwyoung]
 * 
 * Revision 2.2  88/08/06  18:13:51  rpd
 * Created.
 * 
 */
/*
 * File:	ipc_copyout.c
 * Purpose:
 *	msg_copyout and related functions.
 */

#import <mach_np.h>
#import <mach_ipc_xxxhack.h>

#import <sys/boolean.h>
#import <sys/types.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/kern_port.h>
#import <kern/kern_set.h>
#import <kern/kern_msg.h>
#import <kern/ipc_kmesg.h>
#import <kern/ipc_hash.h>
#import <kern/ipc_prims.h>
#import <kern/ipc_cache.h>
#import <kern/ipc_globals.h>
#import <kern/ipc_statistics.h>
#import <kern/ipc_copyout.h>

#if	MACH_IPC_XXXHACK
/*
 *	Routine:	port_destroy_receive [internal]
 *	Purpose:
 *		Destroy receive rights that were in transit.
 *
 *		Allow for possibility that the receive rights are
 *		no longer in the message.
 *	Conditions:
 *		The port is locked; we have a ref to release.
 */
void
port_destroy_receive(port)
	kern_port_t port;
{
	if (port->port_receiver == ipc_soft_task)
		port->port_receiver = TASK_NULL;

	if ((port->port_receiver == TASK_NULL) &&
	    (port->port_owner == TASK_NULL))
		port_destroy(port);
	else {
		port->port_references--;
		/* No need for port_check_unlock, because a receiver or
		   owner exists, so there is some other outstanding ref. */
		port_unlock(port);
	}
}

/*
 *	Routine:	port_destroy_own [internal]
 *	Purpose:
 *		Destroy ownership rights that were in transit.
 *
 *		Allow for possibility that the ownership rights are
 *		no longer in the message.
 *	Conditions:
 *		The port is locked; we have a ref to release.
 */
void
port_destroy_own(port)
	kern_port_t port;
{
	if (port->port_owner == ipc_soft_task)
		port->port_owner = TASK_NULL;

	if ((port->port_receiver == TASK_NULL) &&
	    (port->port_owner == TASK_NULL))
		port_destroy(port);
	else {
		port->port_references--;
		/* No need for port_check_unlock, because a receiver or
		   owner exists, so there is some other outstanding ref. */
		port_unlock(port);
	}
}
#endif	MACH_IPC_XXXHACK

/*
 *	Routine:	port_destroy_receive_own [internal]
 *	Purpose:
 *		Destroy receive/ownership rights that were in transit.
 *
 *		Allow for possibility that the receive/ownership rights are
 *		no longer in the message.
 *	Conditions:
 *		The port is locked; we have a ref to release.
 */
void
port_destroy_receive_own(port)
	kern_port_t port;
{
	if (port->port_receiver == ipc_soft_task)
		port->port_receiver = TASK_NULL;
#if	MACH_IPC_XXXHACK
	if (port->port_owner == ipc_soft_task)
		port->port_owner = TASK_NULL;
#endif	MACH_IPC_XXXHACK

#if	MACH_IPC_XXXHACK
	if ((port->port_receiver == TASK_NULL) &&
	    (port->port_owner == TASK_NULL))
#else	MACH_IPC_XXXHACK
	if (port->port_receiver == TASK_NULL)
#endif	MACH_IPC_XXXHACK
		port_destroy(port);
	else {
		port->port_references--;
		/* No need for port_check_unlock, because a receiver or
		   owner exists, so there is some other outstanding ref. */
		port_unlock(port);
	}
}

#if	MACH_IPC_XXXHACK
/*
 *	Routine:	port_copyout_receive [internal]
 *	Purpose:
 *		Transfer receive rights from a message in transit
 *		to the task in question.
 *	Conditions:
 *		The port is locked throughout.
 *		[Except in MACH_NP, which is buggy.]
 */
int
port_copyout_receive(task, port, name)
	task_t task;
	kern_port_t port;
	port_name_t name;
{
	assert(port->port_receiver == ipc_soft_task);
	port->port_receiver = task;
	port->port_receiver_name = name;

#if	MACH_NP
	port_unlock(port);
	if ((port->port_external != (int **)0)) {
		((void (*)())(port->port_external[PORT_CHANGED_OFF]))(port);
	}		
	port_lock(port);
#endif	MACH_NP

	if (port->port_owner == TASK_NULL) {
		/* if ownership rights were dead, this task gets them too,
		   and a notification gets sent. */
		port->port_owner = task;
		return NOTIFY_OWNERSHIP_RIGHTS;
	}

	return 0;
}

/*
 *	Routine:	port_copyout_own [internal]
 *	Purpose:
 *		Transfer ownership rights from a message in transit
 *		to the task in question.
 *	Conditions:
 *		The port is locked throughout.
 */
int
port_copyout_own(task, port, name)
	task_t task;
	kern_port_t port;
	port_name_t name;
{
#ifdef	lint
	name++;
#endif	lint

	assert(port->port_owner == ipc_soft_task);
	port->port_owner = task;

	if (port->port_receiver == TASK_NULL) {
		/* if receive rights were dead, this task gets them too,
		   and a notification gets sent. */
		port->port_receiver = task;
		return NOTIFY_RECEIVE_RIGHTS;
	}

	return 0;
}
#endif	MACH_IPC_XXXHACK

/*
 *	Routine:	port_copyout_receive_own [internal]
 *	Purpose:
 *		Transfer receive/ownership rights from a message in transit
 *		to the task in question.
 *	Conditions:
 *		The port is locked throughout.
 *		[Except in MACH_NP, which is buggy.]
 */
int
port_copyout_receive_own(task, port, name)
	task_t task;
	kern_port_t port;
	port_name_t name;
{
	assert(port->port_receiver == ipc_soft_task);
	port->port_receiver = task;
	port->port_receiver_name = name;

#if	MACH_IPC_XXXHACK
	assert(port->port_owner == ipc_soft_task);
	port->port_owner = task;
#endif	MACH_IPC_XXXHACK

#if	MACH_NP
	port_unlock(port);
	if ((port->port_external != (int **)0)) {
		((void (*)())(port->port_external[PORT_CHANGED_OFF]))(port);
	}		
	port_lock(port);
#endif	MACH_NP

	return 0;
}

/*
 *	Routine:	object_destroy [internal]
 *	Purpose:
 *		Delete the rights denoted by "rights" for the object "obj"
 *		from a message in transit.
 *	Conditions:
 *		No locks held on entry or exit.
 *		Releases an object reference (in the message); may destroy
 *		the object.
 */
void
object_destroy(obj, rights)
	kern_obj_t obj;
	unsigned int rights;
{
	register object_copyout_table_t *table;

	assert(obj != KERN_OBJ_NULL);

	ipc_event(port_copyouts);

	assert(rights < MSG_TYPE_LAST);
	table = &object_copyout_table[rights];

	obj_lock(obj);
	if (table->destroy && obj->obj_in_use)
		/* consumes ref & unlocks obj */
		(*table->destroy)(obj);
	else {
		obj->obj_references--;
		obj_check_unlock(obj);
	}
}

/*
 *	Routine:	object_copyout [internal]
 *	Purpose:
 *		Transfer the rights denoted by "rights" for the object "obj"
 *		from a message in transit to the task in question.
 *		Return the task's local name in "*namep".
 *	Conditions:
 *		No locks held on entry or exit.
 *		Releases an object reference (in the message); may
 *		add a reference for a new right.
 */
void
object_copyout(task, obj, rights, namep)
	task_t task;
	kern_obj_t obj;
	unsigned int rights;
	port_name_t *namep;
{
	register object_copyout_table_t *table;
	port_hash_t entry;
	port_name_t name;
	port_type_t type;
	int msg_id;

	assert(task != TASK_NULL);
	assert(obj != KERN_OBJ_NULL);
	assert(! task->kernel_ipc_space);

	ipc_task_lock(task);
	obj_lock(obj);

	assert(rights < MSG_TYPE_LAST);
	table = &object_copyout_table[rights];

	entry = obj_entry_make(task, obj, table->nomerge);
	if (entry == PORT_HASH_NULL) {
		/* this is a rare case, so don't mind unlocking and
		   letting object_destroy do all the work.  it will
		   consume our ref. */

		obj_unlock(obj);
		ipc_task_unlock(task);
		object_destroy(obj, rights);
		*namep = PORT_NULL;
		return;
	}

	type = entry->type;
	*namep = name = entry->local_name;
	msg_id = 0;

	/* need to have both task & object locked while
	   manipulating the entry. */

	assert((0 <= type) && (type < PORT_TYPE_LAST));
	entry->type = table->result[type];
	assert(entry->type != PORT_TYPE_NONE);

	/* Would drop task lock here, except for hack below. */

	if (table->func) {
		msg_id = (*table->func)(task, obj, name);

		/* Hack to take care of way notification
		   generation should change the entry's type. */
		if (msg_id != 0)
			entry->type = PORT_TYPE_RECEIVE_OWN;
	}

	/* There must be some other reference, for a translation,
	   so no need for obj_check_unlock. */
	obj->obj_references--;
	obj_unlock(obj);

	ipc_task_unlock(task);

	if (msg_id != 0)
		send_notification(task, msg_id, name);

	ipc_event(port_copyouts);
}

/*
 *	Routine:	object_copyout_to_kernel [internal]
 *	Purpose:
 *		Hack to allow a kernel_ipc_space task to receive rights
 *		in a message by using a pointer to the port,
 *		instead of a local name.
 *		Return the task's local name in "*namep".
 *	Conditions:
 *		No locks held on entry or exit.
 *		Releases an object reference (in the message); may
 *		add a reference for a new right.
 */
void
object_copyout_to_kernel(task, obj, rights, namep)
	task_t task;
	kern_obj_t obj;
	unsigned int rights;
	port_name_t *namep;
{
	register object_copyout_table_t *table;
	port_hash_t entry;
	port_name_t name;
	port_type_t type;
	int msg_id;

	assert(task != TASK_NULL);
	assert(obj != KERN_OBJ_NULL);
	assert(task->kernel_ipc_space);

	ipc_task_lock(task);
	obj_lock(obj);

	assert(rights < MSG_TYPE_LAST);
	table = &object_copyout_table[rights];

	/*
	 * We should already have an entry for this object, find it.
	 * (Should task really be kernel_task, or can we assert that
	 * task == kernel_task, i.e.: kernel_ipc_space ==> task == kernel_task)
	 */
	entry = obj_entry_find(task, obj);
	if (entry == PORT_HASH_NULL) {
		/* this is a rare case, so don't mind unlocking and
		   letting object_destroy do all the work.  it will
		   consume our ref. */

		obj_unlock(obj);
		ipc_task_unlock(task);
		object_destroy(obj, rights);
		*namep = PORT_NULL;
		return;
	}

	type = entry->type;
	*namep = (port_name_t)obj;
	name = entry->local_name;
	msg_id = 0;

	/* need to have both task & object locked while
	   manipulating the entry. */

	assert((0 <= type) && (type < PORT_TYPE_LAST));
	entry->type = table->result[type];
	assert(entry->type != PORT_TYPE_NONE);

	/* Would drop task lock here, except for hack below. */

	if (table->func) {
		msg_id = (*table->func)(task, obj, name);

		/* Hack to take care of way notification
		   generation should change the entry's type. */
		if (msg_id != 0)
			entry->type = PORT_TYPE_RECEIVE_OWN;
	}

	/* There must be some other reference, for a translation,
	   so no need for obj_check_unlock. */
	/*
	 * Keep the reference we already have to this object.  We got
	 * a reference when this object was copied in to the kernel
	 * in msg_send.
	 */
	obj_unlock(obj);

	ipc_task_unlock(task);

	if (msg_id != 0)
		send_notification(task, msg_id, name);

	ipc_event(port_copyouts);
}

/*
 *	Routine:	msg_copyout
 *	Purpose:
 *		Transfer the specified kernel message to user space.
 *		Includes transferring port rights and out-of-line memory
 *		present in the message.
 *	Conditions:
 *		No locks held on entry or exit.
 *		The kernel message is destroyed.
 *	Returns:
 *		Values returned correspond to those for msg_receive.
 */		
msg_return_t
msg_copyout(task, msgptr, kmsg)
	register task_t task;
	register msg_header_t *msgptr;
	register kern_msg_t kmsg;
{
	register kern_port_t dest_port, reply_port;
	register port_name_t name;

	assert(task != TASK_NULL);

	/*
	 *	Translate ports in the header.
	 */

	dest_port = (kern_port_t) kmsg->kmsg_header.msg_local_port;
	reply_port = (kern_port_t) kmsg->kmsg_header.msg_remote_port;

	assert(dest_port != KERN_PORT_NULL);
	port_lock(dest_port);
	if (dest_port->port_receiver == task) {
		assert(dest_port->port_in_use);

		name = dest_port->port_receiver_name;
	} else {
		/* This is a very rare case: just after one thread dequeues
		   a message, another thread gives away or destroys the
		   receive right.  Give the bozo PORT_NULL. */
		name = PORT_NULL;
	}
	dest_port->port_references--;
	port_check_unlock(dest_port);
	kmsg->kmsg_header.msg_local_port = name;

	if (reply_port == KERN_PORT_NULL)
		name = PORT_NULL;
	else {
		register port_hash_t entry;

		/* Do an object_copyout of MSG_TYPE_PORT/reply_port inline. */

		ipc_task_lock(task);
		port_lock(reply_port);

		entry = obj_entry_make(task, (kern_obj_t) reply_port, FALSE);
		if (entry == PORT_HASH_NULL)
			name = PORT_NULL;
		else {
			if (entry->type == PORT_TYPE_NONE)
				entry->type = PORT_TYPE_SEND;
			name = entry->local_name;
			obj_cache_set(task, name, (kern_obj_t) reply_port);
		}

		reply_port->port_references--;
		port_check_unlock(reply_port);
		ipc_task_unlock(task);
	}
	kmsg->kmsg_header.msg_remote_port = name;

	/* If non-simple, translate port rights and memory to receiver. */

	if (!kmsg->kmsg_header.msg_simple) {
		register caddr_t saddr;
		register caddr_t endaddr;
		void (*copyoutf)();

		if (task->kernel_ipc_space)
			copyoutf = object_copyout_to_kernel;
		else
			copyoutf = object_copyout;

		saddr = (caddr_t)(&kmsg->kmsg_header + 1);
		endaddr = (((caddr_t) &kmsg->kmsg_header) + 
			   kmsg->kmsg_header.msg_size);

		while (saddr < endaddr) {
			register msg_type_long_t *tp =
				(msg_type_long_t *) saddr;
			unsigned int tn;
			unsigned int ts;
			vm_size_t numbytes;
			boolean_t is_port;
			long elts;

			if (tp->msg_type_header.msg_type_longform) {
				elts = tp->msg_type_long_number;
				tn = tp->msg_type_long_name;
				ts = tp->msg_type_long_size;
				saddr += sizeof(msg_type_long_t);
			} else {
				tn = tp->msg_type_header.msg_type_name;
				ts = tp->msg_type_header.msg_type_size;
				elts = tp->msg_type_header.msg_type_number;
				saddr += sizeof(msg_type_t);
			}
			numbytes = ((elts * ts) + 7) >> 3;

			/* Translate ports */

			if (is_port = MSG_TYPE_PORT_ANY(tn)) {
				register port_name_t *obj_list;

				if (tp->msg_type_header.msg_type_inline)
					obj_list = (port_name_t *) saddr;
				else
					obj_list = * ((port_name_t **) saddr);

				while (--elts >= 0) {
					register kern_obj_t obj;

					obj = (kern_obj_t) *obj_list;
					assert(obj != KERN_OBJ_INVALID);

					if (obj != KERN_OBJ_NULL)
						(*copyoutf)(task, obj, tn,
							       obj_list);
					obj_list++;
				}
			}

			/*
			 *	Move data, if necessary;
			 *	advance to the next data item.
			 */

			if (tn == MSG_TYPE_INTERNAL_MEMORY) {
				vm_object_t object = * (vm_object_t *) saddr;

				* (vm_offset_t *) saddr = 0;
				if (vm_map_find(task->map, object, 0,
						(vm_offset_t *) saddr,
						numbytes,
						TRUE) != KERN_SUCCESS)
					printf("msg_copyout: cannot copy out data\n");

				assert(tp->msg_type_header.msg_type_longform);
				tp->msg_type_long_name = MSG_TYPE_INTEGER_8;

				assert(! tp->msg_type_header.msg_type_inline);
				saddr += sizeof(caddr_t);
			} else if (tp->msg_type_header.msg_type_inline) {
				saddr += ( (numbytes + 3) & (~0x3) );
			} else {
				if (! fast_pager_data(kmsg)) {
#if	MACH_OLD_VM_COPY
					register vm_map_t map;

					if (is_port)
						map = ipc_kernel_map;
					else
						map = ipc_soft_map;

					(void) vm_move(map,
						       * (vm_offset_t *) saddr,
						       task->map,
						       (vm_size_t) numbytes,
						       TRUE,
						       (vm_offset_t *) saddr);
#else	MACH_OLD_VM_COPY
					if (is_port) {
						vm_map_copy_t	new_addr;

						(void) /*XXX*/
						vm_map_copyin(ipc_kernel_map,
							*(vm_offset_t *)saddr,
							numbytes,
							TRUE,
							&new_addr);
						*(vm_offset_t *)saddr = (vm_offset_t) new_addr;
					}

					if (vm_map_copyout(
						task->map,
						(vm_offset_t *) saddr,
						*(vm_map_copy_t *) saddr)
					    != KERN_SUCCESS) {
						printf("msg_copyout: cannot copy out data\n");
						* (vm_offset_t *) saddr = 0;
					}
#endif	MACH_OLD_VM_COPY
				}

				saddr += sizeof (caddr_t);
			}
		}
	}

	/* Copy out the message header and body, all at once. */

	if (current_thread()->ipc_kernel)
		(void) bcopy((caddr_t) &kmsg->kmsg_header, 
			     (caddr_t) msgptr, 
			      kmsg->kmsg_header.msg_size);
	else if (copyout((caddr_t) &kmsg->kmsg_header, 
			  (caddr_t) msgptr, 
			   kmsg->kmsg_header.msg_size)) {
		kern_msg_free(kmsg);
		return RCV_INVALID_MEMORY;
	}

	kern_msg_free(kmsg);
	return RCV_SUCCESS;
}

/*
 *	Routine:	msg_destroy
 *	Purpose:
 *		Cleans up the specified kernel message, destroying
 *		port rights and deallocating out-of-line memory.
 *		The kernel message is freed.
 *	Conditions:
 *		No locks held on entry or exit.
 */		
void
msg_destroy(kmsg)
	register kern_msg_t kmsg;
{
	register kern_port_t port;

	/* Release the port references in the header. */

	port = (kern_port_t) kmsg->kmsg_header.msg_remote_port;
	if (port != KERN_PORT_NULL)
		port_release_macro(port);

	port = (kern_port_t) kmsg->kmsg_header.msg_local_port;
	if (port != KERN_PORT_NULL)
		port_release_macro(port);

	/* If non-simple, destroy port rights and memory. */

	if (!kmsg->kmsg_header.msg_simple) {
		register caddr_t saddr;
		register caddr_t endaddr;

		saddr = (caddr_t)(&kmsg->kmsg_header + 1);
		endaddr = (((caddr_t) &kmsg->kmsg_header) + 
			   kmsg->kmsg_header.msg_size);

		while (saddr < endaddr) {
			register msg_type_long_t *tp =
				(msg_type_long_t *) saddr;
			long elts;
			unsigned int tn;
			unsigned int ts;
			vm_size_t numbytes;
			boolean_t is_port;

			if (tp->msg_type_header.msg_type_longform) {
				elts = tp->msg_type_long_number;
				tn = tp->msg_type_long_name;
				ts = tp->msg_type_long_size;
				saddr += sizeof(msg_type_long_t);
			} else {
				tn = tp->msg_type_header.msg_type_name;
				ts = tp->msg_type_header.msg_type_size;
				elts = tp->msg_type_header.msg_type_number;
				saddr += sizeof(msg_type_t);
			}
			numbytes = ((elts * ts) + 7) >> 3;

			/* Destroy port rights */

			if (is_port = MSG_TYPE_PORT_ANY(tn)) {
				register kern_obj_t *obj_list;

				if (tp->msg_type_header.msg_type_inline)
					obj_list = (kern_obj_t *) saddr;
				else
					obj_list = * ((kern_obj_t **) saddr);

				while (--elts >= 0) {
					register kern_obj_t obj;

					obj = *obj_list++;
					if (obj == KERN_OBJ_INVALID)
						goto done;

					if (obj != KERN_OBJ_NULL)
						object_destroy(obj, tn);
				}
			}

			/*
			 *	Deallocate data, if necessary;
			 *	advance to the next data item.
			 */

			if (tn == MSG_TYPE_INTERNAL_MEMORY) {
				assert(tp->msg_type_header.msg_type_longform);

				vm_object_deallocate(* (vm_object_t *) saddr);

				assert(! tp->msg_type_header.msg_type_inline);
				saddr += sizeof(caddr_t);
			} else if (tp->msg_type_header.msg_type_inline) {
				saddr += ( (numbytes + 3) & (~0x3) );
			} else {
				if (! fast_pager_data(kmsg)) {
#if	MACH_OLD_VM_COPY
					register vm_map_t map;

					if (is_port)
						map = ipc_kernel_map;
					else
						map = ipc_soft_map;

					(void) vm_deallocate(
						map, * (vm_offset_t *) saddr,
						(vm_size_t) numbytes);
#else	MACH_OLD_VM_COPY
					if (is_port) {
						(void) vm_deallocate(
							ipc_kernel_map,
							* (vm_offset_t *) saddr,
							numbytes);
					} else
						vm_map_copy_discard(*(vm_map_copy_t *) saddr);
#endif	MACH_OLD_VM_COPY
				}

				saddr += sizeof (caddr_t);
			}
		}
	}

      done:
	kern_msg_free(kmsg);
}


