/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 17-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Added AST_ZILCH and THREAD_AST in place of
 *	THREAD_EXIT_CODE and THREAD_EXIT.
 *
 * Revision 2.9  89/10/11  14:54:58  dlb
 * 	Get rid of THREAD_EXIT_CODE and THREAD_EXIT.
 * 	[89/02/02            dlb]
 * 
 * 27-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	Removed dir.h
 *
 * 16-Feb-88  John Seamons (jks) at NeXT
 *	Updated to Mach release 2.
 *
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#import <stdio.h>
#import <sys/param.h>
#import <sys/buf.h>
#import <sys/vmmeter.h>
#import <sys/vmparam.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <sys/mbuf.h>
#import <sys/msgbuf.h>
#import <vm/vm_prot.h>
#import <next/reg.h>
#import <next/psl.h>
#import <next/pcb.h>

thread_t	active_threads[1];

main () {
	struct longbus {
		struct regs lr;
		struct excp_longbus lb;
	};
	struct regs *r = (struct regs *) 0;
	struct longbus *l = (struct longbus *) 0;
	struct proc *p = (struct proc *) 0;
	struct vmmeter *vm = (struct vmmeter *) 0;
	struct user *up = (struct user *) 0;
	struct rusage *rup = (struct rusage *) 0;
	struct thread *thread = (struct thread *) 0;
	struct task *task = (struct task *) 0;
	struct pcb *pcb = (struct pcb *) 0;
	struct utask *utask = (struct utask *) 0;
	struct uthread *uthread = (struct uthread *) 0;
	struct access {
		struct regs ar;
		struct excp_access ae;
	};
	struct access *a = (struct access *) 0;

	printf("#ifndef	_ASSYM_\n");
	printf("#define	_ASSYM_\n");

	/* reg.h */
	printf("#define\tREGS %d\n", &r->r_dreg[0]);
	printf("#define\tR_D0 %d\n", &r->r_dreg[0]);
	printf("#define\tR_SP %d\n", &r->r_sp);
	printf("#define\tFSIZE %d\n", &r->r_fsize);
	printf("#define\tR_SR %d\n", &r->r_sr);
	printf("#define\tR_PC %d\n", &r->r_pc);
	printf("#define\tFMT %d\n", ((int) &r->r_pc) + sizeof (int));
	printf("#define\tVOR %d\n", ((int) &r->r_pc) + sizeof (int));
	printf("#define\tFAKE_SIZE %d\n", ((int) &r->r_pc) + sizeof (int) +
		sizeof (short));
	printf("#define\tSSW %d\n", &l->lb.e_ss);
	printf("#define\tFAULTADDR %d\n", &l->lb.e_faultaddr);
	printf("#define\tSTAGEB %d\n", &l->lb.e_stagebaddr);
	printf("#define\tSIZEOF_NORMAL6 %d\n",
		sizeof (struct excp_normal6));
	printf("#define\tSIZEOF_SHORTBUS %d\n",
		sizeof (struct excp_shortbus));
	printf("#define\tSIZEOF_LONGBUS %d\n",
		sizeof (struct excp_longbus));
	printf("#define\tSIZEOF_ACCESS %d\n",
		sizeof (struct excp_access));
	printf("#define\tFAULTD_BIT %d\n", bitnum (SS_FAULTD));
	printf("#define\tFAULTB_BIT %d\n", bitnum (SS_FAULTB));
	printf("#define\tFAULTC_BIT %d\n", bitnum (SS_FAULTC));
	printf("#define\tREAD_BIT %d\n", bitnum (SS_READ));
	printf("#define\tSSW_040 %d\n", &a->ae.e_ss);
	printf("#define\tFAULTADDR_040 %d\n", &a->ae.e_faultaddr);
	printf("#define\tATC_BIT %d\n", bitnum (SS_ATC));
	printf("#define\tRW_BIT %d\n", bitnum (SS_RW));
	printf("#define\tMA_BIT %d\n", bitnum (SS_MA));
	printf("#define\tLK_BIT %d\n", bitnum (SS_LK));

	printf("#ifdef ASSEMBLER\n");

	/* pcb.h */
	printf("#define\tSIZEOF_PCB %d\n", sizeof (struct pcb));
	printf("#define\tFLAGS %d\n", &pcb->pcb_flags);
	printf("#define\tAST %d\n", &pcb->pcb_flags);
	printf("#define\tTT1 %d\n", &pcb->mmu_tt1);
	printf("#define\tPCB_TT1 %d\n", bitnum (PCB_TT1));
	printf("#define\tAST_SCHED_BIT %d\n", bitnum (AST_SCHEDULE));
	printf("#define\tTRACE_AST_BIT %d\n", bitnum (TRACE_AST));
	printf("#define\tTRACE_USER_BIT %d\n", bitnum (TRACE_USER));
	printf("#define\tFPC_INT %d\n", &pcb->pcb_fpc_i);
	printf("#define\tFPC_EXT %d\n", &pcb->pcb_fpc_e.cr);
	printf("#define\tFPC_REGS %d\n", &pcb->pcb_fpc_e.regs[0]);
	printf("#define\tTHREAD_USER_REG %d\n", &pcb->thread_user_reg);
	printf("#define\tSAVED_REGS %d\n", &pcb->saved_regs);

	/* psl.h */
	printf("#define\tSR_TSINGLE_BIT %d\n", bitnum (SR_TSINGLE));
	printf("#define\tSR_SUPER_BIT %d\n", bitnum (SR_SUPER));

	/* vm_prot.h */
	printf("#define\tVM_PROT_READ %d\n", VM_PROT_READ);
	printf("#define\tVM_PROT_WRITE %d\n", VM_PROT_WRITE);

	/* FIXME: order rest by include filename */
	printf("#define\tP_LINK %d\n", &p->p_link);
	printf("#define\tP_RLINK %d\n", &p->p_rlink);
	printf("#define\tP_PRI %d\n", &p->p_pri);
	printf("#define\tP_STAT %d\n", &p->p_stat);
	printf("#define\tP_CURSIG %d\n", &p->p_cursig);
	printf("#define\tP_SIG %d\n", &p->p_sig);
	printf("#define\tP_FLAG %d\n", &p->p_flag);
	printf("#define\tSSLEEP %d\n", SSLEEP);
	printf("#define\tSRUN %d\n", SRUN);
	printf("#define\tV_SWTCH %d\n", &vm->v_swtch);
	printf("#define\tV_TRAP %d\n", &vm->v_trap);
	printf("#define\tV_SYSCALL %d\n", &vm->v_syscall);
	printf("#define\tV_INTR %d\n", &vm->v_intr);
	printf("#define\tV_SOFT %d\n", &vm->v_soft);
	printf("#define\tV_PDMA %d\n", &vm->v_pdma);
	printf("#define\tV_FAULTS %d\n", &vm->v_faults);
	printf("#define\tV_PGREC %d\n", &vm->v_pgrec);
	printf("#define\tV_FASTPGREC %d\n", &vm->v_fastpgrec);
	printf("#define\tNMBCLUSTERS %d\n", NMBCLUSTERS);
 	printf("#define\tU_PROCP %d\n", &utask->uu_procp);
 	printf("#define\tU_RU %d\n", &utask->uu_ru);
	printf("#define\tRU_MINFLT %d\n", &rup->ru_minflt);
 	printf("#define\tU_ERROR %d\n", &uthread->uu_error);
 	printf("#define\tU_AR0 %d\n", &uthread->uu_ar0);
	printf("#define\tTHREAD_PCB %d\n", &thread->pcb);
	printf("#define\tTHREAD_RECOVER %d\n", &thread->recover);
	printf("#define\tTHREAD_AST %d\n", &thread->ast);
	printf("#define\tAST_ZILCH %d\n", AST_ZILCH);
	printf("#define\tUTHREAD %d\n", &thread->u_address.uthread);
	printf("#define\tUTASK %d\n", &thread->u_address.utask);
	printf("#else	ASSEMBLER\n");
	printf("asm(\"\tU_ARG = %d\");\n", uthread->uu_arg);
	printf("#endif	ASSEMBLER\n");
	printf("#endif	_ASSYM_\n");
}

bitnum (mask)
	register int mask;
{
	register int i;

	for (i = 0; i < 32; i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}
}


