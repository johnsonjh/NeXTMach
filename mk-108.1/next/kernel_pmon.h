/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * This file is the single kernel kernel include file for the pmon interface.
 *
 * HISTORY
 *
 * 1-Mar-90	Brian Pinkerton at NeXT
 *	Created
 *
 */

#ifdef DEBUG

#define PMON	1			/* make PMON conditional on DEBUG */

#import <sys/types.h>
#import <sys/proc.h>
#import <sys/time_stamp.h>
#import <next/eventc.h>
#import <next/pmon.h>			/* generic pmon interface */
#import <next/pmon_targets.h>		/* list of kernel pmon targets */


extern int pmon_flags[];		/* currently enabled event masks */
extern void(*pmon_event_log_p)();	/* pointer to pmon_event_log() 
					   (set by the kern_loader)  */


/*
 *  This is the usual interface to pmon in the kernel.  It just checks to
 *  see if the source,event_type pair is enabled, and if so, grabs the 
 *  current clock and logs the event.
 */

static inline void pmon_log_event(source, event_type, data1, data2, data3)
	int source, event_type;
	int data1, data2, data3;
{
	if (pmon_flags[source] & event_type) {
	    struct tsval event_timestamp;
    
	    event_set_ts(&event_timestamp);
	    (*pmon_event_log_p)(source,
		    event_type,
		    &event_timestamp,
		    data1,
		    data2,
		    data3);
	}
}

#else DEBUG


/*
 *  If we don't have a debug kernel, make sure to turn off pmon event
 *  logging.  We import the definitions anyway, so nothing will be undefined.
 */

#import <next/pmon.h>			/* generic pmon interface */
#import <next/pmon_targets.h>		/* list of kernel pmon targets */

#define pmon_log_event(source, event_type, data1, data2, data3)

#endif DEBUG



