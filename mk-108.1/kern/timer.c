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
 * $Log:	timer.c,v $
 * Revision 2.7  89/10/11  14:35:01  dlb
 * 	Optimize timer_normalize and timer_grab based on new timer
 * 	representation.  This is a simple version of a Lamport clock
 * 	based on a suggestion by dbg (again!).  The savings in
 * 	timer_grab are impressive.
 * 	[89/09/12            dlb]
 * 
 * 	New overflow logic.  Check low_bits directly for risk of
 * 	overflow.  Gets rid of service_timers().  Suggested by dbg.
 * 	[88/10/26            dlb]
 * 
 * Revision 2.6  89/03/09  20:17:23  rpd
 * 	More cleanup.
 * 
 * Revision 2.5  89/02/25  18:10:44  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.4  89/01/30  22:07:42  rpd
 * 	Added declarations of variables from kern/timer.h.
 * 	(The declarations there use "extern" now.)
 * 	[89/01/25  15:19:35  rpd]
 * 
 * Revision 2.3  88/12/19  02:48:18  mwyoung
 * 	Remove lint.
 * 	[88/12/09            mwyoung]
 * 
 * 18-May-88  David Golub (dbg) at Carnegie-Mellon University
 *	HC compiler does not consider '| =' to be the same as '|='.
 *
 *  9-May-88  David Golub (dbg) at Carnegie-Mellon University
 *	Added register declarations.  Not all machines enjoy the luxury
 *	of a reasonable compiler.
 *
 * 20-Apr-88  David Black (dlb) at Carnegie-Mellon University
 *	Removed thread_times system call.
 *
 * 29-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	Normalize old timer on interrupt exit if needed.
 *
 * 19-Feb-88  David Black (dlb) at Carnegie-Mellon University
 *	Massive changes - parametrize most constants to deal with
 *	different clock frequencies, field name changes in timer
 *	structure.  Replace array in thread structure with individual
 *	timers.  Implemented timer_delta and timer_current_delta routines
 *	to efficiently read timing information for scheduler.
 *
 * 12-Feb-88  David Black (dlb) at Carnegie-Mellon University
 *	Use time_value_t's instead of struct timeval's.
 *
 * 28-Jan-88  David Black (dlb) at Carnegie-Mellon University
 *	Rewrote thread_times for new port naming and thread referencing
 *	logic.
 *
 * 26-Mar-87  David Black (dlb) at Carnegie-Mellon University
 *	Changed trap entry and exit routines to always set correct timer
 *	regardless of current timer.
 *
 * 18-Mar-87  David L. Black (dlb) at Carnegie-Mellon University
 *	Moved kickoff of service_timers() into init_main.c because
 *	callout queue is not initialized when init_timers is called.
 *
 * 23-Feb-87  David L. Black (dlb) at Carnegie-Mellon University
 *	Created.
 */ 

#import <cpus.h>
#import <stat_time.h>

#import <sys/param.h>
#import <sys/kernel.h>
#import <sys/kern_return.h>
#import <sys/port.h>
#import <kern/queue.h>
#import <kern/thread.h>
#import <sys/time_value.h>
#import <kern/timer.h>
#import <machine/cpu.h>

#import <kern/macro_help.h>

timer_t		current_timer[NCPUS];
timer_data_t	kernel_timer[NCPUS];

/*
 *	init_timers initializes all non-thread timers and puts the
 *	service routine on the callout queue.  All timers must be
 *	serviced by the callout routine once an hour.
 */
init_timers()
{
	register int	i;
	register timer_t	this_timer;

	/*
	 *	Initialize all the kernel timers and start the one
	 *	for this cpu (master) slaves start theirs later.
	 */
	this_timer = &kernel_timer[0];
	for ( i=0 ; i<NCPUS ; i++, this_timer++) {
		timer_init(this_timer);
		current_timer[i] = (timer_t) 0;
	}

	start_timer(&kernel_timer[cpu_number()]);
}

/*
 *	timer_init initializes a single timer.
 */
timer_init(this_timer)
register
timer_t this_timer;
{
	this_timer->low_bits = 0;
	this_timer->high_bits = 0;
	this_timer->tstamp = 0;
	this_timer->high_bits_check = 0;
}

#if	STAT_TIME
#else	STAT_TIME
/*
 *	start_timer starts the given timer for this cpu. It is called
 *	exactly once for each cpu during the boot sequence.
 */
void
start_timer(timer)
timer_t timer;
{
	timer->tstamp = get_timestamp();
	current_timer[cpu_number()] = timer;
}

/*
 *	time_trap_uentry does trap entry timing.  Caller must lock out
 *	interrupts and take a timestamp.  ts is a timestamp taken after
 *	interrupts were locked out. Must only be called if trap was
 *	from user mode.
 */
