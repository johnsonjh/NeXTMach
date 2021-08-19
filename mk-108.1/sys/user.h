/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 * 27-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: ucred definition moved to ucred.h.
 *			 Addition of u_auid.
 *
 * 28-Apr-88  David Golub (dbg) at Carnegie-Mellon University
 *	Move u_rpause and u_rfs from thread to task U-area - they are
 *	both global process state.
 *
 * 26-Feb-88  David Kirschen (kirschen) at Encore Computer Corporation
 *      Add include of param.h for NGROUPS, etc.
 *
 * 19-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminated conditionals, purged history.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)user.h	7.1 (Berkeley) 6/4/86
 */

/* @(#)user.h	1.8 87/08/24 3.2/4.3NFSSRC */

#ifndef	_SYS_USER_H_
#define	_SYS_USER_H_

#import <sys/types.h>

#import <kern/lock.h>
#import <sys/param.h>
#import <machine/pcb.h>
#import <sys/time.h>
#import <sys/resource.h>
#import <sys/ucred.h>
#if	NeXT
#import <next/fptrace.h>
#endif	NeXT

/*
 * Per process structure containing data that
 * isn't needed in core when the process is swapped out.
 */
 
#define	MAXCOMLEN	16		/* <= MAXNAMLEN, >= sizeof(ac_comm) */
 
#if	(defined(KERNEL) || defined(SHOW_UTT))
/*
 *	Per-thread U area.
 *
 *	It is likely that this structure contains no fields that must be
 *	saved between system calls.
 */
struct uthread {
	int	*uu_ar0;		/* address of users saved R0 */

/* syscall parameters, results and catches */
	int	uu_arg[8];		/* arguments to current system call */
	int	*uu_ap;			/* pointer to arglist */
	label_t	uu_qsave;		/* for non-local gotos on interrupts */
	union {				/* syscall return values */
		struct	{
			int	R_val1;
			int	R_val2;
		} u_rv;
#define	r_val1	u_rv.R_val1
#define	r_val2	u_rv.R_val2
		off_t	r_off;
		time_t	r_time;
	} uu_r;
	char	uu_error;		/* return error code */
	char	uu_eosys;		/* special action on end of syscall */

/* CS_RPAUSE */
	struct fs *uu_rpsfs;		/* resource pause file system */
	char	uu_rpswhich;		/* resource pause operation selection */
/* CS_RPAUSE */

/* thread exception handling */
	int	uu_code;			/* ``code'' to trap */
	char uu_cursig;				/* p_cursig for exc. */
	int  uu_sig;				/* p_sig for exc. */
};

/*
 *	Per-task U area - global process state.
 */
struct utask {
#ifdef	NeXT
#else	NeXT
	struct	pcb uu_pcb;
#endif	NeXT
	struct	proc *uu_procp;		/* pointer to proc structure */
	int	*uu_ar0;		/* address of users saved R0 */
	char	uu_comm[MAXCOMLEN + 1];

/* 1.1 - processes and protection */
#if	ROMP_DUALCALL
	char	uu_calltype;		/* 0 - old calling sequence */
#endif	ROMP_DUALCALL
/* SUN_VFS */
	struct ucred *uu_cred;		/* user credentials (uid, gid, etc) */
#define	uu_uid	uu_cred->cr_uid
#define	uu_gid	uu_cred->cr_gid
#define	uu_groups uu_cred->cr_groups
#define	uu_ruid	uu_cred->cr_ruid
#define	uu_rgid	uu_cred->cr_rgid
/* SUN_VFS */

