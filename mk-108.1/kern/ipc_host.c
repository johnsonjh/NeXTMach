/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	ipc_host.c,v $
 * Revision 2.4  89/12/22  15:52:20  rpd
 * 	Take out extra reference on new processor set ports in
 * 	ipc_pset_init for reply message; these ports are now
 * 	returned untranslated.  Assume caller of ipc_pset_disable
 * 	has pset locked as well as referenced.
 * 	[89/12/15            dlb]
 * 
 * Revision 2.3  89/10/15  02:04:29  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:07:11  dlb
 * 	Fix includes.
 * 	Remove interrupt protection from pset locks.
 * 
 * Revision 2.1.1.4  89/08/02  22:55:15  dlb
 * 	Move includes.
 * 	[89/08/02            dlb]
 * 
 * Revision 2.1.1.3  89/07/25  15:45:02  dlb
 * 	Remove interrupt protection from pset locks.
 * 	[89/06/14            dlb]
 * 
 * Revision 2.1.1.2  89/02/24  21:57:07  dlb
 * 	Use port_alloc instead of xxx_port_allocate.
 * 	[89/02/21            dlb]
 * 
 * Revision 2.1.1.1  89/01/30  17:00:56  dlb
 * 	Reformat includes.
 * 	[89/01/26            dlb]
 * 	
 * 	Break processor_set_default into two pieces.
 * 	[88/12/21            dlb]
 * 	
 * 	Move host_self, host_priv_self to ipc_ptraps.c
 * 	Rewrite processor_set_default to return both ports
 * 	[88/11/30            dlb]
 * 	
 * 	Created.
 * 	[88/10/29            dlb]
 * 
 */

/*
 *	kern/ipc_host.c
 *
 *	Routines to implement host ports.
 */

#import <kern/host.h>
#import <kern/kern_port.h>
#import <sys/message.h>
#import <kern/port_object.h>
#import <kern/processor.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/ipc_host.h>

#import <machine/machparam.h>

/*
 *	ipc_host_init: set up various things.
 */

void ipc_host_init()
{
	kern_port_t	port;
	/*
	 *	Allocate and set up the two host ports.
	 */
	if (port_alloc(kernel_task, &port) != KERN_SUCCESS)
		panic("ipc_host_init: host port allocate");
	port->port_references++;
	port_unlock(port);
	realhost.host_self = (port_t) port;
	port_object_set(port, PORT_OBJECT_HOST, &realhost);

	if (port_alloc(kernel_task, &port) != KERN_SUCCESS)
		panic("ipc_host_init: host priv port allocate");
	port->port_references++;
	port_unlock(port);
	realhost.host_priv_self = (port_t) port;
	port_object_set(port, PORT_OBJECT_HOST_PRIV, &realhost);

	/*
	 *	Set up ipc for default processor set.
	 */
	ipc_pset_init(&default_pset);
	ipc_pset_enable(&default_pset);

	/*
	 *	And for master processor
	 */
	ipc_processor_init(master_processor);
	ipc_processor_enable(master_processor);
}
/*
 *	ipc_processor_init:
 *
 *	Initialize ipc access to processor by allocating port.
 */

void
ipc_processor_init(processor)
processor_t	processor;
{
	kern_port_t	port;

	if (port_alloc(kernel_task, &port) != KERN_SUCCESS)
		panic("ipc_processor_init: port allocate");
	port->port_references++;
	port_unlock(port);
	processor->processor_self = (port_t) port;
}

/*
 *	ipc_processor_enable:
 *
 *	Enable ipc control of processor by setting port object.
 */
void
ipc_processor_enable(processor)
processor_t	processor;
{
	kern_port_t	myport;

	myport = (kern_port_t) processor->processor_self;
	port_lock(myport);
	port_object_set(myport, PORT_OBJECT_PROCESSOR, processor);
	port_unlock(myport);
}

/*
 *	ipc_processor_disable:
 *
 *	Disable ipc control of processor by clearing port object.
 */
void
ipc_processor_disable(processor)
processor_t	processor;
{
	kern_port_t	myport;

	if ((myport = (kern_port_t)processor->processor_self) ==
	    KERN_PORT_NULL) {
		return;
	}
	port_lock(myport);
	port_object_set(myport, PORT_OBJECT_NONE, 0);
	port_unlock(myport);
}
	
