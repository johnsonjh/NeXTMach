/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 24-Sep-90  Gregg Kellogg (gk) at NeXT, Inc.
 *	If errors occur while trying to shut down a server thread
 * 	terminate the thread directly.
 *
 *  7-Jul-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	Made path name of relocatable relative to the directory of the
 *	script file if it's not absolute.
 *
 * 25-Jun-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	Made it so that if the relocated file (in /tmp) disappears
 *	that it will be properly relocated.
 *
 * 18-Apr-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	Created.
 */ 

#import "server.h"
#import "obj.h"
#import "log.h"
#import "load_cmds.h"
#import "misc.h"
#import <ctype.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <kernserv/kern_server.h>
#import <kernserv/kern_loader_types.h>
#import <mach_error.h>
#import <sys/mig_errors.h>
#import <fcntl.h>
#import <libc.h>
#import <sys/stat.h>
#import <servers/netname.h>
#import <sys/thread_switch.h>

#import <machine/psl.h>

queue_head_t server_queue;
mutex_t server_queue_lock;
#define queue_lock()		mutex_lock(server_queue_lock)
#define queue_unlock()		mutex_unlock(server_queue_lock)

extern char *called_as;
extern port_set_name_t port_set;

static boolean_t server_locate_cmds(server_t *server, queue_t cmd_queue);
static void server_init_deallocate(server_t *server, const char *why);
static server_port_type_t server_port_type (
	server_t	*server,
	port_name_t	port_name);

/*
 * Script file parsing.
 */
/*
 * Initialize the server stuff.
 */
void server_queue_init(void)
{
	/*
	 * First time initialization.
	 */
	server_queue_lock = mutex_alloc();
	mutex_init(server_queue_lock);
	queue_init(&server_queue);

}

/*
 * Read a script file describing the load/unload instructions for a
 * given kernel server and advertise the exported ports with the
 * netname server (nmserver).  The exported ports are added to the
 * port_set.
 */
kern_return_t server_init(const char *reloc, server_t **serverp)
{
	server_t	*server;
	int		r;
	vm_address_t	instance;

	server = (server_t *)malloc(sizeof *server);
	/*
	 * Allocate the server.
	 */
	queue_init(&server->load_cmds);
	queue_init(&server->unload_cmds);
	queue_init(&server->notifications);
	server->state = Allocating;
	server->wire = FALSE;
	server->load = FALSE;
	server->please_die = FALSE;
	server->alloc_in_self = FALSE;
	server->alloc_in_exec = FALSE;
	server->server_port = PORT_NULL;
	server->boot_port = PORT_NULL;
	server->log_reply_port = PORT_NULL;
	server->reloc = reloc;
	server->version = 0;
	server->name = strrchr(server->reloc, '/');
	if (!server->name)
		server->name = server->reloc;
	else
		server->name++;
	server->symbols = malloc(sizeof(NXAtom));
	*server->symbols = NULL;
	server->nsymbols = 0;
	server->loadable = NULL;
	server->header = NULL;
	server->vm_addr = (vm_offset_t)NULL;
	server->header = NULL;
	server->executable_task = PORT_NULL;
	server->lock = mutex_alloc();
	server->server_thread = PORT_NULL;
	mutex_init(server->lock);
	server->state_changed = condition_alloc();
	condition_init(server->state_changed);

	server_lock(server);

	queue_lock();
	queue_enter(&server_queue, server, server_t *, link);
	queue_unlock();

	r = lc_init(reloc, server);
	if (r != KERN_SUCCESS) {
		server_init_deallocate(server,
			"Server %s didn't initialize\n");
		return r;
	}

	if (server->please_die) {
		server_init_deallocate(server,
			"Server %s deallocation requested\n");
		return KERN_LOADER_SERVER_DELETED;
	}

	/*
	 * Find addresses and values for functions and arguments that
	 * will be used for loading/unloading the server.
	 */
	if (!server_locate_cmds(server, &server->load_cmds)) {
		server_init_deallocate(server, NULL);
		return KERN_LOADER_SERVER_WONT_LINK;
	}

	if (server->please_die) {
		server_init_deallocate(server,
			"Server %s deallocation requested\n");
		return KERN_LOADER_SERVER_DELETED;
	}

	if (!server_locate_cmds(server, &server->unload_cmds)) {
		server_init_deallocate(server, NULL);
		return KERN_LOADER_SERVER_WONT_LINK;
	}

	if (server->please_die) {
		server_init_deallocate(server,
			"Server %s deallocation requested\n");
		return KERN_LOADER_SERVER_DELETED;
	}

	/*
	 * Locate the server instance variable.
	 */
	r = sym_value(server->name, server->instance, &instance);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "Server %s has no instance variable \"%s\"\n",
			server->name, server->instance);
		server_init_deallocate(server, NULL);
		return KERN_LOADER_SERVER_WONT_LINK;
	}
	server->instance = (char *)instance;

	if (server->please_die) {
		server_init_deallocate(server,
			"Server %s deallocation requested\n");
		return KERN_LOADER_SERVER_DELETED;
	}

	server_lock(server);
	server->state = Allocated;
	condition_broadcast(server->state_changed);

	kllog(LOG_INFO, "Server %s Allocated\n", server->name);

	/*
	 * Load the kernel server into the kernel now, otherwise, wait
	 * for a message to be sent to one of it's advertised ports.
	 */
	if (server->load) {
		r = server_load(server);
		if (r != KERN_SUCCESS) {
			kllog(LOG_ERR, "Server %s won't load\n", server->name);
			server_deallocate(server);
			server_unlock(server);
			return r;
		}
	}
	server_unlock(server);

	if (serverp)
		*serverp = server;
	return KERN_SUCCESS;
}

