/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * Revision 2.24  89/12/22  15:38:25  rpd
 * 	Obscure bug fix: always send SIGMSG and SIGEMSG to traced
 * 	processes to avoid dropping SIGUSR1 and SIGUSR2 on the floor.
 * 	Found by Chia Chao at HP Labs.
 * 	[89/12/01            dlb]
 * 
 * Revision 2.18  89/05/30  10:34:04  rvb
 * 	Fixed the SWITCH_INT thing: only the object you
 * 	switch on needs to be recasted, NOT the individual "case foo"
 * 	entries. [Next time make sure you test your code, tx]
 * 
 * Revision 2.16  89/04/18  16:42:12  mwyoung
 * 	Pick up issig() fix from dlb/avie to avoid signal action on
 * 	system processes, as the comment said.
 * 	[89/04/16            mwyoung]
 * 
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	Removed <sys/dir.h>
 *
 * 24-Oct-88  Steve Stone (steve) at NeXT
 *	Added Attach trace support.
 *
 * 25-Mar-88  Avadis Tevanian, Jr. (avie) at NeXT.
 *	Really be sure task/thread is resumed upon SIGKILL.
 *
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	MACH: No more SSLEEP.
 *
 * 20-Apr-88  David Black (dlb) at Carnegie-Mellon University
 *	Allow exception signals to be used as back-door ipc.  (XXX)
 *
 * 29-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH: Include "machine/vmparam.h" rather than "sys/vm.h" to get
 *	USRSTACK declaration.
 *
 * 15-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Call task_suspend_nowait from interrupt level.
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	issig() and psig() no longer need to switch to/from master; they
 *	are always called when already on master CPU.  (Besides that, there
 *	is no unix_release() in the return path of sig_lock!)
 *
 * 21-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Better sig_lock; and completely set signal state before calling
 *	sendsig().
 *
 * 21-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Cleaned up conditionals (whew!).  Purged history.  Conditionals
 *	could probably be further reduced.
 *
 * 19-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Changed inodes to vnodes.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_sig.c	7.1 (Berkeley) 6/5/86
 */

#import <cputypes.h>
#import <mach_host.h>

#import <machine/reg.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/proc.h>
#import <sys/timeb.h>
#import <sys/times.h>
#import <sys/buf.h>
#import <machine/vmparam.h>	/* Get USRSTACK */
#import <sys/acct.h>
#import <sys/uio.h>
#import <sys/kernel.h>

#import <machine/spl.h>

#import <kern/sched.h>
#import <vm/vm_param.h>
#import <kern/thread.h>
#import <kern/parallel.h>
#import <machine/cpu.h>
#import <kern/sched_prim.h>
#import <kern/task.h>
#import <sys/loader.h>
#import <vm/vm_kern.h>


#define	cantmask	(sigmask(SIGKILL)|sigmask(SIGCONT)|sigmask(SIGSTOP))
#define	stopsigmask	(sigmask(SIGSTOP)|sigmask(SIGTSTP)| \
			sigmask(SIGTTIN)|sigmask(SIGTTOU))

/*
 *	XXX Some ancient code in this file contains a statement that
 *	XXX different compilers want written differently.  Add your machine
 *	XXX to the following #if if its compiler insists that the argument
 *	XXX to switch and all of its cases be integers.  If your compiler
 *	XXX doesn't mind using function pointers that happen to be small
 *	XXX integers, then this doesn't concern you.
 */
#if	defined(vax) || defined(mips) || defined(NeXT) || defined(sequent) || defined(multimax) || defined(__GNU__) || defined(i386)
#define SWITCH_INT	1
#else	
#define SWITCH_INT	0
#endif

/*
 * Generalized interface signal handler.
 */
sigvec()
{
	register struct a {
		int	signo;
		struct	sigvec *nsv;
		struct	sigvec *osv;
	} *uap = (struct a  *)u.u_ap;
	struct sigvec vec;
	register struct sigvec *sv;
	register int sig;
	int bit;

	sig = uap->signo;
	if (sig <= 0 || sig > NSIG || sig == SIGKILL || sig == SIGSTOP) {
		u.u_error = EINVAL;
		return;
	}
	sv = &vec;
	if (uap->osv) {
		sv->sv_handler = u.u_signal[sig];
		sv->sv_mask = u.u_sigmask[sig];
		bit = sigmask(sig);
		sv->sv_flags = 0;
		if ((u.u_sigonstack & bit) != 0)
			sv->sv_flags |= SV_ONSTACK;
		if ((u.u_sigintr & bit) != 0)
			sv->sv_flags |= SV_INTERRUPT;
		u.u_error =
		    copyout((caddr_t)sv, (caddr_t)uap->osv, sizeof (vec));
		if (u.u_error)
			return;
	}
	if (uap->nsv) {
		u.u_error =
		    copyin((caddr_t)uap->nsv, (caddr_t)sv, sizeof (vec));
		if (u.u_error)
			return;
		if (sig == SIGCONT && sv->sv_handler == SIG_IGN) {
			u.u_error = EINVAL;
			return;
		}
		if ((sig == SIGMSG) || (sig == SIGEMSG)) {
			task_t		me = current_task();
			me->ipc_intr_msg = TRUE;
		}
		setsigvec(sig, sv);
	}
}

