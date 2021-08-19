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
 * $Log:	task_info.h,v $
 * 31-May-90  Gregg Kellogg (gk) at NeXT
 *	NeXT doesn't implement TASK_EVENTS_INFO (neither does anyone else, so
 *	far as I can see).
 *
 * Revision 2.4  89/03/09  20:23:59  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:41:06  gm0w
 * 	Changes for cleanup.
 * 
 * 15-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Created, based on old task_statistics.
 *
 */
/*
 *	Machine-independent task information structures and definitions.
 *
 *	The definitions in this file are exported to the user.  The kernel
 *	will translate its internal data structures to these structures
 *	as appropriate.
 *
 */

#ifndef	_SYS_TASK_INFO_H_
#define _SYS_TASK_INFO_H_

#import <machine/vm_types.h>
#import <sys/time_value.h>

/*
 *	Generic information structure to allow for expansion.
 */
typedef	int	*task_info_t;		/* varying array of int */

#define TASK_INFO_MAX	(1024)		/* maximum array size */
typedef	int	task_info_data_t[TASK_INFO_MAX];

/*
 *	Currently defined information structures.
 */
#define TASK_BASIC_INFO		1	/* basic information */

struct task_basic_info {
	int		suspend_count;	/* suspend count for task */
	int		base_priority;	/* base scheduling priority */
	vm_size_t	virtual_size;	/* number of virtual pages */
	vm_size_t	resident_size;	/* number of resident pages */
	time_value_t	user_time;	/* total user run time for
					   terminated threads */
	time_value_t	system_time;	/* total system run time for
					   terminated threads */
};

typedef struct task_basic_info		task_basic_info_data_t;
typedef struct task_basic_info		*task_basic_info_t;
#define TASK_BASIC_INFO_COUNT	\
		(sizeof(task_basic_info_data_t) / sizeof(int))

#if	!NeXT
#define TASK_EVENTS_INFO	2	/* various event counts */

struct task_events_info {
	long		faults;		/* number of page faults */
	long		zero_fills;	/* number of zero fill pages */
	long		reactivations;	/* number of reactivated pages */
	long		pageins;	/* number of actual pageins */
	long		cow_faults;	/* number of copy-on-write faults */
	long		messages_sent;	/* number of messages sent */
	long		messages_received; /* number of messages received */
};
typedef struct task_events_info		task_events_info_data_t;
typedef struct task_events_info		*task_events_info_t;
#define TASK_EVENTS_INFO_COUNT	\
		(sizeof(task_events_info_data_t) / sizeof(int))
#endif	!NeXT

#endif	_SYS_TASK_INFO_H_