static void server_init_deallocate(server_t *server, const char *why)
{
	if (why)
		kllog(LOG_INFO, why, server->name);
	server_lock(server);
	server->state = Allocated;
	condition_broadcast(server->state_changed);
	server_deallocate(server);
	server_unlock(server);
}

static boolean_t server_locate_cmds(server_t *server, queue_t cmd_queue)
{
	server_command_t *scp;
	vm_address_t proc, arg;
	kern_return_t r;

	for (  scp = (server_command_t *)queue_first(cmd_queue)
	     ; !queue_end(cmd_queue, (queue_entry_t)scp)
	     ; scp = (server_command_t *)queue_next(&scp->link))
	{
		switch (scp->msg_type) {
		case S_C_DEATH:
			/*
			 * Get the proc to call for messages on the port.
			 */
			r = sym_value(server->name, scp->function, &proc);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"Server %s can't find symbol for "
					"port death proc named \"%s\"\n",
					server->name, scp->function);
				return FALSE;
			}

			scp->funaddr = (vm_address_t)proc;
			break;

		case S_C_HMAP: case S_C_SMAP:
			/*
			 * Get the proc to call for messages on the port.
			 */
			r = sym_value(server->name, scp->function, &proc);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"Server %s can't find symbol for "
					"map proc named \"%s\"\n",
					server->name, scp->function);
				return FALSE;
			}

			/*
			 * Get the arg to call for messages on the port.
			 */
			if (isdigit(*scp->arg))
				arg = strtol(scp->arg, NULL, 0);
			else {
			    r = sym_value(server->name, scp->arg, &arg);
			    if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"Server %s can't find symbol for "
					"argument named \"%s\"\n",
					server->name, scp->function);
				    return FALSE;
			    }
			}

			scp->funaddr = (vm_address_t)proc;
			scp->argval = (int)arg;
			break;

		case S_C_CALL:
			/*
			 * Get the proc to call for messages on the port.
			 */
			r = sym_value(server->name, scp->function, &proc);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"Server %s can't find symbol for "
					"call proc named \"%s\"\n",
					server->name, scp->function);
				return FALSE;
			}

			if (isdigit(*scp->arg))
				arg = strtol(scp->arg, NULL, 0);
			else {
			    r = sym_value(server->name, scp->arg, &arg);
			    if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"Server %s can't find symbol for "
					"argument named \"%s\"\n",
					server->name, scp->function);
				    return FALSE;
			    }
			}

			scp->funaddr = (vm_address_t)proc;
			scp->argval = (int)arg;
			break;
		default:
			kllog(LOG_ERR, "Server %s has unknown "
				"startup msg_type\n", server->name);
			return FALSE;
		}
	}

	return TRUE;
}

/*
 * Return the type of port used by this server (if any).
 */
