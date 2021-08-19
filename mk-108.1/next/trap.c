/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 27-Oct-90  Gregg Kellogg (gk) at NeXT
 *	Changed the exit from syscall to check for signals after a
 *	csw_needed context switch.  This would allow a signal to get posted
 *	but not be acted upon until the end of the next system call.
 *
 * 20-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Don't rerun traps after psig() is called.
 *
 * 28-Jun-90  Gregg Kellogg (gk) at NeXT
 *	Allow memory faults to return TRAP_RERUN so that continuing from
 *	a vm_protect exception will succeed.
 *
 * 16-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Upgrade for new scheduler.
 *	simple_lock changes.
 *	Use a second map for kernel space page faults for user
 *	threads using kernel_vm_space.
 *	Don't lock u_prof.pr_lock.  It's never initialized!
 *
 * Revision 2.11  89/10/11  14:50:36  dlb
 * 	Only check for termination after an AST.  Add rpd fix to
 * 	termination check after page faults.  CHECK_SIGNALS macro change.
 * 	Use csw_needed instead of runrun and friends.
 * 	[89/10/07            dlb]
 *
 * 28-Sep-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Enable syscalltrace when DEBUG is on only.
 *
 * 16-Feb-88  John Seamons (jks) at NeXT
 *	Updated to Mach release 2.
 *
 * 17-Jan-88  John Seamons (jks) at NeXT
 *	Page fault support for memory write function space.
 *
 * 24-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Implemented MACH exception handling.
 *
 * 21-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Check for thread-exit conditions in more places by using new
 *	macros.
 *
 *  7-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Check for abnormal thread exit conditions.
 *
 * 26-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Enable syscall's switch to master/slave only for multiple CPUS.
 *
 * 01-Oct-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Copyin/Copyout do not use THREAD_RECOVER - fault handler notices if
 *	they were executing based on PC and goes to proper recovery address.
 *
 * 26-Sep-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Removed unix_master calls from code that could never
 *	be executed (old BSD VM code).
 *
 * 25-Sep-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Fix SIGCLEANUP wrt. unix_master.
 *
 * 11-Sep-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	MUST do unix_master to guarantee that  "unix kernel"
 *	operations are locked.  First step in parallelizing
 *	the kernel.
 *	Unix_master() calls must be bracketed with unix_release().
 *	TT locking around addupc.
 *
 * 20-Jul-87  David Black (dlb) at Carnegie-Mellon University
 *	MACH_TT: Added thread exception checks in trap() and syscall().
 *	Removed ACALLpsig(), as it is no longer used.
 *
 * 12-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Don't do thread locking when setting the whichq field in
 *	syscall.  This should be safe since we are running and no one
 *	will touch this field.
 *
 * 12-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminate most of the direct u-area references in syscall to
 *	keep reasonable system call overhead.
 *
 * 29-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	No longer handle fork/vfork here under MACH_TT.
 *
 *  9-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	Use thread_bind to temporarily bind a thread onto the master CPU
 *	to execute a non-parallel system call.  The old thread_force
 *	routine didn't allow for MACH routines that would allow a thread
 *	to migrate onto a slave processor.
 *
 *  1-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Updated for real thread implementation.
 *
 * 30-Jan-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_RPAUSE: Added new required parameter to fspause().
 *	[ V5.1(F1) ]
 *
 * 28-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Support for NEW_SCHED.
 *
 * 17-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */
 
#import <cpus.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)trap.c	7.1 (Berkeley) 6/5/86
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <kern/parallel.h>
#import <sys/proc.h>
#import <sys/acct.h>
#import <sys/kernel.h>
#ifndef	SYSCALLTRACE
#if	DEBUG
#define	SYSCALLTRACE	1
#endif	DEBUG
#endif	SYSCALLTRACE
#if	SYSCALLTRACE
#import <bsd/syscalls.c>
#endif
#import <kern/thread.h>
#import <kern/sched.h>	/* run queue definitions for "bind" hack */
#import <sys/thread_status.h>
#import <kern/task.h>
#import <kern/xpr.h>
#import <sys/exception.h>
#import <sys/ux_exception.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <vm/vm_kern.h>
#import <vm/vm_map.h>
#import <vm/vm_param.h>
#import <sys/kern_return.h>
#import <kern/syscall_sw.h>
#import <next/trap.h>
#import <next/psl.h>
#import <next/reg.h>
#import <next/vmparam.h>
#import <next/event_meter.h>
#import	<next/cpu.h>
#import <next/spl.h>
#import <next/scr.h>

#define	USER	0x8000		/* user-mode flag added to type */

struct	sysent	sysent[];
int	nsysent, traptrace;
volatile int traceback_recursive;
label_t traceback_jb[2];

struct trap_type {
	short	tt_type;	/* processor vector number */
	char	*tt_name;
} trap_type[] = {
	T_BUSERR,	"bus error",
	T_ADDRERR,	"address error",
	T_ILLEGAL,	"illegal instruction",
	T_ZERODIV,	"zero divide",
	T_CHECK,	"CHK, CHK2 instruction",
	T_TRAPV,	"TRAPV, cpTRAPcc, TRAPcc instruction",
	T_PRIVILEGE,	"privilege violation",
	T_TRACE,	"trace trap",
	T_EMU1010,	"1010 emulator trap",
	T_EMU1111,	"1111 emulator trap",
	T_COPROC,	"coprocessor protocol error",
	T_FORMAT,	"stack format error",
	T_BADINTR,	"uninitialized interrupt",
	T_SPURIOUS,	"spurious interrupt",
	T_MMU_CONFIG,	"MMU configuration",
	T_MMU_ILL,	"MMU illegal operation",
	T_MMU_ACCESS,	"MMU access level violation",
	T_MMU_BUSERR,	"MMU bus error",
	T_MMU_LIMIT,	"MMU limit error",
	T_MMU_SUPER,	"MMU supervisor access error",
	T_MMU_WRITE,	"MMU write access error",
	T_MMU_INVALID,	"MMU invalid descriptor during table walk",
	0
};

