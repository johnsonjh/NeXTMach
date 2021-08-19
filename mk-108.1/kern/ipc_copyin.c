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
 * $Log:	ipc_copyin.c,v $
 * Revision 2.17  90/07/03  16:37:10  mrt
 * 	Replace move_msg_data macro with check for internal memory type.
 * 	This results from the purge of fast_pager_data functionality.
 * 	[90/06/27            dlb]
 * 	Add discard argument (TRUE) to vm_map_copyout.
 * 	[90/06/21            dlb]
 * 
 * Revision 2.16  90/03/30  00:19:50  rpd
 * 	Fixed vm_map_copyin call to correctly handle zero-sized OOL regions.
 * 	Changed in concert with vm_map_copy fixes.
 * 	[90/03/28            rpd]
 * 
 * 20-Feb-90  Gregg Kellogg (gk) at NeXT
 *	msg_copyin is conditional on MACH_OLD_VM_COPY in using copy_maps
 *	or old style stuff.
 *
 * Revision 2.15  89/10/10  10:53:31  mwyoung
 * 	Use vm_map_copyin rather than vm_move.  Memory for ports
 * 	should just be copied in directly to newly-allocated,
 * 	avoiding the copy-on-write technology entirely, but
 * 	I haven't made that change.
 * 	[89/06/08  18:21:21  mwyoung]
 * 
 * Revision 2.14  89/06/27  00:23:37  rpd
 * 	Fixed msg_copyin to check that ports' msg_type_size is 32.
 * 	[89/06/26  23:48:48  rpd]
 * 
 * Revision 2.13  89/05/01  16:59:17  rpd
 * 	Updated for new translation cache organization.
 * 	[89/05/01  14:31:34  rpd]
 * 
 * Revision 2.12  89/03/05  16:45:45  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK.
 * 	[89/02/16  13:43:47  rpd]
 * 
 * Revision 2.11  89/02/25  18:01:39  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.10  89/01/12  07:54:24  rpd
 * 	Moved ipc_statistics.h to mach_debug/.
 * 	[89/01/12  04:43:50  rpd]
 * 
 * Revision 2.9  89/01/10  23:27:37  rpd
 * 	Changed MACH_IPCSTATS to MACH_IPC_STATS.
 * 	[89/01/10  22:58:25  rpd]
 * 	
 * 	Removed port_copyin, port_copyin_fast.
 * 	[89/01/09  14:43:43  rpd]
 * 
 * Revision 2.8  88/12/20  13:51:22  rpd
 * 	MACH_NP: Picked up assertion-placating fix from Avie.
 * 	[88/11/26  21:22:04  rpd]
 * 
 * Revision 2.7  88/12/19  02:43:33  mwyoung
 * 	Remove lint.
 * 	[88/12/08            mwyoung]
 * 	
 * 	Determine whether the kernel is the receiver very early in
 * 	msg_copyin().
 * 	[88/12/02            mwyoung]
 * 
 * Revision 2.6  88/11/23  17:12:40  rpd
 * 	Changed mach_ipc_debug to ipc_debug.
 * 	[88/11/23  10:38:17  rpd]
 * 
 * Revision 2.5  88/10/11  10:12:43  rpd
 * 	Changed msg_copyin to fail is the destination port is null.
 * 	Changed msg_copyin to keep statistics for emergency msgs.
 * 	[88/10/09  16:03:12  rpd]
 * 	
 * 	Updated for new obj_entry_lookup_macro.
 * 	[88/10/09  08:39:17  rpd]
 * 	
 * 	Additional size argument to msg_copyin.  No longer generates
 * 	the SEND_MSG_SIZE_CHANGE error.
 * 	[88/10/07  15:45:42  rpd]
 * 	
 * 	Changed "struct KMsg" to "struct kern_msg".  Updated msg_copyin
 * 	for the new SEND_NOTIFY implementation.
 * 	[88/10/04  07:00:37  rpd]
 * 
 * Revision 2.4  88/10/01  21:54:13  rpd
 * 	Swap msg_local_port & msg_remote_port in msg_copyin.
 * 	[88/10/01  21:23:04  rpd]
 * 
 * Revision 2.3  88/09/25  22:09:59  rpd
 * 	Removed msg_copyin_from_kernel.  Added object_copyin_cache.
 * 	Updated for new translation cache.  Fixed MSG_TYPE_INVALID bug
 * 	by eliminating MSG_TYPE_INVALID.  Changed object_copyin_from_kernel
 * 	so it is called exactly like object_copyin.
 * 	[88/09/24  17:53:03  rpd]
 * 	
 * 	Added msg_copyin_from_kernel, by duplicating msg_copyin's
 * 	implementation.
 * 	[88/09/21  00:42:10  rpd]
 * 	
 * 	Fixed vm_move bug.
 * 	[88/09/20  16:09:19  rpd]
 * 	
 * 	Changed includes to the new style.
 * 	[88/09/19  16:10:11  rpd]
 * 
 * Revision 2.2  88/08/06  18:12:51  rpd
 * Created.
 * 
 */
