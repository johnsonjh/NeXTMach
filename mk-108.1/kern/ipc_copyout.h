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
 * $Log:	ipc_copyout.h,v $
 * Revision 2.8  89/05/01  16:59:56  rpd
 * 	Removed object_copyout_cache.
 * 	[89/05/01  14:35:44  rpd]
 * 
 * Revision 2.7  89/03/09  20:11:39  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/03/05  16:46:31  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK.
 * 	[89/02/16  13:45:33  rpd]
 * 
 * Revision 2.5  89/02/25  18:02:02  gm0w
 * 	Put entire file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.4  89/01/10  23:28:42  rpd
 * 	Removed port_copyout.
 * 	[89/01/09  14:45:57  rpd]
 * 
 * Revision 2.3  88/09/25  22:10:39  rpd
 * 	Added object_copyout_cache.  Changed msg_destroy to void.
 * 	[88/09/24  17:58:35  rpd]
 * 	
 * 	Changed name of object_delete to object_destroy.
 * 	Added declaration of msg_destroy.
 * 	[88/09/21  00:45:41  rpd]
 * 	
 * 	Changed includes to the new style.
 * 	[88/09/19  16:11:55  rpd]
 * 
 * Revision 2.2  88/08/06  18:14:17  rpd
 * Created.
 * 
 */ 

#ifndef	_KERN_IPC_COPYOUT_H_
#define _KERN_IPC_COPYOUT_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_ipc_xxxhack.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/message.h>

#if	MACH_IPC_XXXHACK
extern void port_destroy_receive();
extern void port_destroy_own();
#endif	MACH_IPC_XXXHACK
extern void port_destroy_receive_own();

#if	MACH_IPC_XXXHACK
extern int port_copyout_receive();
extern int port_copyout_own();
#endif	MACH_IPC_XXXHACK
extern int port_copyout_receive_own();

extern void object_destroy();
extern void object_copyout();

extern msg_return_t msg_copyout();
extern void msg_destroy();

#endif	_KERN_IPC_COPYOUT_H_