/*
 * Called from the trap handler when a processor trap occurs.
 * Returns {TRAP_RERUN, TRAP_NORERUN} to indicate whether the
 * failed bus cycle(s) should be rerun.
 */
/*ARGSUSED*/
int
trap (type, fcode, rw, faultaddr, regs)
	int type, fcode;
	int rw;				/* VM_PROT_{READ, READ|WRITE} */
	vm_offset_t faultaddr;		/* fault address */
	struct regs regs;
{
	register struct regs *r = &regs;
	register int i, save_error;
	register struct proc *p;
	register struct trap_type *tp;
	struct timeval syst;
	vm_offset_t	recover, va;
	extern		vm_offset_t probe_recover;
	kern_return_t	fault_result = KERN_SUCCESS;
	vm_map_t	map, second_map;
	thread_t	thread;
	struct pcb	*pcb;
	int		rerun = TRAP_RERUN;
	extern int	ALLOW_FAULT_START, ALLOW_FAULT_END, FAULT_ERROR;
	int		exc_type, exc_code, exc_subcode;
	struct NeXT_saved_state *saved_regs;

#if 	DEBUG
/*	if ((i = (vm_offset_t)&exc_type - trunc_page(&exc_type)) < 1000) {
	  printf("getting close to end of stack (%d bytes)\n", i);
	}*/
#endif	DEBUG

	if ((thread = current_thread()) != THREAD_NULL)
		pcb = thread->pcb;
	else
		pcb = 0;
XPR(XPR_TRAPS, ("trap start: type 0x%x fcode %d pc 0x%x, sp 0x%x, usp 0x%x, sr 0x%x\n",
		type, fcode, r->r_pc, &regs, r->r_sp, r->r_sr));
XPR(XPR_TRAPS, ("trap start: rw 0x%x faultaddr 0x%x thread 0x%x proc 0x%x pid %d\n",
		rw, faultaddr, thread, thread? (int)u.u_procp : -1,
			thread? u.u_procp->p_pid : -1));
#if	DEBUG
	if (traptrace < 0 || (thread && traptrace == (int)thread)) {
printf ("trap(%d): type 0x%x fcode %d pc 0x%x, sp 0x%x, usp 0x%x, sr 0x%x\n",
		thread? u.u_procp->p_pid : -1,
		type, fcode, r->r_pc, &regs, r->r_sp, r->r_sr);
printf ("\trw 0x%x faultaddr 0x%x thread 0x%x proc 0x%x\n",
		rw, faultaddr, thread, thread ? (int)u.u_procp : -1);
	}
#endif	DEBUG

	if (!USERMODE(r->r_sr)) {

	/*
	 *	Trap in system mode.  Only page-faults are valid,
	 *	and then only in special circumstances.
	 */
	switch (type) {

	case T_TRACE:	/* happens when trap instruction is traced */
		if (pcb)
			pcb->pcb_flags |= TRACE_PENDING;
		XPR(XPR_TRAPS, ("trap: T_TRACE in kernel mode, return RERUN\n"));
		return (TRAP_RERUN);

	case T_MMU_INVALID:
	case T_MMU_WRITE:
	case T_MMU_LIMIT:
		if (thread != THREAD_NULL) {
			save_error = u.u_error;
			u.u_error = 0;
		}

		/*
		 *	Determine which map to "fault" on.
		 *	We can get user space faults in kernel mode
		 *	from copyin, etc.
		 */
		second_map = VM_MAP_NULL;
		if ((fcode & ~(FC_SUPERI ^ FC_SUPERD)) == 0) {
			/*
			 *	User space:  just use the task's map.
			 */
			map = thread->task->map;
		} else {
			/*
			 *	Kernel space:  use kernel_map, except...
			 */
			if ((thread != THREAD_NULL) &&
			    thread->task->kernel_vm_space &&
			    (thread->task->map != kernel_map)) {
				/*
				 *	if this is a kernel_vm_space task,
				 *	then its map is a submap of kernel_map.
				 *	In an attempt to avoid some deadlock
				 *	problems in the VM code, first try
				 *	the task's map, and then kernel_map.
				 */
				map = thread->task->map;
				second_map = kernel_map;
			} else {
				map = kernel_map;
			}
		}
		XPR(XPR_TRAPS, ("trap MMU: calling vm_fault\n"));

		va = faultaddr;
		fault_result = vm_fault (map, trunc_page (va), rw, FALSE);
		if ((fault_result != KERN_SUCCESS) &&
		    (second_map != VM_MAP_NULL))
			fault_result = vm_fault (second_map, trunc_page (va), rw, FALSE);
		if (thread != THREAD_NULL) {
			u.u_error = save_error;
		}

		if (fault_result == KERN_SUCCESS) {
			event_meter (EM_PAGE_FAULT);

			/*
			 *	Load new entry into ATC (TLB).
			 */
			if (rw == VM_PROT_READ)
				pload_read (fcode, faultaddr);
			else
				pload_write (fcode, faultaddr);
			if (cpu_type != MC68030)
				do_writeback (r);
			XPR(XPR_TRAPS, ("trap MMU: returning RERUN\n"));
			return (TRAP_RERUN);
		}
		/* fall through ... */

	case T_BUSERR:
		/*
		 *	Return control to the failure
		 *	return address that has been saved in
		 *	the thread table.
		 */
		if (thread != THREAD_NULL)
			recover = thread->recover;
		else
			recover = 0;

		/* catch faults in copyin/copyout */
		if (recover == 0 &&
		    (vm_offset_t)r->r_pc > (vm_offset_t)&ALLOW_FAULT_START &&
		    (vm_offset_t)r->r_pc < (vm_offset_t)&ALLOW_FAULT_END)
			recover = (vm_offset_t)&FAULT_ERROR;
		if (recover == 0 && probe_recover != 0) {
			recover = probe_recover;
		}
		if (recover) {
			r->r_pc = recover;
			return (TRAP_NORERUN);
		}
		
		/* get around bug in old DMA chip -- retry failed accesses to P_MON */
		switch (machine_type) {
		
		case NeXT_CUBE:
		case NeXT_WARP9:
		case NeXT_WARP9C:
		case NeXT_X15:
			if ((faultaddr >= P_MON && faultaddr < P_MON + 12) ||
			    (faultaddr >= P_PRINTER && faultaddr < P_PRINTER + 12))
				return (TRAP_RERUN);
		}

		printf("unexpected kernel page fault failure\n");

		/*
		 * Unanticipated page-fault errors in kernel
		 * should not happen.  Fall through ...
		 */
	default:
	Default:
		spl7();
		printf ("trap: type 0x%x fcode %d rw %d faultaddr 0x%x\n",
			type, fcode, rw, faultaddr);
		printf ("trap: pc 0x%x sp 0x%x sr 0x%x\n",
			r->r_pc, r->r_sp, r->r_sr);
		printf ("trap: cpu %d th 0x%x proc 0x%x pid %d pcb 0x%x\n",
			cpu_number(), thread, thread? (int)u.u_procp : -1,
			thread? u.u_procp->p_pid : -1, pcb);

		if (setjmp (&traceback_jb[traceback_recursive]) == 0)
			traceback (r->r_areg[6]);
		type = (type & ~USER);
		for (tp = trap_type; tp->tt_type; tp++)
			if (type == tp->tt_type)
				panic (tp->tt_name);
		printf ("trap: stray vector #%d (0x%x)\n", type >> 2, type);
		panic("trap");
	}
	}

