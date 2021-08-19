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
 * $Log:	syscall_subr.c,v $
 * 31-May-90  Gregg Kellogg (gk) at NeXT
 *	Fixed thread_switch to return an error if the passed thread
 *	doesn't exist (as per spec.)
 *
 * 18-May-90  Avadis Tevanian (avie) at NeXT
 *	Changed to use sensible priorities (higher numbers -> higher pri).
 *
 *  3-May-90  Gregg Kellogg (gk) at NeXT
 *	NeXT:	Change panic to assertion in thread_depress_timeout.
 *
 * 24-Apr-90  Gregg Kellogg (gk) at NeXT
 *	NeXT: map_fd can map zero length files.
 *
 * 17-Feb-90  Gregg Kellogg (gk) at NeXT
 *	NeXT:	map_fd uses vnodes, not inodes.
 *	Removed MACH_?FS and MACH_EMULATION code.
 *
 * Revision 2.17  89/12/22  15:53:23  rpd
 * 	Only check counts on a multiprocessor; thread_block does these
 * 	checks anyway, but grabs a lock to do them.
 * 	[89/12/01            dlb]
 * 	Check runq counts in swtch() and thread_switch() before calling
 * 	thread_block().
 * 	[89/11/20            dlb]
 * 	Set first_quantum to true when switching to a fixed-priority
 * 	thread.
 * 	[89/11/15            dlb]
 * 	Encase fixed priority support code in MACH_FIXPRI switch.
 * 	[89/11/10            dlb]
 * 
 * Revision 2.16  89/11/20  11:24:10  mja
 * 	Set first_quantum to true when switching to a fixed-priority
 * 	thread.
 * 	[89/11/15            dlb]
 * 	Encase fixed priority support code in MACH_FIXPRI switch.
 * 	[89/11/10            dlb]
 * 
 * Revision 2.15  89/10/11  14:27:10  dlb
 * 	Cope with new timeout_scaling factor units.
 * 	Always restore depressed priority when thread runs again.
 * 	       Insightful suggestion courtesy of mwyoung.
 * 	Set up first_quantum flag and priority for new thread when
 * 	       switching to a fixed priority thread in thread_switch.
 * 	Add thread_switch().
 * 	Change priorities to 0-31 from 0-127.
 * 	Added check for zero length mapping request to map_fd.
 * 
 * Revision 2.14  89/08/02  08:03:41  jsb
 * 	Cleaned up and corrected inode type checks in map_fd, allowing
 * 	map_fd to work on afs and nfs files.
 * 	[89/07/17            jsb]
 * 
 * Revision 2.13  89/05/21  22:27:30  mrt
 * 	Put in Glenn's fix in map_fd to have MACH_VFS case call
 * 	getvnodefp instead of getinode.
 * 	[89/05/21            mrt]
 * 
 * Revision 2.12  89/05/01  17:01:39  rpd
 * 	Added mach_sctimes_port_alloc_dealloc (under MACH_SCTIMES).
 * 	[89/05/01  13:59:45  rpd]
 * 
 * Revision 2.11  89/04/22  15:24:59  gm0w
 * 	Changed to use new inode macros for manipulation of inodes.
 * 	Removed old MACH ctimes() system call.
 * 	[89/04/14            gm0w]
 * 
 * Revision 2.10  89/02/25  18:08:54  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.9  89/01/18  00:50:31  jsb
 * 	Vnode support, including changing EREMOTE to ERFSREMOTE.
 * 	[89/01/17  10:19:30  jsb]
 * 
 * Revision 2.8  88/12/19  02:47:17  mwyoung
 * 	Removed lint.
 * 	[88/12/17            mwyoung]
 * 
 * Revision 2.7  88/10/27  10:47:42  rpd
 * 	Added a bunch of syscalls, conditional under MACH_SCTIMES.
 * 	[88/10/26  14:43:08  rpd]
 * 
 * Revision 2.6  88/08/30  00:05:41  mwyoung
 * 	Turned off handling of CS system calls in htg_unix_syscall.  The
 * 	old code was conditionalized on CS_SYSCALL, but the option file
 * 	was never included, so the code never got compiled.  Well, until
 * 	the CS_SYSCALL was converted to CMUCS... then it wouldn't compile.
 * 	[88/08/28            mwyoung]
 * 
 * Revision 2.5  88/08/25  18:18:36  mwyoung
 * 	Corrected include file references.
 * 	[88/08/22            mwyoung]
 * 	
 * 	Reimplement map_fd using vm_map.
 * 	[88/08/20  03:11:12  mwyoung]
 * 
 * Revision 2.4  88/08/03  15:35:37  dorr
 * Make htg_unix_syscall use return value to indicate
 * failure/success, return special error for restartable
 * syscalls, and return errno as the first return value when
 * applicable.
 * 
 * 10-May-88  Douglas Orr (dorr) at Carnegie-Mellon University
 *	Make htg_unix_syscall use return value to indicate
 *	failure/success, return special error for restartable
 *	syscalls, and return errno as the first return value when
 *	applicable.
 *
 * 26-Apr-88  Alessandro Forin (af) at Carnegie-Mellon University
 *	Fixed syscall_setjmp -> setjmp in htg_unix_syscall.
 *
 * 01-Mar-88  Douglas Orr (dorr) at Carnegie-Mellon University
 *	Added htg_unix_syscall
 *
 * 18-Dec-87  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Added an extra pair of parens to make map_fd work correctly.
 *
 *  6-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Changed calling sequence to inode_pager_setup().
 *	Removed old history.
 *
 * 19-Nov-87  Michael Jones (mbj) at Carnegie-Mellon University
 *	Make sure fd passed to map_fd references a local inode before
 *	calling getinode, since getinode will panic for map_fd of RFS files.
 *
 * 11-Aug-87  Peter King (king) at NeXT
 *	MACH_VFS: changes to map_fd and ctimes.
 *
 * 19-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TT: boolean for swtch and swtch_pri is now whether there is
 *	other work that the kernel could run instead of this thread.
 *
 *  7-May-87  David Black (dlb) at Carnegie-Mellon University
 *	New versions of swtch and swtch_pri for MACH_TT.  Both return a
 *	boolean indicating whether a context switch was done.  Documented.
 *
 * 31-Jul-86  Rick Rashid (rfr) at Carnegie-Mellon University
 *	Changed TPswtch_pri to set p_pri to 127 to make sure looping
 *	processes which want to simply reschedule do not monopolize the
 *	cpu.
 *
 *  3-Jul-86  Fil Alleva (faa) at Carnegie-Mellon University
 *	Added TPswtch_pri().  [Added to Mach, 20-jul-86, mwyoung.]
 *
 */