void
time_trap_uentry(ts)
unsigned ts;
{
	int	elapsed;
	int	mycpu;
	timer_t	mytimer;

	/*
	 *	Calculate elapsed time.
	 */
	mycpu = cpu_number();
	mytimer = current_timer[mycpu];
	elapsed = ts - mytimer->tstamp;
#ifdef	TIMER_MAX
	if (elapsed < 0) elapsed += TIMER_MAX;
#endif	TIMER_MAX

	/*
	 *	Update current timer.
	 */
	mytimer->low_bits += elapsed;
	mytimer->tstamp = 0;

	/*
	 *	Record new timer.
	 */
	mytimer = &(active_threads[mycpu]->system_timer);
	current_timer[mycpu] = mytimer;
	mytimer->tstamp = ts;
}

/*
 *	time_trap_uexit does trap exit timing.  Caller must lock out
 *	interrupts and take a timestamp.  ts is a timestamp taken after
 *	interrupts were locked out.  Must only be called if returning to
 *	user mode.
 */
void
time_trap_uexit(ts)
{
	int	elapsed;
	int	mycpu;
	timer_t	mytimer;

	/*
	 *	Calculate elapsed time.
	 */
	mycpu = cpu_number();
	mytimer = current_timer[mycpu];
	elapsed = ts - mytimer->tstamp;
#ifdef	TIMER_MAX
	if (elapsed < 0) elapsed += TIMER_MAX;
#endif	TIMER_MAX

	/*
	 *	Update current timer.
	 */
	mytimer->low_bits += elapsed;
	mytimer->tstamp = 0;

	/*
	 *	Normalize old and new timers if needed.
	 */
	if (mytimer->low_bits & TIMER_LOW_FULL) {
		timer_normalize(mytimer);	/* SYSTEMMODE */
	}

	mytimer = &(active_threads[mycpu]->user_timer);

	if (mytimer->low_bits & TIMER_LOW_FULL) {
		timer_normalize(mytimer);	/* USERMODE */
	}

	/*
	 *	Record new timer.
	 */
	current_timer[mycpu] = mytimer;
	mytimer->tstamp = ts;
}

/*
 *	time_int_entry does interrupt entry timing.  Caller must lock out
 *	interrupts and take a timestamp. ts is a timestamp taken after
 *	interrupts were locked out.  new_timer is the new timer to
 *	switch to.  This routine returns the currently running timer,
 *	which MUST be pushed onto the stack by the caller, or otherwise
 *	saved for time_int_exit.
 */
timer_t
time_int_entry(ts,new_timer)
unsigned	ts;
timer_t	new_timer;
{
	int	elapsed;
	int	mycpu;
	timer_t	mytimer;

	/*
	 *	Calculate elapsed time.
	 */
	mycpu = cpu_number();
	mytimer = current_timer[mycpu];

	elapsed = ts - mytimer->tstamp;
#ifdef	TIMER_MAX
	if (elapsed < 0) elapsed += TIMER_MAX;
#endif	TIMER_MAX

	/*
	 *	Update current timer.
	 */
	mytimer->low_bits += elapsed;
	mytimer->tstamp = 0;

	/*
	 *	Switch to new timer, and save old one on stack.
	 */
	new_timer->tstamp = ts;
	current_timer[mycpu] = new_timer;
	return(mytimer);
}

/*
 *	time_int_exit does interrupt exit timing.  Caller must lock out
 *	interrupts and take a timestamp.  ts is a timestamp taken after
 *	interrupts were locked out.  old_timer is the timer value pushed
 *	onto the stack or otherwise saved after time_int_entry returned
 *	it.
 */
void
time_int_exit(ts, old_timer)
unsigned	ts;
timer_t	old_timer;
{
	int	elapsed;
	int	mycpu;
	timer_t	mytimer;

	/*
	 *	Calculate elapsed time.
	 */
	mycpu = cpu_number();
	mytimer = current_timer[mycpu];
	elapsed = ts - mytimer->tstamp;
#ifdef	TIMER_MAX
	if (elapsed < 0) elapsed += TIMER_MAX;
#endif	TIMER_MAX

	/*
	 *	Update current timer.
	 */
	mytimer->low_bits += elapsed;
	mytimer->tstamp = 0;

	/*
	 *	If normalization requested, do it.
	 */
	if (mytimer->low_bits & TIMER_LOW_FULL) {
		timer_normalize(mytimer);
	}
	if (old_timer->low_bits & TIMER_LOW_FULL) {
		timer_normalize(old_timer);
	}

	/*
	 *	Start timer that was running before interrupt.
	 */
	old_timer->tstamp = ts;
	current_timer[mycpu] = old_timer;
}

/*
 *	timer_switch switches to a new timer.  The machine
 *	dependent routine/macro get_timestamp must return a timestamp.
 *	Caller must lock out interrupts.
 */
