/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * HISTORY
 * 30-Apr-90  Avadis Tevanian (avie) at NeXT
 *	Cause vsunlock to actually mark pages as dirty.
 *
 * 15-Apr-90  Avadis Tevanian (avie) at NeXT
 *	Remove special case code for pid 0 being the kernel task.
 *
 * 08-Apr-90  Avadis Tevanian (avie) at NeXT
 *	Beef up unix_pid to better deal with kernel task and task's without proc
 *	structures.
 *
 * 30-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Don't set new thread's proirity from forking threads, instead it
 *	is inherited from the parent task and set in thread_create().
 *
 * 04-Jan-88  Avadis Tevanian (avie) at NeXT
 *	Delete references to proc->thread.  There were unnecessary and
 *	that field is gone now.
 *
 * 13-Dec-88  Avadis Tevanian (avie) at NeXT
 *	Return kernel_task instead of first_task for proc 0.
 *
 * 12-Sep-88  Avadis Tevanian (avie) at NeXT
 *	Use kalloc/kfree instead of kmem_alloc/kmem_free.
 *
 * 13-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Removed dependencies on proc table.  No special abilities
 *	for group 2.
 *
 * 31-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Some more de-linting.
 *
 *  2-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	fake_u: must get times from thread now.
 *
 *  1-Mar-88  Robert Baron (rvb) at Carnegie-Mellon University
 *	Make object_special mesh with new sun pmap module.
 *
 * 25-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Neither task_create nor thread_create return the data port
 *	any longer.
 *
 * 25-Jan-88  Karl Hauth (hauth) at Carnegie-Mellon University
 *	Extended task_by_unix_pid() to give you the kernel's task if you
 *	ask for pid 0.
 *
 * 21-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Use port_deallocate() rather than port_release() in
 *	procdup().
 *
 * 14-Jan-88  Joseph Boykin (boykin) at Encore Computer Corporation
 *	Added test in procdup on result of task_create and thread_create
 *	to see if we couldn't create the task/thread.  For now, only print
 *	an error message.
 *
 * 14-Jan-88  John Seamons (jks) at NeXT
 *	Changed "len-1" to "len" in useracc, vslock and vsunlock.
 *
 * 11-Sep-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	unix_master() calls must be bracketed with unix_release().
 *
 * 17-Aug-87  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Changed vm_object_special() to save two extra type bits
 *	for Sun physical memory and don't cache bit into three
 *	least significant bits of physical address for
 *	pmap_enter() to pick up later.
 *
 * 14-Jul-87  David Black (dlb) at Carnegie-Mellon University
 *	fake_u: u_code is now in thread uarea.
 *
 * 23-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	Initialize thread priorities in procdup.
 *
 * 06-Jun-87  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Added code to vm_object_special() to deal with the 34 bits
 *	needed to represent a physical address on the Sun 3.
 *
 *  4-Jun-87  William Bolosky (bolosky) at Carnegie-Mellon University
 *	Eliminated pager_ids in device pager.
 *
 * 20-May-87  David Golub (dbg) at Carnegie-Mellon University
 *	Added more fields to fake_u for core()'s benefit.
 *	Add a (gross) hack for VAX to keep adb happy - pretend
 *	the kernel stack is in its old location (ecch).
 *
 * 19-May-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added task_by_unix_pid, to replace task_by_pid as soon as feasible.
 *
 * 27-Apr-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Move ipc_task_init out of procdup.
 *
 * 27-Apr-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Changed how port_copyout is used.
 *
 * 22-Apr-87  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Add code to procdup which will force the is_child variable onto
 *	the stack by passing its address to a do-nothing function.  This
 *	is because certain optimizing compilers will otherwise put this
 *	variable in a register, regardless of the lack of a 'register'
 *	declaration.
 *
 *  4-Apr-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH_MP: task_by_pid moved here to be with unix_pid.
 *
 *  8-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Implement fake_u call for use under MACH_TT.
 *
 *  4-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	Split up paging system lock.
 *	Corrected return value from (*mapfun)() in vm_object_special, so
 *	that it works on all machines.  (I should really change this and
 *	all the device drivers to return physical byte addresses.)
 *
 *  1-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Updated for latest u-area hacks.
 *
 * 30-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Initialize thread priority based on current thread rather than
 *	the parent process.
 *
 * 29-Jan-87  David L. Black (dlb) at Carnegie-Mellon University
 *	Initialize pri field of newly created thread.
 *
 * 08-Jan-87  Robert Beck (beck) at Sequent Computer Systems, Inc.
 *	If BALANCE, fu/su* are defined in asm.
 *	Add include cputypes.h for BALANCE definition.
 *
 * 21-Oct-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added vm_object_special and device pager to map in frame buffers.
 *
 * 21-Oct-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Don't use the routines in here for setting and fetching
 *	user bytes and words for the Sun because the Sun has faster
 *	versions of the same routines.
 *
 * 14-Oct-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Fixed call to ipc_task_init.
 *
 * 30-Sep-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Removed task and thread tables.  Support MACH_TT in procdup.
 *
 *  8-Aug-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added hooks for ipc fields in the task structure.
 *
 * 11-Aug-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added fuibyte for 4.3.
 *
 ****************************************************************
 *
 */
