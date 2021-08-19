#include "exc.h"
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


/* Routine exception_raise */
mig_external kern_return_t exception_raise
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t exception_port,
	port_t clear_port,
	port_t thread,
	port_t task,
	int exception,
	int code,
	int subcode
)
#else
	(exception_port, clear_port, thread, task, exception, code, subcode)
	port_t exception_port;
	port_t clear_port;
	port_t thread;
	port_t task;
	int exception;
	int code;
	int subcode;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t threadType;
		port_t thread;
		msg_type_t taskType;
		port_t task;
		msg_type_t exceptionType;
		int exception;
		msg_type_t codeType;
		int code;
		msg_type_t subcodeType;
		int subcode;
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

	unsigned int msg_size = 64;

#if	UseStaticMsgType
	static const msg_type_t threadType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t taskType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t exceptionType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t codeType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t subcodeType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
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
	InP->threadType = threadType;
#else	UseStaticMsgType
	InP->threadType.msg_type_name = MSG_TYPE_PORT;
	InP->threadType.msg_type_size = 32;
	InP->threadType.msg_type_number = 1;
	InP->threadType.msg_type_inline = TRUE;
	InP->threadType.msg_type_longform = FALSE;
	InP->threadType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->thread /* thread */ = /* thread */ thread;

#if	UseStaticMsgType
	InP->taskType = taskType;
#else	UseStaticMsgType
	InP->taskType.msg_type_name = MSG_TYPE_PORT;
	InP->taskType.msg_type_size = 32;
	InP->taskType.msg_type_number = 1;
	InP->taskType.msg_type_inline = TRUE;
	InP->taskType.msg_type_longform = FALSE;
	InP->taskType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->task /* task */ = /* task */ task;

#if	UseStaticMsgType
	InP->exceptionType = exceptionType;
#else	UseStaticMsgType
	InP->exceptionType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->exceptionType.msg_type_size = 32;
	InP->exceptionType.msg_type_number = 1;
	InP->exceptionType.msg_type_inline = TRUE;
	InP->exceptionType.msg_type_longform = FALSE;
	InP->exceptionType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->exception /* exception */ = /* exception */ exception;

#if	UseStaticMsgType
	InP->codeType = codeType;
#else	UseStaticMsgType
	InP->codeType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->codeType.msg_type_size = 32;
	InP->codeType.msg_type_number = 1;
	InP->codeType.msg_type_inline = TRUE;
	InP->codeType.msg_type_longform = FALSE;
	InP->codeType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->code /* code */ = /* code */ code;

#if	UseStaticMsgType
	InP->subcodeType = subcodeType;
#else	UseStaticMsgType
	InP->subcodeType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->subcodeType.msg_type_size = 32;
	InP->subcodeType.msg_type_number = 1;
	InP->subcodeType.msg_type_inline = TRUE;
	InP->subcodeType.msg_type_longform = FALSE;
	InP->subcodeType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->subcode /* subcode */ = /* subcode */ subcode;

	InP->Head.msg_simple = FALSE;
	InP->Head.msg_size = msg_size;
	InP->Head.msg_type = MSG_TYPE_NORMAL | MSG_TYPE_RPC;
	InP->Head.msg_request_port = exception_port;
	InP->Head.msg_reply_port = clear_port;
	InP->Head.msg_id = 2400;

	msg_result = msg_rpc(&InP->Head, MSG_OPTION_NONE, sizeof(Reply), 0, 0);
	if (msg_result != RPC_SUCCESS) {
		return msg_result;
	}

#if	TypeCheck
	msg_size = OutP->Head.msg_size;
	msg_simple = OutP->Head.msg_simple;
#endif	TypeCheck

	if (OutP->Head.msg_id != 2500)
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
