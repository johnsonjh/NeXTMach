/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 *********************************************************************
 * HISTORY
 * $Log:	ux_exception.c,v $
 * Revision 2.6  88/09/25  22:16:55  rpd
 * 	Changed includes to the new style.
 * 	Changed to use object_copyin instead of port_copyin,
 * 	and eliminated use of PORT_INVALID.
 * 	[88/09/24  18:14:52  rpd]
 * 
 * Revision 2.5  88/08/06  19:22:38  rpd
 * Eliminated use of kern/mach_ipc_defs.h.
 * 
 * Revision 2.4  88/07/20  21:08:20  rpd
 * Modified to use a port set to eat notifications; this fixes a message leak.
 * Also will now send error messages in response to bogus requests.
 * 
 * Revision 2.3  88/07/17  19:30:39  mwyoung
 * Change use of kernel_only to new kernel_vm_space.
 *
 * 21-Nov-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Merge version, no handling of notify ports (assumes to not
 *	have one by default).
 *
 * 05-Sep-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Put messages on stack to save memory.  Received message
 *	is of short size --- bogons sending bad messages will
 *	just get messages dropped on the floor.
 *
 * 10-Mar-88  David Black (dlb) at Carnegie-Mellon University
 *	Check error returns from port_copyin().
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 *  8-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Rewrite to use local port names and internal kernel rpcs.
 *
 *  4-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Update to exc interface.  Set ipc_kernel in handler thread.
 *	Deallocate thread/task references now returned from convert
 *	routines.
 *
 * 30-Nov-87  David Black (dlb) at Carnegie-Mellon University
 *	Split unix-dependent stuff into this separate file.
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
 **********************************************************************
 */

#import <sys/param.h>

#import <sys/boolean.h>
#import <sys/exception.h>
#import <sys/kern_return.h>
#import <sys/message.h>
#import <sys/port.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <sys/user.h>
#import <sys/ux_exception.h>
#import <sys/mig_errors.h>
#import <kern/kalloc.h>

#import <kern/exc.h>
#import <kern/mach_user_internal.h>

#import <kern/sched_prim.h>

#import <kern/ipc_pobj.h>
#import <kern/ipc_copyin.h>

/*
 *	Unix exception handler.
 */

void	ux_exception();

decl_simple_lock_data(,	ux_handler_init_lock)
port_t			ux_exception_port;

port_t	ux_handler_task_self;
task_t	ux_handler_task;

void	ux_handler()
{
	register kern_return_t	r;
	port_name_t		ux_notify_port;
	port_name_t		ux_local_port;
	port_set_name_t		ux_set;
	register port_name_t	rep_port;
	struct rep_msg {
		msg_header_t	h;
		int		d[4];
	} rep_msg;
	struct exc_msg {
		msg_header_t	h;
		int		d[16];	/* max is actually 10 */
	} exc_msg_data, *exc_msg = &exc_msg_data;
	msg_size_t exc_msg_size = sizeof(exc_msg_data);

	ux_handler_task = current_task();
	ux_handler_task->kernel_vm_space = TRUE;
	current_thread()->ipc_kernel = TRUE;

	simple_lock(&ux_handler_init_lock);

	ux_handler_task_self = task_self();

	/*
	 *	Allocate a port set that we will receive on.
	 */
	r = port_set_allocate(ux_handler_task_self, &ux_set);
	if (r != KERN_SUCCESS)
		panic("ux_handler: port_set_allocate failed");

	/*
	 *	Allocate an exception port and use object_copyin to
	 *	translate it to the global name.  Put it into the set.
	 */
	r = port_allocate(ux_handler_task_self, &ux_local_port);
	if (r != KERN_SUCCESS)
		panic("ux_handler: port_allocate failed");
	r = port_set_add(ux_handler_task_self, ux_set, ux_local_port);
	if (r != KERN_SUCCESS)
		panic("ux_handler: port_set_add failed");
	ux_exception_port = ux_local_port;
	if (!object_copyin(ux_handler_task, ux_exception_port,
			   MSG_TYPE_PORT, FALSE,
			   (kern_obj_t *) &ux_exception_port))
		panic("ux_handler: object_copyin(ux_exception_port) failed");

	/*
	 *	Release kernel to continue.
	 */
	thread_wakeup((int) &ux_exception_port);
	simple_unlock(&ux_handler_init_lock);

/*	bcopy("exception_hdlr",u.u_comm,15); /* XXX */

	/* Message handling loop. */

 	for (;;) {
		exc_msg->h.msg_local_port = ux_set;
		exc_msg->h.msg_size = exc_msg_size;
		if ((r = msg_receive(&exc_msg->h, MSG_OPTION_NONE, 0))
		    == RCV_SUCCESS) {
			if (exc_server(&exc_msg->h, &rep_msg.h)) {
			    rep_port = rep_msg.h.msg_remote_port;
			    (void) msg_send(&rep_msg.h, MSG_OPTION_NONE, 0);
			    (void) port_deallocate(ux_handler_task_self,
						   rep_port);
			}
		} else if (r == RCV_TOO_LARGE) {
			if (exc_msg != &exc_msg_data)
				panic("exception_handler: exc_msg");

			/*
			 *	Some bozo sent us a large request.
			 *	To receive it, we have to switch to
			 *	using a larger buffer.
			 */

			exc_msg = (struct exc_msg *) kalloc(MSG_SIZE_MAX);
			exc_msg_size = MSG_SIZE_MAX;
		} else
			panic("exception_handler");
	}
}

