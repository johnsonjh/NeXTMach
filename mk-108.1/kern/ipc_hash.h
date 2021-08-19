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
 * $Log:	ipc_hash.h,v $
 * 23-Nov-88  Avadis Tevanian (avie) at NeXT
 *	Smaller PORT_HASH_COUNT to save some memory.
 *
 * Revision 2.9  89/03/09  20:11:50  rpd
 * 	More cleanup.
 * 
 * Revision 2.8  89/02/25  18:02:21  gm0w
 * 	Kernel code cleanup.	
 * 
 * Revision 2.7  89/01/15  16:22:28  rpd
 * 	Removed hits field from port_hash_bucket_t.
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  14:58:20  rpd]
 * 
 * 23-Nov-88  Avadis Tevanian (avie) at NeXT
 *	Smaller PORT_HASH_COUNT to save some memory.
 *
 * Revision 2.6  88/10/18  03:19:51  mwyoung
 * 	Use <kern/macro_help.h> to avoid lint.
 * 	[88/10/15            mwyoung]
 * 
 * Revision 2.5  88/10/11  10:15:07  rpd
 * 	Removed some declarations which duplicated ones in ipc_globals.h
 * 	and ipc_prims.h.  Added kmsg field to translation entries.
 * 	[88/10/04  07:02:42  rpd]
 * 
 * Revision 2.4  88/09/25  22:11:01  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:12:36  rpd]
 * 
 * Revision 2.3  88/07/22  07:24:36  rpd
 * Added bucket macros, PORT_TYPE_IS_* macros.
 * 
 * Revision 2.2  88/07/20  16:32:24  rpd
 * Fixed IPC_HASH/PORT_HASH mistake.
 * Created from mach_ipc.c.
 * 
 */ 

#ifndef	_KERN_IPC_HASH_H_
#define _KERN_IPC_HASH_H_

#import <sys/port.h>
#import <kern/queue.h>
#import <kern/task.h>
#import <kern/kern_obj.h>
#import <kern/kern_msg.h>
#import <kern/macro_help.h>

/*
 *	Port rights and translation data structures:
 *
 *	Entries representing valid (task, port, local name) tuples
 *	are hashed by (task, port) and by (task, local name);
 *	additionally, each port and task has a chain of all tuples
 *	containing that port/task.
 *
 *	All the fields except for kmsg are locked by both the task
 *	and object.  Both need to be locked for modifications; locking
 *	either gets read access.  The object controls the kmsg field.
 */

typedef struct port_hash {
	queue_chain_t	TP_chain;	/* Chain for (task, object) table */
	queue_chain_t	TL_chain;	/* Chain for (task, name) table */

	task_t		task;		/* The owning task */
	queue_chain_t	task_chain;	/* Chain for "all same task" */

	port_name_t	local_name;	/* The task's name for us */
	port_type_t	type;		/* The type of capability */

	kern_obj_t	obj;		/* Corresponding internal object */
	queue_chain_t	obj_chain;	/* All entries for an object */

	/* special field: only locked by object */
	kern_msg_t	kmsg;		/* Used for SEND_NOTIFY */
} *port_hash_t;

#define PORT_HASH_NULL	((port_hash_t) 0)

/*
 *	The hash tables themselves
 */

#if	NeXT
#define	PORT_HASH_COUNT		(1 << 9)
#else	NeXT
#define PORT_HASH_COUNT		(1 << 11)
#endif	NeXT

typedef struct port_hash_bucket {
	queue_head_t head;
	decl_simple_lock_data(,lock)
} port_hash_bucket_t;


#define port_hash_TP(task,global) \
	( ( ((int)(task)) + (((int)(global)) >> 5) ) & (PORT_HASH_COUNT-1))
#define port_hash_TL(task,local) \
	( (((int)(task)) >> 2) + (int)(local) ) & (PORT_HASH_COUNT - 1)

extern port_hash_bucket_t *TP_table;
extern port_hash_bucket_t *TL_table;

#define bucket_lock_init(bucket)	simple_lock_init(&(bucket)->lock)
#define bucket_lock(bucket)		simple_lock(&(bucket)->lock)
#define bucket_unlock(bucket)		simple_unlock(&(bucket)->lock)

#define BUCKET_ENTER(bucket, entry, chain) \
MACRO_BEGIN								\
	bucket_lock(bucket); 						\
	queue_enter(&(bucket)->head, (entry), port_hash_t, chain); 	\
	bucket_unlock(bucket); 						\
MACRO_END

#define BUCKET_REMOVE(bucket, entry, chain) \
MACRO_BEGIN								\
	bucket_lock(bucket); 						\
	queue_remove(&(bucket)->head, (entry), port_hash_t, chain);	\
	bucket_unlock(bucket); 						\
MACRO_END

#define PORT_TYPE_IS_PORT(type)	((type) != PORT_TYPE_SET)
#define PORT_TYPE_IS_SET(type)	((type) == PORT_TYPE_SET)

#endif	_KERN_IPC_HASH_H_


