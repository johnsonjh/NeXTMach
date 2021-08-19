/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 27-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Fixed bugs in kern_serv_callout.
 *	Remove compatibility for servers < version 2.
 *
 * 12-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Don't allocate MSG_SIZE_MAX for in and out messages.  Only allocate
 *	a minimal amount fo input message and use RCV_TOO_LARGE logic
 *	to allocate more.  Output message is only allocated for servers
 *	and older style handlers.
 *
 * 23-May-90  Gregg kellogg (gk) at NeXT
 *	Changed to use thread_reply port instead of bootstrap port.
 *	Checks to see if it's running in the kernel_task and uses
 *	kernel_ipc_space accordingly.
 *	Terminates on shutdown, doesn't suspend.
 *	Doesn't depend on being in a separate task now.
 *
 * 06-Feb-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

/*
 * Generic Kernel Server for loaded server tasks.
 *
 * This code is run by dynamically loaded Mach kernel servers.  It recieves
 * instructions from it's boot port server to set up it's local environment
 * and enter the server loop to do per-server processing.
 *
 * The server basically exists to dispatch messages recieved on ports
 * to procedures.  Any un-handled messages are either thrown away or handled
 * directly by the server.  Handled messages are either acked, or not acked.
 * If the return status is > 0, then any resources passed (other than ports)
 * are deallocated from the message as well.
 */

#import <sys/mig_errors.h>
#import <sys/callout.h>
#import <sys/printf.h>
#import <sys/notify.h>
#import <kern/kern_port.h>
#import <kern/sched_prim.h>
#import <kern/ipc_ptraps.h>
#import <kern/mach_traps.h>
#import <kern/kalloc.h>
#import <kern/ipc_notify.h>
#import <vm/vm_param.h>

#import <next/spl.h>

#import <kernserv/kern_server_handler.h>
#import <kernserv/kern_server_reply.h>

#define panic(s) (curipl() == 0 ? kern_serv_panic(ksp->bootstrap_port, s) \
				: printf("can't panic: %s\n", s))

static port_t kernel_port;

/*
 * Use internal version of thread_terminate.
 */
#undef	thread_terminate

/*
 * Locally used functions
 */
static kern_return_t kern_serv_dispatch (
	msg_header_t *in_msg,
	kern_server_t ksp);

static void kern_serv_send_log(void *arg);
static void kern_serv_log_init (	// initialize server's log buffer
	log_t	*log,			// uninitialized log struct
	int	num_entries);		// size of log

static void kern_serv_log_free (log_t *log); // deallocate log structure
static void kern_serv_interrupt_server(thread_t thread);

const kern_serv_t kern_serv_proto = {
	0,		// arg to be filled in
	0,		// don't wait for message to be sent
	kern_serv_instance_loc,
	kern_serv_boot_port,
	kern_serv_wire_range,
	kern_serv_unwire_range,
	kern_serv_port_proc,
	kern_serv_port_death_proc,
	kern_serv_call_proc,
	kern_serv_shutdown,
	kern_serv_log_level,
	kern_serv_get_log,
	kern_serv_port_serv,
	kern_serv_version};

/*
 * Prototypes for kernel functions not yet prototyped.
 */
int bcopy(void *to, void *from, int size);
int curipl(void);
void softint_sched(int level, int (*proc)(), void *arg);
kern_return_t vm_map_pageable(
	vm_map_t	map,
	vm_offset_t	start,
	vm_offset_t	end,
	boolean_t	new_pageable);
#if	DEBUG
void xpr(char *msg, ...);
#endif	DEBUG

#if	DEBUG
static boolean_t kserv_debug = TRUE;
#define ks_log(s) {if (kserv_debug) XPR(XPR_LDD, s); }
#define ks_log2(s) ks_log(s)
void kern_server_main1(void);
void kern_server_main2(void);
#else	DEBUG
#define ks_log(s)
#define ks_log2(s)
#endif	DEBUG

/*
 * Entry point to kernel server, set up task environment.
 */