#import <mach_fixpri.h>
#import <mach_sctimes.h>
#import <stat_time.h>
#import <cpus.h>

#import <kern/thread.h>
#import <sys/thread_switch.h>

#import <sys/user.h>
#import <sys/proc.h>

#if	NeXT
#import <vm/vnode_pager.h>
#import <kern/mfs.h>
#import <kern/xpr.h>
#else	NeXT
#import <vm/inode_pager.h>
#endif	NeXT
#import <sys/boolean.h>
#import <kern/ipc_pobj.h>
#import <kern/kern_port.h>
#import <kern/sched.h>
#import <kern/sched_prim_macros.h>
#import <kern/thread.h>
#import <machine/cpu.h>
#import <kern/processor.h>
#if	MACH_SCTIMES
#import <sys/port.h>
#import <vm/vm_kern.h>
#endif	MACH_SCTIMES

#if	MACH_FIXPRI
#import <sys/policy.h>
#endif	MACH_FIXPRI

/*
 *	Note that we've usurped the name "swtch" from its
 *	previous use.
 */

/*
 *	swtch and swtch_pri both attempt to context switch (logic in
 *	thread_block no-ops the context switch if nothing would happen).
 *	A boolean is returned that indicates whether there is anything
 *	else runnable.
 *
 *	This boolean can be used by a thread waiting on a
 *	lock or condition:  If FALSE is returned, the thread is justified
 *	in becoming a resource hog by continuing to spin because there's
 *	nothing else useful that the processor could do.  If TRUE is
 *	returned, the thread should make one more check on the
 *	lock and then be a good citizen and really suspend.
 */

void thread_depress_priority();
kern_return_t thread_depress_abort();

boolean_t swtch()
{
	register processor_t	myprocessor;

#if	NCPUS > 1
	myprocessor = current_processor();
	if (myprocessor->runq.count == 0 &&
	    myprocessor->processor_set->runq.count == 0)
		return(FALSE);
#endif	NCPUS > 1

	thread_block();

	myprocessor = current_processor();
	return(myprocessor->runq.count > 0 ||
	    myprocessor->processor_set->runq.count > 0);
}

