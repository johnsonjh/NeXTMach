/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 * Revision 2.4  89/01/30  22:02:51  rpd
 * 	Added declarations of pidhash, freeproc, zombproc, allproc, qs,
 *	and mpid. (The declarations in .h files are "extern" now.)
 * 	[89/01/25  14:51:34  rpd]
 * 
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: removed dir.h.
 *
 * 13-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Removed dependencies on proc table.
 *
 * 19-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Changed inode.h to vnode.h.  The question is, why is
 *		 this in here at all?
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_proc.c	7.1 (Berkeley) 6/5/86
 */

/*	@(#)kern_mman.c	2.2 88/06/17 4.0NFSSRC SMI */

#import <machine/reg.h>
#import <machine/psl.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/proc.h>
#import <sys/buf.h>
#import <sys/acct.h>
#import <sys/wait.h>
#import <sys/file.h>
#import <sys/uio.h>
#import <sys/mbuf.h>

#if	NeXT
struct	proc *pidhash[PIDHSZ];
struct	proc *kernel_proc, *init_proc;
#else	NeXT
short	pidhash[PIDHSZ];
#endif	NeXT
struct	proc *freeproc, *zombproc, *allproc;
			/* lists of procs in various states */
struct	prochd qs[NQS];
int	mpid;			/* generic for unique process id's */

/*
 * Clear any pending stops for top and all descendents.
 */
spgrp(top)
	struct proc *top;
{
	register struct proc *p;
	int f = 0;

	p = top;
	for (;;) {
		p->p_sig &=
			  ~(sigmask(SIGTSTP)|sigmask(SIGTTIN)|sigmask(SIGTTOU));
		f++;
		/*
		 * If this process has children, descend to them next,
		 * otherwise do any siblings, and if done with this level,
		 * follow back up the tree (but not past top).
		 */
		if (p->p_cptr)
			p = p->p_cptr;
		else if (p == top)
			return (f);
		else if (p->p_osptr)
			p = p->p_osptr;
		else for (;;) {
			p = p->p_pptr;
			if (p == top)
				return (f);
			if (p->p_osptr) {
				p = p->p_osptr;
				break;
			}
		}
	}
}

/*
 * Is p an inferior of the current process?
 */
inferior(p)
	register struct proc *p;
{

	for (; p != u.u_procp; p = p->p_pptr)
		if (p->p_ppid == 0)
			return (0);
	return (1);
}

struct proc *
pfind(pid)
	int pid;
{
	register struct proc *p;

	for (p = pidhash[PIDHASH(pid)]; p != (struct proc *) 0; p = p->p_hash)
		if (p->p_pid == pid)
			return (p);
	return ((struct proc *)0);
}

#if	NeXT
pidhash_enter(struct proc *p)
{
	int	n;

	n = PIDHASH(p->p_pid);
	p->p_hash = pidhash[n];
	pidhash[n] = p;
}

static zone_t	proc_zone;
static int	proc_count;

struct proc *getproc()
{
	if (proc_count >= max_proc)
		return((struct proc *)0);
	proc_count++;
	return((struct proc *)zalloc(proc_zone));
}

pqinit()
{
	register struct proc *p;
	int	size;

	size = sizeof(struct proc);
	proc_zone = zinit(size, 100*max_proc*size, 0, FALSE, "proc structures");
	proc_count = 0;

	freeproc = NULL;
	p = getproc();
	allproc = p;
	p->p_nxt = NULL;
	p->p_prev = &allproc;
	kernel_proc = p;

	zombproc = NULL;
}
#else	NeXT
/*
 * init the process queues
 */
pqinit()
{
	register struct proc *p;

	/*
	 * most procs are initially on freequeue
	 *	nb: we place them there in their "natural" order.
	 */

	freeproc = NULL;
	for (p = procNPROC; --p > proc; freeproc = p)
		p->p_nxt = freeproc;

	/*
	 * but proc[0] is special ...
	 */

	allproc = p;
	p->p_nxt = NULL;
	p->p_prev = &allproc;

	zombproc = NULL;
}
#endif	NeXT


