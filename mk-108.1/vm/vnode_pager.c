/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 *	File:	vnode_pager.c
 *
 *	"Swap" pager that pages to/from vnodes.  Also
 *	handles demand paging from files.
 *
 * HISTORY
 * 13-Apr-90	Brian Pinkerton at NeXT
 *	Split structure definitions out to vnode_pager.h.
 *
 * 25-Mar-90	Brian Pinkerton at NeXT
 *	Allow single objects to page to multiple swap files.
 *
 * 20-Mar-89	Peter King (king) at NeXT
 *	Put locking around allocpage and deallocpage routines now that
 *	deallocpage can sleep.
 *
 *  8-Mar-89	Peter King (king) at NeXT
 *	Implemented basic low-water/high-water mark on pager file size.
 *
 *  7-Dec-88	Peter King (king) at NeXT
 *	Clean out anonymous inode support including the vstruct queue.
 *
 * 18-Sep-88	Peter King (king) at NeXT
 *	Moved [ir]node_read and [ir]node_write routines to VOP_PAGEIN and
 *		VOP_PAGEOUT
 *	Cleaned up pager file offset mapping.
 *
 * 12-Mar-88  John Seamons (jks) at NeXT
 *	NeXT: added event meter instrumentation.
 *
 * 23-Feb-88  Peter King (king) at NeXT
 *	Converted from inode_pager.c.  Added support for NFS rnodes
 *	and paging files.
 *
 * 18-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Delinted.
 *
 *  6-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Use pager_data_unavailable when inode_read fails in inode_pagein!
 *
 *  6-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Move pager_cache() call and text bit handling inside
 *	inode_pager_setup().
 *
 * 24-Nov-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Make all external calls other than inode manipulations go
 *	through IPC.
 *
 *	Condensed history:
 *		Try to avoid deadlock when allocating data structures
 *		 (avie, mwyoung).
 *		Try to printf rather than panic when faced with errors (avie).
 *		"No buffer code" enhancements. (avie)
 *		External paging version. (mwyoung, bolosky)
 *		Allow pageout to ask whether a page has been
 *		 written out.  (dbg)
 *		Keep only a pool of in-core inodes.  (avie)
 *		Use readahead when able. (avie)
 *		Require that inode operations occur on the master
 *		 processor (avie, rvb, dbg).
 *		Combine both "program text" and "normal file" handling
 *		 into one. (avie, mwyoung)
 *		Allocate paging inodes on mounted filesystems (mja);
 *		 allow preferences to be supplied (mwyoung).
 *
 * 23-Sep-87  Peter King (king) at NeXT
 *	SUN_VFS: Interim support for vnodes.
 *
 * 12-Mar-86  David Golub (dbg) at Carnegie-Mellon University
 *	Created.
 */

#import <mach_nbc.h>

#import <sys/boolean.h>
#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <kern/lock.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/uio.h>
#import <sys/vnode.h>
#import <sys/vfs.h>
#import <kern/mach_swapon.h>
#import <sys/pathname.h>
#import <ufs/fs.h>
#import <ufs/inode.h>
#import <ufs/mount.h>
#import <netinet/in.h>
#import <rpc/types.h>
#import <nfs/nfs.h>
#undef	fs_fsok
#undef	fs_tsize
#undef	fs_bsize
#undef	fs_blocks
#undef	fs_bfree
#undef	fs_bavail
#import <nfs/nfs_clnt.h>
#import <nfs/rnode.h>

#import <kern/mach_types.h>
#import <vm/vm_page.h>
#import <vm/vm_map.h>
#import <vm/vm_kern.h>
#import <kern/parallel.h>
#import <kern/zalloc.h>
#import <kern/kalloc.h>

#import <vm/vnode_pager.h>
#import <kern/mfs.h>

#import <sys/proc.h>			/* PMON */
#import <kern/task.h>			/* PMON */
#import <next/kernel_pmon.h>		/* PMON */

#if	NeXT
#import <next/event_meter.h>
#endif	NeXT

#import <kern/xpr.h>

extern struct vnodeops nfs_vnodeops;
extern struct vnodeops spec_vnodeops;

#if	NBBY == 8
#define BYTEMASK 0xff
#else	NBBY
Define a byte mask for this machine.
#endif	NBBY


#define PAGEMAP_THRESHOLD	64 /* Integral of sizeof(vm_offset_t) */
#define	PAGEMAP_ENTRIES		(PAGEMAP_THRESHOLD/sizeof(vm_offset_t))
#define	PAGEMAP_SIZE(npgs)	(npgs*sizeof(long))

#define	INDIRECT_PAGEMAP_ENTRIES(npgs) \
	(((npgs-1)/PAGEMAP_ENTRIES) + 1)
#define INDIRECT_PAGEMAP_SIZE(npgs) \
	(INDIRECT_PAGEMAP_ENTRIES(npgs) * sizeof(caddr_t))
#define INDIRECT_PAGEMAP(size) \
	(PAGEMAP_SIZE(size) > PAGEMAP_THRESHOLD)

