/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 20-Jul-90  Brian Pinkerton (bpinker) at NeXT
 *	Import kernel_stack.h to use new definition of KERNEL_STACK_SIZE
 *
 * 18-Feb-90  Gregg Kellogg (gk) at NeXT
 *	thread->exit_code -> thread->ast
 *	THREAD_EXIT -> AST_ZILCH
 *
 * 15-Dec-88  Avadis Tevanian (avie) at NeXT
 *	Allow any status bits to be set if thread is in a task with
 *	kernel privilegs.
 *
 * 13-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Removed dependencies on proc table.
 *
 * 17-May-88  Avadis Tevanian, Jr. (avie) at NeXT
 *      Implemented 68882 support, updated interface.
 *
 * 16-Feb-88  John Seamons (jks) at NeXT
 *	Updated to Mach release 2.
 *
 * 12-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Modify for new thread_status interface.
 *
 * 15-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Make pcb_init take a thread pointer instead of a pcb pointer as
 *	a parameter.
 *
 * 10-Jul-87  David Black (dlb) at Carnegie-Mellon University
 *	Added dummy pcb_synch() routine.
 *
 * 29-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added thread_dup for proper handling of fork under Unix
 *	compatibility.
 *
 * 27-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added thread_setstatus machine dependent routine which is used to
 *	set the user register state.
 *
 * 02-Dec-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */

#import <cpus.h>

#import <kern/task.h>
#import <kern/thread.h>
#import <kern/kernel_stack.h>
#import <sys/thread_status.h>
#import <sys/types.h>		/* for caddr_t in use of bzero */
#import <vm/vm_param.h>
#import <next/pcb.h>
#import <next/psl.h>
#import <next/mmu.h>

struct pcb *fpc_pcb[NCPUS];		/* last pcb to use fpc (per-cpu) */

void pcb_init (thread, ksp)
	register thread_t	thread;
	register vm_offset_t	ksp;
{
	register struct pcb	*pcb = thread->pcb;
	register struct NeXT_saved_state *saved_state;
	int		thread_bootstrap();
	register int	cpu = cpu_number();

	bzero((caddr_t)pcb, sizeof(struct pcb));

	/*
	 *	Set up thread to start at user bootstrap.  The stack
	 *	pointer is setup to point to the frame that corresponds
	 *	to the user's state (thread_bootstrap will perform a
	 *	rte instruction to simulate returning from a trap).
	 *
	 */
	pcb->pcb_r.r_pc = (int) thread_bootstrap;
	pcb->pcb_r.r_sr = SR_SUPER;	/* kernel mode */
	saved_state = (struct NeXT_saved_state *)
		(ksp + KERNEL_STACK_SIZE - sizeof(struct NeXT_saved_state));

	pcb->pcb_r.r_sp = (int) saved_state;
	pcb->saved_regs = saved_state;

	/*
	 *	Guarantee that the bootrapped thread will be in user
	 *	mode (the r_sr assignment above executes the bootstrap
	 *	code in kernel mode.  Note, this is the only user register
	 *	that we set.  All others are assumed to be random unless
	 *	the user sets them.
	 */
	saved_state->ss_r.r_sr = SR_USER;

	/* flush fpc register cache */
	if (fpc_pcb[cpu] == pcb)
		fpc_pcb[cpu] = 0;
}

void pcb_terminate(thread)
	register thread_t	thread;
{
	register int		cpu = cpu_number();

	if (fpc_pcb[cpu] == thread->pcb)	/* flush fpc register cache */
		fpc_pcb[cpu] = 0;
}

/*
 *	Set up the context for the very first thread to execute
 *	(which is passed in as the parameter).
 */
void initial_context(thread)
	thread_t	thread;
{
	register struct pcb	*pcb;

	pcb = thread->pcb;
	/* FIXME: someday cause this to update cache in msp */
	current_thread() = thread;
	PMAP_ACTIVATE(vm_map_pmap(thread->task->map), thread, cpu_number());
}

/*
 *	thread_start:
 *
 *	Start a thread at the specified routine.  The thread must
 *	be in a suspended state.
 */
