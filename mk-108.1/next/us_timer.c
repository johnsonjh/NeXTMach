/*	@(#)us_timer.c	1.0	12/8/87		(c) 1987 NeXT	*/

/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 14-May-90  Gregg Kellogg (gk) at NeXT
 *	Changed splclock() to splusclock().
 *	Don't let hardclock (or gatherstats) get behind realtime.
 *	Don't schedule timeouts with a delta < 0.
 *
 *  8-Jun-89  Mike DeMoney (mike) at NeXT
 *	Major modifications.
 *  8-Dec-87  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <sys/param.h>
#import <sys/kernel.h>
#import <next/us_timer.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/clock.h>
#import <next/spl.h>
#import <kern/xpr.h>

#if	DEBUG
int timer_debug = 0;
#endif	DEBUG

/*
 * Micro-second Timer.
 */

#ifdef DEBUG
static void sec_flash(boolean_t on);
#define	timerdebug(f)	XPR(XPR_TIMER, f)
#define	timerdebug2(f)	{ if (timer_debug & 2) XPR(XPR_TIMER, f); }
#else
#define	timerdebug(f)
#define	timerdebug2(f)
#endif

#define MIN_TIMER_DELTA 500	/* minimum number of usecs between events */

/*
 * these work for all two's complement machines
 */
#define MAX_INT ((int)(((unsigned)-1)>>1))
#define MIN_INT (~MAX_INT)

extern volatile u_char	*timer_high;	/* high byte of timer value/latch */
extern volatile u_char	*timer_low;	/* low byte of timer value/latch */
extern volatile u_char	*timer_csr;	/* pointer to timer cs register */

struct timeval timefromboot;

static struct usec_mark tfb_mark;
static struct timeval hardclock_tv;	/* boot time of last hardclock */
static struct timeval gatherstats_tv;	/* boot time of last gatherstats */
static int ptick;			/* profiling tick in usecs */
static int hardclock_ps, gatherstats_ps;
static vm_address_t hardclock_pc, gatherstats_pc;

static int load_timer(u_int delta);
static void us_hardclock(void);
static void us_gatherstats(void);
static int us_timer_int(int arg, vm_address_t pc, int ps);

decl_simple_lock_data(,us_callout_lock)

struct callout *us_callfree, *us_callout, us_calltodo;
extern int ncallout;


static inline void bumptime(register struct timeval *tp, int usec)
{
	tp->tv_usec += (usec);
	if (tp->tv_usec >= 1000000) {
		tp->tv_usec -= 1000000;
		tp->tv_sec++;
	}
}

/*
 * difference (in usec) between timevals
 */
static inline int us_timevalsub(struct timeval *tvp, struct timeval *uvp)
{
	register int secdelta = tvp->tv_sec - uvp->tv_sec;

	if (secdelta > 100)
		return(MAX_INT);
	if (secdelta < -100)
		return(MIN_INT);
	return(secdelta * 1000000 + tvp->tv_usec - uvp->tv_usec);
}

static inline int timer_get(void)
{
	register int tl, th;
	do {
		th = *timer_high;
		tl = *timer_low;
	} while (th != *timer_high);
	return((th<<8)|tl);
}

static inline void timer_set(t)
{
	*timer_csr = 0;
	*timer_low = 0xff;
	*timer_high = t>>8;
	*timer_low = t;
	*timer_csr = (TIMER_ENABLE|TIMER_UPDATE);
	*timer_low = 0xff;
	*timer_high = 0xff;
}

static void us_hardclock(void)
{
	int s;

	timerdebug(("invoking hardclock at %u:%u\n",
	     timefromboot.tv_sec, timefromboot.tv_usec));
	s = splsched();
	hardclock((caddr_t)hardclock_pc, hardclock_ps);
	splx(s);
	bumptime(&hardclock_tv, tick);
	/*
	 * Don't get behind real time.
	 */
	if (us_timevalsub(&hardclock_tv, &timefromboot) < 0) {
		hardclock_tv = timefromboot;
		bumptime(&hardclock_tv, tick);
	}
	us_abstimeout((func)us_hardclock, 0, &hardclock_tv,
		CALLOUT_PRI_SOFTINT1);
}

