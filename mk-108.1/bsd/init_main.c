/* 
 *
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 *
 * 20-Apr-90  Doug Mitchell at NeXT
 *	Started up reaper_thread() before mounting root.
 *
 * 19-Mar-90  Gregg Kellogg (gk) at NeXT
 *	NeXT doesn't use schedcpu.
 *	Move kallocinit() to vm/vm_init.c
 *
 * 14-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Changes for new scheduler:
 *		Initialize scheduler
 *		use newproc() to start ux_handler.
 *		Remove process lock initialization.
 *		Remove obsolete service_timers() kickoff
 *		do callout_lock initialization in kern_synch.c/rqinit.
 *
 * 17-Jan-90  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: Minimal cleanup.  Removed ihinit.
 *
 * 07-Nov-88  Avadis Tevanian (avie) at NeXT
 *	Removed code to use transparent addresses since it only works
 *	when we have fully populated buffers (which is a bad assumption).
 *
 * 13-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Removed dependencies on proc table.
 *
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TIME_NEW is now standard.
 *
 * 21-Apr-88  David Black (dlb) at Carnegie-Mellon University
 *	Set kernel_only for kernel task.
 *
 * 10-Apr-88  John Seamons (jks) at NeXT
 *	NeXT: Improve TLB performance for buffers by using the
 *	transparently translated virtual address for single page sized bufs.
 *
 * 07-Apr-88  John Seamons (jks) at NeXT
 *	NeXT: reduced the allocation size of various zones to save space.
 *
 *  3-Apr-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Force the vm_map for the inode_ and device_ pager tasks to
 *	be the kernel map.
 *
 * 25-Jan-88  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Moved float_init() call to configure() in autoconf.c
 *
 * 21-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Neither task_create nor thread_create return the data port
 *	any longer.
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Removed code to shuffle initial processes for idle threads;
 *	MACH doesn't need to make extra processes for them.
 *	Delinted.
 *
 * 12-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added device_pager startup.  Moved setting of ipc_kernel and
 *	kernel_only flags here.
 *
 *  9-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Follow thread_terminate with thread_halt_self for new thread
 *	termination logic; extra reference no longer necessary.
 *
 *  9-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Grab extra reference to first thread before terminating it.
 *
 *  4-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Name changes for exc interface.  set ipc_kernel in first thread
 *	for paranoia purposes.
 *
 * 19-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminated MACH conditionals, purged history.
 *
 *  5-Nov-87  David Golub (dbg) at Carnegie-Mellon University
 *	start up network service thread.
 *
 *  9-Sep-87  Peter King (king) at NeXT
 *	SUN_VFS:  Add a call to vfs_init() in setup_main().
 *
 * 17-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS:  Support credentials record in u.
 *	      Convert to Sun quota system.
 *	      Add call to dnlc_init().  Remove nchinit() call.
 *	      Call swapconf() in binit().
 */
 
#import <mach_xp.h>
#import <quota.h>
#import <cpus.h>

#import <cputypes.h>
#import <mach_old_vm_copy.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)init_main.c	7.1 (Berkeley) 6/5/86
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/vfs.h>
#import <sys/proc.h>
#import <sys/vnode.h>
#import <sys/conf.h>
#import <sys/buf.h>
#import <sys/clist.h>
#import <sys/dk.h>
#import <ufs/quotas.h>
#import <sys/bootconf.h>
#import <machine/reg.h>
#import <machine/cpu.h>

#import <machine/spl.h>

#import <kern/thread.h>
#import <kern/task.h>
#import <sys/machine.h>
#import <kern/timer.h>
#import <sys/version.h>
#import <machine/pmap.h>
#import <vm/vm_param.h>
#import <vm/vm_page.h>
#import <vm/vm_map.h>
#import <vm/vm_kern.h>
#import <vm/vm_object.h>
#import <sys/boolean.h>
#import <kern/sched_prim.h>
#import <kern/ipc_globals.h>
#import <kern/zalloc.h>
#import <kern/ipc_globals.h>
#if	MACH_XP
#import <vm/vnode_pager.h>
#import <vm/device_pager.h>
#endif	MACH_XP

#import <sys/task_special_ports.h>
#import <sys/ux_exception.h>

extern void	ux_handler();

