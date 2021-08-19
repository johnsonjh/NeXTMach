#ifndef	_pager_default
#define	_pager_default

/* Module pager_default */

#include <sys/kern_return.h>
#if	(defined(__STDC__) || defined(c_plusplus)) || defined(LINTLIBRARY)
#include <sys/port.h>
#include <sys/message.h>
#endif

#ifndef	mig_external
#define mig_external extern
#endif

#include "kern/mach_types.h"

/* SimpleRoutine pager_create */
mig_external kern_return_t pager_create
#if	defined(LINTLIBRARY)
    (old_paging_object, new_paging_object, new_request_port, new_name, new_page_size)
	paging_object_t old_paging_object;
	port_all_t new_paging_object;
	port_t new_request_port;
	port_t new_name;
	vm_size_t new_page_size;
{ return pager_create(old_paging_object, new_paging_object, new_request_port, new_name, new_page_size); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	paging_object_t old_paging_object,
	port_all_t new_paging_object,
	port_t new_request_port,
	port_t new_name,
	vm_size_t new_page_size
);
#else
    ();
#endif
#endif

/* SimpleRoutine pager_data_initialize */
mig_external kern_return_t pager_data_initialize
#if	defined(LINTLIBRARY)
    (paging_object, pager_request_port, offset, data, dataCnt)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	pointer_t data;
	unsigned int dataCnt;
{ return pager_data_initialize(paging_object, pager_request_port, offset, data, dataCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	paging_object_t paging_object,
	port_t pager_request_port,
	vm_offset_t offset,
	pointer_t data,
	unsigned int dataCnt
);
#else
    ();
#endif
#endif

#endif	_pager_default
