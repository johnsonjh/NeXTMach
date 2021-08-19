/* Module mach_host */

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

/* Routine host_processors */
mig_internal novalue _Xhost_processors
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
		msg_type_long_t processor_listType;
		processor_array_t processor_list;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t host_processors
#if	(defined(__STDC__) || defined(c_plusplus))
		(host_t host_priv, processor_array_t *processor_list, unsigned int *processor_listCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_long_t processor_listType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_PORT,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

	unsigned int processor_listCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	OutP->RetCode = host_processors(convert_port_to_host_priv(In0P->Head.msg_request_port), &OutP->processor_list, &processor_listCnt);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 48;

#if	UseStaticMsgType
	OutP->processor_listType = processor_listType;
#else	UseStaticMsgType
	OutP->processor_listType.msg_type_long_name = MSG_TYPE_PORT;
	OutP->processor_listType.msg_type_long_size = 32;
	OutP->processor_listType.msg_type_header.msg_type_inline = FALSE;
	OutP->processor_listType.msg_type_header.msg_type_longform = TRUE;
	OutP->processor_listType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->processor_listType.msg_type_long_number /* processor_listCnt */ = /* processor_listType.msg_type_long_number */ processor_listCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine host_info */
mig_internal novalue _Xhost_info
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t flavorType;
		int flavor;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_long_t host_info_outType;
		int host_info_out[1024];
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t host_info
#if	(defined(__STDC__) || defined(c_plusplus))
		(host_t host, int flavor, host_info_t host_info_out, unsigned int *host_info_outCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;
	unsigned int msg_size_delta;

#if	UseStaticMsgType
	static const msg_type_t flavorCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_long_t host_info_outType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_INTEGER_32,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	1024,
	};
#endif	UseStaticMsgType

	unsigned int host_info_outCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->flavorType != * (int *) &flavorCheck)
#else	UseStaticMsgType
	if ((In0P->flavorType.msg_type_inline != TRUE) ||
	    (In0P->flavorType.msg_type_longform != FALSE) ||
	    (In0P->flavorType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->flavorType.msg_type_number != 1) ||
	    (In0P->flavorType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

	host_info_outCnt = 1024;

	OutP->RetCode = host_info(convert_port_to_host(In0P->Head.msg_request_port), In0P->flavor, OutP->host_info_out, &host_info_outCnt);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 44;

#if	UseStaticMsgType
	OutP->host_info_outType = host_info_outType;
#else	UseStaticMsgType
	OutP->host_info_outType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->host_info_outType.msg_type_long_size = 32;
	OutP->host_info_outType.msg_type_header.msg_type_inline = TRUE;
	OutP->host_info_outType.msg_type_header.msg_type_longform = TRUE;
	OutP->host_info_outType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->host_info_outType.msg_type_long_number /* host_info_outCnt */ = /* host_info_outType.msg_type_long_number */ host_info_outCnt;

	msg_size_delta = (4 * host_info_outCnt);
	msg_size += msg_size_delta;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine processor_info */
mig_internal novalue _Xprocessor_info
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t flavorType;
		int flavor;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t hostType;
		port_t host;
		msg_type_long_t processor_info_outType;
		int processor_info_out[1024];
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_info
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_t processor, int flavor, host_t *host, processor_info_t processor_info_out, unsigned int *processor_info_outCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;
	unsigned int msg_size_delta;

#if	UseStaticMsgType
	static const msg_type_t flavorCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t hostType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_long_t processor_info_outType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_INTEGER_32,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	1024,
	};
#endif	UseStaticMsgType

	host_t host;
	unsigned int processor_info_outCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->flavorType != * (int *) &flavorCheck)
