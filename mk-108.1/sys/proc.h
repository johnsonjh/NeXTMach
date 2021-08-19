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
 * $Log:	proc.h,v $
 * Revision 2.10  89/10/11  14:54:00  dlb
 * 	Pass thread to thread_should_halt().
 * 	[88/10/18            dlb]
 * 
 * Revision 2.9  89/03/15  15:59:42  gm0w
 * 	Added pointer to utask to proc structure so that programs
 * 	out-side of the kernel can find it without using the task
 * 	structure.
 * 	[89/03/14            gm0w]
 * 
 * Revision 2.8  89/03/09  22:06:32  rpd
 * 	More cleanup.
 * 
 * Revision 2.7  89/02/27  21:04:05  mrt
 * 	Made p_rmt_seg field in struct proc unconditional.
 * 	[89/02/27            mrt]
 * 
 * Revision 2.6  89/02/25  17:55:23  gm0w
 * 	Got rid of MACH and CMUCS conditionals and all non-MACH code.
 * 	Made CMUCS conditional code true. Make VICE conditionals
 * 	unconditionally false for X75 binary compatibilty, should
 * 	be changed to true.
 * 	[89/02/13            mrt]
 * 
 * Revision 2.5  89/01/30  22:08:17  rpd
 * 	Updated macro definitions to the new style.
 * 	Made variable declarations use "extern".
 * 	[89/01/25  15:22:29  rpd]
 * 
 * Revision 2.4  88/08/24  02:39:19  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:20:09  mwyoung]
 *
 *  4-May-88  David Black (dlb) at Carnegie-Mellon University
 *	Document use of p_stat for MACH.
 *
 * 29-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH: Removed unused variables (whichqs).
 *
 * 11-Apr-88  Mike Accetta (mja) at Carnegie-Mellon University
 *	Move controlling terminal information to proc structure from
 *	U-area (to provide better handle on disconnecting background
 *	processes from a terminal);  CS_SECURITY => CMUCS.
 *	[ V5.1(XF23) ]
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 * 26-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Removed MACH_NOFLOAT.
 *
 * 21-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Check for thread termination condition and return properly
 *	in more places in sig_lock.
 *
 *  9-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Changed forced_exit case of sig_lock to call thread_halt_self
 *	for new thread termination logic.
 *
 *  3-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Added second argument to task_dowait.
 *
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminated MACH conditionals.
 *
 * 28-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	MACH_TT: restore definition of SWEXIT to keep ps happy.
 *
 * 16-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TT: Incorporate exit_thread logic in sig_lock() macro.
 *		This replaces and extends core_thread.
 *
 * 25-Sep-87  David Black (dlb) at Carnegie-Mellon University
 *	MACH: added core_thread field to deal with network core dumps.
 *
 * 18-Sep-87  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Deleted definition of SOWEFPA, as this condition is no longer
 *	associated with processes, but rather with threads.
 *
 *  4-Sep-87  David Black (dlb) at Carnegie-Mellon University
 *	Added sig lock for signals and exit.  This frees proc lock for
 *	other uses.
 *
 * 15-May-87  David Black (dlb) at Carnegie-Mellon University
 *	MACH: Added p_stopsig field to record signal that stopped
 *	process.  Can't use p_cursig for this purpose under MACH.
 *
 * 30-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added a lock to the proc structure to synchronize Unix things in
 *	a multiple thread environement.  This is not conditional on
 *	MACH (but on MACH) so that the same version of ps and friends
 *	will work on both kernels.
 *
 * 06-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Changed to use shorts instead of ints for these values that
 *	are really signed chars anyway since its more space efficient
 *	and consistent with the prior fix to p_nice.
 *	[ V5.1(F5) ]
 *
 * 05-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Make usrpri, pri and nice ints for ROMP due to compiler
 *	difference (this doesn't matter under MACH, but till we run
 *	that everywhere...)
 *
 * 04-Mar-87  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Added pointer to proc structure of tracer for Sun.
 *
 * 02-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Fixed to make the p_nice field a short on the IBM-RT since its
 *	current compiler doesn't provide signed char types and this is
 *	wreaking havoc with high priority processes never getting any
 *	cycles!  This fix is only temporary until a better compiler
 *	becomes the standard.
 *	[ V5.1(F4) ]
 *
 *  7-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Merge VICE changes -- include vice.h and change to #if VICE.
 *
 * 31-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminate p_wchan for MACH as a check for elimination of all
 *	uses of it.
 *
 * 08-Jan-87  Robert Beck (beck) at Sequent Computer Systems, Inc.
 *	If MACH, declare p_pctcpu as a long and define PCTCPU_SCALE.
 *
 * 31-Dec-86  David Golub (dbg) at Carnegie-Mellon University
 *	Purged all MACH uses of p0br and friends.  Removed fields
 *	that refer to text structure, and removed segment size fields
 *	(p_tsize, p_dsize, p_ssize) that are unused under MACH.
 *	ROMP_FPA should be the next to go (it belongs with the thread info).
 *
 *  2-Dec-86  Jay Kistler (jjk) at Carnegie-Mellon University
 *	VICE: 1/ added p_rmt_seq field to "proc" struct;
 *	      2/ added SRMT flag;
 *
 * 31-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Removed include of task/thread header files by using "struct"
 *	instead of typedef.
 *
 * 15-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Well, as it turns out, the Multimax code actually does want the
 *	Vax versions of p0br and friends for compatibility.  Presumably
 *	these will all go away someday anyway.
 *
 * 14-Oct-86  William Bolosky (bolosky) at Carnegie-Mellon University
 *	Changed #ifdef romp #else romp {vax code here} #endif romp to
 *	the (correct) #ifdef vax {vax code here} #endif vax.  This
 *	should NOT be changed back again; we now have more machines than
 *	just the vax and the RT, and I don't think that the Sun and the
 *	Encore want definitions of p_p0br.  It's time to fix the scripts.
 *
 * 30-Sep-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added backpointers from proc to task and thread.
 *
 * 24-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added include of <sys/types.h> to pick up uid_t, etc.
 *
 *  6-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added include of <sys/time.h> for non-KERNEL compiles.
 *
 * 20-Jul-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added include of "time.h" to satisfy "struct itimerval"
 *	reference.
 *
 *  7-Jul-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	romp: removed p_sid0 and p_sid1 (since this info is stored in
 *	the pmap and is no longer used).  Conditionalized
 *	SPTECHG on vax and added SOWEFPA in same bit on romp w/FPA.
 *	Conditionalized p0br and p1br on vax.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 18-Feb-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Added definition of p_sid0 and p_sid1 in proc structure for
 *	IBM-RT under switch romp.
 *
 *  3-Sep-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	MACH:  Added SACTIVE flag to signify that a process is actually
 *	running on a cpu.
 *
 * 25-Aug-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Allow recursive includes.
 *
 * 25-May-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	Upgraded from 4.1BSD.  Carried over changes below.
 *	[V1(1)]
 *
 * 20-Aug-81  Mike Accetta (mja) at Carnegie-Mellon University
 *	CMUCS:  added SXONLY bit definition to flag execute only
 *	processes;
 *
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)proc.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_PROC_H_
#define _SYS_PROC_H_