#import <cputypes.h>
#import <mach_xp.h>

#import <kern/task.h>
#import <kern/thread.h>
#import <sys/time_value.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <vm/vm_page.h>
#import <kern/parallel.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/vm.h>
#import <sys/file.h>
#import <sys/vnode.h>
#import <sys/buf.h>
#import <ufs/mount.h>
#import <sys/trace.h>
#import <sys/kernel.h>

#import <kern/kalloc.h>
#import <sys/port.h>
#import <kern/ipc_globals.h>

#import <machine/spl.h>

useracc(addr, len, prot)
	caddr_t	addr;
	u_int	len;
	int	prot;
{
	return (vm_map_check_protection(
			current_task()->map,
			trunc_page(addr), round_page(addr+len),
			prot == B_READ ? VM_PROT_READ : VM_PROT_WRITE));
}

vslock(addr, len)
	caddr_t	addr;
	int	len;
{
	vm_map_pageable(current_task()->map, trunc_page(addr),
				round_page(addr+len), FALSE);
}

vsunlock(addr, len, dirtied)
	caddr_t	addr;
	int	len;
	int dirtied;
{
#if	NeXT
	pmap_t		pmap;
	vm_page_t	pg;
	vm_offset_t	vaddr, paddr;

	if (dirtied) {
		pmap = current_task()->map->pmap;
		for (vaddr = trunc_page(addr); vaddr < round_page(addr+len);
				vaddr += PAGE_SIZE) {
			paddr = pmap_extract(pmap, vaddr);
			ASSERT(paddr != 0);
			pg = PHYS_TO_VM_PAGE(paddr);
			vm_page_set_modified(pg);
		}
	}
#endif	NeXT
#ifdef	lint
	dirtied++;
#endif	lint
	vm_map_pageable(current_task()->map, trunc_page(addr),
				round_page(addr+len), TRUE);
}

#if	defined(sun) || BALANCE
#else	defined(sun) || BALANCE
subyte(addr, byte)
	caddr_t addr;
	char byte;
{
	return (copyout((caddr_t) &byte, addr, sizeof(char)) == 0 ? 0 : -1);
}

suibyte(addr, byte)
	caddr_t addr;
	char byte;
{
	return (copyout((caddr_t) &byte, addr, sizeof(char)) == 0 ? 0 : -1);
}

int fubyte(addr)
	caddr_t addr;
{
	char byte;

	if (copyin(addr, (caddr_t) &byte, sizeof(char)))
		return(-1);
	return((unsigned) byte);
}

int fuibyte(addr)
	caddr_t addr;
{
	char byte;

	if (copyin(addr, (caddr_t) &byte, sizeof(char)))
		return(-1);
	return((unsigned) byte);
}

