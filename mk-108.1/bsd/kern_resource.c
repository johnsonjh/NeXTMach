/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 *  7-Aug-90  Gregg Kellogg (gk) at NeXT
 *	donice() was incorrectly setting u.u_error to EPERM when it checked
 *	for suser().  Moved the check to a point where the call fails if
 *	the caller isn't the super-user.
 *
 * 18-May-90  Avadis Tevanian (avie) at NeXT
 *	Changed to use sensible priorities (higher numbers -> higher pri).
 *
 *  9-May-90  Gregg Kellogg (gk) at NeXT
 *	Incorporate changes to donice from dlb.
 *
 * 23-Apr-90  Gregg Kellogg (gk) at NeXT
 *	Changed donice to go through the thread-list so that the maximum
 *	priority can be changed.  This is only done if the priority to be
 *	set exceeds that of the present max_priority.
 *
 * 29-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Fixed priority computation in donice().  It now computes the
 *	new task priority based on the old value plus the change in
 *	nice levels.  It also uses task_priority rather than thread_priority
 *	on p->thread, which was bogus.
 *
 * Revision 2.10  89/10/11  13:37:52  dlb
 * 	Change priority computation in donice().
 * 	Have donice() call new interface routines.
 * 	[89/05/11            dlb]
 * 
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Cleaned up casting of uids to ints.  
 *      Removed dir.h.
 *
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TIME_NEW is now standard.
 *
 * 29-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH: Removed use of "sys/vm.h".
 *
 *  2-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	Use thread_read_times to get times for self.  This replaces
 *	and generalized the MACH_TIME_NEW code.
 *
 * 21-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Reduced conditionals, purged history.
 *
 * 19-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Changed inodes to vnodes.
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_resource.c	7.1 (Berkeley) 6/5/86
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/vnode.h>
#import <sys/proc.h>
#import <sys/vfs.h>
#import <sys/uio.h>
#import <sys/kernel.h>

#import <machine/spl.h>

#import <kern/thread.h>
#import <sys/time_value.h>

#import <vm/vm_param.h>
#import <sys/vmparam.h>

/*
 * Resource controls and accounting.
 */

getpriority()
{
	register struct a {
		int	which;
		int	who;
	} *uap = (struct a *)u.u_ap;
	register struct proc *p;
	int low = PRIO_MAX + 1;

	switch (uap->which) {

	case PRIO_PROCESS:
		if (uap->who == 0)
			p = u.u_procp;
		else
			p = pfind(uap->who);
		if (p == 0)
			break;
		low = p->p_nice;
		break;

	case PRIO_PGRP:
		if (uap->who == 0)
			uap->who = u.u_procp->p_pgrp;
		for (p = allproc; p != NULL; p = p->p_nxt) {
			if (p->p_pgrp == uap->who &&
			    p->p_nice < low)
				low = p->p_nice;
		}
		break;

	case PRIO_USER:
		if (uap->who == 0)
			uap->who = (int) u.u_uid;
		for (p = allproc; p != NULL; p = p->p_nxt) {
			if (p->p_uid == uap->who &&
			    p->p_nice < low)
				low = p->p_nice;
		}
		break;

	default:
		u.u_error = EINVAL;
		return;
	}
	if (low == PRIO_MAX + 1) {
		u.u_error = ESRCH;
		return;
	}
	u.u_r.r_val1 = low;
}

setpriority()
{
	register struct a {
		int	which;
		int	who;
		int	prio;
	} *uap = (struct a *)u.u_ap;
	register struct proc *p;
	int found = 0;

	switch (uap->which) {

	case PRIO_PROCESS:
		if (uap->who == 0)
			p = u.u_procp;
		else
			p = pfind(uap->who);
		if (p == 0)
			break;
		donice(p, uap->prio);
		found++;
		break;

	case PRIO_PGRP:
		if (uap->who == 0)
			uap->who = u.u_procp->p_pgrp;
		for (p = allproc; p != NULL; p = p->p_nxt)
			if (p->p_pgrp == uap->who) {
				donice(p, uap->prio);
				found++;
			}
		break;

	case PRIO_USER:
		if (uap->who == 0)
			uap->who = (int) u.u_uid;
		for (p = allproc; p != NULL; p = p->p_nxt)
			if (p->p_uid == uap->who) {
				donice(p, uap->prio);
				found++;
			}
		break;

	default:
		u.u_error = EINVAL;
		return;
	}
	if (found == 0)
		u.u_error = ESRCH;
}