	/*
	 *	Trap from user mode.
	 */
	type |= USER;
	p = u.u_procp;

	if (p) {
	    syst = u.u_ru.ru_stime;
	    u.u_ar0 = &r->r_d0;
	}
	saved_regs = thread->pcb->saved_regs;
	thread->pcb->saved_regs = (struct NeXT_saved_state *)&r->r_d0;

	exc_code = 0;
	exc_subcode = 0;

	switch (type) {

	case T_MMU_INVALID|USER:
	case T_MMU_WRITE|USER:
	case T_MMU_LIMIT|USER:
		save_error = u.u_error;
		u.u_error = 0;
		map = thread->task->map;

		va = faultaddr;	
		XPR(XPR_TRAPS, ("trap MMU: calling vm_fault\n"));
		fault_result = vm_fault (map, trunc_page (va), rw, FALSE);
		u.u_error = save_error;
		if (fault_result == KERN_SUCCESS) {
			event_meter (EM_PAGE_FAULT);
#if	EVENTMETER
			if (em.state == EM_VM && p) {
				int x, y, hit;
				extern int em_y_vm[];

				x = (atop (va) % 1024) + EM_L;
				y = atop (va) / 1024;
				for (hit = 0; hit < EM_NVM; hit++)
					if (p->p_pid == em.pid[hit])
						break;
				if (hit < EM_NVM)
					event_pt (x, y + em_y_vm[hit], BLACK);
			}
#endif	EVENTMETER

			/*
			 *	Load new entry into ATC (TLB).
			 */
			if (rw == VM_PROT_READ)
				pload_read (fcode, faultaddr);
			else
				pload_write (fcode, faultaddr);
			if (cpu_type != MC68030)
				do_writeback (r);
			XPR(XPR_TRAPS, ("trap MMU: returning RERUN\n"));
			thread->pcb->saved_regs = saved_regs;
			return (TRAP_RERUN);
		}
		exc_type = EXC_BAD_ACCESS;
		exc_code = fault_result;
		exc_subcode = va;
		break;

	case T_BUSERR|USER:		/* SIGSEGV */

		/* get around bug in old DMA chip -- retry failed accesses to P_MON */
		switch (machine_type) {
		
		case NeXT_CUBE:
		case NeXT_WARP9:
		case NeXT_WARP9C:
		case NeXT_X15:
			if (faultaddr >= P_MON && faultaddr < P_MON + 12)
				return (TRAP_RERUN);
		}

		exc_type = EXC_BAD_ACCESS;
		exc_code = KERN_INVALID_ADDRESS;
		break;

	case T_ADDRERR|USER:		/* SIGBUS */
		exc_type = EXC_BAD_ACCESS;
		exc_code = KERN_PROTECTION_FAILURE;	/* not really.. */
		break;

	case T_PRIVILEGE|USER:	/* SIGILL */
	case T_COPROC|USER:
	case T_ILLEGAL|USER:
	case T_BADTRAP|USER:
	default:
		exc_type = EXC_BAD_INSTRUCTION;
		exc_code = r->r_vector;		/* FIXME: make mach indep? */
		break;

	case T_EMU1010|USER:		/* SIGEMT */
	case T_EMU1111|USER:
		exc_type = EXC_EMULATION;
		exc_code = r->r_vector;		/* FIXME: make mach indep? */
		break;
		
	case T_FPP|USER:		/* SIGFPE */
	case T_FP_UDT|USER:
	case T_ZERODIV|USER:
	case T_CHECK|USER:
	case T_TRAPV|USER:
		exc_type = EXC_ARITHMETIC;
		exc_code = r->r_vector;		/* FIXME: make mach indep? */
		break;

	case T_USER_BPT|USER:		/* SIGTRAP */
		exc_type = EXC_BREAKPOINT;
		break;

	case T_TRACE|USER:
		dotrace (r);
		goto out;
	}

