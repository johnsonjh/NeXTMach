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
 * $Log:	thread_swap.h,v $
 * Revision 2.5  89/12/22  15:54:16  rpd
 * 	Add MAKE_UNSWAPPABLE flag.
 * 	[89/11/28            dlb]
 * 
 * Revision 2.4  89/03/09  20:17:07  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:10:24  gm0w
 * 	Kernel code cleanup.
 * 	Put entire file under #indef KERNEL.
 * 	[89/02/15            mrt]
 * 
 * 21-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 *	File:	kern/thread_swap.h
 *
 *	Declarations of thread swap_states and swapping routines.
 */

/*
 *	Swap states for threads.
 */

#ifndef	_KERN_THREAD_SWAP_H_
#define _KERN_THREAD_SWAP_H_

#define TH_SW_UNSWAPPABLE	1	/* not swappable */
#define TH_SW_IN		2	/* swapped in */
#define TH_SW_GOING_OUT		3	/* being swapped out */
#define TH_SW_WANT_IN		4	/* being swapped out, but should
					   immediately be swapped in */
#define TH_SW_OUT		5	/* swapped out */
#define TH_SW_COMING_IN		6	/* queued for swapin, or being
					   swapped in */
#define TH_SW_MAKE_UNSWAPPABLE	0x10	/* flag for WANT_IN, COMING_IN
					   to make thread unswappable */

#define TH_SW_STATE		0xf	/* Mask for state */

/*
 *	exported routines
 */
extern void	swapper_init();
extern void	thread_swapin( /* thread_t thread */ );
#if	NeXT
extern void	swapping_thread();
#else	NeXT
extern void	swapin_thread();
extern void	swapout_thread();
#endif	NeXT
extern void	swapout_threads();
extern void	thread_swappable( /* thread_t thread, boolean_t swappable */ );
extern void	thread_doswapin( /* thread_t thread */ );

#endif	_KERN_THREAD_SWAP_H_

