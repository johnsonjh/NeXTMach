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
 * $Log:	parallel.c,v $
 * Revision 2.4  89/12/22  15:52:48  rpd
 * 	MACH_HOST support: when releasing master, context switch away
 * 	immediately if thread is not assigned to default processor set.
 * 	[89/11/16            dlb]
 * 
 * Revision 2.3  89/10/11  14:19:20  dlb
 * 	Processor logic - explicitly record bound processor in thread
 * 	instead of changing whichq pointer.
 * 	[88/09/30            dlb]
 * 
 * Revision 2.2  89/02/25  18:07:24  gm0w
 * 	Changes for cleanup.
 * 
 * 15-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Use thread_bind (inline version) to bind thread to master cpu
 *	while holding unix-lock.
 *
 *  9-Oct-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Define unix_reset for longjmp/setjmp reset.
 *
 * 25-Sep-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Clean out some debugging code.
 *
 * 21-Sep-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Created.
 *
 */


#import <cpus.h>
#import <mach_host.h>

#if	NCPUS > 1

#import <kern/processor.h>
#import <kern/thread.h>
#import <kern/sched_prim.h>
#import <kern/parallel.h>

void unix_master()
{
	register thread_t t = current_thread();
	
	if (! (++( t->unix_lock )))	{

		/* thread_bind(t, master_processor); */
		t->bound_processor = master_processor;

		if (cpu_number() != master_cpu) {
			t->interruptible = FALSE;
			thread_block();
		}
	}
}

void unix_release()
{
	register thread_t t = current_thread();

	t->unix_lock--;
	if (t->unix_lock < 0) {
		/* thread_bind(t, PROCESSOR_NULL); */
		t->bound_processor = PROCESSOR_NULL;
#if	MACH_HOST
		if (t->processor_set != &default_pset)
			thread_block();
#endif	MACH_HOST
	}
}

void unix_reset()
{
	register thread_t	t = current_thread();

	if (t->unix_lock != -1)
		t->unix_lock = 0;
}

#endif	NCPUS > 1