/*
 * File:	ipc_copyin.c
 * Purpose:
 *	msg_copyin and related functions.
 */

#import <mach_np.h>
#import <mach_ipc_stats.h>
#import <mach_ipc_xxxhack.h>
#import <mach_old_vm_copy.h>

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
#import <kern/ipc_copyin.h>
#import <vm/vm_param.h>

/*
 *	Routine:	port_clear_receiver [internal]
 *	Purpose:
 *		Move receive rights out of a task.  The new receiver specifies
 *			ipc_soft_task:	Right is in a queued message.
 *			TASK_NULL:	Right is dead; ownership is
 *					 in a queued message.
 *	Conditions:
 *		The port must be locked.
 *	Side effects:
 *		The port is moved out of any port set it might be in.
 *		Threads waiting to receive on the port in question are
 *		awakened.
 */
void
port_clear_receiver(port, task)
	kern_port_t port;
	task_t task;
{
	kern_set_t set;

	assert((task == ipc_soft_task) || (task == TASK_NULL));

	set = port->port_set;
	if (set != KERN_SET_NULL) {
		/* No threads receiving on port, but must remove from set. */

		set_lock(set);
		set_remove_member(set, port);
		set_check_unlock(set);
	} else {
		/* Else, wake up all receivers, indicating why. */

		msg_queue_changed(&port->port_messages, RCV_INVALID_PORT);
	}

	port->port_receiver = task;
	port->port_receiver_name = PORT_NULL;

#if	MACH_NP
	/* Probably buggy. */

	port->port_references++;		/* placate assertions */
	port_unlock(port);
	if ((port->port_external != (int **)0)) {
		((void (*)())(port->port_external[PORT_CHANGED_OFF]))(port);
	}		
	port_lock(port);
	port->port_references--;
#endif	MACH_NP
}

#if	MACH_IPC_XXXHACK
/*
 *	Routine:	port_copyin_receive [internal]
 *	Purpose:
 *		Copy-in receive rights for the port.
 *	Conditions:
 *		Task and port locked throughout.
 */
void
port_copyin_receive(task, port)
	task_t task;
	kern_port_t port;
{
	assert(port->port_receiver == task);

	port_clear_receiver(port, ipc_soft_task);
}

/*
 *	Routine:	port_copyin_own [internal]
 *	Purpose:
 *		Copy-in ownership rights for the port.
 *	Conditions:
 *		Task and port locked throughout.
 */
void
port_copyin_own(task, port)
	task_t task;
	kern_port_t port;
{
	assert(port->port_owner == task);

	port->port_owner = ipc_soft_task;
}
#endif	MACH_IPC_XXXHACK

/*
 *	Routine:	port_copyin_receive_own [internal]
 *	Purpose:
 *		Copy-in receive/ownership rights for the port.
 *	Conditions:
 *		Task and port locked throughout.
 */
