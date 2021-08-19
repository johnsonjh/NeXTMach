/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	mfs_prim.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1987, Avadis Tevanian, Jr.
 *
 *	Support for mapped file system implementation.
 *
 * HISTORY
 *  3-Aug-90  Doug Mitchell at NeXT
 *	Added primitives for loadable file system support.
 *
 *  7-Mar-90  Brian Pinkerton (bpinker) at NeXT
 *	Changed mfs_trunc to return an indication of change.
 *
 *  9-Mar-88  John Seamons (jks) at NeXT
 *	SUN_VFS: allocate vm_info structures from a zone.
 *
 * 29-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Corrected calls to inode_pager_setup and kmem_alloc.
 *
 * 15-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 18-Jun-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Make most of this file dependent on MACH_NBC.
 *
 * 30-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */

#import <mach_nbc.h>

#import <kern/lock.h>
#import <kern/mfs.h>
#import <kern/sched_prim.h>
#import <kern/assert.h>

#import <sys/param.h>		/* all */
#import <sys/systm.h>		/* for */
#import <sys/mount.h>		/* vnode.h */
#import <sys/dir.h>		/* Sure */
#import <sys/user.h>		/* is */
#import <sys/vnode.h>

#import <vm/vm_kern.h>
#import <vm/vm_pager.h>
#import <vm/vm_param.h>

#import <kern/xpr.h>

/*
 *	Private variables and macros.
 */

queue_head_t		vm_info_queue;		/* lru list of structures */
decl_simple_lock_data(,	vm_info_lock_data)	/* lock for lru list */
int			vm_info_version = 0;	/* version number */

#define	vm_info_lock()		simple_lock(&vm_info_lock_data)
#define	vm_info_unlock()	simple_unlock(&vm_info_lock_data)

#if	MACH_NBC
lock_data_t		mfs_alloc_lock_data;
boolean_t		mfs_alloc_wanted;
long			mfs_alloc_blocks = 0;

#define mfs_alloc_lock()	lock_write(&mfs_alloc_lock_data)
#define mfs_alloc_unlock()	lock_write_done(&mfs_alloc_lock_data)

vm_map_t	mfs_map;

/*
 *	mfs_map_size is the number of bytes of VM to use for file mapping.
 *	It should be set by machine dependent code (before the call to
 *	mfs_init) if the default is inappropriate.
 *
 *	mfs_max_window is the largest window size that will be given to
 *	a file mapping.  A default value is computed in mfs_init based on
 *	mfs_map_size.  This too may be set by machine dependent code
 *	if the default is not appropriate.
 *
 *	mfs_files_max is the maximum number of files that we will
 *	simultaneously leave mapped.  Note th memory for unmapped
 *	files will not necessarily leave the memory cache, but by
 *	unmapping these files the file system can throw away any
 *	file system related info (like vnodes).  Again, this value
 *	can be sent by machine dependent code if the default is not
 *	appropriate.
 */

vm_size_t	mfs_map_size = 8*1024*1024;	/* size in bytes */
vm_size_t	mfs_max_window = 0;		/* largest window to use */

int		mfs_files_max = 100;		/* maximum # of files mapped */
int		mfs_files_mapped = 0;		/* current # mapped */

#define CHUNK_SIZE	(64*1024)	/* XXX */
#endif	MACH_NBC

/*
 *	mfs_init:
 *
 *	Initialize the mfs module.
 */

mfs_init()
{
	register struct vm_info	*vmp;
	int			i;
#if	MACH_NBC
	int			min, max;
#endif	MACH_NBC

	queue_init(&vm_info_queue);
	simple_lock_init(&vm_info_lock_data);
#if	MACH_NBC
	lock_init(&mfs_alloc_lock_data, TRUE);
	mfs_alloc_wanted = FALSE;
	mfs_map = kmem_suballoc(kernel_map, &min, &max, mfs_map_size, TRUE);
	if (mfs_max_window == 0)
		mfs_max_window = mfs_map_size / 20;
	if (mfs_max_window < CHUNK_SIZE)
		mfs_max_window = CHUNK_SIZE;
#endif	MACH_NBC

	i = (vm_size_t) sizeof (struct vm_info);
	vm_info_zone = zinit (i, 10000*i, 8192, FALSE, "vm_info zone");
}