static server_port_type_t server_port_type (
	server_t	*server,
	port_name_t	port_name)
{
	server_command_t	*scp;
	server_notify_t		*snp;

	queue_unlock();

	/*
	 * Is message from kernel server itself?
	 */
	if (port_name == server->boot_port)
		return BootPort;

	if (port_name == server->server_port)
		return ServerPort;

	if (port_name == server->log_reply_port)
		return LogReplyPort;

	if (   server->state == Allocating
	    || server->state == Deallocated)
		return UnknownPort;

	server_lock(server);

	/*
	    * See if this is for a requeted notifcation port.
	    */
	snp = (server_notify_t *)queue_first(&server->notifications);
	
	for (; !queue_end(&server->notifications, (queue_entry_t)snp)
		; snp = (server_notify_t *)queue_next(&snp->link))
	{
		if (snp->req_port == port_name) {
			/*
			    * This is the port.
			    */
			server_unlock(server);
			return NotificationRequest;
		}
		if (snp->reply_port == port_name) {
			/*
			    * The notification request port went away!
			    */
			queue_remove(&server->notifications,
				snp, server_notify_t *, link);
			free(snp);
		}
	}

	/*
	 * This might be the port associated with a port mapping,
	 * used for auto-loading servers.
	 */
	scp = (server_command_t *)queue_first(&server->load_cmds);
	
	for (; !queue_end(&server->load_cmds, (queue_entry_t)scp)
		; scp = (server_command_t *)queue_next(&scp->link))
	{
		/*
		 * Is message for an unloaded server?
		 */
		if (   scp->msg_type == S_C_HMAP
		    && scp->port == port_name)
		{
			server_unlock(server);
			return MappedPort;
		}
		if (   scp->msg_type == S_C_SMAP
		    && scp->port == port_name)
		{
			server_unlock(server);
			return MappedPort;
		}
	}
	server_unlock(server);
	
	return UnknownPort;
}

/*
 * Given a port_name, find the server structure representing this port.
 */
server_t *server_by_port(port_name_t port_name)
{
	server_t *server;

	queue_lock();
	for (  server = (server_t *)queue_first(&server_queue)
	     ; !queue_end(&server_queue, (queue_entry_t)server)
	     ; server = (server_t *)queue_next(&server->link))
	{
		switch (server_port_type(server, port_name)) {
		case BootPort:
		case ServerPort:
		case MappedPort:
		case LogReplyPort:
			return server;
		default:
			break;
		}
		queue_lock();
	}
	queue_unlock();

	return NULL;
}

/*
 * Return the server structure of the first server using this port as
 * a notification request port.
 */
server_t *server_by_notify_req(port_name_t port_name)
{
	server_t *server;

	queue_lock();
	for (  server = (server_t *)queue_first(&server_queue)
	     ; !queue_end(&server_queue, (queue_entry_t)server)
	     ; server = (server_t *)queue_next(&server->link))
	{
		if (server_port_type(server, port_name) == NotificationRequest)
			return server;
		queue_lock();
	}
	queue_unlock();

	return NULL;
}

/*
 * Given a port name and a server, find the server_command describing it.
 */
server_command_t *server_command_by_port_name(
	server_t	*server,
	const char	*name)
{
	server_command_t *scp;

	for (  scp = (server_command_t *)queue_first(&server->load_cmds)
	     ; !queue_end(&server->load_cmds, (queue_entry_t)scp)
	     ; scp = (server_command_t *)queue_next(&scp->link))
	{
		/*
		 * Is message for an unloaded server?
		 */
		if (scp->msg_type == S_C_HMAP && scp->port_name == name) {
			return scp;
		}
		if (scp->msg_type == S_C_SMAP && scp->port_name == name) {
			return scp;
		}
	}

	return NULL;
}

/*
 * Given a server name, return the server structure representing the server.
 */
server_t *server_by_name(const char *name)
{
	server_t *server;

	queue_lock();
	for (  server = (server_t *)queue_first(&server_queue)
	     ; !queue_end(&server_queue, (queue_entry_t)server)
	     ; server = (server_t *)queue_next(&server->link))
	{
		queue_unlock();

		if (!strcmp(server->name, name))
			return server;
		queue_lock();
	}
	queue_unlock();

	return NULL;
}

/*
 * Return the notification port for this notification request.  This
 * servers to remove this mapping from the server as well.
 */
