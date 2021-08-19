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
 * $Log:	ipc_signal.h,v $
 * Revision 2.7  89/03/09  20:12:50  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  18:04:09  gm0w
 * 	Changes for cleanup.
 * 
 * 15-Feb-89  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Kernel code cleanup.	
 *	Put entire file under #ifdef KERNEL
 *
 * Revision 2.5  88/12/20  13:52:22  rpd
 * 	Added MACH_IPC_SIGHACK.  When it isn't enabled,
 * 	the grody PSIGNAL macro disappears.
 * 	[88/11/26  21:32:17  rpd]
 * 
 * Revision 2.4  88/10/18  03:20:58  mwyoung
 * 	Use <kern/macro_help.h> to avoid lint.
 * 	[88/10/15            mwyoung]
 * 
 * Revision 2.3  88/09/25  22:13:46  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:23:46  rpd]
 * 
 * Revision 2.2  88/07/22  07:31:41  rpd
 * Created for PSIGNAL nonsense.
 * 
 */ 

#ifndef	_KERN_IPC_SIGNAL_H_
#define _KERN_IPC_SIGNAL_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_ipc_sighack.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#if	MACH_IPC_SIGHACK

#import <sys/boolean.h>
#import <kern/task.h>
#import <kern/macro_help.h>
#import <sys/signal.h>
#import <sys/proc.h>

#define PSIGNAL(task, emerg) \
	MACRO_BEGIN						\
	if ((task)->ipc_intr_msg)				\
		psignal(&proc[(task)->proc_index],		\
			(emerg) ? SIGEMSG : SIGMSG);		\
	MACRO_END

#else	MACH_IPC_SIGHACK

#define PSIGNAL(task, emerg)

#endif	MACH_IPC_SIGHACK

#endif	_KERN_IPC_SIGNAL_H_

