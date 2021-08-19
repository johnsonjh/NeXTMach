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
 * $Log:	lock.c,v $
 * 19-Apr-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: declared simple_lock_alloc(), simple_lock_free, lock_alloc()
 *	and lock_free() needed for loadable servers.
 *
 *  6-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Added mach_ldebug flag for optionally turning on and off
 *	simple lock debugging.
 *	NeXT: Record pc at which lock was taken, rather than just 1
 *	when debugging locks.
 *
 * Revision 2.6  89/10/11  14:12:24  dlb
 * 	Add debugging panics to check for lock corruption (-1 read
 * 	count).
 * 	[89/08/15            dlb]
 * 
 * Revision 2.5.1.1  89/08/17  17:27:19  dlb
 * 	Add debugging panics to check for lock corruption (-1 read
 * 	count).
 * 	[89/08/15            dlb]
 * 
 * Revision 2.5  89/02/25  18:05:22  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.4  89/01/15  16:23:57  rpd
 * 	Use simple_lock_addr when calling thread_sleep.
 * 	[89/01/15  15:01:33  rpd]
 * 
 * Revision 2.3  88/09/25  22:14:45  rpd
 * 	Changed explicit panics in sanity-checking simple locking calls
 * 	to assertions.
 * 	[88/09/12  23:03:22  rpd]
 * 
 * Revision 2.2  88/07/20  16:35:47  rpd
 * Add simple-locking sanity-checking code.
 * 
 * 23-Jan-88  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	On a UNIPROCESSOR, set lock_wait_time = 0.  There is no reason
 *	for a uni to spin on a complex lock.
 *
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminated previous history.
 *
 */
/*
 *	File:	kern/lock.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Locking primitives implementation
 */

#import <cpus.h>

#import <kern/lock.h>
#import <kern/thread.h>
#import <kern/sched_prim.h>

#if	NCPUS > 1

/*
 *	Module:		lock
 *	Function:
 *		Provide reader/writer sychronization.
 *	Implementation:
 *		Simple interlock on a bit.  Readers first interlock
 *		increment the reader count, then let go.  Writers hold
 *		the interlock (thus preventing further readers), and
 *		wait for already-accepted readers to go away.
 */

/*
 *	The simple-lock routines are the primitives out of which
 *	the lock package is built.  The implementation is left
 *	to the machine-dependent code.
 */

#ifdef	notdef
/*
 *	A sample implementation of simple locks.
 *	assumes:
 *		boolean_t test_and_set(boolean_t *)
 *			indivisibly sets the boolean to TRUE
 *			and returns its old value
 *		and that setting a boolean to FALSE is indivisible.
 */
/*
 *	simple_lock_init initializes a simple lock.  A simple lock
 *	may only be used for exclusive locks.
 */

void simple_lock_init(l)
	simple_lock_t	l;
{
	*(boolean_t *)l = FALSE;
}

void simple_lock(l)
	simple_lock_t	l;
{
	while (test_and_set((boolean_t *)l))
		continue;
}

void simple_unlock(l)
	simple_lock_t	l;
{
	*(boolean_t *)l = FALSE;
}

boolean_t simple_lock_try(l)
	simple_lock_t	l;
{
    	return (!test_and_set((boolean_t *)l));
}
#endif	notdef
#endif	NCPUS > 1

#if	NCPUS > 1
int lock_wait_time = 100;
#else	NCPUS > 1

	/*
	 * 	It is silly to spin on a uni-processor as if we
	 *	thought something magical would happen to the
	 *	want_write bit while we are executing.
	 */
int lock_wait_time = 0;
#endif	NCPUS > 1

#if	MACH_LDEBUG
int	mach_ldebug = 1;
#endif	MACH_LDEBUG

#if	NeXT
simple_lock_t simple_lock_alloc()
{
#if	MACH_SLOCKS
	return (simple_lock_t)kalloc(sizeof(simple_lock_data_t));
#else	MACH_SLOCKS
	return 0;
#endif	MACH_SLOCKS
}

void simple_lock_free(l)
	simple_lock_t l;
{
#if	MACH_SLOCKS
	kfree(l, sizeof(*l));
#endif	MACH_SLOCKS
}
#endif	NeXT

#if	MACH_SLOCKS && (defined(ibmrt) || (NCPUS == 1))
/* Need simple lock sanity checking code if simple locks are being
   compiled in, and either we are on RT (which doesn't have any special
   locking code of its own) or we are compiling for a uniprocessor. */

void simple_lock_init(l)
	simple_lock_t l;
{
	l->lock_data = 0;
}

void simple_lock(l)
	simple_lock_t l;
{
#if	NeXT
	register int frompc;
#endif	NeXT

	if (!mach_ldebug)
		return;
#if	NeXT
	asm("	.text");
	asm("	movl a6@(4),%0" : "=a" (frompc));

	if (l->lock_data != 0) {
		printf("simple_lock failure: already taken at pc 0x%x\n",
			l->lock_data);
		printf("simple_lock return pc 0x%x\n", frompc);
	}
#endif	NeXT
	assert(l->lock_data == 0);

#if	NeXT
	l->lock_data = frompc;
#else	NeXT
	l->lock_data = 1;
#endif	NeXT
}

