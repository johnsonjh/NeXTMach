/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 15-Feb-88  Gregg Kellogg (gk) at NeXT
 *	NeXT: use microsecond counter like multimax
 *
 * 16-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	machtimer.h --> timer.h  Changed to cpp symbols for multimax.
 *
 *  5-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	ts.h -> time_stamp.h
 *
 * 30-Mar-87  David Black (dlb) at Carnegie-Mellon University
 *	Created.
 */ 

#import <sys/kern_return.h>
#import <sys/time_stamp.h>
#if	NeXT
#import <next/eventc.h>	// definition of event_set_ts()
#endif	NeXT

/*
 *	ts.c - kern_timestamp system call.
 */

kern_return_t kern_timestamp(tsp)
	struct	tsval	*tsp;
{
	struct	tsval	temp;
	event_set_ts(&temp);

	if (copyout(&temp, tsp, sizeof(struct tsval)) != KERN_SUCCESS)
		return(KERN_INVALID_ADDRESS);
	return(KERN_SUCCESS);
}

/*
 *	Initialization procedure.
 */

timestamp_init()
{
}