donice(p, n)
	register struct proc *p;
	register int n;
{
        register task_t task;
        register thread_t th;
        register int    pri;

        if (u.u_uid && u.u_ruid &&
            u.u_uid != p->p_uid && u.u_ruid != p->p_uid) {
                u.u_error = EPERM;
                return;
        }
        if (n > PRIO_MAX)
                n = PRIO_MAX;
        if (n < PRIO_MIN)
                n = PRIO_MIN;
        if (n < p->p_nice && !suser()) {
                u.u_error = EACCES;
                return;
	}

	/*
	 * Compute nice from old priority and nice values.
	 */
	task = p->task;
#if	NeXT
	pri = task->priority + p->p_nice/2 - n/2;
#else	NeXT
	pri = task->priority - p->p_nice/2 + n/2;
#endif	NeXT
	p->p_nice = n;
	(void)task_priority(task, pri, FALSE);	/* Cannot fail */

	/*
         *      Now change priority for all threads in the task.
	 */
	task_lock(task);
	for (  th = (thread_t) queue_first(&task->thread_list)
	     ; !queue_end(&task->thread_list, (queue_t)th)
	     ; th = (thread_t) queue_next(&th->thread_list))
	{
#if	NeXT
		if (pri > th->max_priority)
#else	NeXT
		if (th->max_priority > pri)
#endif	NeXT
		{
#if     MACH_HOST
                        /*
                         *      Doing this at exactly the wrong moment
                         *      to a thread being assigned might fail.
                         *      Just repeat it until it works.
                         */
                        while (
                            thread_max_priority(th, th->processor_set, pri)
                                != KERN_SUCCESS);
#else   MACH_HOST
                        /*
                         *      This cannot fail.
                         */
                        (void)thread_max_priority(th, th->processor_set, pri);
#endif  MACH_HOST
		}

		if (thread_priority(th, pri, TRUE) != KERN_SUCCESS) {
			u.u_error = EPERM;
			break;
		}
	}
	task_unlock(task);
}

setrlimit()
{
	register struct a {
		u_int	which;
		struct	rlimit *lim;
	} *uap = (struct a *)u.u_ap;
	struct rlimit alim;
	register struct rlimit *alimp;

	if (uap->which >= RLIM_NLIMITS) {
		u.u_error = EINVAL;
		return;
	}
	alimp = &u.u_rlimit[uap->which];
	u.u_error = copyin((caddr_t)uap->lim, (caddr_t)&alim,
		sizeof (struct rlimit));
	if (u.u_error)
		return;
	if (alim.rlim_cur > alimp->rlim_max || alim.rlim_max > alimp->rlim_max)
		if (!suser())
			return;
	if (uap->which == RLIMIT_STACK) {
		vm_offset_t	addr;
		vm_size_t	size;

		if (alim.rlim_cur > alimp->rlim_cur) {
			/*
			 * Stack size is growing, allocate VM.
			 */
			addr = trunc_page(USRSTACK - alim.rlim_cur);
			size = round_page(alim.rlim_cur);
			size -= round_page(alimp->rlim_cur);
			if (vm_allocate(current_task()->map, &addr, size,
					FALSE) != KERN_SUCCESS) {
				u.u_error = EINVAL;
				return;
			}
		}
		else {
			/*
			 * Stack size is shrinking, free VM.
			 */
			addr = trunc_page(USRSTACK - alimp->rlim_cur);
			size = round_page(alimp->rlim_cur);
			size -= round_page(alim.rlim_cur);
			if (vm_deallocate(current_task()->map, addr, size) !=
			       KERN_SUCCESS) {
				u.u_error = EINVAL;
				return;
			}
		}
	}
	*alimp = alim;
}

getrlimit()
{
	register struct a {
		u_int	which;
		struct	rlimit *rlp;
	} *uap = (struct a *)u.u_ap;

	if (uap->which >= RLIM_NLIMITS) {
		u.u_error = EINVAL;
		return;
	}
	u.u_error = copyout((caddr_t)&u.u_rlimit[uap->which], (caddr_t)uap->rlp,
	    sizeof (struct rlimit));
}

getrusage()
{
	register struct a {
		int	who;
		struct	rusage *rusage;
	} *uap = (struct a *)u.u_ap;
	register struct rusage *rup;
	time_value_t		sys_time, user_time;
	register struct timeval	*tvp;

	switch (uap->who) {

	case RUSAGE_SELF:
		/*
		 *	This is the current_thread.  Don't need to lock it.
		 */
		thread_read_times(current_thread(), &user_time, &sys_time);
		tvp = &u.u_ru.ru_utime;
		tvp->tv_sec = user_time.seconds;
		tvp->tv_usec = user_time.microseconds;
		tvp = &u.u_ru.ru_stime;
		tvp->tv_sec = sys_time.seconds;
		tvp->tv_usec = sys_time.microseconds;
		rup = &u.u_ru;
		break;

	case RUSAGE_CHILDREN:
		rup = &u.u_cru;
		break;

	default:
		u.u_error = EINVAL;
		return;
	}
	u.u_error = copyout((caddr_t)rup, (caddr_t)uap->rusage,
	    sizeof (struct rusage));
}

ruadd(ru, ru2)
	register struct rusage *ru, *ru2;
{
	register long *ip, *ip2;
	register int i;

	timevaladd(&ru->ru_utime, &ru2->ru_utime);
	timevaladd(&ru->ru_stime, &ru2->ru_stime);
	if (ru->ru_maxrss < ru2->ru_maxrss)
		ru->ru_maxrss = ru2->ru_maxrss;
	ip = &ru->ru_first; ip2 = &ru2->ru_first;
	for (i = &ru->ru_last - &ru->ru_first; i > 0; i--)
		*ip++ += *ip2++;
}
