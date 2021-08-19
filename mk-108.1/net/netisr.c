/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/* HISTORY
 * 18-May-90  Avadis Tevanian (avie) at NeXT
 *	Changed to use sensible priorities (higher numbers -> higher pri).
 *
 *  1-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	Goofed... netisr thread must run at splnet, because the routines
 *	it calls expect to be called from the softnet interrupt (at
 *	splnet).
 *
 * 19-Nov-87  David Golub (dbg) at Carnegie-Mellon University
 *	Created.
 *
 */

/*
 *	netisr.c
 *
 *	Kernel thread for network code.
 */

#import <imp.h>

#import <machine/spl.h>
#import <net/netisr.h>

#import <kern/thread.h>
#import <kern/sched_prim.h>

void netisr_thread()
{
	register thread_t thread = current_thread();

	/*
	 *	Make this the highest priority thread,
	 *	and bind it to the master cpu.
	 */
#if	NeXT
	thread->priority = 31;
	thread->sched_pri = 31;
#else	NeXT
	thread->priority = 0;
	thread->sched_pri = 0;
#endif	NeXT
	thread_bind(thread, master_cpu);
	thread_block();	/* resume on master */

	/*
	 *	All routines this thread calls expect to be called
	 *	at splnet.
	 */
	(void) splnet();

	while (TRUE) {
		assert_wait((int) &soft_net_wakeup, FALSE);
		thread_block();

		while (netisr != 0) {
#if	NIMP > 0
			if (netisr & (1<<NETISR_IMP)){
				netisr &= ~(1<<NETISR_IMP);
				impintr();
			}
#endif	NIMP > 0
#ifdef	INET
			if (netisr & (1<<NETISR_IP)){
				netisr &= ~(1<<NETISR_IP);
				ipintr();
			}
#endif	INET
#ifdef	NS
			if (netisr & (1<<NETISR_NS)){
				netisr &= ~(1<<NETISR_NS);
				nsintr();
			}
#endif	NS
			if (netisr & (1<<NETISR_RAW)){
				netisr &= ~(1<<NETISR_RAW);
				rawintr();
			}
		}

	}
}

