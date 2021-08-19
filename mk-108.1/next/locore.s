#define	D25B	1
#define	D43B	1
#define	D50D	0

/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 22-Aug-90  John Seamons (jks) at NeXT
 *	Added back in cache_inval_page.  This just does a cpushp on the 040.
 *
 * 31-May-90  Gregg Kellogg (gk) at NeXT
 *	Added simple_lock routines for non-debugging single_cpu case
 *	so that loadable servers will work.
 *
 * 29-Mar-90  John Seamons (jks) at NeXT
 *	Added back the cache flush trap for Sun binary compatibility.
 *
 * 06-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Added AST_ZILCH and THREAD_AST in place of
 *	THREAD_EXIT_CODE and THREAD_EXIT.
 *
 * 28-Feb_90  John Seamons (jks) at NeXT
 *	Changed cpu_type variable to be referenced as a byte, not a long.
 *
 * 16-Feb-88  John Seamons (jks) at NeXT
 *	Updated to Mach release 2.
 *
 * 20-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#import <cpus.h>
#import <cputypes.h>
#import <iplmeas.h>
#import <eventmeter.h>
#import <mach_ldebug.h>

#import <sys/kern_return.h>
#import <sys/errno.h>
#import <sys/reboot.h>

#import <next/cframe.h>
#import <next/vm_param.h>
#import <next/vmparam.h>
#import <next/cpu.h>
#import <next/psl.h>
#import <next/pcb.h>
#import <next/mmu.h>
#import <next/trap.h>
#import <next/spl.h>

	.data
	.globl	_sdata
_sdata:	/* Marks the start of the data segment */
	.text
	.globl	_start
_start:
	jmp	entry		// so start = lowest address in text seg

	.globl	_start_text
_start_text:
ZERO_PHYS:			// lowest address in object file

	/*
	 *	This is the interrupt stack that is switched to on
	 *	interrupt entry.  This keeps the size of individual
	 *	pre-thread kernel stacks smaller.  It is at the start of
	 *	the text segment so that we get a redzone for free.
	 *	Since the kernel text is direct mapped, references
	 *	that fall off the front will fault.
	 */

	.globl	_intstack, _eintstack
_intstack:
	.skip	INTSTACK_SIZE
_eintstack:

	.data
#ifdef	DEBUG
	.globl	_lastsyscall
_lastsyscall:
	.long	0
#endif	DEBUG

#if	FIXME
	/*
	 *	We need a "fake" PCB to hold the values of the other
	 *	stack pointers during initialization (before we have
	 *	a real PCB).  We need one per CPU.
	 */

	.data
_startup_pcb:
	.skip	SIZEOF_PCB
#endif	FIXME

#if	NCPUS > 1
_start_lock:			// startup interlock
	.byte 0
_master_lock:			// master processor init interlock
	.byte 0
#endif	NCPUS > 1

	.globl	_cache
_cache:
	.long	0

	/*
	 *	The kernel is linked to start at the beginning of physical
	 *	memory in slot zero (0x04000000).  Absolute addressing
	 *	can be used right away even before the MMU is enabled.
	 *
	 *	Multiprocessor systems share the text/data/bss of
	 *	slot zero across the bus, however they maintain
	 *	their own kernel and interrupt stacks.
	 *	FIXME: setup seperate interrupt stack!
	 */

	.text
entry:
	/* use monitor stack until after NeXT_init() is called */

#if	 NCPUS > 1
try:
	tas	_start_lock
	beq	2f
1:	tstb	_start_lock
	bne	1b
	bra	try
2:
	tas	_master_lock
	bne	slave_start
#endif	NCPUS > 1

	jsr	_NeXT_init
	cmpb	#MC68030,_cpu_type
	beq	1f
	movl	#0,d0				// disable MMU
//	movc	d0,tc
	.long	0x4e7b0003
//	movc	d0,itt1				// disable previous TT1
	.long	0x4e7b0005
//	movc	d0,dtt1
	.long	0x4e7b0007
//	cinva	bc				// flush cache
	.word	0xf4d8
//	pflusha					// flush TLB
	.word	0xf518
	movl	_NeXT_kernel_mmu_rp+4,d0	// only load rp_ptbr portion for 68040
//	movc	d0,srp
	.long	0x4e7b0807
	movl	_NeXT_kernel_mmu_040_tt,d0
//	movc	d0,itt0
	.long	0x4e7b0004
//	movc	d0,dtt0
	.long	0x4e7b0006
	movl	_NeXT_kernel_mmu_040_tc,d0
//	movc	d0,tc
	.long	0x4e7b0003
	bra	2f
1:
//	pmove	_NeXT_kernel_mmu_030_tt,tt0	// transparently map kernel
	.long	0xf0390800
	.long	_NeXT_kernel_mmu_030_tt
//	pmove	_NeXT_kernel_mmu_rp,srp	// reconfigure MMU
	.long	0xf0394800
	.long	_NeXT_kernel_mmu_rp
//	pmove	_NeXT_kernel_mmu_030_tc,tc
	.long	0xf0394000
	.long	_NeXT_kernel_mmu_030_tc
2:
	movl	_cache,d0		// enable cache
	movc	d0,cacr
	lea	_eintstack,sp		// switch to interrupt stack
#if	GDB
	movl	#0,a6			// zero fp to keep GDB happy
#endif	GDB

	/*
	 *  Record the fsave stack frame version number
	 */
	 .data
	 .globl		_fpu_version
_fpu_version:
	.long	1
	.text
	cmpb	#MC68040,_cpu_type
	bne	5f
	frestore _fpu_version		// restore null frame to reset FPU
	fmovel	_fpu_version,fp0	// so we get an 'idle' frame..
	fsave	_fpu_version		// ..when the fsave is done
5:
	jsr	_startup_early		// early initializaion
	jsr	_setup_main		// returns initial thread in d0

	/*
	 *	Set up initial PCB and change to kernel stack in context
	 *	of first thread.
	 */

	movl	d0,a0
	movl	a0@(THREAD_PCB),a0	// get virtual address of PCB
	lea	1f,a1			// initial pc
	movl	a1,a0@(R_PC)
	movw	#SR_SUPER,a0@(R_SR)	// kernel mode, ipl 0
	movl	a0@(R_SP),sp		// switch to new kernel stack
	clrl	sp@-			// EF_NORMAL4 format for rte below
	movl	a0@(R_PC),sp@-		// push pc for rte below
	movw	a0@(R_SR),sp@-		// push sr for rte below
#if	NIPLMEAS
	moveml	#0xc0c0,sp@-		// paranoia...
	clrl	d0
	movw	a0@(R_SR),d0
	movl	d0,sp@-
	jsr	_ipl_rte		// ipl_rte(new_sr)
	addql	#4,sp
	moveml	sp@+,#0x0303
#endif	NIPLMEAS
	/*
	 *	Make sure the "cpu number" is still correct.
	 */

	jsr	_set_cpu_number
	rte				// change to thread context
1:

/*
 *	call main to set up the initial processes
 */
	jsr	_main
	/* NOTREACHED */

	.globl	_start_init
_start_init:

	/* fake a complete syscall exception stack */
	lea	_active_threads,a0	// current_thread()
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0
	movl	a0@(UTHREAD),a1		// pointer to u area
	subl	#FAKE_SIZE,sp
	movl	sp,a1@(U_AR0)		// u.u_ar0 = kernel sp
	movl	a0@(THREAD_PCB),a1	// pointer to PCB
	movl	sp,a1@(SAVED_REGS)	// same as u_ar0
	clrw	sp@(VOR)		// EF_NORMAL4 format
					// other fields set by exec()/setregs()
	jsr	_load_init_program	// load init program
#if	NCPUS > 1
	clrb	_start_lock		// release the lock
#endif	NCPUS > 1
	jmp	syscall_rtn		// return to init program

/*
 *	Bootstrap a thread from an already built kernel stack.
 */
	.globl	_thread_bootstrap
_thread_bootstrap:
	lea	_active_threads,a0	// get pointer to pcb
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0
	movl	a0@(THREAD_PCB),a0
	movw	sr,d0
	movw	#0x2700,sr
	bclr	#AST_SCHED_BIT,a0@(AST)	// reschedule?
	jeq	1f			// no
	bset	#TRACE_AST_BIT,a0@(AST)	// AST requested tracing
	jne	1f			// AST already tracing
	bset	#SR_TSINGLE_BIT-8,sp@(R_SR)
	jeq	1f			// wasnt set by user previously
	bset	#TRACE_USER_BIT,a0@(AST)// was set by user previously
