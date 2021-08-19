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
 * $Log:	ipc_statistics.h,v $
 * Revision 2.7  89/05/11  14:41:42  gm0w
 * 	Added ipc_bucket_info_t, ipc_bucket_info_array_t.
 * 	[89/05/07  20:12:10  rpd]
 * 
 * Revision 2.6  89/05/01  17:03:24  rpd
 * 	Renamed port_copyout_hits to port_copyin_miss.
 * 	[89/05/01  14:12:32  rpd]
 * 
 * Revision 2.5  89/03/09  20:26:08  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:43:30  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.3  89/01/15  16:32:13  rpd
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  15:10:25  rpd]
 * 
 * Revision 2.2  89/01/12  07:59:55  rpd
 * 	Created.
 * 	[89/01/12  04:23:06  rpd]
 * 
 */ 

#ifndef	_KERN_IPC_STATISTICS_H_
#define _KERN_IPC_STATISTICS_H_

/*
 *	Remember to update the mig type definition
 *	in mach_debug_types.defs when adding/removing fields.
 */

typedef struct ipc_statistics {
	int		version;
	int		messages;
	int		complex;
	int		kernel;
	int		large;
	int		current;
	int		emergency;
	int		notifications;
	int		port_copyins;
	int		port_copyouts;
	int		port_copyin_hits;
	int		port_copyin_miss;
	int		port_allocations;
	int		bucket_misses;
	int		ip_data_grams;
} ipc_statistics_t;

typedef struct ipc_bucket_info {
	int		count;		/* number of records in bucket */
} ipc_bucket_info_t;

typedef ipc_bucket_info_t *ipc_bucket_info_array_t;

#ifdef	KERNEL
#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_ipc_stats.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <kern/lock.h>
#import <kern/macro_help.h>

decl_simple_lock_data(extern,ipc_statistics_lock_data)
extern ipc_statistics_t ipc_statistics;

extern void ipc_stats_init();

#define ipc_statistics_lock()	simple_lock(&ipc_statistics_lock_data)
#define ipc_statistics_unlock()	simple_unlock(&ipc_statistics_lock_data)

#if	MACH_IPC_STATS
#define ipc_event_count(field, count)		\
MACRO_BEGIN					\
	ipc_statistics_lock();			\
	ipc_statistics.field += count;		\
	ipc_statistics_unlock();		\
MACRO_END
#else	MACH_IPC_STATS
#define ipc_event_count(field, count)
#endif	MACH_IPC_STATS

#define ipc_event(field)		ipc_event_count(field, 1)

#endif	KERNEL
#endif	_KERN_IPC_STATISTICS_H_

