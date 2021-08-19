/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	thread.c,v $
 * Revision 2.19  90/07/20  08:49:48  mrt
 * 	Add depression abort functionality to thread_abort.
 * 	[90/07/16            dlb]
 * 	Add max_priority init to kernel_thread().
 * 	[90/06/20            dlb]
 * 	Initialize priority and do first priority computation in
 * 	thread_create.  Default maximum priority to BASEPRI_USER
 * 	instead of current pset maximum; override still occurs if
 * 	pset maximum is lower.  Suggested by Gregg Kellogg at NeXT.
 * 	Take priority inits out of thread template.  Remove in transit
 * 	check from thread_max_priority.
 * 	[90/05/08            dlb]
 *
 * 30-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Fixed thread_info to return size of THREAD_SCHED_INFO_COUNT
 *	for sched_info request.
 *
 * 20-Jul-90  Brian Pinkerton (bpinker) at NeXT
 *	Use new kernel stack allocation routines in kernel_stack.c
 *
 * 24-May-90  Gregg Kellogg (gk) at NeXT
 *	Added MAXPRI_USER priority for setting the default thread max_priority.
 *
 * 18-May-90  Avadis Tevanian (avie) at NeXT
 *	Changed to use sensible priorities (higher numbers -> higher pri).
 *
 *  9-May-90  Gregg Kellogg (gk) at NeXT
 *	Picked up dlb's changes to max_priority setting in thread_create().
 *
 * 29-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Thread should inherit it's priority from the parent task, not use
 *	the system global value.
 *
 * 28-Mar-90  Doug Mitchell at NeXT
 *	Increased REAPER_RESERVED_MAX to 8.
 *
 * Revision 2.16  89/12/22  15:53:45  rpd
 * 	Check for inactive pset in thread_doassign.
 * 	[89/12/15            dlb]
 * 	Add second argument to thread_swapin.
 * 	[89/11/28            dlb]
 * 	Round user-supplied quanta for fixed priority threads up to
 * 	multiples of clock interrupt period.
 * 	[89/11/15            dlb]
 * 
 * 	Fix up AST_HALT code.
 * 	[89/11/13            dlb]
 * 	Put all fixed priority support code under MACH_FIXPRI flag.
 * 	Replace use of thread_freeze/unfreeze in thread_deallocate.
 * 	Use thread_change_psets in thread_doassign.
 * 	Use thread_sleep in thread_freeze.
 * 	[89/11/10            dlb]
 * 	Add missing success return value to thread_assign.
 * 	Remove zombie panic from thread_release.
 * 	[89/11/08            dlb]
 * 
 * Revision 2.15  89/11/20  11:24:16  mja
 * 	Round user-supplied quanta for fixed priority threads up to
 * 	multiples of clock interrupt period.
 * 	[89/11/15            dlb]
 * 
 * 	Fix up AST_HALT code.
 * 	[89/11/13            dlb]
 * 	Put all fixed priority support code under MACH_FIXPRI flag.
 * 	Replace use of thread_freeze/unfreeze in thread_deallocate.
 * 	Use thread_change_psets in thread_doassign.
 * 	Use thread_sleep in thread_freeze.
 * 	[89/11/10            dlb]
 * 	Add missing success return value to thread_assign.
 * 	Remove zombie panic from thread_release.
 * 	[89/11/08            dlb]
 * 
 * Revision 2.14  89/10/11  14:32:13  dlb
 * 	Make thread_abort abort exceptions too.
 * 	Add thread_deallocate_interrupt for use in fixing a nasty
 * 	       multiprocessor signal problem.  Use thread_list_lock for
 * 	       same reason.
 * 	New flavor of thread_info.
 * 	Add priority inits.  Reset priorities on assignment if needed.
 * 	Add thread_priority, thread_max_priority, thread_policy
 * 	HW_FOOTPRINT: initialize last_processor.
 * 	Major rewrites to thread_halt and thread_terminate to deal with
 * 	       potential races and deadlocks.  thread_abort also affected.
 * 	Add assign_active to avoid extra wakeups in assign logic.
 * 	Fix thread_halt_self to not take extra hold on thread; thread_halt
 * 	       already holds thread instead.
 * 	Processor logic changes.
 * 	Rewrote exit logic to use new ast mechanism.
 * 
 * Revision 2.13  89/10/10  10:54:54  mwyoung
 * 	Initialize per-thread global VM variables.
 * 	Clean up global variables in thread_deallocate().
 * 	[89/04/29            mwyoung]
 * 
 * Revision 2.12  89/06/27  00:25:04  rpd
 * 	Added support for THREAD_KERNEL_PORT.
 * 	[89/06/26  23:57:10  rpd]
 * 
 * Revision 2.11  89/05/06  02:57:22  rpd
 * 	Purged <kern/thread_statistics.h>.
 * 	[89/05/05  20:42:35  rpd]
 * 
 * Revision 2.10  89/04/18  16:43:10  mwyoung
 * 	Add a template for thread initialization.  Many fields were
 * 	previously not initialized at all.
 * 
 * 	Handle resource shortage in thread_create().
 * 	[89/02/15            mwyoung]
 * 
 * Revision 2.6.2.1  89/03/15  01:02:40  mwyoung
 * 	Add a template for thread initialization.  Many fields were
 * 	previously not initialized at all.
 * 
 * 	Handle resource shortage in thread_create().
 * 	[89/02/15            mwyoung]
 * 
 * Revision 2.9  89/03/09  20:16:31  rpd
 * 	More cleanup.
 * 
 * Revision 2.8  89/02/25  18:09:44  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.7  89/01/15  16:27:51  rpd
 * 	Use decl_simple_lock_data, simple_lock_addr.
 * 	[89/01/15  15:08:25  rpd]
 * 
 * Revision 2.6  88/12/19  02:48:03  mwyoung
 * 	Fix include file references.
 * 	[88/12/19  00:24:52  mwyoung]
 * 	
 * 	Remove lint.
 * 	[88/12/09            mwyoung]
 * 
 * Revision 2.5  88/10/27  10:49:43  rpd
 * 	Some cleanup, mainly in the reaper.
 * 	[88/10/26  14:46:35  rpd]
 * 
 * Revision 2.4  88/08/25  18:19:10  mwyoung
 * 	Corrected include file references.
 * 	[88/08/22            mwyoung]
 * 	
 * 	Removed obsolete routines.
 * 	[88/08/11  18:48:39  mwyoung]
 * 
 * 15-Aug-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Lower stack_free_target to 2.
 *
 * 21-Jul-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Implement kernel_thread_noblock (non-blocking kernel thread
 *	creation used at interrupt time) by enhancing the reaper thread.
 *
 * Revision 2.3  88/08/06  18:26:41  rpd
 * Changed to use ipc_thread_lock/ipc_thread_unlock macros.
 * Eliminated use of kern/mach_ipc_defs.h.
 * Added definitions of all_threads, all_threads_lock.
 * 
 *  4-May-88  David Golub (dbg) and David Black (dlb) at CMU
 *	Remove vax-specific code.  Add register declarations.
 *	MACH_TIME_NEW now standard.  Moved thread_read_times to timer.c.
 *	SIMPLE_CLOCK: clock drift compensation in cpu_usage calculation.
 *	Initialize new fields in thread_create().  Implemented cpu usage
 *	calculation in thread_info().  Added argument to thread_setrun.
 *	Initialization changes for MACH_TIME_NEW.
 *
 * 13-Apr-88  David Black (dlb) at Carnegie-Mellon University
 *	Rewrite kernel stack retry code to eliminate races and handle
 *	termination correctly.
 *
 * 07-Apr-88  John Seamons (jks) at NeXT
 *	removed THREAD_CHUNK
 *
 * 19-Feb-88  David Kirschen (kirschen) at Encore Computer Corporation
 *      Retry if kernel stacks exhausted on thread_create
 *
 * 12-Feb-88  David Black (dlb) at Carnegie-Mellon University
 *	Fix MACH_TIME_NEW code.
 *
 *  1-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	In thread_halt: mark the victim thread suspended/runnable so that it
 *	will notify the caller when it hits the next interruptible wait.
 *	The victim may not immediately proceed to a clean point once it
 *	is awakened.
 *
 * 21-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Use new swapping state machine.  Moved thread_swappable to
 *	thread_swap.c.
 *
 * 17-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Added new thread interfaces: thread_suspend, thread_resume,
 *	thread_get_state, thread_set_state, thread_abort,
 *	thread_info.  Old interfaces remain (temporarily) for binary
 *	compatibility, prefixed with 'xxx_'.
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 * 15-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Made thread_reference and thread_deallocate check for null
 *	threads.  Call pcb_terminate when a thread is deallocated.
 *	Call pcb_init with thread pointer instead of pcb pointer.
 *	Add missing case to thread_dowait.
 *
 *  9-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Rewrote thread termination to have a terminating thread clean up
 *	after itself.
 *
 *  9-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Moved reaper invocation to thread_terminate from
 *	thread_deallocate.  [XXX temporary pending rewrite.]
 *
 *  8-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Added call to ipc_thread_disable.
 *
 *  4-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Set ipc_kernel in kernel_thread().
 *
 *  3-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Rewrote thread_create().  thread_terminate() must throw away
 *	an extra reference if called on the current thread [ref is
 *	held by caller who will not be returned to.]  Locking bug fix
 *	to thread_status.
 *
 * 19-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminated TT conditionals.
 *
 * 30-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Fix race condition in thread_deallocate for thread terminating
 *	itself.
 *
 * 23-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Correctly set thread_statistics fields.
 *
 * 13-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	Use counts for suspend and resume primitives.
 *
 *  5-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	MACH_TT: Completely replaced scheduling state machine.
 *
 * 30-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added initialization of thread->flags in thread_create().
 *	Added thread_swappable().
 *	De-linted.
 *
 * 30-Sep-87  David Black (dlb) at Carnegie-Mellon University
 *	Rewrote thread_dowait to more effectively stop threads.
 *
 * 11-Sep-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Initialize thread fields and unix_lock.
 *
 *  9-Sep-87  David Black (dlb) at Carnegie-Mellon University
 *	Changed thread_dowait to count a thread as stopped if it is
 *	sleeping and will stop immediately when woken up.  [i.e. is
 *	sleeping interruptibly].  Corresponding change to
 *	thread_terminate().
 *
 *  4-Aug-87  David Golub (dbg) at Carnegie-Mellon University
 *	Moved ipc_thread_terminate to thread_terminate (from
 *	thread_deallocate), to shut out other threads that are
 *	manipulating the thread via its thread_port.
 *
 * 29-Jul-87  David Golub (dbg) at Carnegie-Mellon University
 *	Make sure all deallocation calls are outside of locks - they may
 *	block.  Moved task_deallocate from thread_deallocate to
 *	thread_destroy, since thread may blow up if task disappears while
 *	thread is running.
 *
 * 26-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	Added update_priority() call to thread_release() for any thread
 *	actually released.
 *
 * 23-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	Initialize thread priorities in thread_create() and kernel_thread().
 *
 * 10-Jun-87  Karl Hauth (hauth) at Carnegie-Mellon University
 *	Added code to fill in the thread_statistics structure.
 *
 *  1-Jun-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added thread_statistics stub.
 *
 * 21-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Clear the thread u-area upon creation of a thread to keep
 *	consistent.
 *
 *  4-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Call uarea_init to initialize u-area stuff.
 *
 * 29-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Moved call to ipc_thread_terminate into the MACH_TT only branch
 *	to prevent problems with non-TT systems.
 *
 * 28-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Support the thread status information as a MiG refarray.
 *	[NOTE: turned off since MiG is still too braindamaged.]
 *
 * 23-Apr-87  Rick Rashid (rfr) at Carnegie-Mellon University
 *	Moved ipc_thread_terminate to thread_deallocate from
 *	thread_destroy to eliminate having the reaper call it after
 *	the task has been freed.
 *
 * 18-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added reaper thread for deallocating threads that cannot
 *	deallocate themselves (some time ago).
 *
 * 17-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	De-linted.
 *
 * 14-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Panic if no space left in the kernel map for stacks.
 *
 *  6-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Add kernel_thread routine which starts up kernel threads.
 *
 *  4-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Make thread_terminate work.
 *
 *  2-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	New kernel stack allocation mechanism.
 *
 * 27-Feb-87  David L. Black (dlb) at Carnegie-Mellon University
 *	MACH_TIME_NEW: Added timer inits to thread_create().
 *
 * 24-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Rewrote thread_suspend/thread_hold and added thread_wait for new
 *	user synchronization paradigm.
 *
 * 24-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Reorded locking protocol in thread_deallocate for the
 *	all_threads_lock (this allows one to reference a thread then
 *	release the all_threads_lock when scanning the thread list).
 *
 * 31-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Merged in my changes for real thread implementation.
 *
 * 30-Sep-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Make floating u-area work, maintain list of threads per task.
 *
 *  1-Aug-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added initialization for Mach-style IPC.
 *
 *  7-Jul-86  Rick Rashid (rfr) at Carnegie-Mellon University
 *	Added thread_in_use_chain to keep track of threads which
 *	have been created but not yet destroyed.
 *
 * 31-May-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Initialize thread state field to THREAD_WAITING.  Some general
 *	cleanup.
 *
 */