thread_start(thread, start, mode)
	thread_t	thread;
	void		(*start)();
	int		mode;
{
	register struct pcb	*pcb;

	pcb = thread->pcb;
	pcb->pcb_r.r_pc = (int) start;
	if (mode == THREAD_USERMODE)
		pcb->pcb_r.r_sr = SR_USER;	/* user mode */
	else
		pcb->pcb_r.r_sr = SR_SUPER;	/* kernel mode */
	pcb->pcb_r.r_sp = thread->kernel_stack + KERNEL_STACK_SIZE;
}

/*
 *	thread_setstatus:
 *
 *	Set the status of the specified thread.
 */
kern_return_t thread_setstatus(thread, flavor, tstate, count)
	thread_t		thread;
	int			flavor;
	thread_state_t		tstate;
	unsigned int		count;
{
	register int i;
	register struct NeXT_thread_state_regs	*state_regs;
	register struct NeXT_thread_state_68882	*state_68882;
	register struct NeXT_thread_state_user_reg *state_user_reg;
	register struct pcb			*pcb;
	register struct NeXT_saved_state	*saved_state;

	switch (flavor) {
	/*
	 *	Normal register state (68030).
	 */
	case NeXT_THREAD_STATE_REGS:
		if (count < NeXT_THREAD_STATE_REGS_COUNT) {
			/* invalid size structure */
			return (KERN_INVALID_ARGUMENT);
		}

		state_regs = (struct NeXT_thread_state_regs *) tstate;
		saved_state = USER_REGS(thread);
		for (i = 0; i < 8; i++) {
			saved_state->ss_r.r_dreg[i] = state_regs->dreg[i];
			saved_state->ss_r.r_areg[i] = state_regs->areg[i];
		}
		saved_state->ss_r.r_sr = state_regs->sr;
		saved_state->ss_r.r_pc = state_regs->pc;

		/*
		 *	Carefully set user bits
		 */
		if (!thread->task->kernel_privilege) {
			saved_state->ss_r.r_sr &= ~SR_USERCLR;
			saved_state->ss_r.r_sr |= SR_USER;
		}
		break;
	/*
	 *	FPC state (68882).
	 */
	case NeXT_THREAD_STATE_68882:
		if (count < NeXT_THREAD_STATE_68882_COUNT) {
			/* invalid size structure */
			return (KERN_INVALID_ARGUMENT);
		}

		state_68882 = (struct NeXT_thread_state_68882 *) tstate;
		pcb = thread->pcb;
		bcopy(&state_68882->regs, &pcb->pcb_fpc_e.regs,
				sizeof(state_68882->regs));
		pcb->pcb_fpc_e.cr = state_68882->cr;
		pcb->pcb_fpc_e.sr = state_68882->sr;
		pcb->pcb_fpc_e.iar = state_68882->iar;
		pcb->pcb_fpc_e.state = state_68882->state;
		break;
	/*
	 *	User register (also settable via fast trap).
	 */
	case NeXT_THREAD_STATE_USER_REG:
		if (count < NeXT_THREAD_STATE_USER_REG_COUNT) {
			/* invalid size structure */
			return (KERN_INVALID_ARGUMENT);
		}

		state_user_reg = (struct NeXT_thread_state_user_reg *) tstate;
		pcb = thread->pcb;
		pcb->thread_user_reg = state_user_reg->user_reg;
		break;
	default:
		return(KERN_INVALID_ARGUMENT);
	}

	return (KERN_SUCCESS);
}

/*
 *	thread_getstatus:
 *
 *	Get the status of the specified thread.
 */
