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
 * $Log:	ipc_prims.c,v $
 * Revision 2.13  89/06/27  00:24:14  rpd
 * 	Added type arguments to obj_entry_create, obj_alloc.
 * 	This fixes locking bug with modifying the type field
 * 	after obj_alloc is called.
 * 	[89/06/26  23:52:54  rpd]
 * 
 * Revision 2.12  89/05/01  17:00:44  rpd
 * 	Updated for the new translation cache code.
 * 	Removed task_check_name.  (Now it is a macro that doesn't
 * 	attempt to remove translations for dying objects.  As it was,
 * 	a task wouldn't get a port-death notification for a translation
 * 	removed by task_check_name.  The current solution isn't perfect:
 * 	port death is visibly not atomic.)
 * 	Rewrote obj_destroy_rights to fix a bug with port-death
 * 	notification generation: we didn't hold a task reference.
 * 	[89/05/01  14:46:58  rpd]
 * 
 * Revision 2.11  89/02/25  18:03:44  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.10  89/01/15  16:23:16  rpd
 * 	Removed the obj_traversing field from kern_obj.
 * 	[89/01/15  14:59:58  rpd]
 * 
 * Revision 2.9  89/01/12  07:55:48  rpd
 * 	Moved ipc_statistics.h to mach_debug/.
 * 	[89/01/12  04:45:38  rpd]
 * 
 * Revision 2.8  89/01/10  23:30:00  rpd
 * 	When MACH_IPC_XXXHACK, the name PORT_ENABLED isn't reserved.
 * 	[89/01/10  23:02:26  rpd]
 * 
 * Revision 2.7  88/12/19  02:45:15  mwyoung
 * 	Remove lint.
 * 	[88/12/17            mwyoung]
 * 	
 * 	Added ipc_obj_print.
 * 	[88/12/04            mwyoung]
 * 
 * Revision 2.6  88/10/11  10:17:46  rpd
 * 	Updated for new obj_entry_lookup_macro.
 * 	[88/10/09  08:44:08  rpd]
 * 	
 * 	Don't need sender_task to implement SEND_NOTIFY.
 * 	[88/10/06  07:54:04  rpd]
 * 	
 * 	Updated for new SEND_NOTIFY implementation.
 * 	[88/10/04  07:05:18  rpd]
 * 
 * Revision 2.5  88/09/25  22:13:00  rpd
 * 	Changed obj_entry_lookup to use the new obj_entry_lookup_macro.
 * 	Changed port_cache_flush to obj_cache_flush.  Changed obj_entry_make
 * 	to only skip over PORT_ENABLED and PORT_NULL.
 * 	[88/09/24  18:05:23  rpd]
 * 	
 * 	Fixed obj_entry_create to check for the possibility of name wrap-around.
 * 	Removed low-bit-test in obj_entry_find.
 * 	[88/09/20  01:59:17  rpd]
 * 	
 * 	Changed includes to the new style.
 * 	[88/09/19  16:20:12  rpd]
 * 
 * Revision 2.4  88/08/06  18:19:18  rpd
 * Moved port functions to ipc_kport.c, set functions to ipc_kset.c.
 * 
 * Revision 2.3  88/07/29  03:19:46  rpd
 * Wrote new function obj_entry_find(), which finds a translation record
 * given the task and object.  Use it in obj_entry_make() and port_dealloc().
 * 
 * Revision 2.2  88/07/22  07:29:48  rpd
 * Created for primitive IPC functions.
 * 
 */
/*
 * File:	ipc_prims.c
 * Purpose:
 *	Primitive translation record & object functions.
 */

#import <mach_ipc_xxxhack.h>

#import <kern/assert.h>
#import <kern/queue.h>
#import <sys/boolean.h>
#import <kern/zalloc.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <sys/port.h>
#import <sys/message.h>
#import <kern/kern_obj.h>
#import <kern/kern_port.h>
#import <kern/kern_msg.h>
#import <kern/kern_set.h>
#import <kern/sched_prim_macros.h>
#import <sys/notify.h>
#import <kern/ipc_statistics.h>
#import <kern/ipc_signal.h>
#import <kern/ipc_hash.h>
#import <kern/ipc_cache.h>
#import <kern/ipc_kmesg.h>
#import <kern/ipc_globals.h>
#import <kern/ipc_prims.h>

/*
 *	Routine:	obj_entry_find [internal]
 *	Purpose:
 *		Given an object, find the task's translation record for it.
 *		Returns PORT_HASH_NULL if not found.
 *
 *	Conditions:
 *		The task or the object (or both) must be locked.
 */
