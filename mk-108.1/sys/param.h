/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
 /*
 * HISTORY
 * $Log:	param.h,v $
 * 02-Apr-90  Avadis Tevanian, Jr. (avie) at NeXT
 *	Increased MAXUPRC to 100 for NeXT.
 *
 * 28-Feb-90  Gregg kellogg (gk) at NeXT
 *	Fixed CHECK_SIGNALS macro to take 3 arguments instead of 2.
 *
 * Revision 2.12  89/10/11  14:53:22  dlb
 * 	Minor macro changes to pass thread to thread_should_halt.
 * 	[88/10/18            dlb]
 * 
 */
#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cputypes.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)param.h	7.1 (Berkeley) 6/4/86
 */

/* SUN_VFS */
/*	@(#)param.h	2.2 88/06/09 4.0NFSSRC SMI;	*/
/* SUN_VFS */

#ifndef	_SYS_PARAM_H_
#define _SYS_PARAM_H_

#define	BSD	43		/* 4.3 * 10, as cpp doesn't do floats */
#define BSD4_3	1

/*
 * Machine type dependent parameters.
 */
#import <machine/machparam.h>

#define	NPTEPG		(NBPG/(sizeof (struct pte)))

/*
 * Machine-independent constants
 */
#define	NMOUNT	40		/* est. of # mountable fs for quota calc */
/* NMOUNT must be <= 255 unless c_mdev (cmap.h) is expanded */
#define	MSWAPX	15		/* pseudo mount table index for swapdev */
#if	NeXT
#define	MAXUPRC	100
#define	NOFILE	256		/* max open files per process */
#else	NeXT
#if	defined(multimax) || defined(balance)
#define	MAXUPRC 100		/* max processes per user */
#else	defined(multimax) || defined(balance)
#define	MAXUPRC	40		/* max processes per user */
#endif	defined(multimax) || defined(balance)
#define	NOFILE	64		/* max open files per process */
#endif	NeXT
/* SUN_VFS */
#define MAXLINK 32767		/* max links */
/* SUN_VFS */
#define	CANBSIZ	256		/* max size of typewriter line */
#if	NeXT
#define NCARGS	40960		/* # characters in exec arglist */
#else	NeXT
#define	NCARGS	20480		/* # characters in exec arglist */
#endif	NeXT
#define	NGROUPS	16		/* max number groups */

#define	NOGROUP	(gid_t)65535	/* marker for empty group set member */

/*
 * Priorities
 */
#define	PSWP	0
#define	PINOD	10
#define	PRIBIO	20
#define	PRIUBA	24
#define	PZERO	25
#define	PPIPE	26
/* SUN_VFS */
#define	PVFS	27
/* SUN_VFS */
#define	PWAIT	30
#define	PLOCK	35
#define	PSLEP	40
#define	PUSER	50
#define PMASK	0177
#define PCATCH	0400	/* return if sleep interrupted, don't longjmp */

#define	NZERO	0

/*
 * Signals
 */
#import <sys/signal.h> 

/*
 * Return values from tsleep().
 */
#define	TS_OK	0	/* normal wakeup */
#define	TS_TIME	1	/* timed-out wakeup */
#define	TS_SIG	2	/* asynchronous signal wakeup */

/*
 *	Check for per-process and per thread signals.
 */
#define SHOULDissig(p,uthreadp) \
	(((p)->p_sig | (uthreadp)->uu_sig) && ((p)->p_flag&STRC || \
	 (((p)->p_sig | (uthreadp)->uu_sig) &~ \
	   ((p)->p_sigignore | (p)->p_sigmask))))

/*
 *	Check for signals, handling possible stop signals.
 *	Ignores signals already 'taken' and per-thread signals.
 *	Use before and after thread_block() in sleep().
 *	(p) is always current process.
 */
#define	ISSIG(p) (thread_should_halt(current_thread()) || \
	 (SHOULDissig(p,current_thread()->u_address.uthread) && issig()))

/*
 *	Check for signals, including signals already taken and
 *	per-thread signals.  Use in trap() and syscall() before
 *	exiting kernel.
 */
#define	CHECK_SIGNALS(p, thread, uthreadp)	\
	(!thread_should_halt(thread)	\
	 && ((p)->p_cursig		\
	     || SHOULDissig(p,uthreadp)))

#define	NBPW	sizeof(int)	/* number of bytes in an integer */

#ifndef NULL
#if	defined(__STRICT_BSD__) || defined(KERNEL)
#define	NULL	0
#else /* __STRICT_BSD__  || KERNEL */
#import <stddef.h>
#endif /* __STRICT_BSD__ || KERNEL */
#endif /* NULL */
#define	CMASK	022		/* default mask for file creation */
#define	NODEV	(dev_t)(-1)

