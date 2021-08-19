/*
 *	File:	next/shutdown.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1989, NeXT, Inc.
 *
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/vnode.h>
#import <sys/proc.h>
#import <sys/file.h>

/*
 *	Shutdown down proc system (release references to current and root
 *	dirs for each process).
 */

proc_shutdown()
{
	struct proc	*p, *self;
	struct vnode	**cdirp, **rdirp, *vp;
	int		restart, i;
	struct utask	*utask;

	/*
	 *	Kill as many procs as we can.  (Except ourself...)
	 */
	self = current_task()->proc;
	
	/*
	 * Suspend /etc/init
	 */
	p = pfind(1);
	if (p && p != self)
		task_suspend(p->task);		/* stop init */

	
	/*
	 * Suspend mach_init
	 */
	p = pfind(2);
	if (p && p != self)
		task_suspend(p->task);		/* stop mach_init */

	printf("Killing all processes ");
	/* SIGTERM and us_delay to allow process to clean-up */
	for (p = allproc; p; p = p->p_nxt) {
		if ((p->p_ppid != 0) && ((p->p_flag&SSYS) == 0) && (p != self))
			psignal(p, SIGTERM);
	}
	us_delay(4000000);
	/* SIGKILL in case the TERM wasn't heard */
	for (p = allproc; p; p = p->p_nxt) {
		if ((p->p_ppid != 0) && ((p->p_flag&SSYS) == 0) && (p != self))
			psignal(p, SIGKILL);
	}
	us_delay(1000000);
	/* Brute force 'em, if necessary */
	p = allproc;
	while (p) {
		if ((p->p_ppid == 0) || (p->p_flag&SSYS) || (p == self)) {
			p = p->p_nxt;
		}
		else {
			/*
			 * NOTE: following code ignores sig_lock and plays
			 * with exit_thread correctly.  This is OK unless we
			 * are a multiprocessor, in which case I do not
			 * understand the sig_lock.  This needs to be fixed.
			 * XXX
			 */
			if (p->exit_thread) {	/* someone already doing it */
				thread_block();	/* give him a chance */
			}
			else {
				p->exit_thread = current_thread();
				printf(".");
				do_exit(p, 1);
			}
			p = allproc;
		}
	}
	printf("\n");
	/*
	 *	Forcibly free resources of what's left.
	 */
	p = allproc;
	while (p) {
		utask = p->task->u_address;
		restart = 0;
		for (i = 0; i <= utask->uu_lastfile; i++) {
			struct file *f;

			f = utask->uu_ofile[i];
			if (f) {
#if	SUN_LOCK
				/* Release all System-V style record locks */
				(void) vno_lockrelease(f);
#endif	SUN_LOCK
				utask->uu_ofile[i] = NULL;
				closef(f);
				restart = 1;
			}
			utask->uu_pofile[i] = 0;
		}
		cdirp = &utask->uu_cdir;
		vp = *cdirp;
		if (vp) {
			*cdirp = 0;
			VN_RELE(vp);
			restart = 1;
		}
		rdirp = &utask->uu_rdir;
		vp = *rdirp;
		if (vp) {
			*rdirp = 0;
			VN_RELE(vp);
			restart = 1;
		}
		if (restart)
			p = allproc;
		else
			p = p->p_nxt;
	}
}

/*
 *	Close all file descriptors, called at shutdown time.
 */
fd_shutdown()
{
	register struct file *fp;

	for (fp = (struct file *) queue_first(&file_list);
    	    !queue_end(&file_list, (queue_entry_t) fp);
  	    fp = (struct file *) queue_next(&fp->links)) {
		while (fp->f_count > 0)
			closef(fp);
	}
}

