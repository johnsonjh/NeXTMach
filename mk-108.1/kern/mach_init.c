/* 
 *
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 14-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Changes for new scheduler:
 *		Deleted user_suspend_count field.
 */

#import <mach_net.h>
#import <mach_xp.h>
#import <xpr_debug.h>

#import <sys/machine.h>
#import <sys/version.h>
#import <sys/proc.h>
#import <kern/thread.h>
#import <kern/task.h>
#import <sys/types.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>

/*
 * Initialization code.
 * Called from cold start routine as
 * soon as a stack and segmentation
 * have been established.
 * Functions:
 *	clear and free user core
 *	turn on clock
 *	hand craft 0th process
 *	call all initialization routines
 *	fork - process 0 to schedule
 *	     - process 1 execute bootstrap
 *	     - process 2 to page out
 */
thread_t	first_thread;

thread_t setup_main()
/*
 *	first_addr contains the first available physical address
 *	running in virtual memory on the interrupt stack
 *
 *	returns initial thread to run
 */
{
	extern vm_offset_t	virtual_avail;
	vm_offset_t		end_stack, cur_stack;
	int			i;
	extern void	initial_context();
	extern void	vm_mem_init();
#if	NeXT && DEBUG
	extern void	init_eventclock();
#endif	NeXT && DEBUG
#if	MACH_NET
	extern void	mach_net_init();
#endif	MACH_NET

	rqinit();
	sched_init();
#import <loop.h>

	vm_mem_init();

#if	NeXT && DEBUG
	init_eventclock();	/* after kernel mem up, before timers run */
#endif	NeXT && DEBUG
	
	init_timers();

#if	XPR_DEBUG
	xprbootstrap();
#endif	XPR_DEBUG

	startup(virtual_avail);

	machine_info.max_cpus = NCPUS;
	machine_info.memory_size = roundup(mem_size, 1024*1024);
	machine_info.avail_cpus = 0;
	machine_info.major_version = KERNEL_MAJOR_VERSION;
	machine_info.minor_version = KERNEL_MINOR_VERSION;

	/*
	 *	Create stacks for other processors (the first
	 *	processor up uses a preallocated stack).
	 */

	cur_stack = kmem_alloc(kernel_map, NCPUS*INTSTACK_SIZE);
	end_stack = cur_stack + round_page(NCPUS*INTSTACK_SIZE);
	for (i = 0; i < NCPUS; i++) {
		if (machine_slot[i].is_cpu) {
			if (i != master_cpu) {
				interrupt_stack[i] = cur_stack;
				cur_stack += INTSTACK_SIZE;
			}
		}
		else {
			interrupt_stack[i] = (vm_offset_t) 0;
		}
	}

	/*
	 *	Free up any stacks we really didn't need.
	 */

	cur_stack = round_page(cur_stack);
	if (end_stack != cur_stack)
		kmem_free(kernel_map, cur_stack, end_stack - cur_stack);

	/*
	 *	Initialize the task and thread subsystems.
	 */

	uzone_init();

	ipc_bootstrap();

	cpu_up(master_cpu);	/* signify we are up */
#if	MACH_NET
	mach_net_init();
#endif	MACH_NET
	task_init();
	thread_init();
	swapper_init();
	ipc_init();
	vnode_pager_init();
#if	MACH_XP
	vm_pager_init();
#endif	MACH_XP
#if	MACH_HOST
	pset_sys_init();
#endif	MACH_HOST

	(void) thread_create(kernel_task, &first_thread);
	initial_context(first_thread);
	first_thread->state = TH_RUN;
	first_thread->user_stop_count = 0;
	first_thread->suspend_count = 0;
	first_thread->ipc_kernel = TRUE;
	(void) thread_resume(first_thread);

	/*
	 *	Tell the pmap system that our cpu is using the kernel pmap
	 */
	PMAP_ACTIVATE(kernel_pmap, first_thread, cpu_number());

	/*
	 *	Return to assembly code to start the first process.
	 */

	return(first_thread);
}
