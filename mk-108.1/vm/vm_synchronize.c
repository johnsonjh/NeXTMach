/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 *  History:
 *
 *	6-Aug-90: Brian Pinkerton at NeXT
 *		created
 */
 

#import <mach_xp.h>

#import <sys/types.h>
#import <sys/param.h>

#import <vm/vm_param.h>
#import <vm/vm_object.h>
#import <vm/vm_map.h>
#import <vm/vm_page.h>
#import <vm/vm_statistics.h>

#import <sys/boolean.h>
#import <sys/kern_return.h>
#import <kern/task.h>
#import <kern/kalloc.h>

#import <kern/mach_types.h>	/* to get vm_address_t */


static kern_return_t map_push_range(vm_map_t map, vm_offset_t start, vm_offset_t end);
	
static kern_return_t object_push(vm_object_t object, vm_offset_t object_start, vm_offset_t object_end);
	
static void seen_pages_init(void);
static void seen_pages_free(void);
static boolean_t seen_page(vm_page_t page);

#define NHASH 128
struct pages_hash {
	struct pages_hash	*next;
	vm_page_t		page;
} pages_hash[NHASH];

#define pages_hash_fn(page) ((((int)(page))/sizeof(*(page)))%NHASH)

struct pages_hash *seen_pages;

int phys_pages;


/*
 * Push all pages to disk.
 */
kern_return_t
vm_synchronize(
	vm_map_t	task_map,
	vm_address_t	vmaddr,
	vm_size_t	vmsize)
{
	kern_return_t	result;

	if (!task_map)
		return KERN_FAILURE;

	if (vmsize == 0)
		vmsize = task_map->header.end - task_map->header.start;

	if (vmaddr == 0)
		vmaddr = task_map->header.start;

	seen_pages_init();

	result = map_push_range(task_map, vmaddr, vmaddr+vmsize);
	
	seen_pages_free();
	return result;
}


/*
 * Push a range of pages in a map.
 */
static kern_return_t
map_push_range(
	vm_map_t	map,
	vm_offset_t	start,
	vm_offset_t	end)
{
	vm_map_entry_t	entry;
	vm_object_t	object;
	vm_offset_t	tend;
	int		offset;
	kern_return_t	rtn, realRtn = KERN_SUCCESS;

	vm_map_lock_read(map);
	/*
	 * Push all the pages in this map.
	 */
	for (  entry = map->header.next
	     ; entry != &map->header
	     ; entry = entry->next)
	{
		if (entry->is_a_map || entry->is_sub_map) {
			vm_map_t sub_map;
			/*
			 * Recurse over this sub-map.
			 */
			sub_map = entry->object.sub_map;
			rtn = map_push_range(sub_map, start, end);
			if (rtn == KERN_FAILURE)
				realRtn = KERN_FAILURE;
			continue;
		}
		if (entry->start > end || entry->end <= start)
			continue;

		/*
		 * Push the pages we can see in this object chain.
		 */
		object = entry->object.vm_object;
		if (entry->start > start)
			start = entry->start;
		tend = (entry->end > end) ? end : entry->end;
		offset = entry->offset + start - entry->start;
		rtn = object_push(object, offset, offset + tend - start);
		if (rtn)
			realRtn = KERN_FAILURE;
	}
	vm_map_unlock(map);

	return realRtn;
}


/*
 *  Page out a page belonging to the given object.  The object must be locked.
 */
static int
vmPageoutPage(vm_object_t object, vm_page_t m)
{
	int pageout_succeeded;
	vm_pager_t pager = object->pager;
	
	vm_page_lock_queues();
	if (m->clean && !pmap_is_modified(VM_PAGE_TO_PHYS(m))) {
		vm_page_unlock_queues();
		return TRUE;
	}

	object->paging_in_progress++;
	
	m->busy = TRUE;
	if (m->inactive)
		vm_page_activate(m);
	vm_page_deactivate(m);
	
	pmap_remove_all(VM_PAGE_TO_PHYS(m));
	vm_stat.pageouts++;

	vm_page_unlock_queues();
	if (pager == vm_pager_null) {
		object->paging_in_progress--;
		return FALSE;
	}
	vm_object_unlock(object);
	
	pageout_succeeded = FALSE;
	if (pager != vm_pager_null) {
	    if (vm_pager_put(pager, m) == PAGER_SUCCESS) {
		pageout_succeeded = TRUE;
	    }
	}

	vm_object_lock(object);
	vm_page_lock_queues();
	m->busy = FALSE;
	PAGE_WAKEUP(m);
	
	object->paging_in_progress--;
	vm_page_unlock_queues();
	
	return pageout_succeeded;
}


/*
 * Count resident pages in this object and all shadow objects over the
 * specified address range.  If addr_list and state are non NULL
 * then fill in information about each page encountered.
 */
static kern_return_t
object_push(vm_object_t object, vm_offset_t object_start, vm_offset_t object_end)
{
	vm_object_t	shadow;
	vm_page_t	page;
	vm_size_t	object_size;
	int		flags;
	int		pageoutSucceeded;
	kern_return_t	rtn;

	if (object == VM_OBJECT_NULL)
		return KERN_SUCCESS;

	vm_object_lock(object);

	for (  page = (vm_page_t) queue_first(&object->memq)
	     ; !queue_end(&object->memq, (queue_entry_t) page)
	     ; page = (vm_page_t) queue_next(&page->listq))
	{
		/*
		 * If this page isn't actually referenced from this
		 * entry, then forget it.
		 */
		if (page->offset < object_start || page->offset >= object_end)
			continue;

		if (seen_page(page))
			continue;
		
		/*
		 *  Push this page.
		 */
		pageoutSucceeded = vmPageoutPage(object, page);
	}

	/*
	 * Recurse over shadow objects.
	 */
	object_size = object_end - object_start;
	if (object->size && object_end - object_start > object->size)
		object_size = object->size;
	object_start += object->shadow_offset;
	object_end = object_start + object_size;

	rtn = object_push(object->shadow, object_start, object_end);
	
	vm_object_unlock(object);
	thread_wakeup((int) object);
	
	if (rtn == KERN_FAILURE || !pageoutSucceeded)
	    	return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}


/*
 * Check to see if we've seen this page yet.  If not, inter it into our
 * hash.
 */
static boolean_t seen_page(vm_page_t page)
{
	int hash_ndx = pages_hash_fn(page);
	struct pages_hash *hash_page;

	for (  hash_page = &pages_hash[hash_ndx]
	     ; hash_page->next
	     ; hash_page = hash_page->next)
	{
		if (hash_page->page == page)
			return TRUE;
	}

	return FALSE;
}

static void seen_pages_init(void)
{
	int i;
	struct pages_hash *page;
	
	phys_pages = mem_size/page_size;
	seen_pages = (struct pages_hash *) kalloc(phys_pages * sizeof(struct pages_hash));
	
	for (i = NHASH-1; i >= 0; i--)
		pages_hash[i].next = NULL;

	for (i = phys_pages-2; i >= 0; i--)
		seen_pages[i].next = &seen_pages[i+1];
	seen_pages[phys_pages-1].next = NULL;
}


static void seen_pages_free(void)
{
	/*
	 * Physical page information.
	 */
	kfree(seen_pages, phys_pages * sizeof(struct pages_hash));
}