long	cp_time[CPUSTATES];
int	dk_ndrive = DK_NDRIVE;
int	dk_busy;
long	dk_time[DK_NDRIVE];
long	dk_seek[DK_NDRIVE];
long	dk_xfer[DK_NDRIVE];
long	dk_wds[DK_NDRIVE];
#ifdef	mips
long	dk_mspw[DK_NDRIVE];
#else	mips
float	dk_mspw[DK_NDRIVE];
#endif	mips
#if	NeXT
long	dk_bps[DK_NDRIVE];
#endif	NeXT

long	tk_nin;
long	tk_nout;

#if	NeXT
thread_t	pageoutThread;
#endif	NeXT

dev_t	rootdev;		/* device of the root */
dev_t	dumpdev;		/* device to take dumps on */
long	dumplo;			/* offset into dumpdev */
int	show_space;
long	hostid;
char	hostname[MAXHOSTNAMELEN];
int	hostnamelen;
char	domainname[MAXDOMNAMELEN];
int	domainnamelen;

struct	timeval boottime;
struct	timeval time;
struct	timezone tz;			/* XXX */
int	hz;
int	phz;				/* alternate clock's frequency */
int	tick;
int	lbolt;				/* awoken once a second */

vm_map_t	kernel_pageable_map;
vm_map_t	mb_map;

int	cmask = CMASK;
/*
 * Initialization code.
 * Called from cold start routine as
 * soon as a stack and segmentation
 * have been established.
 * Functions:
 *	clear and free user core
 *	turn on clock
 *	hand craft 0th process
 *	call all initialization routines
 *	fork - process 0 to schedule
 *	     - process 1 execute bootstrap
 *	     - process 2 to page out
 */

/*
 *	Sets the name for the given task.
 */
void task_name(s)
	char		*s;
{
	int		length = strlen(s);

	bcopy(s, u.u_comm,
		length >= sizeof(u.u_comm) ? sizeof(u.u_comm) :
			length + 1);
}

/* To allow these values to be patched, they're globals here */
#import <machine/vmparam.h>
struct rlimit vm_initial_limit_stack = { DFLSSIZ, MAXSSIZ };
struct rlimit vm_initial_limit_data = { DFLDSIZ, MAXDSIZ };
struct rlimit vm_initial_limit_core = { DFLCSIZ, MAXCSIZ };

extern thread_t first_thread;