/*
 *	vm_info_init:
 *
 *	Initialize a vm_info structure for a vnode.
 */
vm_info_init(vp)
	struct vnode *vp;
{
	register struct vm_info	*vmp;

	vmp = vp->vm_info;
	if (vmp == VM_INFO_NULL)
		vmp = (struct vm_info *) zalloc(vm_info_zone);
	vmp->pager = vm_pager_null;
	vmp->map_count = 0;
	vmp->use_count = 0;
	vmp->va = 0;
	vmp->size = 0;
	vmp->offset = 0;
	vmp->cred = (struct ucred *) NULL;
	vmp->error = 0;
	vmp->queued = FALSE;
	vmp->dirty = FALSE;
	vmp->close_flush = TRUE;	/* for safety, reconsider later */
	vmp->mapped = FALSE;
	vmp->vnode_size = 0;
	lock_init(&vmp->lock, TRUE);	/* sleep lock */
	vmp->object = VM_OBJECT_NULL;
	vp->vm_info = vmp;
}

#if	MACH_NBC
vm_info_enqueue(vmp)
	struct vm_info	*vmp;
{
	assert(!vmp->queued);
	assert(vmp->mapped);
	queue_enter(&vm_info_queue, vmp, struct vm_info *, lru_links);
	vmp->queued = TRUE;
	mfs_files_mapped++;
	vm_info_version++;
}

vm_info_dequeue(vmp)
	struct vm_info	*vmp;
{
	assert(vmp->queued);
	queue_remove(&vm_info_queue, vmp, struct vm_info *, lru_links);
	vmp->queued = FALSE;
	mfs_files_mapped--;
	vm_info_version++;
}

/*
 *	map_vnode:
 *
 *	Indicate that the specified vnode should be mapped into VM.
 *	A reference count is maintained for each mapped file.
 */
map_vnode(vp)
	register struct vnode	*vp;
{
	register struct vm_info	*vmp;
	vm_pager_t	pager;
	extern lock_data_t	vm_alloc_lock;

	vmp = vp->vm_info;
	if (vmp->map_count++ > 0)
		return;		/* file already mapped */

	if (vmp->mapped)
		return;		/* file was still cached */

	vmp_get(vmp);

	pager = vmp->pager = (vm_pager_t) vnode_pager_setup(vp, FALSE, TRUE);
				/* not a TEXT file, can cache */
	/*
	 *	Lookup what object is actually holding this file's
	 *	pages so we can flush them when necessary.  This
	 *	would be done differently in an out-of-kernel implementation.
	 *
	 *	Note that the lookup keeps a reference to the object which
	 *	we must release elsewhere.
	 */
	lock_write(&vm_alloc_lock);
	vmp->object = vm_object_lookup(pager);
	vm_stat.lookups++;
	if (vmp->object == VM_OBJECT_NULL) {
		vmp->object = vm_object_allocate(0);
		vm_object_enter(vmp->object, pager);
		vm_object_setpager(vmp->object, pager, (vm_offset_t) 0, FALSE);
	}
	else {
		vm_stat.hits++;
	}
	lock_write_done(&vm_alloc_lock);

	vmp->error = 0;

	vmp->vnode_size = vnode_size(vp);	/* must be before setting
						   mapped below to prevent
						   mfs_fsync from recursive
						   locking */

	vmp->va = 0;
	vmp->size = 0;
	vmp->offset = 0;
	vmp->mapped = TRUE;

	/*
	 *	If the file is less than the maximum window size then
	 *	just map the whole file now.
	 */

	if (vmp->vnode_size > 0 && vmp->vnode_size < mfs_max_window)
		remap_vnode(vp, 0, vmp->vnode_size);

	vmp_put(vmp);	/* put will queue on LRU list */
}

int close_flush = 1;

/*
 *	unmap_vnode:
 *
 *	Called when an vnode is closed.
 */
