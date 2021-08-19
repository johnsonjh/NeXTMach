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
 * $Log:	ipc_ptraps.h,v $
 * Revision 2.5  89/03/09  20:12:46  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:04:04  gm0w
 * 	Kernel code cleanup.	
 * 	Put entire file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.3  88/09/25  22:13:35  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:22:54  rpd]
 * 
 * Revision 2.2  88/08/06  18:20:38  rpd
 * Created.
 * 
 */ 

#ifndef	_KERN_IPC_PTRAPS_H_
#define _KERN_IPC_PTRAPS_H_

#import <sys/port.h>

extern port_name_t task_self();
extern port_name_t task_notify();

extern port_name_t thread_self();
extern port_name_t thread_reply();

#endif	_KERN_IPC_PTRAPS_H_

