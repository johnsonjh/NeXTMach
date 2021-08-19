/* Module mach */

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
#include <kern/mach_redefines.h>

/* Routine task_create */
mig_internal novalue _Xtask_create
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t inherit_memoryType;
		boolean_t inherit_memory;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t child_taskType;
		port_t child_task;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_create
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, boolean_t inherit_memory, task_t *child_task);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t inherit_memoryCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t child_taskType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t target_task;
	task_t child_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->inherit_memoryType != * (int *) &inherit_memoryCheck)
#else	UseStaticMsgType
	if ((In0P->inherit_memoryType.msg_type_inline != TRUE) ||
	    (In0P->inherit_memoryType.msg_type_longform != FALSE) ||
	    (In0P->inherit_memoryType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->inherit_memoryType.msg_type_number != 1) ||
	    (In0P->inherit_memoryType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = task_create(target_task, In0P->inherit_memory, &child_task);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->child_taskType = child_taskType;
#else	UseStaticMsgType
	OutP->child_taskType.msg_type_name = MSG_TYPE_PORT;
	OutP->child_taskType.msg_type_size = 32;
	OutP->child_taskType.msg_type_number = 1;
	OutP->child_taskType.msg_type_inline = TRUE;
	OutP->child_taskType.msg_type_longform = FALSE;
	OutP->child_taskType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->child_task /* convert_task_to_port child_task */ = /* child_task */ convert_task_to_port(child_task);

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine task_terminate */
mig_internal novalue _Xtask_terminate
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
	extern kern_return_t task_terminate
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	task_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = task_terminate(target_task);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
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

/* Routine task_threads */
mig_internal novalue _Xtask_threads
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
	extern kern_return_t task_threads
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, thread_array_t *thread_list, unsigned int *thread_listCnt);
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

	task_t target_task;
	unsigned int thread_listCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = task_threads(target_task, &OutP->thread_list, &thread_listCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
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

/* Routine thread_terminate */
mig_internal novalue _Xthread_terminate
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
	extern kern_return_t thread_terminate
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t target_thread);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	thread_t target_thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* target_thread */ convert_port_to_thread(In0P->Head.msg_request_port);

	OutP->RetCode = thread_terminate(target_thread);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(target_thread);
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

/* Routine vm_allocate */
mig_internal novalue _Xvm_allocate
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_t sizeType;
		vm_size_t size;
		msg_type_t anywhereType;
		boolean_t anywhere;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t addressType;
		vm_address_t address;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_allocate
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t *address, vm_size_t size, boolean_t anywhere);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
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
	static const msg_type_t anywhereCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t addressType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 48) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((In0P->sizeType.msg_type_inline != TRUE) ||
	    (In0P->sizeType.msg_type_longform != FALSE) ||
	    (In0P->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->sizeType.msg_type_number != 1) ||
	    (In0P->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->anywhereType != * (int *) &anywhereCheck)
#else	UseStaticMsgType
	if ((In0P->anywhereType.msg_type_inline != TRUE) ||
	    (In0P->anywhereType.msg_type_longform != FALSE) ||
	    (In0P->anywhereType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->anywhereType.msg_type_number != 1) ||
	    (In0P->anywhereType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_allocate(target_task, &In0P->address, In0P->size, In0P->anywhere);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->addressType = addressType;
#else	UseStaticMsgType
	OutP->addressType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->addressType.msg_type_size = 32;
	OutP->addressType.msg_type_number = 1;
	OutP->addressType.msg_type_inline = TRUE;
	OutP->addressType.msg_type_longform = FALSE;
	OutP->addressType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	/* 0 */ OutP->address = In0P->address;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine vm_allocate_with_pager */
mig_internal novalue _Xvm_allocate_with_pager
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_t sizeType;
		vm_size_t size;
		msg_type_t anywhereType;
		boolean_t anywhere;
		msg_type_t memory_objectType;
		memory_object_t memory_object;
		msg_type_t offsetType;
		vm_offset_t offset;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t addressType;
		vm_address_t address;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_allocate_with_pager
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t *address, vm_size_t size, boolean_t anywhere, memory_object_t memory_object, vm_offset_t offset);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
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
	static const msg_type_t anywhereCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t memory_objectCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t offsetCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t addressType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 64) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((In0P->sizeType.msg_type_inline != TRUE) ||
	    (In0P->sizeType.msg_type_longform != FALSE) ||
	    (In0P->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->sizeType.msg_type_number != 1) ||
	    (In0P->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->anywhereType != * (int *) &anywhereCheck)
#else	UseStaticMsgType
	if ((In0P->anywhereType.msg_type_inline != TRUE) ||
	    (In0P->anywhereType.msg_type_longform != FALSE) ||
	    (In0P->anywhereType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->anywhereType.msg_type_number != 1) ||
	    (In0P->anywhereType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->memory_objectType != * (int *) &memory_objectCheck)
#else	UseStaticMsgType
	if ((In0P->memory_objectType.msg_type_inline != TRUE) ||
	    (In0P->memory_objectType.msg_type_longform != FALSE) ||
	    (In0P->memory_objectType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->memory_objectType.msg_type_number != 1) ||
	    (In0P->memory_objectType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->offsetType != * (int *) &offsetCheck)
#else	UseStaticMsgType
	if ((In0P->offsetType.msg_type_inline != TRUE) ||
	    (In0P->offsetType.msg_type_longform != FALSE) ||
	    (In0P->offsetType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->offsetType.msg_type_number != 1) ||
	    (In0P->offsetType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_allocate_with_pager(target_task, &In0P->address, In0P->size, In0P->anywhere, In0P->memory_object, In0P->offset);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->addressType = addressType;
#else	UseStaticMsgType
	OutP->addressType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->addressType.msg_type_size = 32;
	OutP->addressType.msg_type_number = 1;
	OutP->addressType.msg_type_inline = TRUE;
	OutP->addressType.msg_type_longform = FALSE;
	OutP->addressType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	/* 0 */ OutP->address = In0P->address;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine vm_deallocate */
mig_internal novalue _Xvm_deallocate
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_t sizeType;
		vm_size_t size;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_deallocate
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t address, vm_size_t size);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
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

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((In0P->sizeType.msg_type_inline != TRUE) ||
	    (In0P->sizeType.msg_type_longform != FALSE) ||
	    (In0P->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->sizeType.msg_type_number != 1) ||
	    (In0P->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_deallocate(target_task, In0P->address, In0P->size);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
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

/* Routine vm_protect */
mig_internal novalue _Xvm_protect
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_t sizeType;
		vm_size_t size;
		msg_type_t set_maximumType;
		boolean_t set_maximum;
		msg_type_t new_protectionType;
		vm_prot_t new_protection;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_protect
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t address, vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
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
	static const msg_type_t set_maximumCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t new_protectionCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 56) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((In0P->sizeType.msg_type_inline != TRUE) ||
	    (In0P->sizeType.msg_type_longform != FALSE) ||
	    (In0P->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->sizeType.msg_type_number != 1) ||
	    (In0P->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->set_maximumType != * (int *) &set_maximumCheck)
#else	UseStaticMsgType
	if ((In0P->set_maximumType.msg_type_inline != TRUE) ||
	    (In0P->set_maximumType.msg_type_longform != FALSE) ||
	    (In0P->set_maximumType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->set_maximumType.msg_type_number != 1) ||
	    (In0P->set_maximumType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->new_protectionType != * (int *) &new_protectionCheck)
#else	UseStaticMsgType
	if ((In0P->new_protectionType.msg_type_inline != TRUE) ||
	    (In0P->new_protectionType.msg_type_longform != FALSE) ||
	    (In0P->new_protectionType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->new_protectionType.msg_type_number != 1) ||
	    (In0P->new_protectionType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_protect(target_task, In0P->address, In0P->size, In0P->set_maximum, In0P->new_protection);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
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

/* Routine vm_inherit */
mig_internal novalue _Xvm_inherit
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_t sizeType;
		vm_size_t size;
		msg_type_t new_inheritanceType;
		vm_inherit_t new_inheritance;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_inherit
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t address, vm_size_t size, vm_inherit_t new_inheritance);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
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
	static const msg_type_t new_inheritanceCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 48) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((In0P->sizeType.msg_type_inline != TRUE) ||
	    (In0P->sizeType.msg_type_longform != FALSE) ||
	    (In0P->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->sizeType.msg_type_number != 1) ||
	    (In0P->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->new_inheritanceType != * (int *) &new_inheritanceCheck)
#else	UseStaticMsgType
	if ((In0P->new_inheritanceType.msg_type_inline != TRUE) ||
	    (In0P->new_inheritanceType.msg_type_longform != FALSE) ||
	    (In0P->new_inheritanceType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->new_inheritanceType.msg_type_number != 1) ||
	    (In0P->new_inheritanceType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_inherit(target_task, In0P->address, In0P->size, In0P->new_inheritance);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
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

/* Routine vm_read */
mig_internal novalue _Xvm_read
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_t sizeType;
		vm_size_t size;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_long_t dataType;
		pointer_t data;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_read
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t address, vm_size_t size, pointer_t *data, unsigned int *dataCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
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
	static const msg_type_long_t dataType = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
		/* msg_type_long_name = */	MSG_TYPE_BYTE,
		/* msg_type_long_size = */	8,
		/* msg_type_long_number = */	0,
	};
#endif	UseStaticMsgType

	vm_map_t target_task;
	unsigned int dataCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((In0P->sizeType.msg_type_inline != TRUE) ||
	    (In0P->sizeType.msg_type_longform != FALSE) ||
	    (In0P->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->sizeType.msg_type_number != 1) ||
	    (In0P->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_read(target_task, In0P->address, In0P->size, &OutP->data, &dataCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 48;

#if	UseStaticMsgType
	OutP->dataType = dataType;
#else	UseStaticMsgType
	OutP->dataType.msg_type_long_name = MSG_TYPE_BYTE;
	OutP->dataType.msg_type_long_size = 8;
	OutP->dataType.msg_type_header.msg_type_inline = FALSE;
	OutP->dataType.msg_type_header.msg_type_longform = TRUE;
	OutP->dataType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->dataType.msg_type_long_number /* dataCnt */ = /* dataType.msg_type_long_number */ dataCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine vm_write */
mig_internal novalue _Xvm_write
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_long_t dataType;
		pointer_t data;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_write
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t address, pointer_t data, unsigned int dataCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 48) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
	if ((In0P->dataType.msg_type_header.msg_type_inline != FALSE) ||
	    (In0P->dataType.msg_type_header.msg_type_longform != TRUE) ||
	    (In0P->dataType.msg_type_long_name != MSG_TYPE_BYTE) ||
	    (In0P->dataType.msg_type_long_size != 8))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_write(target_task, In0P->address, In0P->data, In0P->dataType.msg_type_long_number);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
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

/* Routine vm_copy */
mig_internal novalue _Xvm_copy
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t source_addressType;
		vm_address_t source_address;
		msg_type_t sizeType;
		vm_size_t size;
		msg_type_t dest_addressType;
		vm_address_t dest_address;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_copy
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t source_address, vm_size_t size, vm_address_t dest_address);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t source_addressCheck = {
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
	static const msg_type_t dest_addressCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 48) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->source_addressType != * (int *) &source_addressCheck)
#else	UseStaticMsgType
	if ((In0P->source_addressType.msg_type_inline != TRUE) ||
	    (In0P->source_addressType.msg_type_longform != FALSE) ||
	    (In0P->source_addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->source_addressType.msg_type_number != 1) ||
	    (In0P->source_addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((In0P->sizeType.msg_type_inline != TRUE) ||
	    (In0P->sizeType.msg_type_longform != FALSE) ||
	    (In0P->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->sizeType.msg_type_number != 1) ||
	    (In0P->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->dest_addressType != * (int *) &dest_addressCheck)
#else	UseStaticMsgType
	if ((In0P->dest_addressType.msg_type_inline != TRUE) ||
	    (In0P->dest_addressType.msg_type_longform != FALSE) ||
	    (In0P->dest_addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->dest_addressType.msg_type_number != 1) ||
	    (In0P->dest_addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_copy(target_task, In0P->source_address, In0P->size, In0P->dest_address);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
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

/* Routine vm_region */
mig_internal novalue _Xvm_region
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_t sizeType;
		vm_size_t size;
		msg_type_t protectionType;
		vm_prot_t protection;
		msg_type_t max_protectionType;
		vm_prot_t max_protection;
		msg_type_t inheritanceType;
		vm_inherit_t inheritance;
		msg_type_t is_sharedType;
		boolean_t is_shared;
		msg_type_t object_nameType;
		memory_object_name_t object_name;
		msg_type_t offsetType;
		vm_offset_t offset;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_region
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t *address, vm_size_t *size, vm_prot_t *protection, vm_prot_t *max_protection, vm_inherit_t *inheritance, boolean_t *is_shared, memory_object_name_t *object_name, vm_offset_t *offset);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t addressType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t sizeType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t protectionType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t max_protectionType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t inheritanceType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t is_sharedType = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t object_nameType = {
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

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_region(target_task, &In0P->address, &OutP->size, &OutP->protection, &OutP->max_protection, &OutP->inheritance, &OutP->is_shared, &OutP->object_name, &OutP->offset);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 96;

#if	UseStaticMsgType
	OutP->addressType = addressType;
#else	UseStaticMsgType
	OutP->addressType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->addressType.msg_type_size = 32;
	OutP->addressType.msg_type_number = 1;
	OutP->addressType.msg_type_inline = TRUE;
	OutP->addressType.msg_type_longform = FALSE;
	OutP->addressType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	/* 0 */ OutP->address = In0P->address;

#if	UseStaticMsgType
	OutP->sizeType = sizeType;
#else	UseStaticMsgType
	OutP->sizeType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->sizeType.msg_type_size = 32;
	OutP->sizeType.msg_type_number = 1;
	OutP->sizeType.msg_type_inline = TRUE;
	OutP->sizeType.msg_type_longform = FALSE;
	OutP->sizeType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->protectionType = protectionType;
#else	UseStaticMsgType
	OutP->protectionType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->protectionType.msg_type_size = 32;
	OutP->protectionType.msg_type_number = 1;
	OutP->protectionType.msg_type_inline = TRUE;
	OutP->protectionType.msg_type_longform = FALSE;
	OutP->protectionType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->max_protectionType = max_protectionType;
#else	UseStaticMsgType
	OutP->max_protectionType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->max_protectionType.msg_type_size = 32;
	OutP->max_protectionType.msg_type_number = 1;
	OutP->max_protectionType.msg_type_inline = TRUE;
	OutP->max_protectionType.msg_type_longform = FALSE;
	OutP->max_protectionType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->inheritanceType = inheritanceType;
#else	UseStaticMsgType
	OutP->inheritanceType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->inheritanceType.msg_type_size = 32;
	OutP->inheritanceType.msg_type_number = 1;
	OutP->inheritanceType.msg_type_inline = TRUE;
	OutP->inheritanceType.msg_type_longform = FALSE;
	OutP->inheritanceType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->is_sharedType = is_sharedType;
#else	UseStaticMsgType
	OutP->is_sharedType.msg_type_name = MSG_TYPE_BOOLEAN;
	OutP->is_sharedType.msg_type_size = 32;
	OutP->is_sharedType.msg_type_number = 1;
	OutP->is_sharedType.msg_type_inline = TRUE;
	OutP->is_sharedType.msg_type_longform = FALSE;
	OutP->is_sharedType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->object_nameType = object_nameType;
#else	UseStaticMsgType
	OutP->object_nameType.msg_type_name = MSG_TYPE_PORT;
	OutP->object_nameType.msg_type_size = 32;
	OutP->object_nameType.msg_type_number = 1;
	OutP->object_nameType.msg_type_inline = TRUE;
	OutP->object_nameType.msg_type_longform = FALSE;
	OutP->object_nameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->offsetType = offsetType;
#else	UseStaticMsgType
	OutP->offsetType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->offsetType.msg_type_size = 32;
	OutP->offsetType.msg_type_number = 1;
	OutP->offsetType.msg_type_inline = TRUE;
	OutP->offsetType.msg_type_longform = FALSE;
	OutP->offsetType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine vm_statistics */
mig_internal novalue _Xvm_statistics
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
		msg_type_t vm_statsType;
		vm_statistics_data_t vm_stats;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_statistics
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_statistics_data_t *vm_stats);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t vm_statsType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		13,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

	OutP->RetCode = vm_statistics(target_task, &OutP->vm_stats);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 88;

#if	UseStaticMsgType
	OutP->vm_statsType = vm_statsType;
#else	UseStaticMsgType
	OutP->vm_statsType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->vm_statsType.msg_type_size = 32;
	OutP->vm_statsType.msg_type_number = 13;
	OutP->vm_statsType.msg_type_inline = TRUE;
	OutP->vm_statsType.msg_type_longform = FALSE;
	OutP->vm_statsType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine task_by_unix_pid */
mig_internal novalue _Xtask_by_unix_pid
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t process_idType;
		int process_id;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t result_taskType;
		port_t result_task;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_by_unix_pid
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, int process_id, task_t *result_task);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t process_idCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t result_taskType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t target_task;
	task_t result_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->process_idType != * (int *) &process_idCheck)
#else	UseStaticMsgType
	if ((In0P->process_idType.msg_type_inline != TRUE) ||
	    (In0P->process_idType.msg_type_longform != FALSE) ||
	    (In0P->process_idType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->process_idType.msg_type_number != 1) ||
	    (In0P->process_idType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = task_by_unix_pid(target_task, In0P->process_id, &result_task);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->result_taskType = result_taskType;
#else	UseStaticMsgType
	OutP->result_taskType.msg_type_name = MSG_TYPE_PORT;
	OutP->result_taskType.msg_type_size = 32;
	OutP->result_taskType.msg_type_number = 1;
	OutP->result_taskType.msg_type_inline = TRUE;
	OutP->result_taskType.msg_type_longform = FALSE;
	OutP->result_taskType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->result_task /* convert_task_to_port result_task */ = /* result_task */ convert_task_to_port(result_task);

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine mach_ports_register */
mig_internal novalue _Xmach_ports_register
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_long_t init_port_setType;
		port_array_t init_port_set;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t mach_ports_register
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, port_array_t init_port_set, unsigned int init_port_setCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	task_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
	if ((In0P->init_port_setType.msg_type_header.msg_type_inline != FALSE) ||
	    (In0P->init_port_setType.msg_type_header.msg_type_longform != TRUE) ||
	    (In0P->init_port_setType.msg_type_long_name != MSG_TYPE_PORT) ||
	    (In0P->init_port_setType.msg_type_long_size != 32))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = mach_ports_register(target_task, In0P->init_port_set, In0P->init_port_setType.msg_type_long_number);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
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

/* Routine mach_ports_lookup */
mig_internal novalue _Xmach_ports_lookup
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
		msg_type_long_t init_port_setType;
		port_array_t init_port_set;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t mach_ports_lookup
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, port_array_t *init_port_set, unsigned int *init_port_setCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_long_t init_port_setType = {
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

	task_t target_task;
	unsigned int init_port_setCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = mach_ports_lookup(target_task, &OutP->init_port_set, &init_port_setCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 48;

#if	UseStaticMsgType
	OutP->init_port_setType = init_port_setType;
#else	UseStaticMsgType
	OutP->init_port_setType.msg_type_long_name = MSG_TYPE_PORT;
	OutP->init_port_setType.msg_type_long_size = 32;
	OutP->init_port_setType.msg_type_header.msg_type_inline = FALSE;
	OutP->init_port_setType.msg_type_header.msg_type_longform = TRUE;
	OutP->init_port_setType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->init_port_setType.msg_type_long_number /* init_port_setCnt */ = /* init_port_setType.msg_type_long_number */ init_port_setCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine unix_pid */
mig_internal novalue _Xunix_pid
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
		msg_type_t process_idType;
		int process_id;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t unix_pid
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, int *process_id);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t process_idType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = unix_pid(target_task, &OutP->process_id);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->process_idType = process_idType;
#else	UseStaticMsgType
	OutP->process_idType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->process_idType.msg_type_size = 32;
	OutP->process_idType.msg_type_number = 1;
	OutP->process_idType.msg_type_inline = TRUE;
	OutP->process_idType.msg_type_longform = FALSE;
	OutP->process_idType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine netipc_listen */
mig_internal novalue _Xnetipc_listen
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t src_addrType;
		int src_addr;
		msg_type_t dst_addrType;
		int dst_addr;
		msg_type_t src_portType;
		int src_port;
		msg_type_t dst_portType;
		int dst_port;
		msg_type_t protocolType;
		int protocol;
		msg_type_t ipc_portType;
		port_t ipc_port;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t netipc_listen
#if	(defined(__STDC__) || defined(c_plusplus))
		(port_t request_port, int src_addr, int dst_addr, int src_port, int dst_port, int protocol, port_t ipc_port);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t src_addrCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t dst_addrCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t src_portCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t dst_portCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t protocolCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t ipc_portCheck = {
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
	if ((msg_size != 72) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->src_addrType != * (int *) &src_addrCheck)
#else	UseStaticMsgType
	if ((In0P->src_addrType.msg_type_inline != TRUE) ||
	    (In0P->src_addrType.msg_type_longform != FALSE) ||
	    (In0P->src_addrType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->src_addrType.msg_type_number != 1) ||
	    (In0P->src_addrType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->dst_addrType != * (int *) &dst_addrCheck)
#else	UseStaticMsgType
	if ((In0P->dst_addrType.msg_type_inline != TRUE) ||
	    (In0P->dst_addrType.msg_type_longform != FALSE) ||
	    (In0P->dst_addrType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->dst_addrType.msg_type_number != 1) ||
	    (In0P->dst_addrType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->src_portType != * (int *) &src_portCheck)
#else	UseStaticMsgType
	if ((In0P->src_portType.msg_type_inline != TRUE) ||
	    (In0P->src_portType.msg_type_longform != FALSE) ||
	    (In0P->src_portType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->src_portType.msg_type_number != 1) ||
	    (In0P->src_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->dst_portType != * (int *) &dst_portCheck)
#else	UseStaticMsgType
	if ((In0P->dst_portType.msg_type_inline != TRUE) ||
	    (In0P->dst_portType.msg_type_longform != FALSE) ||
	    (In0P->dst_portType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->dst_portType.msg_type_number != 1) ||
	    (In0P->dst_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->protocolType != * (int *) &protocolCheck)
#else	UseStaticMsgType
	if ((In0P->protocolType.msg_type_inline != TRUE) ||
	    (In0P->protocolType.msg_type_longform != FALSE) ||
	    (In0P->protocolType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->protocolType.msg_type_number != 1) ||
	    (In0P->protocolType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->ipc_portType != * (int *) &ipc_portCheck)
#else	UseStaticMsgType
	if ((In0P->ipc_portType.msg_type_inline != TRUE) ||
	    (In0P->ipc_portType.msg_type_longform != FALSE) ||
	    (In0P->ipc_portType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->ipc_portType.msg_type_number != 1) ||
	    (In0P->ipc_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

	OutP->RetCode = netipc_listen(In0P->Head.msg_request_port, In0P->src_addr, In0P->dst_addr, In0P->src_port, In0P->dst_port, In0P->protocol, In0P->ipc_port);
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

/* Routine netipc_ignore */
mig_internal novalue _Xnetipc_ignore
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t ipc_portType;
		port_t ipc_port;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t netipc_ignore
#if	(defined(__STDC__) || defined(c_plusplus))
		(port_t request_port, port_t ipc_port);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t ipc_portCheck = {
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
	if ((msg_size != 32) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->ipc_portType != * (int *) &ipc_portCheck)
#else	UseStaticMsgType
	if ((In0P->ipc_portType.msg_type_inline != TRUE) ||
	    (In0P->ipc_portType.msg_type_longform != FALSE) ||
	    (In0P->ipc_portType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->ipc_portType.msg_type_number != 1) ||
	    (In0P->ipc_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt0; }
#define	label_punt0
#endif	TypeCheck

	OutP->RetCode = netipc_ignore(In0P->Head.msg_request_port, In0P->ipc_port);
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

/* Routine xxx_host_info */
mig_internal novalue _Xxxx_host_info
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
		msg_type_t infoType;
		machine_info_data_t info;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t xxx_host_info
#if	(defined(__STDC__) || defined(c_plusplus))
		(port_t target_task, machine_info_data_t *info);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t infoType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		5,
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

	OutP->RetCode = xxx_host_info(In0P->Head.msg_request_port, &OutP->info);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 56;

#if	UseStaticMsgType
	OutP->infoType = infoType;
#else	UseStaticMsgType
	OutP->infoType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->infoType.msg_type_size = 32;
	OutP->infoType.msg_type_number = 5;
	OutP->infoType.msg_type_inline = TRUE;
	OutP->infoType.msg_type_longform = FALSE;
	OutP->infoType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine xxx_slot_info */
mig_internal novalue _Xxxx_slot_info
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t slotType;
		int slot;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t infoType;
		machine_slot_data_t info;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t xxx_slot_info
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, int slot, machine_slot_data_t *info);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t slotCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t infoType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		8,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->slotType != * (int *) &slotCheck)
#else	UseStaticMsgType
	if ((In0P->slotType.msg_type_inline != TRUE) ||
	    (In0P->slotType.msg_type_longform != FALSE) ||
	    (In0P->slotType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->slotType.msg_type_number != 1) ||
	    (In0P->slotType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = xxx_slot_info(target_task, In0P->slot, &OutP->info);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 68;

#if	UseStaticMsgType
	OutP->infoType = infoType;
#else	UseStaticMsgType
	OutP->infoType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->infoType.msg_type_size = 32;
	OutP->infoType.msg_type_number = 8;
	OutP->infoType.msg_type_inline = TRUE;
	OutP->infoType.msg_type_longform = FALSE;
	OutP->infoType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine xxx_cpu_control */
mig_internal novalue _Xxxx_cpu_control
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t cpuType;
		int cpu;
		msg_type_t runningType;
		boolean_t running;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t xxx_cpu_control
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, int cpu, boolean_t running);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t cpuCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t runningCheck = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->cpuType != * (int *) &cpuCheck)
#else	UseStaticMsgType
	if ((In0P->cpuType.msg_type_inline != TRUE) ||
	    (In0P->cpuType.msg_type_longform != FALSE) ||
	    (In0P->cpuType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->cpuType.msg_type_number != 1) ||
	    (In0P->cpuType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->runningType != * (int *) &runningCheck)
#else	UseStaticMsgType
	if ((In0P->runningType.msg_type_inline != TRUE) ||
	    (In0P->runningType.msg_type_longform != FALSE) ||
	    (In0P->runningType.msg_type_name != MSG_TYPE_BOOLEAN) ||
	    (In0P->runningType.msg_type_number != 1) ||
	    (In0P->runningType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = xxx_cpu_control(target_task, In0P->cpu, In0P->running);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
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

/* Routine task_suspend */
mig_internal novalue _Xtask_suspend
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
	extern kern_return_t task_suspend
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	task_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = task_suspend(target_task);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
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

/* Routine task_resume */
mig_internal novalue _Xtask_resume
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
	extern kern_return_t task_resume
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	task_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = task_resume(target_task);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
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

/* Routine task_get_special_port */
mig_internal novalue _Xtask_get_special_port
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t which_portType;
		int which_port;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t special_portType;
		port_t special_port;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_get_special_port
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, int which_port, port_t *special_port);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t which_portCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t special_portType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
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
	if (* (int *) &In0P->which_portType != * (int *) &which_portCheck)
#else	UseStaticMsgType
	if ((In0P->which_portType.msg_type_inline != TRUE) ||
	    (In0P->which_portType.msg_type_longform != FALSE) ||
	    (In0P->which_portType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->which_portType.msg_type_number != 1) ||
	    (In0P->which_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = task_get_special_port(task, In0P->which_port, &OutP->special_port);
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
	OutP->special_portType = special_portType;
#else	UseStaticMsgType
	OutP->special_portType.msg_type_name = MSG_TYPE_PORT;
	OutP->special_portType.msg_type_size = 32;
	OutP->special_portType.msg_type_number = 1;
	OutP->special_portType.msg_type_inline = TRUE;
	OutP->special_portType.msg_type_longform = FALSE;
	OutP->special_portType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine task_set_special_port */
mig_internal novalue _Xtask_set_special_port
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t which_portType;
		int which_port;
		msg_type_t special_portType;
		port_t special_port;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_set_special_port
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, int which_port, port_t special_port);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t which_portCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t special_portCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
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
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->which_portType != * (int *) &which_portCheck)
#else	UseStaticMsgType
	if ((In0P->which_portType.msg_type_inline != TRUE) ||
	    (In0P->which_portType.msg_type_longform != FALSE) ||
	    (In0P->which_portType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->which_portType.msg_type_number != 1) ||
	    (In0P->which_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->special_portType != * (int *) &special_portCheck)
#else	UseStaticMsgType
	if ((In0P->special_portType.msg_type_inline != TRUE) ||
	    (In0P->special_portType.msg_type_longform != FALSE) ||
	    (In0P->special_portType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->special_portType.msg_type_number != 1) ||
	    (In0P->special_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = task_set_special_port(task, In0P->which_port, In0P->special_port);
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

/* Routine task_info */
mig_internal novalue _Xtask_info
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
		msg_type_long_t task_info_outType;
		int task_info_out[1024];
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t task_info
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t target_task, int flavor, task_info_t task_info_out, unsigned int *task_info_outCnt);
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
	static const msg_type_long_t task_info_outType = {
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

	task_t target_task;
	unsigned int task_info_outCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_task 0 Head.msg_request_port */ = /* target_task */ convert_port_to_task(In0P->Head.msg_request_port);

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

	task_info_outCnt = 1024;

	OutP->RetCode = task_info(target_task, In0P->flavor, OutP->task_info_out, &task_info_outCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(target_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 44;

#if	UseStaticMsgType
	OutP->task_info_outType = task_info_outType;
#else	UseStaticMsgType
	OutP->task_info_outType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->task_info_outType.msg_type_long_size = 32;
	OutP->task_info_outType.msg_type_header.msg_type_inline = TRUE;
	OutP->task_info_outType.msg_type_header.msg_type_longform = TRUE;
	OutP->task_info_outType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->task_info_outType.msg_type_long_number /* task_info_outCnt */ = /* task_info_outType.msg_type_long_number */ task_info_outCnt;

	msg_size_delta = (4 * task_info_outCnt);
	msg_size += msg_size_delta;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine thread_create */
mig_internal novalue _Xthread_create
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
		msg_type_t child_threadType;
		port_t child_thread;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_create
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t parent_task, thread_t *child_thread);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t child_threadType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

	task_t parent_task;
	thread_t child_thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	parent_task /* convert_port_to_task 0 Head.msg_request_port */ = /* parent_task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = thread_create(parent_task, &child_thread);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	task_deallocate(parent_task);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 40;

#if	UseStaticMsgType
	OutP->child_threadType = child_threadType;
#else	UseStaticMsgType
	OutP->child_threadType.msg_type_name = MSG_TYPE_PORT;
	OutP->child_threadType.msg_type_size = 32;
	OutP->child_threadType.msg_type_number = 1;
	OutP->child_threadType.msg_type_inline = TRUE;
	OutP->child_threadType.msg_type_longform = FALSE;
	OutP->child_threadType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->child_thread /* convert_thread_to_port child_thread */ = /* child_thread */ convert_thread_to_port(child_thread);

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine thread_suspend */
mig_internal novalue _Xthread_suspend
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
	extern kern_return_t thread_suspend
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t target_thread);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	thread_t target_thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* target_thread */ convert_port_to_thread(In0P->Head.msg_request_port);

	OutP->RetCode = thread_suspend(target_thread);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(target_thread);
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

/* Routine thread_resume */
mig_internal novalue _Xthread_resume
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
	extern kern_return_t thread_resume
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t target_thread);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	thread_t target_thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* target_thread */ convert_port_to_thread(In0P->Head.msg_request_port);

	OutP->RetCode = thread_resume(target_thread);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(target_thread);
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

/* Routine thread_abort */
mig_internal novalue _Xthread_abort
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
	extern kern_return_t thread_abort
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t target_thread);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

	thread_t target_thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* target_thread */ convert_port_to_thread(In0P->Head.msg_request_port);

	OutP->RetCode = thread_abort(target_thread);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(target_thread);
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

/* Routine thread_get_state */
mig_internal novalue _Xthread_get_state
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
		msg_type_long_t old_stateType;
		int old_state[1024];
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_get_state
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t target_thread, int flavor, thread_state_t old_state, unsigned int *old_stateCnt);
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
	static const msg_type_long_t old_stateType = {
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

	thread_t target_thread;
	unsigned int old_stateCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* target_thread */ convert_port_to_thread(In0P->Head.msg_request_port);

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

	old_stateCnt = 1024;

	OutP->RetCode = thread_get_state(target_thread, In0P->flavor, OutP->old_state, &old_stateCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(target_thread);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 44;

#if	UseStaticMsgType
	OutP->old_stateType = old_stateType;
#else	UseStaticMsgType
	OutP->old_stateType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->old_stateType.msg_type_long_size = 32;
	OutP->old_stateType.msg_type_header.msg_type_inline = TRUE;
	OutP->old_stateType.msg_type_header.msg_type_longform = TRUE;
	OutP->old_stateType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->old_stateType.msg_type_long_number /* old_stateCnt */ = /* old_stateType.msg_type_long_number */ old_stateCnt;

	msg_size_delta = (4 * old_stateCnt);
	msg_size += msg_size_delta;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine thread_set_state */
mig_internal novalue _Xthread_set_state
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
		msg_type_long_t new_stateType;
		int new_state[1024];
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_set_state
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t target_thread, int flavor, thread_state_t new_state, unsigned int new_stateCnt);
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

	thread_t target_thread;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size < 44) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* target_thread */ convert_port_to_thread(In0P->Head.msg_request_port);

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

#if	TypeCheck
	if ((In0P->new_stateType.msg_type_header.msg_type_inline != TRUE) ||
	    (In0P->new_stateType.msg_type_header.msg_type_longform != TRUE) ||
	    (In0P->new_stateType.msg_type_long_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->new_stateType.msg_type_long_size != 32))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
	msg_size_delta = (4 * In0P->new_stateType.msg_type_long_number);
	if (msg_size != 44 + msg_size_delta)
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = thread_set_state(target_thread, In0P->flavor, In0P->new_state, In0P->new_stateType.msg_type_long_number);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(target_thread);
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

/* Routine thread_get_special_port */
mig_internal novalue _Xthread_get_special_port
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t which_portType;
		int which_port;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t special_portType;
		port_t special_port;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_get_special_port
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t thread, int which_port, port_t *special_port);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t which_portCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t special_portType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
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
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* thread */ convert_port_to_thread(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->which_portType != * (int *) &which_portCheck)
#else	UseStaticMsgType
	if ((In0P->which_portType.msg_type_inline != TRUE) ||
	    (In0P->which_portType.msg_type_longform != FALSE) ||
	    (In0P->which_portType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->which_portType.msg_type_number != 1) ||
	    (In0P->which_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = thread_get_special_port(thread, In0P->which_port, &OutP->special_port);
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
	OutP->special_portType = special_portType;
#else	UseStaticMsgType
	OutP->special_portType.msg_type_name = MSG_TYPE_PORT;
	OutP->special_portType.msg_type_size = 32;
	OutP->special_portType.msg_type_number = 1;
	OutP->special_portType.msg_type_inline = TRUE;
	OutP->special_portType.msg_type_longform = FALSE;
	OutP->special_portType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine thread_set_special_port */
mig_internal novalue _Xthread_set_special_port
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t which_portType;
		int which_port;
		msg_type_t special_portType;
		port_t special_port;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_set_special_port
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t thread, int which_port, port_t special_port);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t which_portCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t special_portCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
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
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* thread */ convert_port_to_thread(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->which_portType != * (int *) &which_portCheck)
#else	UseStaticMsgType
	if ((In0P->which_portType.msg_type_inline != TRUE) ||
	    (In0P->which_portType.msg_type_longform != FALSE) ||
	    (In0P->which_portType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->which_portType.msg_type_number != 1) ||
	    (In0P->which_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->special_portType != * (int *) &special_portCheck)
#else	UseStaticMsgType
	if ((In0P->special_portType.msg_type_inline != TRUE) ||
	    (In0P->special_portType.msg_type_longform != FALSE) ||
	    (In0P->special_portType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->special_portType.msg_type_number != 1) ||
	    (In0P->special_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = thread_set_special_port(thread, In0P->which_port, In0P->special_port);
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

/* Routine thread_info */
mig_internal novalue _Xthread_info
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
		msg_type_long_t thread_info_outType;
		int thread_info_out[1024];
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t thread_info
#if	(defined(__STDC__) || defined(c_plusplus))
		(thread_t target_thread, int flavor, thread_info_t thread_info_out, unsigned int *thread_info_outCnt);
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
	static const msg_type_long_t thread_info_outType = {
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

	thread_t target_thread;
	unsigned int thread_info_outCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_thread /* convert_port_to_thread 0 Head.msg_request_port */ = /* target_thread */ convert_port_to_thread(In0P->Head.msg_request_port);

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

	thread_info_outCnt = 1024;

	OutP->RetCode = thread_info(target_thread, In0P->flavor, OutP->thread_info_out, &thread_info_outCnt);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	thread_deallocate(target_thread);
#ifdef	label_punt0
#undef	label_punt0
punt0:
#endif	label_punt0
	if (OutP->RetCode != KERN_SUCCESS)
		return;

	msg_size = 44;

#if	UseStaticMsgType
	OutP->thread_info_outType = thread_info_outType;
#else	UseStaticMsgType
	OutP->thread_info_outType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->thread_info_outType.msg_type_long_size = 32;
	OutP->thread_info_outType.msg_type_header.msg_type_inline = TRUE;
	OutP->thread_info_outType.msg_type_header.msg_type_longform = TRUE;
	OutP->thread_info_outType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->thread_info_outType.msg_type_long_number /* thread_info_outCnt */ = /* thread_info_outType.msg_type_long_number */ thread_info_outCnt;

	msg_size_delta = (4 * thread_info_outCnt);
	msg_size += msg_size_delta;

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_names */
mig_internal novalue _Xport_names
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
		msg_type_long_t port_names_pType;
		port_name_array_t port_names_p;
		msg_type_long_t port_typesType;
		port_type_array_t port_types;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_names
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_array_t *port_names_p, unsigned int *port_names_pCnt, port_type_array_t *port_types, unsigned int *port_typesCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_long_t port_names_pType = {
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

#if	UseStaticMsgType
	static const msg_type_long_t port_typesType = {
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
	unsigned int port_names_pCnt;
	unsigned int port_typesCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = port_names(task, &OutP->port_names_p, &port_names_pCnt, &OutP->port_types, &port_typesCnt);
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
	OutP->port_names_pType = port_names_pType;
#else	UseStaticMsgType
	OutP->port_names_pType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->port_names_pType.msg_type_long_size = 32;
	OutP->port_names_pType.msg_type_header.msg_type_inline = FALSE;
	OutP->port_names_pType.msg_type_header.msg_type_longform = TRUE;
	OutP->port_names_pType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->port_names_pType.msg_type_long_number /* port_names_pCnt */ = /* port_names_pType.msg_type_long_number */ port_names_pCnt;

#if	UseStaticMsgType
	OutP->port_typesType = port_typesType;
#else	UseStaticMsgType
	OutP->port_typesType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->port_typesType.msg_type_long_size = 32;
	OutP->port_typesType.msg_type_header.msg_type_inline = FALSE;
	OutP->port_typesType.msg_type_header.msg_type_longform = TRUE;
	OutP->port_typesType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->port_typesType.msg_type_long_number /* port_typesCnt */ = /* port_typesType.msg_type_long_number */ port_typesCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_type */
mig_internal novalue _Xport_type
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t port_nameType;
		port_name_t port_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t port_type_pType;
		port_type_t port_type_p;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_type
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t port_name, port_type_t *port_type_p);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t port_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t port_type_pType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if (* (int *) &In0P->port_nameType != * (int *) &port_nameCheck)
#else	UseStaticMsgType
	if ((In0P->port_nameType.msg_type_inline != TRUE) ||
	    (In0P->port_nameType.msg_type_longform != FALSE) ||
	    (In0P->port_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->port_nameType.msg_type_number != 1) ||
	    (In0P->port_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_type(task, In0P->port_name, &OutP->port_type_p);
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
	OutP->port_type_pType = port_type_pType;
#else	UseStaticMsgType
	OutP->port_type_pType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->port_type_pType.msg_type_size = 32;
	OutP->port_type_pType.msg_type_number = 1;
	OutP->port_type_pType.msg_type_inline = TRUE;
	OutP->port_type_pType.msg_type_longform = FALSE;
	OutP->port_type_pType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_rename */
mig_internal novalue _Xport_rename
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t old_nameType;
		port_name_t old_name;
		msg_type_t new_nameType;
		port_name_t new_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_rename
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t old_name, port_name_t new_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t old_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t new_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if (* (int *) &In0P->old_nameType != * (int *) &old_nameCheck)
#else	UseStaticMsgType
	if ((In0P->old_nameType.msg_type_inline != TRUE) ||
	    (In0P->old_nameType.msg_type_longform != FALSE) ||
	    (In0P->old_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->old_nameType.msg_type_number != 1) ||
	    (In0P->old_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->new_nameType != * (int *) &new_nameCheck)
#else	UseStaticMsgType
	if ((In0P->new_nameType.msg_type_inline != TRUE) ||
	    (In0P->new_nameType.msg_type_longform != FALSE) ||
	    (In0P->new_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->new_nameType.msg_type_number != 1) ||
	    (In0P->new_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_rename(task, In0P->old_name, In0P->new_name);
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

/* Routine port_allocate */
mig_internal novalue _Xport_allocate
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
		msg_type_t port_nameType;
		port_name_t port_name;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_allocate
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t *port_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t port_nameType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = port_allocate(task, &OutP->port_name);
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
	OutP->port_nameType = port_nameType;
#else	UseStaticMsgType
	OutP->port_nameType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->port_nameType.msg_type_size = 32;
	OutP->port_nameType.msg_type_number = 1;
	OutP->port_nameType.msg_type_inline = TRUE;
	OutP->port_nameType.msg_type_longform = FALSE;
	OutP->port_nameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_deallocate */
mig_internal novalue _Xport_deallocate
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t port_nameType;
		port_name_t port_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_deallocate
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t port_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t port_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if (* (int *) &In0P->port_nameType != * (int *) &port_nameCheck)
#else	UseStaticMsgType
	if ((In0P->port_nameType.msg_type_inline != TRUE) ||
	    (In0P->port_nameType.msg_type_longform != FALSE) ||
	    (In0P->port_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->port_nameType.msg_type_number != 1) ||
	    (In0P->port_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_deallocate(task, In0P->port_name);
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

/* Routine port_set_backlog */
mig_internal novalue _Xport_set_backlog
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t port_nameType;
		port_name_t port_name;
		msg_type_t backlogType;
		int backlog;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_set_backlog
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t port_name, int backlog);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t port_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t backlogCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if (* (int *) &In0P->port_nameType != * (int *) &port_nameCheck)
#else	UseStaticMsgType
	if ((In0P->port_nameType.msg_type_inline != TRUE) ||
	    (In0P->port_nameType.msg_type_longform != FALSE) ||
	    (In0P->port_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->port_nameType.msg_type_number != 1) ||
	    (In0P->port_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->backlogType != * (int *) &backlogCheck)
#else	UseStaticMsgType
	if ((In0P->backlogType.msg_type_inline != TRUE) ||
	    (In0P->backlogType.msg_type_longform != FALSE) ||
	    (In0P->backlogType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->backlogType.msg_type_number != 1) ||
	    (In0P->backlogType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_set_backlog(task, In0P->port_name, In0P->backlog);
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

/* Routine port_status */
mig_internal novalue _Xport_status
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t port_nameType;
		port_name_t port_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t enabledType;
		port_set_name_t enabled;
		msg_type_t num_msgsType;
		int num_msgs;
		msg_type_t backlogType;
		int backlog;
		msg_type_t ownershipType;
		boolean_t ownership;
		msg_type_t receive_rightsType;
		boolean_t receive_rights;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_status
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t port_name, port_set_name_t *enabled, int *num_msgs, int *backlog, boolean_t *ownership, boolean_t *receive_rights);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t port_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t enabledType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t num_msgsType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t backlogType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t ownershipType = {
		/* msg_type_name = */		MSG_TYPE_BOOLEAN,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t receive_rightsType = {
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
	if (* (int *) &In0P->port_nameType != * (int *) &port_nameCheck)
#else	UseStaticMsgType
	if ((In0P->port_nameType.msg_type_inline != TRUE) ||
	    (In0P->port_nameType.msg_type_longform != FALSE) ||
	    (In0P->port_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->port_nameType.msg_type_number != 1) ||
	    (In0P->port_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_status(task, In0P->port_name, &OutP->enabled, &OutP->num_msgs, &OutP->backlog, &OutP->ownership, &OutP->receive_rights);
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

	msg_size = 72;

#if	UseStaticMsgType
	OutP->enabledType = enabledType;
#else	UseStaticMsgType
	OutP->enabledType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->enabledType.msg_type_size = 32;
	OutP->enabledType.msg_type_number = 1;
	OutP->enabledType.msg_type_inline = TRUE;
	OutP->enabledType.msg_type_longform = FALSE;
	OutP->enabledType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->num_msgsType = num_msgsType;
#else	UseStaticMsgType
	OutP->num_msgsType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->num_msgsType.msg_type_size = 32;
	OutP->num_msgsType.msg_type_number = 1;
	OutP->num_msgsType.msg_type_inline = TRUE;
	OutP->num_msgsType.msg_type_longform = FALSE;
	OutP->num_msgsType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->backlogType = backlogType;
#else	UseStaticMsgType
	OutP->backlogType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->backlogType.msg_type_size = 32;
	OutP->backlogType.msg_type_number = 1;
	OutP->backlogType.msg_type_inline = TRUE;
	OutP->backlogType.msg_type_longform = FALSE;
	OutP->backlogType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->ownershipType = ownershipType;
#else	UseStaticMsgType
	OutP->ownershipType.msg_type_name = MSG_TYPE_BOOLEAN;
	OutP->ownershipType.msg_type_size = 32;
	OutP->ownershipType.msg_type_number = 1;
	OutP->ownershipType.msg_type_inline = TRUE;
	OutP->ownershipType.msg_type_longform = FALSE;
	OutP->ownershipType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

#if	UseStaticMsgType
	OutP->receive_rightsType = receive_rightsType;
#else	UseStaticMsgType
	OutP->receive_rightsType.msg_type_name = MSG_TYPE_BOOLEAN;
	OutP->receive_rightsType.msg_type_size = 32;
	OutP->receive_rightsType.msg_type_number = 1;
	OutP->receive_rightsType.msg_type_inline = TRUE;
	OutP->receive_rightsType.msg_type_longform = FALSE;
	OutP->receive_rightsType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_set_allocate */
mig_internal novalue _Xport_set_allocate
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
		msg_type_t set_nameType;
		port_set_name_t set_name;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_set_allocate
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_set_name_t *set_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t set_nameType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if ((msg_size != 24) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

	OutP->RetCode = port_set_allocate(task, &OutP->set_name);
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
	OutP->set_nameType = set_nameType;
#else	UseStaticMsgType
	OutP->set_nameType.msg_type_name = MSG_TYPE_INTEGER_32;
	OutP->set_nameType.msg_type_size = 32;
	OutP->set_nameType.msg_type_number = 1;
	OutP->set_nameType.msg_type_inline = TRUE;
	OutP->set_nameType.msg_type_longform = FALSE;
	OutP->set_nameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = TRUE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_set_deallocate */
mig_internal novalue _Xport_set_deallocate
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
		port_set_name_t set_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_set_deallocate
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_set_name_t set_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t set_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if (* (int *) &In0P->set_nameType != * (int *) &set_nameCheck)
#else	UseStaticMsgType
	if ((In0P->set_nameType.msg_type_inline != TRUE) ||
	    (In0P->set_nameType.msg_type_longform != FALSE) ||
	    (In0P->set_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->set_nameType.msg_type_number != 1) ||
	    (In0P->set_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_set_deallocate(task, In0P->set_name);
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

/* Routine port_set_add */
mig_internal novalue _Xport_set_add
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
		port_set_name_t set_name;
		msg_type_t port_nameType;
		port_name_t port_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_set_add
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_set_name_t set_name, port_name_t port_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t set_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t port_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if (* (int *) &In0P->set_nameType != * (int *) &set_nameCheck)
#else	UseStaticMsgType
	if ((In0P->set_nameType.msg_type_inline != TRUE) ||
	    (In0P->set_nameType.msg_type_longform != FALSE) ||
	    (In0P->set_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->set_nameType.msg_type_number != 1) ||
	    (In0P->set_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->port_nameType != * (int *) &port_nameCheck)
#else	UseStaticMsgType
	if ((In0P->port_nameType.msg_type_inline != TRUE) ||
	    (In0P->port_nameType.msg_type_longform != FALSE) ||
	    (In0P->port_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->port_nameType.msg_type_number != 1) ||
	    (In0P->port_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_set_add(task, In0P->set_name, In0P->port_name);
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

/* Routine port_set_remove */
mig_internal novalue _Xport_set_remove
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t port_nameType;
		port_name_t port_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_set_remove
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t port_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t port_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if (* (int *) &In0P->port_nameType != * (int *) &port_nameCheck)
#else	UseStaticMsgType
	if ((In0P->port_nameType.msg_type_inline != TRUE) ||
	    (In0P->port_nameType.msg_type_longform != FALSE) ||
	    (In0P->port_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->port_nameType.msg_type_number != 1) ||
	    (In0P->port_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_set_remove(task, In0P->port_name);
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

/* Routine port_set_status */
mig_internal novalue _Xport_set_status
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
		port_set_name_t set_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_long_t membersType;
		port_name_array_t members;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_set_status
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_set_name_t set_name, port_name_array_t *members, unsigned int *membersCnt);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t set_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_long_t membersType = {
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
	unsigned int membersCnt;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 32) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->set_nameType != * (int *) &set_nameCheck)
#else	UseStaticMsgType
	if ((In0P->set_nameType.msg_type_inline != TRUE) ||
	    (In0P->set_nameType.msg_type_longform != FALSE) ||
	    (In0P->set_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->set_nameType.msg_type_number != 1) ||
	    (In0P->set_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_set_status(task, In0P->set_name, &OutP->members, &membersCnt);
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

	msg_size = 48;

#if	UseStaticMsgType
	OutP->membersType = membersType;
#else	UseStaticMsgType
	OutP->membersType.msg_type_long_name = MSG_TYPE_INTEGER_32;
	OutP->membersType.msg_type_long_size = 32;
	OutP->membersType.msg_type_header.msg_type_inline = FALSE;
	OutP->membersType.msg_type_header.msg_type_longform = TRUE;
	OutP->membersType.msg_type_header.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->membersType.msg_type_long_number /* membersCnt */ = /* membersType.msg_type_long_number */ membersCnt;

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_insert_send */
mig_internal novalue _Xport_insert_send
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t my_portType;
		port_t my_port;
		msg_type_t its_nameType;
		port_name_t its_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_insert_send
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_t my_port, port_name_t its_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t my_portCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t its_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->my_portType != * (int *) &my_portCheck)
#else	UseStaticMsgType
	if ((In0P->my_portType.msg_type_inline != TRUE) ||
	    (In0P->my_portType.msg_type_longform != FALSE) ||
	    (In0P->my_portType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->my_portType.msg_type_number != 1) ||
	    (In0P->my_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->its_nameType != * (int *) &its_nameCheck)
#else	UseStaticMsgType
	if ((In0P->its_nameType.msg_type_inline != TRUE) ||
	    (In0P->its_nameType.msg_type_longform != FALSE) ||
	    (In0P->its_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->its_nameType.msg_type_number != 1) ||
	    (In0P->its_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_insert_send(task, In0P->my_port, In0P->its_name);
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

/* Routine port_extract_send */
mig_internal novalue _Xport_extract_send
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t its_nameType;
		port_name_t its_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t its_portType;
		port_t its_port;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_extract_send
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t its_name, port_t *its_port);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t its_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t its_portType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
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
	if (* (int *) &In0P->its_nameType != * (int *) &its_nameCheck)
#else	UseStaticMsgType
	if ((In0P->its_nameType.msg_type_inline != TRUE) ||
	    (In0P->its_nameType.msg_type_longform != FALSE) ||
	    (In0P->its_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->its_nameType.msg_type_number != 1) ||
	    (In0P->its_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_extract_send(task, In0P->its_name, &OutP->its_port);
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
	OutP->its_portType = its_portType;
#else	UseStaticMsgType
	OutP->its_portType.msg_type_name = MSG_TYPE_PORT;
	OutP->its_portType.msg_type_size = 32;
	OutP->its_portType.msg_type_number = 1;
	OutP->its_portType.msg_type_inline = TRUE;
	OutP->its_portType.msg_type_longform = FALSE;
	OutP->its_portType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_insert_receive */
mig_internal novalue _Xport_insert_receive
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t my_portType;
		port_t my_port;
		msg_type_t its_nameType;
		port_name_t its_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_insert_receive
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_t my_port, port_name_t its_name);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t my_portCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT_ALL,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t its_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
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
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->my_portType != * (int *) &my_portCheck)
#else	UseStaticMsgType
	if ((In0P->my_portType.msg_type_inline != TRUE) ||
	    (In0P->my_portType.msg_type_longform != FALSE) ||
	    (In0P->my_portType.msg_type_name != MSG_TYPE_PORT_ALL) ||
	    (In0P->my_portType.msg_type_number != 1) ||
	    (In0P->my_portType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->its_nameType != * (int *) &its_nameCheck)
#else	UseStaticMsgType
	if ((In0P->its_nameType.msg_type_inline != TRUE) ||
	    (In0P->its_nameType.msg_type_longform != FALSE) ||
	    (In0P->its_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->its_nameType.msg_type_number != 1) ||
	    (In0P->its_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_insert_receive(task, In0P->my_port, In0P->its_name);
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

/* Routine port_extract_receive */
mig_internal novalue _Xport_extract_receive
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t its_nameType;
		port_name_t its_name;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t its_portType;
		port_t its_port;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_extract_receive
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t its_name, port_t *its_port);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t its_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t its_portType = {
		/* msg_type_name = */		MSG_TYPE_PORT_ALL,
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
	if (* (int *) &In0P->its_nameType != * (int *) &its_nameCheck)
#else	UseStaticMsgType
	if ((In0P->its_nameType.msg_type_inline != TRUE) ||
	    (In0P->its_nameType.msg_type_longform != FALSE) ||
	    (In0P->its_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->its_nameType.msg_type_number != 1) ||
	    (In0P->its_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_extract_receive(task, In0P->its_name, &OutP->its_port);
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
	OutP->its_portType = its_portType;
#else	UseStaticMsgType
	OutP->its_portType.msg_type_name = MSG_TYPE_PORT_ALL;
	OutP->its_portType.msg_type_size = 32;
	OutP->its_portType.msg_type_number = 1;
	OutP->its_portType.msg_type_inline = TRUE;
	OutP->its_portType.msg_type_longform = FALSE;
	OutP->its_portType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine port_set_backup */
mig_internal novalue _Xport_set_backup
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t port_nameType;
		port_name_t port_name;
		msg_type_t backupType;
		port_t backup;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_t previousType;
		port_t previous;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t port_set_backup
#if	(defined(__STDC__) || defined(c_plusplus))
		(task_t task, port_name_t port_name, port_t backup, port_t *previous);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t port_nameCheck = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t backupCheck = {
		/* msg_type_name = */		MSG_TYPE_PORT,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t previousType = {
		/* msg_type_name = */		MSG_TYPE_PORT,
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
	if ((msg_size != 40) || (msg_simple != FALSE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	task /* convert_port_to_task 0 Head.msg_request_port */ = /* task */ convert_port_to_task(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->port_nameType != * (int *) &port_nameCheck)
#else	UseStaticMsgType
	if ((In0P->port_nameType.msg_type_inline != TRUE) ||
	    (In0P->port_nameType.msg_type_longform != FALSE) ||
	    (In0P->port_nameType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->port_nameType.msg_type_number != 1) ||
	    (In0P->port_nameType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->backupType != * (int *) &backupCheck)
#else	UseStaticMsgType
	if ((In0P->backupType.msg_type_inline != TRUE) ||
	    (In0P->backupType.msg_type_longform != FALSE) ||
	    (In0P->backupType.msg_type_name != MSG_TYPE_PORT) ||
	    (In0P->backupType.msg_type_number != 1) ||
	    (In0P->backupType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = port_set_backup(task, In0P->port_name, In0P->backup, &OutP->previous);
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
	OutP->previousType = previousType;
#else	UseStaticMsgType
	OutP->previousType.msg_type_name = MSG_TYPE_PORT;
	OutP->previousType.msg_type_size = 32;
	OutP->previousType.msg_type_number = 1;
	OutP->previousType.msg_type_inline = TRUE;
	OutP->previousType.msg_type_longform = FALSE;
	OutP->previousType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	OutP->Head.msg_simple = FALSE;
	OutP->Head.msg_size = msg_size;
}

/* Routine vm_synchronize */
mig_internal novalue _Xvm_synchronize
#if	(defined(__STDC__) || defined(c_plusplus))
	(msg_header_t *InHeadP, msg_header_t *OutHeadP)
#else
	(InHeadP, OutHeadP)
	msg_header_t *InHeadP, *OutHeadP;
#endif
{
	typedef struct {
		msg_header_t Head;
		msg_type_t addressType;
		vm_address_t address;
		msg_type_t sizeType;
		vm_size_t size;
	} Request;

	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	register Request *In0P = (Request *) InHeadP;
	register Reply *OutP = (Reply *) OutHeadP;
	extern kern_return_t vm_synchronize
#if	(defined(__STDC__) || defined(c_plusplus))
		(vm_map_t target_task, vm_address_t address, vm_size_t size);
#else
		();
#endif

#if	TypeCheck
	boolean_t msg_simple;
#endif	TypeCheck

	unsigned int msg_size;

#if	UseStaticMsgType
	static const msg_type_t addressCheck = {
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

	vm_map_t target_task;

#if	TypeCheck
	msg_size = In0P->Head.msg_size;
	msg_simple = In0P->Head.msg_simple;
	if ((msg_size != 40) || (msg_simple != TRUE))
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; return; }
#endif	TypeCheck

	target_task /* convert_port_to_map 0 Head.msg_request_port */ = /* target_task */ convert_port_to_map(In0P->Head.msg_request_port);

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->addressType != * (int *) &addressCheck)
#else	UseStaticMsgType
	if ((In0P->addressType.msg_type_inline != TRUE) ||
	    (In0P->addressType.msg_type_longform != FALSE) ||
	    (In0P->addressType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->addressType.msg_type_number != 1) ||
	    (In0P->addressType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

#if	TypeCheck
#if	UseStaticMsgType
	if (* (int *) &In0P->sizeType != * (int *) &sizeCheck)
#else	UseStaticMsgType
	if ((In0P->sizeType.msg_type_inline != TRUE) ||
	    (In0P->sizeType.msg_type_longform != FALSE) ||
	    (In0P->sizeType.msg_type_name != MSG_TYPE_INTEGER_32) ||
	    (In0P->sizeType.msg_type_number != 1) ||
	    (In0P->sizeType.msg_type_size != 32))
#endif	UseStaticMsgType
		{ OutP->RetCode = MIG_BAD_ARGUMENTS; goto punt1; }
#define	label_punt1
#endif	TypeCheck

	OutP->RetCode = vm_synchronize(target_task, In0P->address, In0P->size);
#ifdef	label_punt1
#undef	label_punt1
punt1:
#endif	label_punt1
	vm_map_deallocate(target_task);
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

boolean_t mach_server
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

	if ((InP->msg_id > 2101) || (InP->msg_id < 2000))
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
			0,
			0,
			_Xtask_create,
			_Xtask_terminate,
			0,
			0,
			_Xtask_threads,
			0,
			0,
			0,
			0,
			_Xthread_terminate,
			0,
			0,
			0,
			0,
			_Xvm_allocate,
			_Xvm_allocate_with_pager,
			_Xvm_deallocate,
			_Xvm_protect,
			_Xvm_inherit,
			_Xvm_read,
			_Xvm_write,
			_Xvm_copy,
			_Xvm_region,
			_Xvm_statistics,
			_Xtask_by_unix_pid,
			0,
			_Xmach_ports_register,
			_Xmach_ports_lookup,
			_Xunix_pid,
			_Xnetipc_listen,
			_Xnetipc_ignore,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			_Xxxx_host_info,
			_Xxxx_slot_info,
			_Xxxx_cpu_control,
			0,
			0,
			0,
			0,
			0,
			0,
			_Xtask_suspend,
			_Xtask_resume,
			_Xtask_get_special_port,
			_Xtask_set_special_port,
			_Xtask_info,
			_Xthread_create,
			_Xthread_suspend,
			_Xthread_resume,
			_Xthread_abort,
			_Xthread_get_state,
			_Xthread_set_state,
			_Xthread_get_special_port,
			_Xthread_set_special_port,
			_Xthread_info,
			0,
			0,
			0,
			_Xport_names,
			_Xport_type,
			_Xport_rename,
			_Xport_allocate,
			_Xport_deallocate,
			_Xport_set_backlog,
			_Xport_status,
			_Xport_set_allocate,
			_Xport_set_deallocate,
			_Xport_set_add,
			_Xport_set_remove,
			_Xport_set_status,
			_Xport_insert_send,
			_Xport_extract_send,
			_Xport_insert_receive,
			_Xport_extract_receive,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			_Xport_set_backup,
			0,
			0,
			_Xvm_synchronize,
		};

		if (routines[InP->msg_id - 2000])
			(routines[InP->msg_id - 2000]) (InP, &OutP->Head);
		 else
			return FALSE;
	}
	return TRUE;
}
