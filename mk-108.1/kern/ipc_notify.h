/*	@(#)ipc_notify.h	2.0	05/11/90	(c) 1990 NeXT	*/

/* 
 * ipc_notify.h - definition of interface to port death notification server
 *
 * HISTORY
 * 05-11-90	Doug Mitchell at NeXT
 *	Created.
 */ 

#ifndef	_IPC_NOTIFY_
#define _IPC_NOTIFY_

#define PORT_REGISTER_NOTIFY	0x76543		/* register notification request
						 * msg_id */

/*
 * message struct for registering for port death notification with voltask
 * (msg_id = PORT_REGISTER_NOTIFY).
 */
struct port_register_notify {
	msg_header_t	prn_header;
	msg_type_t	prn_p_type;
	kern_port_t	prn_rights_port;	/* port to which requesting 
						 * thread has send rights */
	kern_port_t	prn_notify_port;	/* port to which notification
						 * should be sent when 
						 * prn_rights_port dies */
	msg_type_t	prn_rpr_type;
	int		prn_rp_rep;		/* non-translated 
						 * representation of 
						 * prn_rights_port */
};

typedef struct port_register_notify *port_register_notify_t;

void port_request_notification(kern_port_t reg_port, 
	kern_port_t notify_port);
kern_return_t get_kern_port(task_t task, port_t in_port, kern_port_t *kport);
void pnotify_start();

#endif	_IPC_NOTIFY_

/* end of ipc_notify.c */