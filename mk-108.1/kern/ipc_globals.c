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
 * $Log:	ipc_globals.c,v $
 * 19-Mar-90  Gregg Kellogg (gk) at NeXT
 *	NeXT && !MACH_IPC_XXXHACK: tread MSG_TYPE_PORT_RECEIVE
 *	the same as MSG_TYPE_PORT_ALL, in the theory that that's what
 *	most people want anyway.
 *
 * 20-Feb-90  Gregg Kellogg (gk) at NeXT
 *	MACH_OLD_VM_COPY: need ipc_soft_map back.
 *	NeXT: use different values for initializing zone size variables.
 *
 * Revision 2.17  89/10/11  14:06:31  dlb
 * 	Bump size of large messages zone to 1024.  XXX
 * 	[89/02/24            dlb]
 * 
 * 	Add ipc_host_init() call to ipc_init().
 * 	[89/01/25            dlb]
 * 
 * Revision 2.16  89/10/10  10:53:59  mwyoung
 * 	Eliminate ipc_soft_map.
 * 	[89/06/18            mwyoung]
 * 
 * Revision 2.15  89/10/03  19:24:57  rpd
 * 	Fix from NeXT:  changed timeout_scaling_factor to usecs/tick.
 * 	This makes IPC timeouts more accurate.
 * 	[89/08/21  19:52:56  rpd]
 * 
 * Revision 2.14  89/06/30  22:30:59  rpd
 * 	Fixed the size of large kmsg buffers, so they can really
 * 	hold a MSG_SIZE_MAX message.
 * 	[89/06/30  22:17:30  rpd]
 * 
 * Revision 2.13  89/03/07  23:44:56  rpd
 * 	Moved max size and chunk size for the IPC zones into
 * 	initialized variables, so they can be easily patched.
 * 	Increased max size of the kmsg/port_hash zones.
 * 
 * Revision 2.12  89/03/05  16:46:44  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK.
 * 	[89/02/16  13:46:16  rpd]
 * 
 * Revision 2.11  89/02/25  18:02:09  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.10  89/01/15  16:22:06  rpd
 * 	Removed the hits fields in the TP/TL hash buckets.
 * 	[89/01/15  14:57:08  rpd]
 * 
 * Revision 2.9  89/01/10  23:29:01  rpd
 * 	Changed MACH_IPCSTATS to MACH_IPC_STATS.
 * 	[89/01/10  22:59:16  rpd]
 * 
 * Revision 2.8  88/12/21  14:21:44  mja
 * 	Changed size pararameter to zinit for port_hash_zone
 * 	to be a function of PORT_MAX.
 * 	[88/12/20  15:26:30  mrt]
 * 
 * Revision 2.7  88/11/23  16:38:12  rpd
 * 	Changed mach_ipc_debug to ipc_debug.
 * 	[88/11/23  10:39:04  rpd]
 * 
 * Revision 2.6  88/10/18  03:19:33  mwyoung
 * 	Use <kern/macro_help.h> to avoid lint.
 * 	[88/10/15            mwyoung]
 * 
 * Revision 2.5  88/10/11  10:14:00  rpd
 * 	Added complex_notification_template.
 * 	[88/10/11  07:57:46  rpd]
 * 
 * Revision 2.4  88/08/25  18:14:53  mwyoung
 * 	Corrected include file references.
 * 	[88/08/22            mwyoung]
 * 	
 * 	Eliminate the ipc_soft_map's pmap.  Inadvertent pmap operations
 * 	during paging IPC operations can cause deadlocks.
 * 	[88/08/11  19:11:01  mwyoung]
 * 
 * Revision 2.3  88/08/06  18:14:44  rpd
 * Replaced ipc_basics.h with ipc_copyin.h/ipc_copyout.h.
 * 
 * Revision 2.2  88/07/22  07:23:14  rpd
 * Created for Mach IPC global variables and initialization.
 * 
 */
/*
 * File:	ipc_globals.c
 * Purpose:
 *	Define & initialize Mach IPC global variables.
 */

