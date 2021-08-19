#ifndef	_kern_server_reply
#define	_kern_server_reply

/* Module kern_server_reply */

#include <sys/kern_return.h>
#if	(defined(__STDC__) || defined(c_plusplus)) || defined(LINTLIBRARY)
#include <sys/port.h>
#include <sys/message.h>
#endif

#ifndef	mig_external
#define mig_external extern
#endif

#include <kern/std_types.h>
#include <kernserv/kern_server_reply_types.h>

/* Routine kern_serv_panic */
mig_external kern_return_t kern_serv_panic
#if	defined(LINTLIBRARY)
    (boot_port, panic_msg)
	port_t boot_port;
	panic_msg_t panic_msg;
{ return kern_serv_panic(boot_port, panic_msg); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t boot_port,
	panic_msg_t panic_msg
);
#else
    ();
#endif
#endif

/* Routine kern_serv_section_by_name */
mig_external kern_return_t kern_serv_section_by_name
#if	defined(LINTLIBRARY)
    (boot_port, segname, sectname, addr, size)
	port_t boot_port;
	macho_header_name_t segname;
	macho_header_name_t sectname;
	vm_address_t *addr;
	vm_size_t *size;
{ return kern_serv_section_by_name(boot_port, segname, sectname, addr, size); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t boot_port,
	macho_header_name_t segname,
	macho_header_name_t sectname,
	vm_address_t *addr,
	vm_size_t *size
);
#else
    ();
#endif
#endif

/* SimpleRoutine kern_serv_log_data */
mig_external kern_return_t kern_serv_log_data
#if	defined(LINTLIBRARY)
    (log_port, log, logCnt)
	port_t log_port;
	log_entry_array_t log;
	unsigned int logCnt;
{ return kern_serv_log_data(log_port, log, logCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t log_port,
	log_entry_array_t log,
	unsigned int logCnt
);
#else
    ();
#endif
#endif

#endif	_kern_server_reply