unmap_vnode(vp)
	register struct vnode	*vp;
{
	register struct vm_info		*vmp;
	register struct vm_object	*object;
	int				links;

	vmp = vp->vm_info;
	if (!vmp->mapped)
		return;	/* not a mapped file */
	if (--vmp->map_count > 0) {
		return;
	}

	/*
	 *	If there are no links left to the file then release
	 *	the resources held.  If there are links left, then keep
	 *	the file mapped under the assumption that someone else
	 *	will soon map the same file.  However, the pages in
	 *	the object are deactivated to put them near the list
	 *	of pages to be reused by the VM system (this would
	 *	be done differently out of the kernel, of course, then
	 *	again, the primitives for this don't exist out of the
	 *	kernel yet.
	 */

	vmp->map_count++;
	VOP_NLINKS(vp, &links);	/* may uncache, see below */
	vmp->map_count--;
	if (links == 0) {
		mfs_memfree(vmp, FALSE);
	}
	else {
		/*
		 *	pushing the pages may cause an uncache
		 *	operation (thanks NFS), so gain an extra
		 *	reference to guarantee that the object
		 *	does not go away.  (Note that such an
		 *	uncache actually takes place since we have
		 *	already released the map_count above).
		 */
		object = vmp->object;
		if (close_flush || vmp->close_flush) {
			vmp->map_count++;	/* prevent uncache race */
			vmp_get(vmp);
			vmp_push(vmp);
		}
		vm_object_lock(object);
		vm_object_deactivate_pages(object);
		vm_object_unlock(object);
		if (close_flush || vmp->close_flush) {
			vmp_put(vmp);
			vmp->map_count--;
		}
	}
}

/*
 *	remap_vnode:
 *
 *	Remap the specified vnode (due to extension of the file perhaps).
 *	Upon return, it should be possible to access data in the file
 *	starting at the "start" address for "size" bytes.
 */
remap_vnode(vp, start, size)
	register struct vnode	*vp;
	vm_offset_t		start;
	register vm_size_t	size;
{
	register struct vm_info	*vmp;
	vm_offset_t		addr, offset;
	kern_return_t		ret;

	vmp = vp->vm_info;
	/*
	 *	Remove old mapping (making its space available).
	 */
	if (vmp->size > 0)
		mfs_map_remove(vmp, vmp->va, vmp->va + vmp->size, TRUE);

	offset = trunc_page(start);
	size = round_page(start + size) - offset;
	if (size < CHUNK_SIZE)
		size = CHUNK_SIZE;
	do {
		addr = vm_map_min(mfs_map);
		mfs_alloc_lock();
		ret = vm_allocate_with_pager(mfs_map, &addr, size, TRUE,
				vmp->pager, offset);
		/*
		 *	If there was no space, see if we can free up mappings
		 *	on the LRU list.  If not, just wait for someone else
		 *	to free their memory.
		 */
		if (ret == KERN_NO_SPACE) {
			register struct vm_info	*vmp1;

			vm_info_lock();
			vmp1 = VM_INFO_NULL;
			if (!queue_empty(&vm_info_queue)) {
				vmp1 = (struct vm_info *)
						queue_first(&vm_info_queue);
				vm_info_dequeue(vmp1);
			}
			vm_info_unlock();
			/*
			 *	If we found someone, free up its memory.
			 */
			if (vmp1 != VM_INFO_NULL) {
				mfs_alloc_unlock();
				mfs_memfree(vmp1, TRUE);
				mfs_alloc_lock();
			}
			else {
				mfs_alloc_wanted = TRUE;
				assert_wait(&mfs_map, FALSE);
				mfs_alloc_blocks++;	/* statistic only */
				mfs_alloc_unlock();
				thread_block();
				mfs_alloc_lock();
			}
		}
		else if (ret != KERN_SUCCESS) {
			printf("Unexpected error on file map, ret = %d.\n",
					ret);
			panic("remap_vnode");
		}
		mfs_alloc_unlock();
	} while (ret != KERN_SUCCESS);
	/*
	 *	Fill in variables corresponding to new mapping.
	 */
	vmp->va = addr;
	vmp->size = size;
	vmp->offset = offset;
	return(TRUE);
}

/*
 *	mfs_trunc:
 *
 *	The specified vnode is truncated to the specified size.
 *	Returns TRUE if anything was changed, FALSE otherwise.
 */
