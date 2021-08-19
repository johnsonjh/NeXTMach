/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	processor.h,v $
 * Revision 2.6  90/07/20  08:49:40  mrt
 * 	processor_set_array_t --> processor_set_name_array_t.
 * 	[90/07/13            dlb]
 *
 * 17-May-90  Gregg Kellogg (gk) at NeXT
 *	Added defninition of processor_set_name_t for non-KERNEL case of
 *	mach_types.h.
 *
 * Revision 2.5  89/11/20  11:23:50  mja
 * 	Put policies field under MACH_FIXPRI conditional.
 * 	[89/11/10            dlb]
 * 
 * Revision 2.4  89/10/15  02:05:13  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.3  89/10/12  21:34:28  dlb
 * 	Get ast_check_t from machine/ast_types.h instead of machine/ast.h
 * 	[89/10/12            dlb]
 * 
 * Revision 2.2  89/10/11  14:20:44  dlb
 * 	Add remote ast check support for multiprocessors.
 * 	Add quantum_adj_lock to pset structure.  Enclose some fields
 * 		in NCPUS > 1 conditionals.
 * 	Add max_priority, policies fields to processor_set structure.
 * 	Add load factor/average fields to processor set structure.
 * 
 * Revision 2.1.1.5  89/08/02  23:01:16  dlb
 * 	Merge to X96
 * 
 * Revision 2.1.1.4  89/07/25  18:50:33  dlb
 * 	Add quantum_adj_lock to pset structure.  Enclose some fields
 * 	in NCPUS > 1 conditionals.
 * 	[89/06/15            dlb]
 * 
 * 	Add all_psets_count declaration.
 * 	[89/06/09            dlb]
 * 
 * 	Add processor_set_array_t.
 * 	[89/06/08            dlb]
 * 
 * 	Add max_priority, policies fields to processor_set structure.
 * 	[89/05/12            dlb]
 * 
 * Revision 2.1.1.3  89/02/13  22:56:03  dlb
 * 	Add load factor/average fields to processor set structure.
 * 	[89/02/09            dlb]
 * 
 * 27-Sep-88  David Black (dlb) at Carnegie-Mellon University
 *	Created.
 *
 */

/*
 *	processor.h:	Processor and processor-set definitions.
 */

#ifndef	_KERN_PROCESSOR_H_
#define	_KERN_PROCESSOR_H_

/*
 *	Data structures for managing processors and sets of processors.
 */

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cpus.h>
#import <mach_fixpri.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/boolean.h>
#import <kern/lock.h>
#import <sys/port.h>
#import <kern/queue.h>
#import <kern/sched.h>

#if	NCPUS > 1
#import <machine/ast_types.h>
#endif	NCPUS > 1

struct processor_set {
	struct	run_queue	runq;		/* runq for this set */
	queue_head_t		idle_queue;	/* idle processors */
	int			idle_count;	/* how many ? */
	simple_lock_data_t	idle_lock;	/* lock for above */
	queue_head_t		processors;	/* all processors here */
	int			processor_count;	/* how many ? */
	boolean_t		empty;		/* true if no processors */
	queue_head_t		tasks;		/* tasks assigned */
	int			task_count;	/* how many */
	queue_head_t		threads;	/* threads in this set */
	int			thread_count;	/* how many */
	int			ref_count;	/* structure ref count */
	queue_chain_t		all_psets;	/* link for all_psets */
	boolean_t		active;		/* is pset in use */
	simple_lock_data_t	lock;		/* lock for everything else */
	port_t			pset_self;	/* port for operations */
	port_t			pset_name_self;	/* port for information */
	int			max_priority;	/* maximum priority */
#if	MACH_FIXPRI
	int			policies;	/* bit vector for policies */
#endif	MACH_FIXPRI
	int			set_quantum;	/* current default quantum */
#if	NCPUS > 1
	int			quantum_adj_index; /* runtime quantum adj. */
	simple_lock_data_t	quantum_adj_lock;  /* lock for above */
	int			machine_quantum[NCPUS+1]; /* ditto */
#endif	NCPUS > 1
	long			mach_factor;	/* mach_factor */
	long			load_average;	/* load_average */
	long			sched_load;	/* load avg for scheduler */
};

