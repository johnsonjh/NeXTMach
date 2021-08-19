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
 * $Log:	kern_port.h,v $
 * Revision 2.12  89/12/22  16:28:14  rpd
 * 	Declared port_lookup() call.
 * 	[89/11/24  15:06:05  rpd]
 * 
 * Revision 2.11  89/03/09  20:13:27  rpd
 * 	More cleanup.
 * 
 * Revision 2.10  89/03/05  16:48:09  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK.
 * 	[89/02/16            rpd]
 * 
 * Revision 2.9  89/02/25  18:05:09  gm0w
 * 	Kernel code cleanup.
 * 	Put entire file under #indef KERNEL.
 * 	[89/02/15            mrt]
 * 
 * Revision 2.8  89/02/07  01:02:16  mwyoung
 * Relocated from sys/kern_port.h
 * 
 * Revision 2.7  88/10/11  10:24:31  rpd
 * 	Added port_backup field to the port structure.
 * 	[88/10/10  07:59:13  rpd]
 * 
 * Revision 2.6  88/08/24  02:31:17  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:14:48  mwyoung]
 * 
 * Revision 2.5  88/08/06  19:20:59  rpd
 * Added macro forms of port_reference, port_release.
 * Added declarations of port_alloc(), port_destroy(), port_dealloc().
 * 
 * Revision 2.4  88/07/29  03:21:22  rpd
 * Fixed include of sys/features.h.
 * 
 * Revision 2.3  88/07/20  16:47:50  rpd
 * Removed port_translations field (now part of port_obj).
 * Added port_* versions of the obj macros.
 * Use kern_obj for common fields.
 * Declare port_reference, port_release.
 * Changes for port sets.  Also rename receiver_name as port_receiver_name
 * and external as port_external.
 * Remove port_name field.  Ports no longer have a global external name.
 * 
 * 20-Jun-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Include "kern/zalloc.h" to get zone_t definition; we've been
 *	sleazing it from other include files so far.
 *
 * 14-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Removed port_spl (since IPC is no longer done from interrupts).
 *	Fixed conditionals on MACH_NP.
 *
 *  9-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Added receiver_name for quick local_port translation on
 *	msg_receive.
 *
 *  1-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added kern_port_t->port_home_zone field.
 *
 * 12-Jul-87  Rick Rashid (rfr) at Carnegie-Mellon University
 *	Added declarations for handling external IPC events under the
 *	MACH_NP conditional.
 *
 *  2-Jun-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Moved port object declarations elsewhere; condensed history.
 *
 */
/*
 * Kernel internal structure associated with a port.
 *
 */

#ifndef	_KERN_KERN_PORT_H_
#define _KERN_KERN_PORT_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_np.h>
#import <mach_ipc_xxxhack.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/port.h>
#import <kern/kern_obj.h>
#import <kern/task.h>
#import <kern/msg_queue.h>
#import <kern/queue.h>
#import <kern/port_object.h>
#import <kern/kern_set.h>

typedef struct kern_port {
	struct kern_obj port_obj;

	task_t		port_receiver;
				/* Task holding receive rights */
	port_name_t	port_receiver_name;
				/* Receiver's local name for port */
#if	MACH_IPC_XXXHACK
	task_t		port_owner;
				/* Task holding ownership */
#endif	MACH_IPC_XXXHACK
	struct kern_port *port_backup;
				/* "Send rights" to a backup port */

	int		port_message_count;
				/* Optimistic number of queued messages */
	int		port_backlog;
				/* Queue limit before blocking */
	msg_queue_t	port_messages;
				/* Queued messages, if not in set */
	queue_chain_t	port_blocked_threads;
				/* Senders waiting to complete */

	port_object_t	port_object;
				/* Kernel object I represent */

	kern_set_t	port_set;
				/* The set I belong to (else NULL) */
	queue_chain_t	port_brothers;
				/* List of all members of that set */

#if	MACH_NP
	int **		port_external;
				/* External info array */
#endif	MACH_NP
} port_data_t, *kern_port_t;

#define port_data_lock		port_obj.obj_data_lock
#define port_in_use		port_obj.obj_in_use
#define port_references		port_obj.obj_references
#define port_home_zone		port_obj.obj_home_zone
#define port_translations	port_obj.obj_translations

#define		KERN_PORT_NULL	((kern_port_t) 0)

#define port_lock(port)		obj_lock(&(port)->port_obj)
#define port_lock_try(port)	obj_lock_try(&(port)->port_obj)
#define port_unlock(port)	obj_unlock(&(port)->port_obj)
#define port_check_unlock(port)	obj_check_unlock(&(port)->port_obj)
#define port_free(port)		obj_free(&(port)->port_obj)

#define port_reference_macro(port)	obj_reference(&(port)->port_obj)
#define port_release_macro(port)	obj_release(&(port)->port_obj)

extern void port_reference();
extern void port_release();
extern kern_return_t port_alloc();
extern void port_destroy();
extern kern_return_t port_dealloc();
extern kern_port_t port_lookup();

#if	MACH_NP
/*
 *	Routine pointer offsets for external to IPC port handling.
 *	(e.g., MACH_NP).
 */
#define MSG_QUEUE_OFF		2
#define PORT_CHANGED_OFF	3
#define PORT_DEAD_OFF		4
#endif	MACH_NP

#endif	_KERN_KERN_PORT_H_

