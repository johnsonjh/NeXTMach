/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 *  1-Mar-90  Gregg Kellogg (gk) at NeXT
 *	kern/syscall_sw.h defines traps with negative arguments,
 *	changed kernel_trap() macro to turn that back into a positive
 *	argument.
 */

#ifndef	_MACHINE_SYSCALL_SW_
#define	_MACHINE_SYSCALL_SW_	1

#define kernel_trap_args_0	;

#define kernel_trap_args_1			\
	movl	a0@(4),d1;

#define kernel_trap_args_2			\
	kernel_trap_args_1;			\
	movl	a0@(8),d2;

#define kernel_trap_args_3			\
	kernel_trap_args_2;			\
	movl	a0@(12),d3;

#define kernel_trap_args_4			\
	kernel_trap_args_3;			\
	movl	a0@(16),d4;

#define kernel_trap_args_5			\
	kernel_trap_args_4;			\
	movl	a0@(20),d5;

#define kernel_trap_args_6			\
	kernel_trap_args_5;			\
	movl	a0@(24),d6;


#define kernel_trap_args_7			\
	kernel_trap_args_6;			\
	movl	a0@(24),d7;

#define save_registers_0	;
#define save_registers_1	;
#define save_registers_2			\
	movl	d2,sp@-
#define save_registers_3			\
	moveml	\#0x3000,sp@-
#define save_registers_4			\
	moveml	\#0x3800,sp@-
#define save_registers_5			\
	moveml	\#0x3c00,sp@-
#define save_registers_6			\
	moveml	\#0x3e00,sp@-
#define save_registers_7			\
	moveml	\#0x3f00,sp@-

#define restore_registers_0	;
#define restore_registers_1	;
#define restore_registers_2			\
	movl	sp@+,d2
#define restore_registers_3			\
	moveml	sp@+,\#0xc
#define restore_registers_4			\
	moveml	sp@+,\#0x1c
#define restore_registers_5			\
	moveml	sp@+,\#0x3c
#define restore_registers_6			\
	moveml	sp@+,\#0x7c
#define restore_registers_7			\
	moveml	sp@+,\#0xfc

#define kernel_trap(name, number, args)		\
	.globl	_##name;			\
_##name:					\
	movl	sp,a0;				\
	save_registers_##args;			\
	kernel_trap_args_##args;		\
	movl	\#-number,d0;			\
	trap	\#3;				\
	restore_registers_##args;		\
	rts

#endif	_MACHINE_SYSCALL_SW_

