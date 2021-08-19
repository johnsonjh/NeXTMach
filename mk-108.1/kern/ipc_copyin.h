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
 * $Log:	ipc_copyin.h,v $
 * Revision 2.7  89/03/09  20:11:33  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/03/05  16:46:01  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK.
 * 	[89/02/16  13:44:18  rpd]
 * 
 * Revision 2.5  89/02/25  18:01:47  gm0w
 * 	Put entire file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.4  89/01/10  23:27:58  rpd
 * 	Removed port_copyin, port_copyin_fast.
 * 	[89/01/09  14:44:21  rpd]
 * 
 * Revision 2.3  88/09/25  22:10:15  rpd
 * 	Added object_copyin_cache, removed msg_copyin_from_kernel.
 * 	[88/09/24  17:54:06  rpd]
 * 	
 * 	Added declaration of msg_copyin_from_kernel.
 * 	[88/09/21  00:42:56  rpd]
 * 	
 * 	Changed includes to the new style.
 * 	[88/09/19  16:10:58  rpd]
 * 
 * Revision 2.2  88/08/06  18:13:22  rpd
 * Created.
 * 
 */ 

#ifndef	_KERN_IPC_COPYIN_H_
#define _KERN_IPC_COPYIN_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_ipc_xxxhack.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/boolean.h>
#import <sys/message.h>

extern void port_clear_receiver();

#if	MACH_IPC_XXXHACK
extern void port_copyin_receive();
extern void port_copyin_own();
#endif	MACH_IPC_XXXHACK
extern void port_copyin_receive_own();

extern boolean_t object_copyin();
extern boolean_t object_copyin_from_kernel();
extern boolean_t object_copyin_cache();

extern msg_return_t msg_copyin();

#endif	_KERN_IPC_COPYIN_H_

