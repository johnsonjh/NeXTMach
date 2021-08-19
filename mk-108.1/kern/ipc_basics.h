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
 * $Log:	ipc_basics.h,v $
 * Revision 2.9  89/05/01  15:24:17  rpd
 * 	Updated to reflect current kern/ipc_basics.c.
 * 	[89/05/01  14:29:33  rpd]
 * 
 * Revision 2.8  89/03/09  20:11:18  rpd
 * 	More cleanup.
 * 
 * Revision 2.7  89/02/25  18:01:26  gm0w
 * 	Put whole file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.6  88/10/11  10:11:23  rpd
 * 	Added send_complex_notification.
 * 	[88/10/11  07:56:51  rpd]
 * 
 * Revision 2.5  88/09/25  22:09:33  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:07:42  rpd]
 * 
 * Revision 2.4  88/08/06  18:11:49  rpd
 * Moved declarations to ipc_copyin.h, ipc_copyout.h, ipc_ptraps.h.
 * 
 * Revision 2.3  88/07/29  03:19:01  rpd
 * Declared new function object_copyin_from_kernel().
 * 
 * Revision 2.2  88/07/22  07:22:23  rpd
 * Created to contain declarations of ipc_basics.c functions.
 * 
 */ 

#ifndef	_KERN_IPC_BASICS_H_
#define _KERN_IPC_BASICS_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_ipc_xxxhack.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <kern/kern_msg.h>
#import <sys/message.h>

extern void send_notification();
extern void send_complex_notification();

extern kern_msg_t mach_msg();
extern msg_return_t msg_queue();

extern msg_return_t msg_send();
extern msg_return_t msg_send_from_kernel();
extern msg_return_t msg_receive();
extern msg_return_t msg_rpc();

extern msg_return_t msg_send_trap();
extern msg_return_t msg_receive_trap();
extern msg_return_t msg_rpc_trap();

#if	MACH_IPC_XXXHACK
extern msg_return_t msg_send_old();
extern msg_return_t msg_receive_old();
extern msg_return_t msg_rpc_old();
#endif	MACH_IPC_XXXHACK

#endif	_KERN_IPC_BASICS_H_