setsigvec(sig, sv)
	int sig;
	register struct sigvec *sv;
{
	register struct proc *p;
	register int bit;

	bit = sigmask(sig);
	p = u.u_procp;
	/*
	 * Change setting atomically.
	 */
	(void) splhigh();
	sig_lock_simple(p);
	u.u_signal[sig] = sv->sv_handler;
	u.u_sigmask[sig] = sv->sv_mask &~ cantmask;
	if (sv->sv_flags & SV_INTERRUPT)
		u.u_sigintr |= bit;
	else
		u.u_sigintr &= ~bit;
	if (sv->sv_flags & SV_ONSTACK)
		u.u_sigonstack |= bit;
	else
		u.u_sigonstack &= ~bit;
	if (sv->sv_handler == SIG_IGN) {
		p->p_sig &= ~bit;		/* never to be seen again */
		/*
		 *	If this is a thread signal, clean out the
		 *	threads as well.
		 */
		if (bit & threadmask) {
			register	queue_head_t	*list;
			register	thread_t	thread;

			list = &(p->task->thread_list);
			task_lock(p->task);
			thread = (thread_t) queue_first(list);
			while (!queue_end(list, (queue_entry_t) thread)) {
				thread->u_address.uthread->uu_sig &= ~bit;
				thread = (thread_t)
					queue_next(&thread->thread_list);
			}
			task_unlock(p->task);
		}
		p->p_sigignore |= bit;
		p->p_sigcatch &= ~bit;
	} else {
		p->p_sigignore &= ~bit;
		if (sv->sv_handler == SIG_DFL)
			p->p_sigcatch &= ~bit;
		else
			p->p_sigcatch |= bit;
	}
	sig_unlock(p);
	(void) spl0();
}

sigblock()
{
	struct a {
		int	mask;
	} *uap = (struct a *)u.u_ap;
	register struct proc *p = u.u_procp;

	(void) splhigh();
	u.u_r.r_val1 = p->p_sigmask;
	p->p_sigmask |= uap->mask &~ cantmask;
	(void) spl0();
}

sigsetmask()
{
	struct a {
		int	mask;
	} *uap = (struct a *)u.u_ap;
	register struct proc *p = u.u_procp;

	(void) splhigh();
	u.u_r.r_val1 = p->p_sigmask;
	p->p_sigmask = uap->mask &~ cantmask;
	(void) spl0();
}

sigpause()
{
	struct a {
		int	mask;
	} *uap = (struct a *)u.u_ap;
	register struct proc *p = u.u_procp;

	/*
	 * When returning from sigpause, we want
	 * the old mask to be restored after the
	 * signal handler has finished.  Thus, we
	 * save it here and mark the proc structure
	 * to indicate this (should be in u.).
	 */
	u.u_oldmask = p->p_sigmask;
	p->p_flag |= SOMASK;
	p->p_sigmask = uap->mask &~ cantmask;
	for (;;)
		sleep((caddr_t)&u, PSLEP);
	/*NOTREACHED*/
}
#undef cantmask

sigstack()
{
	register struct a {
		struct	sigstack *nss;
		struct	sigstack *oss;
	} *uap = (struct a *)u.u_ap;
	struct sigstack ss;

	if (uap->oss) {
		u.u_error = copyout((caddr_t)&u.u_sigstack, (caddr_t)uap->oss, 
		    sizeof (struct sigstack));
		if (u.u_error)
			return;
	}
	if (uap->nss) {
		u.u_error =
		    copyin((caddr_t)uap->nss, (caddr_t)&ss, sizeof (ss));
		if (u.u_error == 0)
			u.u_sigstack = ss;
	}
}

kill()
{
	register struct a {
		int	pid;
		int	signo;
	} *uap = (struct a *)u.u_ap;
	register struct proc *p;

	if (uap->signo < 0 || uap->signo > NSIG) {
		u.u_error = EINVAL;
		return;
	}
	if (uap->pid > 0) {
		/* kill single process */
		p = pfind(uap->pid);
		if (p == 0) {
			u.u_error = ESRCH;
			return;
		}
		if (u.u_uid && u.u_uid != p->p_uid)
			u.u_error = EPERM;
		else if (uap->signo)
			psignal(p, uap->signo);
		return;
	}
	switch (uap->pid) {
	case -1:		/* broadcast signal */
		u.u_error = killpg1(uap->signo, 0, 1);
		break;
	case 0:			/* signal own process group */
		u.u_error = killpg1(uap->signo, 0, 0);
		break;
	default:		/* negative explicit process group */
		u.u_error = killpg1(uap->signo, -uap->pid, 0);
		break;
	}
	return;
}

