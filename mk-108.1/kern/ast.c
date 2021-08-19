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
 * $Log:	ast.c,v $
 * 18-May-90  Avadis Tevanian (avie) at NeXT
 *	Updates to use runq->high instead of runq->low.
 *
 * Revision 2.7  89/11/20  11:23:13  mja
 * 	Add MACH_FIXPRI support.  Don't need to update rq->low because
 * 	it's not lazy evaluated if fixed priority threads are allowed.
 * 	[89/11/15            dlb]
 * 
 * Revision 2.6  89/10/11  14:00:47  dlb
 * 	Change sched_pri to 0-31 from 0-127.
 * 	Update rq->low hint if it affects context switch check.
 * 	Extensive rewrite to request ast's for processor actions
 * 	       and support new Mach ast mechanism (incl. HW_AST).
 * 
 * Revision 2.5  89/04/05  13:03:02  rvb
 * 	Forward declaration of csw_check() as boolean_t.
 * 	[89/03/21            rvb]
 * 
 * Revision 2.4  89/02/25  18:00:04  gm0w
 * 	Changes for cleanup.
 * 
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	Moved cpu not running check to ast_check().
 *	New preempt priority logic.
 *	Increment runrun if ast is for context switch.
 *	Give absolute priority to local run queues.
 *
 * 20-Apr-88  David Black (dlb) at Carnegie-Mellon University
 *	New signal check logic.
 *
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Flushed conditionals, reset history.
 */ 

/*
 *
 *	This file contains routines to check whether an ast is needed.
 *
 *	ast_check() - check whether ast is needed for signal, or context
 *	switch.  Usually called by clock interrupt handler.
 *
 */

#import <cpus.h>
#import <hw_ast.h>
#import <mach_fixpri.h>

#import <machine/cpu.h>
#import <machine/pcb.h>
#import <kern/ast.h>
#import <sys/param.h>
#import <sys/proc.h>
#import <kern/queue.h>
#import <kern/sched.h>
#import <sys/systm.h>
#import <kern/thread.h>
#import <sys/user.h>
#import <kern/processor.h>

#if	MACH_FIXPRI
#import <sys/policy.h>
#endif	MACH_FIXPRI

#if	!HW_AST
int	need_ast[NCPUS];
#endif	!HW_AST

ast_init()
{
#if	!HW_AST
	register int i;

	for (i=0; i<NCPUS; i++)
		need_ast[i] = 0;
#endif	!HW_AST
}

ast_check()
{
	register struct proc	*p;
	register struct uthread	*uthreadp;
	register int		mycpu = cpu_number();
	register processor_t	myprocessor;
	register thread_t	thread = current_thread();
	register run_queue_t	rq;

	/*
	 *	Check processor state for ast conditions.
	 */
	myprocessor = cpu_to_processor(mycpu);
	switch(myprocessor->state) {
	    case PROCESSOR_OFF_LINE:
	    case PROCESSOR_IDLE:
	    case PROCESSOR_DISPATCHING:
		/*
		 *	No ast.
		 */
	    	break;

#if	NCPUS > 1
	    case PROCESSOR_ASSIGN:
	    case PROCESSOR_SHUTDOWN:
	        /*
		 * 	Need ast to force action thread onto processor.
		 *
		 * XXX  Should check if action thread is already there.
		 */
		aston();
		break;
#endif	NCPUS > 1

	    case PROCESSOR_RUNNING:

		/*
		 *	Propagate thread ast to processor.  If we already
		 *	need an ast, don't look for more reasons.
		 */
		ast_propagate(thread, mycpu);
		if (ast_needed(mycpu))
			break;

		/*
		 *	Context switch check.  The csw_needed macro isn't
		 *	used here because the rq->low hint may be wrong,
		 *	and fixing it here avoids an extra ast.
		 *	First check the easy cases.
		 */
		if (thread->state & TH_SUSP || myprocessor->runq.count > 0) {
			aston();
			break;
		}

		/*
		 *	Update lazy evaluated runq->low if only timesharing.
		 */
#if	MACH_FIXPRI
		if (myprocessor->processor_set->policies & POLICY_FIXEDPRI) {
		    if (csw_needed(thread,myprocessor)) {
			aston();
			break;
		    }
		    else {
			/*
			 *	For fixed priority threads, set first_quantum
			 *	so entire new quantum is used.
			 */
			if (thread->policy == POLICY_FIXEDPRI)
			    myprocessor->first_quantum = TRUE;
		    }
		}
		else {
#endif	MACH_FIXPRI			
		rq = &(myprocessor->processor_set->runq);
		if (!(myprocessor->first_quantum) && (rq->count > 0)) {
		    register queue_t 	q;
		    /*
		     *	This is not the first quantum, and there may
		     *	be something in the processor_set runq.
		     *	Check whether low hint is accurate.
		     */
#if	NeXT
		    q = rq->runq + rq->high;
#else	NeXT
		    q = rq->runq + rq->low;
#endif	NeXT
		    if (queue_empty(q)) {
			register int i,s;

			/*
			 *	Need to recheck and possibly update hint.
			 */
			s = splsched();
			simple_lock(&rq->lock);
#if	NeXT
			q = rq->runq + rq->high;
			if (rq->count > 0) {
			    for (i = rq->high; i >= 0; i--) {
				if(!(queue_empty(q)))
				    break;
				q--;
			    }
			    rq->high = i;
			}
#else	NeXT
			q = rq->runq + rq->low;
			if (rq->count > 0) {
			    for (i = rq->low; i < NRQS; i++) {
				if(!(queue_empty(q)))
				    break;
				q++;
			    }
			    rq->low = i;
			}
#endif	NeXT
			simple_unlock(&rq->lock);
			(void) splx(s);
		    }

#if	NeXT
		    if (rq->high >= thread->sched_pri) {
#else	NeXT
		    if (rq->low <= thread->sched_pri) {
#endif	NeXT
			aston();
			break;
		    }
		}
#if	MACH_FIXPRI
		}
#endif	MACH_FIXPRI

		/*
		 *	XXX Else check for signals.
		 */
		p = u.u_procp;
		uthreadp = current_thread()->u_address.uthread;
		if (p->p_cursig || SHOULDissig(p,uthreadp))
			aston();
		break;

	    default:
	        panic("ast_check: Bad processor state");
	}
}

