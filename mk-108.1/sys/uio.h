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
 * $Log:	uio.h,v $
 * 16-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Added uio_fmode flag for vfs compatibility.
 *
 * Revision 2.7  89/04/22  15:32:36  gm0w
 * 	Removed MACH_VFS changes.
 * 	[89/04/14            gm0w]
 * 
 * Revision 2.6  89/03/09  22:09:37  rpd
 * 	More cleanup.
 * 
 * Revision 2.5  89/02/25  17:57:49  gm0w
 * 	Made CMUCS and MACH_VFS conditional code always
 * 	true.
 * 	[89/02/14            mrt]
 * 
 * Revision 2.4  89/01/18  01:20:06  jsb
 * 	Vnode support: define uio_seg as alias for uio_segflg (for Sun-derived
 * 	code which uses uio_seg).
 * 	[89/01/13            jsb]
 * 
 * Revision 2.3  88/08/24  02:51:09  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:26:52  mwyoung]
 * 
 * 26-Feb-88  David Kirschen (kirschen) at Encore Computer Corporation
 *      Added #include of types.h to get caddr_t.
 *
 * 06-Jan-88  Jay Kistler (jjk) at Carnegie Mellon University
 *	Added declarations for __STDC__.
 *
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)uio.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_UIO_H_
#define _SYS_UIO_H_

#import <sys/types.h>

struct iovec {
	caddr_t	iov_base;
	int	iov_len;
};

struct uio {
	struct	iovec *uio_iov;
	int	uio_iovcnt;
	off_t	uio_offset;
	int	uio_segflg;
	short	uio_fmode;
	int	uio_resid;
};

enum	uio_rw { UIO_READ, UIO_WRITE };

/*
 * Segment flag values (should be enum).
 */
#define UIO_USERSPACE	0		/* from user data space */
#define UIO_SYSSPACE	1		/* from system space */
#define UIO_USERISPACE	2		/* from user I space */

#if	defined(__STDC__) && !defined(KERNEL)
extern int readv(int, struct iovec *, int);
extern int writev(int, struct iovec *, int);
#endif	defined(__STDC__) && !defined(KERNEL)

#endif	_SYS_UIO_H_

