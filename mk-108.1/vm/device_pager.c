/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 *	File:	vm/device_pager.c
 *
 *	Provide paging objects for memory-mapped devices.
 */

/*
 * HISTORY
 * 16-Apr-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Add stubs for unused routines to avoid lint.
 *
 * 21-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Pass the pages' physical addresses in vm_page_init().
 *
 * 18-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Delinted.
 *
 *  8-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Created.
 */

#import <vm/vm_pager.h>
#import <sys/port.h>
#import <machine/vm_types.h>
#import <vm/vm_param.h>
#import <vm/vm_page.h>
#import <vm/vm_object.h>
#import <kern/lock.h>
#import <kern/zalloc.h>
#import <kern/task.h>
#import <vm/vm_kern.h>
#import <kern/ipc_globals.h>
/* Something above pulls in sys/types.h... I'm morally
 * opposed to bringing it in merely to get dev_t.
 */

/*
 *	Type:		dev_pager_t
 *	Synopsis:	Descriptor for a device paging object.
 *
 *	Implementation:
 *		One device_pager record is kept for each active
 *		device paging object; all such records are kept
 *		in a singly-linked list.
 */
typedef struct dev_pager {
	struct dev_pager *next;		/* Next in list */
	vm_pager_t	pager;		/* Paging port */
	port_t		pager_request;	/* Pager request port */

	/*
	 *	Arguments passed to device_pager_create to be
	 *	use to do the mapping.
	 */

	dev_t		dev;
	int		(*mapfun)();
	int		prot;
	vm_offset_t	offset;
	vm_size_t	size;

	/*
	 *	State associated with this object
	 */

	struct vm_page	*pages;		/* Page structures allocated */
	vm_size_t	pages_size;	/* ... amount thereof */
} *dev_pager_t;

#define		DEV_PAGER_NULL	((dev_pager_t) 0)

simple_lock_data_t device_pager_lock;
dev_pager_t	device_pager_list = DEV_PAGER_NULL;
zone_t		device_pager_zone;

/*
 *	Other state variables
 */

task_t		device_pager_task;
port_t		device_pager_self;

/*
 *	Routine:	device_pager_init
 *	Function:
 *		Initialize the device pager data structures.
 *	Arguments:
 *		The single argument names the task that
 *		has been created to run the device_pager.
 *	Note:
 *		This call does not operate in the context of
 *		the device_pager task.
 *	In/out conditions:
 *		This routine must be called before any threads
 *		in the device_pager task are started.
 */
void		device_pager_init(task)
	task_t		task;
{
	device_pager_task = task;
	device_pager_task->reply_port = PORT_NULL;

	device_pager_zone = zinit(sizeof(struct dev_pager),
				100 * sizeof(struct dev_pager),
				sizeof(struct dev_pager),
				FALSE,
				"device pager structures");
	device_pager_list = DEV_PAGER_NULL;
	simple_lock_init(&device_pager_lock);
}

/*
 *	Routine:	device_pager_create
 *	Function:
 *		Returns a pager which can be vm_allocate_with_pager'd
 *		to map the specified device.
 *
 *	In/out conditions:
 *		Returns a reference to the port in question.
 *
 *	Implementation:
 *		Create a pager and record the arguments for
 *		later use.  The pager_init() handler does all of the
 *		real work.
 *	Note:
 *		This routine does not run in the context of the
 *		device_pager task.
 */
vm_pager_t	device_pager_create(dev, mapfun, prot, offset, size)
	dev_t		dev;
	int		(*mapfun)();
	int		prot;
	vm_offset_t	offset;
	vm_size_t	size;
{
	dev_pager_t	d;
	vm_pager_t	result;
	port_t		pager;

	/*
	 *	Allocate a structure to hold the arguments
	 *	and port to represent this object.
	 */

	d = (dev_pager_t) zalloc(device_pager_zone);
	if (port_allocate(device_pager_task, &result) != KERN_SUCCESS)
		return(PORT_NULL);
	if (port_enable(device_pager_task, result) != KERN_SUCCESS)
		panic("device_pager_create: cannot enable");

	pager = result;
	port_reference(pager);
	port_copyout(device_pager_task, &pager, MSG_TYPE_PORT);
	
	/*
	 *	Insert the descritor into the list
	 */

	simple_lock(&device_pager_lock);
	d->next = device_pager_list;
	device_pager_list = d;
	d->pager = pager;
	simple_unlock(&device_pager_lock);


	/*
	 *	Record our arguments
	 */

	d->dev = dev;
	d->mapfun = mapfun;
	d->prot = prot;
	d->offset = offset;
	d->size = size;
	d->pager_request = PORT_NULL;

	return(result);
}

