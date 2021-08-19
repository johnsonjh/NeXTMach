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
 * $Log:	exception.c,v $
 * Revision 2.10  89/12/22  15:52:14  rpd
 * 	Oops, must pass thread to thread_exception_abort!
 * 	[89/12/14            dlb]
 * 
 * Revision 2.9  89/10/11  14:03:38  dlb
 * 	Add thread_exception_abort(), and modify thread_doexception() to
 * 	deal with the RCV_INVALID_PORT code it causes exception_raise to
 * 	return.
 * 	[89/08/17            dlb]
 * 
 * Revision 2.8  89/02/25  18:00:58  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.7  89/01/15  16:21:48  rpd
 * 	Updated includes for the new mach/ directory.
 * 	[89/01/15  14:56:02  rpd]
 * 
 * Revision 2.6  89/01/10  23:26:34  rpd
 * 	Replaced xxx_port_allocate with port_alloc.
 * 	[89/01/10  13:21:21  rpd]
 * 	
 * 	Use object_copyout instead of port_copyout.
 * 	[89/01/09  14:42:54  rpd]
 * 
 * Revision 2.5  88/10/11  10:09:31  rpd
 * 	Changed includes to the new style.
 * 	Changed to use ipc_thread_lock/ipc_thread_unlock.
 * 	[88/10/09  15:50:59  rpd]
 * 
 * Revision 2.4  88/08/06  18:10:03  rpd
 * Eliminated kern/mach_ipc_defs.h.
 * 
 * Revision 2.3  88/07/20  16:29:33  rpd
 * Use old form of port_allocate.
 * 
 * 14-Apr-88  David Kirschen (kirschen) at Encore Computer Corporation
 *      Print exception info before panic'ing on Kernel Thread Exception
 *
 * 28-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Removed exception-port routines; they are superceded by
 *	{thread,task}_{get,set}_special_port.
 *
 *  6-Jan-88  David Black (dlb) at Carnegie-Mellon University
 *	Check for thread halt condition if exception rpc fails.
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Made thread_doexception switch to master before calling uprintf
 *	and exit.  Delinted.
 *
 * 14-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	If rpc fails in thread_doexception, leave ports alone; they will
 *	be cleaned up normally.  Locking protocol rewrite.
 *
 *  8-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Translate global port names (from kernel data structures) to
 *	local port names before use.
 *
 * 30-Nov-87  David Black (dlb) at Carnegie-Mellon University
 *	Rewrote for exc interface.
 *
 * 30-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	Get port references right.
 *
 * 19-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	Removed port_copyout to kernel_task.  mach_ipc.c has been fixed
 *	to allow kernel to send to any port.
 *
 *  1-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	Created
 *
 */

#import <sys/param.h>

#import <sys/boolean.h>
#import <sys/kern_return.h>
#import <sys/message.h>
#import <sys/port.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <sys/user.h>
#import <kern/kern_obj.h>
#import <kern/kern_port.h>

#import <kern/exc.h>
#import <kern/ipc_copyout.h>

#import <kern/parallel.h>		/* Only for u*x_master */

/*
 *	thread_doexception does all the dirty work in actually setting
 *	up to send the exception message.  
 *
 *	XXX Can only be called on current thread, because it may exit().
 */
 