	/*
	 *	Check for halt condition here, as both a trace trap and
	 *	a mem violation is reported as a mem violation.
	 */
	while (thread_should_halt(thread))
		thread_halt_self();

	thread_doexception(thread, exc_type, exc_code, exc_subcode);
out:
	if (p) {
	    if (CHECK_SIGNALS(p, thread, thread->u_address.uthread)) {
		unix_master();
		if (p->p_cursig || issig())
		    psig();
		unix_release();
		rerun = TRAP_NORERUN;
	    }
	}

	while (thread_should_halt(thread))
	    thread_halt_self();

	if (USERMODE(r->r_sr)) {
		/*
		 *	If trapped from user mode, handle possible
		 *	rescheduling.
		 */
		if (csw_needed(thread, current_processor())) {
			u.u_ru.ru_nivcsw++;
			thread_block();
			if (p && CHECK_SIGNALS(p, thread,
						thread->u_address.uthread))
			{
				/*
				 * A signal's come it since we blocked,
				 * go back and handle them again.
				 */
				goto out;
			}
		}
	}

	if (u.u_prof.pr_scale) {
	    int	ticks;
	    struct timeval *tv = &u.u_ru.ru_stime;

	    ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
	    if (ticks)
		addupc(r->r_pc, &u.u_prof, ticks);
	}
	thread->pcb->saved_regs = saved_regs;
	return (rerun);
}

#if	SYSCALLTRACE
int syscalltrace = 0;

syscall_trace(syscode, callp)
	short syscode;
	struct sysent *callp;
{
	register int i;
	char *cp;

	printf ("%s(%d): ", u.u_comm, u.u_procp->p_pid);
	if ((u_short)(syscode) >= nsysent)
		printf("0x%x", syscode);
	else
		printf("%s [%d] ", syscallnames[syscode],
			syscode);
	cp = "(";
	for (i = 0; i < callp->sy_narg; i++) {
		printf("%s%x", cp, u.u_arg[i]);
		cp = ", ";
	}
	if (i)
		cp = ")";
	else
		cp = "";
	printf("%s\n", cp);
	XPR(XPR_SYSCALLS, ("%c%c%c%c(%d): ",
		u.u_comm[0], u.u_comm[1], u.u_comm[2],
		u.u_comm[3], u.u_procp->p_pid));
	XPR(XPR_SYSCALLS, ("%s (%x, %x, %x, %x)\n",
		syscallnames[syscode], u.u_arg[0], u.u_arg[1],
		u.u_arg[2], u.u_arg[3]));
}

mach_syscall_trace(syscall_no, uthread, nargbytes)
	struct uthread *uthread;
{
	register int i;
	char *cp;

	printf ("%s(%d): mach call 0x%x ", u.u_comm, u.u_procp->p_pid,
		syscall_no);
	cp = "(";
	for (i = 0; i < nargbytes/sizeof (int); i++) {
		printf("%s%x", cp, uthread->uu_arg[i]);
		cp = ", ";
	}
	if (i)
		cp = ")";
	else
		cp = "";
	printf("%s\n", cp);
	XPR(XPR_SYSCALLS, ("%c%c%c%c(%d): ",
		u.u_comm[0], u.u_comm[1], u.u_comm[2],
		u.u_comm[3], u.u_procp->p_pid));
	XPR(XPR_SYSCALLS, ("mach call 0x%x (%x, %x, %x, %x)\n",
		syscall_no, uthread->uu_arg[0], uthread->uu_arg[1],
		uthread->uu_arg[2], uthread->uu_arg[3]));
}
#endif	SYSCALLTRACE

#ifdef DEBUG
extern int lastsyscall;
#endif DEBUG

/*
 * Called from the trap handler when a system call occurs.
 */
/*ARGSUSED*/
old_syscall(code, regs)
	short code;
	struct regs regs;
{
	register struct regs *r = &regs;
	register caddr_t params;
	register int i;
	register struct sysent *callp;
	register int	error;
	register struct uthread	*uthread;
	register struct utask	*utask;
	register struct proc *p;
	int		s;
	thread_t	thread = current_thread();
	int opc;
	struct timeval syst;
	short	syscode;

	if (u.u_procp == (struct proc *)0) {
	    thread_doexception(thread, EXC_SOFTWARE,
		EXC_UNIX_BAD_SYSCALL, 0);	/* XXX */
	    return;
	}
	event_meter (EM_SYSCALL);
	uthread = thread->u_address.uthread;
	utask = thread->u_address.utask;
	syst = utask->uu_ru.ru_stime;
	ASSERT(USERMODE(r->r_sr));
	uthread->uu_ar0 = &r->r_d0;
	thread->pcb->saved_regs =
		(struct NeXT_saved_state *) &r->r_d0;
	uthread->uu_error = 0;

	/* NBPW*2 to skip over code and ra */
	params = (caddr_t)r->r_sp + NBPW*2;
	opc = r->r_pc - 2;	/* back up to beginning of trap instr */
	if (code < 0) {
		callp = &sysent[63];	/* undefined */
	} else
		callp = (code >= nsysent) ? &sysent[63] : &sysent[code];
	if (callp == sysent) {		/* indir */
		i = fuword(params);
		params += NBPW;
		callp = ((u_short) (i) >= nsysent)?
			&sysent[63] : &sysent[i];
		syscode = i;
#ifdef DEBUG
		lastsyscall = i;
#endif DEBUG
	}
	else
		syscode = code;