#import <sys/boolean.h>
#import <sys/time.h>
#import <sys/types.h>
#import <sys/user.h>

#ifdef	KERNEL
#import <kern/lock.h>
#else	KERNEL
#ifndef	_KERN_LOCK_H_
#define _KERN_LOCK_H_
typedef int simple_lock_data_t;
#endif	_KERN_LOCK_H_
#endif	KERNEL

/*
 * One structure allocated per active
 * process. It contains all data needed
 * about the process while the
 * process may be swapped out.
 * Other per process data (user.h)
 * is swapped with the process.
 */
struct	proc {
	struct	proc *p_link;	/* linked list of running processes */
	struct	proc *p_rlink;
	struct	proc *p_nxt;	/* linked list of allocated proc slots */
	struct	proc **p_prev;		/* also zombies, and free proc's */
#ifdef	ibmrt
	short	p_usrpri;	/* user-priority based on p_cpu and p_nice */
	short	p_pri;		/* priority, negative is high */
	short	p_cpu;		/* cpu usage for scheduling */
#else	ibmrt
	char	p_usrpri;	/* user-priority based on p_cpu and p_nice */
	char	p_pri;		/* priority, negative is high */
	char	p_cpu;		/* cpu usage for scheduling */
#endif	ibmrt
	char	p_stat;
	char	p_time;		/* resident time for scheduling */
#ifdef	ibmrt
	short	p_nice;		/* nice for cpu usage */
#else	ibmrt
	char	p_nice;		/* nice for cpu usage */
#endif	ibmrt
#if	NeXT
	char	pad;		/* keep fields aligned */
#else	NeXT
	char	p_slptime;	/* time since last block */
#endif	NeXT
	char	p_cursig;
	int	p_sig;		/* signals pending to this process */
	int	p_sigmask;	/* current signal mask */
	int	p_sigignore;	/* signals being ignored */
	int	p_sigcatch;	/* signals being caught by user */
	int	p_flag;
	uid_t	p_uid;		/* user id, used to direct tty signals */
	short	p_pgrp;		/* name of process group leader */
	short	p_pid;		/* unique process id */
	short	p_ppid;		/* process id of parent */
	u_short	p_xstat;	/* Exit status for wait */
	struct	rusage *p_ru;	/* mbuf holding exit information */
#if	NeXT
	/* These fields are not used. */
#else	NeXT
	size_t 	p_rssize; 	/* current resident set size in clicks */
	size_t	p_maxrss;	/* copy of u.u_limit[MAXRSS] */
	size_t	p_swrss;	/* resident set size before last swap */
	swblk_t	p_swaddr;	/* disk address of u area when swapped */
#endif	NeXT
	int	p_stopsig;	/* signal that stopped us. */
#if	NeXT
	/* Other elements unused */
	struct	proc	*p_hash;	/* hash table link */
#else	NeXT
	int	p_stopsig;	/* signal that stopped us. */
	short	p_cpticks;	/* ticks of cpu time */
	long	p_pctcpu;	/* %cpu for this process during p_time */
	short	p_ndx;		/* proc index for memall (because of vfork) */
	short	p_idhash;	/* hashed based on p_pid for kill+exit+... */
#endif	NeXT
	struct	proc *p_pptr;	/* pointer to process structure of parent */
	struct	proc *p_cptr;	/* pointer to youngest living child */
	struct	proc *p_osptr;	/* pointer to older sibling processes */
	struct	proc *p_ysptr;	/* pointer to younger siblings */
	struct	itimerval p_realtimer;
	struct	quota *p_quota;	/* quotas for this process */
#if	NeXT
	/* These fields are not used. */
#else	NeXT
	dev_t	    p_logdev;	/* logged-in controlling device */
	dev_t       p_ttyd;	/* controlling tty dev */
	struct tty *p_ttyp;	/* controlling tty pointer */
#endif	NeXT
	struct task	*task;	/* corresponding task */
#if	NeXT
	/* utask was only initialized (in kern_fork.c), don't need it. */
#else	NeXT
	struct utask	*utask; /* utask structure of corresponding task */
#endif	NeXT
	struct thread	*thread;/* corresponding thread */
#if	NeXT
	/* not used. */
#else	NeXT
	int	p_rmt_seq;	/* This process is waiting for a remote file 
				   system reply message containing this
				   sequence number - VICE only */

#endif	NeXT
	simple_lock_data_t siglock;	/* multiple thread signal lock */
	boolean_t	sigwait;	/* indication to suspend */
	struct thread	*exit_thread;	/* XXX Which thread is exiting?
					   XXX That thread does no signal
					   XXX processing, other threads
					   XXX must suspend. */
#if	NeXT
	struct	proc *p_tptr;	/* pointer to process structure of tracer */
	struct  proc *p_aptr;	/* pointer to process attached to debugger */
#endif	NeXT
#ifdef	sun
	struct	proc *p_tptr;	/* pointer to process structure of tracer */
#endif	sun
};

