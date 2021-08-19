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
 * $Log:	mig_support.c,v $
 * Revision 2.5  89/05/01  17:01:28  rpd
 * 	Moved reply_port from task to thread structure.  Don't bother
 * 	allocating a new reply port; just use thread_reply() directly.
 * 	mig_dealloc_reply_port panics now.
 * 	[89/05/01  14:04:12  rpd]
 * 
 * Revision 2.4  89/02/25  18:06:54  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.3  89/01/15  16:25:57  rpd
 * 	Updated includes for the new mach/ directory.
 * 	[89/01/15  15:05:21  rpd]
 * 
 * Revision 2.2  88/07/17  19:04:52  mwyoung
 * .
 * 
 * Revision 2.1.1.1  88/06/28  20:39:03  mwyoung
 * Include <kern/thread.h> to get current_task().
 *
 * 18-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Changed task_data to thread_reply.
 *
 */
/*
 *	File:	kern/mig_support.c
 *
 *	Support for the Mach Interface Generator, as required
 *	for built-in tasks.
 */

#define _MACH_INIT_
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/mach_user_internal.h>

port_t mig_get_reply_port()
{
	register thread_t self = current_thread();

	if (self->reply_port == PORT_NULL)
		self->reply_port = thread_reply();
	return self->reply_port;
}

void mig_dealloc_reply_port()
{
	panic("mach_user_internal: deallocate");
}