	if ((i = callp->sy_narg * sizeof (int)) &&
	    (error = copyin(params,(caddr_t)uthread->uu_arg,(u_int)i)) != 0) {
		r->r_d0 = error;
		r->r_sr |= SR_C;	/* carry bit */
		goto done;
	}
	uthread->uu_r.r_val1 = 0;
	uthread->uu_r.r_val2 = r->r_dreg[1];
	uthread->uu_ap = uthread->uu_arg;
#if	NCPUS > 1
	if (callp->sy_parallel == 0) {
		/*
		 *	System call must execute on master cpu
		 */
		ASSERT(thread->unix_lock == -1);
		unix_master();		/* hold to master during syscall */
	}
#endif	NCPUS > 1
	if (setjmp(&uthread->uu_qsave)) {
		unix_reset();
		if (error == 0 && uthread->uu_eosys != RESTARTSYS)
			error = EINTR;
		while (thread_should_halt(thread))
			thread_halt_self();
	} else {
		uthread->uu_eosys = NORMALRETURN;
#if	SYSCALLTRACE
		if (syscalltrace == u.u_procp->p_pid || syscalltrace == -1)
			syscall_trace(syscode, callp);
#endif	SYSCALLTRACE
		/*
		 *  Show no resource pause conditions.
		 */
		uthread->uu_rpswhich = 0;	/* CS_RPAUSE */
		uthread->uu_rpsfs = 0;		/* CS_RPAUSE */
		(*(callp->sy_call))();
		error = uthread->uu_error;
	}
/* BEGIN CS_RPAUSE */
	if (error) switch (error)
	{
	    case ENOSPC:
	    {
		/*
		 *  The error number ENOSPC indicates disk block or inode
		 *  exhaustion on a file system.  When this occurs during a
		 *  system call, the fsfull() routine will record the file
		 *  system pointer and type of failure (1=disk block, 2=inode)
		 *  in the U area.  If we return from a system call with this
		 *  error set, invoke the fspause() routine to determine
		 *  whether or not to enter a resource pause.  It will check
		 *  that the resource pause fields have been set in the U area
		 *  (then clearing them) and that the process has enabled such
		 *  waits before clearing the error number and pausing.  If a
		 *  signal occurs during the sleep, we will return with a false
		 *  value and the error number set back to ENOSPC.  If the wait
		 *  completes successfully, we return here with a true value.
		 *  In this case, we simply restart the current system call to
		 *  retry the operation.
		 *
		 *  Note:  Certain system calls can not be restarted this
		 *  easily since they may partially complete before running
		 *  into a resource problem.  At the moment, the read() and
		 *  write() calls and their variants have this characteristic
		 *  when performing multiple block operations.  Thus, the
		 *  resource exhaustion processing for these calls must be
		 *  handled directly within the system calls themselves.  When
		 *  they return to this point (even with ENOSPC set), the
		 *  resource pause fields in the U area will have been cleared
		 *  by their previous calls on fspause() and no action will be
		 *  taken here.
		 */
		if (fspause(0))
		    uthread->uu_eosys = RESTARTSYS;
		error = uthread->uu_error;	/* can change in fspause */
		break;
	    }
	    /*
	     *  TODO:  Handle these error numbers, also.
	     */
	    case EAGAIN:
	    case ENOMEM:
	    case ENFILE:
		break;
	}
/* END CS_RPAUSE */

	if (uthread->uu_eosys == NORMALRETURN) {
normalreturn:
		if (error) {
			r->r_d0 = error;
			r->r_sr |= SR_C;	/* carry bit */
#if	SYSCALLTRACE
			if ((syscalltrace < -1? -syscalltrace : syscalltrace)
			    == u.u_procp->p_pid || syscalltrace == -1) {
				if (syscalltrace < -1)
					syscall_trace(syscode, callp);
				printf ("u.u_error = %d\n", error);
				XPR(XPR_SYSCALLS, ("u.u_error = %d\n", error));
			}
#endif	SYSCALLTRACE
		} else {
			r->r_dreg[0] = uthread->uu_r.r_val1;
			r->r_dreg[1] = uthread->uu_r.r_val2;
			r->r_sr &= ~SR_C;
		}
		r->r_sp += NBPW;	/* pop code so libc doesn't have to */
	} else
	if (uthread->uu_eosys == RESTARTSYS) {
		r->r_pc = opc;
		while (thread_should_halt(thread))
			thread_halt_self();
	}
	else
	ASSERT(uthread->uu_eosys == JUSTRETURN);
done:
	if (thread->pcb->pcb_flags & TRACE_PENDING) {
		dotrace (r);
	}
	uthread->uu_error = error;
	p = utask->uu_procp;
check_sig:
	if (CHECK_SIGNALS(p, thread, uthread)) {
		unix_master();
		if (p->p_cursig || issig())
			psig();
		unix_release();
	}
	p->p_pri = p->p_usrpri;	/* FIXME: do this w/ mach exceptions? */
#if	NCPUS > 1
	/*
	 *	It is OK to go back on a slave
	 */
	if (callp->sy_parallel == 0) {
		unix_release();
		ASSERT(thread->unix_lock == -1);
	}
#endif	NCPUS > 1
	while (thread_should_halt(thread))
		thread_halt_self();

	if (csw_needed(thread, current_processor())) {
		u.u_ru.ru_nivcsw++;
		thread_block();
		if (CHECK_SIGNALS(p, thread, uthread)) {
			/*
			 * A signal's come it since we blocked,
			 * go back and handle them again.
			 */
			goto check_sig;
		}
	}
	if (utask->uu_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &utask->uu_ru.ru_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
//			simple_lock(u.u_prof.pr_lock);
			addupc(r->r_pc, &utask->uu_prof, ticks);
//			simple_unlock(u.u_prof.pr_lock);
		}
	}
}

