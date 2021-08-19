#include "kern_server_reply.h"
#include <kern/mach_types.h>
#include <sys/message.h>
#include <sys/mig_errors.h>
#include <sys/msg_type.h>
#if	!defined(KERNEL) && !defined(MIG_NO_STRINGS)
#include <strings.h>
#endif
/* LINTLIBRARY */

extern port_t mig_get_reply_port();
extern void mig_dealloc_reply_port();

#ifndef	mig_internal
#define	mig_internal	static
#endif

#ifndef	TypeCheck
#define	TypeCheck 1
#endif

#ifndef	UseExternRCSId
#ifdef	hc
#define	UseExternRCSId		1
#endif
#endif

#ifndef	UseStaticMsgType
#if	!defined(hc) || defined(__STDC__)
#define	UseStaticMsgType	1
#endif
#endif

#define msg_request_port	msg_remote_port
#define msg_reply_port		msg_local_port


/* Routine kern_serv_panic */
mig_external kern_return_t kern_serv_panic
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t boot_port,
	panic_msg_t panic_msg
)
#else
	(boot_port, panic_msg)
	port_t boot_port;
	panic_msg_t panic_msg;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_long_t panic_msgType;
		panic_msg_t panic_msg;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	union {
		Request In;
		Reply Out;
	} Mess;

	register Request *InP = &Mess.In;
	register Reply *OutP = &Mess.Out;

	msg_return_t msg_result;

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size = 292;

#if	UseStaticMsgType
	static const msg_type_long_t panic_msgType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_STRING,
		/* msg_type_long_size = */	2048,
		/* msg_type_long_number = */	1,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t RetCodeCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	InP->panic_msgType = panic_msgType;
#else	UseStaticMsgType
	InP->panic_msgType.msg_type_long_name = MSG_TYPE_STRING;
	InP->panic_msgType.msg_type_long_size = 2048;
	InP->panic_msgType.msg_type_long_number = 1;
	InP->panic_msgType.msg_type_header.msg_type_inline = TRUE;
	InP->panic_msgType.msg_type_header.msg_type_longform = TRUE;
	InP->panic_msgType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	(void) strncpy(InP->panic_msg /* panic_msg */, /* panic_msg */ panic_msg, 256);
	InP->panic_msg /* panic_msg */[255] = '\0';

	InP->Head.msg_simple = TRUE;
	InP->Head.msg_size = msg_size;
	InP->Head.msg_type = MSG_TYPE_NORMAL | MSG_TYPE_RPC;
	InP->Head.msg_request_port = boot_port;
	InP->Head.msg_reply_port = mig_get_reply_port();
	InP->Head.msg_id = 200;

	msg_result = msg_rpc(&InP->Head, MSG_OPTION_NONE, sizeof(Reply), 0, 0);
	if (msg_result != RPC_SUCCESS) {
		if (msg_result == RCV_INVALID_PORT)
			mig_dealloc_reply_port();
		return msg_result;
	}

#if	TypeCheck
	msg_size = OutP->Head.msg_size;
	msg_simple = OutP->Head.msg_simple;
#endif	TypeCheck

	if (OutP->Head.msg_id != 300)
		return MIG_REPLY_MISMATCH;

#if	TypeCheck
	if (((msg_size != 32) || (msg_simple != TRUE)) &&
	    ((msg_size != sizeof(death_pill_t)) ||
	     (msg_simple != TRUE) ||
	     (OutP->RetCode == KERN_SUCCESS)))
		return MIG_TYPE_ERROR;
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &OutP->RetCodeType != * (int *) &RetCodeCheck)
#else	UseStaticMsgType
	if ((OutP->RetCodeType.msg_type_inline != TRUE) ||
	    (OutP->RetCodeType.msg_type_longform != FALSE) ||
	    (OutP->RetCodeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (OutP->RetCodeType.msg_type_number != 1) ||
	    (OutP->RetCodeType.msg_type_size != 32))
#endif	UseStaticMsgType
		return MIG_TYPE_ERROR;
#endif	TypeCheck

	if (OutP->RetCode != KERN_SUCCESS)
		return OutP->RetCode;

	return OutP->RetCode;
}

/* Routine kern_serv_section_by_name */
mig_external kern_return_t kern_serv_section_by_name
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t boot_port,
	macho_header_name_t segname,
	macho_header_name_t sectname,
	vm_address_t *addr,
	vm_size_t *size
)
#else
	(boot_port, segname, sectname, addr, size)
	port_t boot_port;
	macho_header_name_t segname;
	macho_header_name_t sectname;
	vm_address_t *addr;
	vm_size_t *size;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t segnameType;
		macho_header_name_t segname;
		msg_type_t sectnameType;
		macho_header_name_t sectname;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t addrType;
		vm_address_t addr;
		msg_type_t sizeType;
		vm_size_t size;
	} Reply;

	union {
		Request In;
		Reply Out;
	} Mess;

	register Request *InP = &Mess.In;
	register Reply *OutP = &Mess.Out;

	msg_return_t msg_result;

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size = 64;

