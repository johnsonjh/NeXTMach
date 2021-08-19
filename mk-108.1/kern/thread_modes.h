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
 * $Log:	thread_modes.h,v $
 * Revision 2.4  89/03/09  20:16:48  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:10:05  gm0w
 * 	Changes for cleanup.
 * 
 *  3-Apr-87  David Black (dlb) at Carnegie-Mellon University
 *	Created by extracting thread mode defines from thread.h.
 *
 */ 

/*
 *	Maximum number of thread execution modes.
 */

#ifndef	_KERN_THREAD_MODES_H_
#define _KERN_THREAD_MODES_H_

#define THREAD_MAXMODES		2

/*
 *	Thread execution modes.
 */

#define THREAD_USERMODE		0
#define THREAD_SYSTEMMODE	1

#endif	_KERN_THREAD_MODES_H_
