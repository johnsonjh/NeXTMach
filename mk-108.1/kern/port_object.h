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
 * $Log:	port_object.h,v $
 * Revision 2.6  89/10/11  14:19:31  dlb
 * 	Added monitor object for kernel monitoring (MACH_KM).
 * 	[89/04/08            tfl]
 * 
 * 	Add host/processor/pset port objects.
 * 	[88/10/29            dlb]
 * 
 * Revision 2.5  89/03/09  20:14:57  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:07:36  gm0w
 * 	Kernel code cleanup.
 * 	Put entire file under #indef KERNEL.
 * 	[89/02/15            mrt]
 * 
 * Revision 2.3  89/02/07  01:03:36  mwyoung
 * Relocated from sys/port_object.h
 * 
 * Revision 2.2  88/08/24  02:38:50  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:19:51  mwyoung]
 * 
 *  2-Jun-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Created.
 */
/*
 * Port "kernel object" declarations
 *
 */

#ifndef	_KERN_PORT_OBJECT_H_
#define _KERN_PORT_OBJECT_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_net.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

typedef	enum {
		PORT_OBJECT_NONE,
#if	MACH_NET
		PORT_OBJECT_NET,
#endif	MACH_NET
		PORT_OBJECT_TASK,
		PORT_OBJECT_THREAD,
		PORT_OBJECT_PAGING_REQUEST,
		PORT_OBJECT_PAGER,
		PORT_OBJECT_HOST,
		PORT_OBJECT_HOST_PRIV,
		PORT_OBJECT_PROCESSOR,
		PORT_OBJECT_PSET,
		PORT_OBJECT_PSET_NAME,
} port_object_type_t;

typedef struct {
		port_object_type_t kp_type;
		int		kp_object;
} port_object_t;

#endif	_KERN_PORT_OBJECT_H_

