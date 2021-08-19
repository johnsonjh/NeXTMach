/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 17-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Added astoff() for new scheduler.
 */ 

#ifndef	_PCB_
#define	_PCB_

#import <next/pmap.h>
#import <next/reg.h>
#import <next/fpc.h>
#import <next/mmu.h>

/*
 * NeXT pseudo process control block (PCB)
 */

#ifndef	ASSEMBLER
struct pcb {
	struct	regs pcb_r;		/* saved kernel registers -- MUST BE FIRST */
	struct NeXT_saved_state *saved_regs;	/* where saved */
	int	pcb_szpt; 		/* # of pages of user page table */
	union {
		struct	mmu_030_tt tt1_030;	/* MMU transparent translation reg */
		struct mmu_040_tt tt1_040;
	} mmu_tt1;
	struct	fpc_internal pcb_fpc_i;	/* FPC internal state */
	struct	fpc_external pcb_fpc_e;	/* FPC external state */
	int	thread_user_reg;	/* an extra per-thread reg for user */
	u_char	pcb_flags;
#define	PCB_TT1		0x80		/* thread uses MMU tt1 */
#define	PCB_AST		0x0f		/* flag bits for AST & trace */
};
#endif	ASSEMBLER

/*
 *	Because the 68000 lacks an asynchronous system trap (AST)
 *	facility like the VAX, we simulate one by using the trace
 *	trap mode.  The following flags stored in the pcb keep AST's
 *	and regular user-requested tracing straight.  Currently,
 *	AST's are only used to cause a user process reschedules
 *	(see aston()).  Note that AST's can't be accomplished with
 *	the software interrupt facility because they must be triggered
 *	only when running in the user context.
 */

#define	AST_SCHEDULE	0x8	/* force process to resched ASAP via trace */
#define	TRACE_AST	0x4	/* AST caused trace to be turned on */
#define	TRACE_USER	0x2	/* user caused trace to be turned on */
#define	TRACE_PENDING	0x1	/* wait until syscall done before tracing */

#define	AST_CLR		0xf
#define	AST_NONE	0x0

#define	aston() \
	{ current_thread()->pcb->pcb_flags |= AST_SCHEDULE; }

#define astoff() \
	{ current_thread()->pcb->pcb_flags &= ~AST_SCHEDULE; }

#ifndef	ASSEMBLER
void	pcb_init();
void	pcb_terminate();
#define	pcb_synch(thread)	/* not needed for NeXT */
#endif	ASSEMBLER

#endif	_PCB_