#import <mach_ipc_stats.h>
#import <mach_ipc_xxxhack.h>

#import <sys/boolean.h>
#import <sys/port.h>
#import <kern/task.h>
#import <kern/zalloc.h>
#import <sys/notify.h>
#import <kern/mach_param.h>
#import <kern/kern_port.h>
#import <kern/kern_set.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <vm/vm_kern.h>
#import <kern/ipc_hash.h>
#import <kern/ipc_copyin.h>
#import <kern/ipc_copyout.h>
#import <kern/ipc_globals.h>

zone_t kmsg_zone;
zone_t kmsg_zone_large;

zone_t port_zone;
zone_t port_zone_reserved;

zone_t set_zone;

task_t ipc_soft_task;
vm_map_t ipc_soft_map;
vm_map_t ipc_kernel_map;

unsigned int ipc_debug = 0;

zone_t port_hash_zone;

port_hash_bucket_t *TP_table;
port_hash_bucket_t *TL_table;

notification_t notification_template;
notification_t complex_notification_template;

object_copyout_table_t object_copyout_table[MSG_TYPE_LAST];

object_copyin_table_t object_copyin_table[MSG_TYPE_LAST][PORT_TYPE_LAST];

unsigned int timeout_minimum = 1;
unsigned int timeout_scaling_factor;

#if	NeXT
int kmsg_large_max_num = 128;
#else	NeXT
int kmsg_large_max_num = 1024;
#endif	NeXT
int kmsg_large_alloc_num = 1;

#if	NeXT
int kmsg_max_num = 2048;
#else	NeXT
int kmsg_max_num = 4096;
#endif	NeXT
int kmsg_alloc_num = 32;

#if	NeXT
int port_hash_max_num = 10 * PORT_MAX;
#else	NeXT
int port_hash_max_num = 50 * PORT_MAX;
#endif	NeXT
int port_hash_alloc_num = 256;

int port_max_num = PORT_MAX;
int port_alloc_num = 128;

int set_max_num = SET_MAX;
int set_alloc_num = 32;

int port_reserved_max_num = PORT_MAX;
int port_reserved_alloc_num = 128;

/*
 *	Routine:	object_copyout_init [internal]
 *	Purpose:
 *		Called to initialize object_copyout_table.
 */