int
mfs_trunc(vp, length)
	register struct vnode	*vp;
	register int		length;
{
	register struct vm_info	*vmp;
	register vm_size_t	size, rsize;

	vmp = vp->vm_info;

	if (!vmp->mapped) {	/* file not mapped, just update size */
#ifdef	NeXT
		vmp->vnode_size = length;
#else	NeXT
		if (length < vmp->vnode_size) {
			vmp->vnode_size = length;
		}
#endif	NeXT
		return FALSE;
	}

	vmp_get(vmp);

#ifdef        notdef
	if (length > vmp->vnode_size) {
		vmp_put(vmp);
		return FALSE;
	}
#endif        notdef

	/*
	 *	Unmap everything past the new end page.
	 *	Also flush any pages that may be left in the object using
	 *	vno_flush (is this necessary?).
	 *	rsize is the size relative to the mapped offset.
	 */
	size = round_page(length);
	if (size >= vmp->offset) {
		rsize = size - vmp->offset;
	}
	else {
		rsize = 0;
	}
	if (rsize < vmp->size) {
		mfs_map_remove(vmp, vmp->va + rsize, vmp->va + vmp->size,
			       FALSE);
		vmp->size = rsize;		/* mapped size */
	}
	if (vmp->vnode_size > size)
		vno_flush(vp, size, vmp->vnode_size - size);
	vmp->vnode_size = length;	/* file size */
	/*
	 *	If the new length isn't page aligned, zero the extra
	 *	bytes in the last page.
	 */
	if (length != size) {
		vm_size_t	n;

		n = size - length;
		/*
		 * Make sure the bytes to be zeroed are mapped.
		 */
		if ((length < vmp->offset) ||
		   ((length + n) > (vmp->offset + vmp->size)))
			remap_vnode(vp, length, n);
		bzero(vmp->va + length - vmp->offset, n);
		/*
		 *	Do NOT set dirty flag... the cached memory copy
		 *	is zeroed, but this change doesn't need to be
		 *	flushed to disk (the vnode already has the right
		 *	size.  Besides, if we set this bit, we would need
		 *	to clean it immediately to prevent a later sync
		 *	operation from incorrectly cleaning a cached-only
		 *	copy of this vmp (which causes problems with NFS
		 *	due to the fact that we have changed the mod time
		 *	by truncating and will need to do an mfs_uncache).
		 *	NFS is a pain.  Note that this means that there
		 *	will be a dirty page left in the vmp.  If this
		 *	turns out to be a problem we'll have to set the dirty
		 *	flag and immediately do a flush.
		 *
		 *	UPDATE: 4/4/13.  We need to really flush this.
		 *	Use the map_count hack to prevent a race with
		 *	uncaching.
		 */
		vmp->map_count++;	/* prevent uncache race */
		vmp->dirty = TRUE;
		vmp_push(vmp);
		vmp->map_count--;
	}
	vmp_put(vmp);
	return TRUE;
}

/*
 *	mfs_get:
 *
 *	Get locked access to the specified file.  The start and size describe
 *	the address range that will be accessed in the near future and
 *	serves as a hint of where to map the file if it is not already
 *	mapped.  Upon return, it is guaranteed that there is enough VM
 *	available for remapping operations within that range (each window
 *	no larger than the chunk size).
 */
mfs_get(vp, start, size)
	register struct vnode	*vp;
	vm_offset_t		start;
	register vm_size_t	size;
{
	register struct vm_info	*vmp;

	vmp = vp->vm_info;

	vmp_get(vmp);

	/*
	 *	If the requested size is larger than the size we have
	 *	mapped, be sure we can get enough VM now.  This size
	 *	is bounded by the maximum window size.
	 */

	if (size > mfs_max_window)
		size = mfs_max_window;

	if (size > vmp->size) {
		remap_vnode(vp, start, size);
	}

}

/*
 *	mfs_put:
 *
 *	Indicate that locked access is no longer desired of a file.
 */
mfs_put(vp)
	register struct vnode	*vp;
{
	vmp_put(vp->vm_info);
}

/*
 *	vmp_get:
 *
 *	Get exclusive access to the specified vm_info structure.
 */
vmp_get(vmp)
	struct vm_info	*vmp;
{
	/*
	 *	Remove from LRU list (if its there).
	 */
	vm_info_lock();
	if (vmp->queued) {
		vm_info_dequeue(vmp);
	}
	vmp->use_count++;	/* to protect requeueing in vmp_put */
	vm_info_unlock();

	/*
	 *	Lock out others using this file.
	 */
	lock_write(&vmp->lock);
}

/*
 *	vmp_put:
 *
 *	Release exclusive access gained in vmp_get.
 */