/*
 *	ipc_processor_terminate:
 *
 *	Processor is off-line.  Destroy ipc control port.
 */
void
ipc_processor_terminate(processor)
processor_t	processor;
{
	kern_port_t	myport;
	int	s;

	s = splsched();
	processor_lock(processor);
	if (processor->processor_self == PORT_NULL) {
		processor_unlock(processor);
		(void) splx(s);
		return;
	}

	myport = (kern_port_t) processor->processor_self;
	processor->processor_self = PORT_NULL;
	processor_unlock(processor);
	(void) splx(s);

	port_release(myport);
	(void)port_dealloc(kernel_task, myport);
}
	
/*
 *	ipc_pset_init:
 *
 *	Initialize ipc control of a processor set by allocating its ports.
 *	Takes out two references on each port; one for the structure pointer
 *	and one for the reply message (these ports aren't translated
 *	by MiG in the reply to processor_set_create).
 */

void
ipc_pset_init(pset)
processor_set_t	pset;
{
	kern_port_t	port;

	if (port_alloc(kernel_task, &port) != KERN_SUCCESS)
		panic("ipc_pset_init: pset port allocate");
	port->port_references += 2;
	port_unlock(port);
	pset->pset_self = (port_t) port;

	if (port_alloc(kernel_task, &port) != KERN_SUCCESS)
		panic("ipc_pset_init: name port allocate");
	port->port_references += 2;
	port_unlock(port);
	pset->pset_name_self = (port_t) port;
}

/*
 *	ipc_pset_enable:
 *
 *	Enable ipc access to a processor set.
 */
void
ipc_pset_enable(pset)
processor_set_t	pset;
{
	register kern_port_t	myport;

	pset_lock(pset);
	if (pset->active) {
		myport = (kern_port_t)pset->pset_self;
		port_lock(myport);
		port_object_set(myport, PORT_OBJECT_PSET, pset);
		port_unlock(myport);
		myport = (kern_port_t) pset->pset_name_self;
		port_lock(myport);
		port_object_set(myport, PORT_OBJECT_PSET_NAME, pset);
		port_unlock(myport);
		pset->ref_count += 2;
	}
	pset_unlock(pset);
}

/*
 *	ipc_pset_disable:
 *
 *	Disable ipc access to a processor set by clearing the port objects.
 *	Caller must hold pset lock and a reference to the pset.  Ok to
 *	just decrement pset reference count as a result.
 */
void
ipc_pset_disable(pset)
processor_set_t	pset;
{
	kern_port_t	myport;

	myport = (kern_port_t) pset->pset_self;
	port_lock(myport);
	if (myport->port_object.kp_type == PORT_OBJECT_PSET) {
		port_object_set(myport, PORT_OBJECT_NONE, 0);
		pset->ref_count -= 1;
	}
	port_unlock(myport);
	myport = (kern_port_t) pset->pset_name_self;
	port_lock(myport);
	if (myport->port_object.kp_type == PORT_OBJECT_PSET_NAME) {
		port_object_set(myport, PORT_OBJECT_NONE, 0);
		pset->ref_count -= 1;
	}
	port_unlock(myport);
}

/*
 *	ipc_pset_terminate:
 *
 *	Processor set is dead.  Deallocate the ipc control structures.
 */
void
ipc_pset_terminate(pset)
processor_set_t	pset;
{

	port_release((kern_port_t) pset->pset_self);
	port_release((kern_port_t) pset->pset_name_self);
	port_dealloc(kernel_task, (kern_port_t) pset->pset_self);
	port_dealloc(kernel_task, (kern_port_t) pset->pset_name_self);
}

/*
 *	processor_set_default, processor_set_default_priv:
 *
 *	Return ports for manipulating default_processor set.  MiG code
 *	differentiates between these two routines.
 */
kern_return_t
processor_set_default(host, pset)
host_t	host;
processor_set_t	*pset;
{

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);

	*pset = &default_pset;
	return(KERN_SUCCESS);
}

kern_return_t
xxx_processor_set_default_priv(host, pset)
host_t	host;
processor_set_t	*pset;
{

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);

	*pset = &default_pset;
	return(KERN_SUCCESS);
}

