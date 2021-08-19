/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	policy.h,v $
 * 22-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Added POLICY_INTERACTIVE for interactive threads.
 *
 * Revision 2.3  89/10/15  02:05:50  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:40:53  dlb
 * 	Cleanup changes.
 * 	[89/08/02            dlb]
 * 
 * Revision 2.1.1.2  89/08/02  23:12:03  dlb
 * 	Cleanup changes.
 * 	[89/08/02            dlb]
 * 
 * Revision 2.1.1.1  89/07/25  18:47:00  dlb
 * 	Created.
 * 
 */

#ifndef	_SYS_POLICY_H_
#define _SYS_POLICY_H_

/*
 *	sys/policy.h
 *
 *	Definitions for scheduing policy.
 */

/*
 *	Policy definitions.  Policies must be powers of 2.
 */
#define	POLICY_TIMESHARE	1
#define POLICY_FIXEDPRI		2
#define POLICY_INTERACTIVE	4
#define POLICY_LAST		4

#define invalid_policy(policy)	(((policy) <= 0) || ((policy) > POLICY_LAST))

#endif _SYS_POLICY_H_