vmp_put(vmp)
	register struct vm_info	*vmp;
{
	/*
	 *	Place back on LRU list if noone else using it.
	 */
	vm_info_lock();
	if (--vmp->use_count == 0) {
		vm_info_enqueue(vmp);
	}
	vm_info_unlock();
	/*
	 *	Let others at file.
	 */
	lock_write_done(&vmp->lock);
	if (mfs_files_mapped > mfs_files_max)
		mfs_cache_trim();

	if (vmp->invalidate) {
		vmp->invalidate = FALSE;
		vmp_invalidate(vmp);
	}
}

/*
 *	mfs_uncache:
 *
 *	Make sure there are no cached mappings for the specified vnode.
 */
void mfs_uncache(vp)
	register struct vnode	*vp;
{
	register struct vm_info	*vmp;

	vmp = vp->vm_info;
	/*
	 *	If the file is mapped but there is noone actively using
	 *	it then remove its mappings.
	 */
	if (vmp->mapped && vmp->map_count == 0) {
		mfs_memfree(vmp, FALSE);
	}
}

mfs_memfree(vmp, flush)
	register struct vm_info	*vmp;
	boolean_t		flush;
{
	struct ucred	*cred;
	vm_object_t	object;

	vm_info_lock();
	if (vmp->queued) {
		vm_info_dequeue(vmp);
	}
	vm_info_unlock();
	lock_write(&vmp->lock);
	if (vmp->map_count == 0) {	/* cached only */
		vmp->mapped = FALSE;	/* prevent recursive flushes */
	}
	mfs_map_remove(vmp, vmp->va, vmp->va + vmp->size, flush);
	vmp->size = 0;
	vmp->va = 0;
	object = VM_OBJECT_NULL;
	if (vmp->map_count == 0) {	/* cached only */
		/*
		 * lookup (in map_vnode) gained a reference, so need to
		 * lose it.
		 */
		object = vmp->object;
		vmp->object = VM_OBJECT_NULL;
		cred = vmp->cred;
		if (cred) {
			crfree(cred);
			vmp->cred = NULL;
		}
	}
	lock_write_done(&vmp->lock);
	if (object != VM_OBJECT_NULL)
		vm_object_deallocate(object);
}

/*
 *	mfs_cache_trim:
 *
 *	trim the number of files in the cache to be less than the max
 *	we want.
 */

mfs_cache_trim()
{
	register struct vm_info	*vmp;

	while (TRUE) {
		vm_info_lock();
		if (mfs_files_mapped <= mfs_files_max) {
			vm_info_unlock();
			return;
		}
		/*
		 * grab file at head of lru list.
		 */
		vmp = (struct vm_info *) queue_first(&vm_info_queue);
		vm_info_dequeue(vmp);
		vm_info_unlock();
		/*
		 *	Free up its memory.
		 */
		mfs_memfree(vmp, TRUE);
	}
}

/*
 *	mfs_cache_clear:
 *
 *	Clear the mapped file cache.  Note that the map_count is implicitly
 *	locked by the Unix file system code that calls this routine.
 */
mfs_cache_clear()
{
	register struct vm_info	*vmp;
	int			last_version;

	vm_info_lock();
	last_version = vm_info_version;
	vmp = (struct vm_info *) queue_first(&vm_info_queue);
	while (!queue_end(&vm_info_queue, (queue_entry_t) vmp)) {
		if (vmp->map_count == 0) {
			vm_info_unlock();
			mfs_memfree(vmp, TRUE);
			vm_info_lock();
			/*
			 * mfs_memfree increments version number, causing
			 * restart below.
			 */
		}
		/*
		 *	If the version didn't change, just keep scanning
		 *	down the queue.  If the version did change, we
		 *	need to restart from the beginning.
		 */
		if (last_version == vm_info_version) {
			vmp = (struct vm_info *) queue_next(&vmp->lru_links);
		}
		else {
			vmp = (struct vm_info *) queue_first(&vm_info_queue);
			last_version = vm_info_version;
		}
	}
	vm_info_unlock();
}

/*
 *	mfs_map_remove:
 *
 *	Remove specified address range from the mfs map and wake up anyone
 *	waiting for map space.  Be sure pages are flushed back to vnode.
 */

