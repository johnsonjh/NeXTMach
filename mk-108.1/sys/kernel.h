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
 * $Log:	kernel.h,v $
 * Revision 2.8  89/04/22  15:31:57  gm0w
 * 	Removed MACH_VFS code.  Changed domainname size to MAXDOMNAMELEN.
 * 	[89/04/14            gm0w]
 * 
 * Revision 2.7  89/03/09  22:05:23  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  17:54:31  gm0w
 * 	Removed MACH conditionals. Put entire file under
 * 	an #ifdef KERNEL conditioanal.
 * 	[89/02/13            mrt]
 * 
 * Revision 2.5  89/01/18  01:16:41  jsb
 * 	Vnode support: declare domainname{,len}.
 * 	[89/01/13            jsb]
 * 
 * Revision 2.4  88/08/24  02:32:40  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:15:39  mwyoung]
 *
 *  5-Feb-88  Joseph Boykin (boykin) at ENcore Computer Corporation
 *	Added include of 'time.h'.
 *
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Use MACH conditional.
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kernel.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_KERNEL_H_
#define _SYS_KERNEL_H_

#ifdef	KERNEL
#import <sys/types.h>
#import <sys/time.h>
#import <sys/param.h>		/* for MAXHOSTNAMELEN */

/*
 * Global variables for the kernel
 */

extern long	rmalloc();

/* 1.1 */
extern long	hostid;
extern char	hostname[MAXHOSTNAMELEN];
extern int	hostnamelen;
extern char	domainname[MAXDOMNAMELEN];
extern int	domainnamelen;

/* 1.2 */
extern struct	timeval boottime;
extern struct	timeval time;
#if	NeXT
/* do not support timezone in kernel */
#else	NeXT
extern struct	timezone tz;		/* XXX */
#endif	NeXT
extern int	hz;
extern int	phz;			/* alternate clock's frequency */
extern int	tick;
extern int	lbolt;			/* awoken once a second */
extern int	realitexpire();

#define LSCALE	1000		/* scaling for "fixed point" arithmetic */
extern	long	avenrun[3];
extern	long	mach_factor[3];

#ifdef	GPROF
extern	int profiling;
extern	char *s_lowpc;
extern	u_long s_textsize;
extern	u_short *kcount;
#endif	GPROF

#endif	KERNEL
#endif	_SYS_KERNEL_H_


