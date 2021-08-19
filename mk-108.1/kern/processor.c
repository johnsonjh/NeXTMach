/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	processor.c,v $
 * 30-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Fixed processor_set_info to return size of
 *	PROCESSOR_SET_SCHED_INFO_COUNT for sched_info request.
 *
 * 24-May-90  Gregg Kellogg (gk) at NeXT
 *	Added MAXPRI_USER priority for setting the default pset max_priority.
 *
 * 18-May-90  Avadis Tevanian (avie) at NeXT
 *	Changed to use sensible priorities (higher numbers -> higher pri).
 *
 *  2-Mar-90  Gregg Kellogg (gk) at NeXT
 *	MACH_OLD_VM_COPY: use old vm_map_pageable semantics.
 *
 * Revision 2.5  89/12/22  15:52:54  rpd
 * 	Changes to implement pset garbage collection:
 * 	1.  Add pset_is_garbage to detect abandoned processor sets.
 * 		Add do_pset_scan to look for them.
 * 	2.  Pass back the actual ports from processor_set_create, so they
 * 	        will always have extra references; this way a newly created
 * 		processor set never looks like garbage.
 * 
 * 	Also optimized processor_set_destroy.
 * 	[89/12/15            dlb]
 * 	Put all fixed priority support code under MACH_FIXPRI switch.
 * 	Add thread_change_psets for use by thread_assign.
 * 	[89/11/10            dlb]
 * 	Check for null processor set in pset_deallocate.
 * 	[89/11/06            dlb]
 * 
 * Revision 2.4  89/11/20  11:23:45  mja
 * 	Put all fixed priority support code under MACH_FIXPRI switch.
 * 	Add thread_change_psets for use by thread_assign.
 * 	[89/11/10            dlb]
 * 	Check for null processor set in pset_deallocate.
 * 	[89/11/06            dlb]
 * 
 * Revision 2.3  89/10/15  02:05:04  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:20:11  dlb
 * 	Add processor_set_{tasks,threads}.  Use common internals.
 * 	Maintain all_psets_count for host_processor_sets.
 * 	Add processor_set_policy, sched flavor of processor_set_info.
 * 	Add processor_set_max_priority.
 * 	Remove reference count decrements from pset_remove_{task,thread}.
 * 	Callers must explicitly call pset_deallocate().
 * 	Add load factor/average inits.  Make info available to users.
 * 
 * Revision 2.1.1.7  89/08/02  23:00:49  dlb
 * 	Move includes
 * 	[89/08/02            dlb]
 * 
 * Revision 2.1.1.6  89/07/25  16:10:53  dlb
 * 	Eliminate interrupt protection for pset locks.
 * 	Add init for quantum_adj_lock.
 * 	[89/06/14            dlb]
 * 
 * 	Add processor_set_{tasks,threads}.  Use common internals.
 * 	Maintain all_psets_count for host_processor_sets.
 * 	[89/06/09            dlb]
 * 
 * 	Add processor_set_policy, sched flavor of processor_set_info.
 * 	[89/05/18            dlb]
 * 
 * 	Add processor_set_max_priority.
 * 	[89/05/12            dlb]
 * 
 * 	Add wait argument to processor_assign call.
 * 	[89/05/10            dlb]
 * 
 * Revision 2.1.1.5  89/03/20  14:21:27  dlb
 * 	Move processor reassignment to processor_set_destroy from
 * 	pset_deallocate.
 * 	[89/03/13            dlb]
 * 
 * 	Fix interrupt protection in processor_set_{create,destroy}.
 * 	[89/03/09            dlb]
 * 
 * Revision 2.1.1.4  89/02/24  21:32:03  dlb
 * 	Remove reference count decrements from pset_remove_{task,thread}.
 * 	Callers must explicitly call pset_deallocate().
 * 	[89/02/17            dlb]
 * 
 * Revision 2.1.1.3  89/02/13  22:43:59  dlb
 * 	Add load factor/average inits.  Make info available to users.
 * 	[89/02/09            dlb]
 * 
 * 24-Sep-1988  David Black (dlb) at Carnegie-Mellon University
 *	Created
 */

/*
 *	processor.c: processor and processor_set manipulation routines.
 */

#import <cpus.h>
#import <mach_fixpri.h>
#import <mach_host.h>
#import <mach_old_vm_copy.h>