boolean_t  swtch_pri(pri)
	int pri;
{
	register boolean_t	result;
	register thread_t	thread = current_thread();
#ifdef	lint
	pri++;
#endif	lint

	/*
	 *	XXX need to think about depression duration.
	 *	XXX currently using min quantum.
	 */
	thread_depress_priority(thread, min_quantum);
	result = swtch();
	if (thread->depress_priority >= 0) {
#if	NeXT
		XPR(XPR_SCHED, ("swtch_pri: depress_priority %d, abort\n",
			thread->depress_priority));
#endif	NeXT
		thread_depress_abort(thread);
	}
	return(result);
}

extern unsigned int timeout_scaling_factor;

/*
 *	thread_switch:
 *
 *	Context switch.  User may supply thread hint.
 *
 *	Fixed priority threads that call this get what they asked for
 *	even if that violates priority order.
 */
kern_return_t thread_switch(thread_name, option, option_time)
int	thread_name, option, option_time;
{
    register thread_t		cur_thread = current_thread();
    register processor_t	myprocessor;
    kern_port_t			port;

    /*
     *	Process option.
     */
    switch (option) {
	case SWITCH_OPTION_NONE:
	    /*
	     *	Nothing to do.
	     */
	    break;

	case SWITCH_OPTION_DEPRESS:
	    /*
	     *	Depress priority for given time.
	     */
	    thread_depress_priority(cur_thread, option_time);
	    break;

	case SWITCH_OPTION_WAIT:
	    thread_will_wait_with_timeout(cur_thread,
		(1000*option_time)/timeout_scaling_factor);
	    break;

	default:
	    return(KERN_INVALID_ARGUMENT);
    }
    
    /*
     *	Check and act on thread hint if appropriate.
     */
#if	NeXT
    if (thread_name != 0) {
	register thread_t thread;
	register int s;
	/*
	 * Return an error if the passed port doesn't represent a thread.
	 */
	if (!port_translate(cur_thread->task, thread_name, &port))
		return(KERN_INVALID_ARGUMENT);
	if (port_object_type(port) != PORT_OBJECT_THREAD) {
		port_unlock(port);
		return(KERN_INVALID_ARGUMENT);
	}
	thread = (thread_t) port_object_get(port);
	/*
	 *	Check if the thread is in the right pset. Then
	 *	pull it off its run queue.  If it
	 *	doesn't come, then it's not eligible.
	 */
	s = splsched();
	thread_lock(thread);
	if (   (thread->processor_set == cur_thread->processor_set)
	    && (rem_runq(thread) != RUN_QUEUE_NULL))
	{
		/*
		 *	Hah, got it!!
		 */
		thread_unlock(thread);
		(void) splx(s);
		port_unlock(port);
#if	MACH_FIXPRI
		if (thread->policy == POLICY_FIXEDPRI) {
		    myprocessor = current_processor();
		    myprocessor->quantum = thread->sched_data;
		    myprocessor->first_quantum = TRUE;
		}
#endif	MACH_FIXPRI
		thread_run(thread);
		/*
		 *  Restore depressed priority			 
		 */
		if (cur_thread->depress_priority >= 0) {
			XPR(XPR_SCHED, ("thread_switch: "
				"depress_priority %d, abort\n",
				thread->depress_priority));
			thread_depress_abort(cur_thread);
		}

		return(KERN_SUCCESS);
	}
	thread_unlock(thread);
	(void) splx(s);
	port_unlock(port);
    }
#else	NeXT
    if (thread_name != 0 &&
	port_translate(cur_thread->task, thread_name, &port)) {
	    /*
	     *	Get corresponding thread.
	     */
	    if (port_object_type(port) == PORT_OBJECT_THREAD) {
		register thread_t thread;
		register int s;

		thread = (thread_t) port_object_get(port);
		/*
		 *	Check if the thread is in the right pset. Then
		 *	pull it off its run queue.  If it
		 *	doesn't come, then it's not eligible.
		 */
		s = splsched();
		thread_lock(thread);
		if ((thread->processor_set == cur_thread->processor_set)
		    && (rem_runq(thread) != RUN_QUEUE_NULL)) {
			/*
			 *	Hah, got it!!
			 */
			thread_unlock(thread);
			(void) splx(s);
			port_unlock(port);
#if	MACH_FIXPRI
			if (thread->policy == POLICY_FIXEDPRI) {
			    myprocessor = current_processor();
			    myprocessor->quantum = thread->sched_data;
			    myprocessor->first_quantum = TRUE;
			}
#endif	MACH_FIXPRI
			thread_run(thread);
			/*
			 *  Restore depressed priority			 
			 */
			if (cur_thread->depress_priority >= 0) {
				thread_depress_abort(cur_thread);
			}

			return(KERN_SUCCESS);
		}
		thread_unlock(thread);
		(void) splx(s);
	    }
	    port_unlock(port);
    }
#endif	NeXT

    /*
     *	No handoff hint supplied, or hint was wrong.  Call thread_block() in
     *	hopes of running something else.  If nothing else is runnable,
     *	thread_block will detect this.  WARNING: thread_switch with no
     *	option will not do anything useful if the thread calling it is the
     *	highest priority thread (can easily happen with a collection
     *	of timesharing threads).
     */
#if	NCPUS > 1
    myprocessor = current_processor();
    if (myprocessor->processor_set->runq.count > 0 ||
	myprocessor->runq.count > 0)
#endif	NCPUS > 1
	    thread_block();

    /*
     *  Restore depressed priority			 
     */
    if (cur_thread->depress_priority >= 0) {
#if	NeXT
	XPR(XPR_SCHED, ("thread_switch: depress_priority %d, abort\n",
		cur_thread->depress_priority));
#endif	NeXT
	thread_depress_abort(cur_thread);
    }
    return(KERN_SUCCESS);
}

