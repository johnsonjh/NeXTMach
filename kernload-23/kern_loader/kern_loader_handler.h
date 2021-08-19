/* 
 * Mach Operating System
 * Copyright (c) 1988 NeXT, Inc.
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 */
/*
 * HISTORY
 * 22-Jul-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef _KERN_LOADER_HANDLER_
#define _KERN_LOADER_HANDLER_

#import <mach_types.h>
#import <sys/kern_return.h>
#import <sys/port.h>
#import <sys/message.h>
#import <sys/boolean.h>

#import <kernserv/kern_loader_types.h>

/*
 * Functions to call for handling messages returned.
 */
typedef struct kern_loader {
	void *		arg;		// argument to pass to function
	msg_timeout_t	timeout;	// timeout for RPC return msg_send
	kern_return_t	(*abort)(
				void *		arg,
				port_t		kernel_port,
				boolean_t	restart);
	kern_return_t	(*load_server)(
				void *		arg,
				server_name_t	server_name);
	kern_return_t	(*unload_server)(
				void *		arg,
				port_t		kernel_port,
				server_name_t	server_name);
	kern_return_t	(*add_server)(
				void *		arg,
				port_t		kernel_port,
				server_reloc_t	server_reloc);
	kern_return_t	(*delete_server)(
				void *		arg,
				port_t		kernel_port,
				server_name_t	server_name);
	kern_return_t	(*server_task_port)(
				void *		arg,
				port_t		kernel_port,
				server_name_t	server_name,
				port_t		*server_task_port);
	kern_return_t	(*server_com_port)(
				void *		arg,
				port_t		kernel_port,
				server_name_t	server_name,
				port_t		*server_com_port);
	kern_return_t	(*status_port)(
				void *		arg,
				port_t		listen_port);
	kern_return_t	(*ping)(
				void *		arg,
				port_t		ping_port,
				int		id);
	kern_return_t	(*log_level)(
				void *		arg,
				port_t		server_com_port,
				int		log_level);
	kern_return_t	(*get_log)(
				void *		arg,
				port_t		server_com_port,
				port_t		reply_port);
	kern_return_t	(*server_list)(
				void *		arg,
				server_name_t	**servers,
				unsigned int	*serversCnt);
	kern_return_t	(*server_info)(
				void *		arg,
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
				unsigned int	*advertisedCnt);
} kern_loader_t;

/*
 * Sizes of messages structures for send and receive.
 */
union kern_loader_request {
	struct {
		msg_header_t Head;
		msg_type_t kernel_portType;
		port_t kernel_port;
		msg_type_t restartType;
		boolean_t restart;
	} abort;
	struct {
		msg_header_t Head;
		msg_type_long_t server_nameType;
		server_name_t server_name;
	} load_server;
	struct {
		msg_header_t Head;
		msg_type_t kernel_portType;
		port_t kernel_port;
		msg_type_long_t server_nameType;
		server_name_t server_name;
	} unload_server;
	struct {
		msg_header_t Head;
		msg_type_t kernel_portType;
		port_t kernel_port;
		msg_type_long_t server_relocType;
		server_reloc_t server_reloc;
	} add_server;
	struct {
		msg_header_t Head;
		msg_type_t kernel_portType;
		port_t kernel_port;
		msg_type_long_t server_nameType;
		server_name_t server_name;
	} delete_server;
	struct {
		msg_header_t Head;
		msg_type_t kernel_portType;
		port_t kernel_port;
		msg_type_long_t server_nameType;
		server_name_t server_name;
	} server_task_port;
	struct {
		msg_header_t Head;
		msg_type_t kernel_portType;
		port_t kernel_port;
		msg_type_long_t server_nameType;
		server_name_t server_name;
	} server_com_port;
	struct {
		msg_header_t Head;
		msg_type_t listen_portType;
		port_t listen_port;
	} status_port;
	struct {
		msg_header_t Head;
		msg_type_t ping_portType;
		port_t ping_port;
		msg_type_t idType;
		int id;
	} ping;
	struct {
		msg_header_t Head;
		msg_type_t server_com_portType;
		port_t server_com_port;
		msg_type_t log_levelType;
		int log_level;
	} log_level;
	struct {
		msg_header_t Head;
		msg_type_t server_com_portType;
		port_t server_com_port;
		msg_type_t reply_portType;
		port_t reply_port;
	} get_log;
	struct {
		msg_header_t Head;
	} server_list;
	struct {
		msg_header_t Head;
		msg_type_t task_portType;
		port_t task_port;
		msg_type_long_t server_nameType;
		server_name_t server_name;
	} server_info;
};
#define KERN_LOADER_INMSG_SIZE sizeof(union kern_loader_request)

union kern_loader_reply {
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} abort;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} load_server;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} unload_server;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} add_server;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} delete_server;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t server_task_portType;
		port_t server_task_port;
	} server_task_port;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t server_com_portType;
		port_t server_com_port;
	} server_com_port;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} status_port;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} ping;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} log_level;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} get_log;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_long_t serversType;
		server_name_array_t servers;
	} server_list;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t server_stateType;
		server_state_t server_state;
		msg_type_t load_addressType;
		vm_address_t load_address;
		msg_type_t load_sizeType;
		vm_size_t load_size;
		msg_type_long_t relocatableType;
		server_reloc_t relocatable;
		msg_type_long_t loadableType;
		server_reloc_t loadable;
		msg_type_long_t port_listType;
		port_name_array_t port_list;
		msg_type_long_t namesType;
		port_name_string_array_t names;
		msg_type_long_t advertisedType;
		boolean_array_t advertised;
	} server_info;
};
#define KERN_LOADER_OUTMSG_SIZE sizeof(union kern_loader_reply)

/*
 * Handler routine to call when receiving messages from midi driver.
 */
kern_return_t kern_loader_handler (
	msg_header_t *msg,
	kern_loader_t *kern_loader);

#endif	_KERN_LOADER_HANDLER_