void
object_copyout_init()
{
	int mt;
	port_type_t pt;

#define init(mt, _destroy, _func, _nomerge)		\
MACRO_BEGIN						\
	object_copyout_table[mt].destroy = (_destroy);	\
	object_copyout_table[mt].func = (_func);	\
	object_copyout_table[mt].nomerge = (_nomerge);	\
MACRO_END

	for (mt = 0; mt < MSG_TYPE_LAST; mt++)
		init(mt, 0, 0, FALSE);

	init(MSG_TYPE_PORT, 0, 0, FALSE);
#if	MACH_IPC_XXXHACK
	init(MSG_TYPE_PORT_RECEIVE,
	     port_destroy_receive, port_copyout_receive, FALSE);
	init(MSG_TYPE_PORT_OWNERSHIP,
	     port_destroy_own, port_copyout_own, FALSE);
#else	MACH_IPC_XXXHACK
#if	NeXT
	init(MSG_TYPE_PORT_RECEIVE,
	     port_destroy_receive_own, port_copyout_receive_own, FALSE);
#endif	NeXT
#endif	MACH_IPC_XXXHACK
	init(MSG_TYPE_PORT_ALL,
	     port_destroy_receive_own, port_copyout_receive_own, FALSE);

#undef	init

#define init(mt, pt, _result)					\
MACRO_BEGIN							\
	object_copyout_table[mt].result[pt] = (_result);	\
MACRO_END

	for (mt = 0; mt < MSG_TYPE_LAST; mt++)
		for (pt = 0; pt < PORT_TYPE_LAST; pt++)
			init(mt, pt, PORT_TYPE_NONE);

	init(MSG_TYPE_PORT, PORT_TYPE_NONE, PORT_TYPE_SEND);
	init(MSG_TYPE_PORT, PORT_TYPE_SEND, PORT_TYPE_SEND);
#if	MACH_IPC_XXXHACK
	init(MSG_TYPE_PORT, PORT_TYPE_RECEIVE, PORT_TYPE_RECEIVE);
	init(MSG_TYPE_PORT, PORT_TYPE_OWN, PORT_TYPE_OWN);
#endif	MACH_IPC_XXXHACK
	init(MSG_TYPE_PORT, PORT_TYPE_RECEIVE_OWN, PORT_TYPE_RECEIVE_OWN);

#if	MACH_IPC_XXXHACK
	init(MSG_TYPE_PORT_RECEIVE, PORT_TYPE_NONE, PORT_TYPE_RECEIVE);
	init(MSG_TYPE_PORT_RECEIVE, PORT_TYPE_SEND, PORT_TYPE_RECEIVE);
	init(MSG_TYPE_PORT_RECEIVE, PORT_TYPE_OWN, PORT_TYPE_RECEIVE_OWN);

	init(MSG_TYPE_PORT_OWNERSHIP, PORT_TYPE_NONE, PORT_TYPE_OWN);
	init(MSG_TYPE_PORT_OWNERSHIP, PORT_TYPE_SEND, PORT_TYPE_OWN);
	init(MSG_TYPE_PORT_OWNERSHIP, PORT_TYPE_RECEIVE, PORT_TYPE_RECEIVE_OWN);
#else	MACH_IPC_XXXHACK
#if	NeXT
	init(MSG_TYPE_PORT_RECEIVE, PORT_TYPE_NONE, PORT_TYPE_RECEIVE_OWN);
	init(MSG_TYPE_PORT_RECEIVE, PORT_TYPE_SEND, PORT_TYPE_RECEIVE_OWN);
#endif	NeXT
#endif	MACH_IPC_XXXHACK

	init(MSG_TYPE_PORT_ALL, PORT_TYPE_NONE, PORT_TYPE_RECEIVE_OWN);
	init(MSG_TYPE_PORT_ALL, PORT_TYPE_SEND, PORT_TYPE_RECEIVE_OWN);

#undef	init
}

/*
 *	Routine:	object_copyin_init [internal]
 *	Purpose:
 *		Called to initialize object_copyin_table.
 */
void
object_copyin_init()
{
	int mt;
	port_type_t pt;

#define init(mt, pt, _illegal, _nodealloc, _dodealloc, _result, _func)	\
MACRO_BEGIN								\
	object_copyin_table[mt][pt].illegal = (_illegal);		\
	object_copyin_table[mt][pt].nodealloc = (_nodealloc);		\
	object_copyin_table[mt][pt].dodealloc = (_dodealloc);		\
	object_copyin_table[mt][pt].result = (_result);			\
	object_copyin_table[mt][pt].func = (_func);			\
MACRO_END

	for (mt = 0; mt < MSG_TYPE_LAST; mt++)
		for (pt = 0; pt < PORT_TYPE_LAST; pt++)
			init(mt, pt, TRUE, FALSE, FALSE, PORT_TYPE_NONE, 0);

	init(MSG_TYPE_PORT, PORT_TYPE_SEND,
	     FALSE, FALSE, FALSE,
	     PORT_TYPE_SEND, 0);

#if	MACH_IPC_XXXHACK
	init(MSG_TYPE_PORT, PORT_TYPE_RECEIVE,
	     FALSE, TRUE, FALSE,
	     PORT_TYPE_RECEIVE, 0);

	init(MSG_TYPE_PORT, PORT_TYPE_OWN,
	     FALSE, TRUE, FALSE,
	     PORT_TYPE_OWN, 0);
#endif	MACH_IPC_XXXHACK

	init(MSG_TYPE_PORT, PORT_TYPE_RECEIVE_OWN,
	     FALSE, TRUE, FALSE,
	     PORT_TYPE_RECEIVE_OWN, 0);

#if	MACH_IPC_XXXHACK
	init(MSG_TYPE_PORT_RECEIVE, PORT_TYPE_RECEIVE,
	     FALSE, FALSE, FALSE,
	     PORT_TYPE_SEND, port_copyin_receive);

	init(MSG_TYPE_PORT_RECEIVE, PORT_TYPE_RECEIVE_OWN,
	     FALSE, TRUE, FALSE,
	     PORT_TYPE_OWN, port_copyin_receive);

	init(MSG_TYPE_PORT_OWNERSHIP, PORT_TYPE_OWN,
	     FALSE, FALSE, FALSE,
	     PORT_TYPE_SEND, port_copyin_own);

	init(MSG_TYPE_PORT_OWNERSHIP, PORT_TYPE_RECEIVE_OWN,
	     FALSE, TRUE, FALSE,
	     PORT_TYPE_RECEIVE, port_copyin_own);
#else	MACH_IPC_XXXHACK
#if	NeXT
	init(MSG_TYPE_PORT_RECEIVE, PORT_TYPE_RECEIVE_OWN,
	     FALSE, FALSE, FALSE,
	     PORT_TYPE_SEND, port_copyin_receive_own);
#endif	NeXT
#endif	MACH_IPC_XXXHACK

	init(MSG_TYPE_PORT_ALL, PORT_TYPE_RECEIVE_OWN,
	     FALSE, FALSE, FALSE,
	     PORT_TYPE_SEND, port_copyin_receive_own);

#undef	init
}