port_t server_notify(server_t *server, port_t req_port)
{
	server_notify_t *snp;
	port_t reply_port = PORT_NULL;

	server_lock(server);
	/*
	 * See if this is for a requeted notifcation port.
	 */
	snp = (server_notify_t *)queue_first(&server->notifications);
	
	for (; !queue_end(&server->notifications, (queue_entry_t)snp)
		; snp = (server_notify_t *)queue_next(&snp->link))
	{
		if (snp->req_port == req_port) {
			/*
			 * This is the port.
			 */
			reply_port = snp->reply_port;
			queue_remove(&server->notifications,
				snp, server_notify_t *, link);
			break;
		}
	}
	server_unlock(server);

	return reply_port;
}

/*
 * Record a notification request for this server.
 */
void server_req_notify(server_t *server, port_t req_port, port_t reply_port)
{
	server_notify_t *snp = malloc(sizeof(*snp));

	server_lock(server);
	snp->req_port = req_port;
	snp->reply_port = reply_port;
	queue_enter(&server->notifications, snp, server_notify_t *, link);
	server_unlock(server);
}

/*
 * Load the server associated with the given server structure into the kernel.
 * Once the server is loaded initialize it.
 */
kern_return_t server_load(server_t *server)
{
	kern_return_t			r;
	u_int				reloc_date;
	unsigned int			old_stateCnt;
	struct NeXT_thread_state_regs	regs;
	server_command_t		*scp;
	port_t				reply_port;

	while (temp_server_state(server->state))
		condition_wait(server->state_changed, server->lock);

	switch (server->state) {
	case Allocated:
		break;
	case Loaded:
		return KERN_SUCCESS;
	case Deallocated:
		kllog(LOG_WARNING, "Server %s deallocated, can't load\n",
			server->name);
		return KERN_LOADER_SERVER_DELETED;
	default:
		kllog(LOG_WARNING, "Server %s in odd state %s\n",
			server->name, server_state_string(server->state));
		return KERN_LOADER_SERVER_WONT_LOAD;
	}

	if (!server->alloc_in_exec) {
		kllog(LOG_ERR, "Server %s's executable memory not allocated\n",
			server->name);
		return KERN_LOADER_MEM_ALLOC_PROBLEM;
	}

	if (   !obj_date(server->reloc, &reloc_date)
	    || reloc_date != server->reloc_date)
	{
		const char *reloc = server->reloc;
		kllog(LOG_NOTICE, "Server %s's relocatable changed, "
			"re-initializing\n", server->name);
		server_deallocate(server);
		server_unlock(server);
		r = server_init(reloc, &server);
		if (r != KERN_SUCCESS)
			return r;
		server_lock(server);
	}

 	kllog(LOG_NOTICE, "Server %s loading\n", server->name);

	/*
	 * Get the server Loaded and running.
	 */
	r = vm_write(server->executable_task, server->vm_addr,
		server->vm_addr, server->vm_size);
	if (r != KERN_SUCCESS) {
		return r;
	}

	server->state = Loading;
	condition_broadcast(server->state_changed);

	/*
	 * Create the task as a child of the kernel task.
	 */
	r = task_create(server->executable_task, FALSE,
		&server->server_task);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "task_create: %s(%d)\n",
			mach_error_string(r), r);
		return r;
	}

	r = thread_create(server->server_task, &server->server_thread);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "thread_create: %s(%d)\n",
			mach_error_string(r), r);
		server->state = Loaded;
		return r;
	}

	/*
	 * Get the port that this guy's listening to.
	 */
	r = thread_get_special_port(server->server_thread,
		THREAD_REPLY_PORT, &server->server_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "thread_get_reply_port: %s(%d)\n",
			mach_error_string(r), r);
		server->state = Loaded;
		return r;
	}

	old_stateCnt = sizeof(regs);
	r = thread_get_state(server->server_thread, NeXT_THREAD_STATE_REGS,
		(thread_state_t)&regs, &old_stateCnt);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "thread_get_state: %s(%d)\n",
				mach_error_string(r), r);
		server->state = Loaded;
		return r;
	}

	regs.pc = server->server_start;
	regs.sr = SR_SUPER;
	kllog(LOG_INFO, "regs.pc = %x\n", regs.pc);

	r = thread_set_state(server->server_thread, NeXT_THREAD_STATE_REGS,
		(thread_state_t)&regs, NeXT_THREAD_STATE_REGS_COUNT);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "thread_set_state: %s(%d)\n",
			mach_error_string(r), r);
		server->state = Loaded;
		return r;
	}

	r = thread_resume(server->server_thread);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "thread_resume: %s(%d)\n",
			mach_error_string(r), r);
		server->state = Loaded;
		return r;
	}

	kllog(LOG_INFO, "Server %s download complete\n", server->name);

	/*
	 * Wait for the kern_server to set a different reply port before
	 * we send any messages down on this one.
	 */
	do {
		/*
		 * Let the server run.
		 */
		thread_switch(server->server_thread, SWITCH_OPTION_NONE, 0);

		/*
		 * Get the port that this guy's listening to.
		 */
		r = thread_get_special_port(server->server_thread,
			THREAD_REPLY_PORT, &reply_port);
		if (r != KERN_SUCCESS) {
			kllog(LOG_ERR, "thread_get_reply_port: %s(%d)\n",
				mach_error_string(r), r);
			server->state = Loaded;
			return r;
		}
	} while (reply_port == server->server_port);

	kllog(LOG_INFO, "Server %s starting up\n", server->name);
	server_unlock(server);

	/*
	 * Send him the location of his instance variables first.
	 */
	r = kern_serv_instance_loc(server->server_port,
		(vm_address_t)server->instance);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "kern_serv_instance_loc: %s(%d)\n",
			kern_serv_error_string(r), r);
		server_lock(server);
		server->state = Loaded;
		condition_broadcast(server->state_changed);
		return r;
	}

	/*
	 * Send down server version.
	 */
	r = kern_serv_version(server->server_port, server->version);
	switch (r) {
	case KERN_SUCCESS:
		break;
	default:
		kllog(LOG_ERR, "kern_serv_version: %s(%d)\n",
			kern_serv_error_string(r), r);
		server_lock(server);
		server->state = Loaded;
		condition_broadcast(server->state_changed);
		return r;
	}

	/*
	 * Get the port he's going to talk to us on.
	 */
	r = port_allocate(task_self(), &server->boot_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "port_allocate (boot_port: %s(%d)\n",
			mach_error_string(r), r);
		server_lock(server);
		server->state = Loaded;
		condition_broadcast(server->state_changed);
		return r;
	}

	r = kern_serv_boot_port(server->server_port, server->boot_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "kern_serv_boot_port: %s(%d)\n",
			kern_serv_error_string(r), r);
		server_lock(server);
		server->state = Loaded;
		condition_broadcast(server->state_changed);
		return r;
	}

	r = port_set_add(task_self(), port_set, server->boot_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "port_set_add (boot_port): %s(%d)\n",
			mach_error_string(r), r);
		server_lock(server);
		server->state = Loaded;
		condition_broadcast(server->state_changed);
		return r;
	}

	/*
	 * First, get the sucker wired down.
	 */
	if (server->wire) {
		r = kern_serv_wire_range(server->server_port,
			server->vm_addr, server->vm_size);
		if (r != KERN_SUCCESS) {
			kllog(LOG_ERR, "kern_serv_wire_range: %s(%d)\n",
				kern_serv_error_string(r), r);
			server_lock(server);
			server->state = Loaded;
			condition_broadcast(server->state_changed);
			return r;
		}
	}

	/*
	 * Execute the command list, in order.
	 */
	for (  scp = (server_command_t *)queue_first(&server->load_cmds)
	     ; !queue_end(&server->load_cmds, (queue_entry_t)scp)
	     ; scp = (server_command_t *)queue_next(&scp->link))
	{
		const char *load_command = scp->function;
		switch (scp->msg_type) {
		case S_C_DEATH:
			/*
			 * Have the kernel server call proc when a port
			 * dies.
			 */
			r = kern_serv_port_death_proc(server->server_port,
				(port_death_proc_t)scp->funaddr);
			break;

		case S_C_HMAP:
			/*
			 * Initialize the kernel server to call proc(arg)
			 * when a message comes in on this port.
			 */
			r = kern_serv_port_proc(server->server_port, scp->port,
				(port_map_proc_t)scp->funaddr,
				(int)scp->argval);
			break;

		case S_C_SMAP:
			/*
			 * Initialize the kernel server to call proc(arg)
			 * when a message comes in on this port.
			 */
			r = kern_serv_port_serv(server->server_port, scp->port,
				(port_map_proc_t)scp->funaddr,
				(int)scp->argval);
			break;

		case S_C_CALL:
			/*
			 * Have the kernel server call proc(arg) now.
			 */
			r = kern_serv_call_proc(server->server_port,
				(call_proc_t)scp->funaddr, (int)scp->argval);
			break;
		default:
			load_command = "<unknown>";
			kllog(LOG_ERR, "Server %s has unknown startup "
				"msg_type\n", server->name);
			r = KERN_FAILURE;
			break;
		}

		if (r != KERN_SUCCESS) {
			kllog(LOG_ERR, "Server %s initialization error "
				"command \"%s\" "
				"%s(%d)\n", server->name, load_command,
				kern_serv_error_string(r), r);
			server_lock(server);
			server->state = Loaded;
			condition_broadcast(server->state_changed);
			return r;
		}
	}

	server_lock(server);
	server->state = Loaded;
	condition_broadcast(server->state_changed);
	kllog(LOG_INFO, "Server %s Loaded\n", server->name);

	return KERN_SUCCESS;
}

