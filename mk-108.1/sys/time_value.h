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
 * $Log:	time_value.h,v $
 * Revision 2.5  89/03/09  20:24:28  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:41:34  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.3  89/02/07  00:53:58  mwyoung
 * Relocated from sys/time_value.h
 * 
 * Revision 2.2  89/01/31  01:21:58  rpd
 * 	TIME_MICROS_MAX should be 1 Million, not 10 Million.
 * 	[88/10/12            dlb]
 * 
 *  4-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Created.
 *
 */

#ifndef	_SYS_TIME_VALUE_H_
#define _SYS_TIME_VALUE_H_

/*
 *	Time value returned by kernel.
 */

struct time_value {
	long	seconds;
	long	microseconds;
};
typedef	struct time_value	time_value_t;

/*
 *	Macros to manipulate time values.  Assume that time values
 *	are normalized (microseconds <= 999999).
 */
#define TIME_MICROS_MAX	(1000000)

#define time_value_add_usec(val, micros)	{	\
	if (((val)->microseconds += (micros))		\
		>= TIME_MICROS_MAX) {			\
	    (val)->microseconds -= TIME_MICROS_MAX;	\
	    (val)->seconds++;				\
	}						\
}

#define time_value_add(result, addend)		{		\
	(result)->microseconds += (addend)->microseconds;	\
	(result)->seconds += (addend)->seconds;			\
	if ((result)->microseconds >= TIME_MICROS_MAX) {	\
	    (result)->microseconds -= TIME_MICROS_MAX;		\
	    (result)->seconds++;				\
	}							\
}

#endif	_SYS_TIME_VALUE_H_

