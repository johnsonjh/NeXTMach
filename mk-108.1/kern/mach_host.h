#ifndef	_mach_host
#define	_mach_host

/* Module mach_host */

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

/* Routine host_processors */
mig_external kern_return_t host_processors
#if	defined(LINTLIBRARY)
    (host_priv, processor_list, processor_listCnt)
	host_priv_t host_priv;
	processor_array_t *processor_list;
	unsigned int *processor_listCnt;
{ return host_processors(host_priv, processor_list, processor_listCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	host_priv_t host_priv,
	processor_array_t *processor_list,
	unsigned int *processor_listCnt
);
#else
    ();
#endif
#endif

/* Routine host_info */
mig_external kern_return_t host_info
#if	defined(LINTLIBRARY)
    (host, flavor, host_info_out, host_info_outCnt)
	host_t host;
	int flavor;
	host_info_t host_info_out;
	unsigned int *host_info_outCnt;
{ return host_info(host, flavor, host_info_out, host_info_outCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	host_t host,
	int flavor,
	host_info_t host_info_out,
	unsigned int *host_info_outCnt
);
#else
    ();
#endif
#endif

/* Routine processor_info */
mig_external kern_return_t processor_info
#if	defined(LINTLIBRARY)
    (processor, flavor, host, processor_info_out, processor_info_outCnt)
	processor_t processor;
	int flavor;
	host_t *host;
	processor_info_t processor_info_out;
	unsigned int *processor_info_outCnt;
{ return processor_info(processor, flavor, host, processor_info_out, processor_info_outCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_t processor,
	int flavor,
	host_t *host,
	processor_info_t processor_info_out,
	unsigned int *processor_info_outCnt
);
#else
    ();
#endif
#endif

/* Routine processor_start */
mig_external kern_return_t processor_start
#if	defined(LINTLIBRARY)
    (processor)
	processor_t processor;
{ return processor_start(processor); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_t processor
);
#else
    ();
#endif
#endif

/* Routine processor_exit */
mig_external kern_return_t processor_exit
#if	defined(LINTLIBRARY)
    (processor)
	processor_t processor;
{ return processor_exit(processor); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_t processor
);
#else
    ();
#endif
#endif

/* Routine processor_control */
mig_external kern_return_t processor_control
#if	defined(LINTLIBRARY)
    (processor, processor_cmd, processor_cmdCnt)
	processor_t processor;
	processor_info_t processor_cmd;
	unsigned int processor_cmdCnt;
{ return processor_control(processor, processor_cmd, processor_cmdCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_t processor,
	processor_info_t processor_cmd,
	unsigned int processor_cmdCnt
);
#else
    ();
#endif
#endif

/* Routine processor_set_default */
mig_external kern_return_t processor_set_default
#if	defined(LINTLIBRARY)
    (host, default_set)
	host_t host;
	processor_set_name_t *default_set;
{ return processor_set_default(host, default_set); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	host_t host,
	processor_set_name_t *default_set
);
#else
    ();
#endif
#endif

/* Routine processor_set_create */
mig_external kern_return_t processor_set_create
#if	defined(LINTLIBRARY)
    (host, new_set, new_name)
	host_t host;
	port_t *new_set;
	port_t *new_name;
{ return processor_set_create(host, new_set, new_name); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	host_t host,
	port_t *new_set,
	port_t *new_name
);
#else
    ();
#endif
#endif

/* Routine processor_set_destroy */
mig_external kern_return_t processor_set_destroy
#if	defined(LINTLIBRARY)
    (set)
	processor_set_t set;
{ return processor_set_destroy(set); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_set_t set
);
#else
    ();
#endif
#endif

/* Routine processor_set_info */
mig_external kern_return_t processor_set_info
#if	defined(LINTLIBRARY)
    (set_name, flavor, host, info_out, info_outCnt)
	processor_set_name_t set_name;
	int flavor;
	host_t *host;
	processor_set_info_t info_out;
	unsigned int *info_outCnt;
{ return processor_set_info(set_name, flavor, host, info_out, info_outCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_set_name_t set_name,
	int flavor,
	host_t *host,
	processor_set_info_t info_out,
	unsigned int *info_outCnt
);
#else
    ();
#endif
#endif

/* Routine processor_assign */
mig_external kern_return_t processor_assign
#if	defined(LINTLIBRARY)
    (processor, new_set, wait)
	processor_t processor;
	processor_set_t new_set;
	boolean_t wait;
{ return processor_assign(processor, new_set, wait); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_t processor,
	processor_set_t new_set,
	boolean_t wait
);
#else
    ();
#endif
#endif

/* Routine processor_get_assignment */
mig_external kern_return_t processor_get_assignment
#if	defined(LINTLIBRARY)
    (processor, assigned_set)
	processor_t processor;
	processor_set_name_t *assigned_set;
{ return processor_get_assignment(processor, assigned_set); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_t processor,
	processor_set_name_t *assigned_set
);
#else
    ();
#endif
#endif

/* Routine thread_assign */
mig_external kern_return_t thread_assign
#if	defined(LINTLIBRARY)
    (thread, new_set)
	thread_t thread;
	processor_set_t new_set;
{ return thread_assign(thread, new_set); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	thread_t thread,
	processor_set_t new_set
);
#else
    ();
#endif
#endif

/* Routine thread_assign_default */
mig_external kern_return_t thread_assign_default
#if	defined(LINTLIBRARY)
    (thread)
	thread_t thread;
{ return thread_assign_default(thread); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	thread_t thread
);
#else
    ();
#endif
#endif

/* Routine thread_get_assignment */
mig_external kern_return_t thread_get_assignment
#if	defined(LINTLIBRARY)
    (thread, assigned_set)
	thread_t thread;
	processor_set_name_t *assigned_set;
{ return thread_get_assignment(thread, assigned_set); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	thread_t thread,
	processor_set_name_t *assigned_set
);
#else
    ();
#endif
#endif

/* Routine task_assign */
mig_external kern_return_t task_assign
#if	defined(LINTLIBRARY)
    (task, new_set, assign_threads)
	task_t task;
	processor_set_t new_set;
	boolean_t assign_threads;
{ return task_assign(task, new_set, assign_threads); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	task_t task,
	processor_set_t new_set,
	boolean_t assign_threads
);
#else
    ();
#endif
#endif

/* Routine task_assign_default */
mig_external kern_return_t task_assign_default
#if	defined(LINTLIBRARY)
    (task, assign_threads)
	task_t task;
	boolean_t assign_threads;
{ return task_assign_default(task, assign_threads); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	task_t task,
	boolean_t assign_threads
);
#else
    ();
#endif
#endif

/* Routine task_get_assignment */
mig_external kern_return_t task_get_assignment
#if	defined(LINTLIBRARY)
    (task, assigned_set)
	task_t task;
	processor_set_name_t *assigned_set;
{ return task_get_assignment(task, assigned_set); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	task_t task,
	processor_set_name_t *assigned_set
);
#else
    ();
#endif
#endif

/* Routine host_kernel_version */
mig_external kern_return_t host_kernel_version
#if	defined(LINTLIBRARY)
    (host, kernel_version)
	host_t host;
	kernel_version_t kernel_version;
{ return host_kernel_version(host, kernel_version); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	host_t host,
	kernel_version_t kernel_version
);
#else
    ();
#endif
#endif

/* Routine thread_priority */
mig_external kern_return_t thread_priority
#if	defined(LINTLIBRARY)
    (thread, priority, set_max)
	thread_t thread;
	int priority;
	boolean_t set_max;
{ return thread_priority(thread, priority, set_max); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	thread_t thread,
	int priority,
	boolean_t set_max
);
#else
    ();
#endif
#endif

/* Routine thread_max_priority */
mig_external kern_return_t thread_max_priority
#if	defined(LINTLIBRARY)
    (thread, processor_set, max_priority)
	thread_t thread;
	processor_set_t processor_set;
	int max_priority;
{ return thread_max_priority(thread, processor_set, max_priority); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	thread_t thread,
	processor_set_t processor_set,
	int max_priority
);
#else
    ();
#endif
#endif

/* Routine task_priority */
mig_external kern_return_t task_priority
#if	defined(LINTLIBRARY)
    (task, priority, change_threads)
	task_t task;
	int priority;
	boolean_t change_threads;
{ return task_priority(task, priority, change_threads); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	task_t task,
	int priority,
	boolean_t change_threads
);
#else
    ();
#endif
#endif

/* Routine processor_set_max_priority */
mig_external kern_return_t processor_set_max_priority
#if	defined(LINTLIBRARY)
    (processor_set, max_priority, change_threads)
	processor_set_t processor_set;
	int max_priority;
	boolean_t change_threads;
{ return processor_set_max_priority(processor_set, max_priority, change_threads); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_set_t processor_set,
	int max_priority,
	boolean_t change_threads
);
#else
    ();
#endif
#endif

/* Routine thread_policy */
mig_external kern_return_t thread_policy
#if	defined(LINTLIBRARY)
    (thread, policy, data)
	thread_t thread;
	int policy;
	int data;
{ return thread_policy(thread, policy, data); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	thread_t thread,
	int policy,
	int data
);
#else
    ();
#endif
#endif

/* Routine processor_set_policy_enable */
mig_external kern_return_t processor_set_policy_enable
#if	defined(LINTLIBRARY)
    (processor_set, policy)
	processor_set_t processor_set;
	int policy;
{ return processor_set_policy_enable(processor_set, policy); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_set_t processor_set,
	int policy
);
#else
    ();
#endif
#endif

/* Routine processor_set_policy_disable */
mig_external kern_return_t processor_set_policy_disable
#if	defined(LINTLIBRARY)
    (processor_set, policy, change_threads)
	processor_set_t processor_set;
	int policy;
	boolean_t change_threads;
{ return processor_set_policy_disable(processor_set, policy, change_threads); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_set_t processor_set,
	int policy,
	boolean_t change_threads
);
#else
    ();
#endif
#endif

/* Routine processor_set_tasks */
mig_external kern_return_t processor_set_tasks
#if	defined(LINTLIBRARY)
    (processor_set, task_list, task_listCnt)
	processor_set_t processor_set;
	task_array_t *task_list;
	unsigned int *task_listCnt;
{ return processor_set_tasks(processor_set, task_list, task_listCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_set_t processor_set,
	task_array_t *task_list,
	unsigned int *task_listCnt
);
#else
    ();
#endif
#endif

/* Routine processor_set_threads */
mig_external kern_return_t processor_set_threads
#if	defined(LINTLIBRARY)
    (processor_set, thread_list, thread_listCnt)
	processor_set_t processor_set;
	thread_array_t *thread_list;
	unsigned int *thread_listCnt;
{ return processor_set_threads(processor_set, thread_list, thread_listCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	processor_set_t processor_set,
	thread_array_t *thread_list,
	unsigned int *thread_listCnt
);
#else
    ();
#endif
#endif

/* Routine host_processor_sets */
mig_external kern_return_t host_processor_sets
#if	defined(LINTLIBRARY)
    (host, processor_set_names, processor_set_namesCnt)
	host_t host;
	processor_set_name_array_t *processor_set_names;
	unsigned int *processor_set_namesCnt;
{ return host_processor_sets(host, processor_set_names, processor_set_namesCnt); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	host_t host,
	processor_set_name_array_t *processor_set_names,
	unsigned int *processor_set_namesCnt
);
#else
    ();
#endif
#endif

/* Routine host_processor_set_priv */
mig_external kern_return_t host_processor_set_priv
#if	defined(LINTLIBRARY)
    (host_priv, set_name, set)
	host_priv_t host_priv;
	processor_set_name_t set_name;
	processor_set_t *set;
{ return host_processor_set_priv(host_priv, set_name, set); }
#else
#if	(defined(__STDC__) || defined(c_plusplus))
(
	host_priv_t host_priv,
	processor_set_name_t set_name,
	processor_set_t *set
);
#else
    ();
#endif
#endif

#endif	_mach_host
