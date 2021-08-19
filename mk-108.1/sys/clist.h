/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	clist.h,v $
 * Revision 2.4  89/03/09  22:02:42  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  17:52:17  gm0w
 * 	Got rid of conditional on CMUCS and the code that
 * 	was defined for non-CMUCS case
 * 	[89/02/13            mrt]
 * 
 * Revision 2.2  88/10/11  10:23:32  rpd
 * 	Made declarations be extern.
 * 	[88/10/06  07:55:03  rpd]
 * 
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)clist.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_CLIST_H_
#define _SYS_CLIST_H_

#import <sys/param.h>		/* for CBSIZE */

/*
 * Raw structures for the character list routines.
 */
struct cblock {
	struct cblock *c_next;
	char	c_info[CBSIZE];
};

#ifdef	KERNEL

extern struct cblock *cfree;
extern int nclist;
extern struct cblock *cfreelist;
extern int cfreecount;

#endif	KERNEL
#endif	_SYS_CLIST_H_