port_hash_t
obj_entry_find(task, obj)
	task_t task;
	kern_obj_t obj;
{
	register port_hash_t entry;
	register port_hash_bucket_t *bucket;

	bucket = &TP_table[port_hash_TP(task, obj)];
	bucket_lock(bucket);
	entry = (port_hash_t) queue_first(&bucket->head);

	while (!queue_end(&bucket->head, (queue_entry_t) entry)) {
		if ((entry->task == task) &&
		    (entry->obj == obj)) {
			bucket_unlock(bucket);

			return entry;
		}
		ipc_event(bucket_misses);
		entry = (port_hash_t) queue_next(&entry->TP_chain);
	}
	bucket_unlock(bucket);

	return PORT_HASH_NULL;
}

/*
 *	Routine:	obj_entry_lookup [internal]
 *	Purpose:
 *		Given a task's local name for something, find the translation
 *		record.  Returns PORT_HASH_NULL if not found.
 *	Conditions:
 *		The task must be locked.
 */
port_hash_t
obj_entry_lookup(task, name)
	task_t task;
	port_name_t name;
{
	register port_hash_t entry;

	obj_entry_lookup_macro(task, name, entry, return PORT_HASH_NULL;);
	return entry;
}

/*
 *	Routine:	obj_entry_change [internal]
 *	Purpose:
 *		Change a task's local name for a capability.
 *	Conditions:
 *		The task and object are both locked throughout.
 */
void
obj_entry_change(task, obj, entry, old_name, new_name)
	task_t task;
	kern_obj_t obj;
	port_hash_t entry;
	port_name_t old_name, new_name;
{
	register port_hash_bucket_t *bucket;

	assert(entry->task == task);
	assert(entry->obj == obj);
	assert(entry->local_name == old_name);

	bucket = &TL_table[port_hash_TL(task, old_name)];
	BUCKET_REMOVE(bucket, entry, TL_chain);

	entry->local_name = new_name;

	bucket = &TL_table[port_hash_TL(task, new_name)];
	BUCKET_ENTER(bucket, entry, TL_chain);

	/* flush old translation from the cache */
	obj_cache_flush(task, old_name);
}

/*
 *	Routine:	obj_entry_remove [internal]
 *	Purpose:
 *		Removes a task's capability for an object,
 *		but doesn't deallocate the capability or
 *		remove it from the object's list of capabilities.
 *	Conditions:
 *		Both the task and object are locked throughout.
 */
void
obj_entry_remove(task, obj, entry)
	task_t task;
	kern_obj_t obj;
	port_hash_t entry;
{
	register port_hash_bucket_t *bucket;
	register kern_msg_t kmsg;
	port_name_t name = entry->local_name;

	assert(entry->task == task);
	assert(entry->obj == obj);

	/* check for pending msg-accepted notification */

	kmsg = entry->kmsg;
	if (kmsg != KERN_MSG_NULL) {
		assert(kmsg->sender_entry == entry);
		assert((kern_obj_t) kmsg->kmsg_header.msg_local_port == obj);

		/* clear the flag, so notification won't get generated */
		kmsg->sender_entry = PORT_HASH_NULL;
	}

	/* remove the translation */

	bucket = &TP_table[port_hash_TP(task, obj)];
	BUCKET_REMOVE(bucket, entry, TP_chain);

	bucket = &TL_table[port_hash_TL(task, name)];
	BUCKET_REMOVE(bucket, entry, TL_chain);

	queue_remove(&task->ipc_translations, entry, port_hash_t, task_chain);
	obj->obj_references--;

	/* flush the translation from the task's cache */
	obj_cache_flush(task, name);
}

/*
 *	Routine:	obj_entry_dealloc [internal]
 *	Purpose:
 *		Deallocate a capability for an object.
 *		The object loses a reference.
 *	Conditions:
 *		The object is locked throughout.
 */
void
obj_entry_dealloc(obj, entry)
	kern_obj_t obj;
	port_hash_t entry;
{
	assert(entry->obj == obj);

	queue_remove(&obj->obj_translations, entry, port_hash_t, obj_chain);

	ZFREE(port_hash_zone, (vm_offset_t) entry);
}

/*
 *	Routine:	obj_entry_destroy [internal]
 *	Purpose:
 *		Remove and deallocate a task's capability for an object.
 *		The object loses a reference.
 *	Conditions:
 *		The task and object are locked throughout.
 */