/*
 *	File:	kern/thread.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *					David Golub
 *
 *	Thread management primitives implementation.
 */

#import <cpus.h>
#import <hw_footprint.h>
#import <mach_host.h>
#import <mach_fixpri.h>
#import <simple_clock.h>

#import <kern/ast.h>
#import <kern/mach_param.h>
#import <sys/policy.h>
#import <kern/processor.h>
#import <kern/kernel_stack.h>
#import <kern/queue.h>
#import <kern/sched.h>
#import <kern/thread.h>
#import <sys/thread_status.h>
#import <sys/thread_info.h>
#import <sys/thread_special_ports.h>
#import <sys/time_value.h>
#import <kern/zalloc.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <kern/parallel.h>
#import <kern/ipc_tt.h>
#import <kern/sched_prim.h>
#import <kern/thread_swap.h>
#import <machine/machparam.h>		/* for splsched */

thread_t active_threads[NCPUS];
struct zone *thread_zone, *u_zone;

#if	!NeXT
queue_head_t		stack_queue;
int			stack_free_count = 0;
int			stack_free_target = 5;
decl_simple_lock_data(,	stack_queue_lock)
boolean_t		need_stack_wakeup = FALSE;
#endif	!NeXT

queue_head_t		reaper_queue;
decl_simple_lock_data(,	reaper_lock)

struct reaper_struct {
	queue_chain_t	links;		/* queue links - must be first */
	int		command;	/* what should reaper do */
	boolean_t	reserved;	/* allocated from reserved pool? */
	union {
		struct {
			thread_t	thread;
		} reap;
		struct {
			void		(*start)();
			task_t		task;
		} create;
	} args;
};

/*
 *	Reaper commands.
 */
#define	REAPER_REAP	1
#define REAPER_CREATE	2

#define REAPER_STRUCT_NULL	((struct reaper_struct *)0)

#define REAPER_RESERVED_MAX	8

struct reaper_struct	rps_reserved[REAPER_RESERVED_MAX];
queue_head_t		rps_reserved_queue;

extern int		tick;

#if	!NeXT
vm_offset_t stack_alloc()
{
	register vm_offset_t	stack;
	register boolean_t	msg_printed = FALSE;
	register kern_return_t	result = THREAD_AWAKENED;

	do {
	    simple_lock(&stack_queue_lock);
	    if (stack_free_count != 0) {
		stack = (vm_offset_t) dequeue_head(&stack_queue);
		stack_free_count--;
	    } else {
		stack = (vm_offset_t)0;
	    }
	    simple_unlock(&stack_queue_lock);

	    /*
	     *	If no stacks on queue, kmem_alloc one.  If that fails,
	     *	pause and wait for a stack to be freed.
	     */
	    if (stack == (vm_offset_t)0)
		stack = kmem_alloc(kernel_map, KERNEL_STACK_SIZE);

	    if (stack == (vm_offset_t)0) {
		if (!msg_printed ) {
		    msg_printed = TRUE;
		    uprintf("MACH: Out of kernel stacks, pausing...");
		    if (!need_stack_wakeup)
			printf("stack_alloc: Kernel stacks exhausted\n");
		}
		else if (result != THREAD_AWAKENED) {
		    /*
		     *	Somebody wants us; return a bogus stack.
		     */
		    return((vm_offset_t)0);
		}

		/*
		 *	Now wait for stack, but first make sure one
		 *	hasn't appeared in the interim.
		 */
		simple_lock(&stack_queue_lock);
		if(stack_free_count != 0) {
		    simple_unlock(&stack_queue_lock);
		    result = THREAD_AWAKENED;
		    continue;
		}
		assert_wait((int)&stack_queue, FALSE);
		need_stack_wakeup = TRUE;
		simple_unlock(&stack_queue_lock);
		thread_block();
		result = current_thread()->wait_result;
	    } else {
		if (msg_printed)
		    uprintf("continuing\n");		/* got a stack now */
		}
	} while (stack == (vm_offset_t)0);

	return(stack);
}

void stack_free(stack)
	vm_offset_t	stack;
{
	simple_lock(&stack_queue_lock);
	if (stack_free_count < stack_free_target) {
		stack_free_count++;
		enqueue_head(&stack_queue, (queue_entry_t) stack);
		stack = 0;
	}
	/*
	 * If anyone is waiting for a stack, wake them up.
	 */
	if (need_stack_wakeup) {
		need_stack_wakeup = FALSE;
		thread_wakeup((int)&stack_queue);
	}
	simple_unlock(&stack_queue_lock);

	if (stack != 0)
		kmem_free(kernel_map, stack, KERNEL_STACK_SIZE);
}
#endif	!NeXT

/* private */
struct thread	thread_template;

