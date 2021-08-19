/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 14-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Changes for new scheduler:
 *		Remove process lock initialization.
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: removed #include dir.h, Change SPAGI to SPAGV
 *
 * 05-Jan-89  Avadis Tevanian (avie) at NeXT
 *	Removed unused references to proc table.
 *
 * 13-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Removed dependencies on proc table.
 *
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	Set p_stat to SRUN before resuming child.
 *
 * 29-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Remove references to multprog.
 *
 * 30-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 * 15-Dec-87  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Deleted #ifdef romp call to float_fork().
 *
 * 21-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Cleaned up some conditionals.  Deleted old history.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_fork.c	7.1 (Berkeley) 6/5/86
 */

#import <machine/reg.h>
#import <machine/psl.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/proc.h>
#import <sys/vnode.h>
#import <sys/file.h>
#import <sys/acct.h>

#import <kern/thread.h>

#import <machine/spl.h>

thread_t	newproc(), procdup();
/*
 * fork system call.
 */
fork()
{
	fork1(0);
}

vfork()
{

	fork1(1);
}

fork1(isvfork)
	int isvfork;
{
	register struct proc *p1, *p2;
	register a;
	thread_t	th;

	a = 0;
	if (u.u_uid != 0) {
		for (p1 = allproc; p1; p1 = p1->p_nxt)
			if (p1->p_uid == u.u_uid)
				a++;
		for (p1 = zombproc; p1; p1 = p1->p_nxt)
			if (p1->p_uid == u.u_uid)
				a++;
	}
	/*
	 * Disallow if
	 *  No processes at all;
	 *  not su and too many procs owned; or
	 *  not su and would take last slot.
	 */
	p2 = freeproc;
#if	NeXT
	if (p2 == NULL) {
		p2 = getproc();
		if (p2 == NULL)
			tablefull("proc");
		else {
			p2->p_nxt = freeproc;	/* put on freeproc for cloneproc */
			freeproc = p2;
		}
	}
	if (p2==NULL || (u.u_uid!=0 && a>MAXUPRC)) {
#else	NeXT
	if (p2==NULL)
		tablefull("proc");
	if (p2==NULL || (u.u_uid!=0 && (p2->p_nxt == NULL || a>MAXUPRC))) {
#endif	NeXT
		u.u_error = EAGAIN;
		goto out;
	}
	p1 = u.u_procp;
	th = newproc(isvfork);
	thread_dup(current_thread(), th);
	th->u_address.uthread->uu_r.r_val1 = p1->p_pid;
	th->u_address.uthread->uu_r.r_val2 = 1;	/* child */
#if	NeXT
	microtime(&th->u_address.utask->uu_start);
#else	NeXT
	th->u_address.utask->uu_start = time;
#endif	NeXT
	th->u_address.utask->uu_acflag = AFORK;
	u.u_r.r_val1 = p2->p_pid;

	(void) thread_resume(th);
out:
	u.u_r.r_val2 = 0;

}

thread_t cloneproc();

/*
 * Create a new process-- the internal version of
 * sys fork.
 * It returns 1 in the new process, 0 in the old.
 */
thread_t newproc(isvfork)
	int isvfork;
{
	return (cloneproc(u.u_procp, isvfork));
}

/*
 * Create a new process from a specified process.
 */
thread_t
cloneproc(rip, isvfork)
	register struct proc *rip;
	int isvfork;
{
	register struct proc *rpp;
	register struct utask *utask = rip->task->u_address;
	register int n;
	register struct file *fp;
	static int pidchecked = 0;
	thread_t	th;

	/*
	 * First, just locate a slot for a process
	 * and copy the useful info from this process into it.
	 * The panic "cannot happen" because fork has already
	 * checked for the existence of a slot.
	 */
	mpid++;
retry:
	if (mpid >= 30000) {
		mpid = 100;
		pidchecked = 0;
	}
	if (mpid >= pidchecked) {
		int doingzomb = 0;

		pidchecked = 30000;
		/*
		 * Scan the proc table to check whether this pid
		 * is in use.  Remember the lowest pid that's greater
		 * than mpid, so we can avoid checking for a while.
		 */
		rpp = allproc;
again:
		for (; rpp != NULL; rpp = rpp->p_nxt) {
			if (rpp->p_pid == mpid || rpp->p_pgrp == mpid) {
				mpid++;
				if (mpid >= pidchecked)
					goto retry;
			}
			if (rpp->p_pid > mpid && pidchecked > rpp->p_pid)
				pidchecked = rpp->p_pid;
			if (rpp->p_pgrp > mpid && pidchecked > rpp->p_pgrp)
				pidchecked = rpp->p_pgrp;
		}
		if (!doingzomb) {
			doingzomb = 1;
			rpp = zombproc;
			goto again;
		}
	}
#if	NeXT
	if ((rpp = freeproc) == NULL) {
		rpp = getproc();
		if (rpp == NULL)
			panic("no procs");
		rpp->p_nxt = freeproc;		/* put on freeproc for below */
		freeproc = rpp;
	}
#else	NeXT
	if ((rpp = freeproc) == NULL)
		panic("no procs");
#endif	NeXT

	freeproc = rpp->p_nxt;			/* off freeproc */

	/*
	 * Make a proc table entry for the new process.
	 */
	rpp->p_stat = SIDL;
	timerclear(&rpp->p_realtimer.it_value);
	rpp->p_flag = SLOAD | (rip->p_flag & (SPAGI|SOUSIG|SXONLY));
	rpp->p_uid = rip->p_uid;
	rpp->p_pgrp = rip->p_pgrp;
	rpp->p_nice = rip->p_nice;
	rpp->p_pid = mpid;
	rpp->p_ppid = rip->p_pid;
	rpp->p_pptr = rip;
	rpp->p_osptr = rip->p_cptr;
	if (rip->p_cptr)
		rip->p_cptr->p_ysptr = rpp;
	rpp->p_ysptr = NULL;
	rpp->p_cptr = NULL;
	rip->p_cptr = rpp;
	rpp->p_time = 0;
	rpp->p_cpu = 0;
	rpp->p_sigmask = rip->p_sigmask;
	rpp->p_sigcatch = rip->p_sigcatch;
	rpp->p_sigignore = rip->p_sigignore;
	/* take along any pending signals like stops? */
	rpp->p_tptr = 0;
	rpp->p_aptr = 0;
#if	NeXT
	pidhash_enter(rpp);
#else	NeXT
	n = PIDHASH(rpp->p_pid);
	rpp->p_hash = pidhash[n];
	pidhash[n] = rpp;
#endif	NeXT

	/*
	 * Increase reference counts on shared objects.
	 */
	for (n = 0; n <= utask->uu_lastfile; n++) {
		fp = utask->uu_ofile[n];
		if (fp == NULL)
			continue;
		fp->f_count++;
	}
	if (utask->uu_cdir)
		VN_HOLD(utask->uu_cdir);
	if (utask->uu_rdir)
		VN_HOLD(utask->uu_rdir);
	crhold(utask->uu_cred);
	u_cred_lock_init(&utask->uu_cred_lock);

	/*
	 * This begins the section where we must prevent the parent
	 * from being swapped.
	 */
	rip->p_flag |= SKEEP;
	simple_lock_init(&rpp->siglock);
	rpp->sigwait = FALSE;
	rpp->exit_thread = THREAD_NULL;
	th = procdup(rpp, rip);	/* child, parent */
	uarea_init(th);		/* this shouldn't be necessary here XXX */
	/*
	 *	It is now safe to link onto allproc
	 */
	rpp->p_nxt = allproc;			/* onto allproc */
	rpp->p_nxt->p_prev = &rpp->p_nxt;	/*   (allproc is never NULL) */
	rpp->p_prev = &allproc;
	allproc = rpp;
	rpp->p_stat = SRUN;			/* XXX */
	(void) spl0();

	/*
	 * Cause child to take a non-local goto as soon as it runs.
	 * On older systems this was done with SSWAP bit in proc
	 * table; on VAX we use u.u_pcb.pcb_sswap so don't need
	 * to do rpp->p_flag |= SSWAP.  Actually do nothing here.
	 */
	/* rpp->p_flag |= SSWAP; */

	/*
	 * Now can be swapped.
	 */
	rip->p_flag &= ~SKEEP;

	return(th);
}

#import <kern/mach_param.h>

extern struct zone *u_zone;

uzone_init()
{

	u_zone = zinit(sizeof(struct utask),
			THREAD_MAX * sizeof(struct utask),
			8 * sizeof(struct utask),
			FALSE, "u-areas");
}

utask_free(utask)
	struct utask	*utask;
{
	int	cnt = utask->uu_ofile_cnt;

	if (cnt) {
		kfree(utask->uu_ofile, cnt * sizeof(struct file *));
		kfree(utask->uu_pofile, cnt * sizeof(char));
		utask->uu_ofile_cnt = 0;
	}
	zfree(u_zone, (vm_offset_t) utask);
}

uarea_init(th)
	register thread_t	th;
{
	th->u_address.uthread->uu_ap = th->u_address.uthread->uu_arg;
}

uarea_zero(th)
	thread_t	th;
{
	bzero((caddr_t) th->u_address.uthread, sizeof(struct uthread));
}

utask_zero(ta)
	task_t		ta;
{
	bzero((caddr_t) ta->u_address, sizeof(struct utask));
	ta->proc = (struct proc *)0;
}