/*
 *	Routine:	ipc_bootstrap [exported]
 *	Purpose:
 *		Initialize IPC structures needed even before
 *		the "kernel task" can be initialized
 */
void
ipc_bootstrap()
{
	int i;
	msg_size_t large_size;

	large_size = (MSG_SIZE_MAX +
		      (sizeof(struct kern_msg) - sizeof(msg_header_t)));

#if	NeXT
#else	NeXT
	kmsg_zone_large = zinit((vm_size_t) large_size,
		(vm_size_t) (kmsg_large_max_num * large_size),
		(vm_size_t) round_page(kmsg_large_alloc_num * large_size),
		FALSE, "large messages");
#endif	NeXT

	kmsg_zone = zinit((vm_size_t) KERN_MSG_SMALL_SIZE,
		(vm_size_t) (kmsg_max_num * KERN_MSG_SMALL_SIZE),
		(vm_size_t) round_page(kmsg_alloc_num * KERN_MSG_SMALL_SIZE),
		FALSE, "messages");

	port_hash_zone = zinit((vm_size_t) sizeof(struct port_hash),
		(vm_size_t) (port_hash_max_num * sizeof(struct port_hash)),
		(vm_size_t) round_page(port_hash_alloc_num *
				       sizeof(struct port_hash)),
		FALSE, "port translations");

	port_zone = zinit((vm_size_t) sizeof(struct kern_port),
		(vm_size_t) (port_max_num * sizeof(struct kern_port)),
		(vm_size_t) round_page(port_alloc_num *
				       sizeof(struct kern_port)),
		FALSE, "ports");
	zchange(port_zone, FALSE, FALSE, TRUE);	/* make it exhaustible */

	set_zone = zinit((vm_size_t) sizeof(struct kern_set),
		(vm_size_t) (set_max_num * sizeof(struct kern_set)),
		(vm_size_t) round_page(set_alloc_num *
				       sizeof(struct kern_set)),
		FALSE, "sets");
	zchange(set_zone, FALSE, FALSE, TRUE);	/* make it exhaustible */

	port_zone_reserved = zinit((vm_size_t) sizeof(struct kern_port),
		(vm_size_t) (port_reserved_max_num * sizeof(struct kern_port)),
		(vm_size_t) round_page(port_reserved_alloc_num *
				       sizeof(struct kern_port)),
		FALSE, "ports (reserved)");

#if	NeXT
	TP_table = (port_hash_bucket_t *) kmem_alloc(kernel_map,
		   (vm_size_t) (2 * PORT_HASH_COUNT * sizeof(port_hash_bucket_t)));
	ASSERT(TP_table != (port_hash_bucket_t *) 0);

	for (i = 0; i < 2*PORT_HASH_COUNT; i++) {
		queue_init(&TP_table[i].head);
		bucket_lock_init(&TP_table[i]);
	}

	TL_table = TP_table + PORT_HASH_COUNT;
#else	NeXT
	TP_table = (port_hash_bucket_t *) kmem_alloc(kernel_map,
		   (vm_size_t) (PORT_HASH_COUNT * sizeof(port_hash_bucket_t)));
	if (TP_table == (port_hash_bucket_t *) 0)
		panic("ipc_bootstrap: cannot create TP_table");

	for (i = 0; i < PORT_HASH_COUNT; i++) {
		queue_init(&TP_table[i].head);
		bucket_lock_init(&TP_table[i]);
	}

	TL_table = (port_hash_bucket_t *) kmem_alloc(kernel_map,
		   (vm_size_t) (PORT_HASH_COUNT * sizeof(port_hash_bucket_t)));
	if (TL_table == (port_hash_bucket_t *) 0)
		panic("ipc_bootstrap: cannot create TL_table");

	for (i = 0; i < PORT_HASH_COUNT; i++) {
		queue_init(&TL_table[i].head);
		bucket_lock_init(&TL_table[i]);
	}
#endif	NeXT

#if	MACH_IPC_STATS
	ipc_stats_init();
#endif	MACH_IPC_STATS

	object_copyin_init();
	object_copyout_init();
}

