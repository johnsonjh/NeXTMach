/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 27-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: define arguments to wait4() system call.
 *
 * 06-Jan-88  Jay Kistler (jjk) at Carnegie Mellon University
 *	Made file reentrant.  Added declarations for __STDC__.
 *
 * 14-Dec-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Merged romp and 68000 definitions since they were the same.
 *	The real conditional should be dependent on byte ordering.
 *
 * 30-May-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Added 68000 dependent definitions of w_t and w_S very similar to
 *	those for the ROMP.
 *
 * 19-Feb-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Added alternate definitions of w_T and w_S for Sailboat under
 *	switch ROMP.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)wait.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_WAIT_
#define	_WAIT_	1

/*
 * This file holds definitions relevent to the wait system call.
 * Some of the options here are available only through the ``wait3''
 * entry point; the old entry point with one argument has more fixed
 * semantics, never returning status of unstopped children, hanging until
 * a process terminates if any are outstanding, and never returns
 * detailed information about process resource utilization (<vtimes.h>).
 */

/*
 * Structure of the information in the first word returned by both
 * wait and wait3.  If w_stopval==WSTOPPED, then the second structure
 * describes the information returned, else the first.  See WUNTRACED below.
 */
union wait	{
	int	w_status;		/* used in syscall */
	/*
	 * Terminated process status.
	 */
	struct {
#if	defined(romp) || defined(mc68000)
		unsigned short  w_PAD16;
		unsigned        w_Retcode:8;    /* exit code if w_termsig==0 */
 		unsigned        w_Coredump:1;   /* core dump indicator */
 		unsigned        w_Termsig:7;    /* termination signal */
#else	romp || mc68000
		unsigned short	w_Termsig:7;	/* termination signal */
		unsigned short	w_Coredump:1;	/* core dump indicator */
		unsigned short	w_Retcode:8;	/* exit code if w_termsig==0 */
#endif	romp || mc68000
	} w_T;
	/*
	 * Stopped process status.  Returned
	 * only for traced children unless requested
	 * with the WUNTRACED option bit.
	 */
	struct {
#if	defined(romp) || defined(mc68000)
		unsigned short  w_PAD16;
 		unsigned        w_Stopsig:8;    /* signal that stopped us */
 		unsigned        w_Stopval:8;    /* == W_STOPPED if stopped */
#else	romp || mc68000
		unsigned short	w_Stopval:8;	/* == W_STOPPED if stopped */
		unsigned short	w_Stopsig:8;	/* signal that stopped us */
#endif	romp || mc68000
	} w_S;
};
#define	w_termsig	w_T.w_Termsig
#define w_coredump	w_T.w_Coredump
#define w_retcode	w_T.w_Retcode
#define w_stopval	w_S.w_Stopval
#define w_stopsig	w_S.w_Stopsig


#define	WSTOPPED	0177	/* value of s.stopval if process is stopped */

/*
 * Option bits for the second argument of wait3.  WNOHANG causes the
 * wait to not hang if there are no stopped or terminated processes, rather
 * returning an error indication in this case (pid==0).  WUNTRACED
 * indicates that the caller should receive status about untraced children
 * which stop due to signals.  If children are stopped and a wait without
 * this option is done, it is as though they were still running... nothing
 * about them is returned.
 */
#define WNOHANG		1	/* dont hang in wait */
#define WUNTRACED	2	/* tell about stopped, untraced children */

#define WIFSTOPPED(x)	((x).w_stopval == WSTOPPED)
#define WIFSIGNALED(x)	((x).w_stopval != WSTOPPED && (x).w_termsig != 0)
#define WIFEXITED(x)	((x).w_stopval != WSTOPPED && (x).w_termsig == 0)

/* SUN_VFS */
#ifdef	KERNEL
/* 
 * Arguments to wait4() system call, included here so it may be called by
 * other routines in the kernel 
 */
struct wait4_args {
	int pid;
	union wait *status;
	int options;
	struct rusage *rusage;
};
#endif KERNEL
/* end SUN_VFS */

#if	defined(__STDC__) && !defined(KERNEL)
#import <sys/resource.h>
extern int wait(union wait *);
extern int wait3(union wait *, int, struct rusage *);
#endif	defined(__STDC__) && !defined(KERNEL)
#endif	_WAIT_
