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
 * $Log:	xpr.h,v $
 * 22-Jun-89  Mike DeMoney (mike) at NeXT.
 *	Put xpr macro in {}'s so it wouldn't steal a following "else" clause.
 *
 * Revision 2.10  89/05/30  10:38:29  rvb
 * 	Removed Mips crud.
 * 	[89/04/20            af]
 * 
 * Revision 2.9  89/03/09  20:17:45  rpd
 * 	More cleanup.
 * 
 * Revision 2.8  89/02/25  18:11:09  gm0w
 * 	Kernel code cleanup.
 * 	Made XPR_DEBUG stuff always true outside the kernel
 * 	[89/02/15            mrt]
 * 
 * Revision 2.7  89/02/07  01:06:11  mwyoung
 * Relocated from sys/xpr.h
 * 
 * Revision 2.6  89/01/23  22:30:39  af
 * 	Added more flags, specific to Mips.  Should eventually integrate,
 * 	but for now there are conflicts.
 * 	Also made it usable from assembly code.
 * 	[89/01/05            af]
 * 
 * Revision 2.5  88/12/19  02:51:59  mwyoung
 * 	Added VM system tags.
 * 	[88/11/22            mwyoung]
 * 
 * Revision 2.4  88/08/24  02:55:54  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:29:56  mwyoung]
 *
 *  9-Apr-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Added flags for TCP and MACH_NP debugging.
 *
 *  6-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Make the event structure smaller to make it easier to read from
 *	kernel debuggers.
 *
 * 16-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	MACH:  made XPR_DEBUG definition conditional on MACH
 *	since the routines invoked under it won't link without MACH.
 *	[ V5.1(F7) ]
 */
/*
 * Include file for xpr circular buffer silent tracing.  
 *
 */

/*
 * If the kernel flag XPRDEBUG is set, the XPR macro is enabled.  The 
 * macro should be invoked something like the following:
 *	XPR(XPR_SYSCALLS, ("syscall: %d, 0x%x\n", syscallno, arg1);
 * which will expand into the following code:
 *	if (xprflags & XPR_SYSCALLS)
 *		xpr("syscall: %d, 0x%x\n", syscallno, arg1);
 * Xpr will log the pointer to the printf string and up to 6 arguements,
 * along with a timestamp and cpuinfo (for multi-processor systems), into
 * a circular buffer.  The actual printf processing is delayed until after
 * the buffer has been collected.  It is assumed that the text/data segments
 * of the kernel can easily be reconstructed in a post-processor which
 * performs the printf processing.
 *
 * If the XPRDEBUG compilation switch is not set, the XPR macro expands 
 * to nothing.
 */

#ifndef	_KERN_XPR_H_
#define _KERN_XPR_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <xpr_debug.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <machine/xpr.h>

#if	XPR_DEBUG

#ifndef	ASSEMBLER

extern unsigned int xprflags;
extern unsigned int xprwrap;
#define XPR(flags,xprargs) { if(xprflags&flags) xpr xprargs; }

#endif	ASSEMBLER

/*
 * flags for message types.
 */
#define XPR_SYSCALLS		(1 << 0)
#define XPR_TRAPS		(1 << 1)
#define XPR_SCHED		(1 << 2)
#define XPR_NPTCP		(1 << 3)
#define XPR_NP			(1 << 4)
#define XPR_TCP			(1 << 5)

#define XPR_VM_OBJECT		(1 << 8)
#define XPR_VM_OBJECT_CACHE	(1 << 9)
#define XPR_VM_PAGE		(1 << 10)
#define XPR_VM_PAGEOUT		(1 << 11)
#define XPR_MEMORY_OBJECT	(1 << 12)
#define XPR_VM_FAULT		(1 << 13)
#define XPR_VNODE_PAGER		(1 << 14)
#define XPR_VNODE_PAGER_DATA	(1 << 15)

#else	XPR_DEBUG
#define XPR(flags,xprargs)
#endif	XPR_DEBUG

#ifndef	ASSEMBLER
struct xprbuf {
	char 	*msg;
	int	arg1,arg2,arg3,arg4,arg5;
	int	timestamp;
	int	cpuinfo;
};
#endif	ASSEMBLER
#endif	_KERN_XPR_H_