void
port_copyin_receive_own(task, port)
	task_t task;
	kern_port_t port;
{
#if	MACH_IPC_XXXHACK
	assert(port->port_owner == task);
	port->port_owner = ipc_soft_task;
#endif	MACH_IPC_XXXHACK

	assert(port->port_receiver == task);
	port_clear_receiver(port, ipc_soft_task);
}

/*
 *	Routine:	object_copyin
 *	Purpose:
 *		Copy the given "rights" held by "task" under local "name"
 *		for a message in transit.  If "deallocate" is specified,
 *		all rights to this object are removed from the "task" after
 *		the copy is made.
 *
 *		References to the object in question are made for the message
 *		in transit.  The value returned is the global name of the
 *		object.
 *
 *		Returns FALSE for illegal operations.
 *
 *	Conditions:
 *		No locks held on entry or exit.
 */
boolean_t
object_copyin(task, name, rights, deallocate, objp)
	task_t task;
	port_name_t name;
	unsigned int rights;
	boolean_t deallocate;
	kern_obj_t *objp;
{
	register kern_obj_t obj;
	register object_copyin_table_t *table;
	register port_hash_t entry;
	port_type_t type;

	assert(task != TASK_NULL);
	assert(! task->kernel_ipc_space);

	/* note name might be PORT_NULL, in which case we'll return FALSE */

	ipc_event(port_copyins);

	ipc_task_lock(task);
	if (!task->ipc_active) {
		ipc_task_unlock(task);
		return FALSE;
	}

	entry = obj_entry_lookup(task, name);
	if (entry == PORT_HASH_NULL) {
		ipc_task_unlock(task);
		return FALSE;
	}

	type = entry->type;
	assert(rights < MSG_TYPE_LAST);
	assert((0 <= type) && (type < PORT_TYPE_LAST));
	table = &object_copyin_table[rights][type];

	if (table->illegal || (deallocate && table->nodealloc)) {
		ipc_task_unlock(task);
		return FALSE;
	}

	obj = entry->obj;
	obj_lock(obj);

	if (!obj->obj_in_use) {
		obj_unlock(obj);
		ipc_task_unlock(task);
		return FALSE;
	}

	if (table->func)
		(*table->func)(task, obj);
	entry->type = table->result;

	/* if deallocation is desired or forced, remove the task's right */

	if (deallocate || table->dodealloc)
		obj_entry_destroy(task, obj, entry);

	ipc_task_unlock(task);

	/* take a reference for our caller */
	obj->obj_references++;
	obj_unlock(obj);

	*objp = obj;
	return TRUE;
}

/*
 *	Routine:	object_copyin_from_kernel [internal]
 *	Purpose:
 *		Hack to allow a kernel_ipc_space task to send rights
 *		in a message by using a pointer to the port,
 *		instead of a local name.
 *
 *		Returns FALSE for illegal operations.
 *	Conditions:
 *		Nothing locked.
 */
boolean_t
object_copyin_from_kernel(task, name, rights, deallocate, objp)
	register task_t task;
	port_name_t name;
	unsigned int rights;
	boolean_t deallocate;
	kern_obj_t *objp;
{
	register kern_obj_t obj = (kern_obj_t) name;
	register object_copyin_table_t *table;
	register port_hash_t entry;
	port_type_t type;

	assert(task != TASK_NULL);
	assert(obj != KERN_OBJ_NULL);
	assert(task->kernel_ipc_space);

	ipc_event(port_copyins);

	/* special case for MSG_TYPE_PORT: the task doesn't
	   even have to have rights for the port, if it doesn't
	   want to deallocate the rights. */

	if ((rights == MSG_TYPE_PORT) && !deallocate) {
		obj_reference(obj);
		return TRUE;
	}

	/* otherwise, the task actually has to have the
	   rights it is trying to send.  this code is like
	   object_copyin, except for the entry lookup. */

	ipc_task_lock(task);
	assert(task->ipc_active);

	entry = obj_entry_find(task, obj);
	if (entry == PORT_HASH_NULL) {
		ipc_task_unlock(task);
		return FALSE;
	}

	type = entry->type;
	assert(rights < MSG_TYPE_LAST);
	assert((0 <= type) && (type < PORT_TYPE_LAST));
	table = &object_copyin_table[rights][type];

	if (table->illegal || (deallocate && table->nodealloc)) {
		ipc_task_unlock(task);
		return FALSE;
	}

	assert(obj == entry->obj);
	obj_lock(obj);

	if (!obj->obj_in_use) {
		obj_unlock(obj);
		ipc_task_unlock(task);
		return FALSE;
	}

	if (table->func)
		(*table->func)(task, obj);
	entry->type = table->result;

	/* if deallocation is desired or forced, remove the task's right */

	if (deallocate || table->dodealloc)
		obj_entry_destroy(task, obj, entry);
	else
		assert(entry->type == PORT_TYPE_RECEIVE_OWN);

	ipc_task_unlock(task);

	/* take a reference for the message */
	obj->obj_references++;
	obj_unlock(obj);

	*objp = obj;
	return TRUE;
}

