/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	processor_info.h,v $
 * Revision 2.3  89/10/15  02:05:54  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:41:03  dlb
 * 	Add scheduling flavor of information.
 * 
 * 	Add load average and mach factor to processor set basic info.
 * 	[89/02/09            dlb]
 * 
 * Revision 2.1.1.4  89/08/02  23:12:21  dlb
 * 	Merge to X96
 * 
 * Revision 2.1.1.3  89/07/25  18:52:18  dlb
 * 	Add scheduling flavor of information.
 * 
 * Revision 2.1.1.2  89/02/13  23:05:49  dlb
 * 	Add load average and mach factor to processor set basic info.
 * 	[89/02/09            dlb]
 * 
 */

/*
 *	File:	mach/processor_info.h
 *	Author:	David L. Black
 *
 *	Copyright (C) 1988 David L. Black
 *
 *	Data structure definitions for processor_info, processor_set_info
 */

#ifndef	_SYS_PROCESSOR_INFO_H_
#define _SYS_PROCESSOR_INFO_H_

#import <sys/machine.h>

/*
 *	Generic information structure to allow for expansion.
 */
typedef int	*processor_info_t;	/* varying array of int. */

#define PROCESSOR_INFO_MAX	(1024)	/* max array size */
typedef int	processor_info_data_t[PROCESSOR_INFO_MAX];


typedef int	*processor_set_info_t;	/* varying array of int. */

#define PROCESSOR_SET_INFO_MAX	(1024)	/* max array size */
typedef int	processor_set_info_data_t[PROCESSOR_SET_INFO_MAX];

/*
 *	Currently defined information.
 */
#define	PROCESSOR_BASIC_INFO	1		/* basic information */

struct processor_basic_info {
	cpu_type_t	cpu_type;	/* type of cpu */
	cpu_subtype_t	cpu_subtype;	/* subtype of cpu */
	boolean_t	running;	/* is processor running */
	int		slot_num;	/* slot number */
	boolean_t	is_master;	/* is this the master processor */
};

typedef	struct processor_basic_info	processor_basic_info_data_t;
typedef struct processor_basic_info	*processor_basic_info_t;
#define PROCESSOR_BASIC_INFO_COUNT \
		(sizeof(processor_basic_info_data_t)/sizeof(int))


#define	PROCESSOR_SET_BASIC_INFO	1	/* basic information */

struct processor_set_basic_info {
	int		processor_count;	/* How many processors */
	int		task_count;		/* How many tasks */
	int		thread_count;		/* How many threads */
	int		load_average;		/* Scaled */
	int		mach_factor;		/* Scaled */
};

/*
 *	Scaling factor for load_average, mach_factor.
 */
#define	LOAD_SCALE	1000		

typedef	struct processor_set_basic_info	processor_set_basic_info_data_t;
typedef struct processor_set_basic_info	*processor_set_basic_info_t;
#define PROCESSOR_SET_BASIC_INFO_COUNT \
		(sizeof(processor_set_basic_info_data_t)/sizeof(int))

#define PROCESSOR_SET_SCHED_INFO	2	/* scheduling info */

struct processor_set_sched_info {
	int		policies;	/* allowed policies */
	int		max_priority;	/* max priority for new threads */
};

typedef	struct processor_set_sched_info	processor_set_sched_info_data_t;
typedef struct processor_set_sched_info	*processor_set_sched_info_t;
#define PROCESSOR_SET_SCHED_INFO_COUNT \
		(sizeof(processor_set_sched_info_data_t)/sizeof(int))

#endif	_SYS_PROCESSOR_INFO_H_

