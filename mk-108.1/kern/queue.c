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
 * $Log:	queue.c,v $
 * 21-May-89  Avadis Tevanian, Jr. (avie) at NeXT, Inc.
 *	Use inline expansion if compiling with the GNU compiler.
 *
 * Revision 2.3  89/02/25  18:07:42  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  89/01/31  01:21:11  rpd
 * 	Multimax inline handles all queue routines now.
 * 	[88/05/19            dlb]
 * 
 * 17-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	Created from routines written by David L. Black.
 *
 */ 

/*
 *	Routines to implement queue package.
 */

#ifdef	__GNU__
#define	INLINE	static inline
#else	__GNU__
#import <kern/queue.h>
#define INLINE
#define	COMPILE_QUEUE		/* force compile of this module */
#endif	__GNU__

#ifdef	COMPILE_QUEUE
/*
 *	Insert element at head of queue.
 */
INLINE void enqueue_head(que, elt)
	register queue_t	que;
	register queue_entry_t	elt;
{
	elt->next = que->next;
	elt->prev = que;
	elt->next->prev = elt;
	que->next = elt;
}

/*
 *	Insert element at tail of queue.
 */
INLINE void enqueue_tail(que,elt)
	register queue_t	que;
	register queue_entry_t	elt;
{
	elt->next = que;
	elt->prev = que->prev;
	elt->prev->next = elt;
	que->prev = elt;
}

/*
 *	Remove and return element at head of queue.
 */
INLINE queue_entry_t dequeue_head(que)
	register queue_t	que;
{
	register queue_entry_t	elt;

	if (que->next == que)
		return((queue_entry_t)0);

	elt = que->next;
	elt->next->prev = que;
	que->next = elt->next;
	return(elt);
}

/*
 *	Remove and return element at tail of queue.
 */
INLINE queue_entry_t dequeue_tail(que)
	register queue_t	que;
{
	register queue_entry_t	elt;

	if (que->prev == que)
		return((queue_entry_t)0);

	elt = que->prev;
	elt->prev->next = que;
	que->prev = elt->prev;
	return(elt);
}

/*
 *	Remove arbitrary element from queue.
 *	Does not check whether element is on queue - the world
 *	will go haywire if it isn't.
 */

/*ARGSUSED*/
INLINE void remqueue(que, elt)
	queue_t			que;
	register queue_entry_t	elt;
{
	elt->next->prev = elt->prev;
	elt->prev->next = elt->next;
}

/*
 *	Routines to directly imitate the VAX hardware queue
 *	package.
 */
INLINE insque(entry, pred)
	register queue_entry_t entry, pred;
{
	entry->next = pred->next;
	entry->prev = pred;
	(pred->next)->prev = entry;
	pred->next = entry;
}

INLINE remque(elt)
	register queue_entry_t elt;
{
	(elt->next)->prev = elt->prev;
	(elt->prev)->next = elt->next;
	return((int)elt);
}

#endif	COMPILE_QUEUE
#undef	INLINE




