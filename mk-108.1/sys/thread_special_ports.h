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
 * $Log:	thread_special_ports.h,v $
 * Revision 2.5  89/06/27  00:25:37  rpd
 * 	Added thread_get_kernel_port, thread_set_kernel_port.
 * 	[89/06/27  00:03:21  rpd]
 * 
 * Revision 2.4  89/03/09  20:24:17  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:41:23  gm0w
 * 	Changes for cleanup.
 * 
 * 17-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 *	File:	sys/thread_special_ports.h
 *
 *	Defines codes for special_purpose thread ports.  These are NOT
 *	port identifiers - they are only used for the thread_get_special_port
 *	and thread_set_special_port routines.
 *	
 */

#ifndef	_SYS_THREAD_SPECIAL_PORTS_H_
#define _SYS_THREAD_SPECIAL_PORTS_H_

#define THREAD_KERNEL_PORT	1	/* Represents the thread to the outside
					   world.*/
#define THREAD_REPLY_PORT	2	/* Default reply port for the thread's
					   use. */
#define THREAD_EXCEPTION_PORT	3	/* Exception messages for the thread
					   are sent to this port. */

/*
 *	Definitions for ease of use
 */

#define thread_get_kernel_port(thread, port)	\
		(thread_get_special_port((thread), THREAD_KERNEL_PORT, (port)))

#define thread_set_kernel_port(thread, port)	\
		(thread_set_special_port((thread), THREAD_KERNEL_PORT, (port)))

#define thread_get_reply_port(thread, port)	\
		(thread_get_special_port((thread), THREAD_REPLY_PORT, (port)))

#define thread_set_reply_port(thread, port)	\
		(thread_set_special_port((thread), THREAD_REPLY_PORT, (port)))

#define thread_get_exception_port(thread, port)	\
		(thread_get_special_port((thread), THREAD_EXCEPTION_PORT, (port)))

#define thread_set_exception_port(thread, port)	\
		(thread_set_special_port((thread), THREAD_EXCEPTION_PORT, (port)))

#endif	_SYS_THREAD_SPECIAL_PORTS_H_

