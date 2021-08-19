/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 14-May-90  Gregg Kellogg (gk) at NeXT
 *	NeXT uses timeouts from hardclock again.  Timeout and untimeout
 *	work as on other systems, they're scheduled via hardclock directly
 *	from softclock.  The us_timeout and us_abstimeout functions work
 *	as before.  Us_untimeout untimes out things scheduled via the above.
 *
 * 18-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Moved defined of dk_ndrive to bsd/init_main.c
 *
 * Revision 2.19  89/10/11  13:36:45  dlb
 * 	Add untimeout_try for multiprocessors.
 * 	Remove timestamp support.
 * 	Don't reset timeofday clock here on multimax because multimax
 * 	     routine might block (XXX).
 * 
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Remove dir.h inclusion
 *
 * 09-Jun-89  Mike DeMoney (mike) at NeXT
 *	NeXT no longer has special args for hardclock.  Usec_elapsed()
 *	no takes arg which is "usec_mark" so that usec_elapsed can be
 *	used in multiple contexts.  Hardclock was re-arranged so that
 *	stuff NeXT does from us_timer could be ifdef'ed out as a single
 *	chunk.  NeXT version of timeout no longer spl's, since it just
 *	calls us_timeout.  Hzto() converted back to be "time" rather
 *	than timefromboot base (for BSD compatability).
 *
 * 12-May-88  David Golub (dbg) at Carnegie-Mellon University
 *	MACH: eliminate updates to otherwise unused proc structure
 *	fields: p_rssize, p_cpticks.
 *
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TIME_NEW is now standard.  Removed slow_clock.
 *	Get rid of u_ru.ru_{utime,stime}.  Autonice code removed
 *	for MACH_TIME_NEW (see schedcpu() in kern_synch.c).
 *	Replaced cpu_idle checks with check to see if current_thread()
 *	is an idle thread.
 *
 * 19-Apr-88  Gregg Kellogg (gk) at NeXT
 *	NeXT: Hardclock is run off the microsecond timer via softints.
 *
 * 29-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH: Removed use of "sys/vm.h".
 *
 * 29-Dec-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Define softclock as a function before it is used as an argument
 *	to softcall so that gcc will get the correct type.
 *
 * 21-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Simplified conditionals, purged history.
 */

#import <simple_clock.h>
#import <stat_time.h>
#import <cpus.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_clock.c	7.1 (Berkeley) 6/5/86
 */

#import <machine/reg.h>
#import <machine/psl.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dk.h>
#import <sys/callout.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/proc.h>
#import <sys/table.h>

#import <machine/spl.h>

#ifdef	i386
#import <i386/clock.h>
#endif	i386
#ifdef	vax
#import <vax/mtpr.h>
#import <vax/clock.h>
#endif	vax

#ifdef	balance
#import <machine/slic.h>
#import <machine/intctl.h>
#import <machine/clock.h>
#import <machine/hwparam.h>
#endif	balance

#if	NeXT
#import <next/us_timer.h>
#import <nextdev/busvar.h>		// ipltospl()
#endif	NeXT

#ifdef GPROF
#import <sys/gprof.h>
#endif

#import <machine/cpu.h>

#import <kern/thread.h>
#import <sys/machine.h>
#import <kern/sched.h>
#import <sys/time_value.h>
#import <kern/timer.h>
#import <kern/xpr.h>

#import <sys/boolean.h>

decl_simple_lock_data(,callout_lock)

struct callout *callfree, *callout, calltodo;
int ncallout;

int softclock();

/*
 * Clock handling routines.
 *
 * This code is written to operate with two timers which run
 * independently of each other. The main clock, running at hz
 * times per second, is used to do scheduling and timeout calculations.
 * The second timer does resource utilization estimation statistically
 * based on the state of the machine phz times a second. Both functions
 * can be performed by a single clock (ie hz == phz), however the 
 * statistics will be much more prone to errors. Ideally a machine
 * would have separate clocks measuring time spent in user state, system
 * state, interrupt state, and idle state. These clocks would allow a non-
 * approximate measure of resource utilization.
 */

/*
 * TODO:
 *	time of day, system/user timing, timeouts, profiling on separate timers
 *	allocate more timeout table slots when table overflows.
 */
#define BUMPTIME(t, usec) { \
	register struct timeval *tp = (t); \
 \
	tp->tv_usec += (usec); \
	if (tp->tv_usec >= 1000000) { \
		tp->tv_usec -= 1000000; \
		tp->tv_sec++; \
	} \
}

/*
 * The hz hardware interval timer.
 * We update the events relating to real time.
 * If this timer is also being used to gather statistics,
 * we run through the statistics gathering routine as well.
 */

#define	NTICKS	1