/*
 *	Routine:	object_copyin_cache [internal]
 *	Purpose:
 *		Copy given rights from a message header into the kernel.
 *		A "kernel_ipc_space" task is allowed to copy-in any port.
 *		If successful, the caller gets a ref for "*objp".
 *		Tries to cache the mapping.
 *	Conditions:
 *		The task is locked.
 */
boolean_t
object_copyin_cache(task, name, objp)
	task_t task;
	port_name_t name;
	kern_obj_t *objp;
{
	register port_hash_t entry;
	register kern_obj_t obj;

	assert(task != TASK_NULL);
	assert(name != PORT_NULL);

	ipc_event(port_copyins);

	if (!task->ipc_active)
		return FALSE;

	if (task->kernel_ipc_space) {
		obj = (kern_obj_t) name;
		obj_reference(obj);

		*objp = obj;
		return TRUE;
	}

	obj_entry_lookup_macro(task, name, entry, return FALSE;);

	if (!PORT_TYPE_IS_PORT(entry->type))
		return FALSE;

	obj = entry->obj;
	obj_reference(obj);
	obj_cache_set(task, name, obj);

	*objp = obj;
	return TRUE;
}

/*
 *	Routine:	msg_copyin
 *	Purpose:
 *		Copy a user message into a kernel message structure,
 *		returning that new kernel message in "var_kmsgptr".
 *
 *		Port rights are transferred as appropriate; the
 *		kernel message structure uses only global port names
 *		and holds port references.
 *
 *		Memory for in-transit messages is kept in one of two maps:
 *			ipc_kernel_map:	arrays of ports
 *			ipc_soft_map:	all else
 *	Conditions:
 *		No locks held on entry or exit.
 *	Errors:
 *		Error values appropriate to msg_send are returned; only
 *		for SEND_SUCCESS will a valid kernel message be returned.
 */
