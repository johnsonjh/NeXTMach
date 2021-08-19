/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 * 26-Sep-90  Gregg Kellogg (gk) at NeXT
 *	Added fix from CMU allowing signals associated with masked signals
 *	to be passed properly to the debugger.
 *
 * 26-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Removed dir.h
 *
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	Make sure p_stat is SSTOP in ptrace().
 *
 * 13-Mar-88  David Golub (dbg) at Carnegie-Mellon University
 *	Use vm_map_copy instead of playing with physical pages.
 *
 *  3-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 13-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	run_state --> user_stop_count.
 *
 * 24-Jul-87  David Black (dlb) at Carnegie-Mellon University
 *	Set modified bit on any pages modified by copy_to_phys.
 *
 * 13-Jul-87  David Black (dlb) at Carnegie-Mellon University
 *	If delivering a thread signal, set thread's u.u_cursig.
 *	Optimize and clean up register references.
 *
 *  2-Jul-87  David Black (dlb) at Carnegie-Mellon University
 *	Derived from sys_process.c via major rewrite to eliminate
 *	ipc structure and procxmt.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)sys_process.c	7.1 (Berkeley) 6/5/86
 */

#import <machine/reg.h>
#import <machine/psl.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/ptrace.h>

#import <kern/task.h>
#import <kern/thread.h>
#import <kern/sched_prim.h>

#import <vm/vm_map.h>
#import <vm/vm_param.h>
#import <vm/vm_prot.h>
#import <vm/vm_kern.h>

/*
 * sys-trace system call.
 */
ptrace()
{
	register struct proc *p;
	register struct a {
		int	req;
		int	pid;
		int	*addr;
		int	data;
	} *uap;

	vm_map_t	victim_map;
	vm_offset_t	start_addr, end_addr,
			kern_addr, offset;
	vm_size_t	size;
	boolean_t	change_protection;
	task_t		task;
	thread_t	thread;
	int		*locr0;

	uap = (struct a *)u.u_ap;

	/*
	 *	Intercept and deal with "please trace me" request.
	 */
	if (uap->req <= 0) {
		u.u_procp->p_flag |= STRC;
		/* Non-attached case, our tracer is our parent. */
	 	u.u_procp->p_tptr = u.u_procp->p_pptr;
		u.u_procp->p_pptr->p_aptr = u.u_procp;
		return;
	}

	/*
	 *	Locate victim, and make sure it is traceable.
	 */
	p = pfind(uap->pid);
	if (p == 0) {
	esrch:
		u.u_error = ESRCH;
		return;
		
	}
	task = p->task;
	if (uap->req == PT_ATTACH) {
		register struct proc *myproc = u.u_procp;

		/* First, check for permissions. */
		if (((myproc->p_uid) && (p->p_uid != myproc->p_uid)) || 
		    (p->p_flag & STRC) || myproc->p_aptr)
		  goto esrch;
		/* We are now the tracing process. */
		p->p_flag |= STRC;
		p->p_tptr = u.u_procp;
		myproc->p_aptr = p;
		/* Halt it dead in its tracks. */
		psignal(p, SIGSTOP);
		return;
	}
	if (task->user_stop_count == 0 ||
	    p->p_stat != SSTOP || p->p_tptr != u.u_procp ||
	    !(p->p_flag & STRC)) {
		goto esrch;
	      }
	/*
	 *	Mach version of ptrace executes request directly here,
	 *	thus simplifying the interaction of ptrace and signals.
	 */
	switch (uap->req) {

	case PT_DETACH:
	  { register struct proc *p, *q;

	    p = u.u_procp;
	    q = p->p_aptr;
	    if (!q) {
	    	u.u_error = EINVAL;
		break;
	    }
	    q->p_flag &= ~STRC;
	    q->p_tptr = 0;
	    p->p_aptr = 0;
	    goto resume;
	  }

	case PT_KILL:
		/*
		 *	Tell child process to kill itself after it
		 *	is resumed by adding NSIG to p_cursig. [see issig]
		 */
		p->p_cursig += NSIG;
		goto resume;

	case PT_STEP:			/* single step the child */
	case PT_CONTINUE:		/* continue the child */
		thread = (thread_t) queue_first(&task->thread_list);
		locr0 = thread->u_address.uthread->uu_ar0;
		if ((int)uap->addr != 1)
			locr0[PC] = (int)uap->addr;
		if ((unsigned)uap->data > NSIG)
			goto error;

		if (sigmask(p->p_cursig) & threadmask)
		   thread->u_address.uthread->uu_cursig = 0;
		p->p_cursig = uap->data;	/* see issig */
		if (sigmask(uap->data) & threadmask)
		    thread->u_address.uthread->uu_cursig = uap->data;

		if (uap->req == PT_STEP) 
			locr0[PS] |= PSL_T;
	resume:
		p->p_stat = SRUN;
		if (p->thread && p->p_cursig)
			clear_wait(p->thread, THREAD_INTERRUPTED, TRUE);
		task_resume(task);
		break;
		
	default:
	error:
		u.u_error = EIO;
	}
}

