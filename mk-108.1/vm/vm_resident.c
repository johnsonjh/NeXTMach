/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_page.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Resident memory management module.
 *
 * HISTORY
 * 22-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Reduce size of pre-allocated kernel map entries.
 *
 * 26-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Eliminate vm_page_t->fake field references.
 *	Eliminate vm_page_replace().
 *
 * 21-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added initialization for all fields; most fields are now set in
 *	a static template.
 *
 *  6-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Allow more parameters to be set before boot.
 *
 * 22-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Fixed rampant, improper use of page_size.  Use PAGE_SIZE only!
 *
 * 30-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Separate vm_page_alloc() into allocation and initialization parts;
 *	renamed vm_page_init() to vm_page_startup().
 *	Only free non-fictitious pages in vm_page_free().
 *
 * 26-Sep-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Properly decrement inactive count when wiring down a page that
 *	was inactive.
 *
 * 17-Aug-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Allow page bucket count to be patched in.
 *	Add consistency checking before using pages.
 *
 * 13-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Add vm_page_hash_mask to save precomputed value for use in
 *	object/offset hash value computation.
 *
 * 23-Mar-87  William Bolosky (bolosky) at Carnegie-Mellon University
 *	MACH_XP: Do a PAGE_WAKEUP on a page when it is being freed to
 *	kick anyone who is blocked on a page_lock on a page which is
 *	getting flushed.  Will probably wind up immediately doing a
 *	pager_data_request on the page, but such is life.
 *
 * 18-Mar-87  John Seamons (jks) at NeXT
 *	NeXT: added support for non-contiguous physical memory regions.
 *
 *  4-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	Split up page system lock.  Use thread_wakeup even without
 *	MACH_TT.
 *
 * 26-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Remove support for the kobject zone, which is no longer needed.
 *
 * 13-Feb-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH_XP: Have vm_page_alloc reserve some pages for the kernel-only tasks
 *	(e.g. those doing pageout).
 *
 * 10-Feb-87  William Bolosky (bolosky) at Carnegie-Mellon University
 *	Have vm_page_alloc clear the laundry bit.
 *
 *  9-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Initialize wanted field of resident page structure to FALSE.
 *
 *  3-Feb-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added check for whether page is in VP table before removing
 *	in vm_page_remove; after a vm_page_replace, we often want to
 *	vm_page_free, which does a vm_page_remove.
 *
 *  3-Feb-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added vm_page_replace.
 *
 * 19-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Use thread_wakeup instead of wakeup for MACH_TT.
 *
 * 20-Nov-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added resident_page_count to object.
 *
 *  9-Nov-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Minor fix to vm_page_activate to keep active/inactive counts
 *	properly.
 *
 *  5-Nov-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added copy_on_write bit for pages (for "copy" objects),
 *	cleared when page is allocated.
 *
 * 14-Oct-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added physical-address field to page structure to allow
 *	for holes in memory (or physical device memory in hyperspace,
 *	etc.)  Currently, it is assumed that memory between first_page
 *	and last_page is still contiguous and that we can do 'subscripting'
 *	for PHYS_TO_VM_PAGE.
 *
 * 30-Sep-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Support for inactive list and reference bits.
 *
 * 27-Jul-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Removed pmap_clear_reference call in vm_page_alloc (someone
 *	ALWAYS writes into a newly allocated page before entering it
 *	into an address space, so it's sort of silly).
 *
 *  1-Jun-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added vm_page_activate.
 *
 * 29-May-86  David Golub (dbg) at Carnegie-Mellon University
 *	Changed (auto-increment + test) in vm_page_wire, vm_page_unwire
 *	to separate operations to compensate for RT code-generation bug.
 *
 * 21-May-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added page_mask, and vm_set_page_size to set all page_size
 *	related constants from page_size.
 *
 * 17-May-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Call pmap_clear_modify on newly allocated pages.
 *
 * 10-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */

#import <mach_xp.h>
#import <show_space.h>

#import <kern/task.h>
#import <kern/thread.h>
#import <sys/types.h>
#import <vm/vm_map.h>
#import <vm/vm_page.h>
#import <vm/vm_prot.h>
#import <vm/vm_statistics.h>
#import <vm/vm_pageout.h>

#import <vm/pmap.h>

#import <machine/spl.h>

/*
 *	Associated with page of user-allocatable memory is a
 *	page structure.
 */

vm_offset_t	map_data;
vm_size_t	map_data_size;

vm_offset_t	kentry_data;
vm_size_t	kentry_data_size;

vm_offset_t	zdata;
vm_size_t	zdata_size;

queue_head_t	*vm_page_buckets;		/* Array of buckets */
int		vm_page_bucket_count = 0;	/* How big is array? */
int		vm_page_hash_mask;		/* Mask for hash function */
simple_lock_data_t	bucket_lock;		/* lock for all buckets XXX */

vm_size_t	page_size  = 0;		/* must set in MD code or patch */
vm_size_t	page_mask  = 0;
int		page_shift = 0;

queue_head_t	vm_page_queue_free;
queue_head_t	vm_page_queue_active;
queue_head_t	vm_page_queue_inactive;
simple_lock_data_t	vm_page_queue_lock;
simple_lock_data_t	vm_page_queue_free_lock;

vm_page_t	vm_page_array;
long		first_page;
long		last_page;
vm_offset_t	first_phys_addr;
vm_offset_t	last_phys_addr;

int	vm_page_free_count;
int	vm_page_active_count;
int	vm_page_inactive_count;
int	vm_page_wire_count;
int	vm_page_laundry_count;

int	vm_page_free_target = 0;
int	vm_page_free_min = 0;
int	vm_page_inactive_target = 0;
int	vm_page_free_reserved = 0;

struct vm_page	vm_page_template;

#if	SHOW_SPACE
extern int	show_space;
#endif	SHOW_SPACE

/*
 *	vm_set_page_size:
 *
 *	Sets the page size, perhaps based upon the memory
 *	size.  Must be called before any use of page-size
 *	dependent functions.
 *
 *	Sets page_shift and page_mask from page_size.
 */
void vm_set_page_size()
{
	page_mask = page_size - 1;

	if ((page_size == 0) || ((page_mask & page_size) != 0))
		panic("vm_set_page_size: improper page size");

	for (page_shift = 0; ; page_shift++)
		if ((1 << page_shift) == page_size)
			break;
}


/*
 *	vm_page_startup:
 *
 *	Initializes the resident memory module.
 *
 *	Allocates memory for the page cells, and
 *	for the object/offset-to-page hash table headers.
 *	Each page cell is initialized and placed on the free list.
 */
#if	NeXT
vm_offset_t vm_page_startup(mem_region, num_regions, vaddr)
	register mem_region_t	mem_region;
	int			num_regions;
#else	NeXT
vm_offset_t vm_page_startup(start, end, vaddr)
	register vm_offset_t	start;
	vm_offset_t	end;
#endif	NeXT
	register vm_offset_t	vaddr;
{
#if	NeXT
	register vm_offset_t	start;
	int			mem_size, pages;
	vm_size_t		ovhd_pages;
	mem_region_t		rp;
#endif	NeXT
	register vm_offset_t	mapped;
	register vm_page_t	m;
	register queue_t	bucket;
#if	NeXT
#else	NeXT
	vm_size_t		npages;
#endif	NeXT
	register vm_offset_t	new_start;
	int			i;
	vm_offset_t		pa;

	/*
	 *	Initialize the vm_page template.
	 *	[Indented items must be reset later.]
	 */

	m = &vm_page_template;
	 m->object = VM_OBJECT_NULL;
	 m->offset = 0;
	m->wire_count = 0;

#if	NeXT
	m->free = FALSE;
#endif	NeXT
	m->inactive = FALSE;
	m->active = FALSE;
	m->laundry = FALSE;

	m->clean = TRUE;

	m->busy = TRUE;
	m->wanted = FALSE;
	m->tabled = FALSE;
	m->copy_on_write = FALSE;
	m->fictitious = FALSE;
	m->absent = FALSE;
	m->error = FALSE;
	
	 m->phys_addr = 0;

	m->page_lock = VM_PROT_NONE;
	m->unlock_request = VM_PROT_NONE;

	/*
	 *	Initialize the locks
	 */

	simple_lock_init(&vm_page_queue_free_lock);
	simple_lock_init(&vm_page_queue_lock);

	/*
	 *	Initialize the queue headers for the free queue,
	 *	the active queue and the inactive queue.
	 */

	queue_init(&vm_page_queue_free);
	queue_init(&vm_page_queue_active);
	queue_init(&vm_page_queue_inactive);

	/*
	 *	Allocate (and initialize) the hash table buckets.
	 *
	 *	The number of buckets should be a power of two to
	 *	get a good hash function.  The following computation
	 *	chooses the first power of two that is greater
	 *	than the number of physical pages in the system.
	 */

#if	NeXT
	for (mem_size = 0, i = 0; i < num_regions; i++) {
		rp = &mem_region[i];
		if (rp->region_type != REGION_MEM)
			continue;
		mem_size += rp->last_phys_addr - rp->first_phys_addr;
	}
	start = mem_region[0].first_phys_addr;
#endif	NeXT
#if	NeXT
	/* allocate directly from memory instead of VM */
	vaddr = start;
#endif	NeXT
	vm_page_buckets = (queue_t) vaddr;
	bucket = vm_page_buckets;
	if (vm_page_bucket_count == 0) {
		vm_page_bucket_count = 1;
#if	NeXT
		while (vm_page_bucket_count < atop(mem_size))
#else	NeXT
		while (vm_page_bucket_count < atop(end - start))
#endif	NeXT
			vm_page_bucket_count <<= 1;
	}

	vm_page_hash_mask = vm_page_bucket_count - 1;

	if (vm_page_hash_mask & vm_page_bucket_count)
		printf("vm_page_startup: WARNING -- strange page hash\n");

	/*
	 *	Validate these addresses.
	 */

#if	NeXT
	new_start = (int)((queue_t)start + vm_page_bucket_count);
#else	NeXT
	new_start = round_page(((queue_t)start) + vm_page_bucket_count);
#endif	NeXT
	mapped = vaddr;
#if	NeXT
	vaddr = new_start;
#else	NeXT
	vaddr = pmap_map(mapped, start, new_start,
			VM_PROT_READ|VM_PROT_WRITE);
#endif	NeXT
#if	SHOW_SPACE
	if (show_space)
		printf ("vm_page_buckets %d@%x-%x\n",
			new_start - start, start, new_start);
#endif	SHOW_SPACE
	start = new_start;
	blkclr((caddr_t) mapped, vaddr - mapped);
	mapped = vaddr;

	for (i = vm_page_bucket_count; i--;) {
		queue_init(bucket);
		bucket++;
	}

	simple_lock_init(&bucket_lock);

	/*
	 *	round (or truncate) the addresses to our page size.
	 */

#if	NeXT
	mem_region[0].last_phys_addr =
		 trunc_page (mem_region[0].last_phys_addr);
#else	NeXT
	end = trunc_page(end);
#endif	NeXT

	/*
	 *	Steal pages for some zones that cannot be
	 *	dynamically allocated.  Sum of zdata and map_data is
	 *	still page aligned.
	 */

	zdata_size = 40*sizeof(struct zone);
	zdata = (vm_offset_t) vaddr;
	vaddr += zdata_size;

	map_data_size = 10*sizeof(struct vm_map); /* expand's at run-time */
	map_data = (vm_offset_t) vaddr;
	vaddr += map_data_size;

	/*
	 *	Allow 512 kernel map entries... this should be plenty
	 *	since people shouldn't be cluttering up the kernel
	 *	map (they should use their own maps).
	 */

	kentry_data_size = 512*sizeof(struct vm_map_entry);
	kentry_data = (vm_offset_t) vaddr;
	vaddr += kentry_data_size;
	vaddr = round_page(vaddr);
	kentry_data_size = vaddr - kentry_data;

	/*
	 *	Validate these zone addresses.
	 */

	new_start = start + (vaddr - mapped);
#if	NeXT
#else	NeXT
	pmap_map(mapped, start, new_start, VM_PROT_READ|VM_PROT_WRITE);
#endif	NeXT
	blkclr((caddr_t) mapped, (vaddr - mapped));
	mapped = vaddr;
#if	SHOW_SPACE
	if (show_space)
		printf ("static zones %d@%x-%x\n",
			new_start - start, start, new_start);
#endif	SHOW_SPACE
	start = new_start;

	/*
 	 *	Compute the number of pages of memory that will be
	 *	available for use (taking into account the overhead
	 *	of a page structure per page).
	 */

#if	NeXT
	mem_region[0].first_phys_addr = start;

	for (vm_page_free_count = 0, i = 0; i < num_regions; i++) {
		rp = &mem_region[i];
		if (rp->region_type != REGION_MEM)
			continue;
		rp->num_pages =
			atop (rp->last_phys_addr - rp->first_phys_addr);
		vm_page_free_count += rp->num_pages;
	}
	ovhd_pages = atop (round_page (vm_page_free_count *
		sizeof (struct vm_page)));

	/* assumes all overhead pages will fit into region 0 */
	mem_region[0].num_pages -= ovhd_pages;
	vm_page_free_count -= ovhd_pages;
#else	NeXT
	vm_page_free_count = npages =
		(end - start)/(PAGE_SIZE + sizeof(struct vm_page));
#endif	NeXT

	/*
	 *	Initialize the mem entry structures now, and
	 *	put them in the free queue.
	 */

#if	NeXT
	for (i = 0; i < num_regions; i++) {
		rp = &mem_region[i];
		if (rp->region_type != REGION_MEM)
			continue;
		rp->vm_page_array = (vm_page_t) vaddr;
		vaddr += rp->num_pages * sizeof (struct vm_page);
	}
	new_start = start + (vaddr - mapped);
	mapped = new_start;
	mem_region[0].first_phys_addr = round_page (new_start);
#if	SHOW_SPACE
	if (show_space) {
		printf ("vm_page %d@%x-%x\n",
			 new_start - start, start, new_start);
		printf ("initial vmpf %d wired %d hole %d\n",
			vm_page_free_count, vm_page_wire_count,
			mem_region[0].first_phys_addr - new_start);
	}
#endif	SHOW_SPACE

	for (i = 0; i < num_regions; i++) {
		rp = &mem_region[i];
		if (rp->region_type != REGION_MEM || rp->last_phys_addr == 0)
			continue;
		rp->first_page = atop(rp->first_phys_addr);
		rp->last_page = rp->first_page + rp->num_pages - 1;
		rp->first_phys_addr = ptoa(rp->first_page);
		rp->last_phys_addr = ptoa(rp->last_page) + page_mask;

		m = rp->vm_page_array;

		/*
		 *	Clear all of the page structures
		 */
		blkclr((caddr_t)m, rp->num_pages * sizeof(*m));

		pa = rp->first_phys_addr;
		pages = rp->num_pages;
		while (pages--) {
			m->phys_addr = pa;
			queue_enter(&vm_page_queue_free, m, vm_page_t, pageq);
#if	NeXT
			m->free = TRUE;
#endif	NeXT
			m++;
			pa += page_size;
		}
	}
#else	NeXT
	m = vm_page_array = (vm_page_t) vaddr;
	first_page = start;
	first_page += npages*sizeof(struct vm_page);
	first_page = atop(round_page(first_page));
	last_page  = first_page + npages - 1;

	first_phys_addr = ptoa(first_page);
	last_phys_addr  = ptoa(last_page) + page_mask;

	/*
	 *	Validate these addresses.
	 */

	new_start = start + (round_page(m + npages) - mapped);
	mapped = pmap_map(mapped, start, new_start,
			VM_PROT_READ|VM_PROT_WRITE);
	start = new_start;

	/*
	 *	Clear all of the page structures
	 */
	blkclr((caddr_t)m, npages * sizeof(*m));

	pa = first_phys_addr;
	while (npages--) {
		m->phys_addr = pa;
		queue_enter(&vm_page_queue_free, m, vm_page_t, pageq);
#if	NeXT
		m->free = TRUE;
#endif	NeXT
		m++;
		pa += PAGE_SIZE;
	}
#endif	NeXT

	/*
	 *	Initialize vm_pages_needed lock here - don't wait for pageout
	 *	daemon	XXX
	 */
	simple_lock_init(&vm_pages_needed_lock);

	return(mapped);
}

#if	NeXT
/*
 *	vm_which_region:
 *
 *	Determines the memory region that the physical address
 *	resides in.
 */
vm_which_region (pa)
	register vm_offset_t	pa;
{
	register int		i;
	register mem_region_t	rp;

	for (i = 0; i < num_regions; i++) {
		rp = &mem_region[i];
		if (pa >= rp->first_phys_addr && pa < rp->last_phys_addr)
			return (i);
	}
	return (-1);
}

/*
 *	vm_region_to_vm_page:
 *
 *	Translates physical address to vm_page pointer.
 */
vm_page_t
vm_region_to_vm_page (pa)
	register vm_offset_t	pa;
{
	register int		i;
	register mem_region_t	rp;
	register vm_page_t	m;

	if ((i = vm_which_region (pa)) == -1)
		return (0);
	rp = &mem_region[i];
	if (rp->region_type != REGION_MEM)
		return (0);
	return (&rp->vm_page_array[atop(pa) - rp->first_page]);
}
#endif	NeXT

/*
 *	vm_page_hash:
 *
 *	Distributes the object/offset key pair among hash buckets.
 *
 *	NOTE:	To get a good hash function, the bucket count should
 *		be a power of two.
 */
#define vm_page_hash(object, offset) \
	(((unsigned)object+(unsigned)atop(offset))&vm_page_hash_mask)

/*
 *	vm_page_insert:		[ internal use only ]
 *
 *	Inserts the given mem entry into the object/object-page
 *	table and object list.
 *
 *	The object and page must be locked.
 */

void vm_page_insert(mem, object, offset)
	register vm_page_t	mem;
	register vm_object_t	object;
	register vm_offset_t	offset;
{
	register queue_t	bucket;
	int			spl;

	VM_PAGE_CHECK(mem);

	if (mem->tabled)
		panic("vm_page_insert: already inserted");

	/*
	 *	Record the object/offset pair in this page
	 */

	mem->object = object;
	mem->offset = offset;

	/*
	 *	Insert it into the object_object/offset hash table
	 */

	bucket = &vm_page_buckets[vm_page_hash(object, offset)];
	spl = splimp();
	simple_lock(&bucket_lock);
	queue_enter(bucket, mem, vm_page_t, hashq);
	simple_unlock(&bucket_lock);
	(void) splx(spl);

	/*
	 *	Now link into the object's list of backed pages.
	 */

	queue_enter(&object->memq, mem, vm_page_t, listq);
	mem->tabled = TRUE;

	/*
	 *	And show that the object has one more resident
	 *	page.
	 */

	object->resident_page_count++;
}

/*
 *	vm_page_remove:		[ internal use only ]
 *
 *	Removes the given mem entry from the object/offset-page
 *	table and the object page list.
 *
 *	The object and page must be locked.
 */

void vm_page_remove(mem)
	register vm_page_t	mem;
{
	register queue_t	bucket;
	int			spl;

	VM_PAGE_CHECK(mem);

	if (!mem->tabled)
		return;

	/*
	 *	Remove from the object_object/offset hash table
	 */

	bucket = &vm_page_buckets[vm_page_hash(mem->object, mem->offset)];
	spl = splimp();
	simple_lock(&bucket_lock);
	queue_remove(bucket, mem, vm_page_t, hashq);
	simple_unlock(&bucket_lock);
	(void) splx(spl);

	/*
	 *	Now remove from the object's list of backed pages.
	 */

	queue_remove(&mem->object->memq, mem, vm_page_t, listq);

	/*
	 *	And show that the object has one fewer resident
	 *	page.
	 */

	mem->object->resident_page_count--;

	mem->tabled = FALSE;
}

/*
 *	vm_page_lookup:
 *
 *	Returns the page associated with the object/offset
 *	pair specified; if none is found, VM_PAGE_NULL is returned.
 *
 *	The object must be locked.  No side effects.
 */

vm_page_t vm_page_lookup(object, offset)
	register vm_object_t	object;
	register vm_offset_t	offset;
{
	register vm_page_t	mem;
	register queue_t	bucket;
	int			spl;

	/*
	 *	Search the hash table for this object/offset pair
	 */

	bucket = &vm_page_buckets[vm_page_hash(object, offset)];

	spl = splimp();
	simple_lock(&bucket_lock);
	mem = (vm_page_t) queue_first(bucket);
	while (!queue_end(bucket, (queue_entry_t) mem)) {
		VM_PAGE_CHECK(mem);
		if ((mem->object == object) && (mem->offset == offset)) {
			simple_unlock(&bucket_lock);
			splx(spl);
			return(mem);
		}
		mem = (vm_page_t) queue_next(&mem->hashq);
	}

	simple_unlock(&bucket_lock);
	splx(spl);
	return(VM_PAGE_NULL);
}

/*
 *	vm_page_rename:
 *
 *	Move the given memory entry from its
 *	current object to the specified target object/offset.
 *
 *	The object must be locked.
 */
void vm_page_rename(mem, new_object, new_offset)
	register vm_page_t	mem;
	register vm_object_t	new_object;
	vm_offset_t		new_offset;
{
	if (mem->object == new_object)
		return;

	vm_page_lock_queues();	/* keep page from moving out from
				   under pageout daemon */
    	vm_page_remove(mem);
	vm_page_insert(mem, new_object, new_offset);
	vm_page_unlock_queues();
}

/*
 *	vm_page_init:
 *
 *	Initialize the given vm_page, entering it into
 *	the VP table at the given (object, offset),
 *	and noting its physical address.
 *
 *	Implemented using a template set up in vm_page_startup.
 *	All fields except those passed as arguments are static.
 */
void		vm_page_init(mem, object, offset, phys_addr)
	vm_page_t	mem;
	vm_object_t	object;
	vm_offset_t	offset;
	vm_offset_t	phys_addr;
{
#define	vm_page_init(page, object, offset, pa)  { \
		register \
		vm_offset_t	a = (pa); \
		*(page) = vm_page_template; \
		(page)->phys_addr = a; \
		vm_page_insert((page), (object), (offset)); \
	}

	vm_page_init(mem, object, offset, phys_addr);
}

