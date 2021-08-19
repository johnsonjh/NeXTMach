/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	sys/boolean.h
 *
 *	Boolean data type.
 *
 * HISTORY
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Header file fixup, purge history.
 *
 */

#ifndef	_BOOLEAN_
#define	_BOOLEAN_

/*
 *	Pick up "boolean_t" type definition
 */

#ifndef	ASSEMBLER
#import <machine/boolean.h>
#endif	ASSEMBLER

#endif	_BOOLEAN_

/*
 *	Define TRUE and FALSE, only if they haven't been before,
 *	and not if they're explicitly refused.  Note that we're
 *	outside the _BOOLEAN_ conditional, to avoid ordering
 *	problems.
 */

#if	(defined(KERNEL) || defined(EXPORT_BOOLEAN)) && !defined(NOBOOL)

#ifndef	TRUE
#define TRUE	((boolean_t) 1)
#endif	TRUE

#ifndef	FALSE
#define FALSE	((boolean_t) 0)
#endif	FALSE

#endif	(defined(KERNEL) || defined(EXPORT_BOOLEAN)) && !defined(NOBOOL)
