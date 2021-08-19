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
 * $Log:	ipc_statistics.c,v $
 *  2-Mar-90  Gregg Kellogg (gk) at NeXT
 *	MACH_OLD_VM_COPY:	Host_ipc_bucket_info uses ipc_soft_map
 *				instead of copy_maps.
 * Revision 2.13  89/10/10  10:54:25  mwyoung
 * 	Use new vm_map_copy technology.
 * 	[89/10/02            mwyoung]
 * 
 * Revision 2.12  89/05/12  12:44:14  gm0w
 * 	Need mach/vm_param.h for round_page.
 * 	[89/05/12            gm0w]
 * 
 * Revision 2.11  89/05/11  14:41:21  gm0w
 * 	Added host_ipc_bucket_info.
 * 	[89/05/07  20:11:39  rpd]
 * 
 * Revision 2.10  89/05/01  17:01:07  rpd
 * 	Renamed port_copyout_hits as port_copyin_miss.
 * 	[89/05/01  14:48:19  rpd]
 * 
 * Revision 2.9  89/02/25  18:04:15  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.8  89/01/15  16:23:40  rpd
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  15:00:44  rpd]
 * 
 * Revision 2.7  89/01/12  07:56:09  rpd
 * 	Moved ipc_statistics.h to mach_debug/.  Added MACH_DEBUG conditionals.
 * 	[89/01/12  04:46:45  rpd]
 * 
 * Revision 2.6  89/01/10  23:30:22  rpd
 * 	Added locking around the ipc_statistics structure.
 * 	[89/01/10  23:05:09  rpd]
 * 
 * Revision 2.5  88/12/19  02:45:29  mwyoung
 * 	Remove lint.
 * 	[88/12/08            mwyoung]
 * 
 * Revision 2.4  88/10/11  10:18:41  rpd
 * 	When resetting IPC statistics, don't change current.
 * 	[88/10/10  07:58:08  rpd]
 * 
 * Revision 2.3  88/09/25  22:13:57  rpd
 * 	Changed includes to the new style.
 * 	Added host_ipc_statistics_reset.
 * 	[88/09/09  04:42:43  rpd]
 * 
 * Revision 2.2  88/07/22  07:32:31  rpd
 * Created for Mach IPC statistics functions.
 * 
 */
/*
 * File:	ipc_statistics.c
 * Purpose:
 *	Code for IPC statistics gathering.
 */

#import <mach_debug.h>
#import <mach_old_vm_copy.h>

#import <vm/vm_param.h>
#import <sys/kern_return.h>
#import <kern/ipc_statistics.h>
#import <kern/task.h>
#import <kern/lock.h>

decl_simple_lock_data(,ipc_statistics_lock_data)
ipc_statistics_t ipc_statistics;

/*
 *	Routine:	ipc_stats_init [exported]
 *	Purpose:
 *		Initialize Mach IPC statistics gathering.
 */
void
ipc_stats_init()
{
	simple_lock_init(&ipc_statistics_lock_data);

	ipc_statistics.version = 77;
	ipc_statistics.messages = 0;
	ipc_statistics.complex = 0;
	ipc_statistics.kernel = 0;
	ipc_statistics.large = 0;
	ipc_statistics.current = 0;
	ipc_statistics.emergency = 0;
	ipc_statistics.notifications = 0;
	ipc_statistics.port_copyins = 0;
	ipc_statistics.port_copyouts = 0;
	ipc_statistics.port_copyin_hits = 0;
	ipc_statistics.port_copyin_miss = 0;
	ipc_statistics.port_allocations = 0;
	ipc_statistics.bucket_misses = 0;
	ipc_statistics.ip_data_grams = 0;
}

#if	MACH_DEBUG
#import <kern/ipc_hash.h>
#import <kern/ipc_globals.h>

/*
 *	Routine:	host_ipc_statistics [exported, user]
 *	Purpose:
 *		Return the accumulated IPC statistics.
 */
kern_return_t
host_ipc_statistics(task, statistics)
	task_t task;
	ipc_statistics_t *statistics;
{
	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	ipc_statistics_lock();
	*statistics = ipc_statistics;
	ipc_statistics_unlock();

	return KERN_SUCCESS;
}

/*
 *	Routine:	host_ipc_statistics_reset [exported, user]
 *	Purpose:
 *		Reset the accumulated IPC statistics.
 */