#if	SIMPLE_CLOCK || NeXT
struct usec_mark hardclock_usec_mark;
#endif	SIMPLE_CLOCK || NeXT

/*ARGSUSED*/
hardclock(pc, ps)
	int ps;
	caddr_t pc;
{
	register struct callout *p1;
	register struct proc *p;
	register int s;
#if	SIMPLE_CLOCK || NeXT
#define	tick	myticks
	register int myticks;
#endif	SIMPLE_CLOCK || NeXT

	int needsoft = 0;
	extern int tickdelta;
	extern long timedelta;
	register thread_t	thread;

	thread = current_thread();

#if	SIMPLE_CLOCK || NeXT
	/*
	 *	Simple hardware timer does not restart on overflow, hence
	 *	interrupts do not happen at a constant rate.  Must call
	 *	machine-dependent routine to find out how much time has
	 *	elapsed since last interrupt.
	 *
	 *	On NeXT we use SIMPLE_CLOCK because hardclock is called
	 *	at softint0 level, and although time won't drift, there's
	 *	a fair amount of "jitter". --mike
	 */
	myticks = usec_elapsed(&hardclock_usec_mark);
	XPR(XPR_TIMER, ("hardclock tick is %d usecs\n", myticks));

	/*
	 *	NOTE: tick was #define'd to myticks above.
	 */
#endif	SIMPLE_CLOCK || NeXT

#if	TS_FORMAT == 1
/*
 *	Increment the tick count for the timestamping routine.
 */
	ts_tick_count += NTICKS;
#endif	TS_FORMAT == 1

#if	STAT_TIME
	if (USERMODE(ps)) {
		timer_bump(&thread->user_timer, NTICKS*tick);
	}
	else {
		timer_bump(&thread->system_timer, NTICKS*tick);
	}
#endif	STAT_TIME

	if (thread->state & TH_IDLE) {
		clock_tick(NTICKS, CPU_STATE_IDLE);
	} else if (USERMODE(ps)) {
		clock_tick(NTICKS, CPU_STATE_USER);
	} else {
		clock_tick(NTICKS, CPU_STATE_SYSTEM);
	}
	if (cpu_number() != master_cpu) {
		slave_hardclock(pc, ps);
		return;
	}

	/*
	 * Charge the time out based on the mode the cpu is in.
	 * Here again we fudge for the lack of proper interval timers
	 * assuming that the current state has been around at least
	 * one tick.
	 */
	if (USERMODE(ps)) {
#if	NeXT	
		if (u.u_procp) {
			if (u.u_prof.pr_scale) {
				u.u_procp->p_flag |= SOWEUPC;
				aston();
			}
		}
#else	NeXT
		if (u.u_prof.pr_scale)
			needsoft = 1;
#endif	NeXT
		/*
		 * CPU was in user state.  Increment
		 * user time counter, and process process-virtual time
		 * interval timer. 
		 */
		if (timerisset(&u.u_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&u.u_timer[ITIMER_VIRTUAL], tick) == 0)
			psignal(u.u_procp, SIGVTALRM);
	}

	/*
	 * If the cpu is currently scheduled to a process, then
	 * charge it with resource utilization for a tick, updating
	 * statistics which run in (user+system) virtual time,
	 * such as the cpu time limit and profiling timers.
	 * This assumes that the current process has been running
	 * the entire last tick.
	 */
	if (u.u_procp && !(thread->state & TH_IDLE))
	{
		if (u.u_rlimit[RLIMIT_CPU].rlim_cur != RLIM_INFINITY) {
		    time_value_t	sys_time, user_time;

		    thread_read_times(thread, &user_time, &sys_time);
		    if ((sys_time.seconds + user_time.seconds + 1) >
		        u.u_rlimit[RLIMIT_CPU].rlim_cur) {
			psignal(u.u_procp, SIGXCPU);
			if (u.u_rlimit[RLIMIT_CPU].rlim_cur <
			    u.u_rlimit[RLIMIT_CPU].rlim_max)
				u.u_rlimit[RLIMIT_CPU].rlim_cur += 5;
			}
		}
		if (timerisset(&u.u_timer[ITIMER_PROF].it_value) &&
		    itimerdecr(&u.u_timer[ITIMER_PROF], tick) == 0)
			psignal(u.u_procp, SIGPROF);
	}

	/*
	 * Increment the time-of-day, and schedule
	 * processing of the callouts at a very low cpu priority,
	 * so we don't keep the relatively high clock interrupt
	 * priority any longer than necessary.
	 */
	if (timedelta == 0)
		BUMPTIME(&time, tick)
	else {
		register delta;

		if (timedelta < 0) {
			delta = tick - tickdelta;
			timedelta += tickdelta;
		} else {
			delta = tick + tickdelta;
			timedelta -= tickdelta;
		}
		BUMPTIME(&time, delta);
	}

	/*
	 * If the alternate clock has not made itself known then
	 * we must gather the statistics.
	 */
	if (phz == 0)
		gatherstats(pc, ps);

	/*
	 * Update real-time timeout queue.
	 * At front of queue are some number of events which are ``due''.
	 * The time to these is <= 0 and if negative represents the
	 * number of ticks which have passed since it was supposed to happen.
	 * The rest of the q elements (times > 0) are events yet to happen,
	 * where the time for each is given as a delta from the previous.
	 * Decrementing just the first of these serves to decrement the time
	 * to all events.
	 */
#if	NeXT
	s = splclock();
#else	NeXT
	s = splhigh();
#endif	NeXT
	simple_lock(&callout_lock);
	p1 = calltodo.c_next;
	while (p1) {
		if (--p1->c_time > 0)
			break;
		needsoft = 1;
		if (p1->c_time == 0)
			break;
		p1 = p1->c_next;
	}
	simple_unlock(&callout_lock);
	splx(s);

	if (needsoft) {
		if (BASEPRI(ps)) {
			/*
			 * Save the overhead of a software interrupt;
			 * it will happen as soon as we return, so do it now.
			 */
#if	NeXT
			(void) spln(ipltospl(IPLSOFTCLOCK));
#else	NeXT
			(void) splsoftclock();
#endif	NeXT
#ifdef	sun
			softclock(USERMODE(ps) != 0);
#else	sun
			softclock(pc, ps);
#endif	sun
		} else
#ifdef	sun
	                softcall(softclock, USERMODE(ps) != 0);
#else	sun
# if	NeXT
			softint_sched(CALLOUT_PRI_SOFTINT0, softclock, 0);
# else	NeXT
			setsoftclock();
# endif	NeXT
#endif	sun
	}
}

