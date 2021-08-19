/*
 *	File:	unix_traps.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Macro definition for fast Unix traps.
 */

#ifndef	_UNIX_TRAPS_
#define	_UNIX_TRAPS_

#import <next/syscall_sw.h>

#define fast_kernel_trap(number, args)	\
	movl	sp,a0;			\
	save_registers_##args;		\
	kernel_trap_args_##args;	\
	movl	\#number,d0;		\
	trap	\#4;			\
	restore_registers_##args;

#endif	_UNIX_TRAPS_