void simple_unlock(l)
	simple_lock_t l;
{
	if (!mach_ldebug)
		return;
#if	NeXT
	if (l->lock_data == 0) {
		register int frompc;

		asm("	.text");
		asm("	movl a6@(4),%0" : "=a" (frompc));
		printf("simple_unlock assertion failed, return pc 0x%x\n",
			frompc);
	}
#endif	NeXT
	assert(l->lock_data != 0);

	l->lock_data = 0;
}

boolean_t simple_lock_try(l)
	simple_lock_t l;
{
#if	NeXT
	register int frompc;
#endif	NeXT
	if (!mach_ldebug)
		return TRUE;
#if	NeXT
	asm("	.text");
	asm("	movl a6@(4),%0" : "=a" (frompc));

	if (l->lock_data != 0) {
		printf("simple_lock_try failure: already taken at pc 0x%x\n",
			l->lock_data);
		printf("simple_lock_try return pc 0x%x\n", frompc);
	}
#endif	NeXT
	assert(l->lock_data == 0);

#if	NeXT
	l->lock_data = frompc;
#else	NeXT
	l->lock_data = 1;
#endif	NeXT

	return TRUE;
}
#endif	MACH_SLOCKS && (defined(ibmrt) || (NCPUS == 1))

#if	NeXT
/*
 *	Routine:	lock_alloc
 *	Function:
 *		Allocate a lock_t data structure.  Used by loadable
 *		servers that can't allocate a lock statically.
 */
lock_t lock_alloc()
{
	return (lock_t)kalloc(sizeof(lock_data_t));
}

/*
 *	Routine:	lock_free
 *	Function:
 *		Free a lock allocated by lock_alloc()
 */
void lock_free(l)
	lock_t	l;
{
	kfree(l, sizeof(lock_data_t));
}
#endif	NeXT

/*
 *	Routine:	lock_init
 *	Function:
 *		Initialize a lock; required before use.
 *		Note that clients declare the "struct lock"
 *		variables and then initialize them, rather
 *		than getting a new one from this module.
 */
void lock_init(l, can_sleep)
	lock_t		l;
	boolean_t	can_sleep;
{
	bzero(l, sizeof(lock_data_t));
	simple_lock_init(&l->interlock);
	l->want_write = FALSE;
	l->want_upgrade = FALSE;
	l->read_count = 0;
	l->can_sleep = can_sleep;
	l->thread = (char *)-1;		/* XXX */
	l->recursion_depth = 0;
}

void lock_sleepable(l, can_sleep)
	lock_t		l;
	boolean_t	can_sleep;
{
	simple_lock(&l->interlock);
	l->can_sleep = can_sleep;
	simple_unlock(&l->interlock);
}


/*
 *	Sleep locks.  These use the same data structure and algorithm
 *	as the spin locks, but the process sleeps while it is waiting
 *	for the lock.  These work on uniprocessor systems.
 */

void lock_write(l)
	register lock_t	l;
{
	register int	i;

	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return;
	}

	/*
	 *	Try to acquire the want_write bit.
	 */
	while (l->want_write) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && l->want_write)
				continue;
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && l->want_write) {
			l->waiting = TRUE;
			thread_sleep((int) l,
				simple_lock_addr(l->interlock), FALSE);
			simple_lock(&l->interlock);
		}
	}
	l->want_write = TRUE;

	/* Wait for readers (and upgrades) to finish */

	while ((l->read_count != 0) || l->want_upgrade) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && (l->read_count != 0 ||
					l->want_upgrade))
				continue;
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && (l->read_count != 0 || l->want_upgrade)) {
			l->waiting = TRUE;
			thread_sleep((int) l,
				simple_lock_addr(l->interlock), FALSE);
			simple_lock(&l->interlock);
		}
	}
	simple_unlock(&l->interlock);
}

void lock_done(l)
	register lock_t	l;
{
	simple_lock(&l->interlock);

	if (l->read_count != 0)
		l->read_count--;
	else
	if (l->recursion_depth != 0)
		l->recursion_depth--;
	else
	if (l->want_upgrade)
	 	l->want_upgrade = FALSE;
	else
	 	l->want_write = FALSE;

	if (l->waiting) {
		l->waiting = FALSE;
		thread_wakeup((int) l);
	}
	simple_unlock(&l->interlock);
}

void lock_read(l)
	register lock_t	l;
{
	register int	i;

	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->read_count++;
		simple_unlock(&l->interlock);
		return;
	}

	while (l->want_write || l->want_upgrade) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && (l->want_write || l->want_upgrade))
				continue;
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && (l->want_write || l->want_upgrade)) {
			l->waiting = TRUE;
			thread_sleep((int) l,
				simple_lock_addr(l->interlock), FALSE);
			simple_lock(&l->interlock);
		}
	}

	l->read_count++;
	simple_unlock(&l->interlock);
}