/*
 *	vm_page_alloc:
 *
 *	Allocate and return a memory cell associated
 *	with this VM object/offset pair.
 *
 *	Object must be locked.
 */
vm_page_t vm_page_alloc(object, offset)
	vm_object_t	object;
	vm_offset_t	offset;
{
	register vm_page_t	mem;
	int		spl;

	spl = splimp();				/* XXX */
	simple_lock(&vm_page_queue_free_lock);
	if (queue_empty(&vm_page_queue_free)) {
		simple_unlock(&vm_page_queue_free_lock);
		splx(spl);
		return(VM_PAGE_NULL);
	}

	if ((vm_page_free_count < vm_page_free_reserved) &&
			!current_thread()->vm_privilege) {
		simple_unlock(&vm_page_queue_free_lock);
		splx(spl);
		return(VM_PAGE_NULL);
	}

	queue_remove_first(&vm_page_queue_free, mem, vm_page_t, pageq);
#if	NeXT
	mem->free = FALSE;
#endif	NeXT

	vm_page_free_count--;
	simple_unlock(&vm_page_queue_free_lock);
	splx(spl);

#if	NeXT
 	vm_page_remove(mem);	/* in case it is still in hash table */
#endif	NeXT
	vm_page_init(mem, object, offset, mem->phys_addr);

	/*
	 *	Decide if we should poke the pageout daemon.
	 *	We do this if the free count is less than the low
	 *	water mark, or if the free count is less than the high
	 *	water mark (but above the low water mark) and the inactive
	 *	count is less than its target.
	 *
	 *	We don't have the counts locked ... if they change a little,
	 *	it doesn't really matter.
	 */

	if ((vm_page_free_count < vm_page_free_min) ||
			((vm_page_free_count < vm_page_free_target) &&
			(vm_page_inactive_count < vm_page_inactive_target)))
		thread_wakeup((int) &vm_pages_needed);

#if	NeXT
	/*
	 *	Detect sequential access and inactivate previous page
	 */

	if (offset == object->last_alloc + PAGE_SIZE) {
		vm_page_t	last_mem;

		last_mem = vm_page_lookup(object, object->last_alloc);
		if (last_mem != VM_PAGE_NULL) {
			vm_page_lock_queues();
			vm_page_deactivate(last_mem);
			vm_page_unlock_queues();
		}
	}
	object->last_alloc = offset;
#endif	NeXT
	return(mem);
}