void
timer_switch(new_timer)
timer_t new_timer;
{
	int		elapsed;
	int		mycpu;
	timer_t		mytimer;
	unsigned	ts;

	/*
	 *	Calculate elapsed time.
	 */
	mycpu = cpu_number();
	mytimer = current_timer[mycpu];
	ts = get_timestamp();
	elapsed = ts - mytimer->tstamp;
#ifdef	TIMER_MAX
	if (elapsed < 0) elapsed += TIMER_MAX;
#endif	TIMER_MAX

	/*
	 *	Update current timer.
	 */
	mytimer->low_bits += elapsed;
	mytimer->tstamp = 0;

	/*
	 *	Normalization check
	 */
	if (mytimer->low_bits & TIMER_LOW_FULL) {
		timer_normalize(mytimer);
	}

	/*
	 *	Record new timer.
	 */
	current_timer[mycpu] = new_timer;
	new_timer->tstamp = ts;
}
#endif	STAT_TIME

/*
 *	timer_normalize normalizes the value of a timer.  It is
 *	called only rarely, to make sure low_bits never overflows.
 */
timer_normalize(timer)
register
timer_t	timer;
{
	unsigned int	high_increment;

	/*
	 *	Calculate high_increment, then write high check field first
	 *	followed by low and high.  timer_grab() reads these fields in
	 *	reverse order so if high and high check match, we know
	 *	that the values read are ok.
	 */

	high_increment = timer->low_bits/TIMER_HIGH_UNIT;
	timer->high_bits_check += high_increment;
	timer->low_bits %= TIMER_HIGH_UNIT;
	timer->high_bits += high_increment;
}

/*
 *	timer_grab() is a macro to retrieve the value of a timer.
 */

#define timer_grab(timer, save)						\
	MACRO_BEGIN							\
	do {								\
		(save)->high = (timer)->high_bits;			\
		(save)->low = (timer)->low_bits;			\
	/*								\
	 *	If the timer was normalized while we were doing this,	\
	 *	the high_bits value read above and the high_bits check	\
	 *	value won't match because high_bits_check is the first	\
	 *	field touched by the normalization procedure, and	\
	 *	high_bits is the last.					\
	 *
	 *	Additions to timer only touch low bits and 		\
	 *	are therefore atomic with respect to this.		\
	 */								\
	} while ( (save)->high != (timer)->high_bits_check);		\
	MACRO_END


/*
 *	timer_read reads the value of a timer into a time_value_t.  If the
 *	timer was modified during the read, retry.  The value returned
 *	is accurate to the last update; time accumulated by a running
 *	timer since its last timestamp is not included.
 */

void
timer_read(timer, tv)
timer_t timer;
register
time_value_t *tv;
{
	timer_save_data_t	temp;

	timer_grab(timer,&temp);
	/*
	 *	Normalize the result
	 */
#ifdef	TIMER_ADJUST
	TIMER_ADJUST(&temp);
#endif	TIMER_ADJUST
	tv->seconds = temp.high + temp.low/1000000;
	tv->microseconds = temp.low%1000000;

}

/*
 *	thread_read_times reads the user and system times from a thread.
 *	Time accumulated since last timestamp is not included.  Should
 *	be called at splsched() to avoid having user and system times
 *	be out of step.  Doesn't care if caller locked thread.
 */
void	thread_read_times(thread, user_time_p, system_time_p)
	thread_t 	thread;
	time_value_t	*user_time_p;
	time_value_t	*system_time_p;
{
	timer_save_data_t	temp;
	register timer_t	timer;

	timer = &thread->user_timer;
	timer_grab(timer, &temp);

#ifdef	TIMER_ADJUST
	TIMER_ADJUST(&temp);
#endif	TIMER_ADJUST
	user_time_p->seconds = temp.high + temp.low/1000000;
	user_time_p->microseconds = temp.low % 1000000;

	timer = &thread->system_timer;
	timer_grab(timer, &temp);

#ifdef	TIMER_ADJUST
	TIMER_ADJUST(&temp);
#endif	TIMER_ADJUST
	system_time_p->seconds = temp.high + temp.low/1000000;
	system_time_p->microseconds = temp.low % 1000000;
}

/*
 *	timer_delta takes the difference of a saved timer value
 *	and the current one, and updates the saved value to current.
 *	The difference is returned as a function value.  See
 *	TIMER_DELTA macro (timer.h) for optimization to this.
 */

unsigned
timer_delta(timer, save)
register
timer_t	timer;
timer_save_t	save;
{
	timer_save_data_t	new_save;
	register unsigned	result;

	timer_grab(timer,&new_save);
	result = (new_save.high - save->high) * TIMER_HIGH_UNIT +
		new_save.low - save->low;
	save->high = new_save.high;
	save->low = new_save.low;
	return(result);
}