#import <kern/ipc_globals.h>	/* for ipc_kernel_map */
#import <kern/kern_port.h>	/* for use by pset_is_garbage */

#import <sys/boolean.h>
#import <kern/lock.h>
#import <kern/host.h>
#import <sys/policy.h>
#import <kern/processor.h>
#import <sys/processor_info.h>
#import <kern/sched.h>
#import <kern/thread.h>
#import <vm/vm_param.h>
#import <kern/ipc_host.h>

#import <machine/cpu.h>

#if	MACH_HOST
#import <kern/zalloc.h>
zone_t	pset_zone;
#endif	MACH_HOST

/*
 *	Exported variables.
 */
struct processor_set default_pset;
struct processor processor_array[NCPUS];

queue_head_t		all_psets;
int			all_psets_count;
decl_simple_lock_data(, all_psets_lock);

processor_t	master_processor;
processor_t	processor_ptr[NCPUS];

/*
 *	Bootstrap the processor/pset system so the scheduler can run.
 */
void pset_sys_bootstrap()
{
	register int	i;
	void pset_init(), processor_init();

	pset_init(&default_pset);
	default_pset.empty = FALSE;
	for (i = 0; i < NCPUS; i++) {
		/*
		 *	Initialize processor data structures.
		 *	Note that cpu_to_processor(i) is processor_ptr[i].
		 */
		processor_ptr[i] = &processor_array[i];
		processor_init(processor_ptr[i], i);
	}
	master_processor = cpu_to_processor(master_cpu);
	queue_init(&all_psets);
	simple_lock_init(&all_psets_lock);
	queue_enter(&all_psets, &default_pset, processor_set_t, all_psets);
	all_psets_count = 1;
	default_pset.active = TRUE;
	default_pset.empty = FALSE;

	/*
#if	NeXT
	 *	Note: the default_pset has a max_priority of MAXPRI_USER.
#else	NeXT
	 *	Note: the default_pset has a max_priority of BASEPRI_USER.
#endif	NeXT
	 *	Internal kernel threads override this in kernel_thread.
	 */
}

#if	MACH_HOST
/*
 *	Rest of pset system initializations.
 */
void pset_sys_init()
{
	pset_zone = zinit(sizeof(struct processor_set), 128*PAGE_SIZE,
		PAGE_SIZE, FALSE, "processor sets");
}
#endif	MACH_HOST

/*
 *	Initialize the given processor_set structure.
 */

void pset_init(pset)
register processor_set_t	pset;
{
	int	i;

	simple_lock_init(&pset->runq.lock);
#if	NeXT
	pset->runq.high = NRQS-1;
#else	NeXT
	pset->runq.low = 0;
#endif	NeXT
	pset->runq.count = 0;
	for (i = 0; i < NRQS; i++) {
	    queue_init(&(pset->runq.runq[i]));
	}
	queue_init(&pset->idle_queue);
	pset->idle_count = 0;
	simple_lock_init(&pset->idle_lock);
	queue_init(&pset->processors);
	pset->processor_count = 0;
	pset->empty = TRUE;
	queue_init(&pset->tasks);
	pset->task_count = 0;
	queue_init(&pset->threads);
	pset->thread_count = 0;
	pset->ref_count = 1;
	queue_init(&pset->all_psets);
	pset->active = FALSE;
	simple_lock_init(&pset->lock);
	pset->pset_self = PORT_NULL;
	pset->pset_name_self = PORT_NULL;
#if	NeXT
	pset->max_priority = MAXPRI_USER;
#else	NeXT
	pset->max_priority = BASEPRI_USER;
#endif	NeXT
#if	MACH_FIXPRI
	pset->policies = POLICY_TIMESHARE;
#endif	MACH_FIXPRI
	pset->set_quantum = min_quantum;
#if	NCPUS > 1
	pset->quantum_adj_index = 0;
	simple_lock_init(&pset->quantum_adj_lock);

	for (i = 0; i <= NCPUS; i++) {
	    pset->machine_quantum[i] = min_quantum;
	}
#endif	NCPUS > 1
	pset->mach_factor = 0;
	pset->load_average = 0;
	pset->sched_load = SCHED_SCALE;		/* i.e. 1 */
}

/*
 *	Initialize the given processor structure for the processor in
 *	the slot specified by slot_num.
 */