/*
 *	vm_page_free:
 *
 *	Returns the given page to the free list,
 *	disassociating it with any VM object.
 *
 *	Object and page must be locked prior to entry.
 */
void vm_page_free(mem)
	register vm_page_t	mem;
{
	vm_page_remove(mem);
#if	NeXT
	if (!mem->free) {
		vm_page_addfree(mem);
	}
}

void vm_page_addfree(mem)
	register vm_page_t	mem;
{
#endif	NeXT
	if (mem->active) {
		queue_remove(&vm_page_queue_active, mem, vm_page_t, pageq);
		mem->active = FALSE;
		vm_page_active_count--;
	}

	if (mem->inactive) {
		queue_remove(&vm_page_queue_inactive, mem, vm_page_t, pageq);
		mem->inactive = FALSE;
		vm_page_inactive_count--;
	}

#if	MACH_XP
	if (mem->laundry)
		mem->laundry = FALSE;
	if (vm_page_laundry_count > 0)
		vm_page_laundry_count--;

	PAGE_WAKEUP(mem);
#endif	MACH_XP

	if (!mem->fictitious) {
		int	spl;

		spl = splimp();
		simple_lock(&vm_page_queue_free_lock);
		queue_enter(&vm_page_queue_free, mem, vm_page_t, pageq);
#if	NeXT
		mem->free = TRUE;
#endif	NeXT
		vm_page_free_count++;
		simple_unlock(&vm_page_queue_free_lock);
		splx(spl);
	}
}

