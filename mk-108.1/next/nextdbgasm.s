PCB_REGS=4
PCB_USP=0
DBG_STACK=1024
// Stack for the debugger.
	.lcomm	dbgstack,DBG_STACK
//	 Switch stacks, initialize client_pcb and call nextdbginit.
	.globl	__nextdbginit
__nextdbginit:
	movl	sp@+,a0		// The return PC (Called without args)
	// Heaven help us if we change compilers (alignment!)
	moveml	#0xffff,_client_pcb+4	// Save all registers.
	movw	sr,d0		// And the sr.
	movw	d0,_client_pcb+(17*4)+2
	movw	#0x2700,sr	// Disable all interrupts.
	movl	a0,_client_pcb+(17*4)+4	// And the PC
	movl	#dbgstack+DBG_STACK,sp	// Load the new stack pointer
	movw	#0,_client_pcb+(17*4)+8	// Initialize frame type.
	jsr	_nextdbginit
	// doesnt return.

//
// trap handlers -- all basically just push trap cause and
// branch to generic trap handler
//
	
	.lcomm	dbgrts,4
	.globl	__dbg_trap
__dbg_trap:
	clrw	sp@-
	moveml	#0xffff,sp@-
	movc	usp,a0
	movl	a0,sp@-
	movl	sp,a0
	movl	a0,sp@-
	addl	#(16*4)+2+4,a0
	movl	a0,sp@((15*4)+8)
	jsr	_dbg_trap
	// A return from here means we need to pass the exception to 
	// the old exception vector.  The vector address should be in 
	// d0, restore the state and push d0 on the stack (very carefully)
	// and return to it.
	movl	d0,dbgrts	// save exception address.
	addl	#4,sp
	movl	sp@+,a0
	movc	a0,usp		// restore usp.
	moveml	sp@+,#0xffff
	movl	dbgrts,sp@-
	rts

	.globl	__dbg_kresume
__dbg_kresume:
	movl	#1,_client_running
	movl	_client_pcb+PCB_USP,a0
	movc	a0,usp
	moveml	_client_pcb+PCB_REGS,#0xffff
	//
	// C kresume has already munged stack and sp
	// so this is always a short stack frame
	//
	rte

	.globl	_dbg_setjmp
_dbg_setjmp:
	movl	sp@(4),a0
	movl	sp@,a0@(16*4)
	moveml	#0xffff,a0@
	clrl	d0
	rts

	.globl	_dbg_longjmp
_dbg_longjmp:
	movl	sp@(4),a0		// jmpbuf address
	movl	sp@(8),a0@		// drop d0 into d0 of the jmpbuf
	movl	a0@(16*4),a0@(8*4)	// drop the pc into a0 of the jmpbuf
	moveml	a0@,#0xffff
	jra	a0@
