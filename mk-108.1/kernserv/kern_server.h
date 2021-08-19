#ifndef	_kern_server
#define	_kern_server

/* Module kern_server */

#include <sys/kern_return.h>
#if	(defined(__STDC__) || defined(c_plusplus)) || defined(LINTLIBRARY)
#include <sys/port.h>
#include <sys/message.h>
#endif

#ifndef	mig_external
#define mig_external extern
#endif

#include <kern/std_types.h>
#include <kernserv/kern_server_types.h>

/* Routine instance_loc */
mig_external kern_return_t kern_serv_instance_loc
#if	defined(LINTLIBRARY)
    (server_port, instance_loc)
	port_t server_port;
	vm_address_t instance_loc;
{ return kern_serv_instance_loc(server_port, instance_loc); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	vm_address_t instance_loc
);
#else
    ();
#endif
#endif

/* Routine boot_port */
mig_external kern_return_t kern_serv_boot_port
#if	defined(LINTLIBRARY)
    (server_port, boot_port)
	port_t server_port;
	port_t boot_port;
{ return kern_serv_boot_port(server_port, boot_port); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	port_t boot_port
);
#else
    ();
#endif
#endif

/* Routine wire_range */
mig_external kern_return_t kern_serv_wire_range
#if	defined(LINTLIBRARY)
    (server_port, addr, size)
	port_t server_port;
	vm_address_t addr;
	vm_size_t size;
{ return kern_serv_wire_range(server_port, addr, size); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	vm_address_t addr,
	vm_size_t size
);
#else
    ();
#endif
#endif

/* Routine unwire_range */
mig_external kern_return_t kern_serv_unwire_range
#if	defined(LINTLIBRARY)
    (server_port, addr, size)
	port_t server_port;
	vm_address_t addr;
	vm_size_t size;
{ return kern_serv_unwire_range(server_port, addr, size); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	vm_address_t addr,
	vm_size_t size
);
#else
    ();
#endif
#endif

/* Routine port_proc */
mig_external kern_return_t kern_serv_port_proc
#if	defined(LINTLIBRARY)
    (server_port, port, proc, arg)
	port_t server_port;
	port_all_t port;
	port_map_proc_t proc;
	int arg;
{ return kern_serv_port_proc(server_port, port, proc, arg); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	port_all_t port,
	port_map_proc_t proc,
	int arg
);
#else
    ();
#endif
#endif

/* SimpleRoutine port_death_proc */
mig_external kern_return_t kern_serv_port_death_proc
#if	defined(LINTLIBRARY)
    (server_port, proc)
	port_t server_port;
	port_death_proc_t proc;
{ return kern_serv_port_death_proc(server_port, proc); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	port_death_proc_t proc
);
#else
    ();
#endif
#endif

/* Routine call_proc */
mig_external kern_return_t kern_serv_call_proc
#if	defined(LINTLIBRARY)
    (server_port, proc, arg)
	port_t server_port;
	call_proc_t proc;
	int arg;
{ return kern_serv_call_proc(server_port, proc, arg); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	call_proc_t proc,
	int arg
);
#else
    ();
#endif
#endif

/* SimpleRoutine shutdown */
mig_external kern_return_t kern_serv_shutdown
#if	defined(LINTLIBRARY)
    (server_port)
	port_t server_port;
{ return kern_serv_shutdown(server_port); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port
);
#else
    ();
#endif
#endif

/* SimpleRoutine log_level */
mig_external kern_return_t kern_serv_log_level
#if	defined(LINTLIBRARY)
    (server_port, log_level)
	port_t server_port;
	int log_level;
{ return kern_serv_log_level(server_port, log_level); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	int log_level
);
#else
    ();
#endif
#endif

/* SimpleRoutine get_log */
mig_external kern_return_t kern_serv_get_log
#if	defined(LINTLIBRARY)
    (server_port, reply_port)
	port_t server_port;
	port_t reply_port;
{ return kern_serv_get_log(server_port, reply_port); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	port_t reply_port
);
#else
    ();
#endif
#endif

/* Routine port_serv */
mig_external kern_return_t kern_serv_port_serv
#if	defined(LINTLIBRARY)
    (server_port, port, proc, arg)
	port_t server_port;
	port_all_t port;
	port_map_proc_t proc;
	int arg;
{ return kern_serv_port_serv(server_port, port, proc, arg); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	port_all_t port,
	port_map_proc_t proc,
	int arg
);
#else
    ();
#endif
#endif

/* Routine version */
mig_external kern_return_t kern_serv_version
#if	defined(LINTLIBRARY)
    (server_port, version)
	port_t server_port;
	int version;
{ return kern_serv_version(server_port, version); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t server_port,
	int version
);
#else
    ();
#endif
#endif

#endif	_kern_server