/*
 * Shutdown and unload the given server.
 */
kern_return_t server_unload(server_t *server)
{
	server_command_t *scp;
	kern_return_t r;
	boolean_t errors = FALSE;

	/*
	 * Special case for unloading all servers.
	 */
	if (server == NULL) {
		queue_lock();
		for (  server = (server_t *)queue_first(&server_queue)
		     ; !queue_empty(&server_queue)
		     ; server = (server_t *)queue_first(&server_queue))
		{
			queue_unlock();
			server_lock(server);
			(void) server_unload(server);
			(void) server_deallocate(server);
			server_delete(server);
			queue_lock();
		}
		queue_unlock();
		return KERN_SUCCESS;
	}

	while (server->state != Allocating && temp_server_state(server->state))
		condition_wait(server->state_changed, server->lock);

	if (server->state != Loaded)
		return KERN_LOADER_SERVER_UNLOADED;

	kllog(LOG_INFO, "Server %s unloading\n", server->name);

	server->state = Unloading;
	condition_broadcast(server->state_changed);
	server_unlock(server);

	/*
	 * Execute the command list, in order (if initialized).
	 */
	for (  scp = (server_command_t *)
			queue_first(&server->unload_cmds)
		; !queue_end(&server->unload_cmds, (queue_entry_t)scp)
		; scp = (server_command_t *)queue_next(&scp->link))
	{
		const char *unload_command = scp->function;
		switch (scp->msg_type) {
		case S_C_CALL:
			/*
			 * Have the kernel server call proc(arg) now.
			 */
			r = kern_serv_call_proc(server->server_port,
				(call_proc_t)scp->funaddr,
				(int)scp->argval);
			break;
		default:
			unload_command = "<unknown>";
			kllog(LOG_WARNING,
				"unknown shutdown msg_type\n");
			r = KERN_SUCCESS;
			break;
		}

		if (r != KERN_SUCCESS) {
			kllog(LOG_ERR,
				"Server %s unload command %s: %s(%d)\n",
				server->name, unload_command,
				kern_serv_error_string(r), r);
			errors = TRUE;
			break;
		}
	}

	/*
	 * Remove pending notifications.
	 */
	while (!queue_empty(&server->notifications)) {
		server_notify_t *snp;

		queue_remove_first(&server->notifications, snp,
			server_notify_t *, link);
		port_deallocate(task_self(), snp->reply_port);
		port_deallocate(task_self(), snp->req_port);
		free(snp);
	}

	/*
	 * Get the sucker unwired
	 */
	if (server->wire) {
		r = kern_serv_unwire_range(server->server_port,
			server->vm_addr, server->vm_size);
		if (r != KERN_SUCCESS) {
			kllog(LOG_ERR,
				"kern_serv_unwire_range: %s(%d)\n",
				kern_serv_error_string(r), r);
			errors = TRUE;
		}
	}

	/*
	 * Shut him down (up?).
	 */
	r = kern_serv_shutdown(server->server_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "kern_serv_shutdown: %s(%d)\n",
			kern_serv_error_string(r), r);
		errors = TRUE;
	}

	/*
	 * We'd better just get rid of this guy directly if there were errors
	 * in shutting him down.
	 */
	if (errors == TRUE)
		(void)thread_terminate(server->server_thread);

	/*
	 * Wait for the thread to go away.
	 */
	do
		r = thread_switch(server->server_thread, SWITCH_OPTION_NONE,0);
	while (r == KERN_SUCCESS);
	server->server_thread = PORT_NULL;

	server_lock(server);

	/*
	 * Get rid of a few other resources.
	 */
	port_deallocate(task_self(), server->server_port);
	server->server_port = PORT_NULL;
	port_deallocate(task_self(), server->log_reply_port);
	server->log_reply_port = PORT_NULL;

	/*
	 * Redo advertised ports.
	 */
	for (  scp = (server_command_t *)queue_first(&server->load_cmds)
	     ; !queue_end(&server->load_cmds, (queue_entry_t)scp)
	     ; scp = (server_command_t *)queue_next(&scp->link))
	{
		if (scp->msg_type != S_C_HMAP && scp->msg_type != S_C_SMAP)
			continue;
		if (scp->advertised)
			netname_check_out(name_server_port,
				(char *)scp->port_name, task_self());
		port_deallocate(task_self(), scp->port);
		r = port_allocate(task_self(), &scp->port);
		if (r != KERN_SUCCESS) {
			kllog(LOG_ERR, "port_allocate: %s(%d)\n",
				mach_error_string(r), r);
			errors = TRUE;
			break;
		}

		/*
		 * Add the port to our port set and advertise it with
		 * the netname server.
		 */
		r = port_set_add(task_self(), port_set, scp->port);
		if (r != KERN_SUCCESS) {
			kllog(LOG_ERR, "port_set_add: %s(%d)\n",
				mach_error_string(r), r);
			errors = TRUE;
			break;
		}
		
		if (scp->advertised) {
			r = netname_check_in(name_server_port,
						(char *)scp->port_name,
						task_self(), scp->port);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR, "netname_check_in: %s(%d)\n",
					mach_error_string(r), r);
				errors = TRUE;
				break;
			}
		}
	}

	if (server->server_task)
		/*
		 * Get rid of the task.
		 */
		task_terminate(server->server_task);

	server->server_task = 0;

	server->state = Allocated;
	condition_broadcast(server->state_changed);
	if (errors) {
		kllog(LOG_INFO, "Server %s didn't unload properly, "
			"deallocating\n",
			server->name);
		(void) server_deallocate(server);
		return KERN_LOADER_WONT_UNLOAD;
	}

	kllog(LOG_INFO, "Server %s re-Allocated\n", server->name);
	return KERN_SUCCESS;
}

