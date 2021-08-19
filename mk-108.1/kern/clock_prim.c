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
 * 29-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Changed policy checks to use !FIXEDPRI instead of TIMESHARE so that
 *	POLICY_INTERACTIVE can be used.
 *
 * $Log:	clock_prim.c,v $
 * Revision 2.6  89/11/20  11:23:17  mja
 * 	Always set first_quantum to FALSE on quantum runout.
 * 	[89/11/15            dlb]
 * 	Put all fixed priority support under MACH_FIXPRI switch.
 * 	[89/11/10            dlb]
 * 
 * Revision 2.5  89/10/11  14:02:47  dlb
 * 	Priority depression support.
 * 	Add scheduling policy support.
 * 	Get quantum from thread for fixed priority.
 * 	Processor allocation changes -- almost everything moves into
 * 	       processor and processor_set structures.
 * 
 * Revision 2.4  89/03/09  20:11:03  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:00:27  gm0w
 * 	Changes for cleanup.
 * 
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TIME_NEW is now standard.
 *	Do ageing here on clock interrupts instead of in
 *	recompute_priorities.  Do accurate usage calculations.
 *
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Delete previous history.
 *
 */ 
/*
 *	File:	clock_prim.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr.
 *
 *	Clock primitives.
 */

#import <cpus.h>
#import <mach_fixpri.h>

#import <machine/cpu.h>
#import <sys/machine.h>
#import <kern/processor.h>
#import <kern/thread.h>
#import <kern/sched.h>

#if	MACH_FIXPRI
#import <sys/policy.h>
#endif	MACH_FIXPRI

#import <machine/machparam.h>

/*
 *	USAGE_THRESHOLD is the amount by which usage must change to
 *	cause a priority shift that moves a thread between run queues.
 */

#ifdef	PRI_SHIFT_2
#if	PRI_SHIFT_2 > 0
#define USAGE_THRESHOLD (((1 << PRI_SHIFT) + (1 << PRI_SHIFT_2)) << (2 + SCHED_SHIFT))
#else	PRI_SHIFT_2 > 0
#define USAGE_THRESHOLD (((1 << PRI_SHIFT) - (1 << -(PRI_SHIFT_2))) << (2 + SCHED_SHIFT))
#endif	PRI_SHIFT_2 > 0
#else	PRI_SHIFT_2
#define USAGE_THRESHOLD	(1 << (PRI_SHIFT + 2 + SCHED_SHIFT))
#endif	PRI_SHIFT_2

/*
 *	clock_tick:
 *
 *	Handle hardware clock ticks.  The number of ticks that has elapsed
 *	since we were last called is passed as "nticks."  Note that this
 *	is called for each processor that is taking clock interrupts, and
 *	that some processors may be running at different clock rates.
 *	However, all of these rates must be some multiple of the basic clock
 *	tick.
 *
 *	The state the processor was executing in is passed as "state."
 */

clock_tick(nticks, state)
	int		nticks;
	register int	state;
{
	int				mycpu;
	register thread_t		thread;
	register int			quantum;
	register processor_t		myprocessor;
#if	NCPUS > 1
	register processor_set_t	pset;
#endif	NCPUS > 1
	int				s;

	mycpu = cpu_number();		/* who am i? */
	myprocessor = cpu_to_processor(mycpu);
#if	NCPUS > 1
	pset = myprocessor->processor_set;
#endif	NCPUS > 1

	/*
	 *	Update the cpu ticks for this processor. XXX
	 */

	machine_slot[mycpu].cpu_ticks[state] += nticks;

	/*
	 *	Account for thread's utilization of these ticks.
	 *	This assumes that there is *always* a current thread.
	 *	When the processor is idle, it should be the idle thread.
	 */

	thread = current_thread();

	/*
	 *	Update set_quantum and calculate the current quantum.
	 */
#if	NCPUS > 1
	pset->set_quantum = pset->machine_quantum[
		((pset->runq.count > pset->processor_count) ?
		  pset->processor_count : pset->runq.count)];

	if (myprocessor->runq.count != 0)
		quantum = min_quantum;
	else
		quantum = pset->set_quantum;
#else	NCPUS > 1
	quantum = min_quantum;
	default_pset.set_quantum = quantum;
#endif	NCPUS > 1
		
	/*
	 *	Now recompute the priority of the thread if appropriate.
	 */

	if (state != CPU_STATE_IDLE) {
		myprocessor->quantum -= nticks;
#if	NCPUS > 1
		/*
		 *	Runtime quantum adjustment.  Use quantum_adj_index
		 *	to avoid synchronizing quantum expirations.
		 */
		if ((quantum != myprocessor->last_quantum) &&
		    (pset->processor_count > 1)) {
			myprocessor->last_quantum = quantum;
			simple_lock(&pset->quantum_adj_lock);
			quantum = min_quantum + (pset->quantum_adj_index *
				(quantum - min_quantum)) / 
					(pset->processor_count - 1);
			if (++(pset->quantum_adj_index) >=
			    pset->processor_count)
				pset->quantum_adj_index = 0;
			simple_unlock(&pset->quantum_adj_lock);
		}
#endif	NCPUS > 1
		if (myprocessor->quantum <= 0) {
			s = splsched();
			thread_lock(thread);
			if (thread->sched_stamp != sched_tick) {
				update_priority(thread);
			}
			else {
			    if (
#if	MACH_FIXPRI
				(thread->policy != POLICY_FIXEDPRI) &&
#endif	MACH_FIXPRI
				(thread->depress_priority < 0)) {
				    thread_timer_delta(thread);
				    thread->sched_usage +=
					thread->sched_delta;
				    thread->sched_delta = 0;
				    compute_my_priority(thread);
			    }
			}
			thread_unlock(thread);
			(void) splx(s);
			/*
			 *	This quantum is up, give this thread another.
			 */
			myprocessor->first_quantum = FALSE;
#if	MACH_FIXPRI
			if (thread->policy != POLICY_FIXEDPRI) {
#endif	MACH_FIXPRI
				myprocessor->quantum += quantum;
#if	MACH_FIXPRI
			}
			else {
				/*
				 *    Fixed priority has per-thread quantum.
				 *    
				 */
				myprocessor->quantum += thread->sched_data;
			}
#endif	MACH_FIXPRI
		}
		/*
		 *	Recompute priority if appropriate.
		 */
		else {
		    s = splsched();
		    thread_lock(thread);
		    if (thread->sched_stamp != sched_tick) {
			update_priority(thread);
		    }
		    else {
			if (
#if	MACH_FIXPRI
			    (thread->policy != POLICY_FIXEDPRI) &&
#endif	MACH_FIXPRI
			    (thread->depress_priority < 0)) {
				thread_timer_delta(thread);
				if (thread->sched_delta >= USAGE_THRESHOLD) {
				    thread->sched_usage +=
					thread->sched_delta;
				    thread->sched_delta = 0;
				    compute_my_priority(thread);
				}
			}
		    }
		    thread_unlock(thread);
		    (void) splx(s);
		}
		/*
		 * Check for and schedule ast if needed.
		 */
		ast_check();
	}
}


