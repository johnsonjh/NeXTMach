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
 * $Log:	ipc_ptraps.c,v $
 *  1-Mar-90  Gregg Kellogg (gk) at NeXT
 *	NeXT:	Modified all TRAP macros to check for port in kernel_ipc_space
 *		to avoid object_copyout.
 *
 * Revision 2.7  89/10/11  14:08:54  dlb
 * 	Added host_self, host_priv_self.  Latter is a hack until we have
 * 	a real nameserver and figure out how to authenticate it.
 * 	[88/11/30            dlb]
 * 
 * Revision 2.6  89/06/27  00:24:25  rpd
 * 	Added support for task_tself, thread_tself.
 * 	[89/06/26  23:53:43  rpd]
 * 
 * Revision 2.5  89/03/09  20:12:40  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  18:03:58  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.3  88/09/25  22:13:24  rpd
 * 	Changed includes to the new style.
 * 	[88/09/19  16:21:59  rpd]
 * 
 * Revision 2.2  88/08/06  18:20:16  rpd
 * Fixed to do locking and handle PORT_NULL gracefully.
 * Created.
 * 
 */
/*
 * File:	ipc_ptraps.c
 * Purpose:
 *	Task & thread port traps.
 */

#import <kern/host.h>
#import <sys/port.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/kern_port.h>
#import <kern/ipc_copyout.h>
#import <kern/ipc_ptraps.h>

/*
 *	Routines:	task_self, task_notify, thread_self, thread_reply
 *			[exported, trap]
 *	Purpose:
 *		Primitive traps that provide the currently-executing
 *		task/thread with one of its ports.
 */

#if	NeXT
#define TASK_TRAP(trap_name, port_to_get)				\
port_name_t								\
trap_name()								\
{									\
	register task_t self = current_thread()->task;			\
	register kern_port_t port;					\
	port_name_t name;						\
									\
	ipc_task_lock(self);						\
	port = (kern_port_t) self->port_to_get;				\
	if (port != KERN_PORT_NULL)					\
		port_reference(port);					\
	ipc_task_unlock(self);						\
									\
	if (port != KERN_PORT_NULL) {					\
		if (self->kernel_ipc_space)		/* NeXT */	\
			name = (port_name_t) port;	/* NeXT */	\
		else					/* NeXT */	\
			object_copyout(self, &port->port_obj,		\
					MSG_TYPE_PORT, &name);		\
	}								\
	else								\
		name = PORT_NULL;					\
									\
	return name;							\
}
#else	NeXT
#define TASK_TRAP(trap_name, port_to_get)				\
port_name_t								\
trap_name()								\
{									\
	register task_t self = current_thread()->task;			\
	register kern_port_t port;					\
	port_name_t name;						\
									\
	ipc_task_lock(self);						\
	port = (kern_port_t) self->port_to_get;				\
	if (port != KERN_PORT_NULL)					\
		port_reference(port);					\
	ipc_task_unlock(self);						\
									\
	if (port != KERN_PORT_NULL)					\
		object_copyout(self, &port->port_obj,			\
			       MSG_TYPE_PORT, &name);			\
	else								\
		name = PORT_NULL;					\
									\
	return name;							\
}
#endif	NeXT

TASK_TRAP(task_self, task_tself)
TASK_TRAP(task_notify, task_notify)

#if	NeXT
#define THREAD_TRAP(trap_name, port_to_get)				\
port_name_t								\
trap_name()								\
{									\
	register thread_t self = current_thread();			\
	register kern_port_t port;					\
	port_name_t name;						\
									\
	ipc_thread_lock(self);						\
	port = (kern_port_t) self->port_to_get;				\
	if (port != KERN_PORT_NULL)					\
		port_reference(port);					\
	ipc_thread_unlock(self);					\
									\
	if (port != KERN_PORT_NULL) {					\
		if (self->task->kernel_ipc_space)	/* NeXT */	\
			name = (port_name_t) port;	/* NeXT */	\
		else					/* NeXT */	\
			object_copyout(self->task, &port->port_obj,	\
				       MSG_TYPE_PORT, &name);		\
	} else								\
		name = PORT_NULL;					\
									\
	return name;							\
}
#else	NeXT
#define THREAD_TRAP(trap_name, port_to_get)				\
port_name_t								\
trap_name()								\
{									\
	register thread_t self = current_thread();			\
	register kern_port_t port;					\
	port_name_t name;						\
									\
	ipc_thread_lock(self);						\
	port = (kern_port_t) self->port_to_get;				\
	if (port != KERN_PORT_NULL)					\
		port_reference(port);					\
	ipc_thread_unlock(self);					\
									\
	if (port != KERN_PORT_NULL)					\
		object_copyout(self->task, &port->port_obj,		\
			       MSG_TYPE_PORT, &name);			\
	else								\
		name = PORT_NULL;					\
									\
	return name;							\
}
#endif	NeXT

