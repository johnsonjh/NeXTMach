/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	msgbuf.h,v $
 * Revision 2.5  89/05/30  10:43:37  rvb
 * 	Rephrased a comment.
 * 	[89/05/17            af]
 * 
 * Revision 2.4  89/03/09  22:05:57  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  17:54:58  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  89/01/23  22:27:16  af
 * 	Hack for mips msgbuf allocation
 * 	[89/01/07            af]
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)msgbuf.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_MSGBUF_H_
#define _SYS_MSGBUF_H_

#define MSG_MAGIC	0x063061
#define MSG_BSIZE	(4096 - 3 * sizeof (long))

struct	msgbuf {
	long	msg_magic;
	long	msg_bufx;
	long	msg_bufr;
	char	msg_bufc[MSG_BSIZE];
};

#ifdef	KERNEL
#if	mips || NeXT
/*
 * The message buffer lives at the end of physical address
 * space, so that we can recover it after a crash.
 */
extern struct	msgbuf *pmsgbuf;
#else
extern struct	msgbuf msgbuf;
#endif	mips
#endif	KERNEL
#endif	_SYS_MSGBUF_H_