/*
 *	IMPORTANT NOTE:
 *		The pager interface functions are implemented below
 *		for the device pager.
 *
 *		Note that the routine names have all been changed to
 *		avoid confusion with the versions *used* by the kernel.
 *
 *		The only function of real importance to the device
 *		pager is initialization.  Data requests can occur, but
 *		are ignored, as initialization will provide the entire
 *		range of valid data.  Other interface functions should
 *		never be exercised; they're declared here for completeness
 *		(i.e., to make "lint" happy).
 */

#import <kern/mach_user_internal.h>

#define		pager_init		device_pager_init_pager
#define		pager_data_request	device_pager_data_request
#define		pager_data_unlock	device_pager_data_unlock
#define		pager_data_write	device_pager_data_write
#define		pager_lock_completed	device_pager_lock_completed

kern_return_t pager_data_request(paging_object,pager_request_port,offset,length,desired_access)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	vm_size_t length;
	vm_prot_t desired_access;
{
#if	defined(lint) || defined(hc)
	paging_object++; pager_request_port++; offset++; length++; desired_access++;
#endif	defined(lint) || defined(hc)

	/*
	 *	It's quite possible that a page fault occurs before
	 *	initialization is handled.  We merely throw away any
	 *	pending pageins.
	 */

	return(KERN_SUCCESS);
}

kern_return_t pager_data_unlock(paging_object,pager_request_port,offset,length,desired_access)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	vm_size_t length;
	vm_prot_t desired_access;
{
#if	defined(lint) || defined(hc)
	paging_object++; pager_request_port++; offset++; length++; desired_access++;
#endif	defined(lint) || defined(hc)
	printf("(device_pager)pager_data_unlock: called!\n");
	return(KERN_FAILURE);
}

kern_return_t pager_data_write(paging_object,pager_request_port,offset,data,dataCnt)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	/* pointer_t */ int data;
	unsigned int dataCnt;
{
#if	defined(lint) || defined(hc)
	paging_object++; pager_request_port++; offset++; data++; dataCnt++;
#endif	defined(lint) || defined(hc)
	printf("(device_pager)pager_data_write: called!\n");
	return(KERN_FAILURE);
}

kern_return_t pager_lock_completed(paging_object,pager_request_port,offset,length)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	vm_size_t length;
{
#if	defined(lint) || defined(hc)
	paging_object++; pager_request_port++; offset++; length++;
#endif	defined(lint) || defined(hc)
	printf("(device_pager)pager_lock_completed: called!\n");
	return(KERN_FAILURE);
}