#ifdef	KERNEL
#import <kern/macro_help.h>

/*
 *	Signal lock has the following states and corresponding actions
 *	that the locker must take:
 *
 *	Locked (siglock) - simple lock acquires the lock when free.
 *	Unlocked (sigwait = 0 && exit_thread == 0)  simple lock.
 *	Waiting (sigwait != 0) - Drop siglock after acquiring it, and
 *		call thread_block().  Thread that set the lock to
 *		wait has done a task_suspend().
 *	Exiting (exit_thread != 0) - The thread in exit_thread is going to
 *		call exit().  If we're not that thread, permanently stop
 *		in favor of that thread.  If we're that thread, immediately
 *		bail out (no signal processing is permitted once we're
 *		committed to exit) and indicate that signals should not be
 *		processed.  If we have been asked to halt, bail out and
 *		indicate that signals should be processed (to clean up any
 *		saved state).
 *
 *	The logic for this is in the sig_lock_or_return macro.
 */

/*
 *	Try to grab signal lock.  If we are already exiting,
 *	execute 'false_return'.  If some other thread is exiting,
 *	hold.  If we must halt, execute 'true_return'.
 */
#define sig_lock_or_return(p, false_return, true_return)	\
MACRO_BEGIN							\
	simple_lock(&(p)->siglock);				\
	while ((p)->sigwait || (p)->exit_thread) {		\
	    simple_unlock(&(p)->siglock);			\
	    if ((p)->exit_thread) {				\
		if (current_thread() == (p)->exit_thread) {	\
		    /*						\
		     *	Already exiting - no signals.		\
		     */						\
		    false_return;				\
		}						\
		else {						\
		    /*						\
		     *	Another thread has called exit -	\
		     *	stop (until terminate request).		\
		     */						\
		    thread_hold(current_thread());		\
		}						\
	    }							\
	    thread_block();					\
	    if (thread_should_halt(current_thread())) {		\
		/*						\
		 *	Terminate request - clean up.		\
		 */						\
		true_return;					\
	    }							\
	    simple_lock(&(p)->siglock);				\
	}							\