#define INIT_VARS \
	r = &regs; \
	thread = current_thread(); \
	uthread = thread->u_address.uthread; \
	utask = thread->u_address.utask; \
	syscode = r->r_dreg[0]; \
	if (syscode < 0) { \
		callp = &sysent[63]; /* XXX 172 -> 63 */ \
	} \
	else { \
		callp = (syscode >= nsysent) ? &sysent[63] : \
				&sysent[syscode]; \
	}

unix_syscall(regs)
	struct regs regs;
{
	register struct regs *r;
	register caddr_t params;
	register struct sysent *callp;
	register int	error;
	register struct uthread	*uthread;
	register struct utask	*utask;
	register struct proc *p;
	int		s;
	thread_t	thread;
	int		opc;
	struct timeval syst;
	int		syscode;

	event_meter (EM_SYSCALL);
	INIT_VARS;
	p = utask->uu_procp;
	if (p == (struct proc *)0) {
	    thread_doexception(thread, EXC_SOFTWARE,
		EXC_UNIX_BAD_SYSCALL, 0);	/* XXX */
	    return;
	}
	syst = utask->uu_ru.ru_stime;
	uthread->uu_ar0 = &r->r_dreg[0];

	thread->pcb->saved_regs =
		(struct NeXT_saved_state *) &r->r_dreg[0];
	uthread->uu_error = 0;

	params = (caddr_t)&r->r_dreg[1];
	if (callp == sysent) {		/* indir */
		syscode = r->r_dreg[1];
#ifdef DEBUG
		lastsyscall = syscode;
#endif DEBUG
		params = (caddr_t)&r->r_dreg[2];
		if (syscode < 0) {
			callp = &sysent[63];
		}
		else { \
			callp = (syscode >= nsysent) ? &sysent[63] :
					&sysent[syscode];
		}
	}

	if (callp->sy_narg > 0)
		bcopy(params,
		      (caddr_t)uthread->uu_arg, callp->sy_narg*sizeof(int));
	uthread->uu_r.r_val1 = 0;
#if	NCPUS > 1
	if (callp->sy_parallel == 0) {
		/*
		 *	System call must execute on master cpu
		 */
		ASSERT(thread->unix_lock == -1);
		unix_master();		/* hold to master during syscall */
	}
#endif	NCPUS > 1
	if (fast_setjmp(&uthread->uu_qsave)) {
		INIT_VARS;
		unix_reset();
		if (uthread->uu_error == 0 && uthread->uu_eosys != RESTARTSYS)
			error = EINTR;
		while (thread_should_halt(thread))
			thread_halt_self();
	} else {
		uthread->uu_eosys = NORMALRETURN;
#if	SYSCALLTRACE
		if (syscalltrace == u.u_procp->p_pid || syscalltrace == -1)
			syscall_trace(syscode, callp);
#endif	SYSCALLTRACE
		/*
		 *  Show no resource pause conditions.
		 */
		uthread->uu_rpswhich = 0;	/* CS_RPAUSE */
		uthread->uu_rpsfs = 0;		/* CS_RPAUSE */
		(*(callp->sy_call))();
		error = uthread->uu_error;
	}
/* BEGIN CS_RPAUSE */
	if (error) switch (error)
	{
	    case ENOSPC:
	    {
		/*
		 *  The error number ENOSPC indicates disk block or inode
		 *  exhaustion on a file system.  When this occurs during a
		 *  system call, the fsfull() routine will record the file
		 *  system pointer and type of failure (1=disk block, 2=inode)
		 *  in the U area.  If we return from a system call with this
		 *  error set, invoke the fspause() routine to determine
		 *  whether or not to enter a resource pause.  It will check
		 *  that the resource pause fields have been set in the U area
		 *  (then clearing them) and that the process has enabled such
		 *  waits before clearing the error number and pausing.  If a
		 *  signal occurs during the sleep, we will return with a false
		 *  value and the error number set back to ENOSPC.  If the wait
		 *  completes successfully, we return here with a true value.
		 *  In this case, we simply restart the current system call to
		 *  retry the operation.
		 *
		 *  Note:  Certain system calls can not be restarted this
		 *  easily since they may partially complete before running
		 *  into a resource problem.  At the moment, the read() and
		 *  write() calls and their variants have this characteristic
		 *  when performing multiple block operations.  Thus, the
		 *  resource exhaustion processing for these calls must be
		 *  handled directly within the system calls themselves.  When
		 *  they return to this point (even with ENOSPC set), the
		 *  resource pause fields in the U area will have been cleared
		 *  by their previous calls on fspause() and no action will be
		 *  taken here.
		 */
		if (fspause(0))
		    uthread->uu_eosys = RESTARTSYS;
		error = uthread->uu_error;	/* can change in fspause */
		break;
	    }
	    /*
	     *  TODO:  Handle these error numbers, also.
	     */
	    case EAGAIN:
	    case ENOMEM:
	    case ENFILE:
		break;
	}
/* END CS_RPAUSE */