#else	UseStaticMsgType
	if ((In0P->flavorType.msg_type_inline != TRUE) ||
	    (In0P->flavorType.msg_type_longform != FALSE) ||
	    (In0P->flavorType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->flavorType.msg_type_number != 1) ||
	    (In0P->flavorType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

	processor_info_outCnt = 1024;

	OutP->RetCode = processor_info(convert_port_to_processor(In0P->Head.msg_request_port), In0P->flavor, &host, OutP->processor_info_out, &processor_info_outCnt);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 52;

#if	UseStaticMsgType
	OutP->hostType = hostType;
#else	UseStaticMsgType
	OutP->hostType.msg_type_name = MSG_TYPE_PORT;
	OutP->hostType.msg_type_size = 32;
	OutP->hostType.msg_type_number = 1;
	OutP->hostType.msg_type_inline = TRUE;
	OutP->hostType.msg_type_longform = FALSE;
	OutP->hostType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->host /* convert_host_to_port host */ = /* host */ convert_host_to_port(host);

#if	UseStaticMsgType
	OutP->processor_info_outType = processor_info_outType;
#else	UseStaticMsgType
	OutP->processor_info_outType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->processor_info_outType.msg_type_long_size = 32;
	OutP->processor_info_outType.msg_type_header.msg_type_inline = TRUE;
	OutP->processor_info_outType.msg_type_header.msg_type_longform = TRUE;
	OutP->processor_info_outType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->processor_info_outType.msg_type_long_number /* processor_info_outCnt */ = /* processor_info_outType.msg_type_long_number */ processor_info_outCnt;

	msg_size_delta = (4 * processor_info_outCnt);
	msg_size += msg_size_delta;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine processor_start */
mig_internal novalue _Xprocessor_start
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
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_start
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_t processor);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	OutP->RetCode = processor_start(convert_port_to_processor(In0P->Head.msg_request_port));
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

/* Routine processor_exit */
mig_internal novalue _Xprocessor_exit
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
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_exit
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_t processor);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	OutP->RetCode = processor_exit(convert_port_to_processor(In0P->Head.msg_request_port));
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

/* Routine processor_control */
mig_internal novalue _Xprocessor_control
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_long_t processor_cmdType;
		int processor_cmd[1024];
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_control
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_t processor, processor_info_t processor_cmd, unsigned int processor_cmdCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;
	unsigned int msg_size_delta;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size < 36) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

#if	TypeCheck
	if ((In0P->processor_cmdType.msg_type_header.msg_type_inline != TRUE) ||
	    (In0P->processor_cmdType.msg_type_header.msg_type_longform != TRUE) ||
	    (In0P->processor_cmdType.msg_type_long_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->processor_cmdType.msg_type_long_size != 32))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
	msg_size_delta = (4 * In0P->processor_cmdType.msg_type_long_number);
	if (msg_size != 36 + msg_size_delta)
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

	OutP->RetCode = processor_control(convert_port_to_processor(In0P->Head.msg_request_port), In0P->processor_cmd, In0P->processor_cmdType.msg_type_long_number);
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

/* Routine processor_set_default */
mig_internal novalue _Xprocessor_set_default
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
		msg_type_t default_setType;
		port_t default_set;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_default
#if	(defined(__STDC__) || defined(c_plusplus))
		(host_t host, processor_set_t *default_set);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t default_setType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	processor_set_t default_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	OutP->RetCode = processor_set_default(convert_port_to_host(In0P->Head.msg_request_port), &default_set);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->default_setType = default_setType;
#else	UseStaticMsgType
	OutP->default_setType.msg_type_name = MSG_TYPE_PORT;
	OutP->default_setType.msg_type_size = 32;
	OutP->default_setType.msg_type_number = 1;
	OutP->default_setType.msg_type_inline = TRUE;
	OutP->default_setType.msg_type_longform = FALSE;
	OutP->default_setType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->default_set /* convert_pset_name_to_port default_set */ = /* default_set */ convert_pset_name_to_port(default_set);

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine processor_set_create */
mig_internal novalue _Xprocessor_set_create
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
		msg_type_t new_setType;
		port_t new_set;
		msg_type_t new_nameType;
		port_t new_name;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_create
#if	(defined(__STDC__) || defined(c_plusplus))
		(host_t host, port_t *new_set, port_t *new_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t new_setType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t new_nameType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
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
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	OutP->RetCode = processor_set_create(convert_port_to_host(In0P->Head.msg_request_port), &OutP->new_set, &OutP->new_name);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 48;

#if	UseStaticMsgType
	OutP->new_setType = new_setType;
#else	UseStaticMsgType
	OutP->new_setType.msg_type_name = MSG_TYPE_PORT;
	OutP->new_setType.msg_type_size = 32;
	OutP->new_setType.msg_type_number = 1;
	OutP->new_setType.msg_type_inline = TRUE;
	OutP->new_setType.msg_type_longform = FALSE;
	OutP->new_setType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->new_nameType = new_nameType;
#else	UseStaticMsgType
	OutP->new_nameType.msg_type_name = MSG_TYPE_PORT;
	OutP->new_nameType.msg_type_size = 32;
	OutP->new_nameType.msg_type_number = 1;
	OutP->new_nameType.msg_type_inline = TRUE;
	OutP->new_nameType.msg_type_longform = FALSE;
	OutP->new_nameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine processor_set_destroy */
mig_internal novalue _Xprocessor_set_destroy
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
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_destroy
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_set_t set);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	processor_set_t set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	set /* convert_port_to_pset 0 Head.msg_request_port */ = /* set */ convert_port_to_pset(In0P->Head.msg_request_port);

	OutP->RetCode = processor_set_destroy(set);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(set);
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