MACRO_END

/*
 *	Try to grab signal lock.  Return from caller if
 *	we must halt or task is exiting.
 */
#define sig_lock(p)		sig_lock_or_return(p, return, return)

#define sig_lock_simple(p)	simple_lock(&(p)->siglock)

#define sig_unlock(p)		simple_unlock(&(p)->siglock)

#define sig_lock_to_wait(p)			\
MACRO_BEGIN					\
	(p)->sigwait = TRUE; 			\
	simple_unlock(&(p)->siglock);		\
MACRO_END

#define sig_wait_to_lock(p)			\
MACRO_BEGIN					\
	simple_lock(&(p)->siglock); 		\
	(p)->sigwait = FALSE;			\
MACRO_END

/*
 *	sig_lock_to_exit() also shuts down all other threads except the
 *	current one.  There is no sig_exit_to_lock().  The sig_lock is
 *	left in exit state and is cleaned up by exit().
 */

#define sig_lock_to_exit(p)				\
MACRO_BEGIN						\
	(p)->exit_thread = current_thread();		\
	simple_unlock(&(p)->siglock);			\
	(void) task_hold(current_task());		\
	(void) task_dowait(current_task(), FALSE);	\
MACRO_END
#endif	KERNEL


#define PIDHSZ		64
#define PIDHASH(pid)	((pid) & (PIDHSZ - 1))

#ifdef	KERNEL
#if	NeXT
extern struct	proc *pidhash[PIDHSZ];
extern struct	proc *pfind();
extern struct	proc *freeproc, *zombproc, *allproc;
			/* lists of procs in various states */
extern int	max_proc;		/* Max number of procs */
extern struct	proc *kernel_proc, *init_proc;
struct proc	*getproc();
#else	NeXT
extern short	pidhash[PIDHSZ];
extern struct	proc *pfind();
extern struct	proc *proc, *procNPROC;	/* the proc table itself */
extern struct	proc *freeproc, *zombproc, *allproc;
			/* lists of procs in various states */
extern int	nproc;
#endif	NeXT

#define NQS	32		/* 32 run queues */
extern struct	prochd {
	struct	proc *ph_link;	/* linked list of running processes */
	struct	proc *ph_rlink;
} qs[NQS];

#define PCTCPU_SCALE	1000	/* scaling for p_pctcpu */
#endif	KERNEL

/* stat codes */
/*
 *	MACH uses only NULL, SRUN, SZOMB, and SSTOP.
 */
#define SSLEEP	1		/* awaiting an event */
#define SWAIT	2		/* (abandoned state) */
#define SRUN	3		/* running */
#define SIDL	4		/* intermediate state in process creation */
#define SZOMB	5		/* intermediate state in process termination */
#define SSTOP	6		/* process being traced */

/* flag codes */
#define SLOAD	0x00000001	/* in core */
#define SSYS	0x00000002	/* swapper or pager process */
#define SLOCK	0x00000004	/* process being swapped out */
#define SSWAP	0x00000008	/* save area flag */
#define STRC	0x00000010	/* process is being traced */
#define SWTED	0x00000020	/* another tracing flag */
#define SULOCK	0x00000040	/* user settable lock in core */
#define SPAGE	0x00000080	/* process in page wait state */
#define SKEEP	0x00000100	/* another flag to prevent swap out */
#define SOMASK	0x00000200	/* restore old mask after taking signal */
#define SWEXIT	0x00000400	/* working on exiting */
#define SPHYSIO	0x00000800	/* doing physical i/o (bio.c) */
#define SVFORK	0x00001000	/* process resulted from vfork() */
#define SVFDONE	0x00002000	/* another vfork flag */
#define SNOVM	0x00004000	/* no vm, parent in a vfork() */
#define SPAGI	0x00008000	/* init data space on demand, from inode */
#define SSEQL	0x00010000	/* user warned of sequential vm behavior */
#define SUANOM	0x00020000	/* user warned of random vm behavior */
#define STIMO	0x00040000	/* timing out during sleep */
/* was SDETACH */
#define SACTIVE	0x00080000	/* process is executing */
#define SOUSIG	0x00100000	/* using old signal mechanism */
#define SOWEUPC	0x00200000	/* owe process an addupc() call at next ast */
#define SSEL	0x00400000	/* selecting; wakeup/waiting danger */
#define SLOGIN	0x00800000	/* a login process (legit child of init) */

#define SXONLY	0x02000000	/* process image read protected	*/
#define SIDLE	0x04000000	/* is an idle process */
#define SRMT	0x10000000	/* VICE remote file system access--don't stop job */
#if	NeXT
#define SLKDONE	0x20000000	/* record-locking has been done */
#endif	NeXT

#endif	_SYS_PROC_H_