	lock_data_t	uu_cred_lock;	/* lock for credentials */
#define u_cred_lock()		lock_write(&u.utask->uu_cred_lock)
#define u_cred_unlock()		lock_write_done(&u.utask->uu_cred_lock)
#define u_cred_lock_init(lock)	lock_init(lock, TRUE);

/* 1.2 - memory management */
	time_t	uu_outime;		/* user time at last sample */

/* 1.3 - signal management */
	void	(*uu_signal[NSIG+1])();	/* disposition of signals */
	int	uu_sigmask[NSIG+1];	/* signals to be blocked */
#ifdef	multimax
	int	(*uu_sigcatch)();	/* used as a way not to do tramp. */
#endif	multimax
#ifdef	balance
	int	(*uu_sigtramp)();	/* signal trampoline code */
#endif	balance
	int	uu_sigonstack;		/* signals to take on sigstack */
	int	uu_sigintr;		/* signals that interrupt syscalls */
	int	uu_oldmask;		/* saved mask from before sigpause */
	struct	sigstack uu_sigstack;	/* sp & on stack state variable */
#define	uu_onstack	uu_sigstack.ss_onstack
#define	uu_sigsp	uu_sigstack.ss_sp

/* 1.4 - descriptor management */
#if	NeXT
	struct	file **uu_ofile;	/* file structures for open files */
	char	*uu_pofile;		/* per-process flags of open files */
	int	uu_lastfile;		/* high-water mark of uu_ofile */
	int	uu_ofile_cnt;		/* number of file structs allocated */
#else	NeXT
	struct	file *uu_ofile[NOFILE];	/* file structures for open files */
	char	uu_pofile[NOFILE];	/* per-process flags of open files */
	int	uu_lastfile;		/* high-water mark of uu_ofile */
#endif	NeXT
#define	UF_EXCLOSE 	0x1		/* auto-close on exec */
#define	UF_MAPPED 	0x2		/* mapped from device */
#define UF_FDLOCK	0x4		/* file desc locked (SysV style) */
	struct	vnode *uu_cdir;		/* current directory */
	struct	vnode *uu_rdir;		/* root directory of current process */
	struct	tty *uu_ttyp;		/* controlling tty pointer */
	dev_t	uu_ttyd;		/* controlling tty dev */
	short	uu_cmask;		/* mask for file creation */

/* 1.5 - timing and statistics */
	struct	rusage uu_ru;		/* stats for this proc */
	struct	rusage uu_cru;		/* sum of stats for reaped children */
	struct	itimerval uu_timer[3];
	int	uu_XXX[3];
	struct	timeval uu_start;
	short	uu_acflag;

	struct uuprof {			/* profile arguments */
		simple_lock_t pr_lock;	/* lock for thread updating */
		short	*pr_base;	/* buffer base */
		unsigned pr_size;	/* buffer size */
		unsigned pr_off;	/* pc offset */
		unsigned pr_scale;	/* pc scaling */
	} uu_prof;
/* CS_RPAUSE */
	u_char	uu_rpause;		/* resource pause flags: */
#define	URPS_AGAIN	01		/* - no child processes available */
#define	URPS_NOMEM	02		/* - no memory available */
#define	URPS_NFILE	04		/* - file table overflow */
#define	URPS_NOSPC	010		/* - no space on device */
/* CS_RPAUSE */

/* 1.6 - resource controls */
	struct	rlimit uu_rlimit[RLIM_NLIMITS];

	int	uu_stack[1];
#if	defined(NeXT) && defined(DEBUG)
	struct fptrace_data uu_fptrace;	/* fpemul trace data */
#endif	defined(NeXT) && defined(DEBUG)
};

