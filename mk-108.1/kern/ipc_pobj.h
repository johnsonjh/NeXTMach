/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	ipc_pobj.h,v $
 * Revision 2.6  89/10/11  14:08:00  dlb
 * 	Add host, processor, processor_set conversion routines.
 * 	[89/01/25            dlb]
 * 
 * Revision 2.5  89/03/09  20:12:28  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:03:27  gm0w
 * 	Kernel code cleanup.	
 * 	Put entire file under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.3  88/09/25  22:12:26  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:18:35  rpd]
 * 
 * Revision 2.2  88/08/06  18:18:47  rpd
 * Hacked convert_{task,thread}_to_port to return port_t, to agree with usage
 * everywhere, although they really return kern_port_t.
 * Added include of kern/kern_port.h.
 * Created.
 * 
 */ 

#ifndef	_KERN_IPC_POBJ_H_
#define _KERN_IPC_POBJ_H_

#import <kern/host.h>
#import <sys/port.h>
#import <kern/port_object.h>
#import <kern/processor.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <vm/vm_map.h>

extern void port_object_set();
extern int port_object_get();
extern port_object_type_t port_object_type();

extern task_t convert_port_to_task();
extern thread_t convert_port_to_thread();
extern vm_map_t convert_port_to_map();
extern port_t convert_task_to_port();
extern port_t convert_thread_to_port();

extern host_t convert_port_to_host();
extern host_t convert_port_to_host_priv();
extern processor_t convert_port_to_processor();
extern processor_set_t convert_port_to_pset();
extern processor_set_t convert_port_to_pset_name();
extern port_t convert_host_to_port();
extern port_t convert_processor_to_port();
extern port_t convert_pset_to_port();
extern port_t convert_pset_name_to_port();

#endif	_KERN_IPC_POBJ_H_