#define RMAPSIZE(blocks) \
	(howmany(blocks,NBBY))

/*
 *	Sigh... with NFS/vnodes it is highly likely that we will need
 *	to allocate memory at page-out time, so use the XP hack to reserve
 *	pages and always use kalloc/zalloc instead of kget/zget.
 *	This must be fixed!!!  FIXME - XXX.
 */

#define kget	kalloc
#define zget	zalloc

#if	MACH_NBC
extern int	nbc_debug;
#endif	MACH_NBC

/*
 *	Basic vnode pager data structures
 */

zone_t			vstruct_zone;
simple_lock_data_t	vstruct_lock;

queue_head_t		pager_files;
int			pager_file_count;
pager_file_t		pager_file_index[MAXPAGERFILES];


/*
 *	Routine:	vnode_pager_vput
 *	Function:
 *		Release one use of this vnode_pager_t
 */
void
vnode_pager_vput(vs)
	register vnode_pager_t	vs;
{

	simple_lock(&vstruct_lock);
	vs->vs_count--;
	simple_unlock(&vstruct_lock);
}

/*
 *	vnode_pager_vget:
 *
 *	Return a vnode corresponding to the specified paging space
 *	and guarantee that it will remain in memory (until furthur action
 *	is taken).
 *
 *	The vnode is returned unlocked.
 */
struct vnode *
vnode_pager_vget(vs)
	vnode_pager_t	vs;
{
	register struct vnode *vp;

	simple_lock(&vstruct_lock);
	vs->vs_count++;
	simple_unlock(&vstruct_lock);
	vp = vs->vs_vp;
	return(vp);
}


/*
 * vnode_pager_allocpage - allocate a page in a paging file
 */
daddr_t
vnode_pager_allocpage(pf)
     register struct pager_file *pf;
{
	int bp;			/* byte counter */
	int i;			/* bit counter */
	daddr_t page;		/* page number */

	lock_write(&pf->pf_lock);

	if (pf->pf_pfree == 0) {
		lock_done(&pf->pf_lock);
		return(-1);
	}

	for (bp = 0; bp < howmany(pf->pf_npgs, NBBY); bp++) {
		if (*(pf->pf_bmap + bp) != BYTEMASK) {
			for (i = 0; i < NBBY; i++) {
				if (isclr((pf->pf_bmap + bp), i))
					break;
			}
			break;
		}
	}
	page = bp*NBBY+i;
	if (page >= pf->pf_npgs) {
		panic("vnode_pager_allocpage");
	}
	if (page > pf->pf_hipage) {
		pf->pf_hipage = page;
	}
	setbit(pf->pf_bmap,page);
	if (--pf->pf_pfree == 0)
		printf("vnode_pager: %s is full.\n", pf->pf_name);

	lock_done(&pf->pf_lock);
	return(page);
}


/*
 * vnode_pager_findpage - find an available page in some paging file, using the
 * argument as a preference.  If the pager_file argument is NULL, any file will
 * do.  Return the designated page and file in mapEntry.
 */
kern_return_t
vnode_pager_findpage(struct pager_file *preferPf, pfMapEntry *mapEntry)
{
	daddr_t result;
	pager_file_t pf;
	
	if (preferPf == PAGER_FILE_NULL)
		preferPf = (pager_file_t) queue_first(&pager_files);
	
	if (preferPf == PAGER_FILE_NULL)
		return KERN_FAILURE;
		
	pf = preferPf;		     
	do {
		result = vnode_pager_allocpage(pf);
		if (result != -1) {
			mapEntry->pagerFileIndex = pf->pf_index;
			mapEntry->pageOffset = result;
			return KERN_SUCCESS;
		}
		
		if (queue_end(&pager_files, &pf->pf_chain))
			pf = (pager_file_t) queue_first(&pager_files);
		else
			pf = (pager_file_t) queue_next(&pf->pf_chain);
			
	} while (preferPf != pf);
	
	return KERN_FAILURE;
}


vnode_pager_deallocpage(mapEntry)
	pfMapEntry *mapEntry;
{
	register struct pager_file *pf;
	daddr_t		page;
	int		i;
	struct vnode	*vp;
	struct vattr	vattr;
	int		error;
	struct ucred	*tcred;
	long		truncpage;

	assert(mapEntry->pagerFileIndex <= pager_file_count);
	
	pf = pager_file_index[mapEntry->pagerFileIndex];
	vp = pf->pf_vp;
	page = mapEntry->pageOffset;
	
	lock_write(&pf->pf_lock);

	if (page >= (daddr_t) pf->pf_npgs)
		panic("vnode_pager_deallocpage");
	clrbit(pf->pf_bmap, page);
	if (++pf->pf_pfree == 1) {
		printf("vnode_pager: %s is available again.\n", pf->pf_name);
	}
	if (page == pf->pf_hipage) {
		/*
		 * Find a new high page
		 */
		for (i = page - 1; i >= 0; i--) {
			if (isset(pf->pf_bmap, i)) {
				pf->pf_hipage = i;
				break;
			}
		}
		truncpage = pf->pf_hipage + 1;

		/*
		 * If we are higher than the low water mark, truncate
		 * the file.
 		 */
		if (pf->pf_lowat &&
		    truncpage >= pf->pf_lowat &&
		    vp->vm_info->vnode_size > ptoa(truncpage)) {
			vattr_null(&vattr);
			vattr.va_size = ptoa(truncpage);
			ASSERT( (int) vattr.va_size >= 0 );
			/*
			 * ufs uses u.u_cred for a lot of credentials checking.
			 */
			tcred = u.u_cred;
			u.u_cred = vp->vm_info->cred;
			if (error = VOP_SETATTR(vp, &vattr, vp->vm_info->cred)) {
				printf("vnode_deallocpage: error truncating %s,"
					" error = %d\n", pf->pf_name, error);
			}
			u.u_cred = tcred;
		}
	}
	lock_done(&pf->pf_lock);
}