/* Routine processor_set_info */
mig_internal novalue _Xprocessor_set_info
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t flavorType;
		int flavor;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t hostType;
		port_t host;
		msg_type_long_t info_outType;
		int info_out[1024];
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_info
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_set_t set_name, int flavor, host_t *host, processor_set_info_t info_out, unsigned int *info_outCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;
	unsigned int msg_size_delta;

#if	UseStaticMsgType
	static const msg_type_t flavorCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t hostType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_long_t info_outType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_INTEGER_32,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	1024,
	};
#endif	UseStaticMsgType

	processor_set_t set_name;
	host_t host;
	unsigned int info_outCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	set_name /* convert_port_to_pset_name 0 Head.msg_request_port */ = /* set_name */ convert_port_to_pset_name(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->flavorType != * (int *) &flavorCheck)
#else	UseStaticMsgType
	if ((In0P->flavorType.msg_type_inline != TRUE) ||
	    (In0P->flavorType.msg_type_longform != FALSE) ||
	    (In0P->flavorType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->flavorType.msg_type_number != 1) ||
	    (In0P->flavorType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	info_outCnt = 1024;

	OutP->RetCode = processor_set_info(set_name, In0P->flavor, &host, OutP->info_out, &info_outCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(set_name);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 52;

#if	UseStaticMsgType
	OutP->hostType = hostType;
#else	UseStaticMsgType
	OutP->hostType.msg_type_name = MSG_TYPE_PORT;
	OutP->hostType.msg_type_size = 32;
	OutP->hostType.msg_type_number = 1;
	OutP->hostType.msg_type_inline = TRUE;
	OutP->hostType.msg_type_longform = FALSE;
	OutP->hostType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->host /* convert_host_to_port host */ = /* host */ convert_host_to_port(host);

#if	UseStaticMsgType
	OutP->info_outType = info_outType;
#else	UseStaticMsgType
	OutP->info_outType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->info_outType.msg_type_long_size = 32;
	OutP->info_outType.msg_type_header.msg_type_inline = TRUE;
	OutP->info_outType.msg_type_header.msg_type_longform = TRUE;
	OutP->info_outType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->info_outType.msg_type_long_number /* info_outCnt */ = /* info_outType.msg_type_long_number */ info_outCnt;

	msg_size_delta = (4 * info_outCnt);
	msg_size += msg_size_delta;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine processor_assign */
mig_internal novalue _Xprocessor_assign
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t new_setType;
		port_t new_set;
		msg_type_t waitType;
		boolean_t wait;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_assign
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_t processor, processor_set_t new_set, boolean_t wait);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t new_setCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t waitCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	processor_set_t new_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->new_setType != * (int *) &new_setCheck)
#else	UseStaticMsgType
	if ((In0P->new_setType.msg_type_inline != TRUE) ||
	    (In0P->new_setType.msg_type_longform != FALSE) ||
	    (In0P->new_setType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->new_setType.msg_type_number != 1) ||
	    (In0P->new_setType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

	new_set /* convert_port_to_pset 0 new_set */ = /* new_set */ convert_port_to_pset(In0P->new_set);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->waitType != * (int *) &waitCheck)
#else	UseStaticMsgType
	if ((In0P->waitType.msg_type_inline != TRUE) ||
	    (In0P->waitType.msg_type_longform != FALSE) ||
	    (In0P->waitType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->waitType.msg_type_number != 1) ||
	    (In0P->waitType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = processor_assign(convert_port_to_processor(In0P->Head.msg_request_port), new_set, In0P->wait);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(new_set);
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

/* Routine processor_get_assignment */
mig_internal novalue _Xprocessor_get_assignment
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
		msg_type_t assigned_setType;
		port_t assigned_set;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_get_assignment
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_t processor, processor_set_t *assigned_set);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t assigned_setType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	processor_set_t assigned_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	OutP->RetCode = processor_get_assignment(convert_port_to_processor(In0P->Head.msg_request_port), &assigned_set);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->assigned_setType = assigned_setType;
#else	UseStaticMsgType
	OutP->assigned_setType.msg_type_name = MSG_TYPE_PORT;
	OutP->assigned_setType.msg_type_size = 32;
	OutP->assigned_setType.msg_type_number = 1;
	OutP->assigned_setType.msg_type_inline = TRUE;
	OutP->assigned_setType.msg_type_longform = FALSE;
	OutP->assigned_setType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->assigned_set /* convert_pset_name_to_port assigned_set */ = /* assigned_set */ convert_pset_name_to_port(assigned_set);

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine thread_assign */
mig_internal novalue _Xthread_assign
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t new_setType;
		port_t new_set;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_assign
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t thread, processor_set_t new_set);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t new_setCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	thread_t thread;
	processor_set_t new_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* thread */ convert_port_to_thread(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->new_setType != * (int *) &new_setCheck)
#else	UseStaticMsgType
	if ((In0P->new_setType.msg_type_inline != TRUE) ||
	    (In0P->new_setType.msg_type_longform != FALSE) ||
	    (In0P->new_setType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->new_setType.msg_type_number != 1) ||
	    (In0P->new_setType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	new_set /* convert_port_to_pset 0 new_set */ = /* new_set */ convert_port_to_pset(In0P->new_set);

	OutP->RetCode = thread_assign(thread, new_set);
#ifdef	label_punt2
#undef	label_punt2
punt2:
#endif	label_punt2
	pset_deallocate(new_set);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(thread);
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

/* Routine thread_assign_default */
mig_internal novalue _Xthread_assign_default
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
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_assign_default
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t thread);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	thread_t thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* thread */ convert_port_to_thread(In0P->Head.msg_request_port);

	OutP->RetCode = thread_assign_default(thread);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(thread);
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

/* Routine thread_get_assignment */
mig_internal novalue _Xthread_get_assignment
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
		msg_type_t assigned_setType;
		port_t assigned_set;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_get_assignment
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t thread, processor_set_t *assigned_set);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t assigned_setType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	thread_t thread;
	processor_set_t assigned_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* thread */ convert_port_to_thread(In0P->Head.msg_request_port);

	OutP->RetCode = thread_get_assignment(thread, &assigned_set);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(thread);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->assigned_setType = assigned_setType;
#else	UseStaticMsgType
	OutP->assigned_setType.msg_type_name = MSG_TYPE_PORT;
	OutP->assigned_setType.msg_type_size = 32;
	OutP->assigned_setType.msg_type_number = 1;
	OutP->assigned_setType.msg_type_inline = TRUE;
	OutP->assigned_setType.msg_type_longform = FALSE;
	OutP->assigned_setType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->assigned_set /* convert_pset_name_to_port assigned_set */ = /* assigned_set */ convert_pset_name_to_port(assigned_set);

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine task_assign */
mig_internal novalue _Xtask_assign
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t new_setType;
		port_t new_set;
		msg_type_t assign_threadsType;
		boolean_t assign_threads;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_assign
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, processor_set_t new_set, boolean_t assign_threads);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t new_setCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t assign_threadsCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t task;
	processor_set_t new_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->new_setType != * (int *) &new_setCheck)