kern_return_t server_deallocate(server_t *server)
{
	kern_return_t r;

	kllog(LOG_NOTICE, "Server %s deallocating\n", server->name);

    again:
	switch (server->state) {
	case Loaded:
	case Loading:
		(void) server_unload(server);
		break;
	case Allocating:
		/*
		 * Don't deallocate the thing directly, set the "please_die"
		 * bit and let it happen for itself.
		 */
		server->please_die = 1;
		condition_wait(server->state_changed, server->lock);
		goto again;
	case Allocated:
		break;
	case Deallocated:
	case Zombie:
		return KERN_LOADER_SERVER_DELETED;
	case Unloading:
		condition_wait(server->state_changed, server->lock);
		goto again;
	}

	delete_symfile(server->name);
	if (server->nsymbols >= 0) {
		free(server->symbols);
		server->nsymbols = -1;
	}

	if (server->alloc_in_exec) {
		r = vm_deallocate(server->executable_task, server->vm_addr,
			server->vm_size);
		if (r != KERN_SUCCESS)
			kllog(LOG_ERR, "vm_deallocate in exec failed: "
				"%s (%d)\n",
				mach_error_string(r), r);
		kllog(LOG_DEBUG, "Server %s deallocated %#x bytes "
			"at address %#x in executable \"%s\"\n",
			server->name, server->vm_size, server->vm_addr,
			server->executable);
		server->alloc_in_exec = FALSE;
	}
	if (server->alloc_in_self) {
		r = vm_deallocate(task_self(), (vm_offset_t)server->header,
			  server->vm_size
			+ ((vm_offset_t)server->header - server->vm_addr));
		if (r != KERN_SUCCESS)
			kllog(LOG_ERR, "vm_deallocate in self failed: "
				"%s (%d)\n",
				mach_error_string(r), r);
		kllog(LOG_DEBUG, "Server %s deallocated %#x bytes "
			"at address %#x in self\n",
			server->name,
			  server->vm_size
			+ ((vm_offset_t)server->header - server->vm_addr),
			(vm_offset_t)server->header);
		server->alloc_in_self = FALSE;
	}

	while (!queue_empty(&server->unload_cmds)) {
		server_command_t *scp;

		queue_remove_first(&server->unload_cmds, scp,
			server_command_t *, link);
		free(scp);
	}

	while (!queue_empty(&server->load_cmds)) {
		server_command_t *scp;

		queue_remove_first(&server->load_cmds, scp,
			server_command_t *, link);
		if (scp->msg_type == S_C_HMAP || scp->msg_type == S_C_SMAP)
			port_deallocate(task_self(), scp->port);
		free(scp);
	}

	server->state = Deallocated;
	kllog(LOG_INFO, "Server %s Deallocated\n", server->name);
	condition_broadcast(server->state_changed);
	return KERN_SUCCESS;
}

