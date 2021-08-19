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
 * $Log:	sched.h,v $
 * 14-Nov-90  Gregg Kellogg (gk) at NeXT
 *	Fixed bug in csw_needed which caused it to return true any time
 *	there was something in the run queue.
 *
 * 20-Aug-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: Re-wrote csw_needed as an inline to fix the INTERACTIVE case
 *	and make the code easier to understand.
 *
 * 18-May-90  Avadis Tevanian (avie) at NeXT
 *	Changed to use sensible priorities (higher numbers -> higher pri).
 *
 * Revision 2.11  89/11/20  11:23:53  mja
 * 	New version of csw_needed for MACH_FIXPRI.
 * 	[89/11/15            dlb]
 * 
 * Revision 2.10  89/10/11  14:21:01  dlb
 * 	Change scheduler priorities to 0-31 from 0-127.
 * 	[89/05/11            dlb]
 * 
 * 	Moved sched_load into processor set structure.
 * 	[89/02/09            dlb]
 * 
 * 	Make csw_needed a macro here.  Ignore first_quantum for local_runq.
 * 	No more runrun.
 * 	[88/08/11            dlb]
 * 		
 * 
 * Revision 2.9  89/03/09  20:15:11  rpd
 * 	More cleanup.
 * 
 * Revision 2.8  89/02/25  18:07:55  gm0w
 * 	Kernel code cleanup.
 * 	Put entire file under #indef KERNEL.
 * 	[89/02/15            mrt]
 * 
 * Revision 2.7  89/02/07  01:04:03  mwyoung
 * Relocated from sys/sched.h
 * 
 * Revision 2.6  89/01/30  22:08:24  rpd
 * 	Made variable declarations use "extern".
 * 	[89/01/25  15:23:09  rpd]
 * 
 * Revision 2.5  89/01/15  16:34:43  rpd
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  15:17:50  rpd]
 * 
 * Revision 2.4  88/12/19  02:51:43  mwyoung
 * 	Use <kern/macro_help.h> to avoid lint.
 * 	[88/12/08            mwyoung]
 * 
 * Revision 2.3  88/08/24  02:42:44  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:22:04  mwyoung]
 *
 * 29-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	SIMPLE_CLOCK: added sched_usec for drift compensation.
 *
 * 25-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	Added sched_load and related constants.  Moved thread_timer_delta
 *	here because it depends on sched_load.
 *
 * 19-Feb-88  David Black (dlb) at Carnegie-Mellon University
 *	Added sched_tick and shift definitions for more flexible ageing.
 *
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Removed conditionals, purged history.
 */
/*
 *	File:	sched.h
 *	Author:	Avadis Tevanian, Jr.
 *	Copyright (C) 1985, Avadis Tevanian, Jr.
 *
 *	Header file for scheduler.
 *
 */

#ifndef	_KERN_SCHED_H_
#define _KERN_SCHED_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cpus.h>
#import <mach_fixpri.h>
#import <simple_clock.h>
#import <stat_time.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <kern/queue.h>
#import <kern/lock.h>
#import <kern/macro_help.h>

#if	MACH_FIXPRI
#import <sys/policy.h>
#endif	MACH_FIXPRI

#if	STAT_TIME

/*
 *	Statistical timing uses microseconds as timer units.  18 bit shift
 *	yields priorities.  PRI_SHIFT_2 isn't needed.
 */
#define PRI_SHIFT	18

#else	STAT_TIME

/*
 *	Otherwise machine provides shift(s) based on time units it uses.
 */
#import <machine/sched_param.h>

#endif	STAT_TIME
#define NRQS	32			/* 32 run queues per cpu */

struct run_queue {
	queue_head_t		runq[NRQS];	/* one for each priority */
	decl_simple_lock_data(,	lock)		/* one lock for all queues */
#if	NeXT
	int			high;		/* high queue value */
#else	NeXT
	int			low;		/* low queue value */
#endif	NeXT
	int			count;		/* count of threads runable */
};

typedef struct run_queue	*run_queue_t;
#define RUN_QUEUE_NULL	((run_queue_t) 0)

#if	NeXT
#define csw_needed(thread, processor) \
	_csw_needed( \
		((thread)->state & TH_SUSP), \
		(processor)->runq.count, \
		(processor)->processor_set->runq.count, \
		(processor)->processor_set->runq.high, \
		(processor)->first_quantum, \
		(thread)->sched_pri, \
		(thread)->policy)

