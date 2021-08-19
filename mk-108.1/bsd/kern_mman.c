/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 * 18-Feb-90  Gregg Kellogg (gk) at NeXT
 *	!MACH_OLD_VM_COPY: Changes for new vm_map_entry structure
 *	(end -> vme_end).
 *
 * 22-Jul-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	New obreak that is much more robust.
 *
 * 16-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	Correct fix below to return proper U*X return code.  Check that
 *	mapfun is not NULL, nulldev, or nodev when mapping devices.
 *
 * 30-Jan-88  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Added check to smmap to only work on local inodes.
 *
 * 30-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 *  6-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Changed inode_pager_setup() use to new interface.
 *	Removed meaningless history.
 *
 * 19-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Change inode references to vnodes.
 *
 *  9-Apr-87  William Bolosky (bolosky) at Carnegie-Mellon University
 *	MACH_XP: Turned off device file mmaping for the interim.
 *
 *  2-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	Made mmap handle both special devices and files for all machines.
 *
 * 14-Oct-86  David Golub (dbg) at Carnegie-Mellon University
 *	Made mmap work for character devices to support (sun) frame
 *	buffers.
 */

#import <mach_xp.h>
#import <mach_old_vm_copy.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_mman.c	7.1 (Berkeley) 6/5/86
 */

/*	@(#)kern_mman.c	2.2 88/06/17 4.0NFSSRC SMI */

#import <machine/reg.h>
#import <machine/psl.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/buf.h>
#import <sys/vnode.h>
#import <sys/acct.h>
#import <sys/wait.h>
#import <sys/file.h>
#import <sys/vadvise.h>
#import <sys/trace.h>
#import <sys/mman.h>
#import <sys/conf.h>
#import <sys/kern_return.h>
#import <kern/task.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <vm/vm_pager.h>
#import <vm/vnode_pager.h>
#if	MACH_XP
#import <vm/device_pager.h>
#endif	MACH_XP
#import <specfs/snode.h>
#if	NeXT
#import <kern/mfs.h>
#endif	NeXT

sbrk()
{

}

sstk()
{

}

getpagesize()
{

	u.u_r.r_val1 = NBPG * CLSIZE;
}

