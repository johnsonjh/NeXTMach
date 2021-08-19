/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	timeb.h,v $
 * Revision 2.3  89/03/09  22:08:39  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:57:07  gm0w
 * 	Changes for cleanup.
 * 
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)timeb.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_TIMEB_H_
#define _SYS_TIMEB_H_

#import <sys/types.h>

/*
 * Structure returned by ftime system call
 */
struct timeb
{
	time_t	time;
	unsigned short millitm;
	short	timezone;
	short	dstflag;
};

#endif	_SYS_TIMEB_H_
