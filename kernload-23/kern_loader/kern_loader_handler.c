/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 22-Jul-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import "kern_loader_handler.h"
#import "kern_loaderServer.c"

/*
 * The port argument in each of the following is actually a pointer
 * to a structure containing pointers to functions to call for each of
 * the following messages.
 */

kern_return_t kern_loader_abort (
	port_t		loader_port,
	port_t		kernel_port,
	boolean_t	restart)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->abort == 0)
		return MIG_BAD_ID;
	return (*kern_loader->abort)(
		kern_loader->arg, kernel_port, restart);
}

kern_return_t kern_loader_load_server (
	port_t		loader_port,
	server_name_t	server_name)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->load_server == 0)
		return MIG_BAD_ID;
	return (*kern_loader->load_server)(
		kern_loader->arg, server_name);
}

kern_return_t kern_loader_unload_server (
	port_t		loader_port,
	port_t		kernel_port,
	server_name_t	server_name)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->unload_server == 0)
		return MIG_BAD_ID;
	return (*kern_loader->unload_server)(
		kern_loader->arg, kernel_port, server_name);
}

kern_return_t kern_loader_add_server (
	port_t		loader_port,
	port_t		kernel_port,
	server_reloc_t	server_reloc)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->add_server == 0)
		return MIG_BAD_ID;
	return (*kern_loader->add_server)(
		kern_loader->arg, kernel_port, server_reloc);
}

kern_return_t kern_loader_delete_server (
	port_t		loader_port,
	port_t		kernel_port,
	server_name_t	server_name)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->delete_server == 0)
		return MIG_BAD_ID;
	return (*kern_loader->delete_server)(
		kern_loader->arg, kernel_port,  server_name);
}

kern_return_t kern_loader_server_task_port (
	port_t		loader_port,
	port_t		kernel_port,
	server_name_t	server_name,
	port_t		*server_task_port)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->server_task_port == 0)
		return MIG_BAD_ID;
	return (*kern_loader->server_task_port)(
		kern_loader->arg, kernel_port, server_name, server_task_port);
}

kern_return_t kern_loader_server_com_port (
	port_t		loader_port,
	port_t		kernel_port,
	server_name_t	server_name,
	port_t		*server_com_port)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->server_com_port == 0)
		return MIG_BAD_ID;
	return (*kern_loader->server_com_port)(
		kern_loader->arg, kernel_port, server_name, server_com_port);
}

kern_return_t kern_loader_status_port (
	port_t		loader_port,
	port_t		listen_port)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->status_port == 0)
		return MIG_BAD_ID;
	return (*kern_loader->status_port)(
		kern_loader->arg, listen_port);
}

kern_return_t kern_loader_ping (
	port_t		loader_port,
	port_t		ping_port,
	int		id)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->ping == 0)
		return MIG_BAD_ID;
	return (*kern_loader->ping)(
		kern_loader->arg, ping_port, id);
}

kern_return_t kern_loader_log_level (
	port_t		loader_port,
	port_t		server_com_port,
	int		log_level)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->log_level == 0)
		return MIG_BAD_ID;
	return (*kern_loader->log_level)(
		kern_loader->arg, server_com_port, log_level);
}

kern_return_t kern_loader_get_log (
	port_t		loader_port,
	port_t		server_com_port,
	port_t		reply_port)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->get_log == 0)
		return MIG_BAD_ID;
	return (*kern_loader->get_log)(
		kern_loader->arg, server_com_port, reply_port);
}

kern_return_t kern_loader_server_list (
	port_t		loader_port,
	server_name_t	**servers,
	unsigned int	*serversCnt)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->server_list == 0)
		return MIG_BAD_ID;
	return (*kern_loader->server_list)(
		kern_loader->arg, servers, serversCnt);
}

kern_return_t kern_loader_server_info (
	port_t		loader_port,
	port_t		task_port,
	server_name_t	server_name,
	server_state_t	*server_state,
	vm_address_t	*load_address,
	vm_size_t	*load_size,
	server_reloc_t	relocatable,
	server_reloc_t	loadable,
	port_name_t	**port_list,
	unsigned int	*port_listCnt,
	port_name_string_array_t *names,
	unsigned int	*namesCnt,
	boolean_array_t	*advertised,
	unsigned int	*advertisedCnt)
{
	kern_loader_t *kern_loader = (kern_loader_t *)loader_port;

	if (kern_loader->server_info == 0)
		return MIG_BAD_ID;
	return (*kern_loader->server_info)(
		kern_loader->arg, task_port, server_name, server_state, load_address, load_size, relocatable, loadable, port_list, port_listCnt, names, namesCnt, advertised, advertisedCnt);
}

kern_return_t kern_loader_handler (
	msg_header_t		*msg,
	kern_loader_t		*kern_loader)
{
	char out_msg_buf[KERN_LOADER_OUTMSG_SIZE];
	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;
	Reply *out_msg = (Reply *)out_msg_buf;
	kern_return_t ret_code;

	msg->msg_local_port = (port_t)kern_loader;

	kern_loader_server(msg, (msg_header_t *)out_msg);
	ret_code = out_msg->RetCode;

	if (out_msg->RetCode == MIG_NO_REPLY)
		ret_code = KERN_SUCCESS;

	return msg_send(&out_msg->Head, SEND_TIMEOUT, kern_loader->timeout);
}