smmap()
{
	/*
	 *	Map in special device (must be SHARED) or file
	 */
	struct a {
		caddr_t	addr;
		int	len;
		int	prot;
		int	share;
		int	fd;
		off_t	pos;
	} *uap = (struct a *)u.u_ap;
	struct file *fp;
	register struct vnode *vp;
	vm_map_t	user_map;
	kern_return_t	result;
	vm_offset_t	user_addr;
	vm_size_t	user_size;
	off_t		file_pos;
	int		prot;

#if	MACH_XP
#else	MACH_XP
	extern vm_object_t	vm_object_special();
#endif	MACH_XP

	user_addr = (vm_offset_t) uap->addr;
	user_size = (vm_size_t) uap->len;
	file_pos  = uap->pos;
	prot      = uap->prot;

	/*
	 *	Only work with local inodes.  Do not use getinode - it will
	 *	try to invoke VICE or RFS code, which will panic on smmap.
	 */

	u.u_error = getvnodefp(uap->fd, &fp);
	if (u.u_error)
		return;
	if (fp->f_type != DTYPE_VNODE) {
		u.u_error = EINVAL;
		return;
	}
	vp = (struct vnode *)fp->f_data;

	/*
	 *	We bend a little - round the start and end addresses
	 *	to the nearest page boundary.
	 */
	user_addr = trunc_page(user_addr);
	user_size = round_page(user_size);

	/*
	 *	File can be COPIED at an arbitrary offset.
	 *	File can only be SHARED if the offset is at a
	 *	page boundary.
	 *
	if (uap->share == MAP_SHARED &&
	    ((vm_offset_t)file_pos & page_mask)) {
		u.u_error = EINVAL;
		return;
	}
	/*
	 *	File must be writable if memory will be.
	 */
	if ((prot & PROT_WRITE) && (fp->f_flag&FWRITE) == 0) {
		u.u_error = EINVAL;
		return;
	}
	if ((prot & PROT_READ) && (fp->f_flag&FREAD) == 0) {
		u.u_error = EINVAL;
		return;
	}
	/*
	 *	memory must exist and be writable (even if we're
	 *	just reading)
	 */
	user_map = current_task()->map;

	if (!vm_map_check_protection(user_map, user_addr,
				(vm_offset_t)(user_addr + user_size),
				VM_PROT_READ|VM_PROT_WRITE)) {
		u.u_error = EINVAL;
		return;
	}

	if (vp->v_type == VCHR || vp->v_type == VSTR) {
		/*
		 *	Map in special device memory.
		 *	Must be shared, since we can't assume that
		 *	this memory can be made copy-on-write.
		 */
		int		off;
		dev_t		dev;
#if	MACH_XP
		vm_pager_t	pager;
#else	MACH_XP
		vm_object_t	object;
#endif	MACH_XP
		int		(*mapfun)();
		extern int	nodev();
		extern int	nulldev();

		dev = VTOS(vp)->s_dev;
		mapfun = cdevsw[major(dev)].d_mmap;
		if (mapfun == nulldev || mapfun == nodev || mapfun == 0) {
			u.u_error = EINVAL;
			return;
		}

		for (off=0; off<uap->len; off += NBPG) {
			if ((*mapfun)(dev, file_pos+off, prot) == -1) {
				u.u_error = EINVAL;
				return;
			}
		}

		if (uap->share != MAP_SHARED) {
			u.u_error = EINVAL;
			return;
		}
	
		/*
		 *	Deallocate the existing memory, allocate new memory
		 *	to fill the space, then map the device memory into
		 *	it.
		 */
		result = vm_deallocate(user_map, user_addr, user_size);
		if (result != KERN_SUCCESS) {
			u.u_error = EINVAL;
			return;
		}

#if	MACH_XP
		pager = device_pager_create(dev, mapfun, prot,
				(vm_offset_t) file_pos, user_size);
		if (pager == PORT_NULL) {
			u.u_error = EINVAL;
			return;
		}
		(void) vm_allocate_with_pager(user_map, &user_addr, user_size,
					FALSE, pager, 0);
		port_release(pager);
#else	MACH_XP
		/*
		 *	Create an object with the device's pages attached
		 */
		object = vm_object_special(dev, mapfun, prot,
			(vm_offset_t)file_pos, user_size);

		/*
		 *	Grab some memory, and attach the object to it.
		 */
		result = vm_map_find(user_map, object, (vm_offset_t) 0,
					&user_addr, user_size, FALSE);
		if (result != KERN_SUCCESS) {
			vm_object_deallocate(object);
			u.u_error = EINVAL;
			return;
		}
#endif	MACH_XP
	}
	else {
		/*
		 *	Map in a file.  May be PRIVATE (copy-on-write)
		 *	or SHARED (changes go back to file)
		 */
		vm_pager_t	pager;
		vm_map_t	copy_map;
		vm_offset_t	off;
#if	NeXT
		struct vm_info	*vmp;
#endif	NeXT

		/*
		 *	Only allow regular files for the moment.
		 */
		if (vp->v_type != VREG) {
			u.u_error = EINVAL;
			return;
		}
		
		pager = vnode_pager_setup(vp, FALSE, FALSE);
		
#if	NeXT
		/*
		 *  Set credentials:
		 *	FIXME: if we're writing the file we need a way to
		 *      ensure that someone doesn't replace our R/W creds
		 * 	with ones that only work for read.
		 */
		vmp = vp->vm_info;
		if (vmp->cred == NULL) {			
			crhold(u.u_cred);
			vmp->cred = u.u_cred;
		}
#endif	NeXT
		if (uap->share == MAP_SHARED) {
			/*
			 *	Map it directly, allowing modifications
			 *	to go out to the inode.
			 */
			(void) vm_deallocate(user_map, user_addr, user_size);
			result = vm_allocate_with_pager(user_map,
					&user_addr, user_size, FALSE,
					pager,
					(vm_offset_t)file_pos);
			if (result != KERN_SUCCESS) {
				u.u_error = result;
				return;
			}
		}
		else {
			/*
			 *	Copy-on-write of file.  Map into private
			 *	map, then copy into our address space.
			 */
			copy_map = vm_map_create(pmap_create(user_size),
					0, user_size, TRUE);
			off = 0;
			result = vm_allocate_with_pager(copy_map,
					&off, user_size, FALSE,
					pager,
					(vm_offset_t)file_pos);
			if (result != KERN_SUCCESS) {
				vm_map_deallocate(copy_map);
				u.u_error = result;
				return;
			}
			result = vm_map_copy(user_map, copy_map,
					user_addr, user_size,
					0, FALSE, FALSE);
			if (result != KERN_SUCCESS) {
				vm_map_deallocate(copy_map);
				u.u_error = result;
				return;
			}
			vm_map_deallocate(copy_map);
		}
	}

	/*
	 *	Our memory defaults to read-write.  If it shouldn't
	 *	be readable, protect it.
	 */
	if ((prot & PROT_WRITE) == 0) {
		result = vm_protect(user_map, user_addr, user_size,
					FALSE, VM_PROT_READ);
		if (result != KERN_SUCCESS) {
			(void) vm_deallocate(user_map, user_addr, user_size);
			u.u_error = EINVAL;
			return;
		}
	}

	/*
	 *	Shared memory is also shared with children
	 */
	/*
	 *	HACK HACK HACK
	 *	Since this memory CAN'T be made copy-on-write, and since
	 *	its users fork, we must change its inheritance to SHARED.
	 *	HACK HACK HACK
	 */
	if (uap->share == MAP_SHARED) {
		result = vm_inherit(user_map, user_addr, user_size,
				VM_INHERIT_SHARE);
		if (result != KERN_SUCCESS) {
			(void) vm_deallocate(user_map, user_addr, user_size);
			u.u_error = EINVAL;
			return;
		}
	}

	u.u_pofile[uap->fd] |= UF_MAPPED;
}