/*
 *	vm_page_wire:
 *
 *	Mark this page as wired down by yet
 *	another map, removing it from paging queues
 *	as necessary.
 *
 *	The page queues must be locked.
 */
void vm_page_wire(mem)
	register vm_page_t	mem;
{
	VM_PAGE_CHECK(mem);

	if (mem->wire_count == 0) {
		if (mem->active) {
			queue_remove(&vm_page_queue_active, mem, vm_page_t,
						pageq);
			vm_page_active_count--;
			mem->active = FALSE;
		}
		if (mem->inactive) {
			queue_remove(&vm_page_queue_inactive, mem, vm_page_t,
						pageq);
			vm_page_inactive_count--;
			mem->inactive = FALSE;
		}
#if	NeXT
		if (mem->free) {
			queue_remove(&vm_page_queue_free, mem, vm_page_t,
							pageq);
			vm_page_free_count--;
			mem->free = FALSE;
		}
#endif	NeXT
		vm_page_wire_count++;
	}
	mem->wire_count++;
}

/*
 *	vm_page_unwire:
 *
 *	Release one wiring of this page, potentially
 *	enabling it to be paged again.
 *
 *	The page queues must be locked.
 */
void vm_page_unwire(mem)
	register vm_page_t	mem;
{
	VM_PAGE_CHECK(mem);

	if (--mem->wire_count == 0) {
		queue_enter(&vm_page_queue_active, mem, vm_page_t, pageq);
		vm_page_active_count++;
		mem->active = TRUE;
		vm_page_wire_count--;
	}
}

