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
 * $Log:	lock.h,v $
 *  5-Jul-90  Morris Meyer (mmeyer) at NeXT
 *	Changed the lock structure to use bitfields instead of boolean_t's.
 *	Saves 24k of wired data on a normal system.
 *
 * 31-May-90  Gregg Kellogg (gk) at NeXT
 *	MACH_SLOCKS always turned on for loadable servers.
 *
 * 14-May-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: always declare space for simple_locks
 *
 * 19-Apr-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: declared simple_lock_alloc(), simple_lock_free, lock_alloc()
 *	and lock_free() needed for loadable servers.
 *
 * 15-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Made lock structure use bitfields for all machines.  Vax
 *	is still different but could be made the same.
 *
 * Revision 2.7  89/03/09  20:13:39  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  18:05:30  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.5  89/02/07  01:02:39  mwyoung
 * Relocated from sys/lock.h
 * 
 * Revision 2.4  89/01/15  16:34:11  rpd
 * 	Added decl_simple_lock_data, simple_lock_addr macros.
 * 	Rearranged complex lock structure to use decl_simple_lock_data
 * 	for the interlock field and put it last (except on ns32000).
 * 	[89/01/15  15:16:47  rpd]
 * 
 * Revision 2.3  88/08/24  02:33:07  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:15:53  mwyoung]
 * 
 * Revision 2.2  88/07/20  16:49:35  rpd
 * Allow for sanity-checking of simple locking on uniprocessors,
 * controlled by new option MACH_LDEBUG.  Define composite
 * MACH_SLOCKS, which is true iff simple locking calls expand
 * to code.  It can be used to #if-out declarations, etc, that
 * are only used when simple locking calls are real.
 * 
 *  3-Nov-87  David Black (dlb) at Carnegie-Mellon University
 *	Use optimized lock structure for multimax also.
 *
 * 27-Oct-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Use optimized lock "structure" for balance now that locks are
 *	done inline.
 *
 * 26-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Invert logic of no_sleep to can_sleep.
 *
 * 29-Dec-86  David Golub (dbg) at Carnegie-Mellon University
 *	Removed BUSYP, BUSYV, adawi, mpinc, mpdec.  Defined the
 *	interlock field of the lock structure to be a simple-lock.
 *
 *  9-Nov-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added "unsigned" to fields in vax case, for lint.
 *
 * 21-Oct-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added fields for sleep/recursive locks.
 *
 *  7-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	Merged Multimax changes.
 *
 * 26-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Removed reference to "caddr_t" from BUSYV/P.  I really
 *	wish we could get rid of these things entirely.
 *
 * 24-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Changed to directly import boolean declaration.
 *
 *  1-Aug-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added simple_lock_try, sleep locks, recursive locking.
 *
 * 11-Jun-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Removed ';' from definitions of locking macros (defined only
 *	when NCPU < 2). so as to make things compile.
 *
 * 28-Feb-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Defined adawi to be add when not on a vax.
 *
 * 07-Nov-85  Michael Wayne Young (mwyoung) at Carnegie-Mellon University
 *	Overhauled from previous version.
 */
/*
 *	File:	kern/lock.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Locking primitives definitions
 *
 */

#ifndef	_KERN_LOCK_H_
#define _KERN_LOCK_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cpus.h>
#import <mach_ldebug.h>
#import <mach_ipc_tcache.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/boolean.h>

#define MACH_SLOCKS	((NCPUS > 1) || MACH_LDEBUG)
#if	!defined(MACH_SLOCKS) && defined(KERNEL) && defined(KERNEL_FEATURES)
#define	MACH_SLOCKS	1
#endif	!defined(MACH_SLOCKS) && defined(KERNEL) && defined(KERNEL_FEATURES)
/*
 *	A simple spin lock.
 */

struct slock {
	int		lock_data;	/* in general 1 bit is sufficient */
};

typedef struct slock	simple_lock_data_t;
typedef struct slock	*simple_lock_t;

#if	NeXT
extern simple_lock_t	simple_lock_alloc();
extern void		simple_lock_free();
#endif	NeXT
#if	MACH_SLOCKS
extern void		simple_lock_init();
extern void		simple_lock();
extern void		simple_unlock();
extern boolean_t	simple_lock_try();

