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
 * $Log:	ipc_kmesg.h,v $
 * 17-Jul-90  Avadis Tevanian, Jr. (avie) at NeXT
 *	Removed kern_msg_allocation routines from here, recoded and relocated
 *	to ipc_basics.c.
 *
 * Revision 2.11  89/06/27  00:23:50  rpd
 * 	Initialize kernel_message in kern_msg_allocate_*.
 * 	[89/06/26  23:51:19  rpd]
 * 
 * Revision 2.10  89/03/09  20:11:58  rpd
 * 	More cleanup.
 * 
 * Revision 2.9  89/02/25  18:02:29  gm0w
 * 	Kernel code cleanup.	
 * 	Put entire file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.8  89/01/12  07:55:07  rpd
 * 	Moved ipc_statistics.h to mach_debug/.
 * 	[89/01/12  06:43:11  rpd]
 * 
 * Revision 2.7  88/12/19  02:44:02  mwyoung
 * 	Consider the lint implications of that last change.
 * 	[88/12/08            mwyoung]
 * 	
 * 	Only allow the fast_pager_data optimization when the message is
 * 	destined for the kernel.
 * 	[88/12/02            mwyoung]
 * 
 * Revision 2.6  88/10/18  03:20:06  mwyoung
 * 	Use <kern/macro_help.h> to avoid lint.
 * 	[88/10/15            mwyoung]
 * 
 * Revision 2.5  88/10/01  21:55:11  rpd
 * 	Changed FAST_PAGER_DATA to MACH_XP_FPD.
 * 	[88/09/29  01:07:49  rpd]
 * 
 * Revision 2.4  88/09/25  22:11:13  rpd
 * 	Revamped kern_msg_destroy and the FAST_PAGER_DATA definitions.
 * 	[88/09/24  17:59:53  rpd]
 * 	
 * 	Changed kern_msg_destroy to use msg_destroy.
 * 	[88/09/21  00:46:54  rpd]
 * 	
 * 	Changed includes to the new style.
 * 	[88/09/19  16:13:34  rpd]
 * 
 * Revision 2.3  88/08/06  18:15:41  rpd
 * Moved move_msg_data macro here (for lack of a better place),
 * from ipc_basics.c.
 * 
 * Revision 2.2  88/07/22  07:25:16  rpd
 * Created for kern_msg macros.
 * 
 */ 

#ifndef	_KERN_IPC_KMESG_H_
#define _KERN_IPC_KMESG_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_xp_fpd.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/boolean.h>
#import <sys/port.h>
#import <sys/message.h>
#import <kern/ipc_statistics.h>
#import <kern/task.h>
#import <kern/kern_msg.h>
#import <kern/zalloc.h>
#import <kern/ipc_globals.h>
#import <kern/ipc_copyout.h>
#import <kern/macro_help.h>

/*
 *	Kernel message allocation
 *
 *	The IPC system allocates messages in one of two sizes: small or large.
 *	Other kernel subsystems may allocate messages on their own; the
 *	IPC system will return a message to its "home_zone" once it has been
 *	dequeued.
 *
 *	The internal allocation routines are noticably macros: their
 *	argument is the by-reference result of the allocation.
 *
 *	To destroy a message involves deallocating the resources
 *	associated with it; this is the normal case.
 */

#if	NeXT
kern_msg_t	kern_msg_allocate(int);
void		kern_msg_free(kern_msg_t);
#else	NeXT
#define kern_msg_allocate_small(kmsg)			\
MACRO_BEGIN						\
	ZALLOC(kmsg_zone, (kmsg), kern_msg_t); 		\
	(kmsg)->home_zone = kmsg_zone; 			\
	(kmsg)->kernel_message = FALSE;			\
	ipc_event(current); 				\
MACRO_END

#define kern_msg_allocate_large(kmsg)			\
MACRO_BEGIN						\
	ZALLOC(kmsg_zone_large, (kmsg), kern_msg_t);	\
	(kmsg)->home_zone = kmsg_zone_large; 		\
	(kmsg)->kernel_message = FALSE;			\
	ipc_event(current); 				\
MACRO_END

#define kern_msg_free(kmsg)				\
MACRO_BEGIN						\
	ZFREE(kmsg->home_zone, (vm_offset_t) kmsg);	\
	ipc_event_count(current, -1);			\
MACRO_END
#endif	NeXT

#define kern_msg_destroy(kmsgptr)					     \
MACRO_BEGIN							     	     \
	if ((kmsgptr)->kmsg_header.msg_simple) {			     \
		register kern_port_t port;				     \
									     \
		port = (kern_port_t) (kmsgptr)->kmsg_header.msg_remote_port; \
		if (port != KERN_PORT_NULL)				     \
			port_release_macro(port);			     \
									     \
		port = (kern_port_t) (kmsgptr)->kmsg_header.msg_local_port;  \
		if (port != KERN_PORT_NULL)				     \
			port_release_macro(port);			     \
									     \
		kern_msg_free(kmsgptr); 				     \
	} else 								     \
		msg_destroy(kmsgptr);					     \
MACRO_END

/*
 * 	Macro:	move_msg_data
 *	
 *	Purpose:
 *		Determine if a move from user to kernel space or kernel
 *		to user space is needed for data of a particular type
 *		in a particular message.
 *
 *	Rationale:
 *		It is faster to sometimes avoid the extra copy into the
 *		kernel when we know the kernel will just re-copy the data
 *		anyway.  This is a sleazy hack for now.  A more general
 *		purpose solution must follow.
 */

#ifdef	lint
int	_MACH_XP_FPD_;
#else	lint
#define _MACH_XP_FPD_	MACH_XP_FPD
#endif	lint

#define fast_pager_data(kmsg)				\
	(_MACH_XP_FPD_ &&				\
	 ((kmsg)->kmsg_header.msg_id == 2038) &&	\
	 (kmsg)->kernel_message)

#define move_msg_data(kmsgptr, tn) 			\
	(! fast_pager_data(kmsgptr) &&			\
	 (tn != MSG_TYPE_INTERNAL_MEMORY))

#endif	_KERN_IPC_KMESG_H_