mremap()
{

}

munmap()
{
	register struct a {
		caddr_t	addr;
		int	len;
	} *uap = (struct a *)u.u_ap;
	vm_offset_t	user_addr;
	vm_size_t	user_size;
	kern_return_t	result;

	user_addr = (vm_offset_t) uap->addr;
	user_size = (vm_size_t) uap->len;
	if ((user_addr & page_mask) ||
	    (user_size & page_mask)) {
		u.u_error = EINVAL;
		return;
	}
	result = vm_deallocate(current_task()->map, user_addr, user_size);
	if (result != KERN_SUCCESS) {
		u.u_error = EINVAL;
		return;
	}
}

munmapfd(fd)
{
#ifdef notdef
	register struct fpte *pte;
	register int i;

	for (i = 0; i < u.u_dsize; i++) {
		pte = (struct fpte *)dptopte(u.u_procp, i);
		if (pte->pg_v && pte->pg_fod && pte->pg_fileno == fd) {
			*(int *)pte = (PG_UW|PG_FOD);
			pte->pg_fileno = PG_FZERO;
		}
	}
#endif
	u.u_pofile[fd] &= ~UF_MAPPED;
	
}

mprotect()
{

}

madvise()
{

}

mincore()
{

}

/* BEGIN DEFUNCT */
obreak()
{
	struct a {
		char	*nsiz;
	};
	/*
	 *	This is kinda gross, we poke into the VM system
	 *	to implement this --- should be avoided entirely
	 *	by implementing in user mode.
	 */
	vm_offset_t	old, new;
	vm_map_t	map;
	vm_map_entry_t	entry;
	kern_return_t	ret;

	new = round_page(((struct a *)u.u_ap)->nsiz);
	if ((int)new > u.u_rlimit[RLIMIT_DATA].rlim_cur) {
		u.u_error = ENOMEM;
		return;
	}
	map = current_task()->map;
	vm_map_lock(map);
	/*
	 *	If already in map, do nothing else fill in region.
	 */
	if (!vm_map_lookup_entry(map, new, &entry)) {
#if	MACH_OLD_VM_COPY
		old = entry->end;
#else	MACH_OLD_VM_COPY
		old = entry->vme_end;
#endif	MACH_OLD_VM_COPY
		vm_map_unlock(map);
		ret = vm_allocate(map, &old, new - old, FALSE);
		if (ret != KERN_SUCCESS)
			uprintf("could not sbrk, return = %d\n", ret);
	}
	else {
		vm_map_unlock(map);
	}
}

int	both;

ovadvise()
{

#ifdef lint
	both = 0;
#endif
}
/* END DEFUNCT */