#endif	defined(KERNEL)
struct	user {
	struct	pcb u_pcb;
	struct	proc *u_procp;		/* pointer to proc structure */
	int	*u_ar0;			/* address of users saved R0 */
	char	u_comm[MAXCOMLEN + 1];

/* syscall parameters, results and catches */
	int	u_arg[8];		/* arguments to current system call */
	int	*u_ap;			/* pointer to arglist */
	label_t	u_qsave;		/* for non-local gotos on interrupts */
	union {				/* syscall return values */
		struct	{
			int	R_val1;
			int	R_val2;
		} u_rv;
#define	r_val1	u_rv.R_val1
#define	r_val2	u_rv.R_val2
		off_t	r_off;
		time_t	r_time;
	} u_r;
	char	u_error;		/* return error code */
	char	u_eosys;		/* special action on end of syscall */

/* 1.1 - processes and protection */
#if	ROMP_DUALCALL
	char	u_calltype;		/* 0 == old calling sequence */
#endif	ROMP_DUALCALL
/* SUN_VFS */
	struct ucred *u_cred;		/* user credentials (uid, gid, etc) */
#define	u_uid		u_cred->cr_uid
#define	u_gid		u_cred->cr_gid
#define	u_groups 	u_cred->cr_groups
#define	u_ruid		u_cred->cr_ruid
#define	u_rgid		u_cred->cr_rgid
#define	u_auid		u_cred->cr_auid
/* SUN_VFS */

/* 1.2 - memory management */
	size_t	u_tsize;		/* text size (clicks) */
	size_t	u_dsize;		/* data size (clicks) */
	size_t	u_ssize;		/* stack size (clicks) */
	caddr_t	u_text_start;		/* text starting address */
	caddr_t	u_data_start;		/* data starting address */
	time_t	u_outime;		/* user time at last sample */

/* 1.3 - signal management */
	void	(*u_signal[NSIG+1])();	/* disposition of signals */
	int	u_sigmask[NSIG+1];	/* signals to be blocked */
#ifdef	multimax
	int	(*u_sigcatch)();	/* used as a way not to do tramp. */
#endif	multimax
#ifdef	balance
	int	(*u_sigtramp)();	/* signal trampoline code */
#endif	balance
	int	u_sigonstack;		/* signals to take on sigstack */
	int	u_sigintr;		/* signals that interrupt syscalls */
	int	u_oldmask;		/* saved mask from before sigpause */
	int	u_code;			/* ``code'' to trap */
	struct	sigstack u_sigstack;	/* sp & on stack state variable */
#define	u_onstack	u_sigstack.ss_onstack
#define	u_sigsp		u_sigstack.ss_sp

/* 1.4 - descriptor management */
	struct	file *u_ofile[NOFILE];	/* file structures for open files */
	char	u_pofile[NOFILE];	/* per-process flags of open files */
	int	u_lastfile;		/* high-water mark of u_ofile */
#define	UF_EXCLOSE 	0x1		/* auto-close on exec */
#define	UF_MAPPED 	0x2		/* mapped from device */
#define UF_FDLOCK	0x4		/* file desc locked (SysV style) */
	struct	vnode *u_cdir;		/* current directory */
	struct	vnode *u_rdir;		/* root directory of current process */
	struct	tty *u_ttyp;		/* controlling tty pointer */
	dev_t	u_ttyd;			/* controlling tty dev */
	short	u_cmask;		/* mask for file creation */

/* 1.5 - timing and statistics */
	struct	rusage u_ru;		/* stats for this proc */
	struct	rusage u_cru;		/* sum of stats for reaped children */
	struct	itimerval u_timer[3];
	int	u_XXX[3];
	struct	timeval u_start;
	short	u_acflag;

	struct uprof {			/* profile arguments */
		simple_lock_t pr_lock;	/* lock for thread updating */
		short	*pr_base;	/* buffer base */
		unsigned pr_size;	/* buffer size */
		unsigned pr_off;	/* pc offset */
		unsigned pr_scale;	/* pc scaling */
	} u_prof;
/* CS_RPAUSE */
	struct fs *u_rpsfs;		/* resource pause file system */
	char	u_rpswhich;		/* resource pause operation selection */
#define URPW_FNOSPC	0x01		/* - low on fragments */
#define URPW_INOSPC	0x02		/* - low on inodes */
#define URPW_QNOSPC	0x04		/* - out of quota */
#define URPW_POLL	0x40		/* - poll until available */
#define URPW_NOTIFY	0x80		/* - pause in progress */
	u_char	u_rpause;		/* resource pause flags: */
#define	URPS_AGAIN	01		/* - no child processes available */
#define	URPS_NOMEM	02		/* - no memory available */
#define	URPS_NFILE	04		/* - file table overflow */
#define	URPS_NOSPC	010		/* - no space on device */
/* CS_RPAUSE */

/* 1.6 - resource controls */
	struct	rlimit u_rlimit[RLIM_NLIMITS];

