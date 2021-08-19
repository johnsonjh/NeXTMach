/*
 *	Copyright (C) 1988, 1989,  NeXT, Inc.
 *
 *	File:	kern/mach_loader.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Mach object file loader (kernel version, for now).
 *
 * HISTORY
 *  5-Mar-90  John Seamons (jks) at NeXT
 *	Redefined cpu_type and cpu_subtype definitions to indicate processor
 *	architecture instead of product types for the MC680x0.  Call machine
 *	dependent routine check_cpusubtype() to check the cpu subtype info.
 *
 * 02-Oct-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Don't blow away user's address space in presence
 *	of load errors.
 *
 * 21-Jul-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Started.
 */

#import <sys/types.h>
#import <sys/time.h>
#import <sys/vnode.h>
#import <sys/uio.h>

#import <sys/kern_return.h>
#import <sys/loader.h>
#import <sys/machine.h>
#import <kern/task.h>
#import <kern/thread.h>

#import <machine/cpu.h>

#import <vm/vm_map.h>
#import <vm/vm_kern.h>
#import <vm/vm_param.h>
#import <vm/vm_pager.h>
#import <vm/vnode_pager.h>
#import <vm/pmap.h>

kern_return_t load_machfile(vp, header, needargs)
	struct vnode		*vp;
	struct mach_header	*header;
	boolean_t		*needargs;
{
	pmap_t		pmap;
	vm_map_t	map, old_map;
	kern_return_t	ret;

	old_map = current_task()->map;
	pmap = old_map->pmap;
	pmap_reference(pmap);
	map = vm_map_create(pmap,
			old_map->min_offset,
			old_map->max_offset,
			old_map->entries_pageable);

	if (needargs)
		*needargs = FALSE;

	ret = parse_machfile(vp, map, header, 0, (unsigned long *)0, needargs);

	if (ret != KERN_SUCCESS) {
		vm_map_deallocate(map);	/* will lose pmap reference too */
		return(ret);
	}
	/*
	 *	Commit to new map, destroy old map.  (Destruction
	 *	of old map cleans up pmap data structures too).
	 */
	current_task()->map = map;
	vm_map_deallocate(old_map);
	return(KERN_SUCCESS);
}

kern_return_t parse_machfile(vp, map, header, depth, lib_version, needargs)
	struct vnode		*vp;
	vm_map_t		map;
	struct mach_header	*header;
	int			depth;
	unsigned long		*lib_version;
	boolean_t		*needargs;
{
	struct machine_slot	*ms;
	int			ncmds;
	struct load_command	*lcp, *next;
	vm_pager_t		pager;
	kern_return_t		ret;
	vm_offset_t		addr;
	vm_size_t		size;
	int			offset;
	int			nthreads;
	int			pass;

	if (depth > 6)
		return(KERN_FAILURE);

	depth++;

	/*
	 *	Check to see if right machine type.
	 */
	ms = &machine_slot[cpu_number()];
	if ((header->cputype != ms->cpu_type) ||
	    !check_cpu_subtype(header->cpusubtype))
		return(KERN_FAILURE);

	/*
	 *	Only load libraries if we are not at the top level.
	 */
	if (depth != 1) {
		if (header->filetype != MH_FVMLIB)
			return(KERN_FAILURE);
	}

	pager = (vm_pager_t) vnode_pager_setup(vp, FALSE, TRUE);

	/*
	 *	Map portion that must be accessible directly into
	 *	kernel's map.
	 */

	/* should sanity check size */
	size = round_page(header->sizeofcmds);
	if (size <= 0)
		return(KERN_FAILURE);
	addr = 0;
	ret = vm_allocate_with_pager(kernel_map, &addr, size, TRUE, pager, 0);
	if (ret != KERN_SUCCESS) {
		return(ret);
	}

	/*
	 *	Scan through the commands, processing each one as necessary.
	 */
	nthreads = 0;
	for (pass = 1; pass <= 2; pass++) {
		offset = sizeof(struct mach_header);
		ncmds = header->ncmds;
		while (ncmds--) {
			lcp = (struct load_command *)(addr + offset);
			offset += lcp->cmdsize;
			/*
			 *	Check for valid lcp pointer by checking new offset.
			 */
			if (offset > (size - sizeof(struct load_command))) {
				vm_map_remove(kernel_map, addr, addr + size);
				return(KERN_FAILURE);
			}

			/*
			 *	Check for valid command.
			 */
			switch(lcp->cmd) {
			case LC_SEGMENT:
				if (pass != 1)
					break;
				ret = load_segment(
					   (struct segment_command *) lcp,
						   pager, map);
				break;
			case LC_THREAD:
				if (pass != 2)
					break;
				nthreads++;
				ret = load_thread((struct thread_command *)lcp,
						  nthreads);
				break;
			case LC_UNIXTHREAD:
				if (pass != 2)
					break;
				ret = load_unixthread(
					   (struct thread_command *) lcp, map);
				nthreads++;
				if (needargs)
					*needargs = TRUE;
				break;
			case LC_LOADFVMLIB:
				if (pass != 1)
					break;
				ret = load_fvmlib((struct fvmlib_command *)lcp,
						  map, depth);
				break;
			case LC_IDFVMLIB:
				if (pass != 1)
					break;
				if (lib_version) {
					ret = load_idfvmlib(
						(struct fvmlib_command *)lcp,
						lib_version);
				}
				break;
			default:
				ret = KERN_SUCCESS;/* ignore other stuff */
			}
			if (ret != KERN_SUCCESS)
				break;
		}
		if (ret != KERN_SUCCESS)
			break;
	}
	vm_map_remove(kernel_map, addr, addr + size);
	if ((ret == KERN_SUCCESS) && (depth == 1) && (nthreads == 0))
		ret = KERN_FAILURE;
	return(ret);
}