void server_delete(server_t *server)
{
	/*
	 * Let others get to this.
	 */
	server_unlock(server);

	queue_lock();
	queue_remove(&server_queue, server, server_t *, link);
	queue_unlock();

	server_lock(server);
	mutex_free(server->lock);
	condition_free(server->state_changed);
	free(server);
}

/*
 * Return the names of all known servers.
 */
kern_return_t server_list (
	server_name_t	**names,
	unsigned int	*count)
{
	server_t *server;
	unsigned int i = 0;
	kern_return_t r;

	queue_lock();
	for (  server = (server_t *)queue_first(&server_queue)
	     ; !queue_end(&server_queue, (queue_t)server)
	     ; server = (server_t *)queue_next(&server->link))
	{
		server_lock(server);
		if (server->state == Deallocated) {
			server_unlock(server);
			continue;
		}
		server_unlock(server);
		i++;
	}
	*count = i;

	if (names == NULL) {
		queue_unlock();
		return KERN_SUCCESS;
	}

	r = vm_allocate(task_self(), (vm_address_t *)names,
		i * sizeof(*names), TRUE);
	if (r != KERN_SUCCESS) {
		queue_unlock();
		return r;
	}

	i = 0;

	for (  server = (server_t *)queue_first(&server_queue)
	     ; !queue_end(&server_queue, (queue_t)server)
	     ; server = (server_t *)queue_next(&server->link))
	{
		server_lock(server);
		if (server->state == Deallocated) {
			server_unlock(server);
			continue;
		}
		strcpy((*names)[i++], server->name);
		server_unlock(server);
	}
	queue_unlock();

	return KERN_SUCCESS;
}

