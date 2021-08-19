/* Module mach_debug */

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
#include <kern/type_conversion.h>
#include <kern/mach_types.h>
#include <kern/mach_debug_types.h>

/* Routine host_zone_info */
mig_internal novalue _Xhost_zone_info
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_long_t namesType;
		zone_name_array_t names;
		msg_type_long_t infoType;
		zone_info_array_t info;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t host_zone_info
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, zone_name_array_t *names, unsigned int *namesCnt, zone_info_array_t *info, unsigned int *infoCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_long_t namesType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_CHAR,
		/* msg_type_long_size = */	8,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_long_t infoType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_INTEGER_32,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

	task_t task;
	unsigned int namesCnt;
	unsigned int infoCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = host_zone_info(task, &OutP->names, &namesCnt, &OutP->info, &infoCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 64;

#if	UseStaticMsgType
	OutP->namesType = namesType;
#else	UseStaticMsgType
	OutP->namesType.msg_type_long_name = MSG_TYPE_CHAR;
	OutP->namesType.msg_type_long_size = 8;
	OutP->namesType.msg_type_header.msg_type_inline = FALSE;
	OutP->namesType.msg_type_header.msg_type_longform = TRUE;
	OutP->namesType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->namesType.msg_type_long_number /* 80 namesCnt */ = /* namesType.msg_type_long_number */ 80 * namesCnt;

#if	UseStaticMsgType
	OutP->infoType = infoType;
#else	UseStaticMsgType
	OutP->infoType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->infoType.msg_type_long_size = 32;
	OutP->infoType.msg_type_header.msg_type_inline = FALSE;
	OutP->infoType.msg_type_header.msg_type_longform = TRUE;
	OutP->infoType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->infoType.msg_type_long_number /* 8 infoCnt */ = /* infoType.msg_type_long_number */ 8 * infoCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

boolean_t mach_debug_server
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

	if ((InP->msg_id > 3006) || (InP->msg_id < 3000))
		return FALSE;
	else {
		typedef novalue (*SERVER_STUB_PROC)
#if	(defined(__STDC__) || defined(c_plusplus))
			(msg_header_t *, msg_header_t *);
#else
			();
#endif
		static const SERVER_STUB_PROC routines[] = {
			0,
			0,
			0,
			0,
			0,
			_Xhost_zone_info,
			0,
		};

		if (routines[InP->msg_id - 3000])
			(routines[InP->msg_id - 3000]) (InP, &OutP->Head);
		 else
			return FALSE;
	}
	return TRUE;
}
