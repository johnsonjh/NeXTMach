/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	host.c,v $
 * 14-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Changed host_info to return min_timeout and min_quantum in milliseconds.
 *
 * Revision 2.5  90/07/20  17:00:51  mrt
 * 	Fixed one more processor_set_array_t ->
 * 	processor_set_name_array_t
 * 	[90/07/20            mrt]
 * 
 * Revision 2.4  90/07/20  08:49:34  mrt
 * 	Convert host_processor_sets to unprivileged version -
 * 	return name ports given host port instead of returning
 * 	control port given privileged host port.
 * 	[90/07/13            dlb]
 * 
 *  2-Mar-90  Gregg Kellogg (gk) at NeXT
 *	MACH_OLD_VM_COPY: use old vm_map_pageable semantics.
 *
 * Revision 2.3  89/10/15  02:03:53  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:04:44  dlb
 * 	Optimize host_processor_sets and add simplified !MACH_HOST case.
 * 	Remove interrupt protection from all_psets lock.
 * 	Add host_processor_sets and host_processor_set_priv.
 * 	Add scheduler information flavor to host_info.
 * 
 * Revision 2.1.1.4  89/08/02  22:52:55  dlb
 * 	Move includes.
 * 	[89/08/02            dlb]
 * 
 * Revision 2.1.1.3  89/07/25  15:38:33  dlb
 * 	Remove interrupt protection from all_psets lock.
 * 	[89/06/14            dlb]
 * 
 * 	Add host_processor_sets and host_processor_set_priv.
 * 	[89/06/09            dlb]
 * 
 * 	Add scheduler information flavor to host_info.
 * 	[89/06/08            dlb]
 * 
 * Revision 2.1.1.2  89/02/07  00:31:50  dlb
 * 	Declare realhost here.
 * 	[89/02/03            dlb]
 * 
 * Revision 2.1.1.1  89/01/30  16:54:25  dlb
 *	Reformat includes.  mach_host.h moved to mach/ directory.
 *	[89/01/26            dlb]
 *	
 *	Move kernel version to a separate routine so it can be returned
 *	as a string of characters.
 *	[88/12/02            dlb]
 *	
 *	Bug fixes, add new flavors of host information.
 *	[88/12/01            dlb]
 *	
 *	Created.
 *	[88/10/31            dlb]
 */

/*
 *	host.c
 *
 *	Non-ipc host functions.
 */

#import <cpus.h>
#import <mach_host.h>
#import <mach_old_vm_copy.h>

#import <kern/host.h>
#import <sys/host_info.h>
#import <sys/kern_return.h>
#import <sys/machine.h>
#import <sys/port.h>
#import <kern/processor.h>
#import <kern/assert.h>

#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <vm/vm_map.h>
#import <vm/vm_user.h>

host_data_t	realhost;

kern_return_t host_processors(host, processor_list, count)
	host_t			host;
	processor_array_t	*processor_list;
	long			*count;
{
	vm_size_t		size, new_size;
	register int		i;
	register port_t		*tp;
	vm_offset_t		addr;
	extern vm_map_t		ipc_kernel_map;	/* XXX use different map */

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);

	*processor_list = (processor_array_t) 0;
	*count = 0;

	/*
	 *	Allocate enough VM to hold the list of processors.  This
	 *	may end up being too large since cpu numbering may be
	 *	sparse. 
	 */

	size = round_page(NCPUS * sizeof(port_t));
	(void) vm_allocate(ipc_kernel_map, &addr, size, TRUE);
	tp = (port_t *) addr;
	*processor_list = tp;


	for (i = 0; i < NCPUS; i++) {
		if (machine_slot[i].is_cpu) {
		    *tp++ = convert_processor_to_port(cpu_to_processor(i));
		    (*count)++;
		}
	}
	/*
	 *	Now, since we probably have too much memory, free up any
	 *	that won't be freed automatically on the way out.
	 */
	if (*count < NCPUS) {
		new_size = round_page(*count * sizeof(port_t));
		if (new_size != size) {
			kmem_free(ipc_kernel_map,
				((vm_offset_t)*processor_list) + new_size,
				size - new_size);
		}
	}
	if (*count == 0)
		*processor_list = 0;

	return(KERN_SUCCESS);
}