void kern_server_main(void)
{
#if	DEBUG
	int foo[10];
	kern_server_main1();
}
void kern_server_main1(void)
{
	int foo[10];
	kern_server_main2();
}
void kern_server_main2(void)
{
#endif	DEBUG
	kern_return_t	r;
	kern_server_t	ksp = kalloc(sizeof *ksp);
	port_t		boot_listener_port;
	port_t		bootstrap_port;
	port_t		reply_port;
	port_set_name_t	port_set;
	msg_header_t	*in_msg;
	int		error;
	register int	i, s;
	kern_serv_t kern_serv = kern_serv_proto;

#if	DEBUG
	int instr;

	/*
	 * Write to the loaded text so that the debugger can set break-points.
	 */
	instr = *(int *)&kern_server_main2;
	*(int *)&kern_server_main2 = instr;
#endif	DEBUG

	kern_serv.arg = (void *)&ksp;

	/*
	 *	Find out who we are.
	 */

	if (current_task() != kernel_task) {
		current_task()->kernel_vm_space = TRUE;
		current_task()->kernel_ipc_space = FALSE;
	}
	current_thread()->ipc_kernel = TRUE;

	bzero((char *)ksp, sizeof(*ksp));
	ksp->version = -1;

	/*
	 * Our task port.
	 */
	ksp->task_port = task_self();

	/*
	 * Our thread structure.
	 */
	ksp->server_thread = current_thread();
	simple_lock_init(&ksp->slock);

	/*
	 * Initialize message send queue.
	 */
	queue_init(&ksp->msg_callout_q);
	queue_init(&ksp->msg_callout_fq);
	queue_init(&ksp->notify_q);
	for (  i = sizeof(ksp->msg_send_array)/sizeof(ksp->msg_send_array[0])-1
	     ; i >= 0
	     ; i--)
	{
		queue_enter(&ksp->msg_callout_fq, &ksp->msg_send_array[i],
			struct msg_send_entry *, link);
	}

	/*
	 * Use our thread_reply port as the bootstrap port.  This is
	 * claimed by the kern_loader before we startup, so there's no
	 * race condition.
	 */
	r = thread_get_special_port((thread_t)thread_self(), THREAD_REPLY_PORT,
		&boot_listener_port);
	if (r != KERN_SUCCESS || boot_listener_port == PORT_NULL) {
		printf("k_server: can't find listener port..terminating\n");
		thread_terminate(current_thread());
		thread_halt_self();
	}
	ksp->boot_listener_port = boot_listener_port;

	/*
	 * Allocate a new reply port.
	 */
	r = port_allocate((task_t)task_self(), &reply_port);
	if (r != KERN_SUCCESS) {
		printf("k_server: can't allocate reply port..terminating\n");
		thread_terminate(current_thread());
		thread_halt_self();
	}
	
	r = thread_set_special_port((thread_t)thread_self(), THREAD_REPLY_PORT,
		reply_port);
	if (r != KERN_SUCCESS) {
		printf("k_server: can't set reply port..terminating\n");
		thread_terminate(current_thread());
		thread_halt_self();
	}

	/*
	 * Forget the reply_port we were using so that it isn't
	 * cached anymore.
	 */
	current_thread()->reply_port = PORT_NULL;

	/*
	 * Allocate the basic set of ports we need for communication
	 * (notify port).
	 */
	r = port_set_allocate((task_t)task_self(), &port_set);
	if (r != KERN_SUCCESS) {
		printf("k_server: can't allocate port set..terminating\n");
		thread_terminate(current_thread());
		thread_halt_self();
	}
	ksp->port_set = port_set;

	/*
	 * Add boot listener port to the port set.
	 */
	if (   port_set_add((task_t)task_self(), port_set, boot_listener_port)
	    != KERN_SUCCESS)
	{
		printf("k_server: can't add listener port\n");
		thread_terminate(current_thread());
		thread_halt_self();
	}

	/*
	 * Allocate and set up the notify port.
	 */
	r = port_allocate((task_t)ksp->task_port, &ksp->notify_port);
	if (r == KERN_SUCCESS)
		(void) port_set_add((task_t)ksp->task_port, ksp->port_set,
			ksp->notify_port);
	else
		kern_serv_panic(ksp->bootstrap_port,
			"k_server: can't get notify port");

	if (current_task() != kernel_task) {
		/*
		 * Set this port as our task's notify port.
		 */
		(void) task_set_special_port((task_t)task_self(),
			TASK_NOTIFY_PORT, ksp->notify_port);
	}

	kern_serv_notify(&ksp, ksp->notify_port, ksp->bootstrap_port);

	/*
	 * The kernel's task port.
	 */
	object_copyout(current_task(), kernel_task->task_tself,
		MSG_TYPE_PORT, &kernel_port);
	obj_reference((kern_obj_t)kernel_task->task_tself);

	/*
	 *	Service loop... receive messages and process them.
	 */

	in_msg = (msg_header_t *)kalloc(KERN_SERV_INMSG_SIZE);
	ksp->msg = in_msg;
	ksp->msg_size = KERN_SERV_INMSG_SIZE;
	for (;;) {
		/*
		 * Look for things to do that need to be done
		 * from this task/thread.
		 * Send messages.
		 */
		s = splhigh();
		simple_lock(&ksp->slock);

		while (!queue_empty(&ksp->msg_callout_q)) {
			struct msg_send_entry *msep;

			queue_remove_first(&ksp->msg_callout_q,
				msep, struct msg_send_entry *, link);

			simple_unlock(&ksp->slock);
			splx(s);

			ks_log(("kern_server_main: qed callout func 0x%x\n",
				msep->func));
			(*msep->func)(msep->arg);

			s = splhigh();
			simple_lock(&ksp->slock);

			queue_enter(&ksp->msg_callout_fq, msep,
				struct msg_send_entry *, link);
		}

		simple_unlock(&ksp->slock);
		splx(s);

		/*
		 * Wait for the next request.
		 */
	    receive:
		in_msg->msg_local_port = port_set;
		in_msg->msg_size = ksp->msg_size;
		r = msg_receive(in_msg, RCV_TIMEOUT|RCV_INTERRUPT|RCV_LARGE,
				1000);

		/*
		 * If our receive was interrupted, go back to it.
		 */
		switch (r) {
		case RCV_INTERRUPTED:
			if (thread_should_halt(current_thread()))
				thread_halt_self();
			break;
		case RCV_TIMED_OUT:
			continue;
		case RCV_TOO_LARGE:
			/*
			 * We need to allocate a larger message to return.
			 */
			kfree(ksp->msg, ksp->msg_size);
			ksp->msg_size = ksp->msg->msg_size;
			ksp->msg = (void *)kalloc(ksp->msg_size);
			in_msg = ksp->msg;
			goto receive;

		case KERN_SUCCESS:
			break;
		default:
			kern_serv_panic(bootstrap_port,
				"kern_server_main: received return bad return"
				" from msg_receive");
			break;
		}

		ks_log(("kern_server_main: received msgid %d "
			"on port %d\n", in_msg->msg_id,
			in_msg->msg_local_port));

		/*
		 * Dispatch the message based on it's port.
		 */
		if (in_msg->msg_local_port == ksp->notify_port) {
			notification_t	*n = (notification_t *) in_msg;
			switch(in_msg->msg_id) {
			case NOTIFY_PORT_DELETED:
				/*
				 * Give loaded code a chance to handle it
				 */
				if (ksp->pd_proc) {
					if ((*ksp->pd_proc)(n->notify_port))
						break;
				} else if (ksp->pn_proc)
					(*ksp->pn_proc)(n->notify_port,
						in_msg->msg_id);
				kern_serv_port_gone(&ksp, n->notify_port);
				break;
			default:
				if (ksp->pn_proc == 0)
					(*ksp->pn_proc)(n->notify_port,
				    			in_msg->msg_id);
				break;
			}
			continue;
		} else if (   in_msg->msg_id >= NOTIFY_FIRST
			   && in_msg->msg_id < NOTIFY_LAST)
		{
			notification_t	*n = (notification_t *) in_msg;
			ks_notify_t *np;

			/*
			 * If we have a notify request on this port, forward
			 * the message to the requested port.
			 */
			for (  np = (ks_notify_t *)queue_first(&ksp->notify_q)
			     ; !queue_end(&ksp->notify_q, (queue_entry_t)np)
			     ; np = (ks_notify_t *)queue_next(&np->link))
				if (np->req_port == n->notify_port) {
					in_msg->msg_remote_port =
						np->reply_port;
					ks_log2(("kern_server_main: "
						 "forwarding notification "
						 "request to port %d\n",
						 np->reply_port));
					msg_send(in_msg, MSG_OPTION_NONE, 0);
					queue_remove(&ksp->notify_q, np,
						ks_notify_t *, link);
					kfree(np, sizeof(*np));
					continue;
				}
		}

		ksp->local_port = in_msg->msg_local_port;

		error = kern_serv_dispatch(in_msg, ksp);
		ks_log2(("kern_server_main: return from ks_disp %d\n",
			error));

		if (   error == MIG_BAD_ID
		    && in_msg->msg_local_port == boot_listener_port)
		{
			error = kern_serv_handler(in_msg, &kern_serv);
			ks_log2(("kern_server_main: "
				"return from ks_server %d\n",
				error));
		}
	}
}