static void us_gatherstats(void)
{
	int s;

	timerdebug(("invoking gatherstats at %u:%u\n",
	     timefromboot.tv_sec, timefromboot.tv_usec));
	s = splsched();
	gatherstats((caddr_t)gatherstats_pc, gatherstats_ps);
	splx(s);
	bumptime(&gatherstats_tv, ptick);
	/*
	 * Don't get behind real time.
	 */
	if (us_timevalsub(&gatherstats_tv, &timefromboot) < 0) {
		gatherstats_tv = timefromboot;
		bumptime(&gatherstats_tv, tick);
	}
	us_abstimeout((func)us_gatherstats, 0, &gatherstats_tv,
		CALLOUT_PRI_SOFTINT1);
}

/*
 * Initialize the timer.
 *
 * Set the timer IPL to ipltimer.
 * Add us_timer_int to the set of interrupts maintained at that priority.
 * Start the timer going with the maximum timeout value.
 */
void us_timer_init(void)
{
	register int s;

	timerdebug(("initializing timer\n"));
	s = splusclock();

	/* So the system knows what to do when the interrupt comes in */
	install_scanned_intr (I_TIMER, us_timer_int, 0);

	/* Tell the timer to interrupt at ipl6 */
	*scr2 &= ~SCR2_TIMERIPL7;

	timer_set(TIMER_MAX);

	splx(s);

	/*
	 * Get hardclock running
	 */
	usec_elapsed(&tfb_mark);
	microboot(&hardclock_tv);
	bumptime(&hardclock_tv, tick);
	us_abstimeout((func)us_hardclock, 0, &hardclock_tv,
		CALLOUT_PRI_SOFTINT1);

	/*
	 * Get the "alternate clock" going if it's requested
	 */
	if (phz) {
		/*
		 * Given the current hardware support, I'd say it's just
		 * about worthless to turn this on!  But... here's the code.
		 */
		microboot(&gatherstats_tv);
		ptick = 1000000/phz;
		bumptime(&gatherstats_tv, ptick);
		us_abstimeout((func)us_gatherstats, 0, &gatherstats_tv,
			CALLOUT_PRI_SOFTINT1);
	}
#if	DEBUG
	timeout(sec_flash, 1, hz);
#endif	DEBUG

	/*
	 * Initialize callout lock.
	 */
	simple_lock_init(&us_callout_lock);
}

/*
 * Respond to a usec_timer interrupt.
 * We do stuff here that is drift sensitive, like maintain timevals,
 * and request softints to do other less sensitive things like callouts
 * and user statistics and timer stuff.
 */
static int us_timer_int(int arg, vm_address_t pc, int ps)
{
	register int interval;
	register struct callout *p1;

	ASSERT(curipl() >= IPLCLOCK);

	interval = *timer_csr;			/* clears the interrupt */

#if NCPUS > 1
	/*
	 * Only service timeouts on master cpu.
	 */
	if (cpu_number() != master_cpu)
		return; 
#endif NCPUS > 1

	/* Sync sw maintained high-order bits of event clock */
	event_sync();

	/* advance system absolute time reference (usecs from boot) */
	bumptime(&timefromboot, usec_elapsed(&tfb_mark));
	timerdebug2(("bumped timefromboot %u:%u\n", timefromboot.tv_sec,
	    timefromboot.tv_usec));

	/*
	 * Initiate software interrupts to accomplish callouts that are
	 * now due.  Some minor crude here to make hardclock and gatherstats
	 * fit into general callout model.
	 */
	simple_lock(&us_callout_lock);
again:
	for (p1 = us_calltodo.c_next;
	   p1 && timercmp(&p1->c_timeval, &timefromboot, <);
	   p1 = us_calltodo.c_next) {
		/* Crude and painful, but it get's the job done.... */
		if (p1->c_func == (func)us_hardclock) {
			hardclock_ps = ps;
			hardclock_pc = pc;
		} else if (phz && p1->c_func == (func)us_gatherstats) {
			gatherstats_ps = ps;
			gatherstats_pc = pc;
		}
		timerdebug(("timer run func 0x%x arg 0x%x pnew 0x%x %u:%u\n",
		    p1->c_func, p1->c_arg, p1, p1->c_timeval.tv_sec,
		    p1->c_timeval.tv_usec));
		softint_sched (p1->c_pri, p1->c_func, p1->c_arg);
		us_calltodo.c_next = p1->c_next;
		p1->c_next = us_callfree;
		us_callfree = p1;
	}
	/*
	 * If there's more to do, resync the absolute time clock and
	 * schedule another timer interrupt
	 */
	if (p1) {
		bumptime(&timefromboot, usec_elapsed(&tfb_mark));
		interval = us_timevalsub(&p1->c_timeval, &timefromboot);
		if (interval < 0)
			goto again;
	} else
		interval = TIMER_MAX;
	load_timer(interval);
	simple_unlock(&us_callout_lock);
	return 0;
}