/*
 *	vm_page_deactivate:
 *
 *	Returns the given page to the inactive list,
 *	indicating that no physical maps have access
 *	to this page.  [Used by the physical mapping system.]
 *
 *	The page queues must be locked.
 */
void vm_page_deactivate(m)
	register vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	/*
	 *	Only move active pages -- ignore locked or already
	 *	inactive ones.
	 */

	if (m->active) {
		pmap_clear_reference(VM_PAGE_TO_PHYS(m));
		queue_remove(&vm_page_queue_active, m, vm_page_t, pageq);
		queue_enter(&vm_page_queue_inactive, m, vm_page_t, pageq);
		m->active = FALSE;
		m->inactive = TRUE;
		vm_page_active_count--;
		vm_page_inactive_count++;
		if (m->clean && pmap_is_modified(VM_PAGE_TO_PHYS(m)))
			m->clean = FALSE;

#if	!MACH_XP
		m->laundry = !m->clean;
#endif	!MACH_XP
	}
}

/*
 *	vm_page_activate:
 *
 *	Put the specified page on the active list (if appropriate).
 *
 *	The page queues must be locked.
 */

void vm_page_activate(m)
	register vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	if (m->inactive) {
		queue_remove(&vm_page_queue_inactive, m, vm_page_t,
						pageq);
		vm_page_inactive_count--;
		m->inactive = FALSE;
	}
#if	NeXT
	if (m->free) {
		queue_remove(&vm_page_queue_free, m, vm_page_t,
						pageq);
		vm_page_free_count--;
		m->free = FALSE;
	}
#endif	NeXT
	if (m->wire_count == 0) {
		if (m->active)
			panic("vm_page_activate: already active");

		queue_enter(&vm_page_queue_active, m, vm_page_t, pageq);
		m->active = TRUE;
		vm_page_active_count++;
	}
}

/*
 *	vm_page_zero_fill:
 *
 *	Zero-fill the specified page.
 *	Written as a standard pagein routine, to
 *	be used by the zero-fill object.
 */

boolean_t vm_page_zero_fill(m)
	vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	pmap_zero_page(VM_PAGE_TO_PHYS(m));
	return(TRUE);
}

/*
 *	vm_page_copy:
 *
 *	Copy one page to another
 */

void vm_page_copy(src_m, dest_m)
	vm_page_t	src_m;
	vm_page_t	dest_m;
{
	VM_PAGE_CHECK(src_m);
	VM_PAGE_CHECK(dest_m);

	pmap_copy_page(VM_PAGE_TO_PHYS(src_m), VM_PAGE_TO_PHYS(dest_m));
}