1:	movw	d0,sr
	clrw	sp@(VOR)		// EF_NORMAL4 format
	movl	sp@(R_SP),a0		// restore usp
	movc	a0,usp
	cmpb	#MC68030,_cpu_type
	beq	2f
#if	D25B || D43B
	nop
#endif	D25B || D43B
//	cpusha	bc
	.word	0xf4f8
	bra	3f
2:
	movl	_cache,d0
	movc	d0,cacr			// clear cache and enable it
3:
#if	NIPLMEAS
	clrl	d0
	movw	sp@(R_SR),d0
	movl	d0,sp@-
	jsr	_ipl_rte		// ipl_rte(new_sr)
	addql	#4,sp
#endif	NIPLMEAS
					// FIXME: really restore d0,d1,a0,a1?
	moveml	sp@,#0x7fff		// restore all (except kernel sp)
	addw	#R_SR,sp		// pop register frame, including r_fsize
	rte

#if	NCPUS > 1
#ifdef	FIXME
	.data
foo_stack:
	.skip	512
efoo_stack:
#endif	FIXME

	.text
slave_start:
#ifdef	FIXME
	clrl	r1
	moval	_slave_scb-0x80000000(r1),r0
	mtpr	r0,$SCBB
	/* go into mapped mode */
	mtpr	_vax_sbr,$SBR
	mtpr	_vax_slr,$SLR
	mtpr	_vax_vsbr,$P0BR			# double map for jmp below
	mtpr	_vax_slr,$P0LR

	moval	_startup_pcb,r1		#address fake PCB
	mtpr	r1,$PCBB		#to store other stack pointers in
	mtpr	$1,$TBIA
	mtpr	$1,$MAPEN
	jmp	*$0f
0:
	moval	efoo_stack,sp
	calls	$0,_set_cpu_number		# set the cpu number (returned in r0)
	movl	_interrupt_stack[r0],sp		# virtual stack
	addl2	$INTSTACK_SIZE,sp		# start and end
	bbcci	$1,_start_lock,1f		# release the lock
1:
	calls	$0,_slave_main
	halt
#endif	NCPUS > 1
#endif	FIXME

PROCENTRY (curipl)
	movw	sr,d0
	lsrl	#8,d0
	andl	#7,d0
	PROCEXIT

#ifdef DEBUG
	.globl	_callsite
_callsite:
	movl	a6@(4),d0
	rts

	.globl	_getfp
_getfp:
	movl	a6,d0
	rts
#endif DEBUG

PROCENTRY (pflush_super)
	cmpb	#MC68030,_cpu_type
	beq	1f

	/* need to flush I/D cache when changing page attributes */
#if	D25B || D43B
	nop
#endif	D25B || D43B
//	cpusha	bc
	.word	0xf4f8
//	pflusha
	.word	0xf518
	bra	2f
1:
//	pflush	#0x14,#0x4
	.long	0xf0003094
2:
	PROCEXIT

PROCENTRY (pflush_user)
	cmpb	#MC68030,_cpu_type
	beq	1f

	/* need to flush I/D cache when changing page attributes */
#if	D25B || D43B
	nop
#endif	D25B || D43B
//	cpusha	bc
	.word	0xf4f8
//	pflushan
	.word	0xf510
	bra	2f
1:
//	pflush	#0x10,#0x4
	.long	0xf0003090
2:
	PROCEXIT

PROCENTRY (pload_read)
	movl	a_p0,d0
	movl	a_p1,a0
	cmpb	#MC68030,_cpu_type
	beq	1f
	movc	dfc,d1
	movc	d0,dfc
//	ptestr	a0@
	.word	0xf568
	movc	d1,dfc
	bra	2f
1:
//	pload	#0x8,a0@,READ
	.long	0xf0102208
2:
	PROCEXIT

PROCENTRY (pload_write)
	movl	a_p0,d0
	movl	a_p1,a0
	cmpb	#MC68030,_cpu_type
	beq	1f
	movc	dfc,d1
	movc	d0,dfc
#define	PTESTW_BUG	1
#if	PTESTW_BUG
//	ptestr	a0@
	.word	0xf568
#else	PTESTW_BUG
//	ptestw	a0@
	.word	0xf548
#endif	PTESTW_BUG
	movc	d1,dfc
	bra	2f
1:
//	pload	#0x8,a0@,WRITE
	.long	0xf0102008
2:
	PROCEXIT

PROCENTRY (pmove_crp)
	movl	a_p0,a0
	cmpb	#MC68030,_cpu_type
	beq	1f

	/* need to flush I/D cache when changing page attributes */
#if	D25B || D43B
	nop
#endif	D25B || D43B
//	cpusha	bc
	.word	0xf4f8
//	pflushan
	.word	0xf510
	movl	a0@(4),d0		// only load rp_ptbr portion for 68040
//	movc	d0,urp
	.long	0x4e7b0806
	bra	2f
1:
//	pmove	a0@,crp
	.long	0xf0104c00
2:
	PROCEXIT

PROCENTRY (pmove_tt1)
	movl	a_p0,a0
	cmpb	#MC68030,_cpu_type
	beq	1f
	movl	a0@,d0
//	movc	d0,itt1
	.long	0x4e7b0005
//	movc	d0,dtt1
	.long	0x4e7b0007
	bra	2f
1:
//	pmove	a0@,tt1
	.long	0xf0100c00
2:
	PROCEXIT

PROCENTRY (move_space)
	movl	a_p0,a0
	movl	a_p1,d0
	movc	dfc,d1
	movc	d0,dfc
	movl	a_p3,d0
	cmpl	#SIZE_LONG,a_p2
	bne	1f
	movsl	d0,a0@
	bra	3f
1:	cmpl	#SIZE_WORD,a_p2
	bne	2f
	movsw	d0,a0@
	bra	3f
2:	movsb	d0,a0@
3:	movc	d1,dfc
	PROCEXIT

PROCENTRY (cache_push_page)
	movl	a_p0,a0
	cmpb	#MC68030,_cpu_type
	beq	1f
#if	D25B || D43B
	nop
#endif	D25B || D43B
//	cpushp	bc,a0@
	.word	0xf4f0
1:
	PROCEXIT

PROCENTRY (cache_inval_page)
	movl	a_p0,a0
	cmpb	#MC68030,_cpu_type
	beq	1f
#if	D25B || D43B
	nop
#endif	D25B || D43B
//	cpushp	bc,a0@
	.word	0xf4f0
	bra	2f
1:	movl	_cache,d0
	movc	d0,cacr
2:
	PROCEXIT

PROCENTRY (get_vbr)
	movc	vbr,d0
	PROCEXIT

PROCENTRY (set_vbr)
	movl	a_p0,d0
	movc	d0,vbr
	PROCEXIT

	.data
global_lock:
	.byte	0
	.even
	.text

/* FIXME: SPL(7) at all?  Should drop to SPLX() if tas fails? */
PROCENTRY (bbcci)
	spl7()
	movl	#1,a0
