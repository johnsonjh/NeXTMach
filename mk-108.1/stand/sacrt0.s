//	crt0.o for NeXT standalones

#define ASSEMBLER	1

#import <next/trap.h>
#import <next/cframe.h>

STKSIZE=8*1024
PCB_REGS=4
PCB_USP=0

	.text
	.globl	_entry
	.globl	_realentry
_entry:
	movl	sp,__monsp
	movl	a6,__monfp
	movl	sp@(4),__bootarg
	movl	#__DATA__end,sp
	addl	#STKSIZE,sp		// setup initial stack
	movc	vbr,d0
	movl	d0,sp@-
	movl	#__DATA__end,sp@-	// sainit clears bss
	movl	#__DATA__bss__begin,sp@-
	jsr	_sainit			// sainit return scb base
	addl	#12,sp
	movc	d0,vbr
	movl	sp,d0
	addl	#STKSIZE,d0
	movl	d0,sp@-
	pea	_realentry
	jsr	_kdbg			// returns if debugging == 0
	addql	#8,sp
_realentry:
	movl	#1,_openfirst
	movl	__bootarg,sp@-
	jsr	_main
	addql	#4,sp
	movl	d0,sp@-			// push exit code
	jsr	_exit			// doesn't return

	.globl	_setvbr
_setvbr:
	movl	sp@(4),d0
	movc	d0,vbr
	rts
	
	.globl	_getvbr
_getvbr:
	movc	vbr,d0
	rts

//
// trap handlers -- all basically just push trap cause and
// branch to generic trap handler
//
	.globl	__trap
__trap:
	clrw	sp@-
	moveml	#0xffff,sp@-
	movl	usp,a0
	movl	a0,sp@-
	movl	sp,a0
	movl	a0,sp@-
	addl	#(16*4)+2+4,a0
	movl	a0,sp@((15*4)+8)
	jsr	_trap
	// doesn't return

	.globl	__kresume
__kresume:
	movl	#1,_client_running
	movl	_client_pcb+PCB_USP,a0
	movl	a0,usp
	moveml	_client_pcb+PCB_REGS,#0xffff
	//
	// C kresume has already munged stack and sp
	// so this is always a short stack frame
	//
	rte

	.globl	_setjmp
_setjmp:
	movl	sp@(4),a0
	movl	sp@,a0@(16*4)
	moveml	#0xffff,a0@
	clrl	d0
	rts

	.globl	_longjmp
_longjmp:
	movl	sp@(4),a0		// jmpbuf address
	movl	sp@(8),a0@		// drop d0 into d0 of the jmpbuf
	movl	a0@(16*4),a0@(8*4)	// drop the pc into a0 of the jmpbuf
	moveml	a0@,#0xffff
	jra	a0@

	.globl	_next_monstart
_next_monstart:
	movl	#0,d0
	movl	__monfp,a6
	movl	__monsp,sp
	rts
//	movl	#__halt,d0
//	trap	#13

	.globl	_getsp
_getsp:
	movl	sp,d0
	rts

	.globl	__runit
__runit:					// runit(pc)
	movl	sp@(4),d0
	movl	__monfp,a6
	movl	__monsp,sp
	rts
//	movl	sp@(4),a0
//	jmp	a0@

	.globl	_restore_mg
_restore_mg:
	movc	vbr,a0
	movl	a0@(T_BADTRAP),d0
	rts

//
// This stuff MUST be in .data, otherwise it gets zeroed by sainit
//
	.data

	.globl	__monsp
__monsp:
	.long	0

	.globl	__monfp
__monfp:
	.long	0

__bootarg:
	.long	0

__halt:	.asciz	"-h"

