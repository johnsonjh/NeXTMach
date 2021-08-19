/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 13-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Removed dependencies on proc table.
 *
 * 22-Feb-88  John Seamons (jks) at NeXT
 *	Removed include of "sys/inode.h".
 *
 *  1-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	Created by cutting down psignal to only deal with exceptions.
 *
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/kernel.h>
#import <kern/sched.h>
#import <kern/task.h>
#import <kern/thread.h>

/*
 * Send the specified exception signal to the specified thread.
 *
 * NOTE: unlike its full-blown counterpart, this is completely parallel!
 */
thread_psignal(sig_thread, sig)
	register thread_t	sig_thread;
	register int sig;
{
	register struct proc *p;
	register task_t		sig_task;
	int mask;

	if ((unsigned)sig > NSIG)
		return;

	mask = sigmask(sig);
	if ( (mask & threadmask) == 0) {
		printf("signal = %d\n");
		panic("thread_psignal: signal is not an exception!");
	}

	sig_task = sig_thread->task;
#if	NeXT
	p = sig_task->proc;
#else	NeXT
	p = &proc[sig_task->proc_index];
#endif	NeXT

	/*
	 *	Forget ignored signals UNLESS process is being traced. (XXX)
	 */
	if ((p->p_sigignore & mask) && (p->p_flag & STRC) == 0 )
		return;

	/*
	 *	This is an exception signal - deliver directly to thread.
	 */
	sig_lock_simple(p);
	sig_thread->u_address.uthread->uu_sig |= mask;
	sig_unlock(p);
}


