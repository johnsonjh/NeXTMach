/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 *
 * 12-Jan-89	Brian Pinkerton at NeXT
 *	Created
 *
 */

#if	NeXT && DEBUG

#import <sys/types.h>
#import <kern/task.h>

#import <vm/vm_prot.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>

#import <next/machparam.h>
#import <next/eventc.h>


/*
 *  This file contains support for a memory-mapped, 64 bit microsecond clock.
 *  The idea is that we'll make user-level clock accesses dirt cheap, instead
 *  of using the current mechanism of a system call for every clock read.
 *
 *  The clock is not writeable.
 *
 *  Here's the basic idea:
 *    On the first page, we map the hardware clock.  It's at a fixed physical
 *    address, so we can't control its offset into its physical page.  The user
 *    routine will just have to know the right offset.
 *
 *    On the second page, we map the software maintained upper 44 bits of the
 *    clock.  Because the user doesn't know the offset of these counters, we
 *    should  allocate them at the beginning of a page.  Furthermore, to be
 *    more secure, we will stick these guys on their own physical page.
 *    
 */

vm_offset_t event_page;
extern vm_map_t kernel_map;
extern task_t kernel_task;

/*
 *  This routine should be called before any timers are run (because we set the
 *  location of software maintained counters here) and after kernel memory is
 *  initialized.  It is currently called from setup_main in kern/mach_init.c.
 */
init_eventclock()
{
	/*
	 *  Allocate a page on which to put the software maintained bits.
	 */
	event_page = kmem_alloc(kernel_map, PAGE_SIZE);

	event_middle = (unsigned int *) event_page;
	event_high = (unsigned int *) event_page + 1;
	
	*( (unsigned int *) event_page + 2) = 123456789;
}


/*
 *
 *  Our single routine, clockmap, returns the physical page numbers for a given
 *  offset into the device.  This is easy -- we just return the particular page
 *  for the hardware clock if the offset is < pagesize and the offset for the
 *  sw-maintained bits if the offset is between pagesize and 2 * pagesize.
 */
clockmmap(dev, offset, prot)
    dev_t dev;			/* ignore this */
    int offset;			/* use this to determine which page is wanted */
    vm_prot_t prot;		/* return an error if this is != READ */
{    
    if (prot != VM_PROT_READ)
    	return -1;		/* invalid argument */
	
    if (offset < 0)
    	return -1;		/* invalid address */

    if (offset < page_size)
    	return (atop(eventc_latch));
    
    if (offset < page_size * 2)
    	return (atop(pmap_extract(kernel_task->map->pmap, event_page)));
    
    return -1;			/* invalid address */
}

#endif	NeXT && DEBUG