/*
 *	pagerfile_pager_create
 *
 *	Create an vstruct corresponding to the given pagerfile.
 *
 */
vnode_pager_t
pagerfile_pager_create(pf, size)
	register pager_file_t	pf;
	vm_size_t		size;
{
	register vnode_pager_t	vs;
	register int i;

	/*
	 *	XXX This can still livelock -- if the
	 *	pageout daemon needs an vnode_pager record
	 *	it won't get one until someone else
	 *	refills the zone.
	 */

	vs = (struct vstruct *) zget(vstruct_zone);

	if (vs == VNODE_PAGER_NULL)
		return(vs);

	vs->vs_size = atop(round_page(size));
	
	if (vs->vs_size == 0)
		vs->vs_pmap = (pfMapEntry **) 0;
	else {
		if (INDIRECT_PAGEMAP(vs->vs_size)) {
			vs->vs_pmap = (pfMapEntry **)
				kget(INDIRECT_PAGEMAP_SIZE(vs->vs_size));
		} else {
			vs->vs_pmap = (pfMapEntry **)
				kget(PAGEMAP_SIZE(vs->vs_size));
		}
		if (vs->vs_pmap == (pfMapEntry **) 0) {
			/*
			 * We can't sleep here, so if there are no free pages, then
			 * just return nothing.
			 */
			zfree(vstruct_zone, (vm_offset_t) vs);
			return(VNODE_PAGER_NULL);
		}
	
		if (INDIRECT_PAGEMAP(vs->vs_size)) {
			bzero((caddr_t)vs->vs_pmap,
				INDIRECT_PAGEMAP_SIZE(vs->vs_size));
		} else {
			for (i = 0; i < vs->vs_size; i++)
				((pfMapEntry *) &vs->vs_pmap[i])->pagerFileIndex =
								(int) PAGER_FILE_NULL;
		}
	}
	
	vs->is_device = FALSE;
	vs->vs_count = 1;
	vs->vs_vp = pf->pf_vp;
	vs->vs_swapfile = TRUE;
	vs->vs_pf = pf;
	pf->pf_count++;

	vnode_pager_vput(vs);

	return(vs);
}

/*
 *	pagerfile_bmap
 *
 *	Fill in the map entry (pager file, offset) for a given f_offset into an
 *	object backed this pager map.
 *
 *	Returns: KERN_FAILURE if page not in map or no room left
 */