kern_return_t
host_ipc_statistics_reset(task)
	task_t task;
{
	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	ipc_statistics_lock();
	ipc_statistics.messages = 0;
	ipc_statistics.complex = 0;
	ipc_statistics.kernel = 0;
	ipc_statistics.large = 0;
	ipc_statistics.emergency = 0;
	ipc_statistics.notifications = 0;
	ipc_statistics.port_copyins = 0;
	ipc_statistics.port_copyouts = 0;
	ipc_statistics.port_copyin_hits = 0;
	ipc_statistics.port_copyin_miss = 0;
	ipc_statistics.port_allocations = 0;
	ipc_statistics.bucket_misses = 0;
	ipc_statistics.ip_data_grams = 0;
	ipc_statistics_unlock();

	return KERN_SUCCESS;
}

/*
 *	Routine:	host_ipc_bucket_sizes
 *	Purpose:
 *		Return the number of translations in each IPC bucket.
 */
kern_return_t
host_ipc_bucket_info(task, TLinfo, TLinfoCnt, TPinfo, TPinfoCnt)
	task_t task;
	ipc_bucket_info_array_t *TLinfo;
	unsigned int *TLinfoCnt;
	ipc_bucket_info_array_t *TPinfo;
	unsigned int *TPinfoCnt;
{
	vm_offset_t addr1, addr2;
	vm_size_t size;
	int num_buckets = PORT_HASH_COUNT;
	ipc_bucket_info_t *tlinfo, *tpinfo;
	int i;
	kern_return_t kr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	size = round_page(num_buckets * sizeof(ipc_bucket_info_t));

	/*
	 *	Allocate memory in the ipc_kernel_map, because
	 *	we need to touch it, and then move it to the ipc_soft_map
	 *	(where the IPC code expects to find it) when we're done.
	 *
	 *	Because we don't touch the memory with any locks held,
	 *	it can be left pageable.
	 */

	kr = vm_allocate(ipc_kernel_map, &addr1, size, TRUE);
	if (kr != KERN_SUCCESS)
		panic("host_ipc_bucket_info: vm_allocate");

	kr = vm_allocate(ipc_kernel_map, &addr2, size, TRUE);
	if (kr != KERN_SUCCESS)
		panic("host_ipc_bucket_info: vm_allocate");

	tlinfo = (ipc_bucket_info_t *) addr1;
	tpinfo = (ipc_bucket_info_t *) addr2;

	for (i = 0; i < num_buckets; i++) {
		register port_hash_bucket_t *tl = &TL_table[i];
		register port_hash_bucket_t *tp = &TP_table[i];
		register port_hash_t entry;
		int count;

		count = 0;
		bucket_lock(tl);
		for (entry = (port_hash_t) queue_first(&tl->head);
		     !queue_end(&tl->head, (queue_entry_t) entry);
		     entry = (port_hash_t) queue_next(&entry->TL_chain))
			count++;
		bucket_unlock(tl);
		tlinfo++->count = count;

		count = 0;
		bucket_lock(tp);
		for (entry = (port_hash_t) queue_first(&tp->head);
		     !queue_end(&tp->head, (queue_entry_t) entry);
		     entry = (port_hash_t) queue_next(&entry->TP_chain))
			count++;
		bucket_unlock(tp);
		tpinfo++->count = count;
	}

	/*
	 *	Move memory to ipc_soft_map.
	 */

#if	MACH_OLD_VM_COPY
	kr = vm_move(ipc_kernel_map, addr1,
			ipc_soft_map, size, TRUE,
			(vm_offset_t *) TLinfo);
	if (kr != KERN_SUCCESS)
		panic("host_ipc_bucket_info: vm_move");

	kr = vm_move(ipc_kernel_map, addr2,
			ipc_soft_map, size, TRUE,
			(vm_offset_t *) TPinfo);
	if (kr != KERN_SUCCESS)
		panic("host_ipc_bucket_info: vm_move");
#else	MACH_OLD_VM_COPY
	kr = vm_map_copyin(ipc_kernel_map, addr1,
		     size, TRUE,
		     (vm_map_copy_t *) TLinfo);
	if (kr != KERN_SUCCESS)
		panic("host_ipc_bucket_info: vm_map_copyin");

	kr = vm_map_copyin(ipc_kernel_map, addr2,
		     size, TRUE,
		     (vm_map_copy_t *) TPinfo);
	if (kr != KERN_SUCCESS)
		panic("host_ipc_bucket_info: vm_map_copyin");
#endif	MACH_OLD_VM_COPY

	*TLinfoCnt = num_buckets;
	*TPinfoCnt = num_buckets;

	return KERN_SUCCESS;
}

#endif	MACH_DEBUG


