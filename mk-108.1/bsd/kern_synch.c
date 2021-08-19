/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 22-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Make sure proc structure isn't de-referenced if it isn't there.
 *
 * 19-Mar-90  Gregg Kellogg (gk) at NeXT
 *	NeXT doesn't use schedcpu.
 *
 * Revision 2.16  89/10/11  13:46:15  dlb
 * 	Use new priorities and priority interface to scheduler.
 * 	Remove should_exit[].
 * 	Add ast_context call to slave_start.
 * 	Use task->kernel_vm_space to figure out whether pmap needs
 * 	       to be activated.
 * 
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Removed dir.h
 *
 * 05-Jan-89  Avadis Tevanian (avie) at NeXT
 *	Removed several proc table references (unused).
 *
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	MACH: Cleaned up conditionals in sleep().
 *	      Moved autonice code here from softclock().
 *	      No more SSLEEP state for P_STAT.
 *
 * 18-Feb-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Check for use of kernel_pmap to determine whether to (de,)activate.
 *
 * 26-Jan-88  David Black (dlb) at Carnegie-Mellon University
 *	Don't activate pmaps for kernel_only tasks.
 *
 * 21-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Reduced conditionals, purged history.
 *
 * 19-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Change inodes to vnodes.
 *	SUN_RPC: If PCATCH is set, sleep will return 1 rather than longjmp.
 *	SUN_NFS: Create wakeup_one that will only wakeup one process sleeping
 *		 on a channel.
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_synch.c	7.1 (Berkeley) 6/5/86
 */

#import <cputypes.h>
#import <cpus.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/file.h>
#import <sys/vnode.h>
#import <sys/kernel.h>
#import <sys/buf.h>
#import <sys/table.h>

#import <machine/spl.h>

#import <kern/ast.h>
#import <sys/callout.h>
#import <kern/queue.h>
#import <kern/lock.h>
#import <kern/thread.h>
#import <kern/sched.h>
#import <kern/sched_prim.h>
#import <sys/machine.h>
#import <kern/parallel.h>
#import <kern/processor.h>

#import <machine/cpu.h>
#import <vm/pmap.h>
#import <vm/vm_kern.h>

#ifdef vax
#import <kern/kern_mon.h>
#endif

#import <kern/task.h>
#import <sys/time_value.h>

#if	!NeXT
/*
 * Recompute process priorities, once a second
 */
schedcpu()
{
	register thread_t th;

	wakeup((caddr_t)&lbolt);

	/*
	 *	Autonice code moved here from kern_clock.c
	 */
	th = current_thread();
	if (!(th->task->kernel_vm_space)) {
	    register struct proc *p;

	    p = u.u_procp;
	    if (p->p_uid && p->p_nice == NZERO) {
		time_value_t	user_time;

		timer_read(&(th->user_timer), &user_time);
		if (user_time.seconds > 10 * 60) {
		    p->p_nice = NZERO+4;
		    thread_priority(th, BASEPRI_USER + p->p_nice/2, TRUE);
		}
	    }
	}
	timeout(schedcpu, (caddr_t)0, hz);
}
#endif	!NeXT

/*
 * Give up the processor till a wakeup occurs
 * on chan, at which time the process
 * enters the scheduling queue at priority pri.
 * The most important effect of pri is that when
 * pri<=PZERO a signal cannot disturb the sleep;
 * if pri>PZERO signals will be processed.
 * If pri&PCATCH is set, signals will cause sleep
 * to return 1, rather than longjmp.
 * Callers of this routine must be prepared for
 * premature return, and check that the reason for
 * sleeping has gone away.
 */

int sleep(chan, pri)
	caddr_t chan;
	int pri;
{
	register struct proc *rp;
	register s;

	rp = u.u_procp;
	s = splhigh();
#if	NeXT
	/* allow to continue after panics so we can try to shut down. */
#else	NeXT
	if (panicstr) {
		/*
		 * After a panic, just give interrupts a chance,
		 * then just return; don't run any other procs 
		 * or panic below, in case this is the idle process
		 * and already asleep.
		 * The splnet should be spl0 if the network was being used
		 * by the filesystem, but for now avoid network interrupts
		 * that might cause another panic.
		 */
		(void) splnet();
		splx(s);
		return(0);
	}
#endif	NeXT
	if (rp)
		rp->p_pri = pri & PMASK;
	assert_wait((int) chan, pri > PZERO);
	if (pri > PZERO) {
		/*
		 * If wakeup occurs while in issig, thread_block()
		 * below is a no-op.  If ISSIG finds a signal, clear
		 * sleep condition before going to process it.
		 */
		if (rp && ISSIG(rp)) {
			clear_wait(current_thread(), THREAD_INTERRUPTED,
					TRUE);
			(void) spl0();
			goto psig;
		}
		(void) spl0();
		u.u_ru.ru_nvcsw++;
		if (cpu_number() != master_cpu) {
			printf("unix sleep: on slave?");
		}
		thread_block();
		if (rp && ISSIG(rp))
			goto psig;
	} else {
		(void) spl0();
		u.u_ru.ru_nvcsw++;
		if (cpu_number() != master_cpu) {
			printf("unix sleep: on slave?");
		}
		thread_block();
	}
	splx(s);
	return(0);

	/*
	 * If priority was low (>PZERO) and
	 * there has been a signal, execute non-local goto through
	 * u.u_qsave, aborting the system call in progress (see trap.c),
	 * unless PCATCH is set, in which case we just return 1 so our
	 * caller can release resources and unwind the system call,
	 * or finishing a tsleep (see below).
	 */
psig:
	if (pri & PCATCH)
		return(1);
	longjmp(&u.u_qsave);
	/*NOTREACHED*/
}