kern_return_t
pagerfile_bmap(vs, f_offset, flag, mapEntry)
	struct vstruct *vs;
	vm_offset_t f_offset;
	int flag;
	pfMapEntry *mapEntry;
{
    vm_offset_t f_page;
    register int i;
    vm_offset_t newpage;
    int result;

    f_page = atop(f_offset);

    /*
     * If the object has grown, expand the page map.
     */
    if (f_page + 1 > vs->vs_size) {
	pfMapEntry	**new_pmap;
	int		new_size;

	new_size = f_page + 1;
	assert(new_size > 0);
	
	if (INDIRECT_PAGEMAP(new_size)) {	/* new map is indirect */

	    if (vs->vs_size == 0) {
		/*
		 * Nothing to copy, just get a new
		 * map and zero it.
		 */
		new_pmap = (pfMapEntry **) kget(INDIRECT_PAGEMAP_SIZE(new_size));
		if (new_pmap == NULL)
			return (KERN_FAILURE);
		bzero((caddr_t)new_pmap, INDIRECT_PAGEMAP_SIZE(new_size));	    	
	    }
	    else if (INDIRECT_PAGEMAP(vs->vs_size)) {
	    
		if (INDIRECT_PAGEMAP_SIZE(new_size) == 
		    INDIRECT_PAGEMAP_SIZE(vs->vs_size)) {
			goto leavemapalone;
		}
		
		/* Get a new indirect map */
		new_pmap = (pfMapEntry **) kget(INDIRECT_PAGEMAP_SIZE(new_size));
		if (new_pmap == NULL)
			return KERN_FAILURE;

		bzero((caddr_t)new_pmap, INDIRECT_PAGEMAP_SIZE(new_size));

		/* Old map is indirect, copy the entries */
		for (i = 0; i < INDIRECT_PAGEMAP_ENTRIES(vs->vs_size); i++)
			new_pmap[i] = vs->vs_pmap[i];
			
		/* And free the old map */
		kfree(vs->vs_pmap, INDIRECT_PAGEMAP_SIZE(vs->vs_size));
				
	    } else {		/* old map was direct, new map is indirect */
	    
		/* Get a new indirect map */
		new_pmap = (pfMapEntry **) kget(INDIRECT_PAGEMAP_SIZE(new_size));
		if (new_pmap == NULL)
			return KERN_FAILURE;

		bzero((caddr_t)new_pmap, INDIRECT_PAGEMAP_SIZE(new_size));

		/*
		 * Old map is direct, move it to the first indirect block.
		 */
		new_pmap[0] = (pfMapEntry *) kget(PAGEMAP_THRESHOLD);
		if (new_pmap[0] == NULL) {
			kfree(new_pmap, INDIRECT_PAGEMAP_SIZE(new_size));
			return KERN_FAILURE;
		}
		for (i = 0; i < vs->vs_size; i++)
			new_pmap[0][i] = *((pfMapEntry *) &vs->vs_pmap[i]);
			
		/* Initialize the remainder of the block */
		for (i = vs->vs_size; i < PAGEMAP_ENTRIES; i++)
			new_pmap[0][i].pagerFileIndex = (int) PAGER_FILE_NULL;
		    
		/* And free the old map */
		kfree(vs->vs_pmap, PAGEMAP_SIZE(vs->vs_size));
	    }
	    
	} else {	/* The new map is a direct one */
	
	    new_pmap = (pfMapEntry **) kget(PAGEMAP_SIZE(new_size));
	    if (new_pmap == NULL)
		    return KERN_FAILURE;

	    /* Copy info from the old map */
	    for (i = 0; i < vs->vs_size; i++)
		    new_pmap[i] = vs->vs_pmap[i];

	    /* Initialize the rest of the new map */
	    for (i = vs->vs_size; i < new_size; i++)
		    ((pfMapEntry *) &new_pmap[i])->pagerFileIndex =
		    					(int) PAGER_FILE_NULL;
		    
	    if (vs->vs_size > 0)
	    	kfree(vs->vs_pmap, PAGEMAP_SIZE(vs->vs_size));
	}
	
	vs->vs_pmap = new_pmap;
leavemapalone:
	vs->vs_size = new_size;
    }

    /*
     * Now look for the entry in the map.
     */
    if (INDIRECT_PAGEMAP(vs->vs_size)) {
    
    	    int indirBlock = f_page / PAGEMAP_ENTRIES;	/* the indirect block */
	    int blockOffset = f_page % PAGEMAP_ENTRIES;	/* offset into dir block */
	    
	    if (vs->vs_pmap[indirBlock] == NULL) {
		    if (flag == B_READ)
			    return KERN_FAILURE;

		    vs->vs_pmap[indirBlock]=(pfMapEntry *) kget(PAGEMAP_THRESHOLD);
				
		    if (vs->vs_pmap[indirBlock] == NULL)
			    return KERN_FAILURE;

		    for (i = 0; i < PAGEMAP_ENTRIES; i++)
			vs->vs_pmap[indirBlock][i].pagerFileIndex =
							(int) PAGER_FILE_NULL;
	    }
	   
	    if (vs->vs_pmap[indirBlock][blockOffset].pagerFileIndex ==
	    	(int) PAGER_FILE_NULL) {
		
		    if (flag == B_READ)
			    return KERN_FAILURE;

		    result = vnode_pager_findpage(vs->vs_pf,
		    			&vs->vs_pmap[indirBlock][blockOffset]);
		    if (result == KERN_FAILURE)
			    return KERN_FAILURE;
	    }
	    
	    *mapEntry = vs->vs_pmap[indirBlock][blockOffset];
	    
    } else {	/* direct map */
    
	    if (((pfMapEntry *) &vs->vs_pmap[f_page])->pagerFileIndex ==
	    	(int) PAGER_FILE_NULL) {
	    
		    if (flag == B_READ)		/* not present in map */
			    return KERN_FAILURE;
			    
		    result = vnode_pager_findpage(vs->vs_pf, (pfMapEntry *) &vs->vs_pmap[f_page]);
		    if (result == KERN_FAILURE)
			    return KERN_FAILURE;
	    }
	    
	    *mapEntry = * ((pfMapEntry *) &vs->vs_pmap[f_page]);
    }
    
    return KERN_SUCCESS;
}

/*
 *	vnode_pager_create
 *
 *	Create an vstruct corresponding to the given vp.
 *
 */
vnode_pager_t
vnode_pager_create(vp)
	register struct vnode	*vp;
{
	register vnode_pager_t	vs;

	/*
	 *	XXX This can still livelock -- if the
	 *	pageout daemon needs a vnode_pager record
	 *	it won't get one until someone else
	 *	refills the zone.
	 */

	vs = (struct vstruct *) zalloc(vstruct_zone);

	if (vs == VNODE_PAGER_NULL)
		return(vs);

	vs->is_device = FALSE;
	vs->vs_count = 1;
	vp->vm_info->pager = (vm_pager_t) vs;
	vs->vs_vp = vp;
	vs->vs_swapfile = FALSE;

	VN_HOLD(vp);

	vnode_pager_vput(vs);

	return(vs);
}