/*
 *	Routine:	ipc_init [exported]
 *	Purpose:
 *		Called to initialize remaining data structures before
 *		any user traps are handled.
 */
void
ipc_init()
{
	register notification_t *n;
	vm_offset_t min, max;
	extern int hz;

	/* Create a template for notification messages. */

	n = &notification_template;
	n->notify_header.msg_local_port = PORT_NULL;
	n->notify_header.msg_remote_port = PORT_NULL;
	n->notify_header.msg_simple = TRUE;
	n->notify_header.msg_type = MSG_TYPE_EMERGENCY;
	n->notify_header.msg_id = 0;
	n->notify_header.msg_size = sizeof(notification_t);

	n->notify_type.msg_type_name = MSG_TYPE_PORT_NAME;
	n->notify_type.msg_type_inline = TRUE;
	n->notify_type.msg_type_deallocate = FALSE;
	n->notify_type.msg_type_longform = FALSE;
	n->notify_type.msg_type_number = 1;
	n->notify_type.msg_type_size = 32;

	/* Create a template for complex_notification messages. */

	n = &complex_notification_template;
	n->notify_header.msg_local_port = PORT_NULL;
	n->notify_header.msg_remote_port = PORT_NULL;
	n->notify_header.msg_simple = FALSE;
	n->notify_header.msg_type = MSG_TYPE_EMERGENCY;
	n->notify_header.msg_id = 0;
	n->notify_header.msg_size = sizeof(notification_t);

	n->notify_type.msg_type_name = 0;
	n->notify_type.msg_type_inline = TRUE;
	n->notify_type.msg_type_deallocate = FALSE;
	n->notify_type.msg_type_longform = FALSE;
	n->notify_type.msg_type_number = 1;
	n->notify_type.msg_type_size = 32;

	/* Compute the timeout scaling factor. usecs per tick */

	timeout_scaling_factor = (1000000 / hz);

	/* Create a task used to hold rights and data in transit. */

	if (task_create(TASK_NULL /* kernel_task */, FALSE, &ipc_soft_task)
					!= KERN_SUCCESS)
		panic("ipc_init");

#if	MACH_OLD_VM_COPY
	ipc_soft_map = ipc_soft_task->map;

	ipc_soft_map->pmap = PMAP_NULL;
#endif	MACH_OLD_VM_COPY

	ipc_kernel_map = kmem_suballoc(kernel_map, &min, &max,
				       1024 * 1024, TRUE);

	kernel_task->ipc_privilege = TRUE;
	kernel_task->kernel_ipc_space = TRUE;

	ipc_host_init();
}






