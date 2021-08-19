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
 * $Log:	sched_prim.h,v $
 * 19-Apr-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: Declared thread_wait_result() proecedure.
 *
 * Revision 2.9  89/10/11  14:25:14  dlb
 * 	thread_switch --> thread_run.
 * 	[89/09/01  17:41:25  dlb]
 * 
 * Revision 2.8  89/10/03  19:26:08  rpd
 * 	Defined thread_wakeup, thread_wakeup_with_result, and
 * 	thread_wakeup_one using thread_wakeup_prim.
 * 	[89/09/01  01:30:53  rpd]
 * 
 * Revision 2.7  89/03/09  20:15:35  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/26  16:18:04  mrt
 * 	changed #ifdef	SCHED_PRIM_H_ to #ifndef
 * 
 * Revision 2.5  89/02/25  18:08:14  gm0w
 * 	Kernel code cleanup.
 * 	Put entire file under #indef KERNEL.
 * 	[89/02/15            mrt]
 * 
 * Revision 2.4  89/02/07  01:04:15  mwyoung
 * Relocated from sys/sched_prim.h
 * 
 * Revision 2.3  88/07/17  18:57:09  mwyoung
 * Added thread_wakeup_with_result routine; thread_wakeup
 * is a special case.
 *
 * 16-May-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added thread_wakeup_with_result routine; thread_wakeup
 *	is a special case.
 *
 * 16-Apr-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added THREAD_RESTART wait result value.
 *
 * 29-Feb-88  David Black (dlb) at Carnegie-Mellon University
 *	thread_setrun is now a real routine.
 *
 * 13-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Moved thread_will_wait and thread_go to sched_prim_macros.h,
 *	to avoid including thread.h everywhere.
 *
 *  5-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Created.  Moved thread_will_wait and thread_go here from
 *	mach_ipc.
 *
 */
/*
 *	File:	sched_prim.h
 *	Author:	David Golub
 *
 *	Scheduling primitive definitions file
 *
 */

#ifndef	_KERN_SCHED_PRIM_H_
#define _KERN_SCHED_PRIM_H_

/*
 *	Possible results of assert_wait - returned in
 *	current_thread()->wait_result.
 */
#define THREAD_AWAKENED		0		/* normal wakeup */
#define THREAD_TIMED_OUT	1		/* timeout expired */
#define THREAD_INTERRUPTED	2		/* interrupted by clear_wait */
#define THREAD_SHOULD_TERMINATE	3		/* thread should terminate */
#define THREAD_RESTART		4		/* restart operation entirely */

/*
 *	Exported interface to sched_prim.c 
 */

extern void	sched_init();
extern void	assert_wait();
extern void	clear_wait();
extern void	thread_sleep();
extern void	thread_wakeup();		/* for function pointers */
extern void	thread_wakeup_prim();
extern void	thread_block();
extern void	thread_run();
extern void	thread_set_timeout();
extern void	thread_setrun();

#if	NeXT
extern int	thread_wait_result();
#endif	NeXT

/*
 *	Routines defined as macros
 */

#define thread_wakeup(x)						\
		thread_wakeup_prim((x), FALSE, THREAD_AWAKENED)
#define thread_wakeup_with_result(x, z)					\
		thread_wakeup_prim((x), FALSE, (z))
#define thread_wakeup_one(x)						\
		thread_wakeup_prim((x), TRUE, THREAD_AWAKENED)

#endif	_KERN_SCHED_PRIM_H_