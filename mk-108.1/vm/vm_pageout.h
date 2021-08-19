/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_pageout.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr.
 *
 *	Header file for pageout daemon.
 *
 ************************************************************************
 * HISTORY
 * 19-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Declare vm_pageout_page().
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 * 15-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Converted to new include technology.
 *
 * 17-Feb-87  David Golub (dbg) at Carnegie-Mellon University
 *	Added lock (to avoid losing wakeups).  Moved VM_WAIT from
 *	vm_page.h to this file.
 *
 *  9-Mar-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 ************************************************************************
 */

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_xp.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <kern/lock.h>
#import <kern/sched_prim.h>

/*
 *	Exported data structures.
 */

extern int	vm_pages_needed;	/* should be some "event" structure */
simple_lock_data_t	vm_pages_needed_lock;


/*
 *	Exported routines.
 */

#if	MACH_XP
void	vm_pageout_page();
#endif	MACH_XP

/*
 *	Signal pageout-daemon and wait for it.
 */

#define	VM_WAIT		{ \
			simple_lock(&vm_pages_needed_lock); \
			thread_wakeup((int)&vm_pages_needed); \
			thread_sleep((int)&vm_page_free_count, \
				&vm_pages_needed_lock, FALSE); \
			}