kern_return_t	host_info(host, flavor, info, count)
host_t		host;
int		flavor;
host_info_t	info;
int		*count;
{
	register int			i, *slot_ptr;

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);
	
	switch(flavor) {

	case HOST_BASIC_INFO:
		{ register host_basic_info_t	basic_info;
		/*
		 *	Basic information about this host.
		 */
		if (*count < HOST_BASIC_INFO_COUNT)
			return(KERN_FAILURE);

		basic_info = (host_basic_info_t) info;

		basic_info->max_cpus = machine_info.max_cpus;
		basic_info->avail_cpus = machine_info.avail_cpus;
		basic_info->memory_size = machine_info.memory_size;
		basic_info->cpu_type =
			machine_slot[master_processor->slot_num].cpu_type;
		basic_info->cpu_subtype =
			machine_slot[master_processor->slot_num].cpu_subtype;

		*count = HOST_BASIC_INFO_COUNT;
		return(KERN_SUCCESS);
		}
	case HOST_PROCESSOR_SLOTS:
		/*
		 *	Return numbers of slots with active processors
		 *	in them.
		 */
		if (*count < NCPUS)
			return(KERN_INVALID_ARGUMENT);

		slot_ptr = (int *)info;
		*count = 0;
		for (i = 0; i < NCPUS; i++) {
			if (machine_slot[i].is_cpu &&
				machine_slot[i].running) {
					*slot_ptr++ = i;
					(*count)++;
				}
		}
		return(KERN_SUCCESS);

	case HOST_SCHED_INFO:
		{ register host_sched_info_t	sched_info;
		  extern int tick; /* XXX */
		/*
		 *	Return scheduler information.
		 */
		if (*count < HOST_SCHED_INFO_COUNT)
			return(KERN_FAILURE);

		sched_info = (host_sched_info_t) info;

		sched_info->min_timeout = tick/1000;
		sched_info->min_quantum = tick/1000;

		*count = HOST_SCHED_INFO_COUNT;
		return(KERN_SUCCESS);
		}
		/*
		 */
		
	default:
		return(KERN_INVALID_ARGUMENT);
	}
}

/*
 *	Return kernel version string (more than you ever
 *	wanted to know about what version of the kernel this is).
 */

kern_return_t host_kernel_version(host, out_version)
host_t	host;
kernel_version_t	out_version;
{
	extern char	version[];
	int		count;
	kern_return_t	result;

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);

	count = KERNEL_VERSION_MAX;
	result = copystr(version, (char *)out_version, count, 0);

	if (result != 0)
		return (KERN_INVALID_ARGUMENT);
		
	return(KERN_SUCCESS);
}

/*
 *	host_processor_sets:
 *
 *	List all processor sets on the host.
 */
#if	MACH_HOST
kern_return_t
host_processor_sets(host, pset_list, count)
host_t				host;
processor_set_name_array_t	*pset_list;
unsigned int			*count;
{
	unsigned int actual;	/* this many psets */
	processor_set_t pset;
	port_t *psets;
	int i;
	extern vm_map_t		ipc_kernel_map;	/* XXX use different map */

	vm_size_t size;
	vm_size_t size_used;
	vm_offset_t addr;

	if (host == HOST_NULL)
		return KERN_INVALID_ARGUMENT;

	size = 0; addr = 0;

	for (;;) {
		vm_size_t size_needed;

		simple_lock(&all_psets_lock);
		actual = all_psets_count;

		/* do we have the memory we need? */

		size_needed = actual * sizeof(port_t);
		if (size_needed <= size)
			break;

		/* unlock and allocate more memory */
		simple_unlock(&all_psets_lock);

		if (size != 0)
			(void) kmem_free(ipc_kernel_map, addr, size);

		size = round_page(2 * size_needed);

		/* allocate memory non-pageable, so don't fault
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

	/* OK, have memory and the all_psets_lock */

	psets = (port_t *) addr;

	for (i = 0, pset = (processor_set_t) queue_first(&all_psets);
	     i < actual;
	     i++, pset = (processor_set_t) queue_next(&pset->all_psets))
		psets[i] = convert_pset_name_to_port(pset);
	assert(queue_end(&all_psets, (queue_entry_t) pset));

	/* can unlock now that we've got the ports */
	simple_unlock(&all_psets_lock);

	/*
	 *	Always have default port, so no ports case never happens.
	 */
	*pset_list = psets;
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

	return KERN_SUCCESS;
}
#else	MACH_HOST
/*
 *	Only one processor set, the default processor set, in this case.
 */
kern_return_t
host_processor_sets(host, pset_list, count)
host_t				host;
processor_set_name_array_t	*pset_list;
unsigned int			*count;
{
	vm_offset_t addr;
	extern vm_map_t		ipc_kernel_map;	/* XXX use different map */

	if (host == HOST_NULL)
		return KERN_INVALID_ARGUMENT;

	/*
	 *	Allocate memory.  Can be pageable because it won't be
	 *	touched while holding a lock.
	 */

	(void) vm_allocate(ipc_kernel_map, &addr, PAGE_SIZE, TRUE);


	*((port_t *) addr) = convert_pset_name_to_port(&default_pset);

	*pset_list = (port_t *) addr;
	*count = 1;

	return KERN_SUCCESS;
}
#endif	MACH_HOST

/*
 *	host_processor_set_priv:
 *
 *	Return control port for given processor set.
 */
kern_return_t
host_processor_set_priv(host, pset_name, pset)
host_t	host;
processor_set_t	pset_name, *pset;
{
    if ((host == HOST_NULL) || (pset_name == PROCESSOR_SET_NULL)) {
	*pset = PROCESSOR_SET_NULL;
	return(KERN_INVALID_ARGUMENT);
    }

    *pset = pset_name;
    return(KERN_SUCCESS);
}
