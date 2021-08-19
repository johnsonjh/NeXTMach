/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)callout.h	7.1 (Berkeley) 6/4/86
 *
 * HISTORY
 * 17-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Added callout_lock.
 *
 * 22-Dec-87  Gregg Kellogg (gk) at NeXT
 *	Added c_timeval entry for maintaining microsecond accurate
 *	timeouts.
 */

#ifndef	__CALLOUT__
#define __CALLOUT__
#if	NeXT
#import <sys/time.h>
#endif	NeXT

/*
 * The callout structure is for
 * a routine arranging
 * to be called by the clock interrupt
 * (clock.c) with a specified argument,
 * in a specified amount of time.
 * Used, for example, to time tab
 * delays on typewriters.
 */

typedef int (*func)(void *);

struct	callout {
	int	c_time;		/* incremental time */
	void	*c_arg;		/* argument to routine */
	func	c_func;		/* routine */
	struct	callout *c_next;
#if	NeXT
	struct	timeval c_timeval;	/* absolute time for this interrupt */
	int	c_pri;			/* priority at which to call c_func */
#endif	NeXT
};
#ifdef	KERNEL
#import <kern/lock.h>

extern struct callout *callfree, *callout, calltodo;
extern int ncallout;
decl_simple_lock_data(extern,callout_lock)

#endif	KERNEL

#if	NeXT
#define CALLOUT_PRI_SOFTINT0	0
#define CALLOUT_PRI_SOFTINT1	1
#define CALLOUT_PRI_RETRACE	2
#define CALLOUT_PRI_DSP		3
#define CALLOUT_PRI_THREAD	4	/* run in a thread */
#define CALLOUT_PRI_NOW		5	/* must be last */
#define N_CALLOUT_PRI		6
#endif	NeXT
#endif __CALLOUT__




