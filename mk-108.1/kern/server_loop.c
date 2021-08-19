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
 * $Log:	server_loop.c,v $
 * Revision 2.7  89/12/22  16:28:32  rpd
 * 	Replaced LOCAL_PORT with two arguments to SERVER_LOOP.
 * 	[89/11/24  23:05:48  rpd]
 * 
 * Revision 2.6  89/03/09  20:15:48  rpd
 * 	More cleanup.
 * 
 * Revision 2.5  89/02/25  18:08:28  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.4  89/01/10  23:31:54  rpd
 * 	Changed to require use of LOCAL_PORT to specify a port set.
 * 	Changed xxx_port_enable to port_set_add.
 * 	[89/01/10  13:33:38  rpd]
 * 
 * Revision 2.3  88/10/18  03:36:35  mwyoung
 * 	Allow the local port (on which a message is to be received) to
 * 	be redefined by this module's client.
 * 	[88/10/01            mwyoung]
 * 
 * Revision 2.2  88/07/23  01:21:04  rpd
 * Changed port_enable to xxx_port_enable.
 * 
 * 11-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Corrected error in timeout handling.
 *
 * 15-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Created.
 */

/*
 *	File:	kern/server_loop.c
 *
 *	A common server loop for builtin pagers.
 */

/*
 *	Must define symbols for:
 *		SERVER_NAME		String name of this module
 *		TERMINATE_FUNCTION	How to handle port death
 *		SERVER_LOOP		Routine name for the loop
 *		SERVER_DISPATCH		MiG function(s) to handle message
 *
 *	May optionally define:
 *		RECEIVE_OPTION		Receive option (default NONE)
 *		RECEIVE_TIMEOUT		Receive timeout (default 0)
 *		TIMEOUT_FUNCTION	Timeout handler (default null)
 *
 *	Must redefine symbols for pager_server functions.
 */

#ifndef	RECEIVE_OPTION
#define RECEIVE_OPTION	MSG_OPTION_NONE
#endif	RECEIVE_OPTION

#ifndef	RECEIVE_TIMEOUT
#define RECEIVE_TIMEOUT	0
#endif	RECEIVE_TIMEOUT

#ifndef	TIMEOUT_FUNCTION
#define TIMEOUT_FUNCTION
#endif	TIMEOUT_FUNCTION

#import <sys/boolean.h>
#import <sys/port.h>
#import <sys/message.h>
#import <sys/notify.h>

void		SERVER_LOOP(rcv_set, do_notify)
	port_t		rcv_set;
	boolean_t	do_notify;
{
	msg_return_t	r;
	port_t		my_notify;
	port_t		my_self;
	vm_offset_t	messages;
	register
	msg_header_t	*in_msg;
	msg_header_t	*out_msg;

	/*
	 *	Find out who we are.
	 */

	my_self = task_self();

	if (do_notify) {
		my_notify = task_notify();

		if (port_set_add(my_self, rcv_set, my_notify)
						!= KERN_SUCCESS) {
			printf("%s: can't enable notify port", SERVER_NAME);
			panic(SERVER_NAME);
		}
	}

	/*
	 *	Allocate our message buffers.
	 *	[The buffers must reside in kernel space, since other
	 *	message buffers (in the mach_user_external module) are.]
	 */

	if ((messages = kmem_alloc(kernel_map, 2 * MSG_SIZE_MAX)) == 0) {
		printf("%s: can't allocate message buffers", SERVER_NAME);
		panic(SERVER_NAME);
	}
	in_msg = (msg_header_t *) messages;
	out_msg = (msg_header_t *) (messages + MSG_SIZE_MAX);

	/*
	 *	Service loop... receive messages and process them.
	 */

	for (;;) {
		in_msg->msg_local_port = rcv_set;
		in_msg->msg_size = MSG_SIZE_MAX;
		if ((r = msg_receive(in_msg, RECEIVE_OPTION, RECEIVE_TIMEOUT)) != RCV_SUCCESS) {
			if (r == RCV_TIMED_OUT) {
				TIMEOUT_FUNCTION ;
			} else {
				printf("%s: receive failed, 0x%x.\n", SERVER_NAME, r);
			}
			continue;
		}
		if (do_notify && (in_msg->msg_local_port == my_notify)) {
			notification_t	*n = (notification_t *) in_msg;
			switch(in_msg->msg_id) {
				case NOTIFY_PORT_DELETED:
					TERMINATE_FUNCTION(n->notify_port);
					break;
				default:
					printf("%s: wierd notification (%d)\n", SERVER_NAME, in_msg->msg_id);
					printf("port = 0x%x.\n", n->notify_port);
					break;
			}
			continue;
		}
		if (SERVER_DISPATCH(in_msg, out_msg) &&
		    (out_msg->msg_remote_port != PORT_NULL))
			msg_send(out_msg, MSG_OPTION_NONE, 0);
	}
}

