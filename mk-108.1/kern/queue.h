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
 * $Log:	queue.h,v $
 * 21-May-89  Avadis Tevanian, Jr. (avie) at NeXT, Inc.
 *	Use inline expansion if compiling with the GNU compiler.
 *
 * Revision 2.7  89/03/09  20:15:03  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  18:07:48  gm0w
 * 	Kernel code cleanup.	
 * 	Put all the macros under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.5  89/02/07  01:03:50  mwyoung
 * Relocated from sys/queue.h
 * 
 * Revision 2.4  88/12/19  02:51:30  mwyoung
 * 	Eliminate round_queue.
 * 	[88/11/22  01:22:22  mwyoung]
 * 	
 * 	Add queue_remove_last.
 * 	[88/11/18            mwyoung]
 * 
 * Revision 2.3  88/08/24  02:40:43  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:20:58  mwyoung]
 * 
 *
 * 17-Jan-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Added queue_enter_first, queue_last and queue_prev for use by
 *	the TCP netport code.
 *
 * 17-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	Made queue package return queue_entry_t instead of vm_offset_t.
 *
 * 27-Feb-87  David Golub (dbg) at Carnegie-Mellon University
 *	Made remqueue a real routine on all machines.  Defined old
 *	Queue routines as macros that invoke new queue routines.
 *
 * 25-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Fixed non-KERNEL compilation.
 *
 * 24-Feb-87  David L. Black (dlb) at Carnegie-Mellon University
 *	MULTIMAX: remqueue is now a real routine, removed define.
 *	This is done to mask a compiler bug.
 *
 * 12-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */
/*
 *	File:	queue.h
 *
 *	Type definitions for generic queues.
 */

#ifndef	_KERN_QUEUE_H_
#define _KERN_QUEUE_H_

#import <machine/vm_types.h>
#import <kern/lock.h>
#import <kern/macro_help.h>

/*
 *	Queue of abstract objects.  Queue is maintained
 *	within that object.
 *
 *	Supports fast removal from within the queue.
 *
 *	How to declare a queue of elements of type "foo_t":
 *		In the "*foo_t" type, you must have a field of
 *		type "queue_chain_t" to hold together this queue.
 *		There may be more than one chain through a
 *		"foo_t", for use by different queues.
 *
 *		Declare the queue as a "queue_t" type.
 *
 *		Elements of the queue (of type "foo_t", that is)
 *		are referred to by reference, and cast to type
 *		"queue_entry_t" within this module.
 */

/*
 *	A generic doubly-linked list (queue).
 */

struct queue_entry {
	struct queue_entry	*next;		/* next element */
	struct queue_entry	*prev;		/* previous element */
};

typedef struct queue_entry	*queue_t;
typedef	struct queue_entry	queue_head_t;
typedef	struct queue_entry	queue_chain_t;
typedef	struct queue_entry	*queue_entry_t;

/*
 *	enqueue puts "elt" on the "queue".
 *	dequeue returns the first element in the "queue".
 *	remqueue removes the specified "elt" from the specified "queue".
 */

#define enqueue(queue,elt)	enqueue_tail(queue, elt)
#define	dequeue(queue)		dequeue_head(queue)

#if	defined(__GNU__) && defined(KERNEL) && !defined(KERNEL_FEATURES)
#define	COMPILE_QUEUE		/* flag that queue.c should compile now */
#import <kern/queue.c>
#else	defined(__GNU__) && defined(KERNEL) && !defined(KERNEL_FEATURES)
extern void		enqueue_head();
extern void		enqueue_tail();
extern queue_entry_t	dequeue_head();
extern queue_entry_t	dequeue_tail();
extern void		remqueue();
#endif	defined(__GNU__) && defined(KERNEL) && !defined(KERNEL_FEATURES)


/*
 *	Macro:		queue_init
 *	Function:
 *		Initialize the given queue.
 *	Header:
 *		void queue_init(q)
 *			queue_t		q;	\* MODIFIED *\
 */
#define	queue_init(q)	((q)->next = (q)->prev = q)

/*
 *	Macro:		queue_first
 *	Function:
 *		Returns the first entry in the queue,
 *	Header:
 *		queue_entry_t queue_first(q)
 *			queue_t	q;		\* IN *\
 */
#define	queue_first(q)	((q)->next)

/*
 *	Macro:		queue_next
 *	Header:
 *		queue_entry_t queue_next(qc)
 *			queue_t qc;
 */
#define	queue_next(qc)	((qc)->next)

/*
 *	Macro:		queue_end
 *	Header:
 *		boolean_t queue_end(q, qe)
 *			queue_t q;
 *			queue_entry_t qe;
 */
#define	queue_end(q, qe)	((q) == (qe))

#define	queue_empty(q)		queue_end((q), queue_first(q))