kern_return_t thread_getstatus(thread, flavor, tstate, count)
	register thread_t	thread;
	int			flavor;
	thread_state_t		tstate;		/* pointer to OUT array */
	unsigned int		*count;		/* IN/OUT */
{
	register int i;
	register struct thread_state_flavor	*flavor_regs;
	register struct NeXT_thread_state_regs	*state_regs;
	register struct NeXT_thread_state_68882	*state_68882;
	register struct NeXT_thread_state_user_reg *state_user_reg;
	register struct pcb			*pcb;
	register struct NeXT_saved_state	*saved_state;

	switch (flavor) {
	case THREAD_STATE_FLAVOR_LIST:
		if (*count < NeXT_THREAD_STATE_MAXFLAVOR*2) {
			/* invalid size structure */
			return (KERN_INVALID_ARGUMENT);
		}

		flavor_regs = (struct thread_state_flavor *) tstate;
		flavor_regs->flavor = NeXT_THREAD_STATE_REGS;
		flavor_regs->count = NeXT_THREAD_STATE_REGS_COUNT;
		flavor_regs++;
		flavor_regs->flavor = NeXT_THREAD_STATE_68882;
		flavor_regs->count = NeXT_THREAD_STATE_68882_COUNT;
		flavor_regs++;
		flavor_regs->flavor = NeXT_THREAD_STATE_USER_REG;
		flavor_regs->count = NeXT_THREAD_STATE_USER_REG_COUNT;
		*count = NeXT_THREAD_STATE_MAXFLAVOR*2;
		break;
	case NeXT_THREAD_STATE_REGS:
		if (*count < NeXT_THREAD_STATE_REGS_COUNT) {
			/* invalid size structure */
			return (KERN_INVALID_ARGUMENT);
		}

		state_regs = (struct NeXT_thread_state_regs *) tstate;
		saved_state = USER_REGS(thread);
		for (i = 0; i < 8; i++) {
			state_regs->dreg[i] = saved_state->ss_r.r_dreg[i];
			state_regs->areg[i] = saved_state->ss_r.r_areg[i];
		}
		state_regs->sr = saved_state->ss_r.r_sr;
		state_regs->pc = saved_state->ss_r.r_pc;
		*count = NeXT_THREAD_STATE_REGS_COUNT;
		break;
	case NeXT_THREAD_STATE_68882:
		if (*count < NeXT_THREAD_STATE_68882_COUNT) {
			/* invalid size structure */
			return (KERN_INVALID_ARGUMENT);
		}

		state_68882 = (struct NeXT_thread_state_68882 *) tstate;
		pcb = thread->pcb;
		bcopy(&pcb->pcb_fpc_e.regs, &state_68882->regs,
				sizeof(state_68882->regs));
		state_68882->cr = pcb->pcb_fpc_e.cr;
		state_68882->sr = pcb->pcb_fpc_e.sr;
		state_68882->iar = pcb->pcb_fpc_e.iar;
		state_68882->state = pcb->pcb_fpc_e.state;
		*count = NeXT_THREAD_STATE_68882_COUNT;
		break;
	case NeXT_THREAD_STATE_USER_REG:
		if (*count < NeXT_THREAD_STATE_USER_REG_COUNT) {
			/* invalid size structure */
			return (KERN_INVALID_ARGUMENT);
		}

		state_user_reg = (struct NeXT_thread_state_user_reg *) tstate;
		pcb = thread->pcb;
		state_user_reg->user_reg = pcb->thread_user_reg;
		*count = NeXT_THREAD_STATE_USER_REG_COUNT;
		break;
	default:
		return(KERN_INVALID_ARGUMENT);
	}
	return (KERN_SUCCESS);
}

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/proc.h>

/*
 *	thread_dup:
 *
 *	Duplicate the user's state of a thread.  This is only used to perform
 *	the Unix fork operation.
 */
thread_dup(parent, child)
	register thread_t	parent, child;
{
	register struct NeXT_saved_state	*parent_state, *child_state;

	parent_state = parent->pcb->saved_regs;
	child_state = child->pcb->saved_regs;
	*child_state = *parent_state;
	child_state->ss_r.r_dreg[0] = child->task->proc->p_pid;
	child_state->ss_r.r_dreg[1] = 1;
	child_state->ss_r.r_sr &= ~SR_C;
	/*
	 *	Major hack follows!!!!  If a0 == sp, assume new calling
	 *	conventions and don't pop stack.
	 */
	if (child_state->ss_r.r_areg[0] != child_state->ss_r.r_sp)
		child_state->ss_r.r_sp += NBPW;	/* pop SYS_fork arg in child */
}




