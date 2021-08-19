/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 *  5-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Isolate machine dependencies - machine/time_stamp chooses a
 *	TS_FORMAT, if not chosen this module defaults it to 1.  Also
 *	guarded against multiple inclusion.
 *
 * 30-Mar-87  David Black (dlb) at Carnegie-Mellon University
 *	Created.
 *
 **********************************************************************
 */ 

#ifndef	_SYS_TIME_STAMP_
#define _SYS_TIME_STAMP_

#import <sys/kern_return.h>
#import <machine/time_stamp.h>
/*
 *	time_stamp.h -- definitions for low-overhead timestamps.
 */

struct tsval {
	unsigned	low_val;	/* least significant word */
	unsigned	high_val;	/* most significant word */
};

/*
 *	Format definitions.
 */

#ifndef	TS_FORMAT
/*
 *	Default case - Just return a tick count for machines that
 *	don't support or haven't implemented this.  Assume 100Hz ticks.
 *
 *	low_val - Always 0.
 *	high_val - tick count.
 */
#define	TS_FORMAT	1

#if	KERNEL
extern unsigned ts_tick_count;
#endif	KERNEL
#endif	TS_FORMAT

#if	NeXT
extern kern_return_t kern_timestamp(struct tsval *);
#else	NeXT
#if	KERNEL
extern kern_return_t kern_timestamp();
#endif	KERNEL
#endif	NeXT

/*
 *	List of all format definitions for convert_ts_to_tv.
 */

#define	TS_FORMAT_DEFAULT	1
#define TS_FORMAT_MMAX		2
#define TS_FORMAT_NeXT		3
#endif	_SYS_TIME_STAMP_