#if	SIMPLE_CLOCK || NeXT
#undef	tick
#endif	SIMPLE_CLOCK || NeXT

/*
 * Gather statistics on resource utilization.
 *
 * We make a gross assumption: that the system has been in the
 * state it is in (user state, kernel state, interrupt state,
 * or idle state) for the entire last time interval, and
 * update statistics accordingly.
 */
/*ARGSUSED*/
gatherstats(pc, ps)
	caddr_t pc;
	int ps;
{
	register int cpstate, s;

	/*
	 * Determine what state the cpu is in.
	 */
	if (USERMODE(ps)) {
		/*
		 * CPU was in user state.
		 */
		if (u.u_procp->p_nice > NZERO)
			cpstate = CP_NICE;
		else
			cpstate = CP_USER;
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
		if ((current_thread()->state & TH_IDLE) && BASEPRI(ps))
			cpstate = CP_IDLE;
#ifdef GPROF
		s = pc - s_lowpc;
		if (profiling < 2 && s < s_textsize)
			kcount[s / (HISTFRACTION * sizeof (*kcount))]++;
#endif
	}
	/*
	 * We maintain statistics shown by user-level statistics
	 * programs:  the amount of time in each cpu state, and
	 * the amount of time each of DK_NDRIVE ``drives'' is busy.
	 */
	cp_time[cpstate]++;
	for (s = 0; s < DK_NDRIVE; s++)
		if (dk_busy & (1 << s))
			dk_time[s]++;
}

/*
 * Software priority level clock interrupt.
 * Run periodic events from timeout queue.
 */
/*ARGSUSED*/
softclock(pc, ps)
	caddr_t pc;
	int ps;
{
	for (;;) {
		register struct callout *p1;
		register caddr_t arg;
		register int (*func)();
		register int a, s;

#if	NeXT
		s = splclock();
#else	NeXT
		s = splhigh();
#endif	NeXT
		simple_lock(&callout_lock);
		if ((p1 = calltodo.c_next) == 0 || p1->c_time > 0) {
			simple_unlock(&callout_lock);
			splx(s);
			break;
		}
		arg = p1->c_arg; func = p1->c_func; a = p1->c_time;
		calltodo.c_next = p1->c_next;
		p1->c_next = callfree;
		callfree = p1;
		simple_unlock(&callout_lock);
		splx(s);
		(*func)(arg, a);
	}
#if	NeXT
	/*
	 * Next doesn't do profiling here.
	 */
#else	NeXT
	/*
	 * If trapped user-mode and profiling, give it
	 * a profiling tick.
	 */
	if (USERMODE(ps)) {
		register struct proc *p = u.u_procp;

		if (u.u_prof.pr_scale) {
			p->p_flag |= SOWEUPC;
			aston();
		}
	}
#endif	NeXT
}

/*
 * Arrange that (*fun)(arg) is called in t/hz seconds.
 */