kern_return_t load_segment(scp, pager, map)
	struct segment_command	*scp;
	vm_pager_t		pager;
	vm_map_t		map;
{
	int			vmsize, copysize;
	kern_return_t		ret;
	vm_offset_t		dest_addr, src_addr;
	vm_map_t		temp_map;

	vmsize = round_page(scp->vmsize);
	if (vmsize < 0)
		return(KERN_FAILURE);

	if (vmsize == 0)
		return(KERN_SUCCESS);

	dest_addr = trunc_page(scp->vmaddr);
	ret = vm_map_find(map, VM_OBJECT_NULL, (vm_offset_t)0,
			  &dest_addr, vmsize, FALSE);

	if (ret != KERN_SUCCESS)
		return(ret);

	/*
	 *	Map file into a temporary map.
	 */
	copysize = round_page(scp->filesize);
	if (copysize < 0)
		return(KERN_FAILURE);
	if (copysize > 0) {
		temp_map = vm_map_create(pmap_create(copysize),
				 VM_MIN_ADDRESS, VM_MIN_ADDRESS + copysize);
		src_addr = VM_MIN_ADDRESS;
		ret = vm_allocate_with_pager(temp_map, &src_addr,
				copysize, FALSE, pager, scp->fileoff);
		if (ret != KERN_SUCCESS) {
			vm_map_deallocate(temp_map);
			return(ret);
		}

		/*
		 *	If last page is not complete, we need to zero fill
		 *	it.
		 */
		if (copysize != scp->filesize) {
			vm_offset_t	tmp_addr;
			int		trunc_addr;

			trunc_addr = trunc_page(scp->filesize);
			/*
			 *	Allocate some space accessible to the kernel.
			 */
			tmp_addr = 0;
			ret = vm_map_find(kernel_map, VM_OBJECT_NULL, (vm_offset_t)0,
			  &tmp_addr, PAGE_SIZE, TRUE);
			if (ret != KERN_SUCCESS) {
				vm_map_deallocate(temp_map);
				return(ret);
			}
			/*
			 *	Copy last page into kernel.
			 */
			ret = vm_map_copy(kernel_map, temp_map,
				tmp_addr, PAGE_SIZE, trunc_addr,
				FALSE, FALSE);
			if (ret != KERN_SUCCESS) {
				vm_deallocate(kernel_map, tmp_addr, PAGE_SIZE);
				vm_map_deallocate(temp_map);
				return(ret);
			}
			/*
			 *	Zero appropriate bytes in copy-on-write copy.
			 */
			bzero(tmp_addr + (scp->filesize - trunc_addr),
				copysize - scp->filesize);
			/*
			 *	Copy new data back to user task map.
			 */
			ret = vm_map_copy(map, kernel_map,
				dest_addr + trunc_addr, PAGE_SIZE, tmp_addr,
				FALSE, FALSE);
			vm_deallocate(kernel_map, tmp_addr, PAGE_SIZE);
			if (ret != KERN_SUCCESS) {
				vm_map_deallocate(temp_map);
				return(ret);
			}
			copysize = trunc_addr;	/* for correct copy below */
		}

		/*
		 *	Copy the data into map.
		 */
		ret = vm_map_copy(map, temp_map, dest_addr, copysize, src_addr,
				FALSE, FALSE);
		vm_map_deallocate(temp_map);
		if (ret != KERN_SUCCESS)
			return(ret);
	}

#if	0
	/*
	 *	Prepage data XXX do this only on a flag.
	 */

	/*
	 *	Touch each page by referencing it.
	 */
	addr = dest_addr;
	while (addr < dest_addr + copysize) {
		(void) vm_fault(map, addr, VM_PROT_READ, FALSE);
		addr += PAGE_SIZE;
	}
#endif	0

	/*
	 *	Set protection values. (Note: ignore errors!)
	 */

	if (scp->maxprot != VM_PROT_DEFAULT) {
		(void) vm_map_protect(map,
				      dest_addr,
				      dest_addr + vmsize,
				      scp->maxprot,
				      TRUE);
	}
	if (scp->initprot != VM_PROT_DEFAULT) {
		(void) vm_map_protect(map,
				      dest_addr,
				      dest_addr + vmsize,
				      scp->initprot,
				      FALSE);
	}
	return(KERN_SUCCESS);
}