msg_return_t
msg_copyin(task, msgptr, msg_size, kmsgptr)
	register task_t task;
	msg_header_t *msgptr;
	msg_size_t msg_size;
	kern_msg_t *kmsgptr;
{
	register kern_msg_t kmsg;
	port_t lp, rp;
	kern_obj_t dest_port, reply_port;

	assert(task != TASK_NULL);

	if (msg_size < sizeof(msg_header_t))
		return SEND_MSG_TOO_SMALL;

	/* Allocate a new kernel message. */

#if	NeXT
	if (msg_size > MSG_SIZE_MAX)
		return(SEND_MSG_TOO_LARGE);
	kmsg = kern_msg_allocate(msg_size);
#else	NeXT
	if (msg_size > (KERN_MSG_SMALL_SIZE -
			sizeof(struct kern_msg) +
			sizeof(msg_header_t))) {

		/* Enforce the maximum message size. */

		if (msg_size > MSG_SIZE_MAX)
			return SEND_MSG_TOO_LARGE;

		kern_msg_allocate_large(kmsg);
	} else
		kern_msg_allocate_small(kmsg);
#endif	NeXT

	/* Copy in the message. */

	if (current_thread()->ipc_kernel) {
		bcopy((caddr_t) msgptr,
		      (caddr_t) &kmsg->kmsg_header,
		      msg_size);
	} else {
		if (copyin((caddr_t) msgptr,
			   (caddr_t) &kmsg->kmsg_header,
			   msg_size)) {
			kern_msg_free(kmsg);
			return SEND_INVALID_MEMORY;
		}
	}

#if	MACH_IPC_STATS
	ipc_event(messages);
	if (kmsg->kmsg_header.msg_type & MSG_TYPE_EMERGENCY)
		ipc_event(emergency);
#endif	MACH_IPC_STATS

	/* Copied-in size may not be correct. */
	kmsg->kmsg_header.msg_size = msg_size;

	/* Translate ports in header from user's name to global name. */

	lp = kmsg->kmsg_header.msg_local_port;
	rp = kmsg->kmsg_header.msg_remote_port;

	ipc_task_lock(task);

	if (rp == PORT_NULL) {
		ipc_task_unlock(task);
		kern_msg_free(kmsg);
		return SEND_INVALID_PORT;
	} else {
		register kern_port_t _dest_port;

		obj_cache_copyin(task, rp, dest_port,
			{
				ipc_task_unlock(task);
				kern_msg_free(kmsg);
				return SEND_INVALID_PORT;
			});

		/*
		 *	Check whether this message is destined for the
		 *	kernel.
		 */

		_dest_port = (kern_port_t) dest_port;
		port_lock(_dest_port);
		if (_dest_port->port_receiver == kernel_task)
			kmsg->kernel_message = TRUE;
		port_unlock(_dest_port);
	}

	if (lp == PORT_NULL)
		reply_port = KERN_OBJ_NULL;
	else
		obj_cache_copyin(task, lp, reply_port,
			{
				ipc_task_unlock(task);
				obj_release(dest_port);
				kern_msg_free(kmsg);
				return SEND_INVALID_PORT;
			});

	ipc_task_unlock(task);

	kmsg->kmsg_header.msg_local_port  = (port_t) dest_port;
	kmsg->kmsg_header.msg_remote_port = (port_t) reply_port;

	if (! kmsg->kmsg_header.msg_simple) {
		register caddr_t saddr;
		caddr_t endaddr;
		boolean_t is_simple;
		boolean_t (*copyinf)();
		vm_map_t map = task->map;

		ipc_event(complex);
		is_simple = TRUE;

		if (task->kernel_ipc_space)
			copyinf = object_copyin_from_kernel;
		else
			copyinf = object_copyin;

		saddr = (caddr_t) (&kmsg->kmsg_header + 1);
		endaddr = (((caddr_t) &kmsg->kmsg_header) +
			   kmsg->kmsg_header.msg_size);

		while (saddr < endaddr) {
			unsigned int tn;
			unsigned long ts;
			register long elts;
			unsigned long numbytes;
			boolean_t is_port;

			kern_obj_t *port_list;
			register msg_type_long_t *tp =
				(msg_type_long_t *) saddr;
			boolean_t dealloc =
				tp->msg_type_header.msg_type_deallocate;
			caddr_t staddr = saddr;

			if (tp->msg_type_header.msg_type_longform) {
				tn = tp->msg_type_long_name;
				ts = tp->msg_type_long_size;
				elts = tp->msg_type_long_number;
				saddr += sizeof(msg_type_long_t);
			} else {
				tn = tp->msg_type_header.msg_type_name;
				ts = tp->msg_type_header.msg_type_size;
				elts = tp->msg_type_header.msg_type_number;
				saddr += sizeof(msg_type_t);
			}

			numbytes = ((elts * ts) + 7) >> 3;

			/*
			 *	Determine whether this item requires more
			 *	data than is left in the message body.
			 */

			if ((endaddr - saddr) <
			    (tp->msg_type_header.msg_type_inline ?
			     numbytes : sizeof(caddr_t))) {
				kmsg->kmsg_header.msg_size =
					staddr - (caddr_t) &kmsg->kmsg_header;
				msg_destroy(kmsg);
				return SEND_MSG_TOO_SMALL;
			}

			is_port = MSG_TYPE_PORT_ANY(tn);

			/*
			 *	Check that the size field for ports
			 *	is correct.  The code that examines
			 *	port_list relies on this.
			 */

			if (is_port && (ts != 32)) {
				kmsg->kmsg_header.msg_size =
					staddr - (caddr_t) &kmsg->kmsg_header;
				msg_destroy(kmsg);
				return SEND_INVALID_PORT;
			}

			/*
			 *	Copy data in from user's space, if
			 *	out-of-line, advance to the next item.
			 */

			if (tp->msg_type_header.msg_type_inline) {
				port_list = (kern_obj_t *)saddr;
				saddr += ( (numbytes+3) & (~0x3)) ;
			} else {
				is_simple = FALSE;
				if (tn != MSG_TYPE_INTERNAL_MEMORY) {
					vm_offset_t	user_addr;

					/*
					 *	If length is zero, then address
					 *	should be ignored.
					 */

					user_addr = (numbytes == 0) ? 0 :
						* (vm_offset_t *) saddr;


#if	MACH_OLD_VM_COPY
					if (vm_move(map,
						    user_addr,
						    (is_port ? ipc_kernel_map
						             : ipc_soft_map),
						    (vm_size_t) numbytes, dealloc,
						    (vm_offset_t *) saddr) != KERN_SUCCESS) {
						kmsg->kmsg_header.msg_size =
							staddr - (caddr_t) &kmsg->kmsg_header;
						msg_destroy(kmsg);
						return SEND_INVALID_MEMORY;
					}
					port_list = * (kern_obj_t **) saddr;
#else	MACH_OLD_VM_COPY
					vm_map_copy_t	copy_addr;

					if (vm_map_copyin(map,
						    user_addr,
						    (vm_size_t) numbytes, dealloc,
						    &copy_addr) != KERN_SUCCESS) {

					 BadVMCopy: ;

						kmsg->kmsg_header.msg_size =
							staddr - (caddr_t) &kmsg->kmsg_header;
						msg_destroy(kmsg);
						return SEND_INVALID_MEMORY;
					}

					/*
					 *	Must copy port names out to kernel VM so that
					 *	they can be translated.
					 */

					if (is_port) {
						if (vm_map_copyout(
							ipc_kernel_map,
							(vm_offset_t *) saddr,
							copy_addr,
							TRUE)
						    != KERN_SUCCESS)
							goto BadVMCopy;
						port_list = * (kern_obj_t **) saddr;
					} else {
						*(vm_offset_t *)saddr = 
						    (vm_offset_t) copy_addr;
					}
#endif	MACH_OLD_VM_COPY
				} else
					assert(!is_port);

				saddr += sizeof (caddr_t) ;
			}

			/*
			 *	Translate ports in this item from user's name
			 *	to global.
			 */

			if (is_port) {
				is_simple = FALSE;

				if ((elts > 1024) &&
				    (ipc_debug & IPC_DEBUG_1K_PORTS))
	uprintf("msg_copyin: passing more than 1K ports in one item!\n");

				while (--elts >= 0) {
					kern_obj_t obj = *port_list;
					port_name_t name = (port_name_t) obj;
					
					/*
					 *	Note that any of the
					 *	references to port_list
					 *	may encounter a page fault,
					 *	so those accesses are all
					 *	done here, rather than passing
					 *	an address to the port_list
					 *	cell to the copyin function.
					 */

					if ((name != PORT_NULL) &&
					    ! (*copyinf)(task, name,
							 tn, dealloc,
							 &obj)) {
						*port_list = KERN_OBJ_INVALID;
						msg_destroy(kmsg);
						return SEND_INVALID_PORT;
					}

					*port_list = obj;
					port_list++;
				}
			}
		}
		kmsg->kmsg_header.msg_simple = is_simple;
	}

	*kmsgptr = kmsg;
	return SEND_SUCCESS;
}
