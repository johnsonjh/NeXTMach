/*	@(#)eventc.h	1.0	02/11/88	(c) 1988 NeXT	*/

/* 
 * HISTORY
 * 22-Jan-90  Brian Pinkerton (bpinker) at NeXT
 *	Add 32 more bits to event counter -- now it's 64 bits...
 *
 * 28-Sep-89  John Seamons (jks) at NeXT
 *	Access event counter as a byte device to be compatible with 68040.
 *
 *  7-Jun-89  Mike DeMoney (mike) at NeXT
 *	Re-worked implementation significantly.
 *	
 * 11-Feb-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 * USING THE CLOCK FACILITIES:
 * Relative timeouts can be done with timeout, or for ~100 usec accuracy,
 * with us_timeout.
 *
 * Absolute timeouts can be done with us_abstimeout.  The absolute time-base
 * for these absolute timeouts is retrieved with microboot().  (NOTE: it's
 * completely bogus to base absolute timeouts on microtime()!)
 *
 * You should never call event_sync(), it's taken care of for you!
 *
 * Event_get() returns a 32 bit usec clock that runs continuously from
 * boot.  It's value is not relative to anything, at boottime the value
 * is random and is incremented from there -- be mindful that wrap-around
 * can occur.
 */ 

#ifndef	_EVENTC_
#define _EVENTC_

#if	KERNEL
#import <next/cpu.h>
#import <next/spl.h>
#import <kern/assert.h>
#import <sys/time_stamp.h>

extern volatile u_char		*eventc_latch, *eventc_h, *eventc_m, *eventc_l;
extern volatile unsigned int	*event_high; /* high-order 32 bits of eventc */
extern volatile unsigned int	*event_middle; /* middle 12 bits of eventc */

#define EVENT_HIGHBIT 0x80000
#define EVENT_MASK 0xfffff


/*
 * Synchronize software maintained high bits of event clock with
 * hardware maintained low bits.  NOTE: This routine should be called
 * periodically from a single point; (don't call it from multiple points,
 * don't call it from event_get()).  Currently, this is called from the
 * usec timer interrupt.  The period should be no more than 1/4 the
 * overflow period of the hardware clock.  It doesn't hurt to call this
 * more frequently.
 *
 * 21 January 1990, Brian Pinkerton
 *   Add 32 more software maintained bits to the event counter.  Here's
 *   the situation: the hardware maintains the low order 20 bits of the
 *   clock in *eventc, we maintain the top 44 bits in *event_middle (12)
 *   and *event_high (32).  To make the algorithm work, we actually use
 *   the top 13 bits in event_middle; this extra bit is an indication
 *   of the last high-order hardware maintained bit we saw.  When
 *   event_sync is called, we XOR the middle 13 bits with the lower 20;
 *   these overlap only on bit 19.  If bit 19 is on, then we have to
 *   sync the middle bits.  We do so.  If the result is zero (ie, the
 *   middle bits wrapped) then we add one to the high order word. The
 *   entire 64 bit counter will never wrap as long as we're counting
 *   microseconds.
 *
 */
static inline unsigned int event_sync(void)
{
	register unsigned int t;

	t = *eventc_latch;	/* load the latch from the event counter */
	t = (*eventc_h << 16) | (*eventc_m << 8) | *eventc_l;
	if ((t ^ *event_middle) & EVENT_HIGHBIT) {
		*event_middle += EVENT_HIGHBIT;
		if ((*event_middle & (~(EVENT_MASK>>1))) == 0)
			(*event_high)++;
	}
}

/* Return 32 bit representation of event counter */
static inline unsigned int event_get(void)
{
	register unsigned int high, low;

	high = *event_middle;
	low = *eventc_latch;	/* load the latch from the event counter */
	low = (*eventc_h << 16) | (*eventc_m << 8) | *eventc_l;
	low &= EVENT_MASK;
	if ((high ^ low) & EVENT_HIGHBIT)
		high += EVENT_HIGHBIT;
	ASSERT((high & (EVENT_MASK>>1)) == 0);
	return (high | low);
}


/* fill in the timestamp structure with the current clock values */
static inline void event_set_ts(tsp)
	struct tsval *tsp;
{
	register unsigned int temp;
	
	do {
		temp = event_get();
		tsp->high_val = *event_high;
		tsp->low_val = event_get();
	} while (tsp->low_val < temp);
}


/* Return the number of microseconds between now and passed event */
static inline unsigned int event_delta(unsigned int prev_time)
{
	return (event_get() - prev_time);
}

/*
 * This is used to implement the machine-depended [sched_]usec_elapsed
 * routine.
 */
struct usec_mark {
	int	initialized;
	unsigned int last_usecs;
};
#endif	KERNEL
#endif	_EVENTC_