void
obj_entry_destroy(task, obj, entry)
	task_t task;
	kern_obj_t obj;
	port_hash_t entry;
{
	assert(entry->task == task);
	assert(entry->obj == obj);

	/* If the object isn't in use, then obj_destroy_rights is
	   going to deallocate the entry sooner or later.  Don't
	   disturb the object's list of translations, but indicate
	   what happened by clearing entry->task. */

	obj_entry_remove(task, obj, entry);
	if (obj->obj_in_use)
		obj_entry_dealloc(obj, entry);
	else
		entry->task = TASK_NULL;
}

/*
 *	Routine:	obj_entry_insert [internal]
 *	Purpose:
 *		Initializes an already allocated entry, giving the task
 *		the specified rights for the object.  The object gains
 *		a reference.
 *
 *	Conditions:
 *		The task and object are locked throughout.
 */
void
obj_entry_insert(entry, task, obj, name, type)
	register port_hash_t entry;
	register task_t task;
	register kern_obj_t obj;
	port_name_t name;
	port_type_t type;
{
	register port_hash_bucket_t *bucket;

	entry->task = task;
	entry->obj = obj;
	entry->local_name = name;
	entry->type = type;
	entry->kmsg = KERN_MSG_NULL;

	queue_enter(&obj->obj_translations, entry,
		    port_hash_t, obj_chain);
	queue_enter(&task->ipc_translations,
		    entry, port_hash_t, task_chain);

	bucket = &TP_table[port_hash_TP(task, obj)];
	BUCKET_ENTER(bucket, entry, TP_chain);
	bucket = &TL_table[port_hash_TL(task, name)];
	BUCKET_ENTER(bucket, entry, TL_chain);

	/* take ref for the new translation */
	obj->obj_references++;
}

/*
 *	Routine:	obj_entry_create [internal]
 *	Purpose:
 *		Initializes an already allocated entry, giving the task
 *		the PORT_TYPE_NONE rights for the object.  The object gains
 *		a reference.  Uses the next unused name in the task,
 *		after ipc_next_name.
 *
 *	Conditions:
 *		The task and object are locked throughout.
 */
void
obj_entry_create(entry, task, obj, type)
	register port_hash_t entry;
	register task_t task;
	register kern_obj_t obj;
{
	register port_hash_bucket_t *bucket;
	port_name_t name;

	entry->task = task;
	entry->obj = obj;
	entry->type = type;
	entry->kmsg = KERN_MSG_NULL;

	queue_enter(&obj->obj_translations, entry,
		    port_hash_t, obj_chain);
	queue_enter(&task->ipc_translations,
		    entry, port_hash_t, task_chain);

	/* Bit of a complication here, because we must check that a
	   name is not in use, due to existence of port_rename and
	   port_insert_{send,receive} and the possibility of wrap-around. */

	for (;;) {
		register port_hash_t try_entry;

		name = task->ipc_next_name;

#if	MACH_IPC_XXXHACK
		/* names PORT_ENABLED (-1) and PORT_NULL (0) are reserved */
		if (name == PORT_ENABLED)
			name = PORT_NULL + 1;
#else	MACH_IPC_XXXHACK
		/* the name PORT_NULL (0) is reserved */
		if (name == PORT_NULL)
			name = PORT_NULL + 1;
#endif	MACH_IPC_XXXHACK

		task->ipc_next_name = name + 1;

		bucket = &TL_table[port_hash_TL(task, name)];
		bucket_lock(bucket);
		try_entry = (port_hash_t) queue_first(&bucket->head);

		for (;;) {
			if (queue_end(&bucket->head, (queue_entry_t) try_entry))
				goto found;

			if ((try_entry->task == task) && 
			    (try_entry->local_name == name))
				break;

			ipc_event(bucket_misses);
			try_entry = (port_hash_t) queue_next(&try_entry->TL_chain);
		}

		bucket_unlock(bucket);
	}

    found:
	/* bucket is locked */
	entry->local_name = name;
	queue_enter(&bucket->head, entry, port_hash_t, TL_chain);
	bucket_unlock(bucket);

	bucket = &TP_table[port_hash_TP(task, obj)];
	BUCKET_ENTER(bucket, entry, TP_chain);

	/* take ref for the new translation */
	obj->obj_references++;
}