typedef struct {
	msg_header_t Head;
	msg_type_t RetCodeType;
	kern_return_t RetCode;
} Reply;

static kern_return_t kern_serv_port_proc_call (
	msg_header_t *in_msg,
	port_proc_map_t *pp)
{
	Reply *out_msg;
	kern_return_t ret_code;
	port_t local_port;

	if (pp->type == PP_handler)
		return (*pp->proc)(in_msg, pp->uarg);

	out_msg = (Reply *)kalloc(MSG_SIZE_MAX);
	local_port = in_msg->msg_local_port;
	in_msg->msg_local_port = (port_t)pp->uarg;

	(*((port_map_serv_t)pp->proc))(in_msg, (msg_header_t *)out_msg);
	ret_code = out_msg->RetCode;

	if (out_msg->RetCode == MIG_NO_REPLY)
		ret_code = KERN_SUCCESS;
	else
		ret_code = msg_send(&out_msg->Head, MSG_OPTION_NONE, 0);

	kfree(out_msg, MSG_SIZE_MAX);

	return ret_code;
}

/*
 * Call a proc indirectly based on the port.
 */
static kern_return_t kern_serv_dispatch (
	msg_header_t *in_msg,
	kern_server_t ksp)
{
	int i = ksp->last_rec_index;
	kern_return_t r = RCV_IN_PROGRESS;

	if (!ksp)
		return MIG_BAD_ID;

	if (in_msg->msg_local_port == ksp->last_unrec_port) {
		ks_log2(("kern_serv_dispatch: port %d was unrec\n",
			in_msg->msg_local_port));
		r = MIG_BAD_ID;
	} else if (in_msg->msg_local_port == ksp->last_rec_port) {
		ks_log2(("kern_serv_dispatch: port %d was rec(%d), "
			"proc 0x%x(%d)\n",
			in_msg->msg_local_port, i, ksp->port_proc[i].proc,
			ksp->port_proc[i].uarg));
		r = kern_serv_port_proc_call(in_msg, &ksp->port_proc[i]);
	} else {
		for (   i = 0
		     ;    i < KERN_SERVER_NPORTPROC
		       && ksp->port_proc[i].port != in_msg->msg_local_port
		     ; i++)
			;

		if (i != KERN_SERVER_NPORTPROC) {
			ksp->last_rec_port = in_msg->msg_local_port;
			ksp->last_rec_index = i;
			ks_log2(("kern_serv_dispatch: port %d now rec(%d), "
				"proc 0x%x(%d)\n",
				in_msg->msg_local_port, i,
				ksp->port_proc[i].proc,
				ksp->port_proc[i].uarg));
			r = kern_serv_port_proc_call(in_msg,
				&ksp->port_proc[i]);
		}
	}

	if (r == RCV_IN_PROGRESS) {
		ks_log2(("kern_serv_dispatch: port %d now unrec\n",
			in_msg->msg_local_port));
		ksp->last_unrec_port = in_msg->msg_local_port;
		r = MIG_BAD_ID;
	}

	return r;
}