suword(addr, word)
	caddr_t addr;
	int word;
{
	return (copyout((caddr_t) &word, addr, sizeof(int)) == 0 ? 0 : -1);
}

int fuword(addr)
	caddr_t addr;
{
	int word;

	if (copyin(addr, (caddr_t) &word, sizeof(int)))
		return(-1);
	return(word);
}

/* suiword and fuiword are the same as suword and fuword, respectively */

suiword(addr, word)
	caddr_t addr;
	int word;
{
	return (copyout((caddr_t) &word, addr, sizeof(int)) == 0 ? 0 : -1);
}

int fuiword(addr)
	caddr_t addr;
{
	int word;

	if (copyin(addr, (caddr_t) &word, sizeof(int)))
		return(-1);
	return(word);
}
#endif	defined(sun) || BALANCE

swapon()
{
}

thread_t procdup(child, parent)
	struct proc *child, *parent;
{
	thread_t	thread;
	task_t		task;
 	kern_return_t	result;

	result = task_create(parent->task, TRUE, &task);
	if(result != KERN_SUCCESS)
	    printf("fork/procdup: task_create failed. Code: 0x%x\n", result);
	child->task = task;

	/* XXX Cheat to get proc pointer into task structure */
	task->proc = child;

	result = thread_create(task, &thread);
	if(result != KERN_SUCCESS)
	    printf("fork/procdup: thread_create failed. Code: 0x%x\n", result);

#if	!NeXT
	thread->priority = current_thread()->priority;
#endif	!NeXT
	/*
	 *	Don't need to lock thread here because it can't
	 *	possibly execute and no one else knows about it.
	 */
	compute_priority(thread);

	bcopy((caddr_t) parent->task->u_address,
	      (caddr_t) task->u_address,
	      (unsigned) sizeof(struct utask));
#if	NeXT
	task->u_address->uu_ofile_cnt = 0;
	expand_fdlist(task->u_address, parent->task->u_address->uu_lastfile);
	bcopy(parent->task->u_address->uu_ofile, task->u_address->uu_ofile,
		(parent->task->u_address->uu_lastfile + 1) * sizeof(struct file *));
	bcopy(parent->task->u_address->uu_pofile, task->u_address->uu_pofile,
		(parent->task->u_address->uu_lastfile + 1) * sizeof(char));
#endif	NeXT
	thread->u_address.utask->uu_procp = child;
	bzero((caddr_t) &thread->u_address.utask->uu_ru,
			sizeof(struct rusage));
	bzero((caddr_t) &thread->u_address.utask->uu_cru,
			sizeof(struct rusage));
	thread->u_address.utask->uu_outime = 0;
	return(thread);
}

chgprot(_addr, prot)
	caddr_t		_addr;
	vm_prot_t	prot;
{
	vm_offset_t	addr = (vm_offset_t) _addr;

	return(vm_map_protect(current_task()->map,
				trunc_page(addr),
				round_page(addr + 1),
				prot, FALSE) == KERN_SUCCESS);
}

kern_return_t	unix_pid(t, x)
	task_t	t;
	int	*x;
{
	if (t == TASK_NULL) {
		*x = -1;
		return(KERN_FAILURE);
	} else {
#if	NeXT
		if (t->proc)
			*x = t->proc->p_pid;
		else {
			*x = -1;
			return(KERN_FAILURE);
		}
#else	NeXT
		*x =  t->proc->p_pid;
#endif	NeXT
		return(KERN_SUCCESS);
	}
}

/*
 *	Routine:	task_by_unix_pid
 *	Purpose:
 *		Get the task port for another "process", named by its
 *		process ID on the same host as "target_task".
 *
 *		Only permitted to privileged processes, or processes
 *		with the same user ID.
 */