killpg()
{
	register struct a {
		int	pgrp;
		int	signo;
	} *uap = (struct a *)u.u_ap;

	if (uap->signo < 0 || uap->signo > NSIG) {
		u.u_error = EINVAL;
		return;
	}
	u.u_error = killpg1(uap->signo, uap->pgrp, 0);
}

/* KILL CODE SHOULDNT KNOW ABOUT PROCESS INTERNALS !?! */

killpg1(signo, pgrp, all)
	int signo, pgrp, all;
{
	register struct proc *p;
	int f, error = 0;

	if (!all && pgrp == 0) {
		/*
		 * Zero process id means send to my process group.
		 */
		pgrp = u.u_procp->p_pgrp;
		if (pgrp == 0)
			return (ESRCH);
	}
	for (f = 0, p = allproc; p != NULL; p = p->p_nxt) {
		if ((p->p_pgrp != pgrp && !all) || p->p_ppid == 0 ||
		    (p->p_flag&SSYS) || (all && p == u.u_procp))
			continue;
		if (u.u_uid != 0 && u.u_uid != p->p_uid &&
		    (signo != SIGCONT || !inferior(p))) {
			if (!all)
				error = EPERM;
			continue;
		}
		f++;
		if (signo)
			psignal(p, signo);
	}
	return (error ? error : (f == 0 ? ESRCH : 0));
}

/*
 * Send the specified signal to
 * all processes with 'pgrp' as
 * process group.
 */
gsignal(pgrp, sig)
	register int pgrp;
{
	register struct proc *p;

	if (pgrp == 0)
		return;
	for (p = allproc; p != NULL; p = p->p_nxt)
		if (p->p_pgrp == pgrp)
			psignal(p, sig);
}

/*
 * Send the specified signal to
 * the specified process.
 */