#if	UseStaticMsgType
	static const msg_type_t segnameType = {
		/* msg_type_name = */		MSG_TYPE_STRING,
		/* msg_type_size = */		128,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t sectnameType = {
		/* msg_type_name = */		MSG_TYPE_STRING,
		/* msg_type_size = */		128,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t RetCodeCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t addrCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t sizeCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	InP->segnameType = segnameType;
#else	UseStaticMsgType
	InP->segnameType.msg_type_name = MSG_TYPE_STRING;
	InP->segnameType.msg_type_size = 128;
	InP->segnameType.msg_type_number = 1;
	InP->segnameType.msg_type_inline = TRUE;
	InP->segnameType.msg_type_longform = FALSE;
	InP->segnameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	(void) strncpy(InP->segname /* segname */, /* segname */ segname, 16);
	InP->segname /* segname */[15] = '\0';

#if	UseStaticMsgType
	InP->sectnameType = sectnameType;
#else	UseStaticMsgType
	InP->sectnameType.msg_type_name = MSG_TYPE_STRING;
	InP->sectnameType.msg_type_size = 128;
	InP->sectnameType.msg_type_number = 1;
	InP->sectnameType.msg_type_inline = TRUE;
	InP->sectnameType.msg_type_longform = FALSE;
	InP->sectnameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	(void) strncpy(InP->sectname /* sectname */, /* sectname */ sectname, 16);
	InP->sectname /* sectname */[15] = '\0';

	InP->Head.msg_simple = TRUE;
	InP->Head.msg_size = msg_size;
	InP->Head.msg_type = MSG_TYPE_NORMAL | MSG_TYPE_RPC;
	InP->Head.msg_request_port = boot_port;
	InP->Head.msg_reply_port = mig_get_reply_port();
	InP->Head.msg_id = 201;

	msg_result = msg_rpc(&InP->Head, MSG_OPTION_NONE, sizeof(Reply), 0, 0);
	if (msg_result != RPC_SUCCESS) {
		if (msg_result == RCV_INVALID_PORT)
			mig_dealloc_reply_port();
		return msg_result;
	}

#if	TypeCheck
	msg_size = OutP->Head.msg_size;
	msg_simple = OutP->Head.msg_simple;
#endif	TypeCheck

	if (OutP->Head.msg_id != 301)
		return MIG_REPLY_MISMATCH;

#if	TypeCheck
	if (((msg_size != 48) || (msg_simple != TRUE)) &&
	    ((msg_size != sizeof(death_pill_t)) ||
	     (msg_simple != TRUE) ||
	     (OutP->RetCode == KERN_SUCCESS)))
		return MIG_TYPE_ERROR;
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &OutP->RetCodeType != * (int *) &RetCodeCheck)
#else	UseStaticMsgType
	if ((OutP->RetCodeType.msg_type_inline != TRUE) ||
	    (OutP->RetCodeType.msg_type_longform != FALSE) ||
	    (OutP->RetCodeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (OutP->RetCodeType.msg_type_number != 1) ||
	    (OutP->RetCodeType.msg_type_size != 32))
#endif	UseStaticMsgType
		return MIG_TYPE_ERROR;
#endif	TypeCheck

	if (OutP->RetCode != KERN_SUCCESS)
		return OutP->RetCode;

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &OutP->addrType != * (int *) &addrCheck)
#else	UseStaticMsgType
	if ((OutP->addrType.msg_type_inline != TRUE) ||
	    (OutP->addrType.msg_type_longform != FALSE) ||
	    (OutP->addrType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (OutP->addrType.msg_type_number != 1) ||
	    (OutP->addrType.msg_type_size != 32))
#endif	UseStaticMsgType
		return MIG_TYPE_ERROR;
#endif	TypeCheck

	*addr /* addr */ = /* *addr */ OutP->addr;

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &OutP->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((OutP->sizeType.msg_type_inline != TRUE) ||
	    (OutP->sizeType.msg_type_longform != FALSE) ||
	    (OutP->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (OutP->sizeType.msg_type_number != 1) ||
	    (OutP->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		return MIG_TYPE_ERROR;
#endif	TypeCheck

	*size /* size */ = /* *size */ OutP->size;

	return OutP->RetCode;
}

/* SimpleRoutine kern_serv_log_data */
mig_external kern_return_t kern_serv_log_data
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t log_port,
	log_entry_array_t log,
	unsigned int logCnt
)
#else
	(log_port, log, logCnt)
	port_t log_port;
	log_entry_array_t log;
	unsigned int logCnt;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_long_t logType;
		log_entry_array_t log;
	} Request;

	union {
		Request In;
	} Mess;

	register Request *InP = &Mess.In;

	unsigned int msg_size = 40;

#if	UseStaticMsgType
	static const msg_type_long_t logType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	TRUE,
	},
		/* msg_type_long_name = */	MSG_TYPE_INTEGER_32,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	InP->logType = logType;
#else	UseStaticMsgType
	InP->logType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	InP->logType.msg_type_long_size = 32;
	InP->logType.msg_type_header.msg_type_inline = FALSE;
	InP->logType.msg_type_header.msg_type_longform = TRUE;
	InP->logType.msg_type_header.msg_type_deallocate = TRUE;
#endif	UseStaticMsgType

	InP->log /* log */ = /* log */ log;

	InP->logType.msg_type_long_number /* 8 logCnt */ = /* logType.msg_type_long_number */ 8 * logCnt;

	InP->Head.msg_simple = FALSE;
	InP->Head.msg_size = msg_size;
	InP->Head.msg_type = MSG_TYPE_NORMAL;
	InP->Head.msg_request_port = log_port;
	InP->Head.msg_reply_port = PORT_NULL;
	InP->Head.msg_id = 202;

	return msg_send(&InP->Head, MSG_OPTION_NONE, 0);
}