main()
{
	register int i;
	register struct proc *p;
	extern struct ucred *rootcred;
	int s;
	port_t		dummy_port;
	thread_t	th;
	extern void	idle_thread(), init_task(), vm_pageout();
	extern void	reaper_thread(), swapping_thread();
	extern void	netisr_thread(), sched_thread();
#if	NCPUS > 1
	extern void	 action_thread();
#endif	NCPUS > 1
#if	MACH_XP
	thread_t	inode_th;
	thread_t	device_th;
#endif	MACH_XP
	thread_t	exc_th;
#if	NeXT
	task_t		exc_task;
	extern thread_t	softint_thread;
	extern void	softint_th(void);
#endif	NeXT
	extern thread_t	newproc();

	/*
	 * set up system process 0 (swapper)
	 */

#if	NeXT
	pqinit();
	p = kernel_proc;
	kernel_task->proc = kernel_proc;
	p->p_pid = 0;
	pidhash_enter(p);
#else	NeXT
	p = &proc[0];
#endif	NeXT
	p->task = kernel_task;
	/*
	 *	Now in thread context, switch to thread timer.
	 */
	s = splhigh();
	timer_switch(&current_thread()->system_timer);
	splx(s);

	p->p_stat = SRUN;
	p->p_flag |= SLOAD|SSYS;
	p->p_nice = NZERO;
	simple_lock_init(&p->siglock);
	p->sigwait = FALSE;
	p->exit_thread = THREAD_NULL;
	u.u_procp = p;
	/*
	 * Setup credentials
	 */
	u.u_cred = crget();
	u.u_cmask = cmask;
	u.u_lastfile = -1;
	for (i = 0; i < sizeof(u.u_rlimit)/sizeof(u.u_rlimit[0]); i++)
		u.u_rlimit[i].rlim_cur = u.u_rlimit[i].rlim_max = 
		    RLIM_INFINITY;
	u.u_rlimit[RLIMIT_STACK] = vm_initial_limit_stack;
	u.u_rlimit[RLIMIT_DATA] = vm_initial_limit_data;
	u.u_rlimit[RLIMIT_CORE] = vm_initial_limit_core;

	/*
	 *	Allocate a kernel submap for pageable memory
	 *	for temporary copying (table(), execve()).
	 */
	{
	    vm_offset_t	min, max;

	    kernel_pageable_map = kmem_suballoc(kernel_map,
						&min, &max,
						512*1024,
						TRUE);
#if	MACH_OLD_VM_COPY
#else	MACH_OLD_VM_COPY
	    kernel_pageable_map->wait_for_space = TRUE;
#endif	MACH_OLD_VM_COPY
	}

	mfs_init();
	u_cred_lock_init(&u.utask->uu_cred_lock);
	crhold(u.u_cred);
	rootcred = u.u_cred;
	for (i = 1; i < NGROUPS; i++)
		u.u_groups[i] = NOGROUP;

#if	QUOTA
	qtinit();
#endif	QUOTA
	timestamp_init();

	startrtclock();

	/*
	 * Initialize tables, protocols, and set up well-known inodes.
	 */
	mbinit();
	cinit();
#import <loop.h>
#if NLOOP > 0
	loattach();			/* XXX */
#endif
	/*
	 * Block reception of incoming packets
	 * until protocols have been initialized.
	 */
	s = splimp();
	ifinit();
	domaininit();
	splx(s);
#if	NeXT
	/* done above */
#else	NeXT
	pqinit();
#endif	NeXT
	bhinit();
	dnlc_init();
	/*
	 *	Create kernel idle cpu processes.  This must be done
 	 *	before a context switch can occur (and hence I/O can
	 *	happen in the binit() call).
	 */
	u.u_rdir = NULL;
	u.u_cdir = NULL;

	for (i = 0; i < NCPUS; i++) {
		if (machine_slot[i].is_cpu == FALSE)
			continue;
		(void) thread_create(kernel_task, &th);
		thread_bind(th, cpu_to_processor(i));
		thread_start(th, idle_thread, THREAD_SYSTEMMODE);
		(void) thread_resume(th);
	}

	binit();
#ifdef GPROF
	kmstartup();
#endif

/* kick off timeout driven events by calling first time */
	recompute_priorities();
#if	!NeXT
	schedcpu();
#endif	!NeXT

	/*
	 * Start up netisr thread now in case we are doing an nfs_mountroot.
	 */
	(void) kernel_thread(kernel_task, reaper_thread);
	(void) kernel_thread(kernel_task, netisr_thread);

	/*
	 * mount the root, gets rootdir
	 */
	u.u_error = 0;
	vfs_mountroot();
/*
 * This is now handled by the mach_swapon program:
	vnode_pager_default_init();
 */
	boottime = time;

	/*
	 *  Default to pausing process on these errors.
	 */
	u.u_rpause = (URPS_AGAIN|URPS_NOMEM|URPS_NFILE|URPS_NOSPC);

#if	NeXT
	/*
	 * NeXT has a thread for running things in a thread context
	 * via softint_sched().
	 */
	softint_thread = kernel_thread(kernel_task, (int(*)())softint_th);
#endif	NeXT
	file_init();

	/*
	 * make init process
	 */

	th = newproc(0);
	th->task->kernel_privilege = FALSE;	/* XXX cleaner way to do this?
							*/
#if	NeXT
	init_proc = pfind(1);			/* now that it is set */
#endif	NeXT
	/*
	 *	After calling start_init,
	 *	machine-dependent code must
	 *	set up stack as though a system
	 *	call trap occurred, then call
	 *	load_init_program.
	 */
#if	MACH_XP
	/*inode_pager*/ {
	thread_t	inode_th;
	vm_offset_t	min, max;

	simple_lock_init(&inode_pager_init_lock);

	vm_pager_default = PORT_NULL;
	inode_th = newproc(0);
	inode_th->task->map =
		kmem_suballoc(kernel_map, &min, &max,
				8 * PAGE_SIZE, FALSE);


	/*
	 *	Wait for default pager to become available
	 */

	simple_lock(&inode_pager_init_lock);
	while (vm_pager_default == PORT_NULL)  {
		thread_sleep((int) &vm_pager_default,
			&inode_pager_init_lock, FALSE);
		simple_lock(&inode_pager_init_lock);
	}
	simple_unlock(&inode_pager_init_lock);

	/*inode_pager*/ }

	/*device_pager*/ {
	thread_t	device_th;
	vm_offset_t	min, max;

	device_th = newproc(0);
	device_th->task->map =
		kmem_suballoc(kernel_map, &min, &max,
				8 * PAGE_SIZE, FALSE);

	device_th->ipc_kernel = TRUE;
	device_th->task->kernel_only = TRUE;
	thread_swappable(device_th, FALSE);
	thread_start(device_th, device_pager, THREAD_SYSTEMMODE);
	(void) thread_resume(device_th);
	/*device_pager*/ }
#endif	MACH_XP

	/*
	 *	Default exception server
	 */

	simple_lock_init(&ux_handler_init_lock);
	ux_exception_port = PORT_NULL;
#if	NeXT
	(void) task_create(kernel_task, FALSE, &exc_task);
	(void) kernel_thread(exc_task, ux_handler);
#else	NeXT
	exc_th = newproc(0);
	exc_th->task->kernel_vm_space = TRUE;
	thread_start(exc_th, ux_handler, THREAD_SYSTEMMODE);
	(void) thread_resume(exc_th);
#endif	NeXT
	simple_lock(&ux_handler_init_lock);
	if (ux_exception_port == PORT_NULL) 
		thread_sleep((int) &ux_exception_port,
			simple_lock_addr(ux_handler_init_lock), FALSE);
	else
		simple_unlock(&ux_handler_init_lock);
	(void) task_set_exception_port(th->task, ux_exception_port);
	port_reference(ux_exception_port);
	object_copyout(th->task, (kern_obj_t) ux_exception_port,
		       MSG_TYPE_PORT, &dummy_port);

	thread_start(th, init_task, THREAD_SYSTEMMODE);
	(void) thread_resume(th);

	/*
	 *	Kernel daemon threads that don't need their own tasks
	 */

#if	NeXT
	pageoutThread = kernel_thread(kernel_task, vm_pageout);
#else	NeXT
	(void) kernel_thread(kernel_task, vm_pageout);
#endif	NeXT
	(void) kernel_thread(kernel_task, swapping_thread);
#if	NCPUS > 1
	(void) kernel_thread(kernel_task, action_thread);
#endif	NCPUS > 1
	(void) kernel_thread(kernel_task, sched_thread);
	
	/*
	 * 	vol driver and notification server startup
	 */
#if	NeXT
	vol_start_thread();
	pnotify_start();
#endif	NeXT
	
	u.u_procp->p_flag |= SLOAD|SSYS;
	task_name("kernel idle");
	(void) thread_terminate(current_thread());
	thread_halt_self();
	/*NOTREACHED*/
}

