/*
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 17-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Created. (to fake out kern/ast.h into thinking that our hw
 *	supports this.
 */

/*
 *	next/ast.h: Definitions for vax hardware ast mechanism.
 */

#ifndef	_NEXT_AST_H_
#define _NEXT_AST_H_

#import <kern/macro_help.h>
#import <kern/thread.h>

#import <next/pcb.h>

/*
 *	ast_context sets up ast context for this thread on
 *	the specified cpu (always the current cpu).  On NeXT
 *	doesn't do anything, but it could initialize some registers
 */

#define	ast_context(thread, cpu) 					\
	MACRO_BEGIN							\
	if ((thread)->ast) {						\
		(thread)->pcb->pcb_flags |= AST_SCHEDULE;		\
	} else {							\
		(thread)->pcb->pcb_flags &= ~AST_SCHEDULE;		\
	}								\
	MACRO_END

/*
 *	ast_propagate: cause an ast if needed.
 */

#define ast_propagate(thread, cpu)					\
	MACRO_BEGIN							\
	if ((thread)->ast) {						\
		(thread)->pcb->pcb_flags |= AST_SCHEDULE;		\
	}								\
	MACRO_END

/*
 *	ast_needed: Is an ast already pending?
 */

#define ast_needed(cpu)							\
	((current_thread()->pcb->pcb_flags & AST_SCHEDULE) == AST_SCHEDULE)

#endif _NEXT_AST_H_