/*
 * Got notification that port we have send rights on has gone away, clean up.
 */
void kern_serv_port_gone (
	kern_server_t	*kspp,
	port_name_t	port)
{
	kern_server_t	ksp = *kspp;
	register int i;

	if (ksp == 0)
		return;

	if (ksp->last_rec_port == port)
		ksp->last_rec_port = PORT_NULL;

	/*
	 * If this is the log port, don't try to send anything to it.
	 */
	if (ksp->log_port && port == ksp->log_port)
		ksp->log_port == PORT_NULL;
		
	/*
	 * Free up port/proc pairs
	 */
	for (i = 0; i < KERN_SERVER_NPORTPROC; i++)
		if (ksp->port_proc[i].port == port) {
			ksp->port_proc[i].port = PORT_NULL;
			ksp->port_proc[i].proc = 0;
			break;
		}
	return;
}

kern_return_t kern_serv_instance_loc (
	void		*arg,
	vm_address_t	instance_loc)
{
	kern_server_t ksp = *((kern_server_t *)arg);
	kern_server_t *kspp = (kern_server_t *)instance_loc;

	ks_log(("kern_serv_instance_loc: 0x%x\n", instance_loc));
	*kspp = ksp;

	return KERN_SUCCESS;
}

/*
 * Specify user's version of system user compiled with.
 */