THREAD_TRAP(thread_self, thread_tself)
THREAD_TRAP(thread_reply, thread_reply)

#if	NeXT
#define	HOST_TRAP(trap_name, port_to_get)				\
port_name_t								\
trap_name()								\
{									\
	register task_t self = current_thread()->task;			\
	register kern_port_t port;					\
	port_name_t name;						\
									\
	port = (kern_port_t) realhost.port_to_get;			\
	if (port != KERN_PORT_NULL)					\
		port_reference(port);					\
									\
	if (port != KERN_PORT_NULL) {					\
		if (self->kernel_ipc_space)		/* NeXT */	\
			name = (port_name_t) port;	/* NeXT */	\
		else					/* NeXT */	\
			object_copyout(self, &port->port_obj,		\
				       MSG_TYPE_PORT, &name);		\
	}								\
	else								\
		name = PORT_NULL;					\
									\
	return name;							\
}
#else	NeXT
#define	HOST_TRAP(trap_name, port_to_get)				\
port_name_t								\
trap_name()								\
{									\
	register task_t self = current_thread()->task;			\
	register kern_port_t port;					\
	port_name_t name;						\
									\
	port = (kern_port_t) realhost.port_to_get;			\
	if (port != KERN_PORT_NULL)					\
		port_reference(port);					\
									\
	if (port != KERN_PORT_NULL)					\
		object_copyout(self, &port->port_obj,			\
			       MSG_TYPE_PORT, &name);			\
	else								\
		name = PORT_NULL;					\
									\
	return name;							\
}
#endif	NeXT

HOST_TRAP(host_self, host_self)

#if	NeXT
#define	HOST_PRIV_TRAP(trap_name, port_to_get)				\
port_name_t								\
trap_name()								\
{									\
	register task_t self = current_thread()->task;			\
	register kern_port_t port;					\
	port_name_t name;						\
									\
	if (!suser()) {							\
		name = PORT_NULL;					\
	}								\
	else {								\
		port = (kern_port_t) realhost.port_to_get;		\
		if (port != KERN_PORT_NULL)				\
			port_reference(port);				\
									\
		if (port != KERN_PORT_NULL) {				\
			if (self->kernel_ipc_space)	/* NeXT */	\
				name=(port_name_t)port;	/* NeXT */	\
			else				/* NeXT */	\
				object_copyout(self, &port->port_obj,	\
					       MSG_TYPE_PORT, &name);	\
		}							\
		else							\
			name = PORT_NULL;				\
	}								\
	return name;							\
}
#else	NeXT
#define	HOST_PRIV_TRAP(trap_name, port_to_get)				\
port_name_t								\
trap_name()								\
{									\
	register task_t self = current_thread()->task;			\
	register kern_port_t port;					\
	port_name_t name;						\
									\
	if (!suser()) {							\
		name = PORT_NULL;					\
	}								\
	else {								\
		port = (kern_port_t) realhost.port_to_get;		\
		if (port != KERN_PORT_NULL)				\
			port_reference(port);				\
									\
		if (port != KERN_PORT_NULL)				\
			object_copyout(self, &port->port_obj,		\
			       MSG_TYPE_PORT, &name);			\
		else							\
			name = PORT_NULL;				\
	}								\
	return name;							\
}
#endif	NeXT

HOST_PRIV_TRAP(host_priv_self, host_priv_self)



