/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	ttydev.h,v $
 * Revision 2.3  89/03/09  22:09:15  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:57:34  gm0w
 * 	Changes for cleanup.
 * 
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ttydev.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_TTYDEV_H_
#define _SYS_TTYDEV_H_

/*
 * Terminal definitions related to underlying hardware.
 */

/*
 * Speeds
 */
#define B0	0
#define B50	1
#define B75	2
#define B110	3
#define B134	4
#define B150	5
#define B200	6
#define B300	7
#define B600	8
#define B1200	9
#define B1800	10
#define B2400	11
#define B4800	12
#define B9600	13
#define EXTA	14
#define EXTB	15

#ifdef	KERNEL
/*
 * Hardware bits.
 * SHOULD NOT BE HERE.
 */
#define DONE	0200
#define IENABLE	0100

/*
 * Modem control commands.
 */
#define DMSET		0
#define DMBIS		1
#define DMBIC		2
#define DMGET		3

#endif	KERNEL
#endif	_SYS_TTYDEV_H_
