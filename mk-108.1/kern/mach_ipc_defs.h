/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * Declarations for things exported from "mach_ipc.c"
 *
 * HISTORY
 * 19-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added port_copyin() declaration.
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Removed interrupt message processing calls.
 *
 * 18-Oct-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added interrupt message processing calls; concept by Rick Rashid.
 *
 *  6-Apr-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Coalesce history.
 */

#ifndef	_MACH_IPC_DEFS_
#define	_MACH_IPC_DEFS_	1

#import <vm/vm_map.h>
#import <sys/kern_msg.h>
#import <sys/port_object.h>
#import <sys/message.h>
#import <sys/boolean.h>

void		ipc_bootstrap();
void		ipc_init();

void		ipc_task_init();
void		ipc_thread_init();
void		ipc_task_terminate();
void		ipc_thread_terminate();

void		port_object_set();
int		port_object_get();
port_object_type_t port_object_type();

#ifndef		kern_msg_allocate
kern_msg_t	kern_msg_allocate();
#endif		kern_msg_allocate

#ifndef		kern_msg_free
void		kern_msg_free();
#endif		kern_msg_free

msg_return_t	msg_queue();
boolean_t	msg_queue_hint();

void		port_reference();
void		port_release();

void		port_copyout();
void		port_copyin();

extern
vm_map_t	ipc_soft_map;
extern
vm_map_t	ipc_kernel_map;

#endif	_MACH_IPC_DEFS_
