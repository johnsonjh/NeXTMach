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
 * $Log:	ipc_tt.h,v $
 * Revision 2.6  89/10/11  14:09:39  dlb
 * 	Added monitor declarations.
 * 	[89/07/25            dlb]
 * 
 * Revision 2.5  89/03/09  20:12:58  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:04:29  gm0w
 * 	Kernel code cleanup.	
 * 	Put entire file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.3  88/09/25  22:14:32  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:25:46  rpd]
 * 
 * Revision 2.2  88/08/06  18:21:33  rpd
 * Created.
 * 
 */ 

#ifndef	_KERN_IPC_TT_H_
#define _KERN_IPC_TT_H_

#import <sys/boolean.h>

extern void ipc_task_init();
extern void ipc_task_enable();
extern void ipc_task_disable();
extern void ipc_task_terminate();

extern void ipc_thread_init();
extern void ipc_thread_enable();
extern void ipc_thread_disable();
extern void ipc_thread_terminate();

extern boolean_t task_secure();

#endif	_KERN_IPC_TT_H_


