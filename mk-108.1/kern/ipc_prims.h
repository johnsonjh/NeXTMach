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
 * $Log:	ipc_prims.h,v $
 * Revision 2.10  89/05/01  17:01:00  rpd
 * 	task_check_name is a simple macro now.
 * 	[89/05/01  14:47:40  rpd]
 * 
 * Revision 2.9  89/03/09  20:12:34  rpd
 * 	More cleanup.
 * 
 * Revision 2.8  89/02/25  18:03:51  gm0w
 * 	Kernel code cleanup.	
 * 	Put entire file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.7  88/10/18  03:20:41  mwyoung
 * 	Use <kern/macro_help.h> to avoid lint.
 * 	[88/10/15            mwyoung]
 * 
 * Revision 2.6  88/10/11  10:18:16  rpd
 * 	Revamped obj_entry_lookup_macro.
 * 	[88/10/09  08:45:07  rpd]
 * 
 * Revision 2.5  88/09/25  22:13:14  rpd
 * 	Added obj_entry_lookup_macro.
 * 	[88/09/24  18:06:59  rpd]
 * 	
 * 	Changed includes to the new style.
 * 	[88/09/19  16:21:04  rpd]
 * 
 * Revision 2.4  88/08/06  18:19:51  rpd
 * Moved declarations of port_* functions to kern/kern_port.h,
 * moved declarations of set_* functions to kern/kern_set.h.
 * 
 * Revision 2.3  88/07/29  03:20:01  rpd
 * Declared new function obj_entry_find().  Defined new function
 * task_check_rights() as a macro in terms of obj_entry_find.
 * 
 * Revision 2.2  88/07/22  07:30:59  rpd
 * Created for declarations of IPC primitive functions.
 * 
 */ 

#ifndef	_KERN_IPC_PRIMS_H_
#define _KERN_IPC_PRIMS_H_

#import <sys/kern_return.h>
#import <sys/boolean.h>
#import <kern/ipc_hash.h>
#import <kern/macro_help.h>

extern port_hash_t obj_entry_find();
extern port_hash_t obj_entry_lookup();
extern void obj_entry_change();
extern void obj_entry_remove();
extern void obj_entry_dealloc();
extern void obj_entry_destroy();
extern void obj_entry_insert();
extern void obj_entry_create();
extern port_hash_t obj_entry_make();

extern kern_return_t obj_alloc();
extern void obj_destroy_rights();

#define task_check_name(task, name)	\
		(obj_entry_lookup((task), (name)) != PORT_HASH_NULL)

#define task_check_rights(task, obj)	\
		(obj_entry_find((task), (obj)) != PORT_HASH_NULL)

extern void msg_queue_changed();

/*
 * extern void
 * obj_entry_lookup_macro(task, name, entry, notfound)
 *	task_t task;
 *	port_name_t name;
 *	port_hash_t &entry;
 *	code notfound;
 *
 * The task must be locked.  Upon normal return, the by-reference
 * parameter "entry" points to the translation entry found.
 * If no entry is found, the "notfound" code (which should be a
 * single complete statement) is executed; it should return/goto.
 */

#define obj_entry_lookup_macro(_task, name, entry, notfound) 		\
MACRO_BEGIN								\
	register port_hash_bucket_t *bucket;				\
									\
	bucket = &TL_table[port_hash_TL((_task), (name))];		\
	bucket_lock(bucket);						\
	(entry) = (port_hash_t) queue_first(&bucket->head);		\
									\
	for (;;) {							\
		if (queue_end(&bucket->head, (queue_entry_t) (entry))) {\
			bucket_unlock(bucket);				\
			notfound					\
		}							\
									\
		if (((entry)->task == (_task)) &&			\
		    ((entry)->local_name == (name))) {			\
			bucket_unlock(bucket);				\
			break;						\
		}							\
									\
		ipc_event(bucket_misses);				\
		(entry) = (port_hash_t) queue_next(&(entry)->TL_chain);	\
	}								\
MACRO_END

#endif	_KERN_IPC_PRIMS_H_