void processor_init(pr, slot_num)
register processor_t	pr;
int	slot_num;
{
	int	i;

	simple_lock_init(&pr->runq.lock);
#if	NeXT
	pr->runq.high = NRQS-1;
#else	NeXT
	pr->runq.low = 0;
#endif	NeXT
	pr->runq.count = 0;
	for (i = 0; i < NRQS; i++) {
	    queue_init(&(pr->runq.runq[i]));
	}
	queue_init(&pr->processor_queue);
	pr->state = PROCESSOR_OFF_LINE;
	pr->next_thread = THREAD_NULL;
	pr->idle_thread = THREAD_NULL;
	pr->quantum = 0;
	pr->first_quantum = FALSE;
	pr->last_quantum = 0;
	pr->processor_set = PROCESSOR_SET_NULL;
	pr->processor_set_next = PROCESSOR_SET_NULL;
	queue_init(&pr->processors);
	simple_lock_init(&pr->lock);
	pr->processor_self = PORT_NULL;
	pr->slot_num = slot_num;
}

/*
 *	pset_remove_processor() removes a processor from a processor_set.
 *	It can only be called on the current processor.  Caller must
 *	hold lock on current processor and processor set.
 */

pset_remove_processor(pset, processor)
processor_set_t	pset;
processor_t	processor;
{
	if (pset != processor->processor_set)
		panic("pset_remove_processor: wrong pset");

	queue_remove(&pset->processors, processor, processor_t, processors);
	processor->processor_set = PROCESSOR_SET_NULL;
	pset->processor_count--;
	quantum_set(pset);
}

/*
 *	pset_add_processor() adds a  processor to a processor_set.
 *	It can only be called on the current processor.  Caller must
 *	hold lock on curent processor and on pset.  No reference counting on
 *	processors.  Processor reference to pset is implicit.
 */

pset_add_processor(pset, processor)
processor_set_t	pset;
processor_t	processor;
{
	queue_enter(&pset->processors, processor, processor_t, processors);
	processor->processor_set = pset;
	pset->processor_count++;
	quantum_set(pset);
}

/*
 *	pset_remove_task() removes a task from a processor_set.
 *	Caller must hold locks on pset and task.  Pset reference count
 *	is not decremented; caller must explicitly pset_deallocate.
 */

pset_remove_task(pset, task)
processor_set_t	pset;
task_t	task;
{
	if (pset != task->processor_set)
		return;

	queue_remove(&pset->tasks, task, task_t, pset_tasks);
	task->processor_set = PROCESSOR_SET_NULL;
	pset->task_count--;
}

/*
 *	pset_add_task() adds a  task to a processor_set.
 *	Caller must hold locks on pset and task.  Pset references to
 *	tasks are implicit.
 */

pset_add_task(pset, task)
processor_set_t	pset;
task_t	task;
{
	queue_enter(&pset->tasks, task, task_t, pset_tasks);
	task->processor_set = pset;
	pset->task_count++;
	pset->ref_count++;
}

/*
 *	pset_remove_thread() removes a thread from a processor_set.
 *	Caller must hold locks on pset and thread.  Pset reference count
 *	is not decremented; caller must explicitly pset_deallocate.
 */

pset_remove_thread(pset, thread)
processor_set_t	pset;
thread_t	thread;
{
	queue_remove(&pset->threads, thread, thread_t, pset_threads);
	thread->processor_set = PROCESSOR_SET_NULL;
	pset->thread_count--;
}

/*
 *	pset_add_thread() adds a  thread to a processor_set.
 *	Caller must hold locks on pset and thread.  Pset references to
 *	threads are implicit.
 */

pset_add_thread(pset, thread)
processor_set_t	pset;
thread_t	thread;
{
	queue_enter(&pset->threads, thread, thread_t, pset_threads);
	thread->processor_set = pset;
	pset->thread_count++;
	pset->ref_count++;
}

/*
 *	thread_change_psets() changes the pset of a thread.  Caller must
 *	hold locks on both psets and thread.  The old pset must be
 *	explicitly pset_deallocat()'ed by caller.
 */

thread_change_psets(thread, old_pset, new_pset)
thread_t	thread;
processor_set_t old_pset, new_pset;
{
	queue_remove(&old_pset->threads, thread, thread_t, pset_threads);
	old_pset->thread_count--;
	queue_enter(&new_pset->threads, thread, thread_t, pset_threads);
	thread->processor_set = new_pset;
	new_pset->thread_count++;
	new_pset->ref_count++;
}	

