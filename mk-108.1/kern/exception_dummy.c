/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 ***********************************************************************
 * HISTORY
 * 13-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	Created.
 *
 ***********************************************************************
 */

#import <sys/kern_return.h>
#import <sys/port.h>
#import <sys/task.h>
#import <sys/thread.h>

/*
 *	File containing "not implemented" definitions of kernel exception
 *	routines for non-MACH_EXCEPTION kernels.  Dummy pending
 *	standardization of MACH_EXCEPTION.
 */

kern_return_t task_set_exception_port(task,exception_port)
task_t	task;
port_t	exception_port;
{
#ifdef	lint
	task++;
	exception_port++
#endif	lint
	uprintf("task_set_exception_port: not implemented.\n");
	return(KERN_FAILURE);
}

kern_return_t task_get_exception_port(task,exception_port)
task_t	task;
port_t	*exception_port;
{
#ifdef	lint
	task++;
	*exception_port++;
#endif	lint
	uprintf("task_get_exception_port: not implemented.\n");
	return(KERN_FAILURE);
}


kern_return_t thread_set_exception_port(thread,exception_port)
thread_t	thread;
port_t		exception_port;
{
#ifdef	lint
	thread++;
	exception_port++;
#endif	lint
	uprintf("thread_set_exception_port: not implemented.\n");
	return(KERN_FAILURE);
}


kern_return_t thread_get_exception_port(thread,exception_port)
thread_t	thread;
port_t		*exception_port;
{
#ifdef	lint
	thread++;
	*exception_port++;
#endif	lint
	uprintf("thread_get_exception_port: not implemented.\n");
	return(KERN_FAILURE);
}