typedef	struct processor_set *processor_set_t;
typedef port_t	processor_set_name_t;

#define PROCESSOR_SET_NULL	(processor_set_t)0

extern struct processor_set	default_pset;

struct processor {
	struct run_queue runq;		/* local runq for this processor */
		/* XXX want to do this round robin eventually */
	queue_chain_t	processor_queue; /* idle/assign/shutdown queue link */
	int		state;		/* See below */
	struct thread	*next_thread;	/* next thread to run if dispatched */
	struct thread	*idle_thread;	/* this processor's idle thread. */
	int		quantum;	/* quantum for current thread */
	boolean_t	first_quantum;	/* first quantum in succession */
	int		last_quantum;	/* last quantum assigned */

	processor_set_t	processor_set;	/* processor set I belong to */
	processor_set_t processor_set_next;	/* set I will belong to */
	queue_chain_t	processors;	/* all processors in set */
	simple_lock_data_t	lock;
	port_t		processor_self;	/* port for operations */
	int		slot_num;	/* machine-indep slot number */
#if	NCPUS > 1
	ast_check_t	ast_check_data;	/* for remote ast_check invocation */
#endif	NCPUS > 1
	/* punt id data temporarily */
};

typedef struct processor *processor_t;

#define PROCESSOR_NULL	(processor_t)0

extern struct processor	processor_array[NCPUS];

/*
 *	Chain of all processor sets.
 */
extern queue_head_t		all_psets;
extern int			all_psets_count;
decl_simple_lock_data(extern, all_psets_lock);

/*
 *	XXX need a pointer to the master processor structure
 */

extern processor_t	master_processor;

/*
 *	NOTE: The processor->processor_set link is needed in one of the
 *	scheduler's critical paths.  [Figure out where to look for another
 *	thread to run on this processor.]  It is accessed without locking.
 *	The following access protocol controls this field.
 *
 *	Read from own processor - just read.
 *	Read from another processor - lock processor structure during read.
 *	Write from own processor - lock processor structure during write.
 *	Write from another processor - NOT PERMITTED.
 *
 */

/*
 *	Processor state locking:
 *
 *	Values for the processor state are defined below.  If the processor
 *	is off-line or being shutdown, then it is only necessary to lock
 *	the processor to change its state.  Otherwise it is only necessary
 *	to lock its processor set's idle_lock.  Scheduler code will
 *	typically lock only the idle_lock, but processor manipulation code
 *	will often lock both.
 */

#define PROCESSOR_OFF_LINE	0	/* Not in system */
#define	PROCESSOR_RUNNING	1	/* Running normally */
#define	PROCESSOR_IDLE		2	/* idle */
#define PROCESSOR_DISPATCHING	3	/* dispatching (idle -> running) */
#define	PROCESSOR_ASSIGN	4	/* Assignment is changing */
#define PROCESSOR_SHUTDOWN	5	/* Being shutdown */

/*
 *	Use processor ptr array to find current processor's data structure.
 *	This replaces a multiplication (index into processor_array) with
 *	an array lookup and a memory reference.  It also allows us to save
 *	space if processor numbering gets too sparse.
 */

extern processor_t	processor_ptr[NCPUS];

#define cpu_to_processor(i)	(processor_ptr[i])

#define current_processor()	(processor_ptr[cpu_number()])
#define current_processor_set()	(current_processor()->processor_set)

/* Compatibility -- will go away */

#define cpu_state(slot_num)	(processor_ptr[slot_num]->state)
#define cpu_idle(slot_num)	(processor_state(slot_num) == PROCESSOR_IDLE)

/* Useful lock macros */

#define	pset_lock(pset)		simple_lock(&(pset)->lock)
#define pset_unlock(pset)	simple_unlock(&(pset)->lock)

#define processor_lock(pr)	simple_lock(&(pr)->lock)
#define processor_unlock(pr)	simple_unlock(&(pr)->lock)

typedef port_t	*processor_array_t;
typedef port_t	*processor_set_name_array_t;

#endif	_KERN_PROCESSOR_H_