void thread_doexception(thread, exception, code, subcode)
thread_t	thread;
int		exception, code, subcode;
{
	task_t		task;
	port_t		task_port, thread_port, clear_port, exc_port;
	int		signal, junk;	/* XXX for exit() */
	kern_return_t	r;

	if (thread != current_thread())
		panic("thread_doexception: thread is NOT current_thread!");
	thread_port = thread_self();
	task_port = task_self();
	task = thread->task;

	/*
	 *	Allocate an exception_clear port if there isn't one
	 *	already.  Only done by the thread itself.
	 */
	ipc_thread_lock(thread);
	if (thread->exception_clear_port == PORT_NULL) {
		kern_port_t port;

		if (port_alloc(task, &port) != KERN_SUCCESS)
			panic("thread_doexception: alloc clear port");
		port->port_references++; /* ref for saved pointer */
		port_unlock(port);

		thread->exception_clear_port = (port_t) port;
	}

	/*
	 *	Now translate global port name from structure to local
	 *	port name.  Need an extra reference for object_copyout.
	 */
	clear_port = thread->exception_clear_port;
	port_reference((kern_port_t) clear_port);
	object_copyout(task, (kern_obj_t) clear_port,
		       MSG_TYPE_PORT, &clear_port);

	/*
	 *	Set ipc_kernel flag because msg buffers for rpc are in
	 *	kernel space.
	 */
	if (thread->ipc_kernel) {
	        printf("\nException = 0x%x, Code = 0x%x, SubCode = 0x%x\n",
		       exception, code, subcode);
		panic("Kernel thread exception.");
	}
	thread->ipc_kernel = TRUE;

	/*
	 *	Try thread port first.
	 */
	exc_port = thread->exception_port;
	if (exc_port != PORT_NULL) {
	    port_reference(exc_port);
	}
	ipc_thread_unlock(thread);

	if (exc_port != PORT_NULL) {
	    /*
	     *	Translate global name from data structure to local name.
	     */
	    object_copyout(task, (kern_obj_t) exc_port,
			   MSG_TYPE_PORT, &exc_port);

	    if ((r = exception_raise(exc_port, clear_port, thread_port,
				task_port, exception, code, subcode))
		== KERN_SUCCESS) {
		    /*
		     *	Turn off ipc_kernel before returning
		     */
		    thread->ipc_kernel = FALSE;
		    return;
	    }

	    /*
	     *	If RCV_INVALID_PORT is returned, the receive right to
	     *	the clear port has vanished.  Most likely cause is
	     *	thread_exception_abort.  Assume whoever did this
	     *  knows what they're doing and return.  Make sure the
	     *	exception port field in the thread structure is
	     *	cleared (thread_exception_abort() does this).
	     */
	    if (r == RCV_INVALID_PORT) {
		thread->ipc_kernel = FALSE;
		if (thread->exception_clear_port != PORT_NULL) 
			thread_exception_abort(thread);
		return;
	    }
	}

	simple_lock(&task->ipc_translation_lock);
	exc_port = task->exception_port;
	if (exc_port != PORT_NULL) {
	    port_reference(exc_port);
	}
	simple_unlock(&task->ipc_translation_lock);

	if (exc_port != PORT_NULL) {

	    /*
	     *	Translate global name from data structure to local name.
	     */
	    object_copyout(task, (kern_obj_t) exc_port,
			   MSG_TYPE_PORT, &exc_port);

	    if ((r = exception_raise(exc_port, clear_port, thread_port,
				task_port, exception, code, subcode))
		== KERN_SUCCESS) {
		    /*
		     *	Reset ipc_kernel flag before returning.
		     */
		    thread->ipc_kernel = FALSE;
		    return;
	    }

	    /*
	     *	See above comment on RCV_INVALID_PORT.
	     */
	    if (r == RCV_INVALID_PORT) {
		thread->ipc_kernel = FALSE;
		if (thread->exception_clear_port != PORT_NULL) 
			thread_exception_abort(thread);
		return;
	    }
	}
	/*
	 *	Failed to send outgoing message, reset ipc_kernel flag.
	 */
	thread->ipc_kernel = FALSE;

	/*
	 *	If this thread is being terminated, cooperate.
	 */
	while (thread_should_halt(thread))
		thread_halt_self();

	/*
	 *	All else failed; terminate task.
	 */

#if	1
	/*
	 *	Unfortunately, the rest of this is U*X code, since
	 *	task_terminate doesn't work on a U*X process yet.
	 */

	unix_master();		/* rest of this code is u*x */

	uprintf("Exception %d %d %d; no exception port, terminating task\n",
		exception, code, subcode);  /* for debugging */
	ux_exception(exception, code, subcode, &signal, &junk);
	exit(signal);	/* XXX should be task_terminate(task) */
#else	1
	(void) task_terminate(current_task());
	thread_halt_self();
#endif	1
	/*NOTREACHED*/
}

/*
 *	thread_exception_abort:
 *
 *	Abort the exception receive of a thread waiting in the kernel
 *	by deallocating the exception_clear_port.
 */
kern_return_t thread_exception_abort(thread)
register thread_t thread;
{
	register port_t	exc_clear;

	ipc_thread_lock(thread);
	exc_clear = thread->exception_clear_port;
	thread->exception_clear_port = PORT_NULL;
	ipc_thread_unlock(thread);

	if (exc_clear == PORT_NULL) {
		/*
		 *	Thread couldn't be waiting in an exception
		 *	because it didn't have a port to wait on!
		 */
		return(KERN_FAILURE);
	}

	/*
	 *	Get rid of the port as well as the reference to it
	 *	held by the thread structure.  Ok to use thread->task
	 *	because caller must have a reference to thread.
	 */
	(void) port_dealloc(thread->task, (kern_port_t) exc_clear);
	port_release((kern_port_t) exc_clear);

	return(KERN_SUCCESS);
}