kern_return_t kern_serv_version (
	void		*arg,
	int		version)
{
	kern_server_t ksp = *((kern_server_t *)arg);

	if (version < KS_COMPAT)
		return KERN_SERVER_BAD_VERSION;
	ksp->version = version;

	return KERN_SUCCESS;
}

kern_return_t kern_serv_boot_port (	// how to talk to loader
	void		*arg,
	port_t		boot_port)
{
	kern_server_t ksp = *((kern_server_t *)arg);

	ksp->bootstrap_port = boot_port;
	return KERN_SUCCESS;
}

/*
 * Kernel version of kern_serv_notify, doesn't contact kern_loader, uses
 * internal port_request_notification facility.
 */
kern_return_t kern_serv_notify (
	kern_server_t	*kspp,
	port_t		reply_port,
	port_t		req_port)
{
	kern_server_t ksp;
	port_t	bootstrap_port;
	kern_return_t r;
	kern_port_t rp, np;

	ksp = *kspp;
	bootstrap_port = ksp->bootstrap_port;

	if (current_task() != kernel_task) {
		ks_notify_t *np;

		if (reply_port == ksp->notify_port)
			/*
			 * Notification will happen automatically.
			 */
			return KERN_SUCCESS;

		/*
		 * Make sure that an entry doesn't already exist for this
		 * pair.
		 */
		for (  np = (ks_notify_t *)queue_first(&ksp->notify_q)
		     ; !queue_end(&ksp->notify_q, (queue_entry_t)np)
		     ; np = (ks_notify_t *)queue_next(&np->link))
			if (   np->reply_port == reply_port
			    && np->req_port == req_port)
			    	return KERN_FAILURE;

		np = kalloc(sizeof *np);
		np->reply_port = reply_port;
		np->req_port = req_port;
		queue_enter(&ksp->notify_q, np, ks_notify_t *, link);

		/*
		 * Notification will be forwarded from kern_server.
		 */
		return KERN_SUCCESS;
	}

	r = get_kern_port(current_task(), req_port, &rp);
	if (r != KERN_SUCCESS)
		return r;
	r = get_kern_port(current_task(), reply_port, &np);
	if (r != KERN_SUCCESS)
		return r;
	port_request_notification(rp, np);
	return KERN_SUCCESS;
}

kern_return_t kern_serv_wire_range (	// wire the specified range or memory
	void		*arg,
	vm_address_t	addr,
	vm_size_t	size)
{
	kern_server_t ksp = *((kern_server_t *)arg);

	ks_log(("kern_serv_wire_range: addr 0x%x for 0x%x bytes\n",
		addr, size));
#if	DEBUG
	printf("kern_server: wiring 0x%x to 0x%x\n", trunc_page(addr),
		round_page(addr+size));
#endif	DEBUG
	return vm_map_pageable(kernel_task->map, trunc_page(addr),
		round_page(addr+size), FALSE);
}

kern_return_t kern_serv_unwire_range (	// unwire the specified range or memory
	void		*arg,
	vm_address_t	addr,
	vm_size_t	size)
{
	kern_server_t ksp = *((kern_server_t *)arg);

	ks_log(("kern_serv_unwire_range: addr 0x%x for 0x%x bytes\n",
		addr, size));
#if	DEBUG
	printf("kern_server: unwiring 0x%x to 0x%x\n", trunc_page(addr),
		round_page(addr+size));
#endif	DEBUG
	return vm_map_pageable(kernel_task->map, trunc_page(addr),
		round_page(addr+size), TRUE);
}

/*
 * Map messages coming on on the specified port to call the specified proc.
 * If this mapping can't be made FALSE is returned.
 * The port is added to the portset for this server.
 */