#define decl_simple_lock_data(class,name)	class simple_lock_data_t name;
#define simple_lock_addr(lock)		(&(lock))
#else	MACH_SLOCKS
/*
 *	No multiprocessor locking is necessary.
 */
#define simple_lock_init(l)
#define simple_lock(l)
#define simple_unlock(l)
#define simple_lock_try(l)	(1)	/* always succeeds */

#if	NeXT
#define decl_simple_lock_data(class,name)	class simple_lock_data_t name;
#define simple_lock_addr(lock)		(&(lock))
#else	NeXT
#define decl_simple_lock_data(class,name)
#define simple_lock_addr(lock)		((simple_lock_t)0)
#endif	NeXT
#endif	MACH_SLOCKS

/*
 *	The general lock structure.  Provides for multiple readers,
 *	upgrading from read to write, and sleeping until the lock
 *	can be gained.
 *
 *	On some (many) architectures, assembly language code in the inline
 *	program fiddles the lock structures.  It must be changed in concert
 *	with the structure layout.
 */

struct lock {
#ifdef	vax
	/*
	 *	Efficient VAX implementation -- see field description below.
	 */
	unsigned int	read_count:16,
			want_upgrade:1,
			want_write:1,
			waiting:1,
			can_sleep:1,
			:0;
#else	vax
#ifdef	NeXT
	/*	Only the "interlock" field is used for hardware exclusion;
	 *	other fields are modified with normal instructions after
	 *	acquiring the interlock bit.
	 */
	simple_lock_data_t	interlock;
	unsigned int	read_count:16,
			want_upgrade:1,
			want_write:1,
			waiting:1,
			can_sleep:1,
			recursion_depth:12;
#else	NeXT
#ifdef	ns32000
	/*
	 *	Efficient ns32000 implementation --
	 *	see field description below.
	 *
	 *	The ns32000 assembly hasn't yet been updated
	 *	for decl_simple_lock_data.
	 */
	simple_lock_data_t	interlock;
	unsigned int	read_count:16,
			want_upgrade:1,
			want_write:1,
			waiting:1,
			can_sleep:1,
			:0;

#else	ns32000
	/*	Only the "interlock" field is used for hardware exclusion;
	 *	other fields are modified with normal instructions after
	 *	acquiring the interlock bit.
	 */
	boolean_t	want_write;	/* Writer is waiting, or locked for write */
	boolean_t	want_upgrade;	/* Read-to-write upgrade waiting */
	boolean_t	waiting;	/* Someone is sleeping on lock */
	boolean_t	can_sleep;	/* Can attempts to lock go to sleep */
	int		read_count;	/* Number of accepted readers */
#endif	ns32000
#endif	NeXT
#endif	vax
	char		*thread;
		/* Thread that has lock, if recursive locking allowed */
		/* (Not thread_t to avoid recursive definitions.) */

#if	NeXT
#else	NeXT
	int		recursion_depth;/* Depth of recursion */
#if	!defined(ns32000)
	/*	Put this field last in the structures, so that field
	 *	offsets are constant regardless of whether this is present.
	 *	This makes any assembly language code simpler.
	 */
	decl_simple_lock_data(,interlock)
#endif	!defined(ns32000)
#endif	NeXT
};

typedef struct lock	lock_data_t;
typedef struct lock	*lock_t;

/* Sleep locks must work even if no multiprocessing */

#if	NeXT
extern lock_t		lock_alloc();
extern void		lock_free();
#endif	NeXT
extern void		lock_init();
extern void		lock_sleepable();
extern void		lock_write();
extern void		lock_read();
extern void		lock_done();
extern boolean_t	lock_read_to_write();
extern void		lock_write_to_read();
extern boolean_t	lock_try_write();
extern boolean_t	lock_try_read();
extern boolean_t	lock_try_read_to_write();

#define lock_read_done(l)	lock_done(l)
#define lock_write_done(l)	lock_done(l)

extern void		lock_set_recursive();
extern void		lock_clear_recursive();

#endif	_KERN_LOCK_H_




