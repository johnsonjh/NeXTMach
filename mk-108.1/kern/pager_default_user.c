#include "pager_default.h"
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


/* SimpleRoutine pager_create */
mig_external kern_return_t pager_create
#if	(defined(__STDC__) || defined(c_plusplus))
(
	paging_object_t old_paging_object,
	port_all_t new_paging_object,
	port_t new_request_port,
	port_t new_name,
	vm_size_t new_page_size
)
#else
	(old_paging_object, new_paging_object, new_request_port, new_name, new_page_size)
	paging_object_t old_paging_object;
	port_all_t new_paging_object;
	port_t new_request_port;
	port_t new_name;
	vm_size_t new_page_size;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t new_paging_objectType;
		port_all_t new_paging_object;
		msg_type_t new_request_portType;
		port_t new_request_port;
		msg_type_t new_nameType;
		port_t new_name;
		msg_type_t new_page_sizeType;
		vm_size_t new_page_size;
	} Request;

	union {
		Request In;
	} Mess;

	register Request *InP = &Mess.In;

	unsigned int msg_size = 56;

#if	UseStaticMsgType
	static const msg_type_t new_paging_objectType = {
		/* msg_type_name = */		MSG_TYPE_PORT_ALL,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t new_request_portType = {
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

#if	UseStaticMsgType
	static const msg_type_t new_page_sizeType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	InP->new_paging_objectType = new_paging_objectType;
#else	UseStaticMsgType
	InP->new_paging_objectType.msg_type_name = MSG_TYPE_PORT_ALL;
	InP->new_paging_objectType.msg_type_size = 32;
	InP->new_paging_objectType.msg_type_number = 1;
	InP->new_paging_objectType.msg_type_inline = TRUE;
	InP->new_paging_objectType.msg_type_longform = FALSE;
	InP->new_paging_objectType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->new_paging_object /* new_paging_object */ = /* new_paging_object */ new_paging_object;

#if	UseStaticMsgType
	InP->new_request_portType = new_request_portType;
#else	UseStaticMsgType
	InP->new_request_portType.msg_type_name = MSG_TYPE_PORT;
	InP->new_request_portType.msg_type_size = 32;
	InP->new_request_portType.msg_type_number = 1;
	InP->new_request_portType.msg_type_inline = TRUE;
	InP->new_request_portType.msg_type_longform = FALSE;
	InP->new_request_portType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->new_request_port /* new_request_port */ = /* new_request_port */ new_request_port;

#if	UseStaticMsgType
	InP->new_nameType = new_nameType;
#else	UseStaticMsgType
	InP->new_nameType.msg_type_name = MSG_TYPE_PORT;
	InP->new_nameType.msg_type_size = 32;
	InP->new_nameType.msg_type_number = 1;
	InP->new_nameType.msg_type_inline = TRUE;
	InP->new_nameType.msg_type_longform = FALSE;
	InP->new_nameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->new_name /* new_name */ = /* new_name */ new_name;

#if	UseStaticMsgType
	InP->new_page_sizeType = new_page_sizeType;
#else	UseStaticMsgType
	InP->new_page_sizeType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->new_page_sizeType.msg_type_size = 32;
	InP->new_page_sizeType.msg_type_number = 1;
	InP->new_page_sizeType.msg_type_inline = TRUE;
	InP->new_page_sizeType.msg_type_longform = FALSE;
	InP->new_page_sizeType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->new_page_size /* new_page_size */ = /* new_page_size */ new_page_size;

	InP->Head.msg_simple = FALSE;
	InP->Head.msg_size = msg_size;
	InP->Head.msg_type = MSG_TYPE_NORMAL;
	InP->Head.msg_request_port = old_paging_object;
	InP->Head.msg_reply_port = PORT_NULL;
	InP->Head.msg_id = 2250;

	return msg_send(&InP->Head, MSG_OPTION_NONE, 0);
}

/* SimpleRoutine pager_data_initialize */
mig_external kern_return_t pager_data_initialize
#if	(defined(__STDC__) || defined(c_plusplus))
(
	paging_object_t paging_object,
	port_t pager_request_port,
	vm_offset_t offset,
	pointer_t data,
	unsigned int dataCnt
)
#else
	(paging_object, pager_request_port, offset, data, dataCnt)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	pointer_t data;
	unsigned int dataCnt;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t pager_request_portType;
		port_t pager_request_port;
		msg_type_t offsetType;
		vm_offset_t offset;
		msg_type_long_t dataType;
		pointer_t data;
	} Request;

	union {
		Request In;
	} Mess;

	register Request *InP = &Mess.In;

	unsigned int msg_size = 56;

#if	UseStaticMsgType
	static const msg_type_t pager_request_portType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t offsetType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_long_t dataType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_INTEGER_8,
		/* msg_type_long_size = */	8,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	InP->pager_request_portType = pager_request_portType;
#else	UseStaticMsgType
	InP->pager_request_portType.msg_type_name = MSG_TYPE_PORT;
	InP->pager_request_portType.msg_type_size = 32;
	InP->pager_request_portType.msg_type_number = 1;
	InP->pager_request_portType.msg_type_inline = TRUE;
	InP->pager_request_portType.msg_type_longform = FALSE;
	InP->pager_request_portType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->pager_request_port /* pager_request_port */ = /* pager_request_port */ pager_request_port;

#if	UseStaticMsgType
	InP->offsetType = offsetType;
#else	UseStaticMsgType
	InP->offsetType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->offsetType.msg_type_size = 32;
	InP->offsetType.msg_type_number = 1;
	InP->offsetType.msg_type_inline = TRUE;
	InP->offsetType.msg_type_longform = FALSE;
	InP->offsetType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->offset /* offset */ = /* offset */ offset;

#if	UseStaticMsgType
	InP->dataType = dataType;
#else	UseStaticMsgType
	InP->dataType.msg_type_long_name = MSG_TYPE_INTEGER_8;
	InP->dataType.msg_type_long_size = 8;
	InP->dataType.msg_type_header.msg_type_inline = FALSE;
	InP->dataType.msg_type_header.msg_type_longform = TRUE;
	InP->dataType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->data /* data */ = /* data */ data;

	InP->dataType.msg_type_long_number /* dataCnt */ = /* dataType.msg_type_long_number */ dataCnt;

	InP->Head.msg_simple = FALSE;
	InP->Head.msg_size = msg_size;
	InP->Head.msg_type = MSG_TYPE_NORMAL;
	InP->Head.msg_request_port = paging_object;
	InP->Head.msg_reply_port = PORT_NULL;
	InP->Head.msg_id = 2251;

	return msg_send(&InP->Head, MSG_OPTION_NONE, 0);
}
