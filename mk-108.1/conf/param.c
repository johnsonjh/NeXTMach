/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 *  1-Jun-90  Gregg Kellogg (gk) at NeXT
 *	Doubled ncallout (Lee's running out again).
 *
 * 20-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Added defines for global variables not explicitly defined elsewhere.
 *	(things are defined extern in header files now).
 *
 * 22-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0.
 *	Removed SUN_VFS references.  Added SUN_FIFO compile option
 *
 * 10-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Added relevant include files.  #if'd out namei cache and
 *	     relevant data structures.  Added FIFO initialization.
 *	     FIXME:  Check QUOTA ifdef's to be sure they are correct.
 *
 * 26-Jun-87  William Bolosky (bolosky) at Carnegie-Mellon University
 *	Made QUOTA a #if-type option.
 *
 * 27-Sep-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Changed to include <confdep.h>
 *
 * 10-Jul-86  Supercomputer kernel (mach) at Carnegie-Mellon University
 *	Changed NINODE calculation to *2 from *4.
 *
 * 20-Mar-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	If compiling a system with new VM, then increase NINODE by a
 *	factor of 4 (since the new VM code pages out to inodes).
 *
 */

#import <quota.h>
#import <confdep.h>

/* @(#)param.c	1.3 87/06/16 3.2/4.3NFSSRC */
/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)param.c	7.1 (Berkeley) 6/5/86
 */

#ifndef lint
char bsd_copyright[] =
"@(#) Copyright (c) 1980, 1986 Regents of the University of California.\n\
 All rights reserved.\n";
char cmu_copyright[] =
"Copyright (c) 1987 Carnegie Mellon University.";
char next_copyright[] =
"Copyright (c) 1988, 1989 NeXT, Inc.";
#endif not lint

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/socket.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/vnode.h>
#import <sys/file.h>
#import <sys/callout.h>
#import <sys/clist.h>
#import <sys/mbuf.h>
#import <sys/domain.h>
#import <sys/kernel.h>
#import <ufs/inode.h>
#import <ufs/quotas.h>
#import <specfs/fifo.h>

/*
 *	Pick up machine dependent parameters (e.g., HZ).
 */
#import <machine/param.h>

int	hz = HZ;
int	tick = 1000000 / HZ;
int	tickadj = 240000 / (60 * HZ);		/* can adjust 240ms in 60s */
#if	NeXT
int	max_proc = 500;		/* XXX sanity */
#define	NPROC (20 + 8 * MAXUSERS)
#define NINODE (((NPROC + 16 + MAXUSERS) + 32)*2)
#else	NeXT
struct	timezone tz = { TIMEZONE, DST };
#define	NPROC (20 + 8 * MAXUSERS)
#define NINODE (((NPROC + 16 + MAXUSERS) + 32)*2)
int	nproc = NPROC;
#endif	NeXT
int	nchsize = NINODE * 11 / 10;
#if	NeXT
int	ncallout = 16 + 2*NPROC;
#else	NeXT
int	ncallout = 16 + NPROC;
#endif	NeXT
int	nclist = 60 + 12 * MAXUSERS;
int     nmbclusters = NMBCLUSTERS;
int	nport = NPROC / 2;
int	ncsize = (NPROC + 16 + MAXUSERS) + 32; /* name cache size */
#if	QUOTA
int	ndquot = (MAXUSERS*NMOUNT)/4 + NPROC;
#else
int	ndquot = 0;
#endif	QUOTA

#ifdef INET
#define	NETSLOP	20			/* for all the lousy servers */
#else
#define	NETSLOP	0
#endif
/*
 * These are initialized at bootstrap time
 * to values dependent on memory size
 */
int	nbuf;

/*
 * These have to be allocated somewhere; allocating
 * them here forces loader errors if this file is omitted
 * (if they've been externed everywhere else; hah!).
 */
#if	NeXT
struct	file *file;
#else	NeXT
struct	proc *proc, *procNPROC;
struct	inode *inode;
struct	file *file;
struct	inode *inodeNINODE;
struct	file *fileNFILE;
#endif	NeXT
struct 	callout *callout;
struct	cblock *cfree;
struct	cblock *cfreelist = 0;
int	cfreecount = 0;
struct	buf *buf;
char	*buffers;
#if	QUOTA
struct	dquot *dquot, *dquotNDQUOT;
#endif	QUOTA
/* initialize SystemV named-pipe (and pipe()) information structure */
struct fifoinfo fifoinfo = {
      FIFOBUF,
      FIFOMAX,
      FIFOBSZ,
      FIFOMNB
};
struct	domain *domains;





