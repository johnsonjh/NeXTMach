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
 * $Log:	msg_queue.h,v $
 * Revision 2.8  89/03/09  20:14:29  rpd
 * 	More cleanup.
 * 
 * Revision 2.7  89/02/25  18:07:02  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.6  89/02/07  01:03:25  mwyoung
 * Relocated from sys/msg_queue.h
 * 
 * Revision 2.5  89/01/15  16:34:26  rpd
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  15:17:21  rpd]
 * 
 * Revision 2.4  88/10/18  03:39:04  mwyoung
 * 	Use <kern/macro_help.h> to avoid lint.
 * 	[88/10/15            mwyoung]
 * 
 * Revision 2.3  88/08/24  02:36:51  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:17:59  mwyoung]
 * 
 * Revision 2.2  88/07/20  21:06:22  rpd
 * Added msg_queue_* macro definitions.
 * Eliminate dual emergency/normal queues;
 * leave unified messages queue.
 * 
 * 22-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Removed msg_queue_t.saved_spl field.
 *
 * 12-Mar-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added msg_queue_t.saved_spl field.
 *
 * 28-Feb-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Split up emergency and normal messages.
 *
 *  8-Jan-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Reorganized structure.
 *
 * 18-Nov-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Created.
 *
 */ 

#ifndef	_KERN_MSG_QUEUE_H_
#define _KERN_MSG_QUEUE_H_

#import <kern/queue.h>
#import <kern/lock.h>
#import <kern/macro_help.h>

typedef struct {
	queue_head_t messages;
	decl_simple_lock_data(,lock)
	queue_head_t blocked_threads;
} msg_queue_t;

#define msg_queue_lock(mq)	simple_lock(&(mq)->lock)
#define msg_queue_unlock(mq)	simple_unlock(&(mq)->lock)

#define msg_queue_init(mq)			\
MACRO_BEGIN					\
	simple_lock_init(&(mq)->lock);		\
	queue_init(&(mq)->messages);		\
	queue_init(&(mq)->blocked_threads);	\
MACRO_END

#endif	_KERN_MSG_QUEUE_H_