/*
 *	vnode_pager_setup
 *
 *	Set up a vstruct for a given vnode.  This is an exported routine.
 */
vm_pager_t
vnode_pager_setup(vp, is_text, can_cache)
	struct vnode	*vp;
	boolean_t	is_text;
	boolean_t	can_cache;
{
	register pager_file_t	pf;

	unix_master();

	if (is_text)
		vp->v_flag |= VTEXT;

	if (vp->vm_info->pager == vm_pager_null) {
		/*
		 * Check to make sure this isn't in use as a pager file.
		 */
		for (pf = (pager_file_t) queue_first(&pager_files);
		     !queue_end(&pager_files, &pf->pf_chain);
		     pf = (pager_file_t) queue_next(&pf->pf_chain)) {
			if (pf->pf_vp == vp) {
				return(vm_pager_null);
			}
		}
		(void) vnode_pager_create(vp);
		if (can_cache)
			pager_cache(vm_object_lookup(vp->vm_info->pager), TRUE);
	} 

	unix_release();
	/*
	 * Try to keep something in the vstruct zone since we can sleep
	 * here if necessary.
	 */
	zfree(vstruct_zone, zalloc(vstruct_zone));
	return(vp->vm_info->pager);
}

boolean_t
vnode_pagein(m)
	vm_page_t	m;		/* page to read */

{
	register struct vnode	*vp;
	vnode_pager_t	vs;
	pager_return_t	ret = 0;
	vm_offset_t	f_offset;
	pfMapEntry	mapEntry;

	/*
	 *	Get the vnode and the offset within it to read from.
	 *	Lock the vnode while we play with it.
	 */
	unix_master();

#if	NeXT
	event_meter (EM_PAGER);
#endif	NeXT

	vs = (vnode_pager_t) m->object->pager;
	vp = vnode_pager_vget(vs);
	f_offset = m->offset + m->object->paging_offset;

	XPR(XPR_VNODE_PAGER, ("pagein(S): vp = 0x%x, offset = 0x%x\n", vp, f_offset));

	if (vs->vs_swapfile)
	    if (pagerfile_bmap(vs, f_offset, B_READ, &mapEntry) == KERN_FAILURE)
		ret = PAGER_ABSENT;
	    else {
		f_offset = ptoa(mapEntry.pageOffset);
		vp = pager_file_index[mapEntry.pagerFileIndex]->pf_vp;
		pmon_log_event(PMON_SOURCE_VM, KP_VM_SWAP_PAGEIN,
				f_offset, ((task_t)current_task())->proc->p_pid,
				vp->v_op == &ufs_vnodeops ?
					((struct inode *) vp->v_data)->i_number :
					0);
	    }
	else
		pmon_log_event(PMON_SOURCE_VM, KP_VM_VNODE_PAGEIN,
				f_offset, ((task_t)current_task())->proc->p_pid,
				vp->v_op == &ufs_vnodeops ?
					((struct inode *) vp->v_data)->i_number :
					0);

	if (ret != PAGER_ABSENT)
		ret = VOP_PAGEIN(vp, m, f_offset);

	XPR(XPR_VNODE_PAGER, ("pagein(E): vp = 0x%x, offset = 0x%x\n", vp, f_offset));

	vnode_pager_vput(vs);
	unix_release();
	return(ret);
}

