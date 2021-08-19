/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	mach_types.h,v $
 * 19-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Added THREAD_NULL and friends.
 *
 * Revision 2.10  90/07/20  08:49:00  mrt
 * 	processor_set_array_t --> processor_set_name_array_t.
 * 	[90/07/13            dlb]
 * 
 * 22-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Added typedef of vm_task_t for kernel use.
 *
 * Revision 2.8  89/10/11  14:39:45  dlb
 * 	Kernel monitor support.
 * 	Host, processor, and processor set support.
 * 
 * Revision 2.7  89/05/21  22:45:54  mrt
 * 	Moved the in-kernel conversion functions to 
 * 	kern/type_conversion.h and removed the non-kernel
 * 	include of mach_init.h. It is now included in the user
 * 	level mach.h
 * 	[89/05/20            mrt]
 * 
 * 
 * Revision 2.6  89/05/06  02:58:04  rpd
 * 	Include <mach/std_types.h> for backwards compatibility.
 * 	[89/05/05  21:00:43  rpd]
 * 
 * Revision 2.5  89/04/08  23:40:37  rpd
 * 	Removed the memory_object/msg_send_from_kernel hack.
 * 	[89/04/08  22:56:08  rpd]
 * 
 * Revision 2.4  89/03/09  20:20:59  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:38:04  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  89/01/15  16:30:50  rpd
 * 	Moved from kern/ to mach/.
 * 	[89/01/15  14:35:53  rpd]
 * 
 * Revision 2.10  89/01/12  11:15:18  rpd
 * 	Removed pointer_t declaration; it belongs in std_types.h.
 * 
 * Revision 2.9  89/01/12  07:57:53  rpd
 * 	Moved basic stuff to std_types.h.  Removed debugging definitions.
 * 	Moved io_buf definitions to device_types.h.
 * 	[89/01/12  04:51:54  rpd]
 * 
 * Revision 2.8  89/01/04  13:37:34  rpd
 * 	Include <kern/fpa_counters.h>, for fpa_counters_t.
 * 	[89/01/01  15:03:52  rpd]
 * 
 * Revision 2.7  88/09/25  22:15:28  rpd
 * 	Changed sys/callout.h to kern/callout_statistics.h.
 * 	[88/09/09  14:00:19  rpd]
 * 	
 * 	Changed includes to the new style.
 * 	Added include of sys/callout.h.
 * 	[88/09/09  04:47:42  rpd]
 * 
 * Revision 2.6  88/08/06  18:22:34  rpd
 * Changed sys/mach_ipc_netport.h to kern/ipc_netport.h.
 * 
 * Revision 2.5  88/07/21  00:36:06  rpd
 * Added include of ipc_statistics.h.
 * 
 * Revision 2.4  88/07/17  19:33:20  mwyoung
 * *** empty log message ***
 * 
 * 29-Jun-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Use new <mach/memory_object.h>.
 *
 *  9-Apr-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Changed mach_ipc_vmtp.h to mach_ipc_netport.h.
 *
 *  1-Mar-88  Mary Thompson (mrt) at Carnegie Mellon
 *	Added a conditional on _MACH_INIT_ before the include
 *	of mach_init.h so that the kernel make of mach_user_internal
 *	would not include mach_init.h
 *
 * 18-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Added includes of task_info, thread_info, task_special_ports,
 *	thread_special_ports for new interfaces.
 *
 * 12-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Reduced old history.
 */
/*
 *	File:	kern/mach_types.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Mach external interface definitions.
 *
 */

#ifndef	_KERN_MACH_TYPES_H_
#define _KERN_MACH_TYPES_H_

#import <sys/port.h>
#import <vm/vm_inherit.h>
#import <vm/vm_prot.h>
#import <vm/memory_object.h>
#import <vm/vm_statistics.h>
#import <machine/vm_types.h>
#import <sys/machine.h>
#import <sys/thread_status.h>
#import <sys/thread_info.h>
#import <sys/thread_special_ports.h>
#import <sys/task_info.h>
#import <sys/task_special_ports.h>
#import <sys/netport.h>

#import <sys/host_info.h>
#import <sys/processor_info.h>

#ifdef	KERNEL
#import <kern/task.h>
#import <kern/thread.h>

#import <vm/vm_user.h>
#import <vm/vm_object.h>

#import <kern/host.h>
#import <kern/processor.h>
typedef vm_map_t	vm_task_t;
#else	KERNEL
typedef	port_t		task_t;
typedef	task_t		vm_task_t;
typedef port_t		*task_array_t;
typedef	port_t		thread_t;
typedef port_t		monitor_t;
typedef	thread_t	*thread_array_t;

typedef port_t		host_t;
typedef port_t		host_priv_t;
typedef port_t		processor_t;
typedef port_t		*processor_array_t;
typedef port_t		processor_set_t;
typedef port_t		processor_set_name_t;
typedef port_t		*processor_set_name_array_t;

#if	NeXT
/*
 * Define NULL types here.
 */
#define	HOST_NULL		(host_t)0
#define	PROCESSOR_SET_NULL	(processor_set_t)0
#define	PROCESSOR_NULL		(processor_t)0
#define	TASK_NULL		(task_t)0
#define	THREAD_NULL		(thread_t)0
#endif	NeXT
#endif	KERNEL

/*
 *	Backwards compatibility, for those programs written
 *	before mach/{std,mach}_types.{defs,h} were set up.
 */
#import <kern/std_types.h>


typedef	vm_offset_t	vm_address_t;
typedef	unsigned int	vm_region_t;
typedef	vm_region_t	*vm_region_array_t;

typedef	char		vm_page_data_t[4096];

#endif	_KERN_MACH_TYPES_H_
