/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	dk.h,v $
 * Revision 2.4  89/05/30  10:42:51  rvb
 * 	No floats for mips kernels.
 * 	[89/04/20            af]
 * 
 * Revision 2.3  89/03/09  22:03:17  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:52:44  gm0w
 * 	Changes for cleanup.
 * 
 * 10-Mar-88  John Seamons (jks) at NeXT
 *	Added dk_bps (bytes per second) in order to avoid using
 *	the floating point dk_mspw (milliseconds per word).
 *
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)dk.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_DK_H_
#define _SYS_DK_H_

/*
 * Instrumentation
 */

#define CPUSTATES	4

#define CP_USER		0
#define CP_NICE		1
#define CP_SYS		2
#define CP_IDLE		3

#define DK_NDRIVE	4

#ifdef	KERNEL
extern long	cp_time[CPUSTATES];
extern int	dk_ndrive;
extern int	dk_busy;
extern long	dk_time[DK_NDRIVE];
extern long	dk_seek[DK_NDRIVE];
extern long	dk_xfer[DK_NDRIVE];
extern long	dk_wds[DK_NDRIVE];
#if	NeXT
extern long	dk_bps[DK_NDRIVE];
#endif	NeXT
#ifdef	mips
extern int	dk_mspw[DK_NDRIVE];
#else	mips
extern float	dk_mspw[DK_NDRIVE];
#endif	mips

extern long	tk_nin;
extern long	tk_nout;
#endif	KERNEL
#endif	_SYS_DK_H_

