/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	type_conversion.h,v $
 * Revision 2.3  89/10/11  14:35:34  dlb
 * 	Add host, processor, processor_set, monitor definitions.
 * 	[89/08/03            dlb]
 * 
 * Revision 2.2.2.1  89/08/03  17:33:04  dlb
 * 	Add host, processor, processor_set, monitor definitions.
 * 	[89/08/03            dlb]
 * 
 * Revision 2.2  89/05/21  22:29:43  mrt
 * 	Created by moving the kernel-only definitions out of
 * 	mach_types.h.
 * 	[89/05/18            mrt]
 * 
 */
#ifndef	_SYS_TYPE_CONVERSION_H_
#define	_SYS_TYPE_CONVERSION_H_

#import <sys/port.h>
#import <kern/task.h> 
#import <kern/thread.h>
#import <vm/vm_map.h>
#import <kern/host.h>
#import <kern/processor.h>

/*
 *	Conversion routines, to let Matchmaker do this for
 *	us automagically.
 */

extern task_t convert_port_to_task( /* port_t x */ );
extern thread_t convert_port_to_thread( /* port_t x */ );
extern vm_map_t convert_port_to_map( /* port_t x */ );
extern port_t convert_task_to_port( /* task_t x */ );
extern port_t convert_thread_to_port( /* thread_t x */ );

extern host_t convert_port_to_host( /* port_t x */ );
extern host_t convert_port_to_host_priv( /* port_t x */ );
extern processor_t convert_port_to_processor( /* port_t x */ );
extern processor_set_t convert_port_to_pset( /* port_t x */ );
extern processor_set_t convert_port_to_pset_name( /* port_t x */ );
extern port_t convert_host_to_port( /* host_t x */ );
extern port_t convert_processor_to_port( /* processor_t x */ );
extern port_t convert_pset_to_port( /* processor_set_t x */ );
extern port_t convert_pset_name_to_port( /* processor_set_t x */ );

#endif	_SYS_TYPE_CONVERSION_H_