boolean_t
vnode_pageout(m)
	vm_page_t	m;
{
	register struct vnode	*vp;
	vm_size_t	size = PAGE_SIZE;
	vnode_pager_t	vs;
	vm_offset_t	f_offset;
	pager_return_t	ret;
	pfMapEntry mapEntry;
	
	unix_master();

#if	NeXT
	event_meter (EM_PAGER);
#endif	NeXT

	vs = (vnode_pager_t) m->object->pager;
	vp = vnode_pager_vget(vs);
	f_offset = m->offset + m->object->paging_offset;
	size = PAGE_SIZE;

#if	MACH_NBC
	if (!vs->vs_swapfile) {
		/*
		 *	Be sure that a paging operation doesn't
		 *	accidently extend the size of "mapped" file.
		 *
		 *	However, we do extend the size up to the current
		 *	size kept in the vm_info structure.
		 */
		if (f_offset + size > vp->vm_info->vnode_size) {
			if (f_offset > vp->vm_info->vnode_size)
				size = 0;
			else
				size = vp->vm_info->vnode_size - f_offset;
		}
	}
#endif	MACH_NBC

	XPR(XPR_VNODE_PAGER, ("pageout(S): vp = 0x%x, offset = 0x%x\n", vp, f_offset));

	if (vs->vs_swapfile) {
	    if (pagerfile_bmap(vs, f_offset, B_WRITE, &mapEntry) == KERN_FAILURE) {
		    vnode_pager_vput(vs);
		    unix_release();
		    return(PAGER_ERROR);
	    }
	    /*
	     *	If the paging operation extends the size of the
	     *	pagerfile, update the information in the vm_info
	     *	structure
	     */
	    f_offset = ptoa(mapEntry.pageOffset);
	    vp = pager_file_index[mapEntry.pagerFileIndex]->pf_vp;
	    if (f_offset + size > vp->vm_info->vnode_size) {
		    vp->vm_info->vnode_size = f_offset + size;
	    }
	}
	if (size) {
		pmon_log_event(PMON_SOURCE_VM, KP_VM_PAGEOUT,
				f_offset, ((task_t)current_task())->proc->p_pid,
				vp->v_op == &ufs_vnodeops ?
					((struct inode *) vp->v_data)->i_number :
					0);

		ret = VOP_PAGEOUT(vp, VM_PAGE_TO_PHYS(m), size, f_offset);
	}
	else {
		ret = PAGER_SUCCESS;	/* writing past EOF!  Users problem */
	}
	if (ret == PAGER_SUCCESS) {
		m->clean = TRUE; 		/* XXX - wrong place */
		pmap_clear_modify(VM_PAGE_TO_PHYS(m)); /* XXX - wrong place */
	}
	else {
		printf("vnode_pageout: failed!\n");
	}

	XPR(XPR_VNODE_PAGER, ("pageout(E): vp = 0x%x, offset = 0x%x\n", vp, f_offset));

	vnode_pager_vput(vs);
	unix_release();
	return(ret);
}

/*
 *	vnode_has_page:
 *
 *	Parameters:
 *		pager
 *		id		paging object
 *		offset		Offset in paging object to test
 *
 *	Assumptions:
 *		This is only used on shadowing (copy) objects.
 *		Since copy objects are always backed by a swapfile, we just test
 *		the map for that swapfile to see if the page is present.
 */
boolean_t
vnode_has_page(pager, offset)
	vm_pager_t	pager;
	vm_offset_t	offset;
{
	vnode_pager_t	vs = (vnode_pager_t) pager;
	pfMapEntry	mapEntry;

	/*
	 * For now, we do all inode hacking on the master cpu.
	 */
	unix_master();

	if (vs == VNODE_PAGER_NULL)
		panic("vnode_has_page: failed lookup");

	if (vs->vs_swapfile) {
		unix_release();
		if (pagerfile_bmap(vs, offset, B_READ, &mapEntry) == KERN_FAILURE)
			return FALSE;
		else
			return TRUE;
	}
	else {
		 panic("vnode_has_page called on non-default pager");
	}
}


/*
 * 	Routine:	vnode_pager_file_init
 *	Function:
 *		Create a pager_file structure for a new pager file.
 *	Arguments:
 *		The file in question is specified by vnode pointer.
 *		lowat and hiwat are the low water and high water marks
 *		that the size of pager file will float between.  If
 *		the low water mark is zero, then the file will not
 *		shrink after paging space is freed.  If the high water
 *		mark is zero, the file will grow without bounds.
 */
int
vnode_pager_file_init(pfp, vp, lowat, hiwat)
	pager_file_t	*pfp;
	struct vnode	*vp;
	long lowat;
	long hiwat;
{
	struct vattr	vattr;
	register pager_file_t	pf;
	int	error;
	long	i;
	struct statfs	fstat;
	struct ucred	*cred;
	vm_size_t	size;

	*pfp = PAGER_FILE_NULL;

	/*
	 * Make sure no other object paging to this file?
	 */
	mfs_uncache(vp);
	if (vp->vm_info->mapped) {
		return(EBUSY);
	}
	
	/*
	 * Clean up the file blocks on a pager file by
	 * truncating to length "lowat".
	 */
	cred = u.u_cred;
	error = VOP_GETATTR(vp, &vattr, cred);
	size = vattr.va_size;
	if (size > lowat) {
		vattr_null(&vattr);
		vattr.va_size = size = lowat;
		error = VOP_SETATTR(vp, &vattr, cred);
		if (error) {
			return(error);
		}
	}

	/*
	 * Initialize the vnode_size field
	 */
	vp->vm_info->vnode_size = size;

	pf = (pager_file_t) kalloc(sizeof(struct pager_file));
	VN_HOLD(vp);
	pf->pf_vp = vp;
	crhold(cred);
	vp->vm_info->cred = cred;
	pf->pf_count = 0;
	pf->pf_lowat = atop(round_page(lowat));
	/*
	 * If no maximum space is specified, then we should make a map that
	 * can cover the entire disk, otherwise the block map need only
	 * cover the maximum space allowed.
	 */
	if (!hiwat) {
		error = VFS_STATFS(vp->v_vfsp, &fstat);
		if (error) {
			kfree(pf, sizeof(struct pager_file));
			return(error);
		}
		hiwat = fstat.f_blocks * fstat.f_bsize;
	}
	pf->pf_pfree = pf->pf_npgs = atop(hiwat);
	pf->pf_bmap = (u_char *) kalloc(RMAPSIZE(pf->pf_npgs));
	for (i = 0; i < pf->pf_npgs; i++) {
		clrbit(pf->pf_bmap, i);
	}
	pf->pf_hipage = -1;
	pf->pf_prefer = FALSE;
	lock_init(&pf->pf_lock, TRUE);

	/*
	 * Put the new pager file in the list.
	 */
	queue_enter(&pager_files, pf, pager_file_t, pf_chain);
	pager_file_count++;
	pf->pf_index = pager_file_count;
	pager_file_index[pager_file_count] = pf;
	*pfp = pf;
	return (0);
}