#else	UseStaticMsgType
	if ((In0P->new_setType.msg_type_inline != TRUE) ||
	    (In0P->new_setType.msg_type_longform != FALSE) ||
	    (In0P->new_setType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->new_setType.msg_type_number != 1) ||
	    (In0P->new_setType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	new_set /* convert_port_to_pset 0 new_set */ = /* new_set */ convert_port_to_pset(In0P->new_set);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->assign_threadsType != * (int *) &assign_threadsCheck)
#else	UseStaticMsgType
	if ((In0P->assign_threadsType.msg_type_inline != TRUE) ||
	    (In0P->assign_threadsType.msg_type_longform != FALSE) ||
	    (In0P->assign_threadsType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->assign_threadsType.msg_type_number != 1) ||
	    (In0P->assign_threadsType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt2; }
#define	label_punt2
#endif	TypeCheck

	OutP->RetCode = task_assign(task, new_set, In0P->assign_threads);
#ifdef	label_punt2
#undef	label_punt2
punt2:
#endif	label_punt2
	pset_deallocate(new_set);
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

	msg_size = 32;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine task_assign_default */
mig_internal novalue _Xtask_assign_default
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t assign_threadsType;
		boolean_t assign_threads;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_assign_default
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, boolean_t assign_threads);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t assign_threadsCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->assign_threadsType != * (int *) &assign_threadsCheck)
#else	UseStaticMsgType
	if ((In0P->assign_threadsType.msg_type_inline != TRUE) ||
	    (In0P->assign_threadsType.msg_type_longform != FALSE) ||
	    (In0P->assign_threadsType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->assign_threadsType.msg_type_number != 1) ||
	    (In0P->assign_threadsType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = task_assign_default(task, In0P->assign_threads);
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

	msg_size = 32;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine task_get_assignment */
mig_internal novalue _Xtask_get_assignment
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
		msg_type_t assigned_setType;
		port_t assigned_set;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_get_assignment
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, processor_set_t *assigned_set);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t assigned_setType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t task;
	processor_set_t assigned_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = task_get_assignment(task, &assigned_set);
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

	msg_size = 40;

#if	UseStaticMsgType
	OutP->assigned_setType = assigned_setType;
#else	UseStaticMsgType
	OutP->assigned_setType.msg_type_name = MSG_TYPE_PORT;
	OutP->assigned_setType.msg_type_size = 32;
	OutP->assigned_setType.msg_type_number = 1;
	OutP->assigned_setType.msg_type_inline = TRUE;
	OutP->assigned_setType.msg_type_longform = FALSE;
	OutP->assigned_setType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->assigned_set /* convert_pset_name_to_port assigned_set */ = /* assigned_set */ convert_pset_name_to_port(assigned_set);

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine host_kernel_version */
mig_internal novalue _Xhost_kernel_version
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
		msg_type_long_t kernel_versionType;
		kernel_version_t kernel_version;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t host_kernel_version
#if	(defined(__STDC__) || defined(c_plusplus))
		(host_t host, kernel_version_t kernel_version);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_long_t kernel_versionType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_STRING,
		/* msg_type_long_size = */	4096,
		/* msg_type_long_number = */	1,
	};
