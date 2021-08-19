/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 07-Jun-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <kernserv/kern_server_handler.h>
#import <kernserv/kern_server_server.c>

/*
 * The port argument in each of the following is actually a pointer
 * to a structure containing pointers to functions to call for each of
 * the following messages.
 */

kern_return_t ks_instance_loc (
	port_t server_port,
	vm_address_t instance_loc)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->instance_loc == 0)
		return MIG_BAD_ID;
	return (*kern_serv->instance_loc)(
		kern_serv->arg, instance_loc);
}

kern_return_t ks_boot_port (
	port_t server_port,
	port_t boot_port)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->boot_port == 0)
		return MIG_BAD_ID;
	return (*kern_serv->boot_port)(
		kern_serv->arg, boot_port);
}

kern_return_t ks_wire_range (
	port_t server_port,
	vm_address_t addr,
	vm_size_t size)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->wire_range == 0)
		return MIG_BAD_ID;
	return (*kern_serv->wire_range)(
		kern_serv->arg, addr, size);
}

kern_return_t ks_unwire_range (
	port_t server_port,
	vm_address_t addr,
	vm_size_t size)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->unwire_range == 0)
		return MIG_BAD_ID;
	return (*kern_serv->unwire_range)(
		kern_serv->arg, addr, size);
}

kern_return_t ks_port_proc (
	port_t server_port,
	port_all_t port,
	port_map_proc_t proc,
	int arg)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->port_proc == 0)
		return MIG_BAD_ID;
	return (*kern_serv->port_proc)(
		kern_serv->arg, port, proc, arg);
}

kern_return_t ks_port_death_proc (
	port_t server_port,
	port_death_proc_t proc)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->port_death_proc == 0)
		return MIG_BAD_ID;
	return (*kern_serv->port_death_proc)(
		kern_serv->arg, proc);
}

kern_return_t ks_call_proc (
	port_t server_port,
	call_proc_t proc,
	int arg)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->call_proc == 0)
		return MIG_BAD_ID;
	return (*kern_serv->call_proc)(
		kern_serv->arg, proc, arg);
}

kern_return_t ks_shutdown (
	port_t server_port)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->shutdown == 0)
		return MIG_BAD_ID;
	return (*kern_serv->shutdown)(
		kern_serv->arg);
}

kern_return_t ks_log_level (
	port_t server_port,
	int log_level)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->log_level == 0)
		return MIG_BAD_ID;
	return (*kern_serv->log_level)(
		kern_serv->arg, log_level);
}

kern_return_t ks_get_log (
	port_t server_port,
	port_t reply_port)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->get_log == 0)
		return MIG_BAD_ID;
	return (*kern_serv->get_log)(
		kern_serv->arg, reply_port);
}

kern_return_t ks_port_serv (
	port_t server_port,
	port_all_t port,
	port_map_proc_t proc,
	int arg)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->port_serv == 0)
		return MIG_BAD_ID;
	return (*kern_serv->port_serv)(
		kern_serv->arg, port, proc, arg);
}

kern_return_t ks_version (
	port_t server_port,
	int version)
{
	kern_serv_t *kern_serv = (kern_serv_t *)server_port;

	if (kern_serv->version == 0)
		return MIG_BAD_ID;
	return (*kern_serv->version)(
		kern_serv->arg, version);
}

kern_return_t kern_serv_handler (
	msg_header_t *msg,
	kern_serv_t *kern_serv)
{
	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;
	Reply *out_msg;
	kern_return_t ret_code;
	port_t local_port = msg->msg_local_port;

	out_msg = (void *)kalloc(KERN_SERV_OUTMSG_SIZE);

	msg->msg_local_port = (port_t)kern_serv;

	kern_server_server(msg, (msg_header_t *)out_msg);
	ret_code = out_msg->RetCode;

	if (out_msg->RetCode == MIG_NO_REPLY)
		ret_code = KERN_SUCCESS;
	else
		ret_code = msg_send(
			&out_msg->Head,
			  kern_serv->timeout >= 0
			? SEND_TIMEOUT
			: MSG_OPTION_NONE,
			kern_serv->timeout);

	kfree(out_msg, KERN_SERV_OUTMSG_SIZE);

	return ret_code;
}
