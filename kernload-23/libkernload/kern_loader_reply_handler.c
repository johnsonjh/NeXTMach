/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 20-Jul-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <kernserv/kern_loader_reply_handler.h>
#import "kern_loader_replyServer.c"

/*
 * The port argument in each of the following is actually a pointer
 * to a structure containing pointers to functions to call for each of
 * the following messages.
 */

kern_return_t kern_loader_reply_string
(
	port_t		reply_port,
	printf_data_t	string,
	unsigned int	stringCnt,
	int		level)
{
	kern_loader_reply_t *kern_loader_reply =
		(kern_loader_reply_t *)reply_port;

	if (kern_loader_reply->string == 0)
		return MIG_BAD_ID;
	return (*kern_loader_reply->string)(
		kern_loader_reply->arg, string, stringCnt, level);
}

kern_return_t kern_loader_reply_ping
(
	port_t		reply_port,
	int		id)
{
	kern_loader_reply_t *kern_loader_reply =
		(kern_loader_reply_t *)reply_port;

	if (kern_loader_reply->ping == 0)
		return MIG_BAD_ID;
	return (*kern_loader_reply->ping)(
		kern_loader_reply->arg, id);
}

kern_return_t kern_loader_reply_log_data
(
	port_t		reply_port,
	printf_data_t	log_data,
	unsigned int	log_dataCnt)
{
	kern_loader_reply_t *kern_loader_reply =
		(kern_loader_reply_t *)reply_port;

	if (kern_loader_reply->log_data == 0)
		return MIG_BAD_ID;
	return (*kern_loader_reply->log_data)(
		kern_loader_reply->arg, log_data, log_dataCnt);
}

kern_return_t kern_loader_reply_handler (
	msg_header_t		*msg,
	kern_loader_reply_t	*kern_loader_reply)
{
	char out_msg_buf[KERN_LOADER_REPLY_OUTMSG_SIZE];
	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;
	Reply *out_msg = (Reply *)out_msg_buf;
	kern_return_t ret_code;

	msg->msg_local_port = (port_t)kern_loader_reply;

	kern_loader_reply_server(msg, (msg_header_t *)out_msg);
	ret_code = out_msg->RetCode;

	if (out_msg->RetCode == MIG_NO_REPLY)
		ret_code = KERN_SUCCESS;

	return ret_code;
}