/*
 *	thread_depress_priority
 *
 *	Depress thread's priority to lowest possible for specified period.
 *	Intended for use when thread wants a lock but doesn't know which
 *	other thread is holding it.  As with thread_switch, fixed
 *	priority threads get exactly what they asked for.  Users access
 *	this by the SWITCH_OPTION_DEPRESS option to thread_switch.
 */
void
thread_depress_priority(thread, depress_time)
register thread_t thread;
int	depress_time;
{
    int		s;
    void	thread_depress_timeout();

    depress_time = (1000*depress_time)/timeout_scaling_factor;

    s = splsched();
    thread_lock(thread);

    /*
     *	If thread is already depressed, override previous depression.
     */
    if (thread->depress_priority >= 0) {
#if NCPUS > 1
	if (untimeout_try(thread_depress_timeout, thread) == FALSE) {
	    /*
	     *	Handle multiprocessor race condition.  Some other processor
	     *	is trying to timeout the old depress.  This should be rare.
	     */
	    thread_unlock(thread);
	    (void) splx(s);

	    /*
	     *	Wait for the timeout to do its thing.
	     */
	    while (thread->depress_priority >= 0)
	       continue;

	    /*
	     * Relock the thread and depress its priority.
	     */
	    s = splsched();
	    thread_lock(thread);

	    thread->depress_priority = thread->priority;
#if	NeXT
	    thread->priority = 0;
	    thread->sched_pri = 0;
#else	NeXT
	    thread->priority = 31;
	    thread->sched_pri = 31;
#endif	NeXT
	}
#else	NCPUS > 1
#if	NeXT
	XPR(XPR_SCHED, ("thread_depress_priority: depress_priority %d, "
		"retimeout\n",
		thread->depress_priority));
#endif	NeXT
	untimeout(thread_depress_timeout, thread);
#endif	NCPUS > 1
    }
    else {
	/*
	 *	Save current priority, then set priority and
	 *	sched_pri to their lowest possible values.
	 */
	thread->depress_priority = thread->priority;
#if	NeXT
	thread->priority = 0;
        thread->sched_pri = 0;
#else	NeXT
	thread->priority = 31;
        thread->sched_pri = 31;
#endif	NeXT
#if	NeXT
    XPR(XPR_SCHED, ("thread_depress_priority: set depress_priority to %d, "
	    "timeout\n",
	    thread->depress_priority));
#endif	NeXT
    }
    timeout(thread_depress_timeout, (caddr_t)thread, depress_time);

    thread_unlock(thread);
    (void) splx(s);

}	
    
/*
 *	thread_depress_timeout:
 *
 *	Timeout routine for priority depression.
 */
void
thread_depress_timeout(thread)
register thread_t thread;
{
    int s;

    s = splsched();
    thread_lock(thread);
    /*
     *	Make sure thread is depressed, then undepress it.
     */
#if	NeXT
    XPR(XPR_SCHED, ("thread_depress_timeout: depress_priority %d\n",
    	thread->depress_priority));
#endif	NeXT
    if (thread->depress_priority >= 0) {
	thread->priority = thread->depress_priority;
	thread->depress_priority = -1;
	compute_priority(thread);
    }
    else {
#if	NeXT
	printf("thread_depress_timeout: thread not depressed!");
	assert(thread->depress_priority >= 0);
#else	NeXT
	panic("thread_depress_timeout: thread not depressed!");
#endif	NeXT
    }
    thread_unlock(thread);
    (void) splx(s);
}