void thread_init()
{
	struct reaper_struct	*rs;
	register int i;

	thread_zone = zinit(
			sizeof(struct thread),
			THREAD_MAX * sizeof(struct thread),
#if	NeXT
			0,
#else	NeXT
			THREAD_CHUNK * sizeof(struct thread),
#endif	NeXT
			FALSE, "threads");

	/*
	 *	Fill in a template thread for fast initialization.
	 *	[Fields that must be (or are typically) reset at
	 *	time of creation are so noted.]
	 */

	/* thread_template.links (none) */
	thread_template.runq = RUN_QUEUE_NULL;

	/* thread_template.task (later) */
	/* thread_template.thread_list (later) */
	/* thread_template.pset_threads (later) */

	/* thread_template.lock (later) */
	thread_template.ref_count = 1;

	thread_template.pcb = (struct pcb *) 0;		/* (reset) */
	thread_template.kernel_stack = (vm_offset_t) 0;	/* (reset) */

	thread_template.wait_event = 0;
	/* thread_template.suspend_count (later) */
	thread_template.interruptible = TRUE;
	thread_template.wait_result = KERN_SUCCESS;
	thread_template.timer_set = FALSE;
	thread_template.wake_active = FALSE;
	thread_template.swap_state = TH_SW_IN;
	thread_template.state = TH_SUSP;

	thread_template.priority = BASEPRI_USER;
#if	NeXT
	thread_template.max_priority = MAXPRI_USER;
#else	NeXT
	thread_template.max_priority = BASEPRI_USER;
#endif	NeXT
	thread_template.sched_pri = BASEPRI_USER;
#if	MACH_FIXPRI
	thread_template.sched_data = 0;
	thread_template.policy = POLICY_TIMESHARE;
#endif	MACH_FIXPRI
	thread_template.depress_priority = -1;
	thread_template.cpu_usage = 0;
	thread_template.sched_usage = 0;
	/* thread_template.sched_stamp (later) */

	thread_template.recover = (vm_offset_t) 0;
	thread_template.vm_privilege = FALSE;
	thread_template.tmp_address = (vm_offset_t) 0;
	thread_template.tmp_object = VM_OBJECT_NULL;

	/* thread_template.u_address (later) */
	thread_template.unix_lock = -1;		/* XXX for Unix */

	thread_template.user_stop_count = 1;

	/* thread_template.<IPC structures> (later) */

	timer_init(&(thread_template.user_timer));
	timer_init(&(thread_template.system_timer));
	thread_template.user_timer_save.low = 0;
	thread_template.user_timer_save.high = 0;
	thread_template.system_timer_save.low = 0;
	thread_template.system_timer_save.high = 0;
	thread_template.cpu_delta = 0;
	thread_template.sched_delta = 0;

	thread_template.exception_port = PORT_NULL;
	thread_template.exception_clear_port = PORT_NULL;

	thread_template.active = FALSE; /* reset */
	thread_template.halted = FALSE;
	thread_template.ast = AST_ZILCH;

	/* thread_template.processor_set (later) */
	thread_template.bound_processor = PROCESSOR_NULL;
#if	MACH_HOST
	thread_template.may_assign = TRUE;
	thread_template.assign_active = FALSE;
#endif	MACH_HOST

#if	HW_FOOTPRINT
	/* thread_template.last_processor  (later) */
#endif	HW_FOOTPRINT

#if	NeXT
	thread_template.allocInProgress = FALSE;
#endif	NeXT
	/*
	 *	Initialize other data structures used in
	 *	this module.
	 */

#if	NeXT	
	initKernelStacks();
#else	NeXT
	queue_init(&stack_queue);
	simple_lock_init(&stack_queue_lock);
#endif	NeXT

	queue_init(&reaper_queue);
#if	NeXT
	queue_init(&rps_reserved_queue);
#endif	NeXT
	simple_lock_init(&reaper_lock);

	for (i = 0; i < REAPER_RESERVED_MAX; i++) {
		rs = &rps_reserved[i];
		rs->reserved = TRUE;
		enqueue_tail(&rps_reserved_queue, (queue_entry_t) rs);
	}
}