	if (uthread->uu_eosys == NORMALRETURN) {
normalreturn:
		if (error) {
			r->r_d0 = error;
			r->r_sr |= SR_C;	/* carry bit */
#if	SYSCALLTRACE
			if ((syscalltrace < -1? -syscalltrace : syscalltrace)
			    == u.u_procp->p_pid || syscalltrace == -1) {
				if (syscalltrace < -1)
					syscall_trace(syscode, callp);
				printf ("u.u_error = %d\n", error);
				XPR(XPR_SYSCALLS, ("u.u_error = %d\n", error));
			}
#endif	SYSCALLTRACE
		} else {
			r->r_dreg[0] = uthread->uu_r.r_val1;
			r->r_dreg[1] = uthread->uu_r.r_val2;
			r->r_sr &= ~SR_C;
		}
	} else
	if (uthread->uu_eosys == RESTARTSYS) {
		r->r_pc -= 2;
		while (thread_should_halt(thread))
			thread_halt_self();
	}
	else
	ASSERT(uthread->uu_eosys == JUSTRETURN);
done:
	if (thread->pcb->pcb_flags & TRACE_PENDING) {
		dotrace (r);
	}
check_sig:
	if (CHECK_SIGNALS(p, thread, uthread)) {
		unix_master();
		if (p->p_cursig || issig())
			psig();
		unix_release();
	}
#if	NCPUS > 1
	/*
	 *	It is OK to go back on a slave
	 */
	if (callp->sy_parallel == 0) {
		unix_release();
		ASSERT(thread->unix_lock == -1);
	}
#endif	NCPUS > 1
	if (csw_needed(thread, current_processor())) {
		u.u_ru.ru_nivcsw++;
		thread_block();
		if (CHECK_SIGNALS(p, thread, uthread)) {
			/*
			 * A signal's come it since we blocked,
			 * go back and handle them again.
			 */
			goto check_sig;
		}
	}
	if (utask->uu_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &utask->uu_ru.ru_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
//			simple_lock(u.u_prof.pr_lock);
			addupc(r->r_pc, &utask->uu_prof, ticks);
//			simple_unlock(u.u_prof.pr_lock);
		}
	}
}

/*
 * nonexistent system call-- signal process (may want to handle it)
 * flag error if process won't see signal immediately
 * Q: should we do that all the time ??
 */
nosys()
{
	if (u.u_signal[SIGSYS] == SIG_IGN || u.u_signal[SIGSYS] == SIG_HOLD)
		u.u_error = EINVAL;
	thread_doexception(current_thread(), EXC_SOFTWARE,
		EXC_UNIX_BAD_SYSCALL, 0);
}

/*
 * Process trace traps and AST's
 */
dotrace (r)
	register struct regs *r;
{
	register int ast_trace, s;
	register struct proc *p = u.u_procp;
	register struct pcb *pcb;

	s = spl6();	/* FIXME: really this high? */
	pcb = current_thread()->pcb;
	ast_trace = pcb->pcb_flags & AST_CLR;
	pcb->pcb_flags &= ~AST_CLR;
	r->r_sr &= ~SR_TSINGLE;
	(void) splx(s);

	if (ast_trace & TRACE_AST) {	/* AST is tracing */
		while (thread_should_halt(current_thread()))
			thread_halt_self();

		if ((p->p_flag&SOWEUPC) && u.u_prof.pr_scale) {
			addupc(r->r_pc, &u.u_prof, 1);
			p->p_flag &= ~SOWEUPC;
		}
		if ((ast_trace & TRACE_USER) == 0)
			return;		/* user isn't tracing */
	}
	thread_doexception(current_thread(), EXC_BREAKPOINT, 0, 0);
}

#import <next/cframe.h>

traceback (fp)
register struct frame *fp;
{
	extern int intstack, eintstack;

	if (traceback_recursive) {
		printf ("traceback: recursive call!\n");
		longjmp (&traceback_jb[0]);
	} else
		traceback_recursive = 1;

	printf ("traceback: fp 0x%x\n", fp);
	while ( ((int)fp & 1) == 0
		&& (  (fp >= (struct frame *)&intstack
		      && fp <= (struct frame *)&eintstack)
		   || (fp >= (struct frame *)VM_MIN_KERNEL_ADDRESS
		      && fp <= (struct frame *)VM_MAX_KERNEL_ADDRESS))
	) {
		printf ("called from pc %08x fp %08x 4-args"
		    " %08x %08x %08x %08x\n", fp->f_pc, fp->f_fp,
		    (&fp->f_arg)[0], (&fp->f_arg)[1], (&fp->f_arg)[2],
		    (&fp->f_arg)[3]);
		if (fp == fp->f_fp) {
			printf ("LOOPING fp\n");
			break;
		}
		fp = fp->f_fp;
	}
	printf ("last fp 0x%x\n", fp);
	traceback_recursive = 0;
}

/*
 *  68040 routine to complete pending memory writebacks.
 *  Called after page fault processing is completed.
 *  The "move space" instruction is used because the
 *  write must use the original transfer mode (which
 *  might not be supervisor data).
 */
do_writeback (r)
	struct regs *r;
{
	struct excp_access *e = (struct excp_access*) (r + 1);
	u_int d;
	static struct {
		u_char	lockout2:1,
			lockout3:1;
	} wb;

#if	DEBUG
	if (e->e_tt != TT_NORM) {
		printf ("e_tt %d\n", e->e_tt);
		panic ("do_writeback");
	}
#endif	DEBUG

	/*
	 *  Writeback data #1 is memory aligned -- undo it.
	 *  This writeback should never cause another access fault
	 *  because it corresponds to the faulted access just fixed up.
	 */
	if (e->e_wb1s.wbs_v) {
#if	DEBUG
		if (e->e_wb1s.wbs_tt != TT_NORM) {
			printf ("wb1s_tt %d\n", e->e_wb1s.wbs_tt);
			panic ("do_writeback");
		}
#endif	DEBUG
		d = e->e_wb1d_pd0;
		switch (e->e_wb1s.wbs_size) {

		case SIZE_BYTE:
			switch (e->e_wb1a & 0x3) {
			case 0:  d = d >> 24;  break;
			case 1:  d = d >> 16;  break;
			case 2:  d = d >> 8;  break;
			case 3:  d = d;  break;
			}
			asm ("nop");	/* flush pipeline */
			move_space (e->e_wb1a, e->e_wb1s.wbs_tm, SIZE_BYTE, d);
			break;

		case SIZE_WORD:
			switch (e->e_wb1a & 0x3) {
			case 0:  d = d >> 16;  break;
			case 1:  d = d >> 8;  break;
			case 2:  d = d;  break;
			case 3:  d = (d << 8) | (d >> 24);  break;
			}
			asm ("nop");	/* flush pipeline */
			move_space (e->e_wb1a, e->e_wb1s.wbs_tm, SIZE_WORD, d);
			break;

		case SIZE_LONG:
			d = e->e_wb1d_pd0;
			switch (e->e_wb1a & 0x3) {
			case 0:  d = d;  break;
			case 1:  d = (d << 8) | (d >> 24);  break;
			case 2:  d = (d << 16) | (d >> 16);  break;
			case 3:  d = (d << 24) | (d >> 8);  break;
			}
			asm ("nop");	/* flush pipeline */
			move_space (e->e_wb1a, e->e_wb1s.wbs_tm, SIZE_LONG, d);
			break;
		}
	}
	