kern_return_t	task_by_unix_pid(target_task, pid, t)
	task_t		target_task;
	int		pid;
	task_t		*t;
{
	struct proc	*p;

	unix_master();

	if (
		((p = pfind(pid)) != (struct proc *) 0)
		&& (target_task->proc != (struct proc *) 0)
		&& ((p->p_uid == target_task->proc->p_uid)
		    || suser())
		&& (p->p_stat != SZOMB)
		) {
			*t = p->task;
			unix_release();
			return(KERN_SUCCESS);
	}
	*t = TASK_NULL;
	unix_release();
	return(KERN_FAILURE);
}

/*
 *	Routine:	task_by_pid
 *	Purpose:
 *		Trap form of "task_by_unix_pid"; soon to be eliminated.
 */
port_t		task_by_pid(pid)
	int		pid;
{
	task_t		self = current_task();
	port_t		t = PORT_NULL;
	task_t		result_task;

	if (task_by_unix_pid(self, pid, &result_task) == KERN_SUCCESS) {
		t = convert_task_to_port(result_task);
		if (t != PORT_NULL)
			object_copyout(self, (kern_obj_t) t,
				       MSG_TYPE_PORT, &t);
	}

	return(t);
}

#if	!MACH_XP
/*
 *	Header to point to a 'device' object and the list
 *	of page structures allocated for it.
 */
struct dev_pager {
	boolean_t	is_device;	/* !is_device ==> this is an istruct.
					   used by vm */
    	vm_object_t	object;
	vm_offset_t	start;	/* start... */
	vm_size_t	size;	/* and size of memory allocated
				   to hold vm_page structures */
};
typedef struct dev_pager	*dev_pager_t;

/*
 *	Create an object to represent pre-existing physical memory
 *	attached to a device (e.g., a frame buffer).
 *
 *	Parameters:
 *		dev		Device that object should map
 *		mapfun		Returns the physical page number
 *				for a page within the device
 *		prot		Protection code to pass to mapfun
 *		offset		Byte offset within device to start
 *				mapping (a page multiple)
 *		size		Number of bytes to map (a page
 *				multiple).
 *	Assumptions:
 *		If the VM page size is N machine pages, those N pages
 *		are physically contiguous, as there is only one VM page
 *		structure to map all N of those pages.  This is NOT
 *		checked.
 *		
 *
 */
vm_object_t vm_object_special(dev, mapfun, prot, offset, size)
	dev_t		dev;
	int		(*mapfun)();
	int		prot;
	vm_offset_t	offset;
	vm_size_t	size;
{
	vm_object_t	object;
	int		num_pages;
	vm_offset_t	memory_start;
	vm_size_t	memory_size;
	dev_pager_t	handle;
	vm_page_t	pg_cur;
	int		i;

	size = round_page(size);
	num_pages = atop(size);

	/*
	 *	Allocate an object
	 */
	object = vm_object_allocate(size);

	/*
	 *	Grab enough memory for vm_page structures to
	 *	represent all pages in the object, and a little
	 *	bit more for a handle on them.
	 */
	memory_size = num_pages * sizeof(struct vm_page)
				+ sizeof(struct dev_pager);
	memory_start = (vm_offset_t) kalloc(memory_size);
	bzero(memory_start, memory_size);

	handle = (dev_pager_t) memory_start;

	/*
	 *	Fill in the handle
	 */
	handle->is_device = TRUE;
	handle->object = object;
	handle->start  = memory_start;
	handle->size   = memory_size;

	/*
	 *	Fill in each page with the proper physical address,
	 *	and attach it to the object.  Mark it as wired-down
	 *	to keep the pageout daemon's hands off it.
	 */

	pg_cur = (vm_page_t) (memory_start + sizeof(struct dev_pager));
	for (i = 0;
	     i < num_pages;
	     i++, pg_cur++) {
#ifdef	sun
		int		pfn;
		unsigned int	type;
 
  		pg_cur->wire_count = 1;
		pfn = (*mapfun)(dev, offset + ptoa(i), prot);
		pg_cur->phys_addr = pmap_phys_address(pfn);
#else	sun
		pg_cur->wire_count = 1;
		/*
		 *	mapfun takes a byte offset, but returns
		 *	a machine-dependent page number (was originally
		 *	a page-table entry, but we won't talk about that).
		 */
		pg_cur->phys_addr = (vm_offset_t) ptoa((*mapfun)(dev,
							offset + ptoa(i),
							prot));
#endif	sun
		/*
		 *	Don't need to lock object - it's not visible
		 *	to anything else.
		 */
		vm_page_insert(pg_cur, object, ptoa(i));
	}

	/*
	 *	Make the object's pager be the 'device' pager
	 *	so that the page structures will be deallocated.
	 *	They can't go onto the free page list!
	 */
	vm_object_setpager(object, (vm_pager_t) handle,	(vm_offset_t) 0, FALSE);

	return (object);
}


