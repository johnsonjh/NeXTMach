/*	@(#)fpc.h	1.0	03/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 12-Mar-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 **********************************************************************
 */ 

/*
 *	68881/68882 Floating-Point Coprocessor registers
 */

#ifndef	_FPC_
#define	_FPC_

#ifndef	ASSEMBLER

struct fpc_external {			/* externally visible state */
	struct fpc_regs {
		int	fp[3];		/* 96-bit extended format */
	} regs[8];
	int	cr;			/* control */
	int	sr;			/* status */
	int	iar;			/* instruction address */
	u_char	state;			/* execution state */
#define	FPC_NULL	0		/* never used */
#define	FPC_IDLE	1		/* instruction boundary */
#define	FPC_BUSY	2		/* mid-instruction */
};

struct fpc_internal {			/* internal state */
	u_char	fpc_version;		/* microcode version # */
	u_char	fpc_framesize;		/* sizeof fpc_frame */
#define	FPC_NULLSIZE	0
#define	FPC_IDLESIZE	56
	u_short	fpc_reserved;
#define	FPC_MAXFRAME	212		/* worst-case frame size */
	u_char	fpc_frame[FPC_MAXFRAME]; /* internal state frame */
};

#endif	ASSEMBLER

#endif	_FPC_