kern_return_t kern_serv_port_proc (	// map a message on port to proc/arg
	void		*arg,		// record into structure
	port_all_t	port,		// port to map (all rights passed)
	port_map_proc_t	proc,		// proc to call
	int		uarg)		// replace local_port with uarg
{
	register int i;
	kern_return_t r;
	kern_server_t ksp = *((kern_server_t *)arg);

	ks_log(("kern_serv_port_proc: port %d proc 0x%x uarg %d\n",
		port, proc, uarg));

	if (ksp->last_unrec_port == port)
		ksp->last_unrec_port = PORT_NULL;

	for (i = 0; i < KERN_SERVER_NPORTPROC; i++)
		if (ksp->port_proc[i].port == port)
			ksp->port_proc[i].port = PORT_NULL;

	for (i = 0; i < KERN_SERVER_NPORTPROC; i++)
		if (ksp->port_proc[i].port == PORT_NULL)
			break;

	r = KERN_RESOURCE_SHORTAGE;
	if (   i == KERN_SERVER_NPORTPROC
	    || (r = port_set_add((task_t)ksp->task_port, ksp->port_set, port))
		!= KERN_SUCCESS)
		return r;

	ksp->port_proc[i].port = port;
	ksp->port_proc[i].proc = proc;
	ksp->port_proc[i].uarg = (void *)uarg;
	ksp->port_proc[i].type = PP_handler;

	return KERN_SUCCESS;
}

/*
 * Map messages coming on on the specified port to call the specified proc.
 * If this mapping can't be made FALSE is returned.
 * The port is added to the portset for this server.
 */
kern_return_t kern_serv_port_serv (	// map a message on port to proc/arg
	void		*arg,		// record into structure
	port_all_t	port,		// port to map (all rights passed)
	port_map_proc_t	proc,		// proc to call
	int		uarg)		// replace local_port with uarg
{
	register int i;
	kern_return_t r;
	kern_server_t ksp = *((kern_server_t *)arg);

	ks_log(("kern_serv_port_serv: port %d proc 0x%x uarg %d\n",
		port, proc, uarg));

	if (ksp->last_unrec_port == port)
		ksp->last_unrec_port = PORT_NULL;

	for (i = 0; i < KERN_SERVER_NPORTPROC; i++)
		if (ksp->port_proc[i].port == port)
			ksp->port_proc[i].port = PORT_NULL;

	for (i = 0; i < KERN_SERVER_NPORTPROC; i++)
		if (ksp->port_proc[i].port == PORT_NULL)
			break;

	r = KERN_RESOURCE_SHORTAGE;
	if (   i == KERN_SERVER_NPORTPROC
	    || (r = port_set_add((task_t)ksp->task_port, ksp->port_set, port))
		!= KERN_SUCCESS)
		return r;

	ksp->port_proc[i].port = port;
	ksp->port_proc[i].proc = proc;
	ksp->port_proc[i].uarg = (void *)uarg;
	ksp->port_proc[i].type = PP_server;

	return KERN_SUCCESS;
}

kern_return_t kern_serv_port_death_proc ( // specify port death handler
	void			*arg,	// record into structure
	port_death_proc_t	proc)	// record into structure
{
	kern_server_t ksp = *((kern_server_t *)arg);

	ksp->pd_proc = proc;
	return KERN_SUCCESS;
}

kern_return_t kern_serv_call_proc (	// call procedure with argument
	void		*arg,		// record into structure
	call_proc_t	proc,		// record into structure
	int		uarg)		// arg to supply
{
	kern_server_t ksp = *((kern_server_t *)arg);

	ks_log(("kern_serv_call_proc: proc 0x%x uarg %d\n",
		proc, uarg));

	if (proc) {
		(*proc)(uarg);
		return KERN_SUCCESS;
	}
	return KERN_SERVER_ERROR;
}