/*
 *	pset_deallocate:
 *
 *	Remove one reference to the processor set.  Destroy processor_set
 *	if this was the last reference.
 */
pset_deallocate(pset)
processor_set_t	pset;
{
	if (pset == PROCESSOR_SET_NULL)
		return;

	pset_lock(pset);
	if (--pset->ref_count > 0) {
		pset_unlock(pset);
		return;
	}
#if	!MACH_HOST
	panic("pset_deallocate: default_pset destroyed");
#endif	!MACH_HOST
#if	MACH_HOST
	/*
	 *	Reference count is zero, however the all_psets list
	 *	holds an implicit reference and may make new ones.
	 *	Its lock also dominates the pset lock.  To check for this,
	 *	temporarily restore one reference, and then lock the
	 *	other structures in the right order.
	 */
	pset->ref_count = 1;
	pset_unlock(pset);
	
	simple_lock(&all_psets_lock);
	pset_lock(pset);
	if (--pset->ref_count > 0) {
		/*
		 *	Made an extra reference.
		 */
		pset_unlock(pset);
		simple_unlock(&all_psets_lock);
		return;
	}

	/*
	 *	Ok to destroy pset.  Make a few paranoia checks.
	 */

	if ((pset == &default_pset) || (pset->thread_count > 0) ||
	    (pset->task_count > 0) || pset->processor_count > 0) {
		panic("pset_deallocate: destroy default or active pset");
	}
	/*
	 *	Remove from all_psets queue.
	 */
	queue_remove(&all_psets, pset, processor_set_t, all_psets);
	all_psets_count--;

	pset_unlock(pset);
	simple_unlock(&all_psets_lock);

	/*
	 *	That's it, free data structure.
	 */
	zfree(pset_zone, pset);
#endif	MACH_HOST
}

/*
 *	pset_reference:
 *
 *	Add one reference to the processor set.
 */
pset_reference(pset)
processor_set_t	pset;
{
	pset_lock(pset);
	pset->ref_count++;
	pset_unlock(pset);
}

kern_return_t processor_info(processor, flavor, host, info, count)
register processor_t	processor;
int			flavor;
host_t			*host;
processor_info_t	info;
unsigned int		*count;
{
	register int	slot_num, state;
	register processor_basic_info_t		basic_info;

	if (processor == PROCESSOR_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (flavor != PROCESSOR_BASIC_INFO ||
		*count < PROCESSOR_BASIC_INFO_COUNT)
			return(KERN_FAILURE);

	basic_info = (processor_basic_info_t) info;

	slot_num = processor->slot_num;
	basic_info->cpu_type = machine_slot[slot_num].cpu_type;
	basic_info->cpu_subtype = machine_slot[slot_num].cpu_subtype;
	state = processor->state;
	if (state == PROCESSOR_SHUTDOWN || state == PROCESSOR_OFF_LINE)
		basic_info->running = FALSE;
	else
		basic_info->running = TRUE;
	basic_info->slot_num = slot_num;
	if (processor == master_processor) 
		basic_info->is_master = TRUE;
	else
		basic_info->is_master = FALSE;

	*count = PROCESSOR_BASIC_INFO_COUNT;
	*host = &realhost;
	return(KERN_SUCCESS);
}

kern_return_t processor_start(processor)
processor_t	processor;
{
    	if (processor == PROCESSOR_NULL)
		return(KERN_INVALID_ARGUMENT);
#if	NCPUS > 1
	return(cpu_start(processor->slot_num));
#else	NCPUS > 1
	return(KERN_FAILURE);
#endif	NCPUS > 1
}

kern_return_t processor_exit(processor)
processor_t	processor;
{
	if (processor == PROCESSOR_NULL)
		return(KERN_INVALID_ARGUMENT);

#if	NCPUS > 1
	return(processor_shutdown(processor));
#else	NCPUS > 1
	return(KERN_FAILURE);
#endif	NCPUS > 1
}

kern_return_t processor_control(processor, info, count)
processor_t		processor;
processor_info_t	info;
long			*count;
{
	if (processor == PROCESSOR_NULL)
		return(KERN_INVALID_ARGUMENT);

#if	NCPUS > 1
	return(cpu_control(processor->slot_num, (int *)info, *count));
#else	NCPUS > 1
	return(KERN_FAILURE);
#endif	NCPUS > 1
}

/*
 *	Precalculate the appropriate system quanta based on load.  The
 *	index into machine_quantum is the number of threads on the
 *	processor set queue.  It is limited to the number of processors in
 *	the set.
 */

quantum_set(pset)
processor_set_t	pset;
{
#if	NCPUS > 1
	register int	i,ncpus;

	ncpus = pset->processor_count;

	for ( i=1 ; i <= ncpus ; i++) {
		pset->machine_quantum[i] =
			((min_quantum * ncpus) + (i/2)) / i ;
	}
	pset->machine_quantum[0] = 2 * pset->machine_quantum[1];

	i = ((pset->runq.count > pset->processor_count) ?
		pset->processor_count : pset->runq.count);
	pset->set_quantum = pset->machine_quantum[i];
#else	NCPUS > 1
	default_pset.set_quantum = min_quantum;
#endif	NCPUS > 1
}

#if	MACH_HOST
/*
 *	processor_set_create:
 *
 *	Create and return a new processor set.
 */

kern_return_t	processor_set_create(host, new_set, new_name)
host_t	host;
port_t	*new_set, *new_name;
{
	processor_set_t	pset;

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);
	pset = (processor_set_t) zalloc(pset_zone);
	pset_init(pset);
	ipc_pset_init(pset);
	pset->active = TRUE;

	simple_lock(&all_psets_lock);
	queue_enter(&all_psets, pset, processor_set_t, all_psets);
	all_psets_count++;
	simple_unlock(&all_psets_lock);

	ipc_pset_enable(pset);

	/*
	 *	return ports as new_set and new_name.  ipc_pset_init
	 *	took out the required extra references.
	 */
	*new_set = pset->pset_self;
	*new_name = pset->pset_name_self;
	return(KERN_SUCCESS);
}