kern_return_t thread_create(parent_task, child_thread)
	register task_t	parent_task;
	thread_t	*child_thread;		/* OUT */
{
	register thread_t	new_thread;
	register int		s;
	register processor_set_t	pset;

	if (parent_task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Allocate a thread and initialize static fields
	 */

	new_thread = (thread_t) zalloc(thread_zone);

	if (new_thread == THREAD_NULL)
		return(KERN_RESOURCE_SHORTAGE);

	*new_thread = thread_template;

	/*
	 *	Initialize runtime-dependent fields
	 */

	new_thread->task = parent_task;
	simple_lock_init(&new_thread->lock);
	new_thread->sched_stamp = sched_tick;

	/*
	 *	Create a kernel stack, and put the PCB at the front.
	 */
#if	NeXT
	new_thread->kernel_stack = allocStack();
#else	NeXT
	new_thread->kernel_stack = stack_alloc();
#endif	NeXT

	/*
	 *	Only reason for stack_alloc failure is termination of
	 *	current thread.  Send back a return code anyway.
	 */
	if (new_thread->kernel_stack == 0) {
		zfree(thread_zone, (vm_offset_t) new_thread);
		return(KERN_RESOURCE_SHORTAGE);
	}

	new_thread->pcb = (struct pcb *) new_thread->kernel_stack;

	/*
	 *	Set up the u-address pointers.
	 */
	new_thread->u_address.uthread = (struct uthread *)
			(new_thread->kernel_stack + sizeof(struct pcb));
	uarea_zero(new_thread);
	new_thread->u_address.utask = parent_task->u_address;
	uarea_init(new_thread);

	pcb_init(new_thread, new_thread->kernel_stack);

	*child_thread = new_thread;

	ipc_thread_init(new_thread);
	new_thread->ipc_kernel = FALSE;

	task_lock(parent_task);
	pset = parent_task->processor_set;
	if (!pset->active) {
		pset = &default_pset;
	}

	new_thread->priority = parent_task->priority;
	/*
	 *	Don't need to lock thread here because it can't
	 *	possibly execute and no one else knows about it.
	 */
	compute_priority(new_thread);
  
	new_thread->suspend_count = parent_task->suspend_count + 1;
					/* account for start state */
	parent_task->ref_count++;

	new_thread->active = TRUE;

	pset_lock(pset);
	pset_add_thread(pset, new_thread);
	if (pset->empty)
		new_thread->suspend_count++;

#if	NeXT
	if (pset->max_priority < new_thread->max_priority)
		new_thread->max_priority = pset->max_priority;
  	if (new_thread->max_priority < new_thread->priority)
  		new_thread->priority = new_thread->max_priority;
#else	NeXT
	if (pset->max_priority > new_thread->max_priority)
		new_thread->max_priority = pset->max_priority;
	if (new_thread->max_priority > new_thread->priority)
		new_thread->priority = new_thread->max_priority;
#endif	NeXT

#if	HW_FOOTPRINT
	/*
	 *	Need to set last_processor, idle processor would be best, but
	 *	that requires extra locking nonsense.  Go for tail of
	 *	processors queue to avoid master.
	 */
	if (!(pset->empty)) {
		thread->last_processor = 
			(processor_t)queue_first(&pset->processors);
	}
	else {
		/*
		 *	Thread created in empty processor set.  Pick
		 *	master processor as an acceptable legal value.
		 */
		thread->last_processor = master_processor;
	}
#else	HW_FOOTPRINT
	/*
	 *	Don't need to initialize because the context switch
	 *	code will set it before it can be used.
	 */
#endif	HW_FOOTPRINT
	

	s = splsched();
	simple_lock(&parent_task->thread_list_lock);
	parent_task->thread_count++;
	queue_enter(&parent_task->thread_list, new_thread, thread_t,
					thread_list);
	simple_unlock(&parent_task->thread_list_lock);
	(void) splx(s);

	pset_unlock(pset);

	if (!parent_task->active) {
		task_unlock(parent_task);
		(void) thread_terminate(new_thread);
		return(KERN_FAILURE);
	}
	task_unlock(parent_task);

	ipc_thread_enable(new_thread);

	return(KERN_SUCCESS);
}

void thread_deallocate(thread)
	register thread_t	thread;
{
	int		s;
	register task_t	task;
	register processor_set_t	pset;

	time_value_t	user_time, system_time;

	extern void thread_depress_timeout();

	if (thread == THREAD_NULL)
		return;

	/*
	 *	First, check for new count > 0 (the common case).
	 *	Only the thread needs to be locked.
	 */
	s = splsched();
	thread_lock(thread);
	if (--thread->ref_count > 0) {
		thread_unlock(thread);
		(void) splx(s);
		return;
	}

	/*
	 *	Count is zero.  However, the task's and processor set's
	 *	thread lists have implicit references to
	 *	the thread, and may make new ones.  Their locks also
	 *	dominate the thread lock.  To check for this, we
	 *	temporarily restore the one thread reference, unlock
	 *	the thread, and then lock the other structures in
	 *	the proper order.
	 */
	thread->ref_count = 1;
	thread_unlock(thread);
	(void) splx(s);

	task = thread->task;
	task_lock(task);

	pset = thread->processor_set;
	pset_lock(pset);

#if	MACH_HOST
	/*
	 *	The thread might have moved.
	 */
	while (pset != thread->processor_set) {
	    pset_unlock(pset);
	    pset = thread->processor_set;
	    pset_lock(pset);
	}
#endif	MACH_HOST

	s = splsched();
	simple_lock(&task->thread_list_lock);
	thread_lock(thread);

	if (--thread->ref_count > 0) {
		/*
		 *	Task or processor_set made extra reference.
		 */
		thread_unlock(thread);
		simple_unlock(&task->thread_list_lock);
		(void) splx(s);
		pset_unlock(pset);
		task_unlock(task);
		return;
	}

	/*
	 *	Thread has no references - we can remove it.
	 */

	/*
	 *	Remove pending depress timeout if there is one.
	 */
	if (thread->depress_priority >= 0) {
#if	NCPUS > 1
	    if (!untimeout_try(thread_depress_timeout, thread)) {
		/*
		 *	Missed it, wait for timeout to happen.
		 */
		while (thread->depress_priority >= 0)
		    continue;
	    }
#else	NCPUS > 1
	    untimeout(thread_depress_timeout, thread);
#endif	NCPUS > 1
	}
	/*
	 *	Accumulate times for dead threads in task.
	 */

	thread_read_times(thread, &user_time, &system_time);
	time_value_add(&task->total_user_time, &user_time);
	time_value_add(&task->total_system_time, &system_time);

	/*
	 *	Remove thread from task list and processor_set threads list.
	 */
	task->thread_count--;
	queue_remove(&task->thread_list, thread, thread_t, thread_list);

	pset_remove_thread(pset, thread);

	thread_unlock(thread);		/* no more references - safe */
	simple_unlock(&task->thread_list_lock);
	(void) splx(s);
	pset_unlock(pset);
	pset_deallocate(pset);
	task_unlock(task);

	/*
	 *	Clean up global variables
	 */

	if (thread->tmp_address != (vm_offset_t) 0)
		kmem_free(kernel_map, thread->tmp_address, PAGE_SIZE);
	if (thread->tmp_object != VM_OBJECT_NULL)
		vm_object_deallocate(thread->tmp_object);

	/*
	 *	A couple of quick sanity checks
	 */

	if (thread == current_thread()) {
	    panic("thread deallocating itself");
	}
	if ((thread->state & ~(TH_RUN | TH_SWAPPED)) != TH_SUSP)
		panic("unstopped thread destroyed!");

	/*
	 *	Deallocate the task reference, since we know the thread
	 *	is not running.
	 */
	task_deallocate(thread->task);			/* may block */

	/*
	 *	Since there are no references left, we need not worry about
	 *	locking the thread.
	 */
	if (thread->state & TH_SWAPPED)
		thread_doswapin(thread);

	/*
	 *	Clean up any machine-dependent resources.
	 */
	pcb_terminate(thread);

#if	NeXT
	freeStack(thread->kernel_stack);
#else	NeXT
	stack_free(thread->kernel_stack);
#endif	NeXT
	zfree(thread_zone, (vm_offset_t) thread);
}
	
/*
 *	thread_deallocate_interrupt:
 *
 *	XXX special version of thread_deallocate that can be called from
 *	XXX interrupt level to solve a nasty problem in psignal().
 */

void thread_deallocate_interrupt(thread)
	register thread_t	thread;
{
	int		s;

	if (thread == THREAD_NULL)
		return;

	/*
	 *	First, check for new count > 0 (the common case).
	 *	Only the thread needs to be locked.
	 */
	s = splsched();
	thread_lock(thread);
	if (--thread->ref_count > 0) {
		thread_unlock(thread);
		(void) splx(s);
		return;
	}

	/*
	 *	Count is zero, but we can't actually free the thread
	 *	because that requires a task and a pset lock that
	 *	can't be held at interrupt level.  Since this was called
	 *	from interrupt level, we know the thread's reference to
	 *	itself is gone,	so it can't be running.  Similarly we know
	 *	it's not on the reaper's queue (else it would have
	 *	an additional reference).  Hence we can just put it
	 *	on the reaper's queue so that the reaper will get rid of
	 *	our reference for us.  We have to put that reference
	 *	back (of course).  As long as the thread is on the
	 *	reaper's queue, it will have a reference and hence can't
	 *	be requeued.
	 */

	thread->ref_count = 1;

	simple_lock(&reaper_lock);
	enqueue_tail(&reaper_queue, (queue_entry_t) thread);
	simple_unlock(&reaper_lock);

	thread_unlock(thread);
	(void) splx(s);

	thread_wakeup((int)&reaper_queue);
}


void thread_reference(thread)
	register thread_t	thread;
{
	int		s;

	if (thread == THREAD_NULL)
		return;

	s = splsched();
	thread_lock(thread);
	thread->ref_count++;
	thread_unlock(thread);
	(void) splx(s);
}

/*
 *	thread_terminate:
 *
 *	Permanently stop execution of the specified thread.
 *
 *	A thread to be terminated must be allowed to clean up any state
 *	that it has before it exits.  The thread is broken out of any
 *	wait condition that it is in, and signalled to exit.  It then
 *	cleans up its state and calls thread_halt_self on its way out of
 *	the kernel.  The caller waits for the thread to halt, terminates
 *	its IPC state, and then deallocates it.
 *
 *	If the caller is the current thread, it must still exit the kernel
 *	to clean up any state (thread and port references, messages, etc).
 *	When it exits the kernel, it then terminates its IPC state and
 *	queues itself for the reaper thread, which will wait for the thread
 *	to stop and then deallocate it.  (A thread cannot deallocate itself,
 *	since it needs a kernel stack to execute.)
 */
kern_return_t thread_terminate(thread)
	register thread_t	thread;
{
	register thread_t	cur_thread = current_thread();
	register task_t		cur_task;
	int	s;

	if (thread == THREAD_NULL)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Break IPC control over the thread.
	 */
	ipc_thread_disable(thread);

	if (thread == cur_thread) {

	    /*
	     *	Current thread will queue itself for reaper when
	     *	exiting kernel.
	     */
	    s = splsched();
	    thread_lock(thread);
	    if (thread->active) {
		    thread->active = FALSE;
		    thread_ast_set(thread, AST_TERMINATE);
	    }
	    thread_unlock(thread);
	    splx(s);
	    aston();
	    return(KERN_SUCCESS);
	}

	/*
	 *	Lock both threads and the current task
	 *	to check termination races and prevent deadlocks.
	 */
	cur_task = current_task();
	task_lock(cur_task);
	s = splsched();
	if ((int)thread < (int)cur_thread) {
		thread_lock(thread);
		thread_lock(cur_thread);
	}
	else {
		thread_lock(cur_thread);
		thread_lock(thread);
	}

	/*
	 *	If the current thread being terminated, help out.
	 */
	if ((!cur_task->active) || (!cur_thread->active)) {
		thread_unlock(cur_thread);
		thread_unlock(thread);
		(void) splx(s);
		task_unlock(cur_task);
		thread_terminate(cur_thread);
		return(KERN_FAILURE);
	}
    
	thread_unlock(cur_thread);
	task_unlock(cur_task);

	/*
	 *	Terminate victim thread.
	 */
	if (!thread->active) {
		/*
		 *	Someone else got there first.
		 */
		thread_unlock(thread);
		(void) splx(s);
		return(KERN_FAILURE);
	}

	thread->active = FALSE;

	thread_unlock(thread);
	(void) splx(s);

#if	MACH_HOST
	/*
	 *	Reassign thread to default pset if needed.
	 */
	thread_freeze(thread);
	if (thread->processor_set != &default_pset) {
		thread_doassign(thread, &default_pset, FALSE);
	}
#endif	MACH_HOST

	/*
	 *	Halt the victim at the clean point.
	 */
	(void) thread_halt(thread, TRUE);
#if	MACH_HOST
	thread_unfreeze(thread);
#endif	MACH_HOST
	/*
	 *	Shut down the victims IPC and deallocate its
	 *	reference to itself.
	 */
	ipc_thread_terminate(thread);
	thread_deallocate(thread);
	return(KERN_SUCCESS);
}

/*
 *	thread_force_terminate:
 *
 *	Version of thread_terminate called by task_terminate.  thread is
 *	not the current thread.  task_terminate is the dominant operation,
 *	so we can force this thread to stop.
 */
void
thread_force_terminate(thread)
register thread_t	thread;
{
	boolean_t	deallocate_here = FALSE;
	int s;

	ipc_thread_disable(thread);

#if	MACH_HOST
	/*
	 *	Reassign thread to default pset if needed.
	 */
	thread_freeze(thread);
	if (thread->processor_set != &default_pset)
		thread_doassign(thread, &default_pset, FALSE);
#endif	MACH_HOST

	s = splsched();
	thread_lock(thread);
	deallocate_here = thread->active;
	thread->active = FALSE;
	thread_unlock(thread);
	(void) splx(s);

	(void) thread_halt(thread, TRUE);
	ipc_thread_terminate(thread);

#if	MACH_HOST
	thread_unfreeze(thread);
#endif	MACH_HOST

	if (deallocate_here)
		thread_deallocate(thread);
}


/*
 *	Halt a thread at a clean point, leaving it suspended.
 *
 *	must_halt indicates whether thread must halt.
 *
 */
kern_return_t thread_halt(thread, must_halt)
	register thread_t	thread;
	boolean_t		must_halt;
{
	register thread_t	cur_thread = current_thread();
	register kern_return_t	ret;
	int	s;

	if (thread == cur_thread)
		panic("thread_halt: trying to halt current thread.");
	/*
	 *	If must_halt is FALSE, then a check must be made for
	 *	a cycle of halt operations.
	 */
	if (!must_halt) {
		/*
		 *	Grab both thread locks.
		 */
		s = splsched();
		if ((int)thread < (int)cur_thread) {
			thread_lock(thread);
			thread_lock(cur_thread);
		}
		else {
			thread_lock(cur_thread);
			thread_lock(thread);
		}

		/*
		 *	If target thread is already halted, grab a hold
		 *	on it and return.
		 */
		if (thread->halted) {
			thread->suspend_count++;
			thread_unlock(cur_thread);
			thread_unlock(thread);
			(void) splx(s);
			return(KERN_SUCCESS);
		}

		/*
		 *	If someone is trying to halt us, we have a potential
		 *	halt cycle.  Break the cycle by interrupting anyone
		 *	who is trying to halt us, and causing this operation
		 *	to fail; retry logic will only retry operations
		 *	that cannot deadlock.  (If must_halt is TRUE, this
		 *	operation can never cause a deadlock.)
		 */
		if (cur_thread->ast & AST_HALT) {
			thread_wakeup_with_result(&cur_thread->wake_active,
				THREAD_INTERRUPTED);
			thread_unlock(thread);
			thread_unlock(cur_thread);
			(void) splx(s);
			return(KERN_FAILURE);
		}

		thread_unlock(cur_thread);
	
	}
	else {
		/*
		 *	Lock thread and check whether it is already halted.
		 */
		s = splsched();
		thread_lock(thread);
		if (thread->halted) {
			thread->suspend_count++;
			thread_unlock(thread);
			(void) splx(s);
			return(KERN_SUCCESS);
		}
	}

	/*
	 *	Suspend thread - inline version of thread_hold() because
	 *	thread is already locked.
	 */
	thread->suspend_count++;
	thread->state |= TH_SUSP;

	/*
	 *	If someone else is halting it, wait for that to complete.
	 *	Fail if wait interrupted and must_halt is false.
	 */
	while ((thread->ast & AST_HALT) && (!thread->halted)) {
		thread->wake_active = TRUE;
		thread_sleep((int) &thread->wake_active,
			simple_lock_addr(thread->lock), TRUE);

		if (thread->halted) {
			(void) splx(s);
			return(KERN_SUCCESS);
		}
		if ((current_thread()->wait_result != THREAD_AWAKENED)
		    && !(must_halt)) {
			(void) splx(s);
			thread_release(thread);
			return(KERN_FAILURE);
		}
		thread_lock(thread);
	}

	/*
	 *	Otherwise, have to do it ourselves.
	 */
		
	thread_ast_set(thread, AST_HALT);

	while (TRUE) {
	  	/*
		 *	Wait for thread to stop.
		 */
		thread_unlock(thread);
		(void) splx(s);

		ret = thread_dowait(thread, must_halt);

		/*
		 *	If the dowait failed, so do we.  Drop AST_HALT, and
		 *	wake up anyone else who might be waiting for it.
		 */
		if (ret != KERN_SUCCESS) {
			s = splsched();
			thread_lock(thread);
			thread_ast_clear(thread, AST_HALT);
			thread_wakeup_with_result(&thread->wake_active,
				THREAD_INTERRUPTED);
			thread_unlock(thread);
			(void) splx(s);

			thread_release(thread);
			return(ret);
		}

		/*
		 *	Clear any interruptible wait.
		 */
		clear_wait(thread, THREAD_INTERRUPTED, TRUE);

		/*
		 *	If the thread's at a clean point, we're done.
		 *	Don't need a lock because it really is stopped.
		 */
		if (thread->halted) {
			return(KERN_SUCCESS);
		}

		/*
		 *	Force the thread to stop at a clean
		 *	point, and arrange to wait for it.
		 *
		 *	Set it running, so it can notice.  Override
		 *	the suspend count.  We know that the thread
		 *	is suspended and not waiting.
		 *
		 *	Since the thread may hit an interruptible wait
		 *	before it reaches a clean point, we must force it
		 *	to wake us up when it does so.  This involves some
		 *	trickery:
		 *	  We mark the thread SUSPENDED so that thread_block
		 *	will suspend it and wake us up.
		 *	  We mark the thread RUNNING so that it will run.
		 *	  We mark the thread UN-INTERRUPTIBLE (!) so that
		 *	some other thread trying to halt or suspend it won't
		 *	take it off the run queue before it runs.  Since
		 *	dispatching a thread (the tail of thread_block) marks
		 *	the thread interruptible, it will stop at the next
		 *	context switch or interruptible wait.
		 */

		s = splsched();
		thread_lock(thread);
		switch (thread->state) {

		    case TH_SUSP | TH_SWAPPED:
			thread->state = TH_SUSP | TH_SWAPPED | TH_RUN;
			thread->interruptible = FALSE;
			thread_swapin(thread, FALSE);
			break;

		    case TH_SUSP:
			thread->state = TH_SUSP | TH_RUN;
			thread->interruptible = FALSE;
			thread_setrun(thread, FALSE);
			break;

		    default:
			panic("thread_halt");
		}

		/*
		 *	Continue loop and wait for thread to stop.
		 */
	}
}

/*
 *	Thread calls this routine on exit from the kernel when it
 *	notices a halt request.
 */
void	thread_halt_self()
{
	register thread_t		thread = current_thread();
	register struct reaper_struct	*rs;
	int				s;

	if (thread->ast & AST_TERMINATE) {
		/*
		 *	Thread is terminating itself.  Shut
		 *	down IPC, then queue it up for the
		 *	reaper thread.
		 */
		ipc_thread_terminate(thread);

		thread_hold(thread);

		rs = (struct reaper_struct *)
			kalloc(sizeof(struct reaper_struct));
		s = splsched();
		simple_lock(&reaper_lock);
		rs->reserved = FALSE;
		rs->command = REAPER_REAP;
		rs->args.reap.thread = thread;
		enqueue_tail(&reaper_queue, (queue_entry_t) rs);
		simple_unlock(&reaper_lock);

		thread_lock(thread);
		thread->halted = TRUE;
		thread_unlock(thread);
		(void) splx(s);

		thread_wakeup((int)&reaper_queue);
		thread_block();
		panic("the zombie walks!");
		/*NOTREACHED*/
	}
	else {
		/*
		 *	Thread was asked to halt - show that it
		 *	has done so.
		 */
		s = splsched();
		thread_lock(thread);
		thread->halted = TRUE;
		thread_ast_clear(thread, AST_HALT);
		thread_unlock(thread);
		splx(s);
		thread_block();
		/*
		 *	thread_release resets thread->halted.
		 */
	}
}

/*
 *	thread_hold:
 *
 *	Suspend execution of the specified thread.
 *	This is a recursive-style suspension of the thread, a count of
 *	suspends is maintained.
 */
void thread_hold(thread)
	register thread_t	thread;
{
	int			s;

	s = splsched();
	thread_lock(thread);
	thread->suspend_count++;
	thread->state |= TH_SUSP;
	thread_unlock(thread);
	(void) splx(s);
}

/*
 *	thread_dowait:
 *
 *	Wait for a thread to actually enter stopped state.
 *
 *	must_halt argument indicates if this may fail on interruption.
 *	This is FALSE only if called from thread_abort via thread_halt.
 */
kern_return_t
thread_dowait(thread, must_halt)
	register thread_t	thread;
	boolean_t		must_halt;
{
	register boolean_t	need_wakeup;
	register kern_return_t	ret = KERN_SUCCESS;
	int			s;

	/*
	 *	If we are requested to wait for the thread to really
	 *	be stopped, and that thread is us, we need to context
	 *	switch (giving up execution, stopping ourselves).
	 */

	if (thread == current_thread()) {
		thread_block();
		return(KERN_SUCCESS);
	}

	/*
	 *	If a thread is not interruptible, it may not be suspended
	 *	until it becomes interruptible.  In this case, we wait for
	 *	the thread to stop itself, and indicate that we are waiting
	 *	for it to stop so that it can wake us up when it does stop.
	 *
	 *	If the thread is interruptible, we may be able to suspend
	 *	it immediately.  There are several cases:
	 *
	 *	1) The thread is already stopped (trivial)
	 *	2) The thread is runnable (marked RUN and on a run queue).
	 *	   We pull it off the run queue and mark it stopped.
	 *	3) The thread is running.  We wait for it to stop.
	 */

	need_wakeup = FALSE;
	s = splsched();
	thread_lock(thread);
	switch(thread->state) {
	    case TH_SUSP:
	    case TH_SUSP | TH_SWAPPED:
	    case TH_SUSP | TH_WAIT | TH_SWAPPED:
		/*
		 *	We win!  Since thread is suspended (without any
		 *	other states) or swapped out, it must be
		 *	interruptible.
		 */
		break;

	    case TH_RUN | TH_SUSP:
		/*
		 *	If the thread is interruptible, and we can pull
		 *	it off a runq, stop it here.
		 */
		if (thread->interruptible) {
		    if (rem_runq(thread) != RUN_QUEUE_NULL) {
			thread->state = TH_SUSP;
			need_wakeup = thread->wake_active;
			thread->wake_active = FALSE;
			break;
		    }
#if	NCPUS > 1
		    else {
			/*
			 *	The thread must be running, so make its
			 *	processor execute ast_check().  This
			 *	should cause the thread to take an ast and
			 *	context switch to suspend for us.
			 */
			cause_ast_check(thread->last_processor);
		    }
#endif	NCPUS > 1
		}
		/*
		 *	Fall through to wait for thread to stop.
		 */

	    case TH_RUN | TH_SUSP | TH_SWAPPED:
	    case TH_RUN | TH_WAIT | TH_SUSP:
	    case TH_WAIT | TH_SUSP:
		/*
		 *	Wait for the thread to stop, or sleep interruptibly
		 *	(thread_block will stop it in the latter case).
		 *	Check for failure if interrupted.
		 */
		while ((thread->state & (TH_RUN | TH_SUSP)) != TH_SUSP ||
		    !thread->interruptible) {
			thread->wake_active = TRUE;
			thread_sleep((int) &thread->wake_active,
				simple_lock_addr(thread->lock), TRUE);
			thread_lock(thread);
			if ((current_thread()->wait_result !=
			    THREAD_AWAKENED) && !(must_halt)) {
				ret = KERN_FAILURE;
				break;
			}
		    }
		break;
	}
	thread_unlock(thread);
	(void) splx(s);

	if (need_wakeup)
	    thread_wakeup((int) &thread->wake_active);

	return(ret);
}

void thread_release(thread)
	register thread_t	thread;
{
	int			s;

	s = splsched();
	thread_lock(thread);
	if (--thread->suspend_count == 0) {
		thread->halted = FALSE;
		thread->state &= ~TH_SUSP;
		switch (thread->state & (TH_WAIT | TH_RUN | TH_SWAPPED)) {

		    case TH_SWAPPED:
			thread->state |= TH_RUN;
			thread_swapin(thread, FALSE);
			break;

		    case 0:	/* was only suspended */
			thread->state |= TH_RUN;
			thread_setrun(thread, TRUE);
			break;

		    default:
			break;
		}
	}
	thread_unlock(thread);
	(void) splx(s);

}

kern_return_t thread_suspend(thread)
	register thread_t	thread;
{
	register boolean_t	hold;
	int			spl;

	if (thread == THREAD_NULL)
		return(KERN_INVALID_ARGUMENT);

	hold = FALSE;
	spl = splsched();
	thread_lock(thread);
	if ((thread->user_stop_count)++ == 0)
		hold = TRUE;
	thread_unlock(thread);
	(void) splx(spl);

	/*
	 *	Now hold and wait for the thread if necessary.
	 */
	if (hold) {
		thread_hold(thread);
		(void) thread_dowait(thread, TRUE);
	}
	return(KERN_SUCCESS);
}

kern_return_t thread_resume(thread)
	register thread_t	thread;
{
	register boolean_t	release;
	register kern_return_t	ret;
	int			s;

	if (thread == THREAD_NULL)
		return(KERN_INVALID_ARGUMENT);

	ret = KERN_SUCCESS;
	release = FALSE;

	s = splsched();
	thread_lock(thread);
	if (thread->user_stop_count > 0) {
		if (--(thread->user_stop_count) == 0)
			release = TRUE;
	}
	else {
		ret = KERN_FAILURE;
	}
	thread_unlock(thread);
	(void) splx(s);

	/*
	 *	Now release the thread if necessary.
	 */
	if (release)
		thread_release(thread);
	return(ret);
}

/*
 *	Return thread's machine-dependent state.
 */
kern_return_t thread_get_state(thread, flavor, old_state, old_state_count)
	register thread_t	thread;
	int			flavor;
	thread_state_t		old_state;	/* pointer to OUT array */
	unsigned int		*old_state_count;	/*IN/OUT*/
{
	kern_return_t		ret;

	if (thread == THREAD_NULL || thread == current_thread()) {
		return (KERN_INVALID_ARGUMENT);
	}

	thread_hold(thread);
	(void) thread_dowait(thread, TRUE);

	ret = thread_getstatus(thread, flavor, old_state, old_state_count);

	thread_release(thread);
	return(ret);
}

/*
 *	Change thread's machine-dependent state.
 */
kern_return_t thread_set_state(thread, flavor, new_state, new_state_count)
	register thread_t	thread;
	int			flavor;
	thread_state_t		new_state;
	unsigned int		new_state_count;
{
	kern_return_t		ret;

	if (thread == THREAD_NULL || thread == current_thread()) {
		return (KERN_INVALID_ARGUMENT);
	}

	thread_hold(thread);
	(void) thread_dowait(thread, TRUE);

	ret = thread_setstatus(thread, flavor, new_state, new_state_count);

	thread_release(thread);
	return(ret);
}

kern_return_t thread_info(thread, flavor, thread_info_out,
			thread_info_count)
	register thread_t	thread;
	int			flavor;
	thread_info_t		thread_info_out;	/* pointer to OUT array */
	unsigned int		*thread_info_count;	/*IN/OUT*/
{
	int	s, state, flags;

	if (thread == THREAD_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (flavor == THREAD_BASIC_INFO) {
	    register thread_basic_info_t	basic_info;

	    if (*thread_info_count < THREAD_BASIC_INFO_COUNT) {
		return(KERN_INVALID_ARGUMENT);
	    }

	    basic_info = (thread_basic_info_t) thread_info_out;

	    s = splsched();
	    thread_lock(thread);

	    /*
	     *	Update lazy-evaluated scheduler info because someone wants it.
	     */
	    if ((thread->state & TH_RUN) == 0 &&
		thread->sched_stamp != sched_tick)
		    update_priority(thread);

	    /* fill in info */

	    thread_read_times(thread,
			&basic_info->user_time,
			&basic_info->system_time);
	    basic_info->base_priority	= thread->priority;
	    basic_info->cur_priority	= thread->sched_pri;

	    /*
	     *	To calculate cpu_usage, first correct for timer rate,
	     *	then for 5/8 ageing.  The correction factor [3/5] is
	     *	(1/(5/8) - 1).
	     */
	    basic_info->cpu_usage = thread->cpu_usage /
					(TIMER_RATE/TH_USAGE_SCALE);
	    basic_info->cpu_usage = (basic_info->cpu_usage * 3) / 5;
#if	SIMPLE_CLOCK
	    /*
	     *	Clock drift compensation.
	     */
	    basic_info->cpu_usage =
		(basic_info->cpu_usage * 1000000)/sched_usec;
#endif	SIMPLE_CLOCK

	    if (thread->state & TH_SWAPPED)
		flags = TH_FLAGS_SWAPPED;
	    else if (thread->state & TH_IDLE)
		flags = TH_FLAGS_IDLE;
	    else
		flags = 0;

	    if (thread->halted)
		state = TH_STATE_HALTED;
	    else
	    if (thread->state & TH_RUN)
		state = TH_STATE_RUNNING;
	    else
	    if (!thread->interruptible)
		state = TH_STATE_UNINTERRUPTIBLE;
	    else
	    if (thread->state & TH_SUSP)
		state = TH_STATE_STOPPED;
	    else
	    if (thread->state & TH_WAIT)
		state = TH_STATE_WAITING;
	    else
		state = 0;		/* ? */

	    basic_info->run_state = state;
	    basic_info->flags = flags;
	    basic_info->suspend_count = thread->user_stop_count;
	    if (state == TH_STATE_RUNNING)
		basic_info->sleep_time = 0;
	    else
		basic_info->sleep_time = sched_tick - thread->sched_stamp;

	    thread_unlock(thread);
	    splx(s);

	    *thread_info_count = THREAD_BASIC_INFO_COUNT;
	    return(KERN_SUCCESS);
	}
	else if (flavor == THREAD_SCHED_INFO) {
	    register thread_sched_info_t	sched_info;

	    if (*thread_info_count < THREAD_SCHED_INFO_COUNT) {
		return(KERN_INVALID_ARGUMENT);
	    }

	    sched_info = (thread_sched_info_t) thread_info_out;

	    s = splsched();
	    thread_lock(thread);

#if	MACH_FIXPRI
	    sched_info->policy = thread->policy;
	    if (   thread->policy == POLICY_FIXEDPRI
#if	NeXT
		|| thread->policy == POLICY_INTERACTIVE
#endif	NeXT
	    ) {
		sched_info->data = (thread->sched_data * tick)/1000;
	    }
	    else {
		sched_info->data = 0;
	    }
#else	MACH_FIXPRI
	    sched_info->policy = POLICY_TIMESHARE;
	    sched_info->data = 0;
#endif	MACH_FIXPRI

	    sched_info->base_priority = thread->priority;
	    sched_info->max_priority = thread->max_priority;
	    sched_info->cur_priority = thread->sched_pri;
	    
	    sched_info->depressed = (thread->depress_priority >= 0);
	    sched_info->depress_priority = thread->depress_priority;

	    thread_unlock(thread);
	    splx(s);

	    *thread_info_count = THREAD_SCHED_INFO_COUNT;
	    return(KERN_SUCCESS);
	}

	return(KERN_INVALID_ARGUMENT);
}

kern_return_t	thread_abort(thread)
	register thread_t	thread;
{
	if (thread == THREAD_NULL || thread == current_thread()) {
		return (KERN_INVALID_ARGUMENT);
	}

	/*
	 *	Try to force the thread to a clean point
	 *	If the halt operation fails return KERN_ABORTED.
	 *	ipc code will convert this to an ipc interrupted error code.
	 */
	if (thread_halt(thread, FALSE) != KERN_SUCCESS)
		return(KERN_ABORTED);

	/*
	 *	If the thread was in an exception, abort that too.
	 */
	thread_exception_abort(thread);

	/*
	 *	Then set it going again.
	 */
	thread_release(thread);

	/*
	 *	Also abort any depression.
	 */
	if (thread->depress_priority != -1)
		thread_depress_abort(thread);

	return(KERN_SUCCESS);
}

kern_return_t thread_get_special_port(thread, which_port, port)
	register thread_t	thread;
	int		which_port;
	port_t		*port;
{
	register port_t	*portp;

	if (thread == THREAD_NULL)
		return(KERN_INVALID_ARGUMENT);

	switch (which_port) {
	    case THREAD_KERNEL_PORT:
		portp = &thread->thread_tself;
		break;
	    case THREAD_REPLY_PORT:
		portp = &thread->thread_reply;
		break;
	    case THREAD_EXCEPTION_PORT:
		portp = &thread->exception_port;
		break;
	    default:
		return(KERN_INVALID_ARGUMENT);
	}

	ipc_thread_lock(thread);
	if (thread->thread_self == PORT_NULL) {
		/* thread's IPC already inactive */
		ipc_thread_unlock(thread);
		return(KERN_FAILURE);
	}
	
	if ((*port = *portp) != PORT_NULL) {
		port_reference(*portp);
	}
	ipc_thread_unlock(thread);

	return(KERN_SUCCESS);
}

kern_return_t thread_set_special_port(thread, which_port, port)
	register thread_t	thread;
	int		which_port;
	port_t		port;
{
	register port_t	*portp;
	register port_t	old_port;

	if (thread == THREAD_NULL)
		return(KERN_INVALID_ARGUMENT);

	switch (which_port) {
	    case THREAD_KERNEL_PORT:
		portp = &thread->thread_tself;
		break;
	    case THREAD_REPLY_PORT:
		portp = &thread->thread_reply;
		break;
	    case THREAD_EXCEPTION_PORT:
		portp = &thread->exception_port;
		break;
	    default:
		return(KERN_INVALID_ARGUMENT);
	}

	ipc_thread_lock(thread);
	if (thread->thread_self == PORT_NULL) {
		/* thread's IPC already inactive */
		ipc_thread_unlock(thread);
		return(KERN_FAILURE);
	}
	
	old_port = *portp;
	if ((*portp = port) != PORT_NULL)
		port_reference(port);

	ipc_thread_unlock(thread);

	if (old_port != PORT_NULL)
		port_release(old_port);

	return(KERN_SUCCESS);
}

/*
 *	kernel_thread:
 *
 *	Start up a kernel thread in the specified task.  This version
 *	may block.
 */

thread_t kernel_thread(task, start)
	task_t	task;
	void	(*start)();
{
	thread_t	thread;

	(void) thread_create(task, &thread);
	thread_start(thread, start, THREAD_SYSTEMMODE);
	thread->priority = BASEPRI_SYSTEM;
	thread->sched_pri = BASEPRI_SYSTEM;
	thread->ipc_kernel = TRUE;
	(void) thread_resume(thread);
	return(thread);
}

/*
 *	kernel_thread_noblock:
 *
 *	Start up a kernel thread in the specified task.  This version
 *	will not block as it performs the creation asynchronously via
 *	the reaper_thread.  However, this version cannot return the
 *	thread that was created (since it doesn't know what it is).
 */

void kernel_thread_noblock(task, start)
	task_t	task;
	void	(*start)();
{
	register struct reaper_struct	*rs;
	int				s;

	s = splsched();
	rs = (struct reaper_struct *) kget(sizeof(struct reaper_struct));
	if (rs == REAPER_STRUCT_NULL) {
		rs = (struct reaper_struct *)dequeue_head(&rps_reserved_queue);
		if (rs == REAPER_STRUCT_NULL)
			panic("no reserved reaper structs\n");
	}
	else {
		rs->reserved = FALSE;
	}
	rs->command = REAPER_CREATE;
	rs->args.create.task = task;
	rs->args.create.start = start;
	simple_lock(&reaper_lock);
	enqueue_tail(&reaper_queue, (queue_entry_t) rs);
	simple_unlock(&reaper_lock);
	(void) splx(s);
	thread_wakeup((int)&reaper_queue);
}

/*
 *	reaper_thread:
 *
 *	This kernel thread runs forever looking for threads to destroy
 *	(when they request that they be destroyed, of course).
 *
 *	This kernel thread is also used to create other kernel threads
 *	which must be created from an interrupt handler.
 */
void reaper_thread()
{
	for (;;) {
		register thread_t thread;
		register int s;
#if	NeXT
		register struct reaper_struct	*rs;
#endif	NeXT

		s = splsched();
		simple_lock(&reaper_lock);

#if	NeXT
		do {
			rs = (struct reaper_struct *)
				dequeue_head(&reaper_queue);
			if (rs != REAPER_STRUCT_NULL) {
				simple_unlock(&reaper_lock);
				(void) splx(s);
				switch (rs->command) {
				case REAPER_REAP:
					thread = rs->args.reap.thread;
					thread_dowait(thread);	/* may block */
					thread_deallocate(thread);
					break;
				case REAPER_CREATE:
					kernel_thread(rs->args.create.task,
						rs->args.create.start);
					break;
				}
				s = splsched();
				if (rs->reserved) {
					enqueue_tail(&rps_reserved_queue,
						     (queue_entry_t) rs);
				}
				else {
					kfree(rs,
					      sizeof(struct reaper_struct));
				}
				simple_lock(&reaper_lock);
			}
		} while (rs != REAPER_STRUCT_NULL);
		assert_wait((int) &reaper_queue, FALSE);
		simple_unlock(&reaper_lock);
		splx(s);
		thread_block();
#else	NeXT
		while ((thread = (thread_t) dequeue_head(&reaper_queue))
					== THREAD_NULL) {
			assert_wait((int) &reaper_queue, FALSE);
			simple_unlock(&reaper_lock);
			thread_block();
			simple_lock(&reaper_lock);
		}

		simple_unlock(&reaper_lock);
		(void) splx(s);

		(void) thread_dowait(thread, TRUE);	/* may block */
		thread_deallocate(thread);	/* may block */
#endif	NeXT
	}
}

#if	MACH_HOST
/*
 *	thread_assign:
 *
 *	Change processor set assignment.
 *	Caller must hold an extra reference to the thread (if this is
 *	called directly from the ipc interface, this is an operation
 *	in progress reference).  Caller must hold no locks -- this may block.
 */

kern_return_t
thread_assign(thread, new_pset)
thread_t	thread;
processor_set_t	new_pset;
{
	if (thread == THREAD_NULL || new_pset == PROCESSOR_SET_NULL) {
		return(KERN_INVALID_ARGUMENT);
	}

	thread_freeze(thread);
	thread_doassign(thread, new_pset, TRUE);

	return(KERN_SUCCESS);
}

/*
 *	thread_freeze:
 *
 *	Freeze thread's assignment.  Prelude to assigning thread.
 *	Only one freeze may be held per thread.  
 */
void
thread_freeze(thread)
thread_t	thread;
{
	int s;
	/*
	 *	Freeze the assignment, deferring to a prior freeze.
	 */
	s = splsched();
	thread_lock(thread);
	while (thread->may_assign == FALSE) {
		thread->assign_active = TRUE;
		thread_sleep((int) &thread->assign_active,
			simple_lock_addr(thread->lock), FALSE);
		thread_lock(thread);
	}
	thread->may_assign = FALSE;
	thread_unlock(thread);
	(void) splx(s);

}

/*
 *	thread_unfreeze: release freeze on thread's assignment.
 */
void
thread_unfreeze(thread)
thread_t	thread;
{
	int 	s;

	s = splsched();
	thread_lock(thread);
	thread->may_assign = TRUE;
	if (thread->assign_active) {
		thread->assign_active = FALSE;
		thread_wakeup(&thread->assign_active);
	}
	thread_unlock(thread);
	splx(s);
}

/*
 *	thread_doassign:
 *
 *	Actually do thread assignment.  thread_will_assign must have been
 *	called on the thread.  release_freeze argument indicates whether
 *	to release freeze on thread.
 */

void
thread_doassign(thread, new_pset, release_freeze)
register thread_t		thread;
register processor_set_t	new_pset;
boolean_t			release_freeze;
{
	register processor_set_t	pset;
	register boolean_t		old_empty, new_empty;
	boolean_t	recompute_pri = FALSE;
	int	s;
	
	/*
	 *	Check for silly no-op.
	 */
	pset = thread->processor_set;
	if (pset == new_pset) {
		return;
	}
	/*
	 *	Suspend the thread and stop it if it's not the current thread.
	 */
	thread_hold(thread);
	if (thread != current_thread())
		(void) thread_dowait(thread, TRUE);

	/*
	 *	Lock both psets now, use ordering to avoid deadlocks.
	 */
Restart:
	if ((int)pset < (int)new_pset) {
	    pset_lock(pset);
	    pset_lock(new_pset);
	}
	else {
	    pset_lock(new_pset);
	    pset_lock(pset);
	}

	/*
	 *	Check if new_pset is ok to assign to.  If not, reassign
	 *	to default_pset.
	 */
	if (!new_pset->active) {
	    pset_unlock(pset);
	    pset_unlock(new_pset);
	    new_pset = &default_pset;
	    goto Restart;
	}

	/*
	 *	Grab the thread lock and move the thread.
	 *	Then drop the lock on the old pset and the thread's
	 *	reference to it.
	 */
	s = splsched();
	thread_lock(thread);

	thread_change_psets(thread, pset, new_pset);

	old_empty = pset->empty;
	new_empty = new_pset->empty;

	pset_unlock(pset);
	pset_deallocate(pset);

	/*
	 *	Reset policy and priorities if needed.
	 */
#if	MACH_FIXPRI
	if (thread->policy & new_pset->policies == 0) {
	    thread->policy = POLICY_TIMESHARE;
	    recompute_pri = TRUE;
	}
#endif	MACH_FIXPRI

#if	NeXT
	if (thread->max_priority > new_pset->max_priority) {
	    thread->max_priority = new_pset->max_priority;
	    if (thread->priority > thread->max_priority) {
		thread->priority = thread->max_priority;
		recompute_pri = TRUE;
	    }
	    else {
		if ((thread->depress_priority >= 0) &&
		    (thread->depress_priority > thread->max_priority)) {
			thread->depress_priority = thread->max_priority;
		}
	    }
	}
#else	NeXT
	if (thread->max_priority < new_pset->max_priority) {
	    thread->max_priority = new_pset->max_priority;
	    if (thread->priority < thread->max_priority) {
		thread->priority = thread->max_priority;
		recompute_pri = TRUE;
	    }
	    else {
		if ((thread->depress_priority >= 0) &&
		    (thread->depress_priority < thread->max_priority)) {
			thread->depress_priority = thread->max_priority;
		}
	    }
	}
#endif	NeXT

	pset_unlock(new_pset);

	if (recompute_pri)
		compute_priority(thread);

	if (release_freeze) {
		thread->may_assign = TRUE;
		if (thread->assign_active) {
			thread->assign_active = FALSE;
			thread_wakeup(&thread->assign_active);
		}
	}

	thread_unlock(thread);
	splx(s);

	/*
	 *	Figure out hold status of thread.  Threads assigned to empty
	 *	psets must be held.  Therefore:
	 *		If old pset was empty release its hold.
	 *		Release our hold from above unless new pset is empty.
	 */

	if (old_empty)
		thread_release(thread);
	if (!new_empty)
		thread_release(thread);

	/*
	 *	If current_thread is assigned, context switch to force
	 *	assignment to happen.  This also causes hold to take
	 *	effect if the new pset is empty.
	 */
	if (thread == current_thread())
		thread_block();
}
#else	MACH_HOST
kern_return_t
thread_assign(thread, new_pset)
thread_t	thread;
processor_set_t	new_pset;
{
#ifdef	lint
	thread++; new_pset++;
#endif	lint
	return(KERN_FAILURE);
}
#endif	MACH_HOST

/*
 *	thread_assign_default:
 *
 *	Special version of thread_assign for assigning threads to default
 *	processor set.
 */
kern_return_t
thread_assign_default(thread)
thread_t	thread;
{
	return (thread_assign(thread, &default_pset));
}

/*
 *	thread_get_assignment
 *
 *	Return current assignment for this thread.
 */	    
kern_return_t thread_get_assignment(thread, pset)
thread_t	thread;
processor_set_t	*pset;
{
	*pset = thread->processor_set;
	return(KERN_SUCCESS);
}

/*
 *	thread_priority:
 *
 *	Set priority (and possibly max priority) for thread.
 */
kern_return_t
thread_priority(thread, priority, set_max)
thread_t	thread;
int		priority;
boolean_t	set_max;
{
    int			s;
    kern_return_t	ret = KERN_SUCCESS;

    if ((thread == THREAD_NULL) || invalid_pri(priority))
	return(KERN_INVALID_ARGUMENT);

    s = splsched();
    thread_lock(thread);

    /*
     *	Check for violation of max priority
     */
#if	NeXT
    if (priority > thread->max_priority) {
#else	NeXT
    if (priority < thread->max_priority) {
#endif	NeXT
	ret = KERN_FAILURE;
    }
    else {
	/*
	 *	Set priorities.  If a depression is in progress,
	 *	change the priority to restore.
	 */
	if (thread->depress_priority >= 0) {
	    thread->depress_priority = priority;
	}
	else {
	    thread->priority = priority;
	    compute_priority(thread);
	}

	if (set_max)
	    thread->max_priority = priority;
    }
    thread_unlock(thread);
    (void) splx(s);

    return(ret);
}

/*
 *	thread_max_priority:
 *
 *	Reset the max priority for a thread.
 */
kern_return_t
thread_max_priority(thread, pset, max_priority)
thread_t	thread;
processor_set_t	pset;
int		max_priority;
{
    int			s;
    kern_return_t	ret = KERN_SUCCESS;

    if ((thread == THREAD_NULL) || (pset == PROCESSOR_SET_NULL) ||
    	invalid_pri(max_priority))
	    return(KERN_INVALID_ARGUMENT);

    s = splsched();
    thread_lock(thread);

#if	MACH_HOST
    /*
     *	Check for wrong processor set.
     */
    if (pset != thread->processor_set) {
	ret = KERN_FAILURE;
    }
    else {
#endif	MACH_HOST
	thread->max_priority = max_priority;

	/*
	 *	Reset priority if it violates new max priority
	 */
#if	NeXT
	if (thread->priority > max_priority) {
#else	NeXT
	if (max_priority > thread->priority) {
#endif	NeXT
	    thread->priority = max_priority;

	    compute_priority(thread);
	}
	else {
	    if (thread->depress_priority >= 0 &&
#if	NeXT
		thread->depress_priority > max_priority)
#else	NeXT
		max_priority > thread->depress_priority)
#endif	NeXT
		    thread->depress_priority = max_priority;
	    }
#if	MACH_HOST
    }
#endif	MACH_HOST

    thread_unlock(thread);
    (void) splx(s);

    return(ret);
}

/*
 *	thread_policy:
 *
 *	Set scheduling policy for thread.
 */
kern_return_t
thread_policy(thread, policy, data)
thread_t	thread;
int		policy;
int		data;
{
#if	MACH_FIXPRI
	register kern_return_t	ret = KERN_SUCCESS;
	register int s, temp;
#endif	MACH_FIXPRI

	if ((thread == THREAD_NULL) || invalid_policy(policy))
		return(KERN_INVALID_ARGUMENT);

#if	MACH_FIXPRI
	s = splsched();
	thread_lock(thread);

	/*
	 *	Check if changing policy.
	 */
	if (policy == thread->policy) {
	    /*
	     *	Just changing data.  This is meaningless for
	     *	timeshareing, quantum for fixed priority (but
	     *	has no effect until current quantum runs out).
	     */
	    if (policy == POLICY_FIXEDPRI)
		temp = data * 1000;
		if (temp % tick)
			temp += tick;
		thread->sched_data = temp/tick;
	}
	else {
	    /*
	     *	Changing policy.  Check if new policy is allowed.
	     */
	    if ((thread->processor_set->policies & policy) == 0) {
		    ret = KERN_FAILURE;
	    }
	    else {
		/*
		 *	Changing policy.  Save data and calculate new
		 *	priority.
		 */
		thread->policy = policy;
		if (policy == POLICY_FIXEDPRI) {
			temp = data * 1000;
			if (temp % tick)
				temp += tick;
			thread->sched_data = temp/tick;
		}
#if	NeXT
		/*
		 * Why set this for non-FIXEDPRI policies?
		 */
#else	NeXT
		thread->sched_data = data/tick;
#endif	NeXT
		compute_priority(thread);
	    }
	}
	thread_unlock(thread);
	(void) splx(s);

	return(ret);
#else	MACH_FIXPRI
	if (policy == POLICY_TIMESHARE)
		return(KERN_SUCCESS);
	else
		return(KERN_FAILURE);
#endif	MACH_FIXPRI
}