vnode_pager_shutdown()
{
	pager_file_t	pf;

	while (!queue_empty(&pager_files)) {
		pf = (pager_file_t) queue_first(&pager_files);
		VN_RELE(pf->pf_vp);
		queue_remove(&pager_files, pf, pager_file_t, pf_chain);
		pager_file_count--;
	}
}


/*
 *	Routine:	mach_swapon
 *	Function:
 *		Syscall interface to mach_swapon.
 */
int
mach_swapon(filename, flags, lowat, hiwat)
	char 	*filename;
	int	flags;
	long	lowat;
	long	hiwat;
{
	struct vnode		*vp;
	struct pathname		pn;
	register int		i;
	pager_file_t		pf;
	register int		error;
	char 			*name = NULL;
	int			namelen;

	if (!suser())
		return(EACCES);

	unix_master();

	/*
	 * Get a vnode for the paging area.
	 */
	vp = (struct vnode *)0;
	error = pn_get(filename, UIO_USERSPACE, &pn);
	if (error) {
		return(EINVAL);
	}

	namelen = pn.pn_pathlen + 1;
	name = (char *)kalloc(namelen);
	strncpy(name, pn.pn_path, namelen - 1);
	name[namelen - 1] = '\0';

	error = lookuppn(&pn, FOLLOW_LINK, (struct vnode **)0, &vp);
	pn_free(&pn);
	if (error) {
		goto bailout;
	}

	if (vp->v_type != VREG) {
		error = EINVAL;
		goto bailout;
	}

	/*
	 * Look to see if we are already paging to this file.
	 */
	for (pf = (pager_file_t) queue_first(&pager_files);
	     !queue_end(&pager_files, &pf->pf_chain);
	     pf = (pager_file_t) queue_next(&pf->pf_chain)) {
		if (pf->pf_vp == vp)
			break;
	}
	if (!queue_end(&pager_files, &pf->pf_chain)) {
		error = EBUSY;
		goto bailout;
	}

	error = vnode_pager_file_init(&pf, vp, lowat, hiwat);
	if (error) {
		goto bailout;
	}
	pf->pf_prefer = ((flags & MS_PREFER) != 0);
	pf->pf_name = name;
	name = NULL;

	error = 0;
bailout:
	if (vp) {
		VN_RELE(vp);
	}
	if (name) {
		kfree(name, namelen);
	}
	unix_release();
	return(error);
}

/*
 *	Routine:	vswap_allocate
 *	Function:
 *		Allocate a place for paging out a kernel-created
 *		memory object.
 *
 *	Implementation: 
 *		Looks through the paging files for the one with the
 *		most free space.  First, only "preferred" paging files
 *		are considered, then local paging files, and then
 *		remote paging files.  In each case, the pager file
 *		the most free blocks will be chosen.
 *
 *	In/out conditions:
 *		If the paging area is on a local disk, the inode is
 *		returned locked.
 */
pager_file_t
vswap_allocate()
{
	int		pass;
	int 		mostspace, freespace;
	struct vfs	*vfsp;
	pager_file_t	pf, mostpf;
	struct statfs	fstat;

	mostpf = PAGER_FILE_NULL;
	mostspace = 0;

	if (pager_file_count > 1) {
		for (pass = 0; pass < 4; pass++) {
			for (pf = (pager_file_t)queue_first(&pager_files);
			     !queue_end(&pager_files, &pf->pf_chain);
			     pf = (pager_file_t)queue_next(&pf->pf_chain)) {

				vfsp = pf->pf_vp->v_vfsp;
				if ((pass < 2) && !pf->pf_prefer)
					continue;
				if (((pass & 1) &&
				     (pf->pf_vp->v_op != &nfs_vnodeops)) ||
				    (!(pass &1) &&
				     (pf->pf_vp->v_op != &ufs_vnodeops)))
					continue;

				if (pf->pf_pfree > mostspace) {
					mostspace = pf->pf_pfree;
					mostpf = pf;
				}
			}
			/*
			 * If we found space, then break out of loop.
			 */
			if (mostpf != PAGER_FILE_NULL)
				break;
		}
	} else if (pager_file_count == 1) {
		mostpf = (pager_file_t) queue_first(&pager_files);
	}

	return(mostpf);
}

