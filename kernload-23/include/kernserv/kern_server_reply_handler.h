/* 
 * Mach Operating System
 * Copyright (c) 1988 NeXT, Inc.
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 */
/*
 * HISTORY
 * 07-Sep-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef _KERN_SERV_REPLY_HANDLER_
#define _KERN_SERV_REPLY_HANDLER_

#import <sys/kern_return.h>
#import <sys/port.h>
#import <sys/message.h>
#import <kernserv/kern_server_reply_types.h>

/*
 * Functions to call for handling messages returned
 */
typedef struct kern_server_reply {
	void *		arg;		// argument to pass to function
	int		timeout;	// timeout for RPC return msg_send
	kern_return_t	(*panic)(
				void *		arg,
				panic_msg_t	panic_msg);
	kern_return_t	(*section_by_name)(
				void *		arg,
				const char *	segname,
				const char *	sectname,
				vm_address_t *	addr,
				vm_size_t *	size);
	kern_return_t	(*log_data)(
				void *		arg,
				log_entry_t	*log,
				unsigned int	logCnt);
	kern_return_t	(*notify)(
				void *		arg,
				port_t		reply_port,
				port_t		req_port);
} kern_server_reply_t;

/*
 * Sizes of messages structures for send and receive.
 */
union kern_server_reply_request {
	struct {
		msg_header_t Head;
		msg_type_long_t panic_msgType;
		panic_msg_t panic_msg;
	} panic;
	struct {
		msg_header_t Head;
		msg_type_t logType;
		log_entry_t log[500];
	} log_data;
	struct {
		msg_header_t Head;
		msg_type_t reply_portType;
		port_t reply_port;
		msg_type_t req_portType;
		port_t req_port;
	} notify;
};
#define KERN_SERVER_REPLY_INMSG_SIZE sizeof(union kern_server_reply_request)

union kern_server_reply_reply {
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} panic;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} log_data;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} notify;
};
#define KERN_SERVER_REPLY_OUTMSG_SIZE sizeof(union kern_server_reply_reply)

/*
 * Handler routine to call when receiving messages from midi driver.
 */
kern_return_t kern_server_reply_handler (
	msg_header_t *msg,
	kern_server_reply_t *kern_server_reply);

#endif	_KERN_SERVER_REPLY_HANDLER_