/*
 *	Routine:	obj_entry_make [internal]
 *	Purpose:
 *		Used in copying out a capability in transit.  The
 *		caller must hold a ref for the object.  May take an
 *		additional ref for the object.
 *
 *		It tries to find an existing translation, unless nomerge
 *		is true.  Only existing translations with odd types are
 *		acceptable.
 *
 *	Conditions:
 *		Task and object locked at start and finish.  May be
 *		unlocked, to allocate a new translation entry.
 */
port_hash_t
obj_entry_make(task, obj, nomerge)
	task_t task;
	kern_obj_t obj;
	boolean_t nomerge;
{
	register port_hash_t entry;

	for (;;) {
		/* Can't allow translations unless both task and
		   object are active. */

		if (!task->ipc_active || !obj->obj_in_use)
			return PORT_HASH_NULL;

		/* Look for an eligible existing translation. */

		if (!nomerge) {
			entry = obj_entry_find(task, obj);
			if (entry != PORT_HASH_NULL)
				return entry;
		}

		/* Must create new translation entry.  Try to allocate
		   new entry, using non-blocking call. */

		ZGET(port_hash_zone, entry, port_hash_t);
		if (entry != PORT_HASH_NULL) {
			/* Initialize new translation entry. */

			obj_entry_create(entry, task, obj, PORT_TYPE_NONE);

			return entry;
		}

		/* Must unlock everything before trying
		   the blocking allocation call. */

		obj_unlock(obj);
		ipc_task_unlock(task);

		ZALLOC(port_hash_zone, entry, port_hash_t);
		assert(entry != PORT_HASH_NULL);
		ZFREE(port_hash_zone, (vm_offset_t) entry);

		ipc_task_lock(task);
		obj_lock(obj);

		/* Because we dropped locks, a translation
		   might already exist now.  Start over. */
	}
}

/*
 *	Routine:	obj_alloc [internal]
 *	Purpose:
 *		Abstracts common code for allocating an object
 *		in a task's capability name space.
 *
 *		Returns the new translation entry.  The object
 *		is initialized with one reference (for the translation,
 *		not the caller) and locked.
 *	Conditions:
 *		The task shouldn't be locked on entry or exit.
 *		The entry's type field isn't initialized.
 */
kern_return_t
obj_alloc(task, zone, type, entryp)
	task_t task;
	zone_t zone;
	port_type_t type;
	port_hash_t *entryp;
{
	register kern_obj_t obj;
	register port_hash_t entry;

	ZALLOC(zone, obj, kern_obj_t);
	if (obj == KERN_OBJ_NULL)
		return KERN_RESOURCE_SHORTAGE;
	ZALLOC(port_hash_zone, entry, port_hash_t);
	assert(entry != PORT_HASH_NULL);

	simple_lock_init(&obj->obj_data_lock);
	obj->obj_in_use = TRUE;
	obj->obj_references = 0;
	obj->obj_home_zone = zone;
	queue_init(&obj->obj_translations);

	/* need to have task locked to install new translation */

	ipc_task_lock(task);
	if (!task->ipc_active) {
		/* can't install translations in a dying task */
		ipc_task_unlock(task);
		ZFREE(zone, (vm_offset_t) obj);
		ZFREE(port_hash_zone, (vm_offset_t) entry);
		return KERN_INVALID_ARGUMENT;
	}

	/* obj_lock requires a reference, and the object is yet nascent */
	simple_lock(&obj->obj_data_lock);

	obj_entry_create(entry, task, obj, type);

	ipc_task_unlock(task);
	*entryp = entry;

	return KERN_SUCCESS;
}