mfs_map_remove(vmp, start, end, flush)
	struct vm_info	*vmp;
	vm_offset_t	start;
	vm_size_t	end;
	boolean_t	flush;
{
	vm_object_t	object;

	/*
	 *	Note:	If we do need to flush, the vmp is already
	 *	locked at this point.
	 */
	if (flush) {
/*		vmp->map_count++;	/* prevent recursive flushes */
		vmp_push(vmp);
/*		vmp->map_count--;*/
	}

	/*
	 *	Free the address space.
	 */
	mfs_alloc_lock();
	vm_map_remove(mfs_map, start, end);
	if (mfs_alloc_wanted) {
		mfs_alloc_wanted = FALSE;
		thread_wakeup(&mfs_map);
	}
	mfs_alloc_unlock();
	/*
	 *	Deactivate the pages.
	 */
	object = vmp->object;
	if (object != VM_OBJECT_NULL) {
		vm_object_lock(object);
		vm_object_deactivate_pages(object);
		vm_object_unlock(object);
	}
}

vnode_size(vp)
	struct vnode	*vp;
{
	struct vattr		vattr;

	VOP_GETATTR(vp, &vattr, u.u_cred);
	return(vattr.va_size);
}

#import <sys/uio.h>
#import <sys/vfs.h>

boolean_t mfs_io(vp, uio, rw, ioflag, cred)
	register struct vnode	*vp;
	register struct uio	*uio;
	enum uio_rw		rw;
	int			ioflag;
	struct ucred		*cred;
{
	register vm_offset_t	va;
	register struct vm_info	*vmp;
	register int		n, diff, bsize;
	off_t			vsize;
	int			error;
	off_t			base;
	int			bcount;

	XPR(XPR_VM_OBJECT, ("mfs_io(%c): vp 0x%x, offset %d, size %d\n",
		rw == UIO_READ ? 'R' : 'W',
		vp, uio->uio_offset, uio->uio_resid));

	if (uio->uio_resid == 0) {
		return (0);
	}

	if ((int) uio->uio_offset < 0 ||
	    (int) (uio->uio_offset + uio->uio_resid) < 0) {
		return (EINVAL);
	}
	
	ASSERT(vp->v_type==VDIR || vp->v_type==VREG || vp->v_type==VLNK);

	mfs_get(vp, uio->uio_offset, uio->uio_resid);

	vmp = vp->vm_info;
	vsize = vmp->vnode_size;	/* was vnode_size(vp) */

	if ((rw == UIO_WRITE) && (ioflag & IO_APPEND)) {
		uio->uio_offset = vsize;
	}

	base = uio->uio_offset;
	bcount = uio->uio_resid;
	bsize = vp->v_vfsp->vfs_bsize;

	/*
	 *	Set credentials.
	 */
	if (rw == UIO_WRITE || (rw == UIO_READ && vmp->cred == NULL)) {
		crhold(cred);
		if (vmp->cred) {
			crfree(vmp->cred);
		}
		vmp->cred = cred;
	}

