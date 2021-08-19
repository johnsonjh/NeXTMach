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
 * $Log:	resource.h,v $
 * 27-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	Sun Bugfixes: 1009246 - Comments on some fields were wrong.
 *
 * Revision 2.7  89/03/09  22:07:10  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  17:55:54  gm0w
 * 	Made CMUCS conditionals always true.
 * 	[89/02/14            mrt]
 * 
 * Revision 2.5  88/10/11  12:08:31  rpd
 * 	Added include of time.h since this file uses 
 * 	struct timeval.
 * 	[88/10/11  11:41:22  rpd]
 * 
 * Revision 2.4  88/08/24  02:41:41  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:21:30  mwyoung]
 * 
 * Revision 2.3  88/08/22  21:29:44  mja
 * 	Add RUSAGE_NODEV definition.
 * 	[88/08/11  19:18:02  mja]
 * 
 * Revision 2.2  88/07/15  15:59:57  mja
 * Condensed local conditionals.
 * 
 * 06-Jan-88  Jay Kistler (jjk) at Carnegie Mellon University
 *	Made file reentrant.  Added declarations for __STDC__.
 *
 * 28-Jan-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	CMUCS: Defined new rusage_dev structure for special
 *	wait3() option;
 *	CMUCS: Added RPAUSE definitions.
 *	[ V5.1(F1) ]
 *
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)resource.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_RESOURCE_H_
#define _SYS_RESOURCE_H_

#ifdef	ASSEMBLER
#else	ASSEMBLER
#import <sys/time.h>
#import <sys/types.h>

/*
 * Process priority specifications to get/setpriority.
 */
#define PRIO_MIN	-20
#define PRIO_MAX	20

#define PRIO_PROCESS	0
#define PRIO_PGRP	1
#define PRIO_USER	2

/*
 * Resource utilization information.
 */

#define RUSAGE_SELF	0
#define RUSAGE_CHILDREN	-1

struct	rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
	long	ru_maxrss;
#define ru_first	ru_ixrss
	long	ru_ixrss;		/* XXX: 0 */
	long	ru_idrss;		/* XXX: sum of rm_asrss */
	long	ru_isrss;		/* XXX: 0 */
	long	ru_minflt;		/* any page faults not requiring I/O */
	long	ru_majflt;		/* any page faults requiring I/O */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
#define ru_last		ru_nivcsw
};

/*
 * Resource limits
 */
#define RLIMIT_CPU	0		/* cpu time in milliseconds */
#define RLIMIT_FSIZE	1		/* maximum file size */
#define RLIMIT_DATA	2		/* data size */
#define RLIMIT_STACK	3		/* stack size */
#define RLIMIT_CORE	4		/* core file size */
#define RLIMIT_RSS	5		/* resident set size */

#define RLIM_NLIMITS	6		/* number of resource limits */

#define RLIM_INFINITY	0x7fffffff

struct rlimit {
	int	rlim_cur;		/* current (soft) limit */
	int	rlim_max;		/* maximum value for rlim_cur */
};


/*
 *  Special rusage structure returned with WLOGINDEV option to wait3().
 */

struct rusage_dev {
	struct rusage ru_rusage;
	dev_t	      ru_dev;
};

#define RUSAGE_NODEV	((dev_t)-1)	/* same as NODEV */


/*
 *  Resource pause system call definitions
 */

#define RPAUSE_SAME	0		/* leave state unchanged */
#define RPAUSE_DISABLE	1		/* disable pause on error type(s) */
#define RPAUSE_ENABLE	2		/* enable pause on error type(s) */

#define RPAUSE_ALL	0x7fffffff	/* all error number types */


#if	defined(__STDC__) && !defined(KERNEL)
extern int getpriority(int, int);
extern int setpriority(int, int, int);
extern int getrlimit(int, struct rlimit *);
extern int setrlimit(int, struct rlimit *);
extern int getrusage(int, struct rusage *);
#endif	defined(__STDC__) && !defined(KERNEL)

#endif	ASSEMBLER
#endif	_SYS_RESOURCE_H_