boolean_t device_pagein()
{
	panic("device_pagein called");
}

boolean_t device_pageout()
{
	panic("device_pageout called");
}

/*
 *	Deallocate the page structures allocated for a device pager
 */
boolean_t device_dealloc(pager)
	vm_pager_t	pager;
{
	dev_pager_t	handle;
	vm_object_t	object;

	handle = (dev_pager_t) pager;
	object = handle->object;

	/*
	 *	Remove all page structures from the object.
	 *	The object does not need to be locked, since all
	 *	references to it are gone.
	 */
	while (!queue_empty(&object->memq)) {
		vm_page_t	p;

		p = (vm_page_t) queue_first(&object->memq);
		vm_page_remove(p);
	}

	/*
	 *	Free the memory allocated for the page structures
	 *	and handle.
	 */

	kfree((void *)handle->start, handle->size);
}
#endif	!MACH_XP

/*
 *	fake_u:
 *
 *	fake a u-area structure for the specified thread.  Only "interesting"
 *	fields are filled in.
 */
fake_u(up, thread)
	register struct user	*up;
	register thread_t	thread;
{
	register struct utask	*utask;
	register struct uthread	*uthread;
	time_value_t	sys_time, user_time;
	register int	s;

	utask = thread->u_address.utask;
	uthread = thread->u_address.uthread;
#undef	u_pcb
	up->u_pcb = *(thread->pcb);
#ifdef	vax
	/*	HACK HACK HACK	- keep adb happy	*/
	up->u_pcb.pcb_ksp = (up->u_pcb.pcb_ksp & 0x0FFFFFFF) | 0x70000000;
	/*	HACK HACK HACK	- keep adb happy	*/
#endif	vax
#undef	u_comm
	bcopy(utask->uu_comm, up->u_comm, sizeof(up->u_comm));
#undef	u_arg
	bcopy((caddr_t)uthread->uu_arg, (caddr_t)up->u_arg, sizeof(up->u_arg));
#undef  u_cred
	up->u_cred = utask->uu_cred;
#undef	u_signal
	bcopy((caddr_t)utask->uu_signal,
	      (caddr_t)up->u_signal,
	      sizeof(up->u_signal));
#undef	u_code
	up->u_code = uthread->uu_code;
#undef	u_ttyp
	up->u_ttyp = utask->uu_ttyp;
#undef	u_ttyd
	up->u_ttyd = utask->uu_ttyd;
#undef	u_ru
	up->u_ru = utask->uu_ru;
	/*
	 *	Times aren't in uarea any more.
	 */
	s = splsched();
	thread_lock(thread);
	thread_read_times(thread, &user_time, &sys_time);
	thread_unlock(thread);
	splx(s);
	up->u_ru.ru_stime.tv_sec = sys_time.seconds;
	up->u_ru.ru_stime.tv_usec = sys_time.microseconds;
	up->u_ru.ru_utime.tv_sec = user_time.seconds;
	up->u_ru.ru_utime.tv_usec = user_time.microseconds;
#undef	u_cru
	up->u_cru = utask->uu_cru;
}

