#ifndef	_exc
#define	_exc

/* Module exc */

#include <sys/kern_return.h>
#if	(defined(__STDC__) || defined(c_plusplus)) || defined(LINTLIBRARY)
#include <sys/port.h>
#include <sys/message.h>
#endif

#ifndef	mig_external
#define mig_external extern
#endif

#include <kern/std_types.h>

/* Routine exception_raise */
mig_external kern_return_t exception_raise
#if	defined(LINTLIBRARY)
    (exception_port, clear_port, thread, task, exception, code, subcode)
	port_t exception_port;
	port_t clear_port;
	port_t thread;
	port_t task;
	int exception;
	int code;
	int subcode;
{ return exception_raise(exception_port, clear_port, thread, task, exception, code, subcode); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	port_t exception_port,
	port_t clear_port,
	port_t thread,
	port_t task,
	int exception,
	int code,
	int subcode
);
#else
    ();
#endif
#endif

#endif	_exc
