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
 * $Log:	slave.c,v $
 * Revision 2.7  89/10/11  14:25:45  dlb
 * 	Eliminated should_exit.
 * 	[89/01/25            dlb]
 * 
 * 08-Jun-89  Mike DeMoney (mike) at NeXT
 *	Modified usec_elapsed(void) to be more general routine
 *	usec_elapsed(struct usec_mark *).  The argument maintains
 *	time of previous call.  The new usec_elapsed() is used in numerous
 *	places (slave_hardclock, eventc).
 *
 * Revision 2.6  89/05/30  10:38:16  rvb
 * 	No psl.h for mips.
 * 	[89/04/20            af]
 * 
 * Revision 2.5  89/03/09  20:15:57  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:08:35  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.3  88/12/19  02:46:51  mwyoung
 * 	Fix include file references.
 * 	[88/12/19  00:16:52  mwyoung]
 * 	
 * 	Remove lint.
 * 	[88/12/09            mwyoung]
 * 
 * 12-May-88  David Golub (dbg) at Carnegie-Mellon University
 *	Eliminated updates to various proc structure fields that are not
 *	used under MACH:  p_rssize, p_cpticks.
 *
 *  3-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	No more utime and stime timers in u.u_ru.  Rewrites and
 *	optimizations to slave_hardclock.
 *
 * 19-Feb-88  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TIME_NEW: initialize kernel timer on startup to eliminate
 *	possible (but highly unlikely) restart race in timer code.
 *	Optimized slave_hardclock.
 *
 * 24-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Eliminated more conditionals and fixed include file names.
 *
 * 19-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminated many conditionals.
 *
 * 14-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 14-Jul-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	intctl.h is necessary to define SPL0 for the sequent
 *
 * 25-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	Implemented SIMPLE_CLOCK for machines (mmax) w/o rollover timers.
 *	Moved startrtclock() call for slaves to slave_start() to avoid
 *	race.  Moved first reference of cpu_number() after slave_config().
 *	Removed balance include of machine/intctl.h.
 *
 * 24-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	Added profiling support for slaves.
 *
 * 23-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TT: ast check now done in clock_prim.c
 *
 * 28-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Call cpu_up when processor starts up.  Moved some VAX specific
 *	code to vax/autoconf.c.
 *
 *  7-Apr-87  David Black (dlb) at Carnegie-Mellon University
 *	SLOW_CLOCK: do ast check every time.
 *
 *  3-Mar-87  David L. Black (dlb) at Carnegie-Mellon University
 *	MACH_TIME_NEW support
 *
 *  5-Feb-87  David L. Black (dlb) at Carnegie-Mellon University
 *	MULTIMAX: tseinit renamed startclock.
 *
 * 31-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Support for MACH_TT.
 *
 * 19-Jan-87  David L. Black (dlb) at Carnegie-Mellon University
 *	NEW_SCHED: cpu_idle changes, keep runrun for slaves, and
 *	call ast_check() for ast scheduling.
 *
 * 08-Jan-87  Robert Beck (beck) at Sequent Computer Systems, Inc.
 *	Condionally (BALANCE) include ../machine/intctl.h (for SPL0 def).
 *	slave_main() now doesn't do printfs for BALANCE or MULTIMAX.
 *
 * 17-Dec-86  David Golub (dbg) at Carnegie-Mellon University
 *	Removed uses of text structure for MACH_VM.
 *
 *  9-Nov-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 30-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Replace slave_proc array with new cpu_idle array.
 *
 * 20-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	Added include of cputypes.h to pick up new MULTIMAX definition.
 *
 * 17-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	Added support for slave clocks on Multimax.
 *
 *  7-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	Merged in Multimax changes.
 *
 * 20-May-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Check for current_thread being non-null when deciding whether or
 *	not to reschedule.
 *
 * 28-Feb-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Conditionalized use of mtpr to be off when ROMP is on.  Made
 *	include of ../vax/{cpu,clock}.h into ../machine/{cpu,clock}.h.
 *
 *  8-Apr-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 *	File:	slave.c
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr.
 *
 *	Misc. slave routines.
 */

#import <cpus.h>
#import <simple_clock.h>

#import <machine/reg.h>
#ifdef	ibmrt
#import <ca/scr.h>
#endif	ibmrt
#if	!defined(ibmrt) && !defined(mips)
#import <machine/psl.h>
#endif	!defined(ibmrt) && !defined(mips)

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dk.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/proc.h>
#import <machine/cpu.h>
#import <machine/clock.h>
#ifdef	vax
#import <vax/mtpr.h>
#endif	vax