/*
 *	thread_depress_abort:
 *
 *	Prematurely abort priority depression if there is one.
 */
kern_return_t
thread_depress_abort(thread)
register thread_t	thread;
{
    kern_return_t	ret = KERN_SUCCESS;
    int	s;

    if (thread == THREAD_NULL) {
	return(KERN_INVALID_ARGUMENT);
    }

    s = splsched();
    thread_lock(thread);

    /*
     *	Only restore priority if thread is depressed and we can
     *	grab the depress timeout off of the callout queue.
     */
    if (thread->depress_priority >= 0) {
#if	NeXT
	XPR(XPR_SCHED, ("thread_depress_abort: depress_priority %d, "
	    "untimeout\n",
	    thread->depress_priority));
#endif	NeXT
#if	NCPUS > 1
	if (untimeout_try(thread_depress_timeout, thread)) {
#else	NCPUS > 1
	untimeout(thread_depress_timeout, thread);
#endif	NCPUS > 1
	    thread->priority = thread->depress_priority;
	    thread->depress_priority = -1;
	    compute_priority(thread);
#if	NCPUS > 1
	}
	else {
	    ret = KERN_FAILURE;
	}
#endif	NCPUS > 1
    }

    thread_unlock(thread);
    (void) splx(s);
    return(ret);
}

/* Many of these may be unnecessary */
#import <sys/systm.h>
#import <sys/mount.h>
#if	!NeXT
#import <sys/fs.h>
#endif	!NeXT
#import <sys/buf.h>
#if	NeXT
#import <sys/vnode.h>
#else	NeXT
#import <sys/inode.h>
#endif	NeXT
#import <sys/dir.h>
#import <sys/quota.h>
#import <sys/kernel.h>
#import <sys/file.h>

#import <sys/kern_return.h>
#import <kern/task.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <vm/memory_object.h>
#import <vm/vnode_pager.h>

map_fd(fd, offset, va, findspace, size)
	int		fd;
	vm_offset_t	offset;
	vm_offset_t	*va;
	boolean_t	findspace;
	vm_size_t	size;
{
	register struct file	*fp;
	register struct vnode *vp;
	vm_map_t	user_map;
	kern_return_t	result;
	vm_offset_t	user_addr;
	vm_size_t	user_size;
	vm_pager_t	pager;
	vm_map_t	copy_map;
	vm_offset_t	off;

	user_map = current_task()->map;

	/*
	 *	Find the inode; verify that it's a regular file.
	 */

	fp = getf(fd);
	if (fp == NULL)
		return(KERN_INVALID_ARGUMENT);
	vp = (struct vnode *)fp->f_data;
	if (fp->f_type != DTYPE_VNODE)
		return(KERN_INVALID_ARGUMENT);

	if (vp->v_type != VREG)
		return(KERN_INVALID_ARGUMENT);

	user_size = round_page(size);

	if (findspace) {
		/*
		 *	Allocate dummy memory.
		 */
		result = vm_allocate(user_map, &user_addr, size, TRUE);
		if (result != KERN_SUCCESS)
			return(result);
		if (copyout(&user_addr, va, sizeof(vm_offset_t))) {
			(void) vm_deallocate(user_map, user_addr, size);
			return(KERN_INVALID_ADDRESS);
		}
	}
	else {
		/*
		 *	Get user's address, and verify that it's
		 *	page-aligned and writable.
		 */

		if (copyin(va, &user_addr, sizeof(vm_offset_t)))
			return(KERN_INVALID_ADDRESS);
		if ((trunc_page(user_addr) != user_addr))
			return(KERN_INVALID_ARGUMENT);
		if (!vm_map_check_protection(user_map, user_addr,
				(vm_offset_t)(user_addr + user_size),
				VM_PROT_READ|VM_PROT_WRITE))
			return(KERN_INVALID_ARGUMENT);
	}

#if	NeXT
	/*
	 * Allow user to map in a zero length file.
	 */
	if (size == 0)
		return KERN_SUCCESS;
#endif	NeXT

	/*
	 *	Map in the file.
	 */

	pager = vnode_pager_setup(vp, FALSE, FALSE);

	/*
	 *	Map into private map, then copy into our address space.
	 */
	copy_map = vm_map_create(pmap_create(user_size), 0, user_size, TRUE);
	off = 0;
	result = vm_allocate_with_pager(copy_map, &off, user_size, FALSE,
					pager,
					offset);
	if (result == KERN_SUCCESS)
		result = vm_map_copy(user_map, copy_map, user_addr, user_size,
					0, FALSE, FALSE);

	if ((result != KERN_SUCCESS) && findspace)
		(void) vm_deallocate(user_map, user_addr, user_size);

	vm_map_deallocate(copy_map);

	/*
	 * Set credentials.
	 */
	if (vp->vm_info->cred == NULL) {
		crhold(u.u_cred);
		vp->vm_info->cred = u.u_cred;
	}

	return(result);
}

