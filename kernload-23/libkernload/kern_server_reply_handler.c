/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 07-Jun-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <kernserv/kern_server_reply_handler.h>
#import "kern_server_replyServer.c"

/*
 * The port argument in each of the following is actually a pointer
 * to a structure containing pointers to functions to call for each of
 * the following messages.
 */

kern_return_t kern_serv_panic (
	port_t boot_port,
	panic_msg_t panic_msg)
{
	kern_server_reply_t *kern_server_reply =
		(kern_server_reply_t *)boot_port;

	if (kern_server_reply->panic == 0)
		return MIG_BAD_ID;
	return (*kern_server_reply->panic)(
		kern_server_reply->arg, panic_msg);
}

kern_return_t kern_serv_section_by_name (
	port_t boot_port,
	const char *segname,
	const char *sectname,
	vm_address_t *addr,
	vm_size_t *size)
{
	kern_server_reply_t *kern_server_reply =
		(kern_server_reply_t *)boot_port;

	if (kern_server_reply->section_by_name == 0)
		return MIG_BAD_ID;
	return (*kern_server_reply->section_by_name)(
		kern_server_reply->arg, segname, sectname, addr, size);
}

kern_return_t kern_serv_log_data (
	port_t log_port,
	log_entry_t *log,
	unsigned int logCnt)
{
	kern_server_reply_t *kern_server_reply =
		(kern_server_reply_t *)log_port;

	if (kern_server_reply->log_data == 0)
		return MIG_BAD_ID;
	return (*kern_server_reply->log_data)(
		kern_server_reply->arg, log, logCnt);
}

kern_return_t kern_serv_notify (
	port_t boot_port,
	port_t reply_port,
	port_t req_port)
{
	kern_server_reply_t *kern_server_reply =
		(kern_server_reply_t *)boot_port;

	if (kern_server_reply->notify == 0)
		return MIG_BAD_ID;
	return (*kern_server_reply->notify)(
		kern_server_reply->arg, reply_port, req_port);
}

kern_return_t kern_server_reply_handler (
	msg_header_t *msg,
	kern_server_reply_t *kern_server_reply)
{
	char out_msg_buf[KERN_SERVER_REPLY_OUTMSG_SIZE];
	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;
	Reply *out_msg = (Reply *)out_msg_buf;
	kern_return_t ret_code;

	msg->msg_local_port = (port_t)kern_server_reply;

	kern_server_reply_server(msg, (msg_header_t *)out_msg);
	ret_code = out_msg->RetCode;

	if (out_msg->RetCode == MIG_NO_REPLY)
		ret_code = KERN_SUCCESS;

	return msg_send(out_msg, SEND_TIMEOUT, kern_server_reply->timeout);
}