static inline boolean_t _csw_needed(
	int susp,
	int p_rq_count,
	int ps_rq_count,
	int rq_high,
	int first_quantum,
	int sched_pri,
	int policy)
{
	/*
	 * I think this is still broken.  We should also check
	 * priority or to see if this thread is also bound before
	 * checking for runnable bound processes (p_rq_count > 0).
	 */
	if (susp || p_rq_count > 0)
		return TRUE;

	switch (policy) {
	case POLICY_FIXEDPRI:
	case POLICY_INTERACTIVE:
	default:
		/*
		 * Don't preempt if we're not doing anything to preempt,
		 * or the interrupting thread is of lower priority.
		 */
		if (ps_rq_count == 0 || rq_high < sched_pri)
			break;
		/*
		 * Preempt if the new thread is of strictly higher priority
		 * or (of equal priority) and the running thread's used
		 * up its first quantum allowance.
		 */
		if (rq_high > sched_pri || first_quantum == FALSE)
			return TRUE;
		break;
	case POLICY_TIMESHARE:
		/*
		 * Always let the running thread use it's first quantum up
		 * if the preempting thread is timesharing, otherwise
		 * preempt the running thread if this thread's of equal
		 * or greater priority.
		 */
		if (   first_quantum == FALSE
		    && ps_rq_count > 0
		    && rq_high >= sched_pri)
		    	return TRUE;
		break;
	}
	return FALSE;
}
#else	NeXT
#if	MACH_FIXPRI
/*
 *	NOTE: For fixed priority threads, first_quantum indicates
 *	whether context switch at same priority is ok.  For timeshareing
 *	it indicates whether preempt is ok.
 */

#define csw_needed(thread, processor) ((thread)->state & TH_SUSP ||	\
	((processor)->runq.count > 0) ||				\
	((thread)->policy == POLICY_TIMESHARE &&			\
		(processor)->first_quantum == FALSE &&			\
		(processor)->processor_set->runq.count > 0 &&		\
		  (processor)->processor_set->runq.low <=		\
			(thread)->sched_pri) ||				\
	((thread)->policy == POLICY_FIXEDPRI &&				\
		(processor)->processor_set->runq.count > 0 &&		\
		 ((((processor)->first_quantum == FALSE) &&		\
		  ((processor)->processor_set->runq.low <=		\
			(thread)->sched_pri)) ||			\
		 ((processor)->processor_set->runq.low <		\
			(thread)->sched_pri))))

#else	MACH_FIXPRI
#define csw_needed(thread, processor) ((thread)->state & TH_SUSP ||	\
		((processor)->runq.count > 0) ||			\
		((processor)->first_quantum == FALSE &&			\
		 ((processor)->processor_set->runq.count > 0 &&		\
		  (processor)->processor_set->runq.low <=		\
			((thread)->sched_pri))))
#endif	MACH_FIXPRI
#endif	NeXT

/*
 *	Scheduler routines.
 */

extern struct run_queue	*rem_runq();
extern struct thread	*choose_thread();
extern queue_head_t	action_queue;	/* assign/shutdown queue */
decl_simple_lock_data(extern,action_lock);

extern int		min_quantum;	/* defines max context switch rate */

/*
 *	Default base priorities for threads.
 */
#if	NeXT
#define BASEPRI_SYSTEM	24
#define MAXPRI_USER	18
#define BASEPRI_USER	10
#else	NeXT
#define BASEPRI_SYSTEM	6
#define BASEPRI_USER	12
#endif	NeXT

/*
 *	Macro to check for invalid priorities.
 */

#define invalid_pri(pri) (((pri) < 0) || ((pri) >= NRQS))

/*
 *	Shift structures for holding update shifts.  Actual computation
 *	is  usage = (usage >> shift1) +/- (usage >> abs(shift2))  where the
 *	+/- is determined by the sign of shift 2.
 */
struct shift {
	int	shift1;
	int	shift2;
};

typedef	struct shift	*shift_t, shift_data_t;

/*
 *	sched_tick increments once a second.  Used to age priorities.
 */

extern unsigned	sched_tick;

#define SCHED_SCALE	128
#define SCHED_SHIFT	7

/*
 *	thread_timer_delta macro takes care of both thread timers.
 */

#define thread_timer_delta(thread)  				\
MACRO_BEGIN							\
	register unsigned	delta;				\
								\
	delta = 0;						\
	TIMER_DELTA((thread)->system_timer,			\
		(thread)->system_timer_save, delta);		\
	TIMER_DELTA((thread)->user_timer,			\
		(thread)->user_timer_save, delta);		\
	(thread)->cpu_delta += delta;				\
	(thread)->sched_delta += delta * 			\
			(thread)->processor_set->sched_load;	\
MACRO_END

#if	SIMPLE_CLOCK
/*
 *	sched_usec is an exponential average of number of microseconds
 *	in a second for clock drift compensation.
 */

extern int	sched_usec;
#endif	SIMPLE_CLOCK

#endif	_KERN_SCHED_H_