#if	MACH_SCTIMES
kern_return_t
mach_sctimes_0()
{
	return KERN_SUCCESS;
}

kern_return_t
mach_sctimes_1(arg0)
{
#ifdef	lint
	arg0++;
#endif	lint
	return KERN_SUCCESS;
}

kern_return_t
mach_sctimes_2(arg0, arg1)
{
#ifdef	lint
	arg0++; arg1++;
#endif	lint
	return KERN_SUCCESS;
}

kern_return_t
mach_sctimes_3(arg0, arg1, arg2)
{
#ifdef	lint
	arg0++; arg1++; arg2++;
#endif	lint
	return KERN_SUCCESS;
}

kern_return_t
mach_sctimes_4(arg0, arg1, arg2, arg3)
{
#ifdef	lint
	arg0++; arg1++; arg2++; arg3++;
#endif	lint
	return KERN_SUCCESS;
}

kern_return_t
mach_sctimes_5(arg0, arg1, arg2, arg3, arg4)
{
#ifdef	lint
	arg0++; arg1++; arg2++; arg3++; arg4++;
#endif	lint
	return KERN_SUCCESS;
}

kern_return_t
mach_sctimes_6(arg0, arg1, arg2, arg3, arg4, arg5)
{
#ifdef	lint
	arg0++; arg1++; arg2++; arg3++; arg4++; arg5++;
#endif	lint
	return KERN_SUCCESS;
}

kern_return_t
mach_sctimes_7()
{
	return KERN_SUCCESS;
}

kern_return_t
mach_sctimes_8(arg0, arg1, arg2, arg3, arg4, arg5)
{
#ifdef	lint
	arg0++; arg1++; arg2++; arg3++; arg4++; arg5++;
#endif	lint
	return KERN_SUCCESS;
}

vm_offset_t mach_sctimes_buffer = 0;
vm_size_t mach_sctimes_bufsize = 0;

kern_return_t
mach_sctimes_9(size)
	vm_size_t size;
{
	register kern_return_t kr;

	if (mach_sctimes_bufsize != 0)
		kmem_free(kernel_map, mach_sctimes_buffer,
			  mach_sctimes_bufsize);

	if (size == 0) {
		mach_sctimes_bufsize = 0;
		kr = KERN_SUCCESS;
	} else {
		mach_sctimes_buffer = kmem_alloc(kernel_map, size);
		if (mach_sctimes_buffer == 0) {
			mach_sctimes_bufsize = 0;
			kr = KERN_FAILURE;
		} else {
			mach_sctimes_bufsize = size;
			kr = KERN_SUCCESS;
		}
	}

	return kr;
}

kern_return_t
mach_sctimes_10(addr, size)
	char *addr;
	vm_size_t size;
{
	register kern_return_t kr;

	if (size > mach_sctimes_bufsize)
		kr = KERN_FAILURE;
	else if (copyin(addr, mach_sctimes_buffer, size))
		kr = KERN_FAILURE;
	else
		kr = KERN_SUCCESS;

	return kr;
}

kern_return_t
mach_sctimes_11(addr, size)
	char *addr;
	vm_size_t size;
{
	register kern_return_t kr;

	if (size > mach_sctimes_bufsize)
		kr = KERN_FAILURE;
	else if (copyout(mach_sctimes_buffer, addr, size))
		kr = KERN_FAILURE;
	else
		kr = KERN_SUCCESS;

	return kr;
}

kern_return_t
mach_sctimes_port_alloc_dealloc(times)
	int times;
{
	task_t self = current_task();
	int i;

	for (i = 0; i < times; i++) {
		port_name_t port;

		(void) port_allocate(self, &port);
		(void) port_deallocate(self, port);
	}

	return KERN_SUCCESS;
}
#endif	MACH_SCTIMES
