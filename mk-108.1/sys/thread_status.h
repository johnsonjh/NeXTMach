/*
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	thread_status.h,v $
 * Revision 2.5  89/03/09  20:24:23  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:41:29  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.3  89/02/07  00:53:47  mwyoung
 * Relocated from mach/thread_status.h
 * 
 * Revision 2.2  88/08/25  18:21:12  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/16  04:16:13  mwyoung]
 * 	
 * 	Add THREAD_STATE_FLAVOR_LIST; remove old stuff.
 * 	[88/08/11  18:49:48  mwyoung]
 * 
 *
 * 15-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Replaced with variable-length array for flexibile interface.
 *
 * 28-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Latest hacks to keep MiG happy wrt refarrays.
 *
 * 27-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 *	File:	sys/thread_status.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	This file contains the structure definitions for the user-visible
 *	thread state.  This thread state is examined with the thread_get_state
 *	kernel call and may be changed with the thread_set_state kernel call.
 *
 */

#ifndef	_SYS_THREAD_STATUS_H_
#define _SYS_THREAD_STATUS_H_

/*
 *	The actual structure that comprises the thread state is defined
 *	in the machine dependent module.
 */
#import <machine/thread_status.h>

/*
 *	Generic definition for machine-dependent thread status.
 */

typedef	int		*thread_state_t;	/* Variable-length array */

#define THREAD_STATE_MAX	(1024)		/* Maximum array size */
typedef	int	thread_state_data_t[THREAD_STATE_MAX];

#define THREAD_STATE_FLAVOR_LIST	0	/* List of valid flavors */
#define THREAD_STATE_FLAVORS		0	/* Compat with NeXT 1.0 */

struct thread_state_flavor {
	int	flavor;			/* the number for this flavor */
	int	count;			/* count of ints in this flavor */
};

#endif	_SYS_THREAD_STATUS_H_