void init_task()
{
	task_name("init");
	start_init();
}

/*
 * Initialize hash links for buffers.
 */
bhinit()
{
	register int i;
	register struct bufhd *bp;

	for (bp = bufhash, i = 0; i < BUFHSZ; i++, bp++)
		bp->b_forw = bp->b_back = (struct buf *)bp;
}

/*
 * Initialize the buffer I/O system by freeing
 * all buffers and setting all device buffer lists to empty.
 */
binit()
{
	register struct buf *bp, *dp;
	register int i;
	int base, residual;

	for (dp = bfreelist; dp < &bfreelist[BQUEUES]; dp++) {
		dp->b_forw = dp->b_back = dp->av_forw = dp->av_back = dp;
		dp->b_flags = B_HEAD;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		bp = &buf[i];
		bp->b_dev = NODEV;
		bp->b_bcount = 0;
		bp->b_un.b_addr = buffers + i * MAXBSIZE;
 		if (i < residual)
			bp->b_bufsize = (base + 1) * page_size;
		else
			bp->b_bufsize = base * page_size;
#if	NeXT
		bp->b_rtpri = RTPRI_NONE;
#endif	NeXT
		if (bp->b_bufsize) {
			binshash(bp, &bfreelist[BQ_AGE]);
		} else {
			binshash(bp, &bfreelist[BQ_EMPTY]);
		}
		bp->b_vp = NULL;
		bp->b_flags = B_BUSY|B_INVAL;
		brelse(bp);
	}
	/*
	 * Count swap devices, and adjust total swap space available.
	 * Some of this space will not be available until a vswapon()
	 * system is issued, usually when the system goes multi-user.
	 */
}


/*
 * Initialize clist by freeing all character blocks, then count
 * number of character devices. (Once-only routine)
 */
cinit()
{
	register int ccp;
	register struct cblock *cp;

	ccp = (int)cfree;
	ccp = (ccp+CROUND) & ~CROUND;
	for(cp=(struct cblock *)ccp; cp < &cfree[nclist-1]; cp++) {
		cp->c_next = cfreelist;
		cfreelist = cp;
		cfreecount += CBSIZE;
	}
}