	do {
		n = MIN((unsigned)bsize, uio->uio_resid);
		if (rw == UIO_READ) {
			diff = vsize - uio->uio_offset;
			if (diff <= 0) {
				mfs_put(vp);
				return (0);
				}
			if (diff < n)
				n = diff;
		}

		if ((rw==UIO_WRITE) && (uio->uio_offset+n > vmp->vnode_size))
			vmp->vnode_size = uio->uio_offset + n;

		/*
		 *	Check to be sure we have a valid window
		 *	for the mapped file.
		 */
		if ((uio->uio_offset < vmp->offset) ||
		   ((uio->uio_offset + n) > (vmp->offset + vmp->size)))
			remap_vnode(vp, uio->uio_offset, n);

		va = vmp->va + uio->uio_offset - vmp->offset;
		XPR(XPR_VM_OBJECT, ("uiomove: va = 0x%x, n = %d.\n", va, n));
		error = uiomove(va, n, rw, uio);
		/*
		 *	Set dirty bit each time through loop just in
		 *	case remap above caused it to be cleared.
		 */
		if (rw == UIO_WRITE)
			vmp->dirty = TRUE;
		/*
		 *	Check for errors left by the pager.  Report the
		 *	error only once.
		 */
		if (vmp->error) {
			error = vmp->error;
			vmp->error = 0;
			/*
			 * The error might have been a permission
			 * error based on the credential.  We release it
			 * so that the next person who tries a read doesn't
			 * get stuck with it.
			 */
			crfree(vmp->cred);
			vmp->cred = NULL;
		}
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	if ((error == 0) && (rw == UIO_WRITE) && (ioflag & IO_SYNC)) {
		off_t offset;
		
		vmp_push(vmp);	/* initiate all i/o */
		
		/* wait for i/o to complete */
		for (offset = base; offset < base + bcount; offset += bsize)
			blkflush(vp, btodb(offset), bsize);

		if (vmp->error) {
			error = vmp->error;
			vmp->error = 0;
		}
	}
	mfs_put(vp);
	return(error);
}

/*
 *	mfs_sync:
 *
 *	Sync the mfs cache (called by sync()).
 */
mfs_sync()
{
	register struct vm_info	*vmp, *next;
	int			last_version;

	vm_info_lock();
	last_version = vm_info_version;
	vmp = (struct vm_info *) queue_first(&vm_info_queue);
	while (!queue_end(&vm_info_queue, (queue_entry_t) vmp)) {
		next = (struct vm_info *) queue_next(&vmp->lru_links);
		if (vmp->dirty) {
			vm_info_unlock();
			vmp_get(vmp);
			vmp_push(vmp);
			vmp_put(vmp);
			vm_info_lock();
			/*
			 *	Since we unlocked, the get and put
			 *	operations would increment version by
			 *	two, so add two to our version.
			 *	If anything else happened in the meantime,
			 *	version numbers will not match and we
			 *	will restart.
			 */
			last_version += 2;
		}
		/*
		 *	If the version didn't change, just keep scanning
		 *	down the queue.  If the version did change, we
		 *	need to restart from the beginning.
		 */
		if (last_version == vm_info_version) {
			vmp = next;
		}
		else {
			vmp = (struct vm_info *) queue_first(&vm_info_queue);
			last_version = vm_info_version;
		}
	}
	vm_info_unlock();
}

/*
 *	Sync pages in specified vnode.
 */
mfs_fsync(vp)
	struct vnode	*vp;
{
	struct vm_info	*vmp;

	vmp = vp->vm_info;
	if ((vmp != VM_INFO_NULL) && (vmp->mapped)) {
		vmp_get(vmp);
		vmp_push(vmp);
		vmp_put(vmp);
	}
	return(vmp->error);
}

#ifdef	NeXT
/*
 *	Invalidate pages in specified vnode.
 */
mfs_invalidate(vp)
	struct vnode	*vp;
{
	struct vm_info	*vmp;

	vmp = vp->vm_info;
	if ((vmp != VM_INFO_NULL) && (vmp->mapped)) {
		if (vmp->use_count > 0)
			vmp->invalidate = TRUE;
		else {
			vmp_get(vmp);
			vmp_invalidate(vmp);
			vmp_put(vmp);
		}
	}
	return(vmp->error);
}
#endif	NeXT

#import <vm/vm_page.h>
#import <vm/vm_object.h>

/*
 *	Search for and flush pages in the specified range.  For now, it is
 *	unnecessary to flush to disk since I do that synchronously.
 */
vno_flush(vp, start, size)
	struct vnode		*vp;
	register vm_offset_t	start;
	vm_size_t		size;
{
	register vm_offset_t	end;
	register vm_object_t	object;
	register vm_page_t	m;

	object = vp->vm_info->object;
	if (object == VM_OBJECT_NULL)
		return;

	vm_page_lock_queues();
	vm_object_lock(object);	/* mfs code holds reference */
	end = round_page(size + start);	/* must be first */
	start = trunc_page(start);
	while (start < end) {
		m = vm_page_lookup(object, start);
		if (m != VM_PAGE_NULL) {
			if (m->busy) {
				PAGE_ASSERT_WAIT(m, FALSE);
				vm_object_unlock(object);
				vm_page_unlock_queues();
				thread_block();
				vm_page_lock_queues();
				vm_object_lock(object);
				continue;	/* try again */
			}
			vm_page_free(m);
		}
		start += PAGE_SIZE;
	}
	vm_object_unlock(object);
	vm_page_unlock_queues();
}


#ifdef	NeXT
/*
 *	Search for and free pages in the specified vmp.
 */
vmp_invalidate(struct vm_info *vmp)
{
	register vm_offset_t	end;
	register vm_object_t	object;
	register vm_page_t	m;

	object = vmp->object;
	if (object == VM_OBJECT_NULL)
		return;

	vm_page_lock_queues();
	vm_object_lock(object);	/* mfs code holds reference */
	m = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) m)) {
		vm_page_t next = (vm_page_t) queue_next(&m->listq);
		if (m->busy) {
			PAGE_ASSERT_WAIT(m, FALSE);
			vm_object_unlock(object);
			vm_page_unlock_queues();
			thread_block();
			vm_page_lock_queues();
			vm_object_lock(object);
			continue;	/* try again */
		}
		vm_page_free(m);
		m = next;
	}
	vm_object_unlock(object);
	vm_page_unlock_queues();
}
#endif	NeXT