	/*
	 *  An access fault may occur for writeback 2 and 3.
	 *  Because the new exception frame will also indicate
	 *  a pending writeback 2 and/or 3, we must lockout
	 *  continued processing of the writeback.  Use a "nop"
	 *  instruction before the writeback to flush the pipe
	 *  to keep any subsequent exception frame as simple as possible.
	 */

	/* writeback data #2 */
	if (e->e_wb2s.wbs_v && wb.lockout2 == 0) {
#if	DEBUG
		if (e->e_wb2s.wbs_tt != TT_NORM) {
			printf ("wb2s_tt %d\n", e->e_wb2s.wbs_tt);
			panic ("do_writeback");
		}
#endif	DEBUG
		wb.lockout2 = 1;
		asm ("nop");	/* flush pipeline */
		move_space (e->e_wb2a, e->e_wb2s.wbs_tm,
			e->e_wb2s.wbs_size, e->e_wb2d);
		wb.lockout2 = 0;
	}

	/* writeback data #3 */
	if (e->e_wb3s.wbs_v && wb.lockout3 == 0) {
#if	DEBUG
		if (e->e_wb3s.wbs_tt != TT_NORM) {
			printf ("wb3s_tt %d\n", e->e_wb3s.wbs_tt);
			panic ("do_writeback");
		}
#endif	DEBUG
		wb.lockout3 = 1;
		asm ("nop");	/* flush pipeline */
		move_space (e->e_wb3a, e->e_wb3s.wbs_tm,
			e->e_wb3s.wbs_size, e->e_wb3d);
		wb.lockout3 = 0;
	}
}

#if	FP_DEBUG
int fp;

fp_trace (action, p, p1, p2)
{
	switch (action) {
		case 7:
			u.u_fptrace.unsupp++; 
			if ((*(int*)p1 & 0x7fff0000) == 0 && 
					(*(int*)(p1+4) & 0x80000000) == 0)
				u.u_fptrace.denorm++; 
			else if ((*(int*)p1 & 0x7fff0000) != 0 && 
					(*(int*)(p1+4) & 0x80000000) == 0)
				u.u_fptrace.unnorm++; 
			break;
		case 6:
		case 8: 	
			u.u_fptrace.unimp[p]++; 
			break;
		case 10:	
			u.u_fptrace.bsun++;	
			break;
		case 11:
			u.u_fptrace.inex++;
			break;
		case 12:
			u.u_fptrace.dz++;
			break;
		case 13:
			u.u_fptrace.unfl++;
			break;
		case 14:
			u.u_fptrace.operr++;
			break;
		case 15:
			u.u_fptrace.ovfl++;
			break;
		case 16:
			u.u_fptrace.snan++;
			break;
		default:
			break;
	}
	if (fp == 0)
		return;
	if (fp < 0 && u.u_procp->p_pid != -fp)
			return;
	printf ("fp_emul(%s, %d): ", u.u_comm, u.u_procp->p_pid);
	switch (action) {
	case 5:  printf ("fp_usr exception\n");  break;
	case 6:  
		 if (fp < 0 || fp & 0x2)
			printf ("unimp 0x%x at 0x%x sop 0x%x 0x%x 0x%x\n",
				p, p1, *(int*)p2, *(int*)(p2+4),
				*(int*)(p2+8));
		 break;
	case 8:  if (fp < 0 || fp & 0x2)
			printf ("unimp out 0x%x 0x%x 0x%x\n",
				*(int*)p, *(int*)(p+4), *(int*)(p+8));
		 break;
	case 7:  if (fp > 0 && (fp & 0x4) == 0)
			break;
		printf ("unsupp at 0x%x sop 0x%x 0x%x 0x%x ",
		p, *(int*)p1, *(int*)(p1+4), *(int*)(p1+8));
		if ((*(int*)p1 & 0x7fff0000) == 0 && (*(int*)(p1+4) & 0x80000000) == 0)
			printf ("(denorm)\n");
		else
		if ((*(int*)p1 & 0x7fff0000) != 0 && (*(int*)(p1+4) & 0x80000000) == 0)
			printf ("(unnorm)\n");
		else
			printf ("(not a denorm or unnorm!)\n");
		break;
	case 9:
		if (fp < 0 || fp & 0x4)
			printf ("unsupp exit usp 0x%x\n", p);
		break;
	case 10: printf ("x_bsun at 0x%x\n", p);  break;
	case 11: printf ("x_inex at 0x%x\n", p);  break;
	case 12: printf ("x_dz at 0x%x\n", p);  break;
	case 13: printf ("x_unfl at 0x%x\n", p);  break;
	case 14: printf ("x_operr at 0x%x da 0x%x\n", p, p1);  break;
	case 15: printf ("x_ovfl at 0x%x\n", p);  break;
	case 16: printf ("x_snan at 0x%x\n", p);  break;
	default:  printf ("unknown fp_trace action!\n");  break;
	}
}
#endif	FP_DEBUG
