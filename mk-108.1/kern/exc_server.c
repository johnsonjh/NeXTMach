/* Module exc */

#define EXPORT_BOOLEAN
#include <sys/boolean.h>
#include <sys/message.h>
#include <sys/mig_errors.h>

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

/* Due to pcc compiler bug, cannot use void */
#if	(defined(__STDC__) || defined(c_plusplus)) || defined(hc)
#define novalue void
#else
#define novalue int
#endif

#define msg_request_port	msg_local_port
#define msg_reply_port		msg_remote_port
#include <kern/std_types.h>

/* Routine exception_raise */
mig_internal novalue _Xexception_raise
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
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

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t catch_exception_raise
#if	(defined(__STDC__) || defined(c_plusplus))
		(port_t exception_port, port_t thread, port_t task, int exception, int code, int subcode);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t threadCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t taskCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t exceptionCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t codeCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t subcodeCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 64) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->threadType != * (int *) &threadCheck)
#else	UseStaticMsgType
	if ((In0P->threadType.msg_type_inline != TRUE) ||
	    (In0P->threadType.msg_type_longform != FALSE) ||
	    (In0P->threadType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->threadType.msg_type_number != 1) ||
	    (In0P->threadType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->taskType != * (int *) &taskCheck)
#else	UseStaticMsgType
	if ((In0P->taskType.msg_type_inline != TRUE) ||
	    (In0P->taskType.msg_type_longform != FALSE) ||
	    (In0P->taskType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->taskType.msg_type_number != 1) ||
	    (In0P->taskType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->exceptionType != * (int *) &exceptionCheck)
#else	UseStaticMsgType
	if ((In0P->exceptionType.msg_type_inline != TRUE) ||
	    (In0P->exceptionType.msg_type_longform != FALSE) ||
	    (In0P->exceptionType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->exceptionType.msg_type_number != 1) ||
	    (In0P->exceptionType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->codeType != * (int *) &codeCheck)
#else	UseStaticMsgType
	if ((In0P->codeType.msg_type_inline != TRUE) ||
	    (In0P->codeType.msg_type_longform != FALSE) ||
	    (In0P->codeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->codeType.msg_type_number != 1) ||
	    (In0P->codeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->subcodeType != * (int *) &subcodeCheck)
#else	UseStaticMsgType
	if ((In0P->subcodeType.msg_type_inline != TRUE) ||
	    (In0P->subcodeType.msg_type_longform != FALSE) ||
	    (In0P->subcodeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->subcodeType.msg_type_number != 1) ||
	    (In0P->subcodeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

	OutP->RetCode = catch_exception_raise(In0P->Head.msg_request_port, In0P->thread, In0P->task, In0P->exception, In0P->code, In0P->subcode);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 32;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

boolean_t exc_server
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	register msg_header_t *InP =  InHeadP;
	register death_pill_t *OutP = (death_pill_t *) OutHeadP;

#if	UseStaticMsgType
	static const msg_type_t RetCodeType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = sizeof *OutP;
	OutP->Head.msg_type = InP->msg_type;
	OutP->Head.msg_local_port = PORT_NULL;
	OutP->Head.msg_remote_port = InP->msg_reply_port;
	OutP->Head.msg_id = InP->msg_id + 100;

#if	UseStaticMsgType
	OutP->RetCodeType = RetCodeType;
#else	UseStaticMsgType
	OutP->RetCodeType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->RetCodeType.msg_type_size = 32;
	OutP->RetCodeType.msg_type_number = 1;
	OutP->RetCodeType.msg_type_inline = TRUE;
	OutP->RetCodeType.msg_type_longform = FALSE;
	OutP->RetCodeType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType
	OutP->RetCode = MIG_BAD_ID;

	if ((InP->msg_id > 2400) || (InP->msg_id < 2400))
		return FALSE;
	else {
		typedef novalue (*SERVER_STUB_PROC)
#if	(defined(__STDC__) || defined(c_plusplus))
			(msg_header_t *, msg_header_t *);
#else
			();
#endif
		static const SERVER_STUB_PROC routines[] = {
			_Xexception_raise,
		};

		if (routines[InP->msg_id - 2400])
			(routines[InP->msg_id - 2400]) (InP, &OutP->Head);
		 else
			return FALSE;
	}
	return TRUE;
}