kern_return_t load_unixthread(tcp, map)
	struct thread_command	*tcp;
	vm_map_t		map;
{
	kern_return_t	ret, create_unix_stack();
	vm_offset_t	addr;
	vm_size_t	size;

	/*
	 *	Setup the stack.
	 */
	ret = create_unix_stack(map);
	if (ret != KERN_SUCCESS)
		return(ret);
	ret = load_threadstate(current_thread(),
		       ((vm_offset_t)tcp) + sizeof(struct thread_command),
		       tcp->cmdsize - sizeof(struct thread_command));
	return(ret);
}

kern_return_t load_thread(tcp, nthreads)
	struct thread_command	*tcp;
	int			nthreads;
{
	thread_t	thread;
	kern_return_t	ret;

	if (nthreads == 1)
		thread = current_thread();
	else {
		ret = thread_create(current_task(), &thread);
		if (ret != KERN_SUCCESS)
			return(ret);
	}

	ret = load_threadstate(thread,
		       ((vm_offset_t)tcp) + sizeof(struct thread_command),
		       tcp->cmdsize - sizeof(struct thread_command));

	/*
	 *	Resume thread now, note that this means that the thread
	 *	commands should appear after all the load commands to
	 *	be sure they don't reference anything not yet mapped.
	 */
	if (nthreads != 1)
		thread_resume(thread);

	return(ret);
}

kern_return_t load_threadstate(thread, ts, total_size)
	thread_t	thread;
	unsigned long	*ts;
	unsigned long	total_size;
{
	kern_return_t	ret;
	unsigned long	size;
	int		flavor;

	/*
	 *	Set the thread state.
	 */

	while (total_size > 0) {
		flavor = *ts++;
		size = *ts++;
		total_size -= (size+2)*sizeof(unsigned long);
		if (total_size < 0)
			return(KERN_FAILURE);
		ret = thread_setstatus(thread, flavor, ts, size);
		if (ret != KERN_SUCCESS)
			return(ret);
		ts += size;	/* ts is a (unsigned long *) */
	}
	return(KERN_SUCCESS);
}

kern_return_t load_fvmlib(lcp, map, depth)
	struct fvmlib_command	*lcp;
	vm_map_t		map;
	int			depth;
{
	char		*name, *p;
	struct vnode	*vp;
	struct mach_header	header;
	unsigned long	lib_version;
	int		error;
	kern_return_t	ret;

	name = (char *)lcp + lcp->fvmlib.name.offset;
	/*
	 *	Check for a proper null terminated string.
	 */
	p = name;
	do {
		if (p >= (char *)lcp + lcp->cmdsize)
			return(KERN_FAILURE);
	} while (*p++);

	ret = KERN_SUCCESS;
	error = lookupname(name, UIO_SYSSPACE, FOLLOW_LINK,
			   (struct vnode **)0, &vp);

	if (error)
		return(KERN_FAILURE);

	error = check_exec_access(vp);
	if (error) {
		ret = KERN_FAILURE;
		goto out;
	}

	error = vn_rdwr(UIO_READ, vp, (caddr_t)&header, sizeof(header),
			0, UIO_SYSSPACE, IO_UNIT, 0);

	if (error || (header.magic != MH_MAGIC)) {
		ret = KERN_FAILURE;
		goto out;
	}

	ret = parse_machfile(vp, map, &header, depth, &lib_version, 0);

	if ((ret == KERN_SUCCESS) &&
	    (lib_version < lcp->fvmlib.minor_version))
		ret = KERN_FAILURE;

out:
	VN_RELE(vp);
	return(ret);
}

kern_return_t load_idfvmlib(lcp, version)
	struct fvmlib_command	*lcp;
	unsigned long		*version;
{
	*version = lcp->fvmlib.minor_version;
	return(KERN_SUCCESS);
}




