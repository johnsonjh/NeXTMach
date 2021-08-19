#ifndef	_pager
#define	_pager

/* Module pager */

#include <sys/kern_return.h>
#if	(defined(__STDC__) || defined(c_plusplus)) || defined(LINTLIBRARY)
#include <sys/port.h>
#include <sys/message.h>
#endif

#ifndef	mig_external
#define mig_external extern
#endif

#include "kern/mach_types.h"

/* SimpleRoutine pager_init */
mig_external kern_return_t pager_init
#if	defined(LINTLIBRARY)
    (paging_object, pager_request_port, pager_name, page_size)
	paging_object_t paging_object;
	port_t pager_request_port;
	port_t pager_name;
	vm_size_t page_size;
{ return pager_init(paging_object, pager_request_port, pager_name, page_size); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	paging_object_t paging_object,
	port_t pager_request_port,
	port_t pager_name,
	vm_size_t page_size
);
#else
    ();
#endif
#endif

/* SimpleRoutine pager_data_request */
mig_external kern_return_t pager_data_request
#if	defined(LINTLIBRARY)
    (paging_object, pager_request_port, offset, length, desired_access)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	vm_size_t length;
	vm_prot_t desired_access;
{ return pager_data_request(paging_object, pager_request_port, offset, length, desired_access); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	paging_object_t paging_object,
	port_t pager_request_port,
	vm_offset_t offset,
	vm_size_t length,
	vm_prot_t desired_access
);
#else
    ();
#endif
#endif

/* SimpleRoutine pager_data_unlock */
mig_external kern_return_t pager_data_unlock
#if	defined(LINTLIBRARY)
    (paging_object, pager_request_port, offset, length, desired_access)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	vm_size_t length;
	vm_prot_t desired_access;
{ return pager_data_unlock(paging_object, pager_request_port, offset, length, desired_access); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	paging_object_t paging_object,
	port_t pager_request_port,
	vm_offset_t offset,
	vm_size_t length,
	vm_prot_t desired_access
);
#else
    ();
#endif
#endif

/* SimpleRoutine pager_data_write */
mig_external kern_return_t pager_data_write
#if	defined(LINTLIBRARY)
    (paging_object, pager_request_port, offset, data, dataCnt)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	pointer_t data;
	unsigned int dataCnt;
{ return pager_data_write(paging_object, pager_request_port, offset, data, dataCnt); }
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

/* SimpleRoutine pager_lock_completed */
mig_external kern_return_t pager_lock_completed
#if	defined(LINTLIBRARY)
    (paging_object, pager_request_port, offset, length)
	paging_object_t paging_object;
	port_t pager_request_port;
	vm_offset_t offset;
	vm_size_t length;
{ return pager_lock_completed(paging_object, pager_request_port, offset, length); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	paging_object_t paging_object,
	port_t pager_request_port,
	vm_offset_t offset,
	vm_size_t length
);
#else
    ();
#endif
#endif

#endif	_pager
