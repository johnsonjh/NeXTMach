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
 * $Log:	ipc_globals.h,v $
 * 20-Feb-90  Gregg Kellogg (gk) at NeXT
 *	MACH_OLD_VM_COPY: need ipc_soft_map back.
 *
 * Revision 2.9  89/10/15  02:04:24  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.8  89/03/09  20:11:45  rpd
 * 	More cleanup.
 * 
 * Revision 2.7  89/02/25  18:02:16  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.6  88/11/23  16:38:30  rpd
 * 	Changed mach_ipc_debug to ipc_debug.
 * 	Added IPC_DEBUG_* defines.
 * 	[88/11/23  10:39:48  rpd]
 * 
 * Revision 2.5  88/10/11  10:14:42  rpd
 * 	Added complex_notification_template.
 * 	[88/10/11  07:58:37  rpd]
 * 
 * Revision 2.4  88/09/25  22:10:50  rpd
 * 	Changed to new-style includes.
 * 	[88/09/09  18:30:37  rpd]
 * 
 * Revision 2.3  88/08/06  18:15:14  rpd
 * Declared ipc_bootstrap(), ipc_init().
 * 
 * Revision 2.2  88/07/22  07:24:01  rpd
 * Created for declarations of Mach IPC global variables.
 * 
 */ 

#ifndef	_KERN_IPC_GLOBALS_H_
#define _KERN_IPC_GLOBALS_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_old_vm_copy.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/boolean.h>
#import <sys/port.h>
#import <sys/notify.h>
#import <kern/zalloc.h>
#import <kern/task.h>
#import <kern/ipc_hash.h>
#import <vm/vm_map.h>

typedef struct object_copyout_table {
	void (*destroy)(/* kern_obj_t obj */);
	int (*func)(/* task_t task, kern_obj_t obj, port_name_t name */);
	boolean_t nomerge;
	port_type_t result[PORT_TYPE_LAST];
} object_copyout_table_t;

typedef struct object_copyin_table {
	boolean_t illegal;
	boolean_t nodealloc;
	boolean_t dodealloc;
	port_name_t result;
	void (*func)(/* task_t task, kern_obj_t obj */);
} object_copyin_table_t;

extern zone_t kmsg_zone;
extern zone_t kmsg_zone_large;

extern zone_t port_zone;
extern zone_t port_zone_reserved;

extern zone_t set_zone;

extern task_t ipc_soft_task;
#if	!defined(MACH_OLD_VM_COPY) || MACH_OLD_VM_COPY
extern vm_map_t ipc_soft_map;
#endif	!defined(MACH_OLD_VM_COPY) || MACH_OLD_VM_COPY
extern vm_map_t ipc_kernel_map;

#define IPC_DEBUG_BOGUS_KMSG	0x00000001
#define IPC_DEBUG_SEND_INT	0x00000002
#define IPC_DEBUG_1K_PORTS	0x00000004
#define IPC_DEBUG_KPORT_DIED	0x00000008
#define IPC_DEBUG_SET_REFS	0x00000010
#define IPC_DEBUG_PORT_REFS	0x00000020

extern unsigned int ipc_debug;

extern zone_t port_hash_zone;

extern port_hash_bucket_t *TP_table;
extern port_hash_bucket_t *TL_table;

extern notification_t notification_template;
extern notification_t complex_notification_template;

extern object_copyout_table_t
object_copyout_table[MSG_TYPE_LAST];

extern object_copyin_table_t
object_copyin_table[MSG_TYPE_LAST][PORT_TYPE_LAST];

extern unsigned int timeout_minimum;
extern unsigned int timeout_scaling_factor;

extern void ipc_bootstrap();
extern void ipc_init();

#endif	_KERN_IPC_GLOBALS_H_


