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
 * $Log:	mach_user_internal.c,v $
 * 19-Sep-90  Gregg Kellogg
 *	Added vm_read for KERNSERV.
 *
 * Revision 2.6  89/05/06  02:56:58  rpd
 * 	Include just those routines used, instead of <mach/mach_user.c>.
 * 	[89/05/06  02:41:22  rpd]
 * 
 * Revision 2.5  89/04/09  01:55:35  rpd
 * 	Added MIG_NO_STRINGS.
 * 	[89/04/09  01:50:41  rpd]
 * 
 * Revision 2.4  89/03/09  20:14:02  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:06:18  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  89/01/15  16:25:19  rpd
 * 	Updated includes for the new mach/ directory.
 * 	[89/01/15  15:03:54  rpd]
 * 
 */
/*
 *	File:	kern/mach_user_internal.c
 *
 *	Internal form of the mach_user interface, for
 *	use by builtin (kernel-mode) tasks that wish to
 *	use the RPC interface anyway.
 */

#undef	KERNEL
#define MIG_NO_STRINGS	/* tells Mig code to not include strings.h  */
#define __STRICT_BSD__
#define _MACH_INIT_	/* tells mach_types.h to not include mach_init.h */
#import <kern/mach_user_internal.h>	/* renames user stubs to _EXTERNAL */

#import <kern/port_allocate.c>
#import <kern/port_deallocate.c>
#import <kern/port_set_add.c>
#import <kern/port_set_allocate.c>
#import <kern/port_set_backlog.c>
#import <kern/vm_allocate.c>
#import <kern/vm_deallocate.c>

#import <kernserv.h>
#if	KERNSERV
#import <kern/port_set_deallocate.c>
#import <kern/task_set_special_port.c>
#import <kern/task_get_special_port.c>
#import <kern/thread_get_special_port.c>
#import <kern/thread_set_special_port.c>
#import <kern/thread_suspend.c>
#import <kern/vm_read.c>
#endif	KERNSERV

