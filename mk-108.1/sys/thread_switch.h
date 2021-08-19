/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	thread_switch.h,v $
 * Revision 2.3  89/10/15  02:06:04  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:41:47  dlb
 * 	Merge.
 * 	[89/09/01  17:57:58  dlb]
 * 
 * Revision 2.1.1.2  89/08/02  23:12:52  dlb
 * 	Merge to X96
 * 
 * Revision 2.1.1.1  89/07/25  19:05:41  dlb
 * 	Created.
 * 
 */

#ifndef	_SYS_THREAD_SWITCH_H_
#define	_SYS_THREAD_SWITCH_H_

/*
 *	Constant definitions for thread_switch trap.
 */

#define	SWITCH_OPTION_NONE	0
#define SWITCH_OPTION_DEPRESS	1
#define SWITCH_OPTION_WAIT	2

#define valid_switch_option(opt)	((0 <= (opt)) && ((opt) <= 2))

#endif	_SYS_THREAD_SWITCH_H_

