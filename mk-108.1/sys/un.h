/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	un.h,v $
 * Revision 2.3  89/03/09  22:09:44  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:57:56  gm0w
 * 	Changes for cleanup.
 * 
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)un.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_UN_H_
#define _SYS_UN_H_

/*
 * Definitions for UNIX IPC domain.
 */
struct	sockaddr_un {
	short	sun_family;		/* AF_UNIX */
	char	sun_path[108];		/* path name (gag) */
};

#ifdef	KERNEL
extern int	unp_discard();
#endif	KERNEL
#endif	_SYS_UN_H_