/*
 * Us_timeout, a microsecond accurate timeout mechanism.
 * allows function to be called at a specific priority.
 * Us_abstimeout times out at the specified boottime relative time.
 * Us_timeout times out in tvp secs/usecs from now.
 */

void us_timeout(func proc, vm_address_t arg, struct timeval *tvp, int pri)
{
	struct timeval tv;
	
	microboot(&tv);
	timevaladd(&tv, tvp);
	us_abstimeout(proc, arg, &tv, pri);
}

void us_abstimeout(func proc, vm_address_t arg, struct timeval *tvp, int pri)
{
	register struct callout *p1, *p2, *pnew;
	register int s, newdelta, quanta;
	struct timeval tv;

	ASSERT (!( pri < 0 || pri >= N_CALLOUT_PRI));

	/*
	 * Quantize timeout point based on pri
	 */
	quanta = 0;
	tv = *tvp;
	switch (pri) {
	case CALLOUT_PRI_THREAD:
	case CALLOUT_PRI_SOFTINT0:
		quanta = tick;	/* so we don't "beat" against hardclock */
		break;
	case CALLOUT_PRI_SOFTINT1:
		quanta = 1024;	/* 1.024 ms */
		break;
	}
	if (quanta) {	/* round to next quanta */
		bumptime(&tv, quanta>>1);
		tv.tv_usec = (tv.tv_usec / quanta) * quanta;
	}

	s = splusclock();
	simple_lock(&us_callout_lock);
	pnew = us_callfree;
	if (pnew == NULL)
		panic("us_abstimeout table overflow");
	us_callfree = pnew->c_next;

	timerdebug(("timeout func 0x%x arg 0x%x pnew 0x%x at %u:%u\n",
	    	proc, arg, pnew, tv.tv_sec, tv.tv_usec));

	pnew->c_pri = pri;
	pnew->c_arg = (caddr_t)arg;
	pnew->c_func = proc;

	for (p1 = &us_calltodo;
	    (p2 = p1->c_next) && timercmp(&(p2->c_timeval), &tv, <);
	    p1 = p2)
		continue;
	p1->c_next = pnew;
	pnew->c_next = p2;
	pnew->c_timeval = tv;

	/*
	 * check to see if we need to load the timer.
	 */
	if (pnew == us_calltodo.c_next) {
		microboot(&tv);
		newdelta = us_timevalsub(&pnew->c_timeval, &tv);
		load_timer(newdelta < 0 ? 0 : newdelta);
	}
	simple_unlock(&us_callout_lock);
	splx(s);
}

/*
 * Remove something scheduled via us_timeout or us_abstimeout.
 */
boolean_t us_untimeout(func proc, vm_address_t arg)
{
	register struct callout *p1, *p2;
	register int s;

	s = splusclock();
	simple_lock(&us_callout_lock);
	for (p1 = &us_calltodo; (p2 = p1->c_next) != 0; p1 = p2) {
		if (p2->c_func == proc && p2->c_arg == (caddr_t)arg) {
			if (p2->c_next && p2->c_time > 0)
				p2->c_next->c_time += p2->c_time;
			p1->c_next = p2->c_next;
			p2->c_next = us_callfree;
			us_callfree = p2;
			simple_unlock(&us_callout_lock);
			splx(s);
			return TRUE;
			break;
		}
	}
	simple_unlock(&us_callout_lock);
	splx(s);
	return FALSE;
}

