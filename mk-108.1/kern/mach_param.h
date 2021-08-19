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
 * $Log:	mach_param.h,v $
 *  7-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Moved PORT_BACKLOG_DEFAULT and PORT_BACKLOG_MAX from kern/mach_param.h to sys/port.h
 *
 * Revision 2.9  89/03/10  01:59:14  rpd
 * 	Moved TASK_MAX, PORT_MAX, etc. here from mach/mach_param.h.
 * 
 * Revision 2.8  89/03/10  01:29:57  rpd
 * 	More cleanup.
 * 
 * 07-Apr-88  John Seamons (jks) at NeXT
 *	removed TASK_CHUNK and THREAD_CHUNK
 *
 */
/*
 *	File:	kern/mach_param.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Mach system sizing parameters
 *
 */

#ifndef	_KERN_MACH_PARAM_H_
#define _KERN_MACH_PARAM_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_np.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#if	NeXT
#define TASK_PORT_REGISTER_MAX	4	/* Number of "registered" ports */
#else	NeXT
#import <mach/mach_param.h>		/* for backwards compatibility */
#endif
#if	NeXT
#define THREAD_MAX	512		/* Max number of threads */
#define TASK_MAX	512		/* Max number of tasks */
#else	NeXT
#define THREAD_MAX	1024		/* Max number of threads */
#define TASK_MAX	1024		/* Max number of tasks */
#endif	NeXT

#define PORT_MAX	((TASK_MAX * 3 + THREAD_MAX)	/* kernel */ \
				+ (THREAD_MAX * 2)	/* user */ \
				+ 20000)		/* slop for objects */
					/* Number of ports, system-wide */

#define SET_MAX		(TASK_MAX + THREAD_MAX + 200)
					/* Max number of port sets */

#if	MACH_NP
#define KERN_MSG_SMALL_SIZE	256	/* Size of small kernel message */
#else	MACH_NP
#define KERN_MSG_SMALL_SIZE	128	/* Size of small kernel message */
#endif	MACH_NP

#endif	_KERN_MACH_PARAM_H_

