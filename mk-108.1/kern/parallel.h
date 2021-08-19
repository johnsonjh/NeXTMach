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
 * $Log:	parallel.h,v $
 * Revision 2.3  89/03/09  20:14:51  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  18:07:31  gm0w
 * 	Kernel code cleanup.
 * 	Put entire file under #indef KERNEL.
 * 	[89/02/15            mrt]
 * 
 *  9-Oct-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Define unix_reset for longjmp/setjmp reset.
 *
 * 21-Sep-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Created.
 *
 */

#ifndef	_KERN_PARALLEL_H_
#define _KERN_PARALLEL_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cpus.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#if	NCPUS > 1

#define unix_master()  _unix_master()
#define unix_release() _unix_release()
#define unix_reset()   _unix_reset()
extern void _unix_master(), _unix_release(), _unix_reset();

#else	NCPUS > 1

#define unix_master()
#define unix_release()
#define unix_reset()

#endif	NCPUS > 1

#endif	_KERN_PARALLEL_H_