#import <kern/timer.h>

#import <kern/sched.h>
#import <kern/thread.h>
#import <sys/machine.h>

#ifdef	balance
#import <machine/intctl.h>
#endif	balance


slave_main()
{
	slave_config();
	cpu_up(cpu_number());

	timer_init(&kernel_timer[cpu_number()]);
	start_timer(&kernel_timer[cpu_number()]);

	slave_start();
	/*NOTREACHED*/
}

slave_hardclock(pc, ps)
	caddr_t	pc;
	int	ps;
{
	register struct proc 	*p;
	register int 		cpstate;
	register struct utask 	*utaskp;
	thread_t	 	thread;

#if	SIMPLE_CLOCK
#define tick	myticks
	register int myticks;
#if	NeXT
	static struct usec_mark slaveclock_usec_mark[NCPUS];
#endif	NeXT

	/*
	 *	Simple hardware timer does not restart on overflow, hence
	 *	interrupts do not happen at a constant rate.  Must call
	 *	machine-dependent routine to find out how much time has
	 *	elapsed since last interrupt.
	 */
#if	NeXT
	myticks = usec_elapsed(&slaveclock_usec_mark[cpu_number()]);
#else	NeXT
	myticks = usec_elapsed();
#endif	NeXT
#endif	SIMPLE_CLOCK
#ifdef	lint
	pc++;
#endif	lint

	thread = current_thread();
	utaskp = thread->u_address.utask;
	p = utaskp->uu_procp;

	/*
	 * Charge the time out based on the mode the cpu is in.
	 * Here again we fudge for the lack of proper interval timers
	 * assuming that the current state has been around at least
	 * one tick.
	 */
	if (USERMODE(ps)) {
		/*
		 * CPU was in user state.  Increment
		 * user time counter, and process process-virtual time
		 * interval timer. 
		 */
		if (timerisset(&utaskp->uu_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&utaskp->uu_timer[ITIMER_VIRTUAL], tick) == 0)
			psignal(p, SIGVTALRM);
		if (p->p_nice > NZERO)
			cpstate = CP_NICE;
		else
			cpstate = CP_USER;

		/*
		 *	Profiling check.
		 */
		if (utaskp->uu_prof.pr_scale) {
			p->p_flag |= SOWEUPC;
			aston();
		}

	} else {
		/*
		 * CPU was in system state.  If profiling kernel
		 * increment a counter.  If no process is running
		 * then this is a system tick if we were running
		 * at a non-zero IPL (in a driver).  If a process is running,
		 * then we charge it with system time even if we were
		 * at a non-zero IPL, since the system often runs
		 * this way during processing of system calls.
		 * This is approximate, but the lack of true interval
		 * timers makes doing anything else difficult.
		 */
		cpstate = CP_SYS;
		if ((thread->state & TH_IDLE) && BASEPRI(ps)) {
			cpstate = CP_IDLE;
		}
	}

	/*
	 * If the cpu is currently scheduled to a process, then
	 * charge it with resource utilization for a tick, updating
	 * statistics which run in (user+system) virtual time,
	 * such as the cpu time limit and profiling timers.
	 * This assumes that the current process has been running
	 * the entire last tick.
	 */
	if (!(thread->state & TH_IDLE)) {
	    if (utaskp->uu_rlimit[RLIMIT_CPU].rlim_cur != RLIM_INFINITY) {
		time_value_t	sys_time, user_time;

		thread_read_times(thread, &user_time, &sys_time);
		if ((sys_time.seconds + user_time.seconds + 1) >
		    utaskp->uu_rlimit[RLIMIT_CPU].rlim_cur) {
			psignal(p, SIGXCPU);
			if (utaskp->uu_rlimit[RLIMIT_CPU].rlim_cur <
			    utaskp->uu_rlimit[RLIMIT_CPU].rlim_max)
				utaskp->uu_rlimit[RLIMIT_CPU].rlim_cur += 5;
		}
	    }
	    if (timerisset(&utaskp->uu_timer[ITIMER_PROF].it_value) &&
		itimerdecr(&utaskp->uu_timer[ITIMER_PROF], tick) == 0)
		    psignal(p, SIGPROF);
	}
	cp_time[cpstate]++;

}

#if	SIMPLE_CLOCK
#undef	tick
#endif	SIMPLE_CLOCK

