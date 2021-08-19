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
 * $Log:	timer.h,v $
 * Revision 2.9  89/12/22  15:54:21  rpd
 * 	Minor stylistic change to TIMER_DELTA, plus explanatory comment.
 * 	[89/11/28            dlb]
 * 
 * Revision 2.8  89/10/11  14:35:23  dlb
 * 	Change internal representation of timers: flags field replaced
 * 	by high_bits_check field.
 * 	[89/09/12            dlb]
 * 
 * 	Added TIMER_LOW_FULL mask to check for possible overflow of
 * 	low_bits field.  Removed TIMER_SERVICE flag.
 * 	[88/10/27            dlb]
 * 
 * Revision 2.7  89/03/09  20:17:32  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  18:10:52  gm0w
 * 	Kernel code cleanup.
 * 	Put entire file under #indef KERNEL.
 * 	[89/02/15            mrt]
 * 
 * Revision 2.5  89/02/07  01:05:51  mwyoung
 * Relocated from sys/timer.h
 * 
 * Revision 2.4  89/01/30  22:08:37  rpd
 * 	Updated macro definitions to the new style.
 * 	Made declarations use "extern".
 * 	[89/01/25  15:24:36  rpd]
 * 
 * Revision 2.3  88/08/24  02:49:17  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:25:43  mwyoung]
 *
 *  7-Apr-88  David Black (dlb) at Carnegie-Mellon University
 *	Added interface definitions and null routines for STAT_TIME.
 *
 * 19-Feb-88  David Black (dlb) at Carnegie-Mellon University
 *	Renamed fields, added timer_save structure.
 *
 * 31-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	machine/machtimer.h -> machine/timer.h
 *
 * 23-Feb-87  David L. Black (dlb) at Carnegie-Mellon University
 *	Created.
 */ 

#ifndef	_KERN_TIMER_H_
#define _KERN_TIMER_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cpus.h>
#import <stat_time.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <kern/macro_help.h>

#if	STAT_TIME
/*
 *	Statistical timer definitions - use microseconds in timer, seconds
 *	in high unit field.  No adjustment needed to convert to time_value_t
 *	as a result.  Service timers once an hour.
 */

#define TIMER_RATE	1000000
#define TIMER_HIGH_UNIT	TIMER_RATE
#undef	TIMER_ADJUST

#else	STAT_TIME
/*
 *	Machine dependent definitions based on hardware support.
 */

#import <machine/timer.h>

#endif	STAT_TIME

/*
 *	Definitions for accurate timers.  high_bits_check is a copy of
 *	high_bits that allows reader to verify that values read are ok.
 */

struct timer {
	unsigned	low_bits;
	unsigned	high_bits;
	unsigned	high_bits_check;
	unsigned	tstamp;
};

typedef struct timer		timer_data_t;
typedef	struct timer		*timer_t;

/*
 *	Mask to check if low_bits is in danger of overflowing
 */

#define	TIMER_LOW_FULL	0x80000000

/*
 *	Kernel timers and current timer array.  [Exported]
 */

extern timer_t		current_timer[NCPUS];
extern timer_data_t	kernel_timer[NCPUS];

/*
 *	save structure for timer readings.  This is used to save timer
 *	readings for elapsed time computations.
 */

struct timer_save {
	unsigned	low;
	unsigned	high;
};

typedef struct timer_save	timer_save_data_t, *timer_save_t;

/*
 *	Exported kernel interface to timers
 */

#if	STAT_TIME
#define start_timer(timer)
#define timer_switch(timer)
#else	STAT_TIME
extern void	start_timer();
extern void	timer_switch();
#endif	STAT_TIME

extern void		timer_read();
extern void		thread_read_times();
extern unsigned		timer_delta();

#if	STAT_TIME
/*
 *	Macro to bump timer values.
 */	
#define timer_bump(timer, usec)					\
MACRO_BEGIN							\
	(timer)->low_bits += usec;				\
	if ((timer)->low_bits & TIMER_LOW_FULL) {		\
		timer_normalize(timer);				\
	}							\
MACRO_END

#else	STAT_TIME
/*
 *	Exported hardware interface to timers
 */
extern void	time_trap_uentry();
extern void	time_trap_uexit();
extern timer_t	time_int_entry();
extern void	time_int_exit();
#endif	STAT_TIME

/*
 *	TIMER_DELTA finds the difference between a timer and a saved value,
 *	and updates the saved value.  Look at high_bits check field after
 *	reading low because that's the first written by a normalize
 *	operation; this isn't necessary for current usage because
 *	this macro is only used when the timer can't be normalized:
 *	thread is not running, or running thread calls it on itself at
 *	splsched().
 */

#define TIMER_DELTA(timer, save, result)			\
MACRO_BEGIN							\
	register unsigned	temp;				\
								\
	temp = (timer).low_bits;				\
	if ((save).high != (timer).high_bits_check) {		\
		result += timer_delta(&(timer), &(save));	\
	}							\
	else {							\
		result += temp - (save).low;			\
		(save).low = temp;				\
	}							\
MACRO_END

#endif	_KERN_TIMER_H_