/*
 *	Macro:		queue_enter
 *	Header:
 *		void queue_enter(q, elt, type, field)
 *			queue_t q;
 *			<type> elt;
 *			<type> is what's in our queue
 *			<field> is the chain field in (*<type>)
 */
#define queue_enter(head, elt, type, field)			\
MACRO_BEGIN							\
	if (queue_empty((head))) {				\
		(head)->next = (queue_entry_t) elt;		\
		(head)->prev = (queue_entry_t) elt;		\
		(elt)->field.next = head;			\
		(elt)->field.prev = head;			\
	}							\
	else {							\
		register queue_entry_t prev;			\
								\
		prev = (head)->prev;				\
		(elt)->field.prev = prev;			\
		(elt)->field.next = head;			\
		(head)->prev = (queue_entry_t)(elt);		\
		((type)prev)->field.next = (queue_entry_t)(elt);\
	}							\
MACRO_END

/*
 *	Macro:		queue_field [internal use only]
 *	Function:
 *		Find the queue_chain_t (or queue_t) for the
 *		given element (thing) in the given queue (head)
 */
#define	queue_field(head, thing, type, field)			\
		(((head) == (thing)) ? (head) : &((type)(thing))->field)

/*
 *	Macro:		queue_remove
 *	Header:
 *		void queue_remove(q, qe, type, field)
 *			arguments as in queue_enter
 */
#define	queue_remove(head, elt, type, field)			\
MACRO_BEGIN							\
	register queue_entry_t	next, prev;			\
								\
	next = (elt)->field.next;				\
	prev = (elt)->field.prev;				\
								\
	queue_field((head), next, type, field)->prev = prev;	\
	queue_field((head), prev, type, field)->next = next;	\
MACRO_END

/*
 *	Macro:		queue_assign
 */
#define	queue_assign(to, from, type, field)			\
MACRO_BEGIN							\
	((type)((from)->prev))->field.next = (to);		\
	((type)((from)->next))->field.prev = (to);		\
	*to = *from;						\
MACRO_END

#define	queue_remove_first(h, e, t, f)				\
MACRO_BEGIN							\
	e = (t) queue_first((h));				\
	queue_remove((h), (e), t, f);				\
MACRO_END

#define queue_remove_last(h, e, t, f)				\
MACRO_BEGIN							\
	e = (t) queue_last((h));				\
	queue_remove((h), (e), t, f);				\
MACRO_END

/*
 *	Macro:		queue_enter_first
 *	Header:
 *		void queue_enter_first(q, elt, type, field)
 *			queue_t q;
 *			<type> elt;
 *			<type> is what's in our queue
 *			<field> is the chain field in (*<type>)
 */
#define queue_enter_first(head, elt, type, field)		\
MACRO_BEGIN							\
	if (queue_empty((head))) {				\
		(head)->next = (queue_entry_t) elt;		\
		(head)->prev = (queue_entry_t) elt;		\
		(elt)->field.next = head;			\
		(elt)->field.prev = head;			\
	}							\
	else {							\
		register queue_entry_t next;			\
								\
		next = (head)->next;				\
		(elt)->field.prev = head;			\
		(elt)->field.next = next;			\
		(head)->next = (queue_entry_t)(elt);		\
		((type)next)->field.prev = (queue_entry_t)(elt);\
	}							\
MACRO_END

/*
 *	Macro:		queue_last
 *	Function:
 *		Returns the last entry in the queue,
 *	Header:
 *		queue_entry_t queue_last(q)
 *			queue_t	q;		\* IN *\
 */
#define	queue_last(q)	((q)->prev)

/*
 *	Macro:		queue_prev
 *	Header:
 *		queue_entry_t queue_prev(qc)
 *			queue_t qc;
 */
#define	queue_prev(qc)	((qc)->prev)

/*
 *	Define macros for queues with locks.
 */
struct mpqueue_head {
	struct queue_entry	head;		/* header for queue */
	struct slock		lock;		/* lock for queue */
};

typedef struct mpqueue_head	mpqueue_head_t;

#define round_mpq(size)		(size)

#define mpqueue_init(q)						\
MACRO_BEGIN							\
	queue_init(&(q)->head);					\
	simple_lock_init(&(q)->lock);				\
MACRO_END

#define mpenqueue_tail(q, elt)					\
MACRO_BEGIN							\
	simple_lock(&(q)->lock);				\
	enqueue_tail(&(q)->head, elt);				\
	simple_unlock(&(q)->lock);				\
MACRO_END

#define mpdequeue_head(q, elt)					\
MACRO_BEGIN							\
	simple_lock(&(q)->lock);				\
	if (queue_empty(&(q)->head))				\
		*(elt) = 0;					\
	else							\
		*(elt) = dequeue_head(&(q)->head);		\
	simple_unlock(&(q)->lock);				\
MACRO_END

#endif	_KERN_QUEUE_H_




