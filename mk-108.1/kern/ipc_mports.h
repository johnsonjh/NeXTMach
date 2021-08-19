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
 * $Log:	ipc_mports.h,v $
 * Revision 2.5  89/03/09  20:12:04  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:02:58  gm0w
 * 	Kernel code cleanup.	
 * 	Put entire file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.3  88/09/25  22:11:48  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:16:00  rpd]
 * 
 * Revision 2.2  88/07/22  07:26:26  rpd
 * Created for declarations of mach_ports_* functions.
 * 
 */ 

#ifndef	_KERN_IPC_MPORTS_H_
#define _KERN_IPC_MPORTS_H_

#import <sys/kern_return.h>

extern kern_return_t mach_ports_register();
extern kern_return_t mach_ports_lookup();

#endif	_KERN_IPC_MPORTS_H_