#endif	UseStaticMsgType

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	OutP->RetCode = host_kernel_version(convert_port_to_host(In0P->Head.msg_request_port), OutP->kernel_version);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 556;

#if	UseStaticMsgType
	OutP->kernel_versionType = kernel_versionType;
#else	UseStaticMsgType
	OutP->kernel_versionType.msg_type_long_name = MSG_TYPE_STRING;
	OutP->kernel_versionType.msg_type_long_size = 4096;
	OutP->kernel_versionType.msg_type_long_number = 1;
	OutP->kernel_versionType.msg_type_header.msg_type_inline = TRUE;
	OutP->kernel_versionType.msg_type_header.msg_type_longform = TRUE;
	OutP->kernel_versionType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine thread_priority */
mig_internal novalue _Xthread_priority
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t priorityType;
		int priority;
		msg_type_t set_maxType;
		boolean_t set_max;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_priority
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t thread, int priority, boolean_t set_max);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t priorityCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t set_maxCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	thread_t thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* thread */ convert_port_to_thread(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->priorityType != * (int *) &priorityCheck)
#else	UseStaticMsgType
	if ((In0P->priorityType.msg_type_inline != TRUE) ||
	    (In0P->priorityType.msg_type_longform != FALSE) ||
	    (In0P->priorityType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->priorityType.msg_type_number != 1) ||
	    (In0P->priorityType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->set_maxType != * (int *) &set_maxCheck)
#else	UseStaticMsgType
	if ((In0P->set_maxType.msg_type_inline != TRUE) ||
	    (In0P->set_maxType.msg_type_longform != FALSE) ||
	    (In0P->set_maxType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->set_maxType.msg_type_number != 1) ||
	    (In0P->set_maxType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = thread_priority(thread, In0P->priority, In0P->set_max);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(thread);
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

/* Routine thread_max_priority */
mig_internal novalue _Xthread_max_priority
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t processor_setType;
		port_t processor_set;
		msg_type_t max_priorityType;
		int max_priority;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_max_priority
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t thread, processor_set_t processor_set, int max_priority);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t processor_setCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t max_priorityCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	thread_t thread;
	processor_set_t processor_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* thread */ convert_port_to_thread(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->processor_setType != * (int *) &processor_setCheck)
#else	UseStaticMsgType
	if ((In0P->processor_setType.msg_type_inline != TRUE) ||
	    (In0P->processor_setType.msg_type_longform != FALSE) ||
	    (In0P->processor_setType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->processor_setType.msg_type_number != 1) ||
	    (In0P->processor_setType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	processor_set /* convert_port_to_pset 0 processor_set */ = /* processor_set */ convert_port_to_pset(In0P->processor_set);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->max_priorityType != * (int *) &max_priorityCheck)
#else	UseStaticMsgType
	if ((In0P->max_priorityType.msg_type_inline != TRUE) ||
	    (In0P->max_priorityType.msg_type_longform != FALSE) ||
	    (In0P->max_priorityType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->max_priorityType.msg_type_number != 1) ||
	    (In0P->max_priorityType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt2; }
#define	label_punt2
#endif	TypeCheck

	OutP->RetCode = thread_max_priority(thread, processor_set, In0P->max_priority);
#ifdef	label_punt2
#undef	label_punt2
punt2:
#endif	label_punt2
	pset_deallocate(processor_set);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(thread);
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

/* Routine task_priority */
mig_internal novalue _Xtask_priority
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t priorityType;
		int priority;
		msg_type_t change_threadsType;
		boolean_t change_threads;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_priority
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, int priority, boolean_t change_threads);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t priorityCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t change_threadsCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->priorityType != * (int *) &priorityCheck)
#else	UseStaticMsgType
	if ((In0P->priorityType.msg_type_inline != TRUE) ||
	    (In0P->priorityType.msg_type_longform != FALSE) ||
	    (In0P->priorityType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->priorityType.msg_type_number != 1) ||
	    (In0P->priorityType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->change_threadsType != * (int *) &change_threadsCheck)
#else	UseStaticMsgType
	if ((In0P->change_threadsType.msg_type_inline != TRUE) ||
	    (In0P->change_threadsType.msg_type_longform != FALSE) ||
	    (In0P->change_threadsType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->change_threadsType.msg_type_number != 1) ||
	    (In0P->change_threadsType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = task_priority(task, In0P->priority, In0P->change_threads);
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

	msg_size = 32;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine processor_set_max_priority */
mig_internal novalue _Xprocessor_set_max_priority
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t max_priorityType;
		int max_priority;
		msg_type_t change_threadsType;
		boolean_t change_threads;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_max_priority
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_set_t processor_set, int max_priority, boolean_t change_threads);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t max_priorityCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t change_threadsCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	processor_set_t processor_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	processor_set /* convert_port_to_pset 0 Head.msg_request_port */ = /* processor_set */ convert_port_to_pset(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->max_priorityType != * (int *) &max_priorityCheck)
#else	UseStaticMsgType
	if ((In0P->max_priorityType.msg_type_inline != TRUE) ||
	    (In0P->max_priorityType.msg_type_longform != FALSE) ||
	    (In0P->max_priorityType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->max_priorityType.msg_type_number != 1) ||
	    (In0P->max_priorityType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->change_threadsType != * (int *) &change_threadsCheck)
#else	UseStaticMsgType
	if ((In0P->change_threadsType.msg_type_inline != TRUE) ||
	    (In0P->change_threadsType.msg_type_longform != FALSE) ||
	    (In0P->change_threadsType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->change_threadsType.msg_type_number != 1) ||
	    (In0P->change_threadsType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = processor_set_max_priority(processor_set, In0P->max_priority, In0P->change_threads);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(processor_set);
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

/* Routine thread_policy */
mig_internal novalue _Xthread_policy
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t policyType;
		int policy;
		msg_type_t dataType;
		int data;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_policy
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t thread, int policy, int data);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t policyCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t dataCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	thread_t thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* thread */ convert_port_to_thread(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->policyType != * (int *) &policyCheck)
#else	UseStaticMsgType
	if ((In0P->policyType.msg_type_inline != TRUE) ||
	    (In0P->policyType.msg_type_longform != FALSE) ||
	    (In0P->policyType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->policyType.msg_type_number != 1) ||
	    (In0P->policyType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->dataType != * (int *) &dataCheck)
#else	UseStaticMsgType
	if ((In0P->dataType.msg_type_inline != TRUE) ||
	    (In0P->dataType.msg_type_longform != FALSE) ||
	    (In0P->dataType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->dataType.msg_type_number != 1) ||
	    (In0P->dataType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = thread_policy(thread, In0P->policy, In0P->data);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(thread);
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

/* Routine processor_set_policy_enable */
mig_internal novalue _Xprocessor_set_policy_enable
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t policyType;
		int policy;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_policy_enable
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_set_t processor_set, int policy);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t policyCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	processor_set_t processor_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	processor_set /* convert_port_to_pset 0 Head.msg_request_port */ = /* processor_set */ convert_port_to_pset(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->policyType != * (int *) &policyCheck)
#else	UseStaticMsgType
	if ((In0P->policyType.msg_type_inline != TRUE) ||
	    (In0P->policyType.msg_type_longform != FALSE) ||
	    (In0P->policyType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->policyType.msg_type_number != 1) ||
	    (In0P->policyType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = processor_set_policy_enable(processor_set, In0P->policy);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(processor_set);
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

/* Routine processor_set_policy_disable */
mig_internal novalue _Xprocessor_set_policy_disable
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t policyType;
		int policy;
		msg_type_t change_threadsType;
		boolean_t change_threads;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_policy_disable
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_set_t processor_set, int policy, boolean_t change_threads);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t policyCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t change_threadsCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	processor_set_t processor_set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	processor_set /* convert_port_to_pset 0 Head.msg_request_port */ = /* processor_set */ convert_port_to_pset(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->policyType != * (int *) &policyCheck)
#else	UseStaticMsgType
	if ((In0P->policyType.msg_type_inline != TRUE) ||
	    (In0P->policyType.msg_type_longform != FALSE) ||
	    (In0P->policyType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->policyType.msg_type_number != 1) ||
	    (In0P->policyType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->change_threadsType != * (int *) &change_threadsCheck)
#else	UseStaticMsgType
	if ((In0P->change_threadsType.msg_type_inline != TRUE) ||
	    (In0P->change_threadsType.msg_type_longform != FALSE) ||
	    (In0P->change_threadsType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->change_threadsType.msg_type_number != 1) ||
	    (In0P->change_threadsType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = processor_set_policy_disable(processor_set, In0P->policy, In0P->change_threads);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(processor_set);
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

/* Routine processor_set_tasks */
mig_internal novalue _Xprocessor_set_tasks
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
		msg_type_long_t task_listType;
		task_array_t task_list;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_tasks
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_set_t processor_set, task_array_t *task_list, unsigned int *task_listCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_long_t task_listType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_PORT,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

	processor_set_t processor_set;
	unsigned int task_listCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	processor_set /* convert_port_to_pset 0 Head.msg_request_port */ = /* processor_set */ convert_port_to_pset(In0P->Head.msg_request_port);

	OutP->RetCode = processor_set_tasks(processor_set, &OutP->task_list, &task_listCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(processor_set);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 48;

#if	UseStaticMsgType
	OutP->task_listType = task_listType;
#else	UseStaticMsgType
	OutP->task_listType.msg_type_long_name = MSG_TYPE_PORT;
	OutP->task_listType.msg_type_long_size = 32;
	OutP->task_listType.msg_type_header.msg_type_inline = FALSE;
	OutP->task_listType.msg_type_header.msg_type_longform = TRUE;
	OutP->task_listType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->task_listType.msg_type_long_number /* task_listCnt */ = /* task_listType.msg_type_long_number */ task_listCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine processor_set_threads */
mig_internal novalue _Xprocessor_set_threads
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
		msg_type_long_t thread_listType;
		thread_array_t thread_list;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t processor_set_threads
#if	(defined(__STDC__) || defined(c_plusplus))
		(processor_set_t processor_set, thread_array_t *thread_list, unsigned int *thread_listCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_long_t thread_listType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_PORT,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

	processor_set_t processor_set;
	unsigned int thread_listCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	processor_set /* convert_port_to_pset 0 Head.msg_request_port */ = /* processor_set */ convert_port_to_pset(In0P->Head.msg_request_port);

	OutP->RetCode = processor_set_threads(processor_set, &OutP->thread_list, &thread_listCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(processor_set);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 48;

#if	UseStaticMsgType
	OutP->thread_listType = thread_listType;
#else	UseStaticMsgType
	OutP->thread_listType.msg_type_long_name = MSG_TYPE_PORT;
	OutP->thread_listType.msg_type_long_size = 32;
	OutP->thread_listType.msg_type_header.msg_type_inline = FALSE;
	OutP->thread_listType.msg_type_header.msg_type_longform = TRUE;
	OutP->thread_listType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->thread_listType.msg_type_long_number /* thread_listCnt */ = /* thread_listType.msg_type_long_number */ thread_listCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine host_processor_sets */
mig_internal novalue _Xhost_processor_sets
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
		msg_type_long_t processor_set_namesType;
		processor_set_name_array_t processor_set_names;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t host_processor_sets
#if	(defined(__STDC__) || defined(c_plusplus))
		(host_t host, processor_set_name_array_t *processor_set_names, unsigned int *processor_set_namesCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_long_t processor_set_namesType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_PORT,
		/* msg_type_long_size = */	32,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

	unsigned int processor_set_namesCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	OutP->RetCode = host_processor_sets(convert_port_to_host(In0P->Head.msg_request_port), &OutP->processor_set_names, &processor_set_namesCnt);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 48;

#if	UseStaticMsgType
	OutP->processor_set_namesType = processor_set_namesType;
#else	UseStaticMsgType
	OutP->processor_set_namesType.msg_type_long_name = MSG_TYPE_PORT;
	OutP->processor_set_namesType.msg_type_long_size = 32;
	OutP->processor_set_namesType.msg_type_header.msg_type_inline = FALSE;
	OutP->processor_set_namesType.msg_type_header.msg_type_longform = TRUE;
	OutP->processor_set_namesType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->processor_set_namesType.msg_type_long_number /* processor_set_namesCnt */ = /* processor_set_namesType.msg_type_long_number */ processor_set_namesCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine host_processor_set_priv */
mig_internal novalue _Xhost_processor_set_priv
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t set_nameType;
		port_t set_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t setType;
		port_t set;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t host_processor_set_priv
#if	(defined(__STDC__) || defined(c_plusplus))
		(host_t host_priv, processor_set_t set_name, processor_set_t *set);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t set_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t setType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	processor_set_t set_name;
	processor_set_t set;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->set_nameType != * (int *) &set_nameCheck)
#else	UseStaticMsgType
	if ((In0P->set_nameType.msg_type_inline != TRUE) ||
	    (In0P->set_nameType.msg_type_longform != FALSE) ||
	    (In0P->set_nameType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->set_nameType.msg_type_number != 1) ||
	    (In0P->set_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

	set_name /* convert_port_to_pset_name 0 set_name */ = /* set_name */ convert_port_to_pset_name(In0P->set_name);

	OutP->RetCode = host_processor_set_priv(convert_port_to_host_priv(In0P->Head.msg_request_port), set_name, &set);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	pset_deallocate(set_name);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->setType = setType;
#else	UseStaticMsgType
	OutP->setType.msg_type_name = MSG_TYPE_PORT;
	OutP->setType.msg_type_size = 32;
	OutP->setType.msg_type_number = 1;
	OutP->setType.msg_type_inline = TRUE;
	OutP->setType.msg_type_longform = FALSE;
	OutP->setType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->set /* convert_pset_to_port set */ = /* set */ convert_pset_to_port(set);

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

boolean_t mach_host_server
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

	if ((InP->msg_id > 2629) || (InP->msg_id < 2600))
		return FALSE;
	else {
		typedef novalue (*SERVER_STUB_PROC)
#if	(defined(__STDC__) || defined(c_plusplus))
			(msg_header_t *, msg_header_t *);
#else
			();
#endif
		static const SERVER_STUB_PROC routines[] = {
			_Xhost_processors,
			_Xhost_info,
			_Xprocessor_info,
			_Xprocessor_start,
			_Xprocessor_exit,
			_Xprocessor_control,
			_Xprocessor_set_default,
			_Xprocessor_set_create,
			_Xprocessor_set_destroy,
			_Xprocessor_set_info,
			_Xprocessor_assign,
			_Xprocessor_get_assignment,
			_Xthread_assign,
			_Xthread_assign_default,
			_Xthread_get_assignment,
			_Xtask_assign,
			_Xtask_assign_default,
			_Xtask_get_assignment,
			_Xhost_kernel_version,
			_Xthread_priority,
			_Xthread_max_priority,
			_Xtask_priority,
			_Xprocessor_set_max_priority,
			_Xthread_policy,
			_Xprocessor_set_policy_enable,
			_Xprocessor_set_policy_disable,
			_Xprocessor_set_tasks,
			_Xprocessor_set_threads,
			_Xhost_processor_sets,
			_Xhost_processor_set_priv,
		};

		if (routines[InP->msg_id - 2600])
			(routines[InP->msg_id - 2600]) (InP, &OutP->Head);
		 else
			return FALSE;
	}
	return TRUE;
}