kern_return_t kern_serv_shutdown (
	void		*arg)
{
	kern_server_t ksp = *((kern_server_t *)arg);
	int i;

	ks_log(("kern_serv_shutdown\n"));

	/*
	 * Stop logging and free the data
	 * if we were logging.
	 */
	if (ksp->log.level) {
		kern_serv_log_free(&ksp->log);
		ksp->log.level = 0;
	}

	/*
	 * Deallocate all the port mappings we have.
	 */
	for (i = 0; i < KERN_SERVER_NPORTPROC; i++)
		if (ksp->port_proc[i].port != PORT_NULL) {
			port_deallocate((task_t)ksp->task_port,
				ksp->port_proc[i].port);
			ksp->port_proc[i].port = PORT_NULL;
			ksp->port_proc[i].proc = 0;
		}

	/*
	 * Deallocate our boot_listener and notify ports.
	 */
	port_deallocate((task_t)ksp->task_port, ksp->boot_listener_port);
	port_deallocate((task_t)ksp->task_port, ksp->notify_port);

	/*
	 * Deallocate our port_set
	 */
	port_set_deallocate((task_t)ksp->task_port, ksp->port_set);

	/*
	 * Deallocate our message frame.
	 */
	kfree(ksp->msg, ksp->msg_size);
	kfree(ksp, sizeof *ksp);
	thread_terminate(current_thread());
	while (1)
		/*
		 * Wait to be killed.
		 */
		thread_halt_self();

	return KERN_SUCCESS;
}

// log level from message on port
kern_return_t kern_serv_log_level (
	void		*arg,
	int		log_level)
{
	kern_server_t ksp = *((kern_server_t *)arg);
	int old_level = ksp->log.level;

	ksp->log.level = log_level;

	/*
	 * Only keep space around for the log
	 * if we need it.
	 */
	if (old_level == 0 && ksp->log.level)
		kern_serv_log_init(&ksp->log,
			KERN_SERVER_LOG_SIZE);
	else if (ksp->log.level == 0 && old_level)
		kern_serv_log_free(&ksp->log);

	return KERN_SUCCESS;
}

kern_return_t kern_serv_get_log (
	void		*arg,
	port_t		reply_port)	// port to send log information to
{
	int s, n;
	kern_server_t ksp = *((kern_server_t *)arg);

	if (ksp->log.level == 0) {
		port_deallocate((task_t)ksp->task_port, reply_port);
		return KERN_SERVER_NOTLOGGING;
	}

	if (ksp->log.ptr == ksp->log.base) {
		ksp->log_port = reply_port;
		return KERN_SUCCESS;
	} else {
		vm_size_t size =
		    round_page((char *)ksp->log.ptr - (char *)ksp->log.base);
		pointer_t ldata;
		unsigned int ldata_count;
		kern_return_t r;

		ASSERT(    (void *)ksp->log.base
			== (void *)round_page(ksp->log.base));
		r = vm_read((vm_task_t)kernel_port,
			(vm_address_t)ksp->log.base, size, &ldata,
			&ldata_count);
		ASSERT(r == KERN_SUCCESS);
		kern_serv_log_data(reply_port,
			(log_entry_t *)ldata,
			ksp->log.ptr - ksp->log.base);

		port_deallocate((task_t)ksp->task_port, reply_port);
		vm_deallocate((vm_task_t)ksp->task_port, ldata, size);
	}

	s = splhigh();
	simple_lock(&ksp->slock);
	ksp->log.ptr = ksp->log.base;
	simple_unlock(&ksp->slock);
	splx(s);

	return KERN_SUCCESS;
}

void kern_serv_log (			// log a message
	kern_server_t	*kspp,		// kern_server instance vars
	int		log_level,	// level to log at
	char		*msg,		// what to log (followed by args)
	int		arg1,
	int		arg2,
	int		arg3,
	int		arg4,
	int		arg5)
{
	kern_server_t ksp = *kspp;
	register int s;
	log_entry_t *x;

	if (log_level > ksp->log.level || ksp->log.base == 0)
		return;

	s = splhigh();
	simple_lock(&ksp->slock);

	x = ksp->log.ptr++;
	if (ksp->log.ptr == ksp->log.last) {
		ksp->log.ptr--;
		simple_unlock(&ksp->slock);
		splx(s);
		return;
	}
	simple_unlock(&ksp->slock);
	splx(s);

	x->msg = msg;
	x->arg1 = arg1;
	x->arg2 = arg2;
	x->arg3 = arg3;
	x->arg4 = arg4;
	x->arg5 = arg5;
	x->timestamp = XPR_TIMESTAMP;
	x->cpuinfo = log_level;

	if (ksp->log_port)
		kern_serv_callout(kspp, kern_serv_send_log, (void *)ksp);
}

/*
 * Send the log back to the user.  Have to copy the log into our address
 * space.
 */
