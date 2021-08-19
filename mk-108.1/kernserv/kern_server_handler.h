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

#ifndef _KERN_SERV_HANDLER_
#define _KERN_SERV_HANDLER_

#include <sys/kern_return.h>
#include <sys/port.h>
#include <sys/message.h>

#include <kernserv/kern_server_types.h>

/*
 * Functions to call for handling messages returned
 */
typedef struct kern_serv {
	void *		arg;		// argument to pass to function
	int		timeout;	// timeout for RPC return msg_send
	kern_return_t	(*instance_loc)(
				void *		arg,
				vm_address_t	instance_loc);
	kern_return_t	(*boot_port)(
				void *		arg,
				port_t		boot_port);
	kern_return_t	(*wire_range)(
				void *		arg,
				vm_address_t	addr,
				vm_size_t	size);
	kern_return_t	(*unwire_range)(
				void *		arg,
				vm_address_t	addr,
				vm_size_t	size);
	kern_return_t	(*port_proc)(
				void *		arg,
				port_all_t	port,
				port_map_proc_t	proc,
				int		uarg);
	kern_return_t	(*port_death_proc)(
				void *		arg,
				port_death_proc_t
						proc);
	kern_return_t	(*call_proc)(
				void *		arg,
				call_proc_t	proc,
				int		uarg);
	kern_return_t	(*shutdown)(
				void *		arg);
	kern_return_t	(*log_level)(
				void *		arg,
				int		log_level);
	kern_return_t	(*get_log)(
				void *		arg,
				port_t		reply_port);
	kern_return_t	(*port_serv)(
				void *		arg,
				port_all_t	port,
				port_map_proc_t	proc,
				int		uarg);
	kern_return_t	(*version)(
				void *		arg,
				int		version);
} kern_serv_t;

/*
 * Sizes of messages structures for send and receive.
 */
union kern_serv_request {
	struct {
		msg_header_t Head;
		msg_type_t instance_locType;
		vm_address_t instance_loc;
	} instance_loc;
	struct {
		msg_header_t Head;
		msg_type_t boot_portType;
		port_t boot_port;
	} boot_port;
	struct {
		msg_header_t Head;
		msg_type_t addrType;
		vm_address_t addr;
		msg_type_t sizeType;
		vm_size_t size;
	} wire_range;
	struct {
		msg_header_t Head;
		msg_type_t addrType;
		vm_address_t addr;
		msg_type_t sizeType;
		vm_size_t size;
	} unwire_range;
	struct {
		msg_header_t Head;
		msg_type_t portType;
		port_all_t port;
		msg_type_t procType;
		port_map_proc_t proc;
		msg_type_t argType;
		int arg;
	} port_proc;
	struct {
		msg_header_t Head;
		msg_type_t procType;
		port_death_proc_t proc;
	} port_death_proc;
	struct {
		msg_header_t Head;
		msg_type_t procType;
		call_proc_t proc;
		msg_type_t argType;
		int arg;
	} call_proc;
	struct {
		msg_header_t Head;
	} shutdown;
	struct {
		msg_header_t Head;
		msg_type_t log_levelType;
		int log_level;
	} log_level;
	struct {
		msg_header_t Head;
		msg_type_t reply_portType;
		port_t reply_port;
	} get_log;
	struct {
		msg_header_t Head;
		msg_type_t portType;
		port_all_t port;
		msg_type_t procType;
		port_map_proc_t proc;
		msg_type_t argType;
		int arg;
	} port_serv;
	struct {
		msg_header_t Head;
		msg_type_t versionType;
		int version;
	} version;
};
#define KERN_SERV_INMSG_SIZE sizeof(union kern_serv_request)

union kern_serv_reply {
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} instance_loc;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} boot_port;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} wire_range;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} unwire_range;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} port_proc;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} port_death_proc;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} call_proc;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} shutdown;
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
	} port_serv;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} version;
};
#define KERN_SERV_OUTMSG_SIZE sizeof(union kern_serv_reply)

/*
 * Handler routine to call when receiving messages from midi driver.
 */
kern_return_t kern_serv_handler (
	msg_header_t *msg,
	kern_serv_t *kern_serv);

#endif	_KERN_SERV_HANDLER_