/*
 * Load the timer with delta.
 * Checks to ensure that delta isn't too big or small.
 * If it's too big load_timer will load the latch too and returns 0.
 * Otherwise load_timer returns 1 indicating that the latch needs to
 * be loaded too.
 * Must be called at spltimer
 */
static int load_timer(u_int delta)
{
	ASSERT(curipl() >= IPLCLOCK);

	if (delta < MIN_TIMER_DELTA)
		delta = MIN_TIMER_DELTA;
	else if (delta > TIMER_MAX)
		delta = TIMER_MAX;

	timerdebug2(("setting timer to %u at %u:%u\n", delta,
	    timefromboot.tv_sec, timefromboot.tv_usec));
	timer_set(delta);
}

/*
 * Return time in usecs since usec_elapsed last called with this
 * usec_mark.
 */
unsigned int usec_elapsed(struct usec_mark *ump)
{
	unsigned	result;

	if (ump->initialized) {
		result = event_delta(ump->last_usecs);
		ump->last_usecs += result;
	} else {
		result = 0;
		ump->initialized = TRUE;
		ump->last_usecs = event_get();
	}
	return(result);
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We do this by reading the interval count
 * register to determine the time remaining to the next clock tick.
 * We must compensate for wraparound which is not yet reflected in the time.
 * Also check that this time is no less than
 * any previously-reported time, which could happen around the time
 * of a clock adjustment.  Just for fun, we guarantee that the time
 * will be greater than the value obtained by a previous call.
 */
void microtime(struct timeval * tvp)
{
	int s;
	static struct timeval lasttime;
	register long t;
	extern struct usec_mark hardclock_usec_mark;

	s = splusclock();
	*tvp = time;
	splx(s);
	timerdebug2(("microtime called\n"));
	if (hardclock_usec_mark.initialized) {
		t =  event_delta(hardclock_usec_mark.last_usecs);
		tvp->tv_usec += t;
		if (tvp->tv_usec >= 1000000) {
			tvp->tv_sec++;
			tvp->tv_usec -= 1000000;
		}
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
}

/*
 * microboot returns the number of microseconds since boot.
 */
void microboot(struct timeval * tvp)
{
	register s;

	s = splusclock();
	*tvp = timefromboot;
	splx(s);
	timerdebug2(("microboot called\n"));
	if (tfb_mark.initialized) {
		tvp->tv_usec += event_delta(tfb_mark.last_usecs);

		while ((unsigned)tvp->tv_usec > 1000000) {
			tvp->tv_usec -= 1000000;
			tvp->tv_sec++;
		}
	}
}

/*
 *	Delay for the specified number of microseconds.
 */
void delay(unsigned int n)
{
	register int d = event_get();

	while (event_delta(d) < (n+1))
		continue;
}

void
us_delay(unsigned usecs)
{
	int s;
	unsigned start, remain, delta;
	struct timeval tv;
	extern void wakeup(caddr_t chan);

	if ((int)usecs < 0)
		panic("us_delay");
	s = splnet();
	start = event_get();
	while ((delta = event_delta(start)) < usecs) {
		remain = usecs - delta;
		if (remain < 10)
			break;
		tv.tv_sec = remain / 1000000;
		tv.tv_usec = remain % 1000000;
		us_timeout((func)wakeup, (vm_address_t)&tv, &tv,
			CALLOUT_PRI_SOFTINT1);
		sleep((caddr_t)&tv, PSLEP);
	}
	splx(s);
}

#if	DEBUG
struct timeval sec_tv = {1, 0};
extern int light_acquired;
static void sec_flash(boolean_t on)
{
	int s = spldma();
	if (timer_debug&4) {
		light_acquired = 1;
		if (on)
			*scr2 |= SCR2_EKG_LED;
		else
			*scr2 &= ~SCR2_EKG_LED;
	} else
		light_acquired = 0;
	splx(s);

	if (timer_debug&8)
		us_timeout((func)sec_flash,
			(vm_address_t)!on, &sec_tv, CALLOUT_PRI_SOFTINT0);
	else
		timeout(sec_flash, !on, hz);
}
#endif	DEBUG
