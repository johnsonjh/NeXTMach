/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 21-May-89  Avadis Tevanian, Jr. (avie) at NeXT, Inc.
 *	Purged all crud that had accumulated in this file.  Machine specific
 *	definitions now go in <machine/signal.h> (including sigcontext).
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)signal.h	7.1 (Berkeley) 6/4/86
 */

#import <machine/signal.h>

#ifndef	NSIG
#define NSIG	32

#define	SIGHUP	1	/* hangup */
#define	SIGINT	2	/* interrupt */
#define	SIGQUIT	3	/* quit */
#define	SIGILL	4	/* illegal instruction (not reset when caught) */
/* CHME, CHMS, CHMU are not yet given back to users reasonably */
#define	SIGTRAP	5	/* trace trap (not reset when caught) */
#define	SIGIOT	6	/* IOT instruction */
#define	SIGABRT	SIGIOT	/* compatibility */
#define	SIGEMT	7	/* EMT instruction */
#define	SIGFPE	8	/* floating point exception */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	SIGBUS	10	/* bus error */
#define	SIGSEGV	11	/* segmentation violation */
#define	SIGSYS	12	/* bad argument to system call */
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGALRM	14	/* alarm clock */
#define	SIGTERM	15	/* software termination signal from kill */
#define	SIGURG	16	/* urgent condition on IO channel */
#define	SIGSTOP	17	/* sendable stop signal not from tty */
#define	SIGTSTP	18	/* stop signal from tty */
#define	SIGCONT	19	/* continue a stopped process */
#define	SIGCHLD	20	/* to parent on child stop or exit */
#define	SIGCLD	SIGCHLD	/* compatibility */
#define	SIGTTIN	21	/* to readers pgrp upon background tty read */
#define	SIGTTOU	22	/* like TTIN for output if (tp->t_local&LTOSTOP) */
#define	SIGIO	23	/* input/output possible signal */
#define	SIGXCPU	24	/* exceeded CPU time limit */
#define	SIGXFSZ	25	/* exceeded file size limit */
#define	SIGVTALRM 26	/* virtual time alarm */
#define	SIGPROF	27	/* profiling time alarm */
#define SIGWINCH 28	/* window size changes */
/* SUN_LOCK*/
#define	SIGLOST 29	/* resource lost (eg, record-lock lost) */
/* SUN_LOCK*/
#define SIGUSR1 30	/* user defined signal 1 */
#define SIGUSR2 31	/* user defined signal 2 */
/* used by MACH_IPC kernels */
#define SIGEMSG 30	/* process received an emergency message */
#define	SIGMSG	31	/* process received normal message */

#if	!defined(__STDC__) && !defined(KERNEL)
int	(*signal())();
#endif	!defined(__STDC__) && !defined(KERNEL)

/*
 * Signal vector "template" used in sigvec call.
 */
#ifndef	ASSEMBLER
struct	sigvec {
	void	(*sv_handler)();	/* signal handler */
	int	sv_mask;		/* signal mask to apply */
	int	sv_flags;		/* see signal options below */
};
#endif	ASSEMBLER
#define SV_ONSTACK	0x0001	/* take signal on signal stack */
#define SV_INTERRUPT	0x0002	/* do not restart system on signal return */
#define sv_onstack sv_flags	/* isn't compatibility wonderful! */

/*
 * Structure used in sigstack call.
 */
#ifndef	ASSEMBLER
struct	sigstack {
	char	*ss_sp;			/* signal stack pointer */
	int	ss_onstack;		/* current status */
};
#endif	ASSEMBLER

#ifdef __STRICT_BSD__
#define	BADSIG		(int (*)())-1
#define	SIG_DFL		(int (*)())0
#define	SIG_IGN		(int (*)())1

#ifdef KERNEL
#define	SIG_CATCH	(int (*)())2
#define	SIG_HOLD	(int (*)())3
#endif

#else /* __STRICT_BSD__ */

#define	BADSIG		(void (*)())-1
#define	SIG_DFL		(void (*)())0
#define	SIG_IGN		(void (*)())1

#ifdef KERNEL
#define	SIG_CATCH	(void (*)())2
#define	SIG_HOLD	(void (*)())3
#endif

#endif /* __STRICT_BSD__ */

#if	defined(__STDC__) && !defined(KERNEL)
/* man(2) declarations */
extern int sigblock(int);
extern int sigpause(int);
extern int sigreturn(struct sigcontext *);
extern int sigsetmask(int);
extern int sigstack(struct sigstack *, struct sigstack *);
extern int sigvec(int, struct sigvec *, struct sigvec *);
/* man(3) declarations */
extern int siginterrupt(int, int);
#ifdef __STRICT_BSD__
extern int (*signal(int, int (*)()))();
#else
extern void (*signal(int, void (*)(int)))(int);
#endif
#endif	defined(__STDC__) && !defined(KERNEL)
#endif

/*
 * Macro for converting signal number to a mask suitable for
 * sigblock().
 */
#define sigmask(m)	(1 << ((m)-1))

#if	KERNEL
/*
 *	signals delivered on a per-thread basis.
 */
#define threadmask	(sigmask(SIGILL)|sigmask(SIGTRAP)|sigmask(SIGIOT)| \
			sigmask(SIGEMT)|sigmask(SIGFPE)|sigmask(SIGBUS)| \
			sigmask(SIGSEGV)|sigmask(SIGSYS)|sigmask(SIGPIPE))
#endif	KERNEL