psignal(p, sig)
	register struct proc *p;
	register int sig;
{
	register int s;
	register void (*action)();
	register thread_t	sig_thread;
	register task_t		sig_task;
	register thread_t	cur_thread;
	int mask;

	if ((unsigned)sig > NSIG)
		return;
	mask = sigmask(sig);
	/*
	 *	We will need the task pointer later.  Grab it now to
	 *	check for a zombie process.  Also don't send signals
	 *	to kernel internal tasks.
	 */
	if (((sig_task = p->task) == TASK_NULL) || sig_task->kernel_vm_space)
		return;

	/*
	 * If proc is traced, always give parent a chance.
	 */
	if (p->p_flag & STRC)
		action = SIG_DFL;
	else {
		/*
		 * If the signal is being ignored,
		 * then we forget about it immediately.
		 */
		if (p->p_sigignore & mask)
			return;
		if (p->p_sigmask & mask)
			action = SIG_HOLD;
		else if (p->p_sigcatch & mask)
			action = SIG_CATCH;
		else
			action = SIG_DFL;
	}

	if (sig) {
		p->p_sig |= mask;
		switch (sig) {

		case SIGTERM:
			if ((p->p_flag&STRC) || action != SIG_DFL)
				break;
			/* fall into ... */

		case SIGCONT:
			p->p_sig &= ~stopsigmask;
			break;

		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			p->p_sig &= ~sigmask(SIGCONT);
			break;
		case SIGMSG:
		case SIGEMSG:
			/*
			 *	Don't post these signals unless
			 *	they'll be handled, regardless of
			 *	the state of the recipient.  Always send
			 *	them to a traced process.
			 */
			if ((action == SIG_DFL) && !(p->p_flag & STRC)){
				p->p_sig &= ~mask;
				return;
			}
			break;
		}
	}
	/*
	 * Defer further processing for signals which are held.
	 */
	if (action == SIG_HOLD)
		return;

	/* 
	 *	Pick a thread to get signal -- 	current thread
	 *	if possible, else first thread in the task.  We have
	 *	an implicit reference to the current_thread, but need
	 *	an explicit one otherwise.  The thread reference keeps
	 *	the corresponding task data structures around too.  This
	 *	reference is released by thread_deallocate_interrupt
	 *	because psignal() can be called from interrupt level).
	 */
	s = splhigh();		/* at least splsched */

	cur_thread = current_thread();
	if (sig_task == cur_thread->task) {
		sig_thread = cur_thread;
	}
	else {
		/*
		 *	This is a mess.  The thread_list_lock is a special
		 *	lock that excludes insert and delete operations
		 *	on the task's thread list for our benefit (can't
		 *	grab task lock because we might be at interrupt
		 *	level.  Check if there are any threads in the
		 *	task.  If there aren't, sending it a signal
		 *	isn't going to work very well, so just return.
		 */
		simple_lock(&sig_task->thread_list_lock);
		if (queue_empty(&sig_task->thread_list)) {
			simple_unlock(&sig_task->thread_list_lock);
			(void) splx(s);
			return;
		}
		sig_thread = (thread_t) queue_first(&sig_task->thread_list);
		thread_reference(sig_thread);
		simple_unlock(&sig_task->thread_list_lock);
	}

	/*
	 *	SIGKILL priority twiddling moved here from above because
	 *	it needs sig_thread.  Could merge it into large switch
	 *	below if we didn't care about priority for tracing
	 *	as SIGKILL's action is always SIG_DFL.
	 */
	if ((sig == SIGKILL) && (p->p_nice > NZERO)) {
		p->p_nice = NZERO;
		thread_max_priority(sig_thread, sig_thread->processor_set,
			BASEPRI_USER);
		thread_priority(sig_thread, BASEPRI_USER, FALSE);
	}

	if (p->p_flag&STRC) {
		/*
		 *	Process is traced - wake it up (if not already
		 *	stopped) so that it can discover the signal in
		 *	issig() and stop for the parent.
		 */
		if (p->p_stat != SSTOP) {
			/*
			 *	Wake it up to get signal
			 */
			goto run;
		}
		goto out;
	}
	else if (action != SIG_DFL) {
		/*
		 *	User wants to catch the signal.
		 *	Wake up the thread, but don't un-suspend it
		 *	(except for SIGCONT).
		 */
		if (sig == SIGCONT) {
			(void) task_resume(sig_task);
			/*
			 *	Process will be running after 'run'
			 */
			p->p_stat = SRUN;
		}
		goto run;
	}
	else {
		/*
		 *	Default action - varies
		 */

		switch (sig) {

		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			/*
			 * These are the signals which by default
			 * stop a process.
			 *
			 * Don't clog system with children of init
			 * stopped from the keyboard.
			 */
#if	NeXT
			if (sig != SIGSTOP && p->p_pptr == init_proc) {
#else	NeXT
			if (sig != SIGSTOP && p->p_pptr == &proc[1]) {
#endif	NeXT
				psignal(p, SIGKILL);
				p->p_sig &= ~mask;
				goto out;
			}
			/*
			 *	Stop the task.
			 */
			if ((sig_thread->state & TH_RUN) == 0) {
				/*
				 *	If task hasn't already been stopped by
				 *	a signal, stop it.
				 */
				p->p_sig &= ~mask;
				if (sig_task->user_stop_count == 0) {
					/*
					 * p_cursig must not be set, because
					 * it will be psig()'d if it is not
					 * zero, and the signal is being
					 * handled here.  But save the signal
					 * in p_stopsig so WUNTRACED
					 * option to wait can find it.
					 */
					p->p_stopsig = sig;
					psignal(p->p_pptr, SIGCHLD);
					stop(p);
				}
				goto out;
			}
			else {
				if ((p == u.u_procp) && (p->p_stat != SZOMB))
					aston();
				goto out;
			}

		case SIGIO:
		case SIGURG:
		case SIGCHLD:
		case SIGWINCH:
		case SIGMSG:
		case SIGEMSG:
			/*
			 * These signals do not get propagated.  If the
			 * process isn't interested, forget it.
			 */
			p->p_sig &= ~mask;
			goto out;

		case SIGKILL:
			/*
			 * Kill signal always sets process running and
			 * unsuspends it.
			 */
			while (sig_task->user_stop_count > 0)
				(void) task_resume(sig_task);
			/*
			 *	Process will be running after 'run'
			 */
			p->p_stat = SRUN;

			/*
			 * Break it out of user wait, as well.
			 */
			while (sig_thread->user_stop_count > 0)
				(void) thread_resume(sig_thread);

			/*
			 * Clear system wait if possible.  The
			 * THREAD_SHOULD_TERMINATE is overkill, but
			 * saves us from potentially buggy code elsewhere.
			 */
			clear_wait(sig_thread, THREAD_SHOULD_TERMINATE, FALSE);

#if	MACH_HOST
			/*
			 * Make sure it can run.
			 */
			if (sig_thread->processor_set->empty)
				thread_assign(sig_thread, &default_pset);
#endif	MACH_HOST
			/*
			 * If we're delivering the signal to some other
			 * thread, that thread might be stuck in an
			 * exception.  Break it out.  Can't call
			 * thread_exception_abort from high spl, but
			 * SIGKILL can't be sent from interrupt level, so
			 * it's ok to drop spl.  Can call thread_deallocate
			 * for same reason.
			 */
			splx(s);
			if (sig_thread != cur_thread) {
				thread_exception_abort(sig_thread);
				thread_deallocate(sig_thread);
			}
			return;

		case SIGCONT:
			/*
			 * Let the process run.  If it's sleeping on an
			 * event, it remains so.
			 */
			(void) task_resume(sig_task);
			p->p_stat = SRUN;
			goto out;

		default:
			/*
			 * All other signals wake up the process, but don't
			 * resume it.
			 */
			goto run;
		}
	}
	/*NOTREACHED*/
run:
	/*
	 *	BSD used to raise priority here.  This has been broken
	 *	for ages and nobody's noticed.  Code deleted. -dlb
	 */

	/*
	 *	Wake up the thread if it is interruptible.
	 */
	clear_wait(sig_thread, THREAD_INTERRUPTED, TRUE);
out:
	splx(s);
	if (sig_thread != cur_thread)
		thread_deallocate_interrupt(sig_thread);
}

/*
 * Returns true if the current
 * process has a signal to process.
 * The signal to process is put in p_cursig.
 * This is asked at least once each time a process enters the
 * system (though this can usually be done without actually
 * calling issig by checking the pending signal masks.)
 * A signal does not do anything
 * directly to a process; it sets
 * a flag that asks the process to
 * do something to itself.
 */
issig()
{
	register struct proc *p;
	register int sig;
	int sigbits, mask;

	p = u.u_procp;
	/*
	 *	This must be called on master cpu
	 */
	if (cpu_number() != master_cpu)
		panic("issig not on master");

	/*
	 *	Try for the signal lock.
	 *	If we already have it, return FALSE: don't handle any signals.
	 *	If we must halt, return TRUE to clean up our state.
	 */
	sig_lock_or_return(p, return(FALSE), return(TRUE));
	for (;;) {
		/*
		 *	In a multi-threaded task it is possible for
		 *	one thread to interrupt another's issig(); psig()
		 *	sequence.  In this case, the thread signal may
		 *	be left in u.u_cursig.  We recover here
		 *	by getting it out and starting over.
		 */
		if(u.u_cursig != 0) {
		    u.u_sig |= sigmask(u.u_cursig);
		    u.u_cursig = 0;
		}
		sigbits = (u.u_sig | p->p_sig) &~ p->p_sigmask;
		if ((p->p_flag&STRC) == 0)
			sigbits &= ~p->p_sigignore;
		if (p->p_flag&SVFORK)
			sigbits &= ~stopsigmask;
		if (sigbits == 0)
			break;
		sig = ffs((long)sigbits);
		mask = sigmask(sig);
		if (mask & threadmask) {
			u.u_cursig = sig;
			u.u_sig &= ~mask;
		}
		p->p_sig &= ~mask;		/* take the signal! */
		p->p_cursig = sig;
		if (p->p_flag&STRC && (p->p_flag&SVFORK) == 0) {
			register int	hold;
			register task_t	task;

			/*
			 * If traced, always stop, and stay
			 * stopped until released by the parent.
			 */
			psignal(p->p_pptr, SIGCHLD);
			/*
			 *	New ptrace() has no procxmt.  Only action
			 *	executed on behalf of parent is exit.
			 *	This is requested via a large p_cursig.
			 *	sig_lock_to_wait causes other threads
			 *	that try to handle signals to stop for
			 *	debugger.
			 */
			p->thread = current_thread();
#ifdef	i386
#else	i386
			pcb_synch(p->thread);
#endif	i386
			/*
			 *	XXX Have to really stop for debuggers;
			 *	XXX stop() doesn't do the right thing.
			 *	XXX Inline the task_suspend because we
			 *	XXX have to diddle Unix state in the
			 *	XXX middle of it.
			 */
			task = p->task;
			hold = FALSE;
			task_lock(task);
			if ((task->user_stop_count)++ == 0)
				hold = TRUE;
			task_unlock(task);

			if (hold) {
				(void) task_hold(task);
				sig_lock_to_wait(p);
				(void) task_dowait(task, TRUE);
				thread_hold(current_thread());
			}
			else {
				sig_lock_to_wait(p);
			}
			p->p_stat = SSTOP;
			p->p_flag &= ~SWTED;
			wakeup((caddr_t)p->p_pptr);
			thread_block();
			sig_wait_to_lock(p);
			/*
			 *	We get here only if task
			 *	is continued or killed.  Kill condition
			 *	is signalled by adding NSIG to p_cursig.
			 *	Pass original p_cursig as exit value in
			 *	this case.
			 */
			if (p->p_cursig > NSIG) {
				label_t qsave_tmp;

				sig = p->p_cursig - NSIG;
				/*
				 *	Wait event may still be outstanding;
				 *	clear it, since sig_lock_to_exit will
				 *	wait.
				 */
				clear_wait(current_thread(),
					THREAD_INTERRUPTED,
					FALSE);
				sig_lock_to_exit(p);
				/*
				 * Since this thread will be resumed
				 * to allow the current syscall to
				 * be completed, must save u_qsave
				 * before calling exit().  (Since exit()
				 * calls closef() which can trash u_qsave.)
				 */
				qsave_tmp = u.u_qsave;
				exit(sig);
				u.u_qsave = qsave_tmp;
			}
			/*
			 *	We may have to quit
			 */
			if (thread_should_halt(current_thread())) {
				sig_unlock(p);
				return(TRUE);
			}
			/*
			 * If the traced bit got turned off,
			 * then put the signal taken above back into p_sig
			 * and go back up to the top to rescan signals.
			 * This ensures that p_sig* and u_signal are consistent.
			 */
			if ((p->p_flag&STRC) == 0) {
				if (mask & threadmask)
					u.u_sig |= mask;
				else
					p->p_sig |= mask;
				continue;
			}

			/*
			 * If parent wants us to take the signal,
			 * then it will leave it in p->p_cursig;
			 * otherwise we just look for signals again.
			 */
			sig = p->p_cursig;
			if (sig == 0)
				continue;

			/*
			 * If signal is being masked put it back
			 * into p_sig and look for other signals.
			 */
			mask = sigmask(sig);
			if (p->p_sigmask & mask) {
				if (mask & threadmask)
					u.u_sig |= mask;
				else
					p->p_sig |= mask;
				continue;
			}
		}
		switch ((int)u.u_signal[sig]) {

		case (int)SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (p->p_ppid == 0) {
				u.u_cursig = 0;
				break;
			}
			switch (sig) {

			case SIGTSTP:
			case SIGTTIN:
			case SIGTTOU:
				/*
				 * Children of init aren't allowed to stop
				 * on signals from the keyboard.
				 */
#if	NeXT
				if (p->p_pptr == init_proc) {
#else	NeXT
				if (p->p_pptr == &proc[1]) {
#endif	NeXT
					psignal(p, SIGKILL);
					continue;
				}
				/* fall into ... */

			case SIGSTOP:
				if (p->p_flag&STRC)
					continue;
				psignal(p->p_pptr, SIGCHLD);
				stop(p);
				sig_lock_to_wait(p);
				thread_block();
				sig_wait_to_lock(p);
				/*
				 *	We may have to quit
				 */
				if (thread_should_halt(current_thread())) {
					sig_unlock(p);
					return(TRUE);
				}
				continue;

			case SIGCONT:
			case SIGCHLD:
			case SIGURG:
			case SIGIO:
			case SIGWINCH:
			case SIGMSG:
			case SIGEMSG:
				/*
				 * These signals are normally not
				 * sent if the action is the default.
				 */
				continue;		/* == ignore */

			default:
				goto send;
			}
			/*NOTREACHED*/

		case (int)SIG_HOLD:
		case (int)SIG_IGN:
			/*
			 * Masking above should prevent us
			 * ever trying to take action on a held
			 * or ignored signal, unless process is traced.
			 */
			if ((p->p_flag&STRC) == 0)
				printf("issig\n");
			continue;

		default:
			/*
			 * This signal has an action, let
			 * psig process it.
			 */
			goto send;
		}
		/*NOTREACHED*/
	}
	/*
	 * Didn't find a signal to send.
	 */
	p->p_cursig = 0;
	u.u_cursig = 0;
	sig_unlock(p);
	return (0);

send:
	/*
	 * Let psig process the signal.
	 */
	sig_unlock(p);
	return (sig);
}

/*
 * Put the argument process into the stopped
 * state and notify the parent via wakeup.
 * Signals are handled elsewhere.
 */
stop(p)
	register struct proc *p;
{

	/*
	 *	Call special task_suspend routine,
	 *	because this routine is called from interrupts
	 *	(psignal) and cannot sleep.
	 */
	(void) task_suspend_nowait(p->task);	/*XXX*/
	p->p_stat = SSTOP;
	p->p_flag &= ~SWTED;
	wakeup((caddr_t)p->p_pptr);
}

/*
 * Perform the action specified by
 * the current signal.
 * The usual sequence is:
 *	if (issig())
 *		psig();
 * The signal bit has already been cleared by issig,
 * and the current signal number stored in p->p_cursig.
 */
psig()
{
	register struct proc *p = u.u_procp;
	register int sig = p->p_cursig;
	int mask = sigmask(sig), returnmask;
	register void (*action)();

	/*
	 *	This must be called on master cpu
	 */
	if (cpu_number() != master_cpu)
		panic("psig not on master");

	/*
	 *	Try for the signal lock.  Don't proceed further if we
	 *	are already supposed to halt.
	 */
	sig_lock(p);
	sig = p->p_cursig;
	mask = sigmask(sig);

	/*
	 *	If another thread got here first (sig == 0) or this is
	 *	a thread signal for another thread, bail out.
	 */
	if ((sig == 0) || ((mask & threadmask) && (sig != u.u_cursig))) {
		sig_unlock(p);
		return;
	}
	/*
	 *  A polled resource pause condition which is being retried from the
	 *  system call level may be interrupted on the way back out to user
	 *  mode to be retried with a pause diagnostic message pending.  Always
	 *  clear the condition here before processing an interrupting signal
	 *  to keep the pause/continue diagnostic messages paired.
	 */
	if (u.u_rpswhich&URPW_NOTIFY)
	    rpcont();

	action = u.u_signal[sig];
	if (action != SIG_DFL) {
		if (action == SIG_IGN || (p->p_sigmask & mask))
			panic("psig action");
		u.u_error = 0;
		/*
		 * Set the new mask value and also defer further
		 * occurences of this signal (unless we're simulating
		 * the old signal facilities). 
		 *
		 * Special case: user has done a sigpause.  Here the
		 * current mask is not of interest, but rather the
		 * mask from before the sigpause is what we want restored
		 * after the signal processing is completed.
		 */
		(void) splhigh();
		if (p->p_flag & SOUSIG) {
			if (sig != SIGILL && sig != SIGTRAP) {
				u.u_signal[sig] = SIG_DFL;
				p->p_sigcatch &= ~mask;
			}
			mask = 0;
		}
		if (p->p_flag & SOMASK) {
			returnmask = u.u_oldmask;
			p->p_flag &= ~SOMASK;
		} else
			returnmask = p->p_sigmask;
		p->p_sigmask |= u.u_sigmask[sig] | mask;
		/*
		 *	Fix up the signal state and unlock before
		 *	we send the signal.
		 */
		p->p_cursig = 0;
		if (sigmask(sig) & threadmask)
			u.u_cursig = 0;
		sig_unlock(p);
 		(void) spl0();
 		u.u_ru.ru_nsignals++;
 		sendsig(action, sig, returnmask);
		return;
	}
	u.u_acflag |= AXSIG;
	switch (sig) {
	/*
	 *	The new signal code for multiple threads makes it possible
	 *	for a multi-threaded task to get here (a thread that didn't
	 *	originally process a "stop" signal notices that cursig is
	 *	set), therefore, we must handle this.
	 */
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
	case SIGSTOP:
		sig_unlock(p);
		return;

	case SIGILL:
	case SIGIOT:
	case SIGBUS:
	case SIGQUIT:
	case SIGTRAP:
	case SIGEMT:
	case SIGFPE:
	case SIGSEGV:
	case SIGSYS:
		u.u_arg[0] = sig;
		/*
		 *	Indicate that we are about to exit.
		 *	disables all further signal processing for p.
		 */
		sig_lock_to_exit(p);
		if (core())
			sig += 0200;
		break;
	default:
		sig_lock_to_exit(p);
	}
	exit(sig);
}

/*
 * Create a core image on the file "core"
 * If you are looking for protection glitches,
 * there are probably a wealth of them here
 * when this occurs to a suid command.
 *
 * It writes UPAGES block of the
 * user.h area followed by the entire
 * data+stack segments.
 */
core()
{
	struct vnode	*vp;
	struct vattr	vattr;
	vm_map_t	map;
	int		thread_count, segment_count;
	int		command_size, header_size;
	int		hoffset, foffset, vmoffset;
	vm_offset_t	header;
	struct machine_slot	*ms;
	struct mach_header	*mh;
	struct segment_command	*sc;
	struct thread_command	*tc;
	vm_size_t	size;
	vm_prot_t	prot;
	vm_prot_t	maxprot;
	vm_inherit_t	inherit;
	boolean_t	is_shared;
	port_t		name;
	vm_offset_t	offset;
	int		error;
	task_t		task;
	thread_t	thread;
	char		core_name[20];

	if (u.u_procp->p_flag&SXONLY)
		return (0);
	u.u_uid = u.u_ruid;
	u.u_procp->p_uid = u.u_ruid;
	u.u_gid = u.u_rgid;

	/*
	 *	Don't want to resource-pause while core-dumping.
	 */
	u.u_rpause = 0;

	task = current_task();
	map = task->map;
	if (map->size >= u.u_rlimit[RLIMIT_CORE].rlim_cur)
		return (0);
	(void) task_halt(task);	/* stop this task, except for current thread */
	/*
	 *	Make sure all registers, etc. are in pcb so they get
	 *	into core file.
	 */
	pcb_synch(current_thread());
	u.u_error = 0;
	vattr_null(&vattr);
	vattr.va_type = VREG;
	vattr.va_mode = 0644;
	sprintf(core_name, "/cores/core.%d", u.u_procp->p_pid);
	u.u_error =
	    vn_create(core_name, UIO_SYSSPACE, &vattr, NONEXCL, VWRITE, &vp,
		      u.u_cred);
	if (u.u_error) {
		u.u_error = 0;
		vattr_null(&vattr);
		vattr.va_type = VREG;
		vattr.va_mode = 0644;
		u.u_error =
		    vn_create("core", UIO_SYSSPACE, &vattr, NONEXCL,
		    	VWRITE, &vp, u.u_cred);
		if (u.u_error)
			return (0);
	}
	if (vattr.va_nlink != 1) {
		u.u_error = EFAULT;
		goto out;
	}
	vattr_null(&vattr);
	vattr.va_size = 0;
	(void) VOP_SETATTR(vp, &vattr, u.u_cred);
	u.u_acflag |= ACORE;

	/*
	 *	If the task is modified while dumping the file
	 *	(e.g., changes in threads or VM, the resulting
	 *	file will not necessarily be correct.
	 */

	thread_count = task->thread_count;
	segment_count = map->nentries;

	command_size = segment_count*sizeof(struct segment_command) +
		thread_count*sizeof(struct thread_command);
	command_size += (6*sizeof(unsigned long)
		+ sizeof(struct NeXT_thread_state_regs)
		+ sizeof(struct NeXT_thread_state_68882)
		+ sizeof(struct NeXT_thread_state_user_reg))*thread_count;

	header_size = command_size + sizeof(struct mach_header);

	header = kmem_alloc(kernel_map, header_size);

	/*
	 *	Set up Mach-O header.
	 */
	mh = (struct mach_header *) header;
	ms = &machine_slot[cpu_number()];
	mh->magic = MH_MAGIC;
	mh->cputype = ms->cpu_type;
	mh->cpusubtype = ms->cpu_subtype;
	mh->filetype = MH_CORE;
	mh->ncmds = segment_count + thread_count;
	mh->sizeofcmds = command_size;

	hoffset = sizeof(struct mach_header);	/* offset into header */
	foffset = round_page(header_size);	/* offset into file */
	vmoffset = VM_MIN_ADDRESS;		/* offset into VM */
	error = 0;
	while ((segment_count > 0) && (error == 0)) {
		/*
		 *	Get region information for next region.
		 */
		if (vm_region(map, &vmoffset, &size, &prot, &maxprot,
			  &inherit, &is_shared, &name, &offset)
					== KERN_NO_SPACE)
			break;

		/*
		 *	Fill in segment command structure.
		 */
		sc = (struct segment_command *) (header + hoffset);
		sc->cmd = LC_SEGMENT;
		sc->cmdsize = sizeof(struct segment_command);
		/* segment name is zerod by kmem_alloc */
		sc->vmaddr = vmoffset;
		sc->vmsize = size;
		sc->fileoff = foffset;
		sc->filesize = size;
		sc->maxprot = maxprot;
		sc->initprot = prot;
		sc->nsects = 0;

		/*
		 *	Write segment out.  Try as hard as possible to
		 *	get read access to the data.
		 */
		if ((prot & VM_PROT_READ) == 0) {
			vm_protect(map, vmoffset, size, FALSE,
				   prot|VM_PROT_READ);
		}
		/*
		 *	Only actually perform write if we can read.
		 *	Note: if we can't read, then we end up with
		 *	a hole in the file.
		 */
		if ((maxprot & VM_PROT_READ) == VM_PROT_READ) {
			error = vn_rdwr(UIO_WRITE, vp, vmoffset, size, foffset,
				UIO_USERSPACE, IO_UNIT, (int *) 0);
		}

		hoffset += sizeof(struct segment_command);
		foffset += size;
		vmoffset += size;
		segment_count--;
	}
	task_lock(task);
	thread = (thread_t) queue_first(&task->thread_list);
	while (thread_count > 0) {
		/*
		 *	Fill in thread command structure.
		 */
		tc = (struct thread_command *) (header + hoffset);
		tc->cmd = LC_THREAD;
		tc->cmdsize = sizeof(struct thread_command)
				+ 6*sizeof(unsigned long) +
				+ sizeof(struct NeXT_thread_state_regs)
				+ sizeof(struct NeXT_thread_state_68882)
				+ sizeof(struct NeXT_thread_state_user_reg);
		hoffset += sizeof(struct thread_command);

		size = NeXT_THREAD_STATE_REGS_COUNT;
		*(unsigned long *)(header+hoffset) = NeXT_THREAD_STATE_REGS;
		hoffset += sizeof(unsigned long);
		*(unsigned long *)(header+hoffset) = size;
		hoffset += sizeof(unsigned long);
		thread_getstatus(thread, NeXT_THREAD_STATE_REGS,
				 (struct NeXT_thread_state_regs *)
					(header+hoffset),
				 &size);
		hoffset += size*sizeof(unsigned long);
		size = NeXT_THREAD_STATE_68882_COUNT;
		*(unsigned long *)(header+hoffset) = NeXT_THREAD_STATE_68882;
		hoffset += sizeof(unsigned long);
		*(unsigned long *)(header+hoffset) = size;
		hoffset += sizeof(unsigned long);
		thread_getstatus(thread, NeXT_THREAD_STATE_68882,
				 (struct NeXT_thread_state_68882 *)
					(header+hoffset),
				 &size);
		hoffset += size*sizeof(unsigned long);
		size = NeXT_THREAD_STATE_USER_REG_COUNT;
		*(unsigned long *)(header+hoffset) = NeXT_THREAD_STATE_USER_REG;
		hoffset += sizeof(unsigned long);
		*(unsigned long *)(header+hoffset) = size;
		hoffset += sizeof(unsigned long);
		thread_getstatus(thread, NeXT_THREAD_STATE_USER_REG,
				 (struct NeXT_thread_state_user_reg *)
					(header+hoffset),
				 &size);
		hoffset += size*sizeof(unsigned long);
		thread = (thread_t) queue_next(&thread->thread_list);
		thread_count--;
	}
	task_unlock(task);

	/*
	 *	Write out the Mach header at the beginning of the
	 *	file.
	 */
	error = vn_rdwr(UIO_WRITE, vp, header, header_size, (off_t)0,
			UIO_SYSSPACE, IO_UNIT, (int *) 0);
	kmem_free(kernel_map, header, header_size);
out:
	VN_RELE(vp);
	u.u_error = error;
	return(error == 0);
}