timeout(fun, arg, t)
	int (*fun)();
	caddr_t arg;
	register int t;
{
	register struct callout *p1, *p2, *pnew;
#if	NeXT
	register int s = splclock();
#else	NeXT
	register int s = splhigh();
#endif	NeXT

	simple_lock(&callout_lock);
	if (t <= 0)
		t = 1;
	pnew = callfree;
	if (pnew == NULL)
		panic("timeout table overflow");
	callfree = pnew->c_next;
	pnew->c_arg = arg;
	pnew->c_func = fun;
	for (p1 = &calltodo; (p2 = p1->c_next) && p2->c_time < t; p1 = p2)
		if (p2->c_time > 0)
			t -= p2->c_time;
	p1->c_next = pnew;
	pnew->c_next = p2;
	pnew->c_time = t;
	if (p2)
		p2->c_time -= t;
	simple_unlock(&callout_lock);
	splx(s);
}

/*
 * untimeout is called to remove a function timeout call
 * from the callout structure.
 */
untimeout(fun, arg)
	int (*fun)();
	caddr_t arg;
{
	register struct callout *p1, *p2;
	register int s;

#if	NeXT
	ASSERT(!us_untimeout((func)fun, (vm_address_t)arg));
	s = splclock();
#else	NeXT
	s = splhigh();
#endif	NeXT
	simple_lock(&callout_lock);
	for (p1 = &calltodo; (p2 = p1->c_next) != 0; p1 = p2) {
		if (p2->c_func == fun && p2->c_arg == arg) {
			if (p2->c_next && p2->c_time > 0)
				p2->c_next->c_time += p2->c_time;
			p1->c_next = p2->c_next;
			p2->c_next = callfree;
			callfree = p2;
			break;
		}
	}
	simple_unlock(&callout_lock);
	splx(s);
}

#if	NCPUS > 1
/*
 * untimeout_try is a multiprocessor version of timeout that returns
 * a boolean indicating whether it successfully removed the entry.
 */
boolean_t
untimeout_try(fun, arg)
	int (*fun)();
	caddr_t arg;
{
	register struct callout *p1, *p2;
	register int s;
	register boolean_t	ret = FALSE;

#if	NeXT
	s = splclock();
#else	NeXT
	s = splhigh();
#endif	NeXT
	simple_lock(&callout_lock);
	for (p1 = &calltodo; (p2 = p1->c_next) != 0; p1 = p2) {
		if (p2->c_func == fun && p2->c_arg == arg) {
			if (p2->c_next && p2->c_time > 0)
				p2->c_next->c_time += p2->c_time;
			p1->c_next = p2->c_next;
			p2->c_next = callfree;
			callfree = p2;
			ret = TRUE;
			break;
		}
	}
	simple_unlock(&callout_lock);
	splx(s);
	return(ret);
}
#endif	NCPUS > 1

/*
 * Compute number of hz until specified time.
 * Used to compute third argument to timeout() from an
 * absolute time.
 */
hzto(tv)
	struct timeval *tv;
{
	register long ticks;
	register long sec;
	int s = splhigh();

	/*
	 * If number of milliseconds will fit in 32 bit arithmetic,
	 * then compute number of milliseconds to time and scale to
	 * ticks.  Otherwise just compute number of hz in time, rounding
	 * times greater than representible to maximum value.
	 *
	 * Delta times less than 25 days can be computed ``exactly''.
	 * Maximum value for any timeout in 10ms ticks is 250 days.
	 */
	sec = tv->tv_sec - time.tv_sec;
	if (sec <= 0x7fffffff / 1000 - 1000)
		ticks = ((tv->tv_sec - time.tv_sec) * 1000 +
			(tv->tv_usec - time.tv_usec) / 1000) / (tick / 1000);
	else if (sec <= 0x7fffffff / hz)
		ticks = sec * hz;
	else
		ticks = 0x7fffffff;
	splx(s);
	return (ticks);
}

#if	NeXT
/*
 * Convert ticks to a timeval
 */
ticks_to_timeval(ticks, tvp)
	register long ticks;
	struct timeval *tvp;
{
	tvp->tv_sec = ticks/hz;
	tvp->tv_usec = (ticks%hz) * tick;
	ASSERT(tvp->tv_usec < 1000000);
}
#endif	NeXT

profil()
{
	register struct a {
		short	*bufbase;
		unsigned bufsize;
		unsigned pcoffset;
		unsigned pcscale;
	} *uap = (struct a *)u.u_ap;
	register struct uuprof *upp = &u.u_prof;

	upp->pr_base = uap->bufbase;
	upp->pr_size = uap->bufsize;
	upp->pr_off = uap->pcoffset;
	upp->pr_scale = uap->pcscale;
}
