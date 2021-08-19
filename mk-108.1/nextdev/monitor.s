/*	@(#)monitor.s	1.0	21March88	(c) 1988 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 21-Mar-88  Gregg Kellogg  (gk) at NeXT.
 *	Created.
 *
 **********************************************************************
 */

#import <next/cframe.h>
#import <next/cpu.h>
#import <nextdev/monreg.h>
#import <next/spl.h>

	.data
minit:	.long	0
	.text

PROCENTRY(mon_reset)
	movl	#P_MON,a1		// use monitor interface
	tstl	minit
	bne	1f
	movl	#RESET,a1@		// reset monitor interface
	movl	#1,minit
1:	PROCEXIT

	.globl	_mon_send
_mon_send:
	movl	#P_MON,a1		// use monitor interface
	bra	1f

	.globl	_lpr_send
_lpr_send:
	movl	#P_PRINTER,a1		// use lazer printer interface
	bra	1f

/*
 * Send a command (with data) down either the printer or monitor interface
 */
//	Notes:
//	
//	1. This if a sound out DMA is in progress, this routines waits for
//	   2 sound out packets to be transmitted before sending the command.
//	   The packet will be sent in the ~6 us gap between DMA out packets.
//
//	2. This code was lifteed verbatim (and translated into Unix assy)
//	   from D. Gallatin's monitor PROM code. It may be righteously be
//	   considered to be black magic. It seems to work.
//
//      3. On return, the packet has been completely shifted out of the
//	   DMA chip and has supposedly been received by the LPM in the monitor.
//
#define	SAVED_D2	-4
#define SAVED_A2	-8
#define SAVED_IPL	-12
#define SAVED_BYTES	(-(SAVED_IPL))
#define MAXLOOP		100

1:				// interface to use in a1
	link	a6,#-SAVED_BYTES
	movl	d2,a6@(SAVED_D2)
	movl	a2,a6@(SAVED_A2)
	splhigh()
	movl	d0,d2
	lea	1f,a2
	movl	a6,a0
	subl	#(-(SAVED_IPL)+4),a0
	bra	2f			// preload move to data into cache
					// byproduct is to save d0 (saved ipl)
1:
	movl	a1,a0			// next time through use monitor
	lea	3f,a2

	/* Load up arguments */
	movl	a6@(0x8),d1		// command
	movl	a6@(0xc),d2		// data
/*
 * Wait for any previous command to complete transmission.
 * Since DMA won't be doing this, we assume we're the only one
 * trying right now.  Maximum latency ~= 8.6 usec.
 */

	movl	#MAXLOOP,d0		// only wait a while
1:
	btst	#5,a1@(2)		// ctx_pend clear?
	beq	1f
	btst	#4,a1@(2)		// ctx clear?
	beq	1f
	dbf	d0,1b			// loop back
1:

/*
 * Prime the command.  This allows us to send things out with
 * just the write to the data register.
 */
	movb	d1, a1@(3)

/*
 * If DMA out is enabled then wait for negative transition of
 * dtx before sending data so we don't collide.  We do this at
 * splhigh to assure us that we can squirt the command out
 * right after the DMA is done.  This keeps us from getting
 * underrun errors, which shuts down DMA, and is messy.
 * Maximum latency once DMA starts sending stuff is ~= 8.6 usec.
 * How long we will have to sit here depends on how long it
 * takes the DMA to get the next data request.
 */
	btst	#7,a1@
	beq	2f

/*
 * Wait for two dma transmits to ensure everything's in the cache
 */
	movl	#1,d1
4:

/*
 * Wait for dtx to go high
 */
	movl	#MAXLOOP,d0		// only wait a while
1:
	btst	#6,a1@(2)		// dtx clear?
	dbne	d0,1b

/*
 * Wait for dtx to go low
 */
1:
	btst	#6,a1@(2)		// dtx set?
	bne	1b
	dbf	d1, 4b			// go back for 2nd pass while in cache
2:
	movl	d2,a0@(4)		// send the data (a0 == a1 or fp)
	jmp	a2@
3:
	movl	a6@(SAVED_D2),d2	// restore saved values
	movl	a6@(SAVED_A2),a2
	movl	a6@(SAVED_IPL),d1
	splx(d1)

	clrl	d0
	unlk	a6
	rts

//
//	This routine ANDs the passed byte into the high byte of the sound 
//	out/in CSR by waiting for a DMA packet to be sent and doing the andb
//	immediately afterwards.
//
//	The algorithm is the same as in pack_2_mon(), above.
//
//	Passed: u_char and_byte;	-- actually passed an an int
//	returns: Nothing
//

	.globl	_mon_csr_and
_mon_csr_and:
   	movel	#P_MON,a0
	bra	1f

	.globl	_lpr_csr_and
_lpr_csr_and:
	movel	#P_PRINTER,a0
	bra	1f

1:
	movel	d2,sp@-
	
	// saved d2 = sp@
	// saved PC = sp@(4)
	// (int)  and_byte = sp@(8)
	// (char) and_byte = sp@(0xb)

	movel	sp@(8),d2		// operand...
	movw	sr, d1			// disable interrupts
	orw	#0x700, sr
	btst	#7,a0@
	beq	9f			// if dma out is off then just 
					// do the AND
	lea	a0@(2),a1		// rtx/ctx/dtx register
	
	// wait for 1 DMA packet to be sent (but don't wait forever...)
	
mca_wt:
	movew	#MAXLOOP, d0		// only wait for a while
1:
	btst	#6, a1@			// wait till dma tx in progress
	dbne	d0, 1b
2:	
	btst	#6, a1@			// wait till dma tx not in progress
	bne	2b

9:
	andb	d2,a0@			// mon_csr_ptr &= and_long
	movew	d1, sr			// restore interrupt level
	movel	sp@+,d2
	rts
	
//
//	This routine ORs the passed byte into the high byte of the sound 
//	out/in CSR by waiting for a DMA packet to be sent and doing the orb
//	immediately afterwards.
//
//	The algorithm is the same as in pack_2_mon(), above.
//
//	Passed: u_int or_byte;		-- actually passed as an int
//	returns: Nothing
//

	.globl	_mon_csr_or
_mon_csr_or:
   	movel	#P_MON,a0
	bra	1f

	.globl	_lpr_csr_or
_lpr_csr_or:
	movel	#P_PRINTER,a0
	bra	1f

1:
	movel	d2,sp@-
	
	// saved d2 = sp@
	// saved PC = sp@(4)
	// (int)  and_byte = sp@(8)
	// (char) and_byte = sp@(0xb)

	movel	sp@(8),d2		// operand...
	movw	sr, d1			// disable interrupts
	orw	#0x700, sr
	btst	#7,a0@
	beq	9f			// if dma out is off then just 
					// do the AND
	lea	a0@(2),a1		// rtx/ctx/dtx register
	
	// wait for 1 DMA packet to be sent (but don't wait forever...)
	
	movew	#MAXLOOP, d0		// only wait for a while
1:
	btst	#6, a1@			// wait till dma tx in progress
	dbne	d0, 1b
2:	
	btst	#6, a1@			// wait till dma tx not in progress
	bne	2b

9:
	orb	d2,a0@			// mon_csr_ptr &= and_long
	movew	d1, sr			// restore interrupt level
	movel	sp@+,d2
	rts