kern_return_t	pager_init(pager, pager_request, pager_name, pager_page_size)
	port_t		pager;
	port_t		pager_request;
	port_t		pager_name;
	vm_size_t	pager_page_size;
{
	dev_pager_t	d;
	vm_object_t	object;
	vm_offset_t	offset;
	int		num_pages;
	vm_page_t	pg_cur;
	int		i;
	port_t		pager_global;

	if (pager_page_size != PAGE_SIZE)
		panic("(device_pager)pager_init: wrong page size");

	/*
	 *	Find the mapping arguments associated with this object
	 */

	simple_lock(&device_pager_lock);
	for (d = device_pager_list; d != DEV_PAGER_NULL; d = d->next)
		if (d->pager == pager)
			break;
	simple_unlock(&device_pager_lock);

	if (d == DEV_PAGER_NULL)
		panic("(device_pager)pager_init: bad pager");

	/*
	 *	Record the request port
	 */

	if (d->pager_request != PORT_NULL)
		panic("(device_pager)pager_init: already initialized!");

	d->pager_request = pager_request;
#ifdef	lint
	pager_name++;
#endif	lint

	/*
	 *	Look for the object into which we will jam
	 *	physical pages.
	 */

	pager_global = pager_request;
	port_copyin(device_pager_task, &pager_global, MSG_TYPE_PORT, FALSE);
	port_release(pager_global);
	if ((object = vm_object_lookup(pager_global)) == VM_OBJECT_NULL) {
		/*
		 *	Termination has already occurred or
		 *	someone did something bogus with our
		 *	pager port.
		 *
		 *	We can safely ignore initialization
		 *	in either case.
		 */
		d->pages_size = 0;
		return(KERN_FAILURE);
	}

	/*
	 *	Grab enough memory for vm_page structures to
	 *	represent all pages in the object, and a little
	 *	bit more for a handle on them.
	 */
	num_pages = atop(d->size);
	d->pages_size = round_page(num_pages * sizeof(struct vm_page));
	d->pages = (vm_page_t) kmem_alloc(kernel_map, d->pages_size);

	/*
	 *	Fill in each page with the proper physical address,
	 *	and attach it to the object.  Mark it as wired-down
	 *	to keep the pageout daemon's hands off it.
	 */

	for (i = 0, pg_cur = d->pages, offset = d->offset; i < num_pages;
			i++, pg_cur++, offset += PAGE_SIZE) {
		vm_offset_t	addr;
		vm_page_t	old_page;

		/*
		 *	The mapping function takes a byte offset, but returns
		 *	a machine-dependent page frame number.  We convert
		 *	that into something that the pmap module will
		 *	accept later.
		 */

		addr = pmap_phys_address(
		   (*(d->mapfun))(d->dev, offset, d->prot));

		/*
		 *	Insert a page with the appropriate physical
		 *	address.  [In the even that a thread using this data
		 *	gets to it before we initialize, it will have
		 *	entered a page and waited on it.  We throw it away,
		 *	causing that thread to be awakened.  We'll later
		 *	ignore the pagein request.]
		 */

		vm_object_lock(object);
		if ((old_page = vm_page_lookup(object, offset)) != VM_PAGE_NULL) {
			vm_page_lock_queues();
			vm_page_free(old_page);
			vm_page_unlock_queues();
		}
		/* XXX This is very close to vm_page_replace */

		vm_page_init(pg_cur, object, offset, addr);
  		pg_cur->wire_count = 1;
		pg_cur->fictitious = TRUE;	/* Page structure is ours */
		PAGE_WAKEUP(pg_cur);
		vm_object_unlock(object);
	}
	
	vm_object_deallocate(object);

	return(KERN_SUCCESS);
}


/*
 *	Deallocate the page structures allocated for a device pager
 */
void		device_pager_terminate(port)
	port_t		port;
{
	dev_pager_t	d, p;

	/*
	 *	Find this pager
	 */

	simple_lock(&device_pager_lock);
	for (d = device_pager_list; d != DEV_PAGER_NULL; p = d, d = d->next)
		if (d->pager_request == port)
			break;

	if (d == DEV_PAGER_NULL) {
		/*
		 *	We don't recognize this port.
		 *
		 *	It's probably a pager's "name" port, which
		 *	we don't record.
		 */

		simple_unlock(&device_pager_lock);
		return;
	}

	/*
	 *	Remove this pager from the list.
	 */

	if (d == device_pager_list)
		device_pager_list = d->next;
	 else
	 	p->next = d->next;

	simple_unlock(&device_pager_lock);
	

	/*
	 *	Free the memory allocated for the page structures
	 *	and free the device pager structure.
	 */

	kmem_free(kernel_map, (vm_offset_t) d->pages, d->pages_size);
	port_deallocate(device_pager_self, d->pager);
	zfree(device_pager_zone, (vm_offset_t) d);
}

#define	SERVER_LOOP		device_pager_server_loop
#define	SERVER_NAME		"device_pager"
#define	SERVER_DISPATCH(in,out)	device_pager_server(in, out)
#define	TERMINATE_FUNCTION	device_pager_terminate
#define	pager_server		device_pager_server

#import <kern/server_loop.c>
#import <kern/pager_server.c>

void		device_pager()
{
	SERVER_LOOP();
}

