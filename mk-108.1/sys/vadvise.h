/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	vadvise.h,v $
 * Revision 2.3  89/03/09  22:10:08  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:58:14  gm0w
 * 	Changes for cleanup.
 * 
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)vadvise.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_VADVISE_H_
#define _SYS_VADVISE_H_

/*
 * Parameters to vadvise() to tell system of particular paging
 * behaviour:
 *	VA_NORM		Normal strategy
 *	VA_ANOM		Sampling page behaviour is not a win, don't bother
 *			Suitable during GCs in LISP, or sequential or random
 *			page referencing.
 *	VA_SEQL		Sequential behaviour expected.
 *	VA_FLUSH	Invalidate all page table entries.
 */
#define VA_NORM	0
#define VA_ANOM	1
#define VA_SEQL	2
#define VA_FLUSH 3

#endif	_SYS_VADVISE_H_
