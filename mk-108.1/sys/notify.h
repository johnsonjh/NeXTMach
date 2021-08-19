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
 * $Log:	notify.h,v $
 * Revision 2.7  89/03/09  20:22:23  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/03/05  16:48:54  rpd
 * 	Moved ownership rights under MACH_IPC_XXXHACK (when KERNEL).
 * 	[89/02/16            rpd]
 * 
 * Revision 2.5  89/02/25  18:39:31  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.4  89/02/07  00:52:49  mwyoung
 * Relocated from sys/notify.h
 * 
 * Revision 2.3  88/10/11  10:25:29  rpd
 * 	Added NOTIFY_PORT_DESTROYED.
 * 	[88/10/11  08:05:35  rpd]
 * 
 * Revision 2.2  88/08/24  02:37:53  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:19:20  mwyoung]
 * 
 *
 * 21-Nov-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added notification message structure definition.
 *
 * 01-Jul-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Created, based on Accent values.
 */
/*
 *	Kernel notification message definitions.
 *
 */

#ifndef	_SYS_NOTIFY_H_
#define _SYS_NOTIFY_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_ipc_xxxhack.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/message.h>


/*
 *	Notifications sent upon interesting system events.
 */

#define NOTIFY_FIRST			0100
#define NOTIFY_PORT_DELETED		( NOTIFY_FIRST + 001 )
#define NOTIFY_MSG_ACCEPTED		( NOTIFY_FIRST + 002 )
#if	!defined(KERNEL) || MACH_IPC_XXXHACK
#define NOTIFY_OWNERSHIP_RIGHTS		( NOTIFY_FIRST + 003 )
#define NOTIFY_RECEIVE_RIGHTS		( NOTIFY_FIRST + 004 )
#endif	!defined(KERNEL) || MACH_IPC_XXXHACK
#define NOTIFY_PORT_DESTROYED		( NOTIFY_FIRST + 005 )
#define NOTIFY_LAST			( NOTIFY_FIRST + 015 )

typedef struct {
	msg_header_t	notify_header;
	msg_type_t	notify_type;
	port_t		notify_port;
} notification_t;

#endif	_SYS_NOTIFY_H_