/*
 *	Search for and push (to disk) pages in the specified range.
 *	We need some better interactions with the VM system to simplify
 *	the code.
 */
vmp_push(vmp/*, start, size*/)
	struct vm_info		*vmp;
/*	register vm_offset_t	start;
	vm_size_t		size;*/
{
	register vm_offset_t	start;
	vm_size_t		size;
	register vm_offset_t	end;
	register vm_object_t	object;
	register vm_page_t	m;

	if (!vmp->dirty)
		return;
	vmp->dirty = FALSE;

	start = vmp->offset;
	size = vmp->size;

	object = vmp->object;
	if (object == VM_OBJECT_NULL)
		return;

	vm_page_lock_queues();
	vm_object_lock(object);	/* mfs code holds reference */
	end = round_page(size + start);	/* must be first */
	start = trunc_page(start);
	while (start < end) {
		m = vm_page_lookup(object, start);
		if (m != VM_PAGE_NULL) {
			if (m->busy) {
				PAGE_ASSERT_WAIT(m, FALSE);
				vm_object_unlock(object);
				vm_page_unlock_queues();
				thread_block();
				vm_page_lock_queues();
				vm_object_lock(object);
				continue;	/* try again */
			}
			if (!m->active) {
				vm_page_activate(m); /* so deactivate works */
			}
			vm_page_deactivate(m);	/* gets dirty/laundry bit */
			/*
			 *	Prevent pageout from playing with
			 *	this page.  We know it is inactive right
			 *	now (and are holding lots of locks keeping
			 *	it there).
			 */
			queue_remove(&vm_page_queue_inactive, m, vm_page_t,
				     pageq);
			m->inactive = FALSE;
			vm_page_inactive_count--;
			m->busy = TRUE;
			if (m->laundry) {
				pager_return_t	ret;

				pmap_remove_all(VM_PAGE_TO_PHYS(m));
				object->paging_in_progress++;
				vm_object_unlock(object);
				vm_page_unlock_queues();
				/* should call pageout daemon code */
				ret = vnode_pageout(m);
				vm_page_lock_queues();
				vm_object_lock(object);
				object->paging_in_progress--;
				if (ret == PAGER_SUCCESS) {
					m->laundry = FALSE;
				}
				else {
					/* don't set dirty bit, unrecoverable
					   errors will cause update to go
					   crazy.  User is responsible for
					   retrying the write */
					/* vmp->dirty = TRUE; */
				}
				/* if pager failed, activate below */
			}
			vm_page_activate(m);
			m->busy = FALSE;
			PAGE_WAKEUP(m);
		}
		start += PAGE_SIZE;
	}
	vm_object_unlock(object);
	vm_page_unlock_queues();
}

/*
 * Loadable file system support to avoid exporting struct vm_info.
 */
void vm_info_free(struct vnode *vp)
{
	mfs_uncache(vp);
	zfree(vm_info_zone, (vm_offset_t)vp->vm_info);
}

vm_size_t vm_get_vnode_size(struct vnode *vp) 
{
	return(vp->vm_info->vnode_size);
}

void vm_set_vnode_size(struct vnode *vp, vm_size_t vnode_size) 
{
	vp->vm_info->vnode_size = vnode_size;
}

void vm_set_close_flush(struct vnode *vp, boolean_t close_flush)
{
	vp->vm_info->close_flush = close_flush ? 1 : 0;
}

void vm_set_error(struct vnode *vp, int error)
{
	vp->vm_info->error = error;
}
#endif	MACH_NBC