/*
 *	processor_set_destroy:
 *
 *	destroy a processor set.  Any tasks, threads or processors
 *	currently assigned to it are reassigned to the default pset.
 */
kern_return_t processor_set_destroy(pset)
processor_set_t	pset;
{
	register queue_entry_t	elem;
	register queue_head_t	*list;

	if (pset == PROCESSOR_SET_NULL || pset == &default_pset)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Handle multiple termination race.  First one through sets
	 *	active to FALSE and disables ipc access.
	 */
	pset_lock(pset);
	if (!(pset->active)) {
		pset_unlock(pset);
		return(KERN_FAILURE);
	}

	pset->active = FALSE;
	ipc_pset_disable(pset);


	/*
	 *	Now reassign everything in this set to the default set.
	 */

	if (pset->task_count > 0) {
	    list = &pset->tasks;
	    while (!queue_empty(list)) {
		elem = queue_first(list);
		task_reference((task_t) elem);
		pset_unlock(pset);
		task_assign((task_t) elem, &default_pset, FALSE);
		task_deallocate((task_t) elem);
		pset_lock(pset);
	    }
	}

	if (pset->thread_count > 0) {
	    list = &pset->threads;
	    while (!queue_empty(list)) {
		elem = queue_first(list);
		thread_reference((thread_t) elem);
		pset_unlock(pset);
		thread_assign((thread_t) elem, &default_pset);
		thread_deallocate((thread_t) elem);
		pset_lock(pset);
	    }
	}
	
	if (pset->processor_count > 0) {
	    list = &pset->processors;
	    while(!queue_empty(list)) {
		elem = queue_first(list);
		pset_unlock(pset);
		processor_assign((processor_t) elem, &default_pset, TRUE);
		pset_lock(pset);
	    }
	}

	pset_unlock(pset);

	/*
	 *	Destroy ipc state.
	 */
	ipc_pset_terminate(pset);

	/*
	 *	Deallocate pset's reference to itself.
	 */
	pset_deallocate(pset);
	return(KERN_SUCCESS);
}

/*
 *	pset_is_garbage:
 *
 *	Determine if a processor set is garbage and should be destroyed.
 *	A processor set is garbage if it has no tasks, threads, or
 *	processors assigned  AND there are no send rights on either
 *	of its ports AND it hasn't already been destroyed.
 *	Caller must hold lock on processor set.
 */
