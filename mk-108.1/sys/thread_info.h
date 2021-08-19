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
 * $Log:	thread_info.h,v $
 * Revision 2.6  89/10/11  14:41:37  dlb
 * 	Add scheduling information.
 * 	[89/05/18            dlb]
 * 
 * Revision 2.5  89/03/09  20:24:12  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:41:18  gm0w
 * 	Changes for cleanup.
 * 
 *  4-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	Added TH_USAGE_SCALE for cpu_usage field.
 *
 * 15-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Changed to generic interface (variable-length array) to allow
 *	for expansion.  Renamed to thread_info.
 *
 *  1-Jun-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 *	File:	sys/thread_info
 *
 *	Thread information structure and definitions.
 *
 *	The defintions in this file are exported to the user.  The kernel
 *	will translate its internal data structures to these structures
 *	as appropriate.
 *
 */

#ifndef	_SYS_THREAD_INFO_H_
#define _SYS_THREAD_INFO_H_

#import <sys/policy.h>
#import <sys/time_value.h>

/*
 *	Generic information structure to allow for expansion.
 */
typedef	int	*thread_info_t;		/* varying array of int */

#define THREAD_INFO_MAX		(1024)	/* maximum array size */
typedef	int	thread_info_data_t[THREAD_INFO_MAX];

/*
 *	Currently defined information.
 */
#define THREAD_BASIC_INFO	1		/* basic information */

struct thread_basic_info {
	time_value_t	user_time;	/* user run time */
	time_value_t	system_time;	/* system run time */
	int		cpu_usage;	/* scaled cpu usage percentage */
	int		base_priority;	/* base scheduling priority */
	int		cur_priority;	/* current scheduling priority */
	int		run_state;	/* run state (see below) */
	int		flags;		/* various flags (see below) */
	int		suspend_count;	/* suspend count for thread */
	long		sleep_time;	/* number of seconds that thread
					   has been sleeping */
};

typedef struct thread_basic_info	thread_basic_info_data_t;
typedef struct thread_basic_info	*thread_basic_info_t;
#define THREAD_BASIC_INFO_COUNT	\
		(sizeof(thread_basic_info_data_t) / sizeof(int))

/*
 *	Scale factor for usage field.
 */

#define TH_USAGE_SCALE	1000

/*
 *	Thread run states (state field).
 */

#define TH_STATE_RUNNING	1	/* thread is running normally */
#define TH_STATE_STOPPED	2	/* thread is stopped */
#define TH_STATE_WAITING	3	/* thread is waiting normally */
#define TH_STATE_UNINTERRUPTIBLE 4	/* thread is in an uninterruptible
					   wait */
#define TH_STATE_HALTED		5	/* thread is halted at a
					   clean point */

/*
 *	Thread flags (flags field).
 */
#define TH_FLAGS_SWAPPED	0x1	/* thread is swapped out */
#define TH_FLAGS_IDLE		0x2	/* thread is an idle thread */

#define THREAD_SCHED_INFO	2

struct thread_sched_info {
	int		policy;		/* scheduling policy */
	int		data;		/* associated data */
	int		base_priority;	/* base priority */
	int		max_priority;   /* max priority */
	int		cur_priority;	/* current priority */
	boolean_t	depressed;	/* depressed ? */
	int		depress_priority; /* priority depressed from */
};

typedef struct thread_sched_info	thread_sched_info_data_t;
typedef struct thread_sched_info	*thread_sched_info_t;
#define	THREAD_SCHED_INFO_COUNT	\
		(sizeof(thread_sched_info_data_t) / sizeof(int))

#endif	_SYS_THREAD_INFO_H_