	int	u_stack[1];
};

/* u_eosys values */
#define	JUSTRETURN	1
#define	RESTARTSYS	2
#define NORMALRETURN	3

/* u_error codes */
#import <sys/errno.h>

#ifdef KERNEL
#import <kern/thread.h>
#import <machine/user.h>

#ifndef	u
#define u	(current_thread()->u_address)
#endif	u

#define u_pcb		uthread->uu_pcb
#define u_procp		utask->uu_procp
#define u_ar0		uthread->uu_ar0
#define u_comm		utask->uu_comm

#define u_arg		uthread->uu_arg
#define u_ap		uthread->uu_ap
#define u_qsave		uthread->uu_qsave
#define u_r		uthread->uu_r
#define u_error		uthread->uu_error
#define u_eosys		uthread->uu_eosys

/* SUN_VFS */
#undef	u_cred
#undef	u_uid
#undef	u_gid
#undef	u_groups
#undef	u_ruid
#undef	u_rgid
#define	u_cred		utask->uu_cred
#define	u_uid		utask->uu_cred->cr_uid
#define	u_gid		utask->uu_cred->cr_gid
#define	u_groups 	utask->uu_cred->cr_groups
#define	u_ruid		utask->uu_cred->cr_ruid
#define	u_rgid		utask->uu_cred->cr_rgid
/* SUN_VFS */

#define u_tsize		utask->uu_tsize
#define u_dsize		utask->uu_dsize
#define u_ssize		utask->uu_ssize
#define	u_text_start	utask->uu_text_start
#define	u_data_start	utask->uu_data_start
#define u_outime	utask->uu_outime

#define u_signal	utask->uu_signal
#ifdef	multimax
#define	u_sigcatch	utask->uu_sigcatch
#endif	multimax
#ifdef	balance
#define	u_sigtramp	utask->uu_sigtramp
#endif	balance
#define u_sigmask	utask->uu_sigmask
#define u_sigonstack	utask->uu_sigonstack
#define u_sigintr	utask->uu_sigintr
#define u_oldmask	utask->uu_oldmask
#define u_code		uthread->uu_code
#define u_sigstack	utask->uu_sigstack

#define	u_onstack	u_sigstack.ss_onstack
#define	u_sigsp		u_sigstack.ss_sp

#define u_ofile		utask->uu_ofile
#define u_pofile	utask->uu_pofile
#define u_lastfile	utask->uu_lastfile
#if	NeXT
#define u_ofile_cnt	utask->uu_ofile_cnt
#define u_fptrace	utask->uu_fptrace
#endif	NeXT
#define u_cdir		utask->uu_cdir
#define u_rdir		utask->uu_rdir
#define u_ttyp		utask->uu_ttyp
#define u_ttyd		utask->uu_ttyd
#define u_cmask		utask->uu_cmask

#define u_ru		utask->uu_ru
#define u_cru		utask->uu_cru
#define u_timer		utask->uu_timer
#define u_XXX		utask->uu_XXX
#define u_start		utask->uu_start
#define u_acflag	utask->uu_acflag

#define u_prof		utask->uu_prof
/* CS_RPAUSE */
#define u_rpsfs		uthread->uu_rpsfs
#define u_rpswhich	uthread->uu_rpswhich
#define u_rpause	utask->uu_rpause
/* CS_RPAUSE */

#define u_rlimit	utask->uu_rlimit

#define u_sig		uthread->uu_sig
#define u_cursig	uthread->uu_cursig
#endif
#endif	_SYS_USER_H_