boolean_t pset_is_garbage(pset)
processor_set_t	pset;
{
	register kern_port_t	port, name_port;

	/*
	 *	Is anything assigned ?  Is it still active?
	 */
	if ((pset->task_count != 0) || (pset->thread_count != 0) ||
	    (pset->processor_count != 0) || !(pset->active))
		return(FALSE);

	/*
	 *	Check reference counts on the ports.  There must be
	 *	at least 2 references: 1 for the pointer in the
	 *	data structure, and one for rights held by the kernel.
	 *	If only those two references exist, then nobody else
	 *	knows about the port.  Grab lock on first port to
	 *	avoid possibility of someone exchanging one port
	 *	right for another.
	 */
	port = (kern_port_t) pset->pset_self;
	name_port = (kern_port_t) pset->pset_name_self;

	port_lock(port);
	if ((port->port_references > 2) || (name_port->port_references > 2)) {
	    port_unlock(port);
	    return(FALSE);
	}
	port_unlock(port);

	/*
	 *	Ok, it is garbage !!
	 */
	return(TRUE);
}

/*
 *	do_pset_scan:
 *
 *	Garbage collect the all_psets queue.  A pset is garbage precisely
 *	when pset_is_garbage() says it is.  This should be replaced
 *	by checks when tasks, threads, and processes are removed, and
 *	the use of no more senders notifications when they get implemented.
 */
void
do_pset_scan()
{
	register processor_set_t	pset, next_pset, temp_pset;

	next_pset = PROCESSOR_SET_NULL;

	/*
	 *	Scan list of all processor sets; it cannot be empty.
	 */
	simple_lock(&all_psets_lock);
	pset = (processor_set_t) queue_first(&all_psets);
	do {

	    /*
	     *	Is this pset garbage??
	     */
	    pset_lock(pset);
	    if (pset_is_garbage(pset)) {

		/*
		 *	Start the destruction by inactivating it and
		 *	disabling ipc access.
		 */
		pset->active = FALSE;
		ipc_pset_disable(pset);
		pset_unlock(pset);

		/*
		 *	Grab a reference to the next pset to hold our
		 *	place in the all_psets queue.  If we have a reference
		 *	from doing this before, put it in temp_pset so we can
		 *	dispose of it.
		 */
		temp_pset = next_pset;
		next_pset = (processor_set_t) queue_next(&pset->all_psets);
		pset_reference(next_pset);

		/*
		 *	Now drop the all_psets lock and finish destroying
		 *	this processor set.  Nothing to reassign because
		 *	it wouldn't be garbage if there was.
		 */
		simple_unlock(&all_psets_lock);

		ipc_pset_terminate(pset);
		pset_deallocate(pset);

		/*
		 *	Deallocate reference to previous next pset if needed.
		 *	Then pick up lock and advance to new next pset.
		 */
		if (temp_pset != PROCESSOR_SET_NULL)
		    pset_deallocate(temp_pset);

		simple_lock(&all_psets_lock);
		pset = next_pset;
	    }
	    else {
		pset_unlock(pset);
		pset = (processor_set_t) queue_next(&pset->all_psets);
	    }
	} while (!queue_end(&all_psets, (queue_entry_t) pset));

	/*
	 *	Unlock and clean up leftover reference if there is one.
	 */
	simple_unlock(&all_psets_lock);

	if (next_pset != PROCESSOR_SET_NULL)
	    pset_deallocate(next_pset);
}

#else	MACH_HOST
	    
kern_return_t	processor_set_create(host, new_set, new_name)
host_t	host;
processor_set_t	*new_set, *new_name;
{
#ifdef	lint
	host++; new_set++; new_name++;
#endif	lint
	return(KERN_FAILURE);
}

kern_return_t processor_set_destroy(pset)
processor_set_t	pset;
{
#ifdef	lint
	pset++;
#endif	lint
	return(KERN_FAILURE);
}

#endif	MACH_HOST

kern_return_t processor_get_assignment(processor, pset)
processor_t	processor;
processor_set_t	*pset;
{
    	int state;

	state = processor->state;
	if (state == PROCESSOR_SHUTDOWN || state == PROCESSOR_OFF_LINE)
		return(KERN_FAILURE);

	*pset = processor->processor_set;
	return(KERN_SUCCESS);
}