/*
 * Return information about a particular server.
 */
kern_return_t server_info (
	server_t	*server,
	port_name_t	**port_list,
	server_name_t	**names,
	boolean_t	**advertised,
	unsigned int	*count)
{
	server_command_t *scp;
	unsigned int i = 0;
	kern_return_t r;

	if (server->state == Allocating || server->state == Deallocated) {
		*count = 0;
		return KERN_SUCCESS;
	}

	for (  scp = (server_command_t *)
			queue_first(&server->load_cmds)
	     ; !queue_end(&server->load_cmds, (queue_t)scp)
	     ; scp = (server_command_t *)queue_next(&scp->link))
		if (   scp->msg_type == S_C_HMAP
		    || scp->msg_type == S_C_SMAP)
			i++;

	*count = i;

	r = vm_allocate(task_self(), (vm_address_t *)port_list,
		i * sizeof(*port_list), TRUE);
	if (r != KERN_SUCCESS) {
		return r;
	}
	r = vm_allocate(task_self(), (vm_address_t *)names,
		i * sizeof(*names), TRUE);
	if (r != KERN_SUCCESS) {
		return r;
	}
	r = vm_allocate(task_self(), (vm_address_t *)advertised,
		i * sizeof(*advertised), TRUE);
	if (r != KERN_SUCCESS) {
		return r;
	}

	i = 0;

	/*
	 * List ports this guy has advertised.
	 */
	for (  scp = (server_command_t *)
			queue_first(&server->load_cmds)
	     ; !queue_end(&server->load_cmds, (queue_t)scp)
	     ; scp = (server_command_t *)queue_next(&scp->link))
	{
		if (   scp->msg_type == S_C_HMAP
		    || scp->msg_type == S_C_SMAP)
		{
			(*port_list)[i] = scp->port;
			strcpy((*names)[i], scp->port_name);
			(*advertised)[i] = scp->advertised;
			i++;
		}
	}

	return KERN_SUCCESS;
}