/*
 * Clustering of hardware pages on machines with ridiculously small
 * page sizes is done here.  The paging subsystem deals with units of
 * CLSIZE pte's describing NBPG (from vm.h) pages each.
 *
 * NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE
 */
#define	CLBYTES		(CLSIZE*NBPG)
#define	CLOFSET		(CLSIZE*NBPG-1)	/* for clusters, like PGOFSET */
#define	claligned(x)	((((int)(x))&CLOFSET)==0)
#define	CLOFF		CLOFSET
#define	CLSHIFT		(PGSHIFT+CLSIZELOG2)

#if CLSIZE==1
#define	clbase(i)	(i)
#define	clrnd(i)	(i)
#else
/* give the base virtual address (first of CLSIZE) */
#define	clbase(i)	((i) &~ (CLSIZE-1))
/* round a number of clicks up to a whole cluster */
#define	clrnd(i)	(((i) + (CLSIZE-1)) &~ (CLSIZE-1))
#endif

/* CBLOCK is the size of a clist block, must be power of 2 */
#define	CBLOCK	64
#define	CBSIZE	(CBLOCK - sizeof(struct cblock *))	/* data chars/clist */
#define	CROUND	(CBLOCK - 1)				/* clist rounding */

#ifndef ASSEMBLER
#import <sys/types.h>
#endif

/*
 * File system parameters and macros.
 *
 * The file system is made out of blocks of at most MAXBSIZE units,
 * with smaller units (fragments) only in the last direct block.
 * MAXBSIZE primarily determines the size of buffers in the buffer
 * pool. It may be made larger without any effect on existing
 * file systems; however making it smaller make make some file
 * systems unmountable.
 *
 * Note that the blocked devices are assumed to have DEV_BSIZE
 * "sectors" and that fragments must be some multiple of this size.
 * Block devices are read in BLKDEV_IOSIZE units. This number must
 * be a power of two and in the range of
 *	DEV_BSIZE <= BLKDEV_IOSIZE <= MAXBSIZE
 * This size has no effect upon the file system, but is usually set
 * to the block size of the root file system, so as to maximize the
 * speed of ``fsck''.
 */
#define	MAXBSIZE	8192
#if	NeXT
#define	DEV_BSIZE	1024
#define	DEV_BSHIFT	10		/* log2(DEV_BSIZE) */
#else	NeXT
#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#endif	NeXT
#define BLKDEV_IOSIZE	2048
#define MAXFRAG 	8

#define	btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	((unsigned)(bytes) >> DEV_BSHIFT)
#define	dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	((unsigned)(db) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and will be after we
 * add an entry to cdevsw for that purpose.  For now though
 * just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/*
 * MAXPATHLEN defines the longest permissable path length
 * after expanding symbolic links. It is used to allocate
 * a temporary buffer from the buffer pool in which to do the
 * name expansion, hence should be a power of two, and must
 * be less than or equal to MAXBSIZE.
 * MAXSYMLINKS defines the maximum number of symbolic links
 * that may be expanded in a path name. It should be set high
 * enough to allow all legitimate uses, but halt infinite loops
 * reasonably quickly.
 */
#define MAXPATHLEN	1024
#define MAXSYMLINKS	20

/*
 * bit map related macros
 */
#define	setbit(a,i)	(*(((char *)(a)) + ((i)/NBBY)) |= 1<<((i)%NBBY))
#define	clrbit(a,i)	(*(((char *)(a)) + ((i)/NBBY)) &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	(*(((char *)(a)) + ((i)/NBBY)) & (1<<((i)%NBBY)))
#define	isclr(a,i)      ((*(((char *)(a)) + ((i)/NBBY)) & (1<<((i)%NBBY))) == 0)

#if	!defined(vax) && !defined(i386)
#define _bit_set(i,a)   setbit(a,i)
#define _bit_clear(i,a)	clrbit(a,i)
#define _bit_tst(i,a)	isset(a,i)
#endif	!defined(vax) && !defined(i386)

/*
 * Macros for fast min/max.
 */
#ifndef	MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif	MIN
#ifndef	MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif	MAX

/*
 * Macros for counting and rounding.
 */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))

/* SUN_VFS */
/*
 * Scale factor for scaled integers used to count
 * %cpu time and load averages.
 */
#define FSHIFT	8	/* bits to right of fixed binary point */
#define FSCALE	(1<<FSHIFT)
/* SUN_VFS */

/*
 * Maximum size of hostname recognized and stored in the kernel.
 */
#define MAXHOSTNAMELEN	256
#define MAXDOMNAMELEN	256		/* maximum domain name length */

#define	DEFAULTHOSTNAME	"localhost"

#endif	_SYS_PARAM_H_