vm_pager_t
vnode_alloc(size)
	vm_size_t	size;
{
	pager_file_t	pf;
	vnode_pager_t 	vs = (vnode_pager_t) vm_pager_null;

#ifdef	lint
	size++;
#endif	lint

	unix_master();

	/*
	 *	Get a pager_file, then turn it into a paging space.
	 */

	if ((pf = vswap_allocate()) == PAGER_FILE_NULL) {
		goto out;
	}
	if ((vs = pagerfile_pager_create(pf, size)) ==
	    VNODE_PAGER_NULL) {
		vs = (vnode_pager_t) vm_pager_null;
		goto out;
	}
out:
	unix_release();
	return((vm_pager_t) vs);
}

boolean_t
vnode_dealloc(pager)
	vm_pager_t	pager;
{
	register struct vnode	*vp;
	vnode_pager_t	vs = (vnode_pager_t) pager;

	unix_master();

	vp = vnode_pager_vget(vs);

	ASSERT(vs->vs_count == 1);

	if (vs->vs_swapfile) {
	    pager_file_t	pf;
	    int		i,j;

	    ASSERT(vs->vs_pf);

	    pf = vs->vs_pf;
	    if (INDIRECT_PAGEMAP(vs->vs_size)) {
		for (i = 0; i < INDIRECT_PAGEMAP_ENTRIES(vs->vs_size); i++) {
		    if (vs->vs_pmap[i] != NULL) {
			for(j = 0; j < PAGEMAP_ENTRIES; j++) {
			    if (vs->vs_pmap[i][j].pagerFileIndex != (int) PAGER_FILE_NULL)
				    vnode_pager_deallocpage(&vs->vs_pmap[i][j]);
			}
			kfree(vs->vs_pmap[i], PAGEMAP_THRESHOLD);
		    }
		}
		kfree(vs->vs_pmap, INDIRECT_PAGEMAP_SIZE(vs->vs_size));
	    } else {
		for (i = 0; i < vs->vs_size; i++) {
		    if (((pfMapEntry *) &vs->vs_pmap[i])->pagerFileIndex !=
		    					(int) PAGER_FILE_NULL)
			vnode_pager_deallocpage((pfMapEntry *)&vs->vs_pmap[i]);
		}
		if (vs->vs_size > 0)
			kfree(vs->vs_pmap, PAGEMAP_SIZE(vs->vs_size));
	    }
	    pf->pf_count--;
	} else {
	    vp->v_flag &= ~VTEXT;
	    vp->vm_info->pager = vm_pager_null; /* so VN_RELE will free */
    
	    VN_RELE(vp);
	}

	zfree(vstruct_zone, (vm_offset_t) vs);
	unix_release();
}

/*
 *	Remove an vnode from the object cache.
 */
void
vnode_uncache(vp)
	register struct vnode	*vp;
{
	register boolean_t	was_locked;

	if (vp->vm_info->pager == vm_pager_null)
		return;
	unix_master();

	/*
	 * The act of uncaching may cause an object to be deallocated
	 * which may need to wait for the pageout daemon which in turn
	 * may be waiting for this inode's lock, so be sure to unlock
	 * and relock later if necessary.  (This of course means that
	 * code calling this routine must be able to handle the fact
	 * that the inode has been unlocked temporarily).  This code, of
	 * course depends on the Unix master restriction for proper
	 * synchronization.
	 */

/* FIXME: Make it so this routine is called only when the vnode is unlocked,
	  It is wrong to unlock it here, as we have no idea who may have
	  it locked an for what purpose! XXXXXXXX */
/* FIXME: We will just panic here to see when it happens
	if (was_locked = (ip->i_flag & ILOCKED))
		IUNLOCK(ip);
*/
	was_locked = FALSE;
	if (vp->v_op ==  &ufs_vnodeops) {
		if (VTOI(vp)->i_flag & ILOCKED) {
			was_locked = TRUE;
			VTOI(vp)->i_flag &= ~ILOCKED;
		}
	} else if (vp->v_op == &nfs_vnodeops) {
		if (vtor(vp)->r_flags & RLOCKED) {
			was_locked = TRUE;
			vtor(vp)->r_flags &= ~RLOCKED;
		}
	}

#if	MACH_NBC
	mfs_uncache(vp);
#endif	MACH_NBC
	vm_object_uncache(vp->vm_info->pager);

	if (was_locked) {
		if (vp->v_op ==  &ufs_vnodeops) {
			VTOI(vp)->i_flag |= ILOCKED;
		} else if (vp->v_op == &nfs_vnodeops) {
			vtor(vp)->r_flags |= RLOCKED;
		}
	}
/* FIXME: see above
	if (was_locked)
		ILOCK(ip);
*/

	unix_release();
}

void
vnode_pager_init()
{
	register vm_size_t	size;
	register int		i;

	/*
	 *	Initialize zone of paging structures.
	 */

	size = (vm_size_t) sizeof(struct vstruct);
	vstruct_zone = zinit(size,
			(vm_size_t) 10000*size,	/* XXX */
			PAGE_SIZE,
			FALSE, "vnode pager structures");
	simple_lock_init(&vstruct_lock);
	queue_init(&pager_files);
	pager_file_count = 1;
}