kern_return_t
catch_exception_raise(exception_port, thread_port, task_port,
	exception, code, subcode)
port_t		exception_port, thread_port, task_port;
int		exception, code, subcode;
{
    thread_t	thread;
    task_t	task;
    int		signal = 0;
    int		ret = KERN_SUCCESS;

#ifdef	lint
    exception_port++;
#endif	lint

    /*
     *	Convert local port names to structure pointers.  Have object_copyin
     *	deallocate our rights to the ports.  (it returns a reference).
     */
    if (!object_copyin(ux_handler_task, task_port,
		       MSG_TYPE_PORT, TRUE,
		       (kern_obj_t *) &task_port))
	return(KERN_INVALID_ARGUMENT);

    if (!object_copyin(ux_handler_task, thread_port,
		       MSG_TYPE_PORT, TRUE,
		       (kern_obj_t *) &thread_port)) {
	port_release(task_port);
	return(KERN_INVALID_ARGUMENT);
    }

    task = convert_port_to_task(task_port);
    thread = convert_port_to_thread(thread_port);

    /*
     *	Catch bogus ports
     */
    if (task != TASK_NULL && thread != THREAD_NULL) {

	    /*
	     *	Convert exception to unix signal and code.
	     */
	    ux_exception(exception, code, subcode, &signal,
		&thread->u_address.uthread->uu_code);

	    /*
	     *	Send signal.
	     */
	    if (signal != 0)
		thread_psignal(thread, signal);
    }
    else {
	ret = KERN_INVALID_ARGUMENT;
    }

    /*
     *	Delete the references acquired in the convert routines.
     */
    if (task != TASK_NULL) 
	task_deallocate(task);

    if (thread != THREAD_NULL)
	thread_deallocate(thread);

    /*
     *	Delete the port references that came from port_copyin.
     */
    port_release(task_port);
    port_release(thread_port);

    return(ret);
}


boolean_t	machine_exception();

/*
 *	ux_exception translates a mach exception, code and subcode to
 *	a signal and u.u_code.  Calls machine_exception (machine dependent)
 *	to attempt translation first.
 */

void ux_exception(exception, code, subcode, ux_signal, ux_code)
int	exception, code, subcode;
int	*ux_signal, *ux_code;
{
	/*
	 *	Try machine-dependent translation first.
	 */
	if (machine_exception(exception, code, subcode, ux_signal, 
	    ux_code))
		return;
	
	switch(exception) {

	    case EXC_BAD_ACCESS:
		if (code == KERN_INVALID_ADDRESS)
		    *ux_signal = SIGSEGV;
		else
		    *ux_signal = SIGBUS;
		break;

	    case EXC_BAD_INSTRUCTION:
	        *ux_signal = SIGILL;
		break;

	    case EXC_ARITHMETIC:
	        *ux_signal = SIGFPE;
		break;

	    case EXC_EMULATION:
		*ux_signal = SIGEMT;
		break;

	    case EXC_SOFTWARE:
		switch (code) {
		    case EXC_UNIX_BAD_SYSCALL:
			*ux_signal = SIGSYS;
			break;
		    case EXC_UNIX_BAD_PIPE:
		    	*ux_signal = SIGPIPE;
			break;
		    case EXC_UNIX_ABORT:
		    	*ux_signal = SIGABRT;
			break;
		}
		break;

	    case EXC_BREAKPOINT:
		*ux_signal = SIGTRAP;
		break;
	}
}