/*
 *	Routine:	lock_read_to_write
 *	Function:
 *		Improves a read-only lock to one with
 *		write permission.  If another reader has
 *		already requested an upgrade to a write lock,
 *		no lock is held upon return.
 *
 *		Returns TRUE if the upgrade *failed*.
 */
boolean_t lock_read_to_write(l)
	register lock_t	l;
{
	register int	i;

	simple_lock(&l->interlock);

	if (l->read_count == 0)	panic("lock upgrade w/o read lock");
	l->read_count--;

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return(FALSE);
	}

	if (l->want_upgrade) {
		/*
		 *	Someone else has requested upgrade.
		 *	Since we've released a read lock, wake
		 *	him up.
		 */
		if (l->waiting) {
			l->waiting = FALSE;
			thread_wakeup((int) l);
		}

		simple_unlock(&l->interlock);
		return (TRUE);
	}

	l->want_upgrade = TRUE;

	while (l->read_count != 0) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && l->read_count != 0)
				continue;
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && l->read_count != 0) {
			l->waiting = TRUE;
			thread_sleep((int) l,
				simple_lock_addr(l->interlock), FALSE);
			simple_lock(&l->interlock);
		}
	}

	simple_unlock(&l->interlock);
	return (FALSE);
}

void lock_write_to_read(l)
	register lock_t	l;
{
	simple_lock(&l->interlock);

	l->read_count++;
	if (l->recursion_depth != 0)
		l->recursion_depth--;
	else
	if (l->want_upgrade)
		l->want_upgrade = FALSE;
	else
	 	l->want_write = FALSE;

	if (l->waiting) {
		l->waiting = FALSE;
		thread_wakeup((int) l);
	}

	simple_unlock(&l->interlock);
}


/*
 *	Routine:	lock_try_write
 *	Function:
 *		Tries to get a write lock.
 *
 *		Returns FALSE if the lock is not held on return.
 */

boolean_t lock_try_write(l)
	register lock_t	l;
{

	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return(TRUE);
	}

	if (l->want_write || l->want_upgrade || l->read_count) {
		/*
		 *	Can't get lock.
		 */
		simple_unlock(&l->interlock);
		return(FALSE);
	}

	/*
	 *	Have lock.
	 */

	l->want_write = TRUE;
	simple_unlock(&l->interlock);
	return(TRUE);
}

/*
 *	Routine:	lock_try_read
 *	Function:
 *		Tries to get a read lock.
 *
 *		Returns FALSE if the lock is not held on return.
 */

boolean_t lock_try_read(l)
	register lock_t	l;
{
	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock
		 */
		l->read_count++;
		simple_unlock(&l->interlock);
		return(TRUE);
	}

	if (l->want_write || l->want_upgrade) {
		simple_unlock(&l->interlock);
		return(FALSE);
	}

	l->read_count++;
	simple_unlock(&l->interlock);
	return(TRUE);
}

/*
 *	Routine:	lock_try_read_to_write
 *	Function:
 *		Improves a read-only lock to one with
 *		write permission.  If another reader has
 *		already requested an upgrade to a write lock,
 *		the read lock is still held upon return.
 *
 *		Returns FALSE if the upgrade *failed*.
 */
boolean_t lock_try_read_to_write(l)
	register lock_t	l;
{

	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock
		 */
		if (l->read_count == 0)	panic("lock upgrade w/o read lock");
		l->read_count--;
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return(TRUE);
	}

	if (l->want_upgrade) {
		simple_unlock(&l->interlock);
		return(FALSE);
	}
	l->want_upgrade = TRUE;
	if (l->read_count == 0)	panic("lock upgrade w/o read lock");
	l->read_count--;

	while (l->read_count != 0) {
		l->waiting = TRUE;
		thread_sleep((int) l, simple_lock_addr(l->interlock), FALSE);
		simple_lock(&l->interlock);
	}

	simple_unlock(&l->interlock);
	return(TRUE);
}

/*
 *	Allow a process that has a lock for write to acquire it
 *	recursively (for read, write, or update).
 */
void lock_set_recursive(l)
	lock_t		l;
{
	simple_lock(&l->interlock);
	if (!l->want_write) {
		panic("lock_set_recursive: don't have write lock");
	}
	l->thread = (char *) current_thread();
	simple_unlock(&l->interlock);
}

/*
 *	Prevent a lock from being re-acquired.
 */
void lock_clear_recursive(l)
	lock_t		l;
{
	simple_lock(&l->interlock);
	if (((thread_t) l->thread) != current_thread()) {
		panic("lock_clear_recursive: wrong thread");
	}
	if (l->recursion_depth == 0)
		l->thread = (char *)-1;		/* XXX */
	simple_unlock(&l->interlock);
}