kern_return_t processor_set_info(pset, flavor, host, info, count)
processor_set_t		pset;
int			flavor;
host_t			*host;
processor_set_info_t	info;
unsigned int		*count;
{
	if (pset == PROCESSOR_SET_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (flavor == PROCESSOR_SET_BASIC_INFO) {
		register processor_set_basic_info_t	basic_info;

		if (*count < PROCESSOR_SET_BASIC_INFO_COUNT)
			return(KERN_FAILURE);

		basic_info = (processor_set_basic_info_t) info;

		pset_lock(pset);
		basic_info->processor_count = pset->processor_count;
		basic_info->task_count = pset->task_count;
		basic_info->thread_count = pset->thread_count;
		basic_info->mach_factor = pset->mach_factor;
		basic_info->load_average = pset->load_average;
		pset_unlock(pset);

		*count = PROCESSOR_SET_BASIC_INFO_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}
	else if (flavor == PROCESSOR_SET_SCHED_INFO) {
		register processor_set_sched_info_t	sched_info;

		if (*count < PROCESSOR_SET_SCHED_INFO_COUNT)
			return(KERN_FAILURE);

		sched_info = (processor_set_sched_info_t) info;

		pset_lock(pset);
#if	MACH_FIXPRI
		sched_info->policies = pset->policies;
#else	MACH_FIXPRI
		sched_info->policies = POLICY_TIMESHARE;
#endif	MACH_FIXPRI
		sched_info->max_priority = pset->max_priority;
		pset_unlock(pset);

		*count = PROCESSOR_SET_SCHED_INFO_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}

	*host = HOST_NULL;
	return(KERN_INVALID_ARGUMENT);
}

/*
 *	processor_set_max_priority:
 *
 *	Specify max priority permitted on processor set.  This affects
 *	newly created and assigned threads.  Optionally change existing
 * 	ones.
 */
kern_return_t
processor_set_max_priority(pset, max_priority, change_threads)
processor_set_t	pset;
int		max_priority;
boolean_t	change_threads;
{
	if (pset == PROCESSOR_SET_NULL || invalid_pri(max_priority))
		return(KERN_INVALID_ARGUMENT);

	pset_lock(pset);
	pset->max_priority = max_priority;

	if (change_threads) {
	    register queue_head_t *list;
	    register thread_t	thread;

	    list = &pset->threads;
	    thread = (thread_t) queue_first(list);
	    while (!queue_end(list, (queue_entry_t) thread)) {
		if (thread->max_priority < max_priority)
			thread_max_priority(thread, pset, max_priority);

		thread = (thread_t) queue_next(&thread->pset_threads);
	    }
	}

	pset_unlock(pset);

	return(KERN_SUCCESS);
}

/*
 *	processor_set_policy_enable:
 *
 *	Allow indicated policy on processor set.
 */

kern_return_t
processor_set_policy_enable(pset, policy)
processor_set_t	pset;
int	policy;
{
	if ((pset == PROCESSOR_SET_NULL) || invalid_policy(policy))
		return(KERN_INVALID_ARGUMENT);

#if	MACH_FIXPRI
	pset_lock(pset);
	pset->policies |= policy;
	pset_unlock(pset);
	
    	return(KERN_SUCCESS);
#else	MACH_FIXPRI
	if (policy == POLICY_TIMESHARE)
		return(KERN_SUCCESS);
	else
		return(KERN_FAILURE);
#endif	MACH_FIXPRI
}

/*
 *	processor_set_policy_disable:
 *
 *	Forbid indicated policy on processor set.  Time sharing cannot
 *	be forbidden.
 */

kern_return_t
processor_set_policy_disable(pset, policy, change_threads)
processor_set_t	pset;
int	policy;
boolean_t	change_threads;
{
	if ((pset == PROCESSOR_SET_NULL) || policy == POLICY_TIMESHARE ||
	    invalid_policy(policy))
		return(KERN_INVALID_ARGUMENT);

#if	MACH_FIXPRI
	pset_lock(pset);

	/*
	 *	Check if policy enabled.  Disable if so, then handle
	 *	change_threads.
	 */
	if (pset->policies & policy) {
	    pset->policies &= ~policy;

	    if (change_threads) {
		register queue_head_t	*list;
		register thread_t	thread;

		list = &pset->threads;
		thread = (thread_t) queue_first(list);
		while (!queue_end(list, (queue_entry_t) thread)) {
		    if (thread->policy == policy)
			thread_policy(thread, POLICY_TIMESHARE, 0);

		    thread = (thread_t) queue_next(&thread->pset_threads);
		}
	    }
	}
	pset_unlock(pset);
#endif	MACH_FIXPRI

	return(KERN_SUCCESS);
}

#define THING_TASK	0
#define THING_THREAD	1

/*
 *	processor_set_things:
 *
 *	Common internals for processor_set_{threads,tasks}
 */
kern_return_t
processor_set_things(pset, thing_list, count, type)
processor_set_t	pset;
port_t		**thing_list;
unsigned int	*count;
int		type;
{
	unsigned int actual;	/* this many things */
	port_t *things;
	int i;

	vm_size_t size;
	vm_offset_t addr;

	/*
	 *	Need a type-breaking union and some cpp magic.
	 */
	union {
	    thread_t	foo_thread;
	    task_t	foo_task;
	} foo;
#define thread	(foo.foo_thread)
#define task	(foo.foo_task)

	if (pset == PROCESSOR_SET_NULL)
		return KERN_INVALID_ARGUMENT;

	size = 0; addr = 0;

	for (;;) {
		vm_size_t size_needed;

		pset_lock(pset);
		if (!pset->active) {
			pset_unlock(pset);
			return KERN_FAILURE;
		}

		if (type == THING_TASK)
			actual = pset->task_count;
		else
			actual = pset->thread_count;

		/* do we have the memory we need? */

		size_needed = actual * sizeof(port_t);
		if (size_needed <= size)
			break;

		/* unlock the task and allocate more memory */
		pset_unlock(pset);

		if (size != 0)
			(void) kmem_free(ipc_kernel_map, addr, size);

		size = round_page(2 * size_needed);

		/* allocate memory non-pageable, so no faults
		   while holding locks */

		(void) vm_allocate(ipc_kernel_map, &addr, size, TRUE);
#if	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map, addr, addr + size,
			FALSE);
#else	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map, addr, addr + size,
			VM_PROT_READ|VM_PROT_WRITE);
#endif	MACH_OLD_VM_COPY
	}

	/* OK, have memory and the processor_set is locked & active */

	things = (port_t *) addr;

	switch (type) {
	    case THING_TASK:
		for (i = 0, task = (task_t) queue_first(&pset->tasks);
		     i < actual;
		     i++, task = (task_t) queue_next(&task->pset_tasks))
			things[i] = convert_task_to_port(task);
		assert(queue_end(&pset->tasks, (queue_entry_t) task));
		break;

	    case THING_THREAD:
		for (i = 0, thread = (thread_t) queue_first(&pset->threads);
		     i < actual;
		     i++,
		     thread = (thread_t) queue_next(&thread->pset_threads))
			things[i] = convert_thread_to_port(thread);
		assert(queue_end(&pset->threads, (queue_entry_t) thread));
		break;
	}

	/* can unlock processor set now that we have the ports */
	pset_unlock(pset);

	if (actual == 0) {
		/* no things, so return null pointer and deallocate memory */
		*thing_list = 0;
		*count = 0;

		if (size != 0)
			(void) kmem_free(ipc_kernel_map, addr, size);
	} else {
		vm_size_t size_used;

		*thing_list = things;
		*count = actual;

		size_used = round_page(actual * sizeof(port_t));

		/* finished touching it, so make the memory pageable */
#if	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map,
				       addr, addr + size_used, TRUE);
#else	MACH_OLD_VM_COPY
		(void) vm_map_pageable(ipc_kernel_map,
				       addr, addr + size_used, VM_PROT_NONE);
#endif	MACH_OLD_VM_COPY

		/* free any unused memory */
		if (size_used != size)
			(void) kmem_free(ipc_kernel_map,
					 addr + size_used, size - size_used);
	}

	return(KERN_SUCCESS);
}


/*
 *	processor_set_tasks:
 *
 *	List all tasks in the processor set.
 */
kern_return_t
processor_set_tasks(pset, task_list, count)
processor_set_t	pset;
task_array_t	*task_list;
unsigned int	*count;
{
    return(processor_set_things(pset, task_list, count, THING_TASK));
}

/*
 *	processor_set_threads:
 *
 *	List all threads in the processor set.
 */
kern_return_t
processor_set_threads(pset, thread_list, count)
processor_set_t	pset;
thread_array_t	*thread_list;
unsigned int	*count;
{
    return(processor_set_things(pset, thread_list, count, THING_THREAD));
}