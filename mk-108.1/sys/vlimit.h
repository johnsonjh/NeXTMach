/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	vlimit.h,v $
 * Revision 2.3  89/03/09  22:10:43  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:58:44  gm0w
 * 	Changes for cleanup.
 * 
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)vlimit.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_VLIMIT_H_
#define _SYS_VLIMIT_H_

/*
 * Limits for u.u_limit[i], per process, inherited.
 */

#define LIM_NORAISE	0	/* if <> 0, can't raise limits */
#define LIM_CPU		1	/* max secs cpu time */
#define LIM_FSIZE	2	/* max size of file created */
#define LIM_DATA	3	/* max growth of data space */
#define LIM_STACK	4	/* max growth of stack */
#define LIM_CORE	5	/* max size of ``core'' file */
#define LIM_MAXRSS	6	/* max desired data+stack core usage */

#define NLIMITS		6

#define INFINITY	0x7fffffff

#endif	_SYS_VLIMIT_H_