static void kern_serv_send_log(void *arg)
{
	kern_server_t ksp = (kern_server_t)arg;
	port_name_t log_port;
	int s;
	vm_size_t size =
		round_page((char *)ksp->log.ptr - (char *)ksp->log.base);
	pointer_t ldata;
	unsigned int ldata_count;
	kern_return_t r;

	s = splhigh();
	simple_lock(&ksp->slock);

	log_port = ksp->log_port;
	ksp->log_port = PORT_NULL;

	simple_unlock(&ksp->slock);
	splx(s);
	if (log_port == PORT_NULL)
		return;

	ASSERT((void *)ksp->log.base == (void *)round_page(ksp->log.base));
	r = vm_read((vm_task_t)kernel_port, (vm_address_t)ksp->log.base,
		size, &ldata, &ldata_count);
	ASSERT(r == KERN_SUCCESS);
	kern_serv_log_data(log_port, (log_entry_t *)ldata,
		ksp->log.ptr - ksp->log.base);

	port_deallocate((task_t)ksp->task_port, log_port);
	vm_deallocate((vm_task_t)ksp->task_port, ldata, size);

	s = splhigh();
	simple_lock(&ksp->slock);

	ksp->log.ptr = ksp->log.base;

	simple_unlock(&ksp->slock);
	splx(s);

}

/*
 * Ensure that the number of entries is rounded up to a page boundary.
 */
static void kern_serv_log_init (	// initialize server's log buffer
	log_t	*log,			// uninitialized log struct
	int	num_entries)		// size of log
{
	vm_size_t size = round_page(sizeof(log_entry_t)*num_entries);
	num_entries = size / sizeof(log_entry_t);

	log->base = (log_entry_t *)kalloc(size);

	log->last = log->base + num_entries;
	log->ptr = log->base;
	printf("kern_serv_log_init: log 0x%x log.last 0x%x, log.base 0x%x\n",
		log, log->last, log->base);
}

static void kern_serv_log_free (log_t *log) // deallocate log structure
{
	kfree(log->base, round_page((char *)log->last - (char *)log->base));
	log->base = log->last = log->ptr = 0;
}

kern_return_t kern_serv_callout (
	kern_server_t	*kspp,
	void		(*proc)(void *arg),
	void *		arg)
{
	kern_server_t ksp = *kspp;
	struct msg_send_entry *msep;
	int s;

	/*
	 * Just call this guy directly if the conditions are right.
	 */
	if (curipl() == 0 && task_self() == ksp->task_port) {
		(*proc)(arg);
		return KERN_SUCCESS;
	}

	s = splhigh();
	simple_lock(&ksp->slock);

	if (queue_empty(&ksp->msg_callout_fq)) {
#ifdef	DEBUG
		printf("kern_serv_callout: no free messages");
#endif	DEBUG
		simple_unlock(&ksp->slock);
		splx(s);
		return KERN_RESOURCE_SHORTAGE;
	}

	queue_remove_first(&ksp->msg_callout_fq, msep, struct msg_send_entry *,
		link);
	ks_log(("kern_serv_send: (queue) proc 0x%x\n", proc));

	msep->func = proc;
	msep->arg = arg;
	queue_enter(&ksp->msg_callout_q, msep, struct msg_send_entry *, link);

	simple_unlock(&ksp->slock);
	splx(s);

	/*
	 * Interrupt the server thread to that it'll look at this sucker.
	 */
	softint_sched(CALLOUT_PRI_THREAD,
		(int (*)())kern_serv_interrupt_server,
		(caddr_t)ksp->server_thread);

	return KERN_SUCCESS;
}

static void kern_serv_interrupt_server(thread_t thread)
{
	int s;

	thread_dowait(thread, TRUE);	// wait for the guy to stop.
	clear_wait(thread, THREAD_INTERRUPTED, TRUE);
}

/*
 * Structure access functions.
 */
port_t kern_serv_local_port(kern_server_t *ksp)
{
	return (*ksp)->local_port;
}

port_t kern_serv_bootstrap_port(kern_server_t *ksp)
{
	return (*ksp)->bootstrap_port;
}

port_t kern_serv_notify_port(kern_server_t *ksp)
{
	return (*ksp)->notify_port;
}

port_set_name_t kern_serv_port_set(kern_server_t *ksp)
{
	return (*ksp)->port_set;
}