1:
	tas	global_lock
	bne	1b
	movl	a_p0,d1
	movl	a_p1,a1
	bfclr	a1@{d1:#1}
	beq	2f
	movl	#0,a0
2:	clrb	global_lock
	movl	d0,d1
	splx(d1)
	movl	a0,d0
	PROCEXIT

PROCENTRY (bbssi)
	spl7()
	movl	#1,a0
1:
	tas	global_lock
	bne	1b
	movl	a_p0,d1
	movl	a_p1,a1
	bfset	a1@{d1:#1}
	bne	2f
	movl	#0,a0
2:	clrb	global_lock
	movl	d0,d1
	splx(d1)
	movl	a0,d0
	PROCEXIT

/*
 *	WARNING FIXME:
 *		sfc & dfc not saved or restored
 *		assumes all counts are < 64K
 *		doesn't check for address wrap
 */

	.globl	_ALLOW_FAULT_START
_ALLOW_FAULT_START:

PROCENTRY (copyoutstr)			// (from_k, to_u, bytes, &copied)
	movl	#FC_USERD,d0		// set destination func code
	movc	d0,dfc
	movl	a_p0,a0			// from kernel
	movl	a_p1,a1			// to user
	movl	a_p2,d1			// byte count
	jle	copy_inval		// byte count < 0
	bra	2f
1:	movb	a0@+,d0
	movsb	d0,a1@+
	beq	3f			// null byte copied
2:	dbra	d1,1b
	addqw	#1,d1
3:	movl	a_p2,d0			// return # bytes copied
	subw	d1,d0
	extl	d0
	movl	a_p3,d1
	beq	copy_exit
	movl	d1,a0
	movl	d0,a0@
	bra	copy_exit

PROCENTRY (copyinstr)			// (from_u, to_k, bytes, &copied)
	movl	#FC_USERD,d0		// set source func code
	movc	d0,sfc
	movl	a_p0,a0			// from user
	movl	a_p1,a1			// to kernel
	movl	a_p2,d1			// byte count
	jle	copy_inval		// byte count < 0
	bra	2f
1:	movsb	a0@+,d0
	movb	d0,a1@+
	beq	3f			// null byte copied
2:	dbra	d1,1b
	addqw	#1,d1
3:	movl	a_p2,d0			// return # bytes copied
	subw	d1,d0
	extl	d0
	movl	a_p3,d1
	beq	copy_exit
	movl	d1,a0
	movl	d0,a0@
	bra	copy_exit

PROCENTRY (copystr)			// (from_k, to_k, bytes, &copied)
	// note: copystr does NOT catch faults
	movl	a_p0,a0			// from kernel
	movl	a_p1,a1			// to kernel
	movl	a_p2,d1			// byte count
	bra	2f
1:	movb	a0@+,a1@+
	beq	3f			// null byte copied
2:	dbra	d1,1b
	addqw	#1,d1
3:	movl	a_p2,d0			// return # bytes copied
	subw	d1,d0
	extl	d0
	movl	a_p3,d1
	beq	4f
	movl	d1,a0
	movl	d0,a0@
4:	clrl	d0
	PROCEXIT

#define	COPY_THRESH	64

PROCENTRY(copyout)
	movl	#FC_USERD,d0	// set destination func code
	movc	d0,dfc
	movl	a_p0,a0		// a0 = source address
	movl	a_p1,a1		// a1 = destination address
	movl	a_p2,d0		// d0 = length
	jeq	copy_exit	// byte count = 0
	jle	copy_inval	// byte count < 0

	movl	a1,d1		// compute dst - (dst & 3)
	negl	d1
	andl	#3,d1
	jra	2f

1:	movb	a0@+,d0		// copy bytes to get dst to long word boundary
	movsb	d0,a1@+
	subql	#1,a_p2		// update length
2:	dbeq	d1,1b		// byte count or alignment count exhausted

	movl	a_p2,d0	// d0 = length
3:	rorl	#2,d0		// longwords to move
	bra	5f
4:	movl	a0@+,d1
	movsl	d1,a1@+
5:	dbra	d0,4b
	roll	#2,d0

	btst	#1,d0		// copy last short
	jeq	6f
	movw	a0@+,d1
	movsw	d1,a1@+

6:	btst	#0,d0		// copy last byte
	jeq	copy_exit
	movb	a0@,d1
	movsb	d1,a1@

	bra	copy_exit

PROCENTRY(copyin)
	movl	#FC_USERD,d0	// set source func code
	movc	d0,sfc
	movl	a_p0,a0		// a0 = source address
	movl	a_p1,a1		// a1 = destination address
	movl	a_p2,d0		// d0 = length
	jeq	copy_exit	// byte count = 0
	jle	copy_inval	// byte count < 0

	movl	a1,d1		// compute dst - (dst & 3)
	negl	d1
	andl	#3,d1
	jra	2f

1:	movsb	a0@+,d0		// copy bytes to get dst to long word boundary
	movb	d0,a1@+
	subql	#1,a_p2		// update length
2:	dbeq	d1,1b		// byte count or alignment count exhausted

	movl	a_p2,d0		// d0 = length
3:	rorl	#2,d0		// longwords to move
	bra	5f
4:	movsl	a0@+,d1
	movl	d1,a1@+
5:	dbra	d0,4b
	roll	#2,d0

	btst	#1,d0		// copy last short
	jeq	6f
	movsw	a0@+,d1
	movw	d1,a1@+

6:	btst	#0,d0		// copy last byte
	jeq	copy_exit
	movsb	a0@,d1
	movb	d1,a1@

	bra	copy_exit

/*
 * "Safe" version of bcopy.  It will return EFAULT if there is a
 * error in a page fault caused by this routine.
 */

PROCENTRY(copywithin)
	movl	a_p0,a0		// a0 = source address
	movl	a_p1,a1		// a1 = destination address
	movl	a_p2,d0		// d0 = length
	jeq	copy_exit	// length == 0!
	jle	copy_inval

	cmpl	a0,a1
	jhi	rcopy		// gotta do reverse copy
	jeq	copy_exit	// src == dst!

	movl	a1,d1		// compute dst - (dst & 3)
	negl	d1
	andl	#3,d1
	jra	2f

1:	movb	a0@+,a1@+	// copy bytes to get dst to long word boundary
	subql	#1,d0
2:	dbeq	d1,1b		// byte count or alignment count exhausted

	movl	d0,d1		// 4 bytes moved per 2 byte instruction
	andl	#0x1c,d1	// so instruction offset is:
	lsrl	#1,d1		// 2 * (k - n % k)
	negl	d1
	addl	#18,d1		// + fudge term of 2 for indexed jump
	jmp	pc@(0,d1)	// now dive into middle of unrolled loop

3:	movl	a0@+,a1@+	// copy next <k> longs
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+

	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+

	subl	#32,d0		// decrement loop count by k*4
	jge	3b

	btst	#1,d0		// copy last short
	jeq	4f
	movw	a0@+,a1@+

4:	btst	#0,d0		// copy last byte
	jeq	copy_exit
	movb	a0@,a1@

	bra	copy_exit

/*
 * Reverse copy
 */
rcopy:	addl	d0,a0		// point at end of source
	addl	d0,a1		// point at end of destination
	movl	a1,d1		// compute (dst & 3)
	andl	#3,d1
	jra	2f

1:	movb	a0@-,a1@-	// copy bytes to get src to long word boundary
	subql	#1,d0
2:	dbeq	d1,1b		// byte count or alignment count exhausted

	movl	d0,d1		// 4 bytes moved per 2 byte instruction
	andl	#0x1c,d1	// so instruction offset is:
	lsrl	#1,d1		// 2 * (k - n % k)
	negl	d1
	addl	#18,d1		// + fudge term of 2 for indexed jump
	jmp	pc@(0,d1)	// now dive into middle of unrolled loop

3:	movl	a0@-,a1@-	// copy next <k> longs
	movl	a0@-,a1@-
	movl	a0@-,a1@-
	movl	a0@-,a1@-

	movl	a0@-,a1@-
	movl	a0@-,a1@-
	movl	a0@-,a1@-
	movl	a0@-,a1@-

	subl	#32,d0		// decrement loop count by k*4
	jge	3b

	btst	#1,d0		// copy last short
	jeq	4f
	movw	a0@-,a1@-

4:	btst	#0,d0		// copy last byte
	jeq	copy_exit
	movb	a0@-,a1@-

	bra	copy_exit	// that's all folks!

copy_inval:
	movl	#EINVAL,d0
	PROCEXIT

	.globl _FAULT_ERROR
_FAULT_ERROR:
	movl	#EFAULT,d0
	PROCEXIT

copy_exit:
	clrl	d0
	PROCEXIT

	.globl	_ALLOW_FAULT_END
_ALLOW_FAULT_END:

	.globl	_fast_setjmp
_fast_setjmp:
	movl	sp@(4),a0
	movl	sp@,a0@
	movl	a6,a0@(44)
	movl	sp,a0@(48)
	clrl	d0
	rts

	.globl	_setjmp
_setjmp:
	movl	sp@(4),a0
	movl	sp@,a0@
	moveml	#0xfcfc,a0@(4)		// d2-d7, a2-a7
	clrl	d0
	rts

	.globl	_longjmp
_longjmp:
	movl	sp@(4),a0
	moveml	a0@(4),#0xfcfc		// d2-d7, a2-a7
	movl	a0@,sp@
	movl	#1,d0
	rts

	.data
	.globl	_probe_recover
_probe_recover:
	.long	0
	.text

PROCENTRY (probe_rb)
	movl	#1f,_probe_recover
	movl	a_p0,a0
	movb	a0@,d0
	clrl	_probe_recover
	movql	#1,d0
	bra	2f
1:
	clrl	_probe_recover
	clrl	d0
2:
	PROCEXIT

#if	MACH_LDEBUG && (NCPUS == 1)
/*
 * Debugging version in kern/lock.c used.
 */
#elif	(NCPUS == 1)
 	.globl	_simple_lock_init, _simple_lock, _simple_unlock
_simple_lock_init:
_simple_lock:
_simple_unlock:
	rts

	.globl	_simple_lock_try
_simple_lock_try:
	movl	#1,d0
	rts
#else	MACH_LDEBUG
	.globl	_simple_lock_init
_simple_lock_init:
PROCENTRY (simple_unlock)
	movl	a_p0,a0
	clrl	a0@
	PROCEXIT

PROCENTRY (simple_lock)
1:
	movl	a_p0,a0
	tas	a0@
	bne	1b
	PROCEXIT

PROCENTRY (simple_lock_try)
	clrl	d0
	movl	a_p0,a0
	tas	a0@
	bne	1f
	movl	#1,d0
1:	PROCEXIT
#endif	MACH_LDEBUG && (NCPUS == 1)

	.globl	_ovbcopy
_ovbcopy:
	jmp	_bcopy		// libc bcopy() handles overlap

	.globl	_blkclr
_blkclr:
	jmp	_bzero

#ifdef	notdef
PROCENTRY (em_chirp_build)
	movl	a_p0,a1
	movw	#0x3ff,d0		| make 1k points
	fmoveb	#0,fp0			| f0 = 0 = theta
	fmovecrx #0x00,fp1		| f1 = pi = delta theta
	fscales	#10,fp1			| f1 = 1024 Hz
	fscales	#-15,fp1		| f1 = 2pi/64k = pi/32k
	fmovex	fp1,fp2
	fscales	#-12,fp2
1:	fsinx	fp0,fp3			| f3 = sin theta
	faddx	fp1,fp0			| update theta
	faddx	fp2,fp1			| update freq
	fscales	#11,fp3			| scale to 12 bits
	fmovew	fp3,a1@			| store left result
	movw	a1@+,a1@+		| store right result
	dbra	d0,1b			| done?
	PROCEXIT
#endif

//----sched stuff----//
	.text

PROCENTRY (set_cpu_number)
#if	NCPUS == 1
	clrl	d0
#else	NCPUS == 1
	jsr	_cpu_number		// do it the hard way
#endif	NCPUS == 1
	movc	msp,d1			// store in low byte
	andl	#0xffffff00,d1
	orl	d0,d1
	movc	d1,msp			// save in msp
	PROCEXIT

/*
 *	Save and load kernel context by saving kernel register state in
 *	the pcb (user registers are saved in the kernel stack).
 *	We also save and restore the floating-point coprocessor here.
 */
 null_frame:
 	.long	0
	
	.globl	_save_context
_save_context:
	lea	_active_threads,a0	// current_thread()
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0
	movl	a0@(THREAD_PCB),a0	// pointer to pcb
	fsave	a0@(FPC_INT)		// save internal state
	tstb	a0@(FPC_INT)		// null state?
	beq	1f			// yes
//	fmovem	fpcr/fpsr/fpiar,a0@(FPC_EXT) // no, save external state too
	.long	0xf228bc00
	.word	FPC_EXT
//	fmovem	fp0-fp7,a0@(FPC_REGS)	// save registers
	.long	0xf228f0ff
	.word	FPC_REGS
	
	// 040 bug: restore a null frame before subsequent frestore
	nop
	frestore null_frame
1:
	spl7()				// spl7() -- lowered by load_context
	movw	d0,a0@(R_SR)		// save previous sr
	movl	sp@+,a1			// pop return address so saved sp
					// can be pushed again in load_context
	movl	a1,a0@(R_PC)		// where to resume on load_context
	moveml	#0xffff,a0@(REGS)	// save regs including kernel sp

	/*
	 *	We assume the caller won't trash anything deeper on
	 *	the stack than the return address on the top of the stack
	 *	(which we have saved in the pcb) before doing a load_context.
	 */
	clrl	d0			// return 0 to caller of save_context()
	jmp	a1@

	.globl	_load_context
_load_context:			/* (struct thread*) */
	addql	#1,_cnt+V_SWTCH
	movl	sp@(4),a0		// argument points to thread
	movl	a0@(THREAD_PCB),a0	// pointer to pcb
	tstb	a0@(FPC_INT)		// null state?
	beq	1f			// yes
//	fmovem	a0@(FPC_EXT),fpcr/fpsr/fpiar // no, restore external state
	.long	0xf2289c00
	.word	FPC_EXT
//	fmovem	a0@(FPC_REGS),fp0-fp7	// restore registers
	.long	0xf228d0ff
	.word	FPC_REGS
//	movl	a0,a1@(0,d0:w:4)	// mark this pcbs regs still loaded
1:
	// 040 bug: flush pending writes before frestore
	nop
	frestore a0@(FPC_INT)		// restore internal state
	cmpb	#MC68030,_cpu_type
	beq	2f
	movl	a0@(TT1),d0
//	movc	d0,itt1
	.long	0x4e7b0005
//	movc	d0,dtt1
	.long	0x4e7b0007
	bra	3f
2:
//	pmove	a0@(TT1),tt1		// set MMU tt1 reg
	.long	0xf0280c00
	.word	TT1
3:
#if	FIXME
#if	NCPUS == 1
	// FIXME: this really doesnt do anything!
	CPU_NUMBER_d0
	movc	msp,d1			// store in low byte
	andl	#0xffffff00,d1
	orl	d0,d1
	movc	d1,msp			// save in msp
#else	NCPUS == 1
	jsr	_set_cpu_number		// update cpu number
#endif	NCPUS == 1

//	lea	_active_threads,a1	// current_thread()
//	CPU_NUMBER_d0
//	movl	a1@(d0:w:4),a1
//	movc	a1,msp			// FIXME: someday update cache in msp
#endif	FIXME

	movl	a0@(R_SP),sp		// switch to new kernel stack
	movl	a0@(R_PC),sp@-		// restore return address
#if	NIPLMEAS
	clrl	d1
	movw	a0@(R_SR),d1
	splx(d1)
#else	NIPLMEAS
	movw	a0@(R_SR),sr		// restore new sr
#endif	NIPLMEAS
	cmpb	#MC68030,_cpu_type
	bne	5f
	movl	_cache,d0
	movc	d0,cacr			// clear cache and enable it
5:
	moveml	a0@(REGS),#0x7fff	// restore all except kernel sp
	movl	#1,d0			// return 1 to caller of save_context()
	rts

/* The following is used by the ptrace PT_SETFPREGS to restore fpregs */
PROCENTRY(loadfpregs)
	movl	a_p0,a0			/* Pointer to fp regs base */
//	fmovem	fp0-fp7,a0@+		/* restore regs */
	.long	0xf218d0ff
//	fmovem	fpcr/fpsr/fpiar,a0@+
	.long	0xf2109c00
	PROCEXIT
	
//----traps----//

#define	DO_ALIGNMENT	clrw	sp@-;

#if	NIPLMEAS

#define	INTR_CALL_LIT(funclit, arg) \
	movl	arg,sp@-; \
	movl	\#funclit,sp@-; \
	jsr	_intr_call; \
	addql	\#8,sp;

#define	INTR_CALL_REG(funcreg, arg) \
	movl	arg,sp@-; \
	movl	funcreg,sp@-; \
	jsr	_intr_call; \
	addql	\#8,sp;

#define	INTR_CALL_REG0(funcreg) \
	clrl	sp@-; \
	movl	funcreg,sp@-; \
	jsr	_intr_call; \
	addql	\#8,sp;

#else NIPLMEAS

#define	INTR_CALL_LIT(funclit, arg) \
	movl	arg,sp@-; \
	jsr	funclit; \
	addql	\#4,sp;

#define	INTR_CALL_REG(funcreg, arg) \
	movl	arg,sp@-; \
	jsr	funcreg@; \
	addql	\#4,sp;

#define	INTR_CALL_REG0(funcreg) \
	jsr	funcreg@;

#endif NIPLMEAS

#if	GDB
#define	IRSO	(16*4+4)		/* interrupt register save offset */

#if	NIPLMEAS
#define	IPL_INTR() \
	clrl	d0; \
	movw	sp@(R_SR),d0; \
	movl	d0,sp@-; \
	jsr	_ipl_intr;		/* ipl_intr(old_sr) */ \
	addql	\#4,sp;
#else	NIPLMEAS
#define	IPL_INTR()
#endif	NIPLMEAS

#define	TRAP(name, type, fsize) \
	.globl	name; \
name: \
	movw	\#fsize,sp@-;		/* set r_fsize */ \
	DO_ALIGNMENT; \
	moveml	\#0xffff,sp@-; \
	movc	usp,a0; \
	movl	a0,sp@(R_SP); \
	movl	sp,a6;			/* special fp for gdb */ \
	addl	\#1,a6; \
	subw	\#12,sp;		/* dummy fcode, rw and faultaddr */ \
	movl	\#type,sp@-; \
	jra	trap;

#define	TRAPVEC(name) \
	.globl	name; \
name: \
	clrw	sp@-;			/* zero r_fsize */ \
	DO_ALIGNMENT; \
	moveml	\#0xffff,sp@-; \
	movc	usp,a0; \
	movl	a0,sp@(R_SP); \
	movl	sp,a6;			/* special fp for gdb */ \
	addl	\#1,a6; \
	movw	sp@(VOR),d0; 		/* get trap type from vor */ \
	andl	\#0xfff,d0; \
	subw	\#12,sp;		/* dummy fcode, rw and faultaddr */ \
	movl	d0,sp@-; \
	jra	trap;

#define	INTRARG(name, vector, arg, ra) \
	.globl	name; \
name: \
	clrw	sp@-;			/* zero r_fsize */ \
	DO_ALIGNMENT; \
	moveml	\#0xffff,sp@-; \
	movc	usp,a0; \
	movl	a0,sp@(R_SP); \
	IPL_INTR (); \
	movl	sp,a6;			/* special fp for gdb */ \
	addl	\#1,a6; \
	movl	sp,a2;			/* save sp */ \
	cmpl	\#_eintstack,sp;	/* on interrupt stack? */ \
	jls	1f; \
	lea	_eintstack,sp;		/* switch to interrupt stack */ \
1:	INTR_CALL_LIT(vector, arg); \
	jra	ra;			/* sp & regs restored in intr_rtn */

#define	INTRSCAN(name, scanlist, scanarg, start, len) \
	.globl	name; \
name: \
	clrw	sp@-;			/* zero r_fsize */ \
	DO_ALIGNMENT; \
	moveml	\#0xffff,sp@-; \
	movc	usp,a0; \
	movl	a0,sp@(R_SP); \
	IPL_INTR (); \
	movl	sp,a6;			/* special fp for gdb */ \
	addl	\#1,a6; \
	movl	sp,a2;			/* save sp */ \
	cmpl	\#_eintstack,sp;	/* on interrupt stack? */ \
	jls	1f; \
	lea	_eintstack,sp;		/* switch to interrupt stack */ \
1:	movw	a2@(IRSO+0),d0;		/* push sr for timer */ \
	movl	d0,sp@-; \
	movl	a2@(IRSO+2),sp@-;	/* push pc */ \
2:	movl	_slot_id,a0; \
	addl	_intrstat,a0; \
	movl	a0@,d0;			/* see who's interrupting */ \
	andl	_intr_mask,d0;		/* scan only if enabled */ \
	bfffo	d0{\#start##:\#len##},d1;	/* scan all bits each time */ \
	jne	1f; \
	addql	\#8,sp;			/* pop off sr and pc */ \
	jmp	intr_rtn;		/* sp & regs restored in intr_rtn */ \
1:	subl	\#start,d1;		/* reset origin */ \
	lea	scanarg,a0; \
	movl	a0@(0,d1:w:4),a1;	/* arg to interrupt routine */ \
	lea	scanlist,a0; \
	movl	a0@(0,d1:w:4),a0; \
	INTR_CALL_REG(a0,a1); \
	bra	2b;			/* keep looking until no more */

#define	INTRPOLL(name, vector) \
	.globl	name; \
name: \
	clrw	sp@-;			/* zero r_fsize */ \
	DO_ALIGNMENT; \
	moveml	\#0xffff,sp@-; \
	movc	usp,a0; \
	movl	a0,sp@(R_SP); \
	IPL_INTR (); \
	movl	sp,a6;			/* special fp for gdb */ \
	addl	\#1,a6; \
	movl	sp,a2;			/* save sp */ \
	cmpl	\#_eintstack,sp;	/* on interrupt stack? */ \
	jls	1f; \
	lea	_eintstack,sp;		/* switch to interrupt stack */ \
1:	movl	a3,sp@-;		/* save a3 */ \
	movl	\#vector,a3;		/* list of routines to poll */ \
2:	movl	a3@+,a1; \
	INTR_CALL_REG0(a1); \
	tstl	d0;			/* interrupt serviced? */ \
	beqs	2b;			/* no, keep looking */ \
	movl	sp@+,a3;		/* restore a3 */ \
	jra	intr_rtn;		/* sp & regs restored in intr_rtn */

#else	GDB
#define	IRSO	(5*4)			/* interrupt register save offset */

#if	NIPLMEAS
#define	IPL_INTR() \
	clrl	d0; \
	movw	sp@(5*4),d0; \
	movl	d0,sp@-; \
	jsr	_ipl_intr;		/* ipl_intr(old_sr) */ \
	addql	\#4,sp;
#else	NIPLMEAS
#define	IPL_INTR()
#endif	NIPLMEAS

#define	TRAP(name, type, fsize) \
	.globl	name; \
name: \
	movw	\#fsize,sp@-;		/* set r_fsize */ \
	DO_ALIGNMENT; \
	moveml	\#0xffff,sp@-; \
	movc	usp,a0; \
	movl	a0,sp@(R_SP); \
	subw	\#12,sp;		/* dummy fcode, rw and faultaddr */ \
	movl	\#type,sp@-; \
	jra	trap;

#define	TRAPVEC(name) \
	.globl	name; \
name: \
	clrw	sp@-;			/* zero r_fsize */ \
	DO_ALIGNMENT; \
	moveml	\#0xffff,sp@-; \
	movc	usp,a0; \
	movl	a0,sp@(R_SP); \
	movw	sp@(VOR),d0; 		/* get trap type from vor */ \
	andl	\#0xfff,d0; \
	subw	\#12,sp;		/* dummy fcode, rw and faultaddr */ \
	movl	d0,sp@-; \
	jra	trap;

#define	INTRARG(name, vector, arg, ra) \
	.globl	name; \
name: \
	moveml	\#0xc0e0,sp@-;		/* save d0,d1,a0,a1,a2 */ \
	IPL_INTR (); \
	movl	sp,a2;			/* save sp */ \
	cmpl	\#_eintstack,sp;	/* on interrupt stack? */ \
	jls	1f; \
	lea	_eintstack,sp;		/* switch to interrupt stack */ \
1:	INTR_CALL_LIT(vector, arg); \
	jra	ra;			/* sp & regs restored in intr_rtn */

#define	INTRSCAN(name, scanlist, scanarg, start, len) \
	.globl	name; \
name: \
	moveml	\#0xc0e0,sp@-;		/* save d0,d1,a0,a1,a2 */ \
	IPL_INTR (); \
	movl	sp,a2;			/* save sp */ \
	cmpl	\#_eintstack,sp;	/* on interrupt stack? */ \
	jls	1f; \
	lea	_eintstack,sp;		/* switch to interrupt stack */ \
1:	movw	a2@(IRSO+0),d0;		/* push sr for timer */ \
	movl	d0,sp@-; \
	movl	a2@(IRSO+2),sp@-;	/* push pc */ \
2:	movl	_slot_id,a0; \
	addl	_intrstat,a0; \
	movl	a0@,d0;			/* see who's interrupting */ \
	andl	_intr_mask,d0;		/* scan only if enabled */ \
	bfffo	d0{\#start:\#len},d1;	/* scan all bits each time */ \
	jne	1f; \
	addql	\#8,sp;			/* pop off sr and pc */ \
	jmp	intr_rtn;		/* sp & regs restored in intr_rtn */ \
1:	subl	\#start,d1;		/* reset origin */ \
	lea	scanarg,a0; \
	movl	a0@(0,d1:w:4),a1;	/* arg to interrupt routine */ \
	lea	scanlist,a0; \
	movl	a0@(0,d1:w:4),a0; \
	INTR_CALL_REG(a0,a1); \
	bra	2b;			/* keep looking until no more */

#define	INTRPOLL(name, vector) \
	.globl	name; \
name: \
	moveml	\#0xc0e0,sp@-;		/* save d0,d1,a0,a1,a2 */ \
	IPL_INTR (); \
	movl	sp,a2;			/* save sp */ \
	cmpl	\#_eintstack,sp;	/* on interrupt stack? */ \
	jls	1f; \
	lea	_eintstack,sp;		/* switch to interrupt stack */ \
1:	movl	a3,sp@-;		/* save a3 */ \
	movl	\#vector,a3;		/* list of routines to poll */ \
2:	movl	a3@+,a1; \
	INTR_CALL_REG0(a1); \
	tstl	d0;			/* interrupt serviced? */ \
	beqs	2b;			/* no, keep looking */ \
	movl	sp@+,a3;		/* restore a3 */ \
	jra	intr_rtn;		/* sp & regs restored in intr_rtn */
#endif	GDB

	TRAPVEC(stray)
	TRAPVEC(badtrap)

	TRAP(illegal, T_ILLEGAL, 0)
	TRAP(zerodiv, T_ZERODIV, 4)
	TRAP(check, T_CHECK, 4)
	TRAP(trapv, T_TRAPV, 4)
	TRAP(privilege, T_PRIVILEGE, 0)
	TRAP(trace, T_TRACE, 4)
	TRAP(emu1010, T_EMU1010, 0)
	TRAP(emu1111, T_EMU1111, 0)
	TRAP(coproc, T_COPROC, 12)
	TRAP(format, T_FORMAT, 0)
	TRAP(badintr, T_BADINTR, 0)
	TRAP(spurious, T_SPURIOUS, 0)
	TRAP(breakpt, T_USER_BPT, 0)
	TRAP(fpp, T_FPP, 0)
	TRAP(mmu_config, T_MMU_CONFIG, 0)
	TRAP(mmu_ill, T_MMU_ILL, 0)
	TRAP(mmu_access, T_MMU_ACCESS, 0)

	INTRARG(ipl1, _softint_run, #0, softint_rtn)
	INTRARG(ipl2, _softint_run, #1, softint_rtn)
	INTRSCAN(ipl3, _ipl3_scan, _ipl3_arg, I_IPL3_BASE, I_IPL3_BITS)
	INTRSCAN(ipl4, _ipl4_scan, _ipl4_arg, I_IPL4_BASE, I_IPL4_BITS)
	INTRPOLL(ipl5, _poll_intr)
	INTRSCAN(ipl6, _ipl6_scan, _ipl6_arg, I_IPL6_BASE, I_IPL6_BITS)
	INTRSCAN(ipl7, _ipl7_scan, _ipl7_arg, I_IPL7_BASE, I_IPL7_BITS)

	.globl	_call_nmi
_call_nmi:
	subl	d0,d0
	movw	a2@(IRSO+0),d0		// push sr
	movl	d0,sp@-
	movl	a2@(IRSO+2),sp@-	// push pc
	movl	sp,sp@-			// push isp
	movc	usp,a0
	movl	a0,sp@-			// push usp
	movl	a2,sp@-			// push ksp
	jsr	_nmi
	addl	#20,sp
	rts

//
//	trap3 is used for fast Mach system calls.
//
//	Calling conventions are as follows:
//
//	d0	contains the system call number.
//	d1-d7	contains arguments.
//
//	On exit
//
//	d0	contains return value (a kern_return_t)
//
//	All registers are saved and restored (a big hit) to make
//	thread_get_state and thread_set_state work properly.  These
//	could perhaps still work without saving/restoring registers
//	if we committed to forcing the thread to take an AST and
//	get its registers saved that way.
//
	.globl	trap3
trap3:
//
//	Save necessary state
//
	addql	#1,_cnt+V_SYSCALL
	subqw	#4,sp			// space for r_fsize
	moveml	#0xffff,sp@-		// save regs
	movc	usp,a0			// grab usp
	movl	a0,sp@(R_SP)		// and save it
//
//	Read in args and call handler.  NOTE:  depends on the
//	fact that a entry in the syscall table is 8 bytes!
//
	tstl	d0			// syscall < 0?
	blt	3f			// yes, bad
	cmpl	_mach_trap_count,d0	// mach_trap_count < syscall number?
	bge	3f			// yes, bad
	lea	_mach_trap_table,a0	// get start of table
	lea	a0@(d0:w:8),a0		// point at entry
	movl	a0@,d0			// grab arg count
	movl	d0,a2			// save for safe keeping
	lea	argc_table,a1		// table pointer
	movl	a1@(d0:w:4),a1		// entry in table
	jmp	a1@			// jump indirect
3:	movl	#KERN_INVALID_ARGUMENT,d0
	jmp	5f
	.data
argc_table:
	.long	argc_0
	.long	argc_1
	.long	argc_2
	.long	argc_3
	.long	argc_4
	.long	argc_5
	.long	argc_6
	.long	argc_7
	.text
//	Following could perhaps be better done with moveml's
argc_7:	movl	d7,sp@-			// push arg and fall through
argc_6:	movl	d6,sp@-			// push arg and fall through
argc_5:	movl	d5,sp@-			// push arg and fall through
argc_4:	movl	d4,sp@-			// push arg and fall through
argc_3:	movl	d3,sp@-			// push arg and fall through
argc_2:	movl	d2,sp@-			// push arg and fall through
argc_1:	movl	d1,sp@-			// push arg and fall through
argc_0:
	movl	a0@(4),a0		// grab function pointer
#ifdef DEBUG
	movl	a0,_lastsyscall
#endif DEBUG
	jsr	a0@			// go for it
	movl	a2,d7			// get count again
	lsll	#2,d7			// convert to long count
	addw	d7,sp			// pop args off stack
	movl	d0,sp@(R_D0)		// save return code for later
//
//	Execute return sequence.
//
//
//	NOTE:  this does not check for signals... this assumes that
//	anything that uses this trap handler will not generate
//	a signal that it expects to see immediately.  What should
//	really be done is that the signal mechanism should cause
//	an AST to occur rather than expect the syscall handler
//	to check for it (the same is true of the exit code).
//
5:
	lea	_active_threads,a0	// get pointer to pcb
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0
	movl	a0,a1
	movl	a0@(THREAD_PCB),a0
	cmpl	#AST_ZILCH,a1@(THREAD_AST)	// normal exit?
	bne	7f			// no, force ast
	movl	a1@(UTASK),a1		// point to task U-area
	movl	a1@(U_PROCP),a1		// then to proc table slot
	tstb	a1@(P_CURSIG)		// check cursig
	bne	7f			// there's a sig, cause AST
	tstl	a1@(P_SIG)		// test sig bits
	beq	8f			// zero -> no sig
7:	bset	#AST_SCHED_BIT,a0@(AST)	// force AST and fall through
8:	movw	sr,d0
	movw	#0x2700,sr
	bclr	#AST_SCHED_BIT,a0@(AST)	// reschedule?
	jeq	9f			// no
	bset	#TRACE_AST_BIT,a0@(AST)	// AST requested tracing
	jne	9f			// AST already tracing
	bset	#SR_TSINGLE_BIT-8,sp@(R_SR)
	jeq	9f			// wasnt set by user previously
	bset	#TRACE_USER_BIT,a0@(AST)// was set by user previously
9:	movw	d0,sr
#if	NIPLMEAS
	clrl	d0
	movw	sp@(R_SR),d0
	movl	d0,sp@-
	jsr	_ipl_urte		// ipl_urte(new_sr)
	addql	#4,sp
#endif	NIPLMEAS
	movl	sp@(R_SP),a0		// restore usp
	movc	a0,usp
	moveml	sp@,#0x7fff		// restore regsiters (exc. kernel sp)
	addw	#R_SR,sp		// pop register frame and r_fsize
	rte				// return to user

//
//	trap4 is used for fast Unix system calls.
//
//	Calling conventions are as follows:
//
//	d0	contains the system call number.
//	d1-d7	contains arguments.
//
//	On exit
//
//	d0	contains u.u_rval1 or if condition code has
//		carry set contains u.u_error.
//	d1	contains u.u_rval2
//
//
	.globl	trap4
trap4:
//
//	Save necessary state
//
	addql	#1,_cnt+V_SYSCALL
	subqw	#4,sp			// space for r_fsize
	moveml	#0xffff,sp@-		// save regs
	movc	usp,a0			// grab usp
	movl	a0,sp@(R_SP)		// and save it
#ifdef DEBUG
	movl	sp@(R_D0),_lastsyscall
#endif DEBUG
	jsr	_unix_syscall		// call handler
//
//	Execute return sequence (signals were checked in C).
//
5:
	lea	_active_threads,a0	// get pointer to pcb
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0
	movl	a0,a1
	movl	a0@(THREAD_PCB),a0
	cmpl	#AST_ZILCH,a1@(THREAD_AST)	// normal exit?
	beq	6f			// yes, continue
	bset	#AST_SCHED_BIT,a0@(AST)	// force AST and fall through
6:	movw	sr,d0
	movw	#0x2700,sr
	bclr	#AST_SCHED_BIT,a0@(AST)	// reschedule?
	jeq	9f			// no
	bset	#TRACE_AST_BIT,a0@(AST)	// AST requested tracing
	jne	9f			// AST already tracing
	bset	#SR_TSINGLE_BIT-8,sp@(R_SR)
	jeq	9f			// wasnt set by user previously
	bset	#TRACE_USER_BIT,a0@(AST)// was set by user previously
9:	movw	d0,sr
#if	NIPLMEAS
	clrl	d0
	movw	sp@(R_SR),d0
	movl	d0,sp@-
	jsr	_ipl_urte		// ipl_urte(new_sr)
	addql	#4,sp
#endif	NIPLMEAS
	movl	sp@(R_SP),a0		// restore usp
	movc	a0,usp
	moveml	sp@,#0x7fff		// restore registers (exc. kernel sp)
	addw	#R_SR,sp		// pop register frame and r_fsize
	rte				// return to user

//
//	traps 5 and 6 are used to retrieve/store a per thread register.
//	While it would nice to have real language support for this, I
//	am in a weak mood and will implement it this way.
//
	.globl	trap5
trap5:					// get users register
	lea	_active_threads,a0
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0		// current thread
	movl	a0@(THREAD_PCB),a0	// current pcb
	movl	a0@(THREAD_USER_REG),d0	// user's register
	rte				// done

	.globl	trap6
trap6:					// set users register
	movl	d0,d1
	lea	_active_threads,a0
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0		// current thread
	movl	a0@(THREAD_PCB),a0	// current pcb
	movl	d1,a0@(THREAD_USER_REG)	// user's register
	rte				// done

/* old UNIX syscall handler */
	.globl	trap0, _active_threads
trap0:
	addql	#1,_cnt+V_SYSCALL
	subqw	#4,sp			// space for r_fsize
					// FIXME: really save d0,d1,a0,a1?
	moveml	#0xffff,sp@-		// save regs
	movc	usp,a0			// save usp
	movl	a0,sp@(R_SP)
	lea	_active_threads,a1	// catch fault getting syscall code
	CPU_NUMBER_d0
	movl	a1@(d0:w:4),a2
	movl	#syscall_fault,a2@(THREAD_RECOVER)
	movl	#FC_USERD,d0		// set source func code
	movc	d0,sfc
	movsl	a0@,d0			// get syscall code

syscall_recover:
	clrl	a2@(THREAD_RECOVER)
	movl	d0,sp@-			// pass syscall code to old_syscall()
#ifdef DEBUG
	movl	d0,_lastsyscall
#endif DEBUG
	jsr	_old_syscall
	addqw	#4,sp
syscall_rtn:
	lea	_active_threads,a0	// get pointer to pcb
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0
	movl	a0@(THREAD_PCB),a0
	movw	sr,d0
	movw	#0x2700,sr
	bclr	#AST_SCHED_BIT,a0@(AST)	// reschedule?
	jeq	1f			// no
	bset	#TRACE_AST_BIT,a0@(AST)	// AST requested tracing
	jne	1f			// AST already tracing
	bset	#SR_TSINGLE_BIT-8,sp@(R_SR)
	jeq	1f			// wasnt set by user previously
	bset	#TRACE_USER_BIT,a0@(AST)// was set by user previously
1:	movw	d0,sr
	movl	sp@(R_SP),a0		// restore usp
	movc	a0,usp
					// FIXME: really restore d0,d1,a0,a1?
#if	NIPLMEAS
	clrl	d0
	movw	sp@(R_SR),d0
	movl	d0,sp@-
	jsr	_ipl_urte		// ipl_urte(new_sr)
	addql	#4,sp
#endif	NIPLMEAS
	moveml	sp@,#0x7fff		// restore all (except kernel sp)
	addw	#R_SR,sp		// pop register frame, including r_fsize
	rte

syscall_fault:
	movl	#63,d0			// illegal syscall code
	jra	syscall_recover

/* exception handlers */
	.globl	addrerr
addrerr:
	subqw	#4,sp			// room for r_fsize
	moveml	#0xffff,sp@-		// save regs
	movc	usp,a0			// save usp
	movl	a0,sp@(R_SP)
	movl	sp,a6			// special fp for gdb
	addl	#1,a6
	bfextu	sp@(FMT){#0:#4},d0	// extract format bits
	cmpl	#EF_NORMAL6,d0		// 040 only
	bne	1f
	movw	#SIZEOF_NORMAL6,sp@(FSIZE) // remember size of exception frame
	bra	3f
1:
	cmpl	#EF_SHORTBUS,d0		// 030 only
	bne	2f
	movw	#SIZEOF_SHORTBUS,sp@(FSIZE) // remember size of exception frame
	bra	3f
2:
	cmpl	#EF_LONGBUS,d0		// 030 only
	bne	4f
	movw	#SIZEOF_LONGBUS,sp@(FSIZE) // remember size of exception frame
3:
	subw	#12,sp			// dummy fcode, rw and faultaddr
	movl	#T_ADDRERR,sp@-
	jra	trap
4:
	pea	1f
	jsr	_panic
	.data
1:	.asciz	"addrerr: bad exception stack format"
	.even
	.text

	.globl	buserr
buserr:
	subqw	#4,sp			// room for r_fsize
	moveml	#0xffff,sp@-		// save regs
	movc	usp,a0			// save usp
	movl	a0,sp@(R_SP)
	movl	sp,a6			// special fp for gdb
	addl	#1,a6
	bfextu	sp@(FMT){#0:#4},d0	// extract format bits
	cmpl	#EF_ACCESS,d0		// 040 only
	jeq	access
	cmpl	#EF_SHORTBUS,d0		// 030 only
	jeq	short_bus
	cmpl	#EF_LONGBUS,d0		// 030 only
	jeq	long_bus
	pea	1f
	jsr	_panic
	.data
1:	.asciz	"buserr: bad exception stack format"
	.even
	.text

badfault:
	pea	1f
	jsr	_panic
	.data
1:	.asciz	"buserr: no fault indicated"
	.even
	.text

access:
	movw	#SIZEOF_ACCESS,sp@(FSIZE) // remember size of exception frame
	movl	#T_BUSERR,d2		// assume bus error
	movl	sp@(FAULTADDR_040),a0	// fault address
	movl	#VM_PROT_READ+VM_PROT_WRITE,d0	// read/write
	btst	#RW_BIT,sp@(SSW_040)
	beq	1f
	btst	#LK_BIT,sp@(SSW_040)
	bne	1f
	movl	#VM_PROT_READ,d0
1:	movw	sp@(SSW_040),d1		// function code (transfer mode)
	andl	#SS_TM,d1
	btst	#ATC_BIT,sp@(SSW_040)	// MMU fault?
	beq	2f			// no
	movl	#T_MMU_INVALID,d2	// indicate MMU error
	btst	#MA_BIT,sp@(SSW_040)	// misaligned fault addr is +4
	beq	2f
	addql	#4,a0
2:	movl	a0,sp@-			// push fault address
	movl	d0,sp@-			// push rw
	movl	d1,sp@-			// push fcode
	movl	d2,sp@-			// push trap type
	jra	trap
	
short_bus:
	movw	#SIZEOF_SHORTBUS,sp@(FSIZE) // remember size of exception frame
	btst	#FAULTD_BIT-8,sp@(SSW)	// data fault?
	bne	data_fault
	btst	#FAULTB_BIT-8,sp@(SSW)	// stage B fault?
	beq	1f
	movl	sp@(R_PC),a0		// yes, fault address is r_pc + 4
	addqw	#4,a0
	bra	inst_fault
1:	btst	#FAULTC_BIT-8,sp@(SSW)	// stage C fault?
	beq	badfault
	movl	sp@(R_PC),a0		// yes, fault address is r_pc + 2
	addqw	#2,a0
	bra	inst_fault

long_bus:
	movw	#SIZEOF_LONGBUS,sp@(FSIZE) // remember size of exception frame
	btst	#FAULTD_BIT-8,sp@(SSW)	// data fault?
	bne	data_fault
	btst	#FAULTB_BIT-8,sp@(SSW)	// stage B fault?
	beq	1f
	movl	sp@(STAGEB),a0		// yes, fault address is e_stageb
	bra	inst_fault
1:	btst	#FAULTC_BIT-8,sp@(SSW)	// stage C fault?
	beq	badfault
	movl	sp@(STAGEB),a0		// yes, fault address is e_stageb - 2
	subqw	#2,a0
	// fall into ...

inst_fault:
	movl	#1,d2			// indicate read
	movl	#FC_USERI,d1
	btst	#SR_SUPER_BIT-8,sp@(R_SR) // super mode?
	beq	call_trap
	movl	#FC_SUPERI,d1
	bra	call_trap

data_fault:
	movl	sp@(FAULTADDR),a0	// fault address is r_faultaddr
	movw	sp@(SSW),d1		// extract fcode
	andl	#SS_FCODE,d1
	bfextu	sp@(SSW+1){#0:#2},d2	// extract rmw & rw
	// fall into ...

call_trap:
	btst	#1,d2			// read/modify/write cycle?
	bne 	1f
	btst	#0,d2			// read cycle?
	beq 	1f
	movl	#VM_PROT_READ,d0
//	ptest	d1,a0@,7,READ		// see if MMU caused buserr
	.long	0xf0109e09
	bra	2f
1:	movl	#VM_PROT_READ+VM_PROT_WRITE,d0
//	ptest	d1,a0@,7,WRITE		// see if MMU caused buserr
	.long	0xf0109c09
2:	movl	a0,sp@-			// push fault address
	movl	d0,sp@-			// push rw
	movl	d1,sp@-			// push fcode
//	pmove	sr,mmu_status
	.long	0xf0396200
	.long	mmu_status
	bfffo	mmu_status{#0:#6},d0
	beq	not_mmu			// MMU did not cause buserr
	lea	mmu_type,a0
	movl	a0@(0,d0:w:4),sp@-	// push MMU trap type
	bra	trap
not_mmu:
	movl	#T_BUSERR,sp@-		// push trap type
	bra	trap

mmu_type:
	.long	T_MMU_BUSERR
	.long	T_MMU_LIMIT
	.long	T_MMU_SUPER
	.long	T_STRAY
	.long	T_MMU_WRITE
	.long	T_MMU_INVALID

	.data
mmu_status:
	.word	0
	.text

trap:
	addql	#1,_cnt+V_TRAP
	jsr	_trap
	addl	#16,sp			// pop type, fcode, rw and faultaddr
	// fall into ...

/*
 * Return from a trap:
 *	- check for AST's.
 *	- d0 is {TRAP_RERUN, TRAP_NORERUN} -- fixup stack as required.
 */
trap_rtn:
	movl	d0,d2			// save rerun mode
	btst	#SR_SUPER_BIT-8,sp@(R_SR) // returning to SUPER mode?
	bne	2f			// yes, dont deal with ASTs
	lea	_active_threads,a0	// get pointer to pcb
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0
	movl	a0@(THREAD_PCB),a0
	movw	sr,d0
	movw	#0x2700,sr
	bclr	#AST_SCHED_BIT,a0@(AST)	// reschedule?
	jeq	1f			// no
	bset	#TRACE_AST_BIT,a0@(AST)	// AST requested tracing
	jne	1f			// AST already tracing
	bset	#SR_TSINGLE_BIT-8,sp@(R_SR)
	jeq	1f			// wasnt set by user previously
	bset	#TRACE_USER_BIT,a0@(AST)// was set by user previously
1:	movw	d0,sr
	movl	sp@(R_SP),a0		// restore usp
	movc	a0,usp
2:
	tstl	d2			// TRAP_RERUN?
	beq	5f			// no, fixup stack not to rerun cycle
#if	NIPLMEAS
	clrl	d0
	movw	sp@(R_SR),d0
	movl	d0,sp@-
	jsr	_ipl_rte		// ipl_rte(new_sr)
	addql	#4,sp
#endif	NIPLMEAS
	moveml	sp@,#0x7fff		// restore all (except ssp)
	addw	#R_SR,sp		// pop register frame, including r_fsize
	rte				// rerun failing bus cycle(s)

	//
	// The following assumes that enough exception information is
	// being poped that the old sr/pc/format/vor will not get stepped on
	// as its being copied.
	//

5:	movl	sp,a0			// top of old stack
 	addw	sp@(FSIZE),a0		// advance by amount to get rid of
	movl	sp@(R_PC),a0@(R_PC)	// copy pc
	movw	sp@(R_SR),a0@(R_SR)	// copy sr
	clrw	a0@(VOR)		// EF_NORMAL4 format
	movl	a0,sp@(R_SP)		// new ksp (usp already restored)
#if	NIPLMEAS
	clrl	d0
	movw	sp@(R_SR),d0
	movl	d0,sp@-
	jsr	_ipl_rte		// ipl_rte(new_sr)
	addql	#4,sp
#endif	NIPLMEAS
	moveml	sp@,#0xffff		// restore all including new ksp
	addw	#R_SR,sp		// pop register frame, including f_size
	rte

intr_rtn:
#if	EVENTMETER
	jsr	_event_intr
#endif	EVENTMETER
	addql	#1,_cnt+V_INTR
softint_rtn:
	movl	a2,sp			// restore sp
	btst	#SR_SUPER_BIT-8,sp@(IRSO)// returning to SUPER mode?
	bne	2f			// yes, dont deal with ASTs
	lea	_active_threads,a0	// get pointer to pcb
	CPU_NUMBER_d0
	movl	a0@(d0:w:4),a0
	movl	a0@(THREAD_PCB),a0
	movw	sr,d0
	movw	#0x2700,sr
	bclr	#AST_SCHED_BIT,a0@(AST)	// reschedule?
	jeq	1f			// no
	bset	#TRACE_AST_BIT,a0@(AST)	// AST requested tracing
	jne	1f			// AST already tracing
	bset	#SR_TSINGLE_BIT-8,sp@(IRSO)
	jeq	1f			// wasnt set by user previously
	bset	#TRACE_USER_BIT,a0@(AST)// was set by user previously
1:	movw	d0,sr
2:
#if	GDB
#if	NIPLMEAS
	clrl	d0
	movw	sp@(R_SR),d0
	movl	d0,sp@-
	jsr	_ipl_rte		// ipl_rte(new_sr)
	addql	#4,sp
#endif	NIPLMEAS
	moveml	sp@,#0x7fff
	addw	#R_SR,sp		// pop register frame, including f_size
#else	GDB
#if	NIPLMEAS
	clrl	d0
	movw	sp@(5*4),d0
	movl	d0,sp@-
	jsr	_ipl_rte		// record crossing ipl boundary
	addql	#4,sp
#endif	NIPLMEAS
	moveml	sp@+,#0x0703		// restore d0,d1,a0,a1,a2
#endif	GDB
	rte

#if NIPLMEAS
	.globl	_splu_measured
_splu_measured:					// old_ipl=splu_measured(new_sr)
	jsr	_ipl_splu			// old_ipl = ipl_splu(pc,new_sr)
	movl	sp@(4),d1			// get new sr
	movw	d1,sr				// sr = new_sr
	rts

	.globl	_spld_measured
_spld_measured:					// old_ipl=spld_measured(new_sr)
	jsr	_ipl_spld			// old_ipl = ipl_spld(pc,new_sr)
	movl	sp@(4),d1			// get new sr
	movw	d1,sr				// sr = new_sr
	rts

#ifdef notdef
	.globl	_Rspl7
_Rspl7:
	clrl	d0
	movw	sr,d0
	movw	#0x2700,sr
	rts

	.globl	_Rsplx
_Rsplx:
	clrl	d0
	movw	sr,d0
	movl	sp@(4),d1
	movw	d1,sr
	rts
#endif notdef
#endif NIPLMEAS

//	Cache flush trap for Sun binary compatibility
trap2:
	cmpb	#MC68030,_cpu_type
	beq	4f
#if	D25B || D43B
	nop
#endif	D25B || D43B
//	cpusha	bc
	.word	0xf4f8
	rte
4:
	movl	_cache,d0
	movc	d0,cacr			// clear cache and enable it
	rte
