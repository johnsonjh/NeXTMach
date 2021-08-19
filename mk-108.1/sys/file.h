/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 11-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Added f_cred pointer to user's credentials
 *	     Moved fcntl and open flag defines to fcntl.h
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)file.h	7.1 (Berkeley) 6/4/86
 */

/* @(#)file.h	2.1 88/05/18 4.0NFSSRC SMI;	from UCB 7.1 06/04/86	*/
#import <sys/fcntl.h>

#ifdef	KERNEL_FILE
#define	KERNEL
#endif	KERNEL_FILE
#ifdef KERNEL
#import <kern/queue.h>

/*
 * Descriptor table entry.
 * One for each kernel object.
 */
struct	file {
	queue_chain_t	links;	/* links for list of all files */
	int	f_flag;		/* see below */
	short	f_type;		/* descriptor type */
	short	f_count;	/* reference count */
	short	f_msgcount;	/* references from message queue */
	struct	fileops {
		int	(*fo_rw)();
		int	(*fo_ioctl)();
		int	(*fo_select)();
		int	(*fo_close)();
	} *f_ops;
	caddr_t	f_data;     /* ptr to file specific struct ([iv]node/socket) */
	off_t	f_offset;
/*VFS*/	struct	ucred *f_cred;	/* credentials of user who opened file */
};

queue_head_t	file_list;
struct	file *getf();
struct	file *falloc();
#endif

/*
 * flags- also for fcntl call. (VFS: see also fcntl.h)
 */
#define	FOPEN		(-1)
#define	FREAD		00001		/* descriptor read/receive'able */
#define	FWRITE		00002		/* descriptor write/send'able */
#define	FMARK		00020		/* mark during gc() */
#define	FDEFER		00040		/* defer for next gc pass */
#define	FSHLOCK		00200		/* shared lock present */
#define	FEXLOCK		00400		/* exclusive lock present */

/* bits to save after open */
#define	FMASK		00113
#define	FCNTLCANT	(FREAD|FWRITE|FMARK|FDEFER|FSHLOCK|FEXLOCK)

/*
 * User calls
 */


/*
 * Flock call.
 */
#define	LOCK_SH		1	/* shared lock */
#define	LOCK_EX		2	/* exclusive lock */
#define	LOCK_NB		4	/* don't block when locking */
#define	LOCK_UN		8	/* unlock */

/*
 * Lseek call.
 */
#define	L_SET		0	/* absolute offset */
#define	L_INCR		1	/* relative to current offset */
#define	L_XTND		2	/* relative to end of file */

#ifdef KERNEL
#if	NeXT
#define	GETF(fp, fd) { \
	if ((unsigned)(fd) >= u.u_ofile_cnt || ((fp) = u.u_ofile[fd]) == NULL) { \
		u.u_error = EBADF; \
		return; \
	} \
}
#else	NeXT
#define	GETF(fp, fd) { \
	if ((unsigned)(fd) >= NOFILE || ((fp) = u.u_ofile[fd]) == NULL) { \
		u.u_error = EBADF; \
		return; \
	} \
}
#endif	NeXT
#define	DTYPE_VNODE	1	/* file */
#define	DTYPE_SOCKET	2	/* communications endpoint */
#endif
#ifdef	KERNEL_FILE
#undef	KERNEL
#endif	KERNEL_FILE


