#ifndef	_mach_debug
#define	_mach_debug

/* Module mach_debug */

#include <sys/kern_return.h>
#if	(defined(__STDC__) || defined(c_plusplus)) || defined(LINTLIBRARY)
#include <sys/port.h>
#include <sys/message.h>
#endif

#ifndef	mig_external
#define mig_external extern
#endif

#include <kern/std_types.h>
#include <kern/mach_types.h>
#include <kern/mach_debug_types.h>

/* Routine host_zone_info */
mig_external kern_return_t host_zone_info
#if	defined(LINTLIBRARY)
    (task, names, namesCnt, info, infoCnt)
	task_t task;
	zone_name_array_t *names;
	unsigned int *namesCnt;
	zone_info_array_t *info;
	unsigned int *infoCnt;
{ return host_zone_info(task, names, namesCnt, info, infoCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	task_t task,
	zone_name_array_t *names,
	unsigned int *namesCnt,
	zone_info_array_t *info,
	unsigned int *infoCnt
);
#else
    ();
#endif
#endif

#endif	_mach_debug
