/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	bkmac.h,v $
 * Revision 2.3  89/03/09  22:02:19  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:52:01  gm0w
 * 	Changes for cleanup.
 * 
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)bkmac.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_BKMAC_H_
#define _SYS_BKMAC_H_

#ifdef	KERNEL
#import <kern/macro_help.h>

/*
 * Macro definition of bk.c/netinput().
 * This is used to replace a call to
 *		(*linesw[tp->t_line].l_rint)(c,tp);
 * with
 *
 *		if (tp->t_line == NETLDISC)
 *			BKINPUT(c, tp);
 *		else
 *			(*linesw[tp->t_line].l_rint)(c,tp);
 */

#define BKINPUT(c, tp)						\
MACRO_BEGIN							\
	if ((tp)->t_rec == 0) {					\
		*(tp)->t_cp++ = (c);				\
		if (++(tp)->t_inbuf == 1024 || (c) == '\n') {	\
			(tp)->t_rec = 1;			\
			wakeup((caddr_t)&(tp)->t_rawq);		\
		}						\
	}							\
MACRO_END

#endif	KERNEL
#endif	_SYS_BKMAC_H_
