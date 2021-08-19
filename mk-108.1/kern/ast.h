/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	ast.h,v $
 * Revision 2.3  89/10/15  02:03:45  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:01:04  dlb
 * 	extern need_ast array.
 * 	AST_NONE --> AST_ZILCH to avoid conflict with vax usage.
 * 	[89/02/03            dlb]
 * 
 * 	Add HW_AST option to support hardware (e.g. vax) that does ast's
 * 	in hardware.
 * 	[88/09/27            dlb]
 * 
 * 11-Aug-88  David Black (dlb) at Carnegie-Mellon University
 *	Created.  dbg gets equal credit for the design.
 *
 */

/*
 *	kern/ast.h: Definitions for Asynchronous System Traps.
 */

#ifndef	_KERN_AST_H_
#define _KERN_AST_H_

/*
 *	There are two types of AST's:
 *		1.  This thread must context switch [call thread_block()]
 *		2.  This thread must do something bizarre
 *			[call thread_halt_self()]
 *
 *	Type 2 ASTs are kept in a field in the thread which encodes the
 *	bizarre thing the thread must do.
 *
 *	The need_ast array (per processor) records whether ASTs are needed
 *	for each processor.  For now each processor only has access to its
 *	own cell in that array.  [May change when we record which 
 *	processor each thread is executing on.]
 *
 *	need_ast is initialized from the thread's ast field at context
 *	switch.  Type 1 ASTs are entered directly into the field
 *	by aston().  The actual values in need_ast do not matter, 
 *	an ast is required if it is non-zero.
 */

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cpus.h>
#import <hw_ast.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <machine/cpu.h>

/*
 *	Bits for reasons
 */

#define	AST_ZILCH	0x0
#define AST_HALT	0x1
#define AST_TERMINATE	0x2
/*#define AST_PROFILE	0x4  For future use */

/*
 *	Machines with hardware support (e.g. vax) turn on HW_AST option.
 *	This causes all type 1 ast support to be pulled in from machine/ast.h.
 */

#if	HW_AST
#import <machine/ast.h>
#else	HW_AST

extern int	need_ast[NCPUS];

/*
 *	Type 1 ASTs
 */
#define	aston()		need_ast[cpu_number()] = 1
#define astoff()	need_ast[cpu_number()] = 0

#endif	HW_AST
/*
 *	Type 2 ASTs
 */
#define	thread_ast_set(thread, reason)	(thread)->ast |= (reason)
#define thread_ast_clear(thread, reason)	(thread)->ast &= ~(reason)
#define thread_ast_clear_all(thread)	(thread)->ast = AST_ZILCH

/*
 *	NOTE: if thread is the current thread, thread_ast_set should
 *	be followed by aston() 
 */

#if	HW_AST
/*
 *	machine/ast.h must define versions of these macros.
 */
#else	HW_AST
/*
 *	Macros to propagate thread asts to need_ast at context switch and
 *	clock interrupts.  (Context switch replaces old ast requests,
 *	clock interrupt reflects new requests from thread to need_ast.
 *
 *	NOTE: cpu is always the current cpu.  It is in these macros
 *	solely to avoid recalculating it on machines where that may
 *	be expensive.
 */

#define	ast_context(thread, cpu)	need_ast[(cpu)] = (thread)->ast
#define	ast_propagate(thread, cpu)	need_ast[(cpu)] |= (thread)->ast
#define	ast_needed(cpu)			need_ast[(cpu)]
#endif	HW_AST

#endif	_KERN_AST_H_