/* 
 *  rpsleep - perform a resource pause sleep
 *
 *  rsleep = function to perform resource specific sleep
 *  arg1   = first function parameter
 *  arg2   = second function parameter
 *  mesg1  = first component of user pause message
 *  mesg2  = second component of user pause message
 *
 *  Display the appropriate pause message on the user's controlling terminal.
 *  Save the current non-local goto information and establish a new return
 *  environment to transfer here.  Invoke the supplied function to sleep
 *  (possibly interruptably) until the resource becomes available.  When the
 *  sleep finishes (either normally or abnormally via a non-local goto caused
 *  by a signal), restore the old return environment and display a resume
 *  message on the terminal.  The notify flag bit is set when the pause message
 *  is first printed.  If it is cleared on return from the function, the
 *  continue message is printed here.  If not, this bit will remain set for the
 *  duration of the polling process and the rpcont() routine will be called
 *  directly from the poller when the resource pause condition is no longer
 *  pending.
 *
 *  Return: true if the resource has now become available, or false if the wait
 *  was interrupted by a signal.
 */

boolean_t
rpsleep(rsleep, arg1, arg2, mesg1, mesg2)
int (*rsleep)();
int arg1;
int arg2;
char *mesg1;
char *mesg2;
{
    label_t lsave;
    boolean_t ret = TRUE;

    if ((u.u_rpswhich&URPW_NOTIFY) == 0)
    {
        u.u_rpswhich |= URPW_NOTIFY;
	uprintf("[%s: %s%s, pausing ...]\r\n", u.u_comm, mesg1, mesg2);
    }
    
    bcopy((caddr_t)&u.u_qsave, (caddr_t)&lsave, sizeof(lsave));
    if (setjmp(&u.u_qsave) == 0)
	(*rsleep)(arg1, arg2);
    else
	ret = FALSE;
    bcopy((caddr_t)&lsave, (caddr_t)&u.u_qsave, sizeof(lsave));

    if ((u.u_rpswhich&URPW_NOTIFY) == 0)
	rpcont();
    return(ret);
}

/* 
 *  rpcont - continue from resource pause sleep
 *
 *  Clear the notify flag and print the continuation message on the controlling
 *  terminal.  When this routine is called, the resource pause condition is no
 *  longer pending and we can afford to clear all bits since only the notify
 *  bit should be set to begin with.
 */

rpcont()
{
    u.u_rpswhich = 0;
    uprintf("[%s: ... continuing]\r\n", u.u_comm);
}

/* 
 * Sleep on chan at pri.
 * Return in no more than the indicated number of seconds.
 * (If seconds==0, no timeout implied)
 * Return	TS_OK if chan was awakened normally
 *		TS_TIME if timeout occurred
 *		TS_SIG if asynchronous signal occurred
 */

tsleep(chan, pri, seconds)
	caddr_t	chan;
{
 	struct timeval when;
	int	s;
	register struct proc *p = u.u_procp;

	s = splhigh();
	assert_wait((int)chan, pri > PZERO);
	if (seconds) {
		when = time;
		when.tv_sec += seconds;
		thread_set_timeout(hzto(&when));
	}
	if (p && pri > PZERO) {
		if (ISSIG(p)) {
			clear_wait(current_thread(), THREAD_INTERRUPTED, TRUE);
			splx(s);
			return (TS_SIG);
		}
		thread_block();
		if (ISSIG(p)) {
			splx(s);
			return (TS_SIG);
		}
	}
	else {
		thread_block();
	}
	if (current_thread()->wait_result == THREAD_TIMED_OUT) {
		splx(s);
		return(TS_TIME);
	}
	splx(s);
	return(TS_OK);
}


/*
 * Wake up all processes sleeping on chan.
 */
wakeup(chan)
	register caddr_t chan;
{
	int s;

	s = splhigh();
	thread_wakeup((int) chan);
	splx(s);
}

/*
 * Wake up the first process sleeping on chan.
 *
 * Be very sure that the first process is really
 * the right one to wakeup.
 */
wakeup_one(chan)
	register caddr_t chan;
{
	int s;

	s = splhigh();
	thread_wakeup_one((int) chan);
	splx(s);
}

/*
 * Initialize the (doubly-linked) run queues
 * to be empty.
 */
rqinit()
{
	register int i;

	for (i = 0; i < NQS; i++)
		qs[i].ph_link = qs[i].ph_rlink = (struct proc *)&qs[i];
	simple_lock_init(&callout_lock);
}

slave_start()
{
	register struct thread	*th;
	register int		mycpu;

	/*	Find a thread to execute */

	mycpu = cpu_number();

	splhigh();
	th = choose_thread(current_processor());
	if (th == NULL) {
		printf("Slave %d failed to find any threads.\n", mycpu);
		printf("Should have at least found idle thread.\n");
		halt_cpu();
	}

	/*
	 *	Show that this cpu is using the kernel pmap
	 */
	PMAP_ACTIVATE(kernel_pmap, th, mycpu);

	active_threads[mycpu] = th;

	if (th->task->kernel_vm_space == FALSE) {
		PMAP_ACTIVATE(vm_map_pmap(th->task->map), th, mycpu);
	}

	/*
	 *	Clock interrupt requires that this cpu have an active
	 *	thread, hence it can't be done before this.
	 */
	startrtclock();
	ast_context(th, mycpu);
	load_context(th);
	/*NOTREACHED*/
}

