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
 * $Log:	task_special_ports.h,v $
 * Revision 2.5  89/06/27  00:25:33  rpd
 * 	Added task_get_kernel_port, task_set_kernel_port.
 * 	[89/06/27  00:02:52  rpd]
 * 
 * Revision 2.4  89/03/09  20:24:06  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:41:12  gm0w
 * 	Changes for cleanup.
 * 
 * 17-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 *	File:	sys/task_special_ports.h
 *
 *	Defines codes for special_purpose task ports.  These are NOT
 *	port identifiers - they are only used for the task_get_special_port
 *	and task_set_special_port routines.
 *	
 */

#ifndef	_SYS_TASK_SPECIAL_PORTS_H_
#define _SYS_TASK_SPECIAL_PORTS_H_

#define TASK_KERNEL_PORT	1	/* Represents task to the outside
					   world.*/
#define TASK_NOTIFY_PORT	2	/* Task receives kernel IPC
					   notifications here. */
#define TASK_EXCEPTION_PORT	3	/* Exception messages for task are
					   sent to this port. */
#define TASK_BOOTSTRAP_PORT	4	/* Bootstrap environment for task. */

/*
 *	Definitions for ease of use
 */

#define task_get_kernel_port(task, port)	\
		(task_get_special_port((task), TASK_KERNEL_PORT, (port)))

#define task_set_kernel_port(task, port)	\
		(task_set_special_port((task), TASK_KERNEL_PORT, (port)))

#define task_get_notify_port(task, port)	\
		(task_get_special_port((task), TASK_NOTIFY_PORT, (port)))

#define task_set_notify_port(task, port)	\
		(task_set_special_port((task), TASK_NOTIFY_PORT, (port)))

#define task_get_exception_port(task, port)	\
		(task_get_special_port((task), TASK_EXCEPTION_PORT, (port)))

#define task_set_exception_port(task, port)	\
		(task_set_special_port((task), TASK_EXCEPTION_PORT, (port)))

#define task_get_bootstrap_port(task, port)	\
		(task_get_special_port((task), TASK_BOOTSTRAP_PORT, (port)))

#define task_set_bootstrap_port(task, port)	\
		(task_set_special_port((task), TASK_BOOTSTRAP_PORT, (port)))

#endif	_SYS_TASK_SPECIAL_PORTS_H_