/*
 *	Routine:	obj_destroy_rights [internal]
 *	Purpose:
 *		Removes all rights to an object.  Sends death
 *		notications to tasks that had rights.
 *	Conditions:
 *		The object is locked and dead.  The caller must hold
 *		a ref for the object, because the object is unlocked
 *		during intermediate stages.
 *	Discussion:
 *		This is a tough problem.  Some factors:
 *		    Can't have anything locked when sending notifications.
 *
 *		    Can't (ipc) lock tasks when an object (port/set) is locked,
 *		    but need both object and task locked to remove translation.
 *
 *		    Need to be able to remove a task's translation for
 *		    a dead object when killing tasks.
 *
 *		    Translation records don't directly hold refs for tasks
 *		    the way they do for objects.  However, a task isn't
 *		    destroyed until after all of its translations are gone.
 *
 *		    Translation records don't have ref counts; to keep
 *		    them around, either the task or object must be
 *		    kept locked.
 *
 *		So obj_destroy_rights is looking at an entry, which it
 *		wants to convert to a dead name or remove,
 *		while having the object locked but not the task.
 *		To lock the task, must first unlock the object.  But with
 *		the object unlocked, the entry can disappear and the task
 *		be destroyed.  We can reference the task before unlocking
 *		the object, but this won't keep the entry from disappearing
 *		out from under us.
 *
 *		Solution: make destroying entries a two-step process.
 *		First the entry is removed from the task (after which the
 *		task might be destroyed).  Then the entry is removed
 *		from the object and freed, and the object loses a reference.
 *		If the object is dead, obj_entry_destroy doesn't do
 *		the second step, but sets the task pointer in the entry
 *		to TASK_NULL as an indication that the entry is halfway dead.
 *		This way, obj_destroy_rights doesn't have to worry about
 *		disappearing entries.  However, it must careful with task refs.
 *
 *		Another concern: task_deallocate can take a *long* time.
 *		Not sure if it can allocate memory or enter the IPC system.
 *		In any case, don't hold locks across it.
 */
void
obj_destroy_rights(obj)
	register kern_obj_t obj;
{
	task_t otask = TASK_NULL;

	assert(!obj->obj_in_use);

	while (!queue_empty(&obj->obj_translations)) {
		/* object is locked here */

		register port_hash_t entry = (port_hash_t)
				queue_first(&obj->obj_translations);
		register task_t task = entry->task;
		port_name_t name;

		/* The task might be null, if obj_entry_destroy
		   already removed the translation from a task.
		   In that case, just finish the job. */

		if (task == TASK_NULL) {
			obj_entry_dealloc(obj, entry);
			continue;
		}

		/* Ensure that the task doesn't go away after we've
		   unlocked the object. */

		task_reference(task);
		obj_unlock(obj);
		if (otask != TASK_NULL)
			task_deallocate(otask);
		otask = task;
		ipc_task_lock(task);
		obj_lock(obj);
		assert(entry->obj == obj);

		/* While the object was unlocked, obj_entry_destroy
		   might have removed the entry from the task.
		   In that case, just finish the job. */

		if (entry->task == TASK_NULL) {
			ipc_task_unlock(task);
			obj_entry_dealloc(obj, entry);
			continue;
		}

		assert(entry->task == task);
		name = entry->local_name;
		obj_entry_remove(task, obj, entry);
		ipc_task_unlock(task);
		obj_entry_dealloc(obj, entry);

		/* note the task reference is still held */

		obj_unlock(obj);
		send_notification(task, NOTIFY_PORT_DELETED, name);
		obj_lock(obj);
	}

	if (otask != TASK_NULL) {
		obj_unlock(obj);
		task_deallocate(otask);
		obj_lock(obj);
	}
}

/*
 *	Routine:	msg_queue_changed [internal]
 *	Purpose:
 *		Wake-up threads waiting to receive on a msg_queue, because
 *		the port (or set) is dead or was sent away in a message (state
 *		should be RCV_INVALID_PORT) or was moved into a set
 *		(state should be RCV_PORT_CHANGE).
 *	Conditions:
 *		The port or set that the queue is in is locked.
 */
void
msg_queue_changed(mq, new_state)
	msg_queue_t *mq;
	msg_return_t new_state;
{
	msg_queue_lock(mq);

	while (!queue_empty(&mq->blocked_threads)) {
		register thread_t t;

		queue_remove_first(&mq->blocked_threads, t,
				   thread_t, ipc_wait_queue);
		t->ipc_state = new_state;
		thread_go(t);
	}

	msg_queue_unlock(mq);
}

#if	DEBUG
/*
 *	Routine:	ipc_obj_print [exported]
 *	Purpose:
 */
void
ipc_obj_print(obj)
	kern_obj_t	obj;
{
	register port_hash_t entry;
	extern int indent;

	iprintf("in_use = %d, ref count = %d, zone = 0x%x\n",
		obj->obj_in_use, obj->obj_references, obj->obj_home_zone);
	iprintf("translations:\n");

	indent += 2;
	for (entry = (port_hash_t) queue_first(&obj->obj_translations);
	     !queue_end(&obj->obj_translations, (queue_entry_t) entry);
	     entry = (port_hash_t) queue_next(&entry->obj_chain))
		iprintf("task 0x%x, name 0x%x, type 0x%x\n",
			entry->task, entry->local_name, entry->type);
	indent -= 2;
}
#endif	DEBUG

