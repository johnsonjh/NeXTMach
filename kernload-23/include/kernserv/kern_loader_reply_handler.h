/* 
 * Mach Operating System
 * Copyright (c) 1988 NeXT, Inc.
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 */
/*
 * HISTORY
 * 07-July-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef _KERN_LOADER_REPLY_HANDLER_
#define _KERN_LOADER_REPLY_HANDLER_

#import <sys/kern_return.h>
#import <sys/port.h>
#import <sys/message.h>

#import <kernserv/kern_loader_types.h>

/*
 * Functions to call for handling messages returned.
 */
typedef struct kern_loader_reply {
	void *		arg;		// argument to pass to function
	msg_timeout_t	timeout;	// timeout for RPC return msg_send
	kern_return_t	(*string)(
				void *		arg,
				printf_data_t	string,
				unsigned int	stringCnt,
				int		level);
	kern_return_t	(*ping)(
				void *		arg,
				int		id);
	kern_return_t	(*log_data)(
				void *		arg,
				printf_data_t	log_data,
				unsigned int	log_dataCnt);
} kern_loader_reply_t;

/*
 * Sizes of messages structures for send and receive.
 */
union kern_loader_reply_request {
	struct {
		msg_header_t Head;
		msg_type_long_t stringType;
		printf_data_t string;
		msg_type_t levelType;
		int level;
	} string;
	struct {
		msg_header_t Head;
		msg_type_t idType;
		int id;
	} ping;
	struct {
		msg_header_t Head;
		msg_type_long_t log_dataType;
		printf_data_t log_data;
	} log_data;
};
#define KERN_LOADER_REPLY_INMSG_SIZE sizeof(union kern_loader_reply_request)

union kern_loader_reply_reply {
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} string;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} ping;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} log_data;
};
#define KERN_LOADER_REPLY_OUTMSG_SIZE sizeof(union kern_loader_reply_reply)

/*
 * Handler routine to call when receiving messages from the kern_loader.
 */
kern_return_t kern_loader_reply_handler (
	msg_header_t *msg,
	kern_loader_reply_t *kern_loader_reply);

#endif	_KERN_LOADER_REPLY_HANDLER_
