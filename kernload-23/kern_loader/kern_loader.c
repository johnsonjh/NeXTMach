/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 17-Sep-90  Gregg Kellogg (gk) at NeXT
 *	ks_log_data changed to expect out-of-line log_data.
 *
 *  9-Jan-90  Gregg Kellogg (gk) at NeXT
 *	Make calls to servers asynchronous.
 *
 * 15-Apr-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import "server.h"
#import "log.h"
#import "async.h"
#import <stdlib.h>
#import <stddef.h>
#import <libc.h>
#import <stdio.h>
#import <fcntl.h>
#import <signal.h>
#import <strings.h>
#import <mach_error.h>
#import <mig_errors.h>
#import <sys/ioctl.h>
#import <sys/notify.h>
#import <sys/syslog.h>
#import <servers/bootstrap.h>
#import <kernserv/kern_server_reply_handler.h>
#import <kernserv/kern_server.h>
#import <kernserv/kern_loader_reply.h>
#import "server.h"
#import "misc.h"
#import "kern_loader_handler.h"

char	*config_file = "/etc/kern_loader.conf";
port_name_t kernel_task;
boolean_t verbose = FALSE;
boolean_t debug = FALSE;
char	*called_as;
port_set_name_t port_set;
port_name_t	loader_port, notify_port;
enum server_loop_status {Continue, Abort, Restart}
	server_loop_status = Continue;

static void usage(const char *name);
static void receive_loop(void);
static void sigint(int sig);
static void sighup(int sig);
static void background(void);

static kern_return_t kl_abort (
	void		*arg,
	port_t		priv_port,
	boolean_t	restart);
static kern_return_t kl_load_server(
	void		*arg,
	server_name_t	server_name);
static kern_return_t kl_unload_server(
	void		*arg,
	port_t		task_port,
	server_name_t	server_name);
static kern_return_t kl_add_server(
	void		*arg,
	port_t		task_port,
	server_reloc_t	server_reloc);
static kern_return_t kl_delete_server (
	void		*arg,
	port_t		task_port,
	server_name_t	server_name);
static kern_return_t kl_server_task_port (
	void		*arg,
	port_t		task_port,
	server_name_t	server_name,
	port_t		*server_task_port);
static kern_return_t kl_server_com_port (
	void		*arg,
	port_t		task_port,
	server_name_t	server_name,
	port_t		*server_com_port);
static kern_return_t kl_status_port (
	void		*arg,
	port_t		listen_port);
static kern_return_t kl_ping (
	void		*arg,
	port_t		ping_port,
	int		id);
static kern_return_t kl_log_level (
	void *		arg,
	port_t		server_com_port,
	int		log_level);
static kern_return_t kl_get_log (
	void *		arg,
	port_t		server_com_port,
	port_t		reply_port);
static kern_return_t kl_server_list (
	void *		arg,
	server_name_t	**servers,
	unsigned int	*serversCnt);
static kern_return_t kl_server_info (
	void *		arg,
	port_t		task_port,
	server_name_t	server_name,
	server_state_t	*server_state,
	vm_address_t	*load_address,
	vm_size_t	*load_size,
	server_reloc_t	relocatable,
	server_reloc_t	loadable,
	port_name_t	**port_list,
	unsigned int	*port_listCnt,
	port_name_string_array_t *names,
	unsigned int	*namesCnt,
	boolean_array_t	*advertised,
	unsigned int	*advertisedCnt);

kern_loader_t kern_loader = {
	0,
	1000,
	kl_abort,
	kl_load_server,
	kl_unload_server,
	kl_add_server,
	kl_delete_server,
	kl_server_task_port,
	kl_server_com_port,
	kl_status_port,
	kl_ping,
	kl_log_level,
	kl_get_log,
	kl_server_list,
	kl_server_info
};

static kern_return_t ks_panic(
	void		*arg,
	panic_msg_t	panic_msg);
static kern_return_t ks_section_by_name(
	void		*arg,
	const char	*segname,
	const char	*sectname,
	vm_address_t	*addr,
	vm_size_t	*size);
static kern_return_t ks_log_data(
	void		*arg,
	log_entry_t	*log,
	unsigned int	logCnt);
static kern_return_t ks_notify(
	void		*arg,
	port_t		reply_port,
	port_t		req_port);

kern_server_reply_t kern_server_reply = {
	0,
	10000,
	ks_panic,
	ks_section_by_name,
	ks_log_data,
	ks_notify,
};

static void server_init1(void *server);

/*
 *	ld_driver [-v] [-d <time>] script_file
 *
 *	This will allocate kernel virtual memory after examining the *.o
 *	file(s) to install and will then copy the text and data segments
 *	to the new space. It will then invoke the initialization routine
 *	in the kernel (loaded if necessary) which should do the appropriate
 *	linking to attach the driver to the kernel. This assumes that the
 *	data segment immediately follows the text segment.
 *
 *	-v		verbose
 *	-d		debug
 */

void main(argc, argv)
int	argc;
char	*argv[];
{
	kern_return_t r;
	FILE *f;
	char c;
	boolean_t nofork = FALSE;
	extern char *optarg;
	extern int optind;
	int i;
	unsigned int count;

	called_as = strrchr(argv[0], '/');
	if (called_as)
		called_as++;
	else
		called_as = argv[0];

	unix_lock = mutex_alloc();
	mutex_init(unix_lock);
	
	klinit(called_as);
	hash_init();

	while ((c = getopt(argc, argv, "vdnc:")) != EOF)
		switch(c) {
		case 'v':
			verbose++;
			break;
		case 'd':
			debug++;
			nofork++;
			break;
		case 'n':
			nofork++;
			break;
		case 'c':
			config_file = optarg;
			break;
		case '?':
		default:
			usage(called_as);
			break;
		}

	if (getuid()) {
		kllog(LOG_ERR, "%s: Must be root\n", called_as);
		exit(1);
	}

	signal(SIGHUP, (void (*)())sighup);
	signal(SIGINT, (void (*)())sigint);
	signal(SIGQUIT, (void (*)())sigint);
	signal(SIGTERM, (void (*)())sigint);

	/*
	 * Clean ourselves up to run as a daemon.
	 */
	if (!nofork)
		background();

	/*
	 * Initialize syslog.
	 */
	openlog(called_as, 0, LOG_DAEMON);
	kllog(LOG_NOTICE, "initialized\n");

	/*
	 * Check ourselves in with the bootstrap server.
	 */
	r = bootstrap_check_in(bootstrap_port, KERN_LOADER_NAME,
			     &loader_port);
	if (r != KERN_SUCCESS) {
		port_allocate(task_self(), &loader_port);
		r = bootstrap_register(bootstrap_port, KERN_LOADER_NAME,
			     loader_port);
	}
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "cannot checkin bootstrap port: %s\n",
			mach_error_string(r));
		return;
	}

	r = task_by_unix_pid(task_self(), 0, &kernel_task);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "cannot get kernel task port: %s\n",
			mach_error_string(r));
		exit(1);
	}
	save_task_name("/mach", kernel_task);

	/*
	 * Allocate a port set to receive messages on.
	 */
	r = port_set_allocate(task_self(), &port_set);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "cannot get allocate port set: %s\n",
			mach_error_string(r));
		exit(1);
	}

	r = port_set_add(task_self(), port_set, loader_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "cannot add port to port set: %s\n",
			mach_error_string(r));
		return;
	}

	/*
	 * Get notification messages.
	 */
	r = port_allocate(task_self(), &notify_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "cannot allocate notify port: %s\n",
			mach_error_string(r));
		return;
	}

	r = task_set_notify_port(task_self(), notify_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "cannot set notify port: %s\n",
			mach_error_string(r));
		return;
	}

	r = port_set_add(task_self(), port_set, notify_port);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "cannot add port to port set: %s\n",
			mach_error_string(r));
		return;
	}

	server_queue_init();

	/*
	 * Parse each script file, creating a port and advertising it
	 * with the net_message server.  When messages are received
	 * on the advertised port the code is downloaded into the kernel
	 * and initialized.  Receive rights to the advertised ports are
	 * sent down to the kernel server and the received message is
	 * forwarded down to the kernel server.
	 */
	for (; optind < argc; optind++)
		cthread_detach(cthread_fork((cthread_fn_t)server_init1,
			(void *)NXUniqueString(argv[optind])));

	while (server_loop_status != Abort) {
		if (server_loop_status == Restart) {
			server_loop_status = Continue;
			kllog(LOG_NOTICE, "restart\n");
		}

		/*
		 * Initialize from the config file.
		 */
		f = fopen(config_file, "r");
		if (f) {
			char buf[80], *s1, *s2;
			while (fgets(buf, sizeof(buf), f)) {
				s1 = buf + strspn(buf, " \t\n");
				if (*s1 == '#' || !*s1)
					continue;
				s2 = s1 + strcspn(s1, " \t\n");
				if (s2 && *s2)
					*s2 = '\0';
				if (strstr(s1, ".kern_server")) {
					kllog(LOG_ERR, "server %s "
						"is in old format, update "
						"using kl_util -c\n", s1);
					continue;
				}

				cthread_detach(cthread_fork(
					(cthread_fn_t)server_init1,
					(void *)NXUniqueString(s1)));
			}
			fclose(f);
		}
	
		/*
		 * Wait for messages, either for connections to kernel servers
		 * yet to be loaded, or from a loaded kernel server.
		 */
		receive_loop();
	
		/*
		 * Unload loaded servers in the background.
		 * We'll exit when they're all gone.  In the
		 * mean time, go back into the receive loop so
		 * that we can respond to messages from kernel servers.
		 */
		kllog(LOG_NOTICE, "shutting down...\n" );

		cthread_detach(cthread_fork((cthread_fn_t)server_unload,
			(void *)NULL));
		/*
		 * Wait for everything to finish, or until we're tired of
		 * waiting.
		 */
		for (i = 30; i; i--) {
			thread_array_t	threads;
			unsigned int	nthreads;
			int		j;

			(void) server_list(NULL, &count);
			if (count == 0)
				break;
			/*
			 * Interrupt the rest of the threads to get them
			 * out of their stupor.
			 */
			r = task_threads(task_self(), &threads, &nthreads);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR, "cannot get task threads: %s\n",
					mach_error_string(r));
				break;
			}
			for (j = nthreads - 1; j >= 0; j--) {
				if (threads[i] == thread_self())
					continue;
				(void)thread_abort(threads[i]);
			}
			vm_deallocate(task_self(), (vm_address_t)threads,
				sizeof(threads)*nthreads);
			sleep(1);
		}
	}
	kllog(LOG_NOTICE, "exiting\n" );
	exit(0);
}

static void usage(const char *name)
{
	 kllog(LOG_ERR, "%s [-v] [-d] [-c config] [server...]\n",
		name);
	 exit(1);
}

static void background(void)
{
	if (fork())
		exit(0);
	{ int s;
	for (s = 0; s < 10; s++)
		(void) close(s);
	}
	(void) open("/", O_RDONLY, 0);
	(void) dup2(0, 1);
	(void) dup2(0, 2);
	{int tt = open("/dev/tty", O_RDWR, 0);
	  if (tt > 0) {
		ioctl(tt, TIOCNOTTY, 0);
		close(tt);
	  }
	}
}

static void server_lock_load(void *server)
{
	server_lock((server_t *)server);
	(void)server_load((server_t *)server);
	server_unlock((server_t *)server);
}

static void server_lock_unload(void *server)
{
	server_lock((server_t *)server);
	(void)server_unload((server_t *)server);
	server_unlock((server_t *)server);
}

static void server_init1(void *server)
{
	(void)server_init((const char *)server, NULL);
}

static void receive_loop(void)
{
	msg_header_t *imsg = (msg_header_t *)malloc(MSG_SIZE_MAX);
	msg_header_t *omsg = (msg_header_t *)malloc(MSG_SIZE_MAX);
	server_t *server;
	kern_return_t r;
	typedef struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	while (server_loop_status == Continue) {
		imsg->msg_size = MSG_SIZE_MAX;
		imsg->msg_local_port = port_set;
		omsg->msg_size = MSG_SIZE_MAX;
		r = msg_receive(imsg, RCV_INTERRUPT, 0);
		switch (r) {
		case KERN_SUCCESS:
			break;
		case RCV_INTERRUPTED:
			continue;
		default:
			kllog(LOG_ERR, "msg_receive failed: %s\n",
				mach_error_string(r));
			continue;
		}

		/*
		 * Look for messages to ourselves, first.
		 */
		if (imsg->msg_local_port == loader_port)
			handler_fork_user(imsg);
		else if (imsg->msg_local_port == notify_port) {
			notification_t *n = (notification_t *)imsg;

			/*
			 * Forward notification messages to servers
			 * requesting them.
			 */
			while (server = server_by_notify_req(n->notify_port)) {
				imsg->msg_remote_port =
					server_notify(server, n->notify_port);
				msg_send(imsg, MSG_OPTION_NONE, 0);
			}

			server = server_by_port(n->notify_port);
			if (imsg->msg_id == NOTIFY_PORT_DELETED) {
				if (   server
				    && n->notify_port == server->log_reply_port)
					server->log_reply_port = PORT_NULL;
				else
					klremoveport(n->notify_port);
			}
		} else {
			port_name_t temp_port;

			server = server_by_port(imsg->msg_local_port);

			if (!server) {
				kllog(LOG_ERR,
					"server not found for local port\n");
				continue;
			}

			if (imsg->msg_local_port == server->boot_port) {
				handler_fork_reply(imsg, (void *)server);
				continue;
			}
			
			/*
			 * This must be a message for a server that needs to be
			 * loaded.
			 */
			if (server->state == Loaded) {
				kllog(LOG_WARNING, "intercepted message for "
					"loaded server\n");
			}

			cthread_detach(
				cthread_fork((cthread_fn_t)server_lock_load,
					(void *)server));

			/*
			 * Remove the port from our port-set so that we don't
			 * re-receive the message before the loaded server's
			 * had an opportunity to deal with it.
			 */
			port_set_remove(task_self(), imsg->msg_local_port);

			/*
			 * Forward the message.
			 */
			temp_port = imsg->msg_remote_port;
			imsg->msg_remote_port = imsg->msg_local_port;
			imsg->msg_local_port = temp_port;
			r = msg_send(imsg, MSG_OPTION_NONE, 0);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"cannot forward message: %s\n",
					mach_error_string(r));
			}
			continue;
		}
	}
}

/*
 * Cause the kernel loader server to shut down.
 */
static kern_return_t kl_abort (
	void		*arg,
	port_t		priv_port,
	boolean_t	restart)
{
	if (priv_port != host_priv_self())
		return KERN_LOADER_NO_PERMISSION;

	kllog(LOG_NOTICE, "recieved abort message, terminating\n");

	/*
	 * Unload loaded servers.
	 */
	server_loop_status = restart ? Restart : Abort;
	return KERN_SUCCESS;
}

/*
 * Load a kernel server.
 */
static kern_return_t kl_load_server(
	void		*arg,
	server_name_t	server_name)
{
	server_t *server;
	kern_return_t r;
	
	server = server_by_name(server_name);
	if (server == NULL || server->state == Deallocated)
		return KERN_LOADER_UNKNOWN_SERVER;

	server_lock(server);

	r = server_load(server);
	if (r != KERN_SUCCESS) {
		(void) server_deallocate(server);
		server_unlock(server);
		return r;
	}

	/*
	 * Wait for the thing to be fully loaded.
	 */
	while (temp_server_state(server->state))
		condition_wait(server->state_changed, server->lock);

	server_unlock(server);

	return KERN_SUCCESS;
}

/*
 * Unload a kernel server.
 */
static kern_return_t kl_unload_server(
	void		*arg,
	port_t		task_port,
	server_name_t	server_name)
{
	server_t *server;
	kern_return_t r;

	server = server_by_name(server_name);
	if (server == NULL || server->state == Deallocated)
		return KERN_LOADER_UNKNOWN_SERVER;

	server_lock(server);
	if (task_port != server->executable_task) {
		server_unlock(server);
		return KERN_LOADER_NO_PERMISSION;
	}

	while (temp_server_state(server->state))
		condition_wait(server->state_changed, server->lock);

	if (server->state == Allocated) {
		server_unlock(server);
		return KERN_LOADER_SERVER_UNLOADED;
	}
	
	r = server_unload(server);

	while (temp_server_state(server->state))
		condition_wait(server->state_changed, server->lock);

	server_unlock(server);

	return r;
}

/*
 * Add a server load file for future use.
 */
static kern_return_t kl_add_server(
	void		*arg,
	port_t		task_port,
	server_reloc_t	server_reloc)
{
	server_t *server;
	kern_return_t r;

	kllog(LOG_NOTICE, "Adding server with relocatable %s\n", server_reloc);

	r = server_init(NXUniqueString(server_reloc), &server);
	if (r != KERN_SUCCESS)
		return r;
	if (server->state == Deallocated)
		return KERN_LOADER_SERVER_WONT_LINK;

	if (task_port != server->executable_task) {
		kllog(LOG_INFO, "No permission to add server %s\n",
			server->name);
		server_lock(server);
		(void) server_deallocate(server);
		server_unlock(server);
		return KERN_LOADER_NO_PERMISSION;
	}

	return KERN_SUCCESS;
}

/*
 * Remove a server load file for future use.
 */
static kern_return_t kl_delete_server (
	void		*arg,
	port_t		task_port,
	server_name_t	server_name)
{
	server_t *server;
	kern_return_t r;

	kllog(LOG_NOTICE, "removing server %s\n", server_name);

	server = server_by_name(server_name);
	if (server == NULL || server->state == Deallocated)
		return KERN_LOADER_UNKNOWN_SERVER;

	server_lock(server);
	if (task_port != server->executable_task) {
		server_unlock(server);
		return KERN_LOADER_NO_PERMISSION;
	}

	r = server_deallocate(server);
	server_unlock(server);

	return r;
}

/*
 * Get the task port the the specified server.
 */
static kern_return_t kl_server_task_port (
	void		*arg,
	port_t		task_port,
	server_name_t	server_name,
	port_t		*server_task_port)
{
	server_t *server;

	server = server_by_name(server_name);
	if (server == NULL || server->state == Deallocated)
		return KERN_LOADER_UNKNOWN_SERVER;

	server_lock(server);
	while (server->state == Allocating)
		condition_wait(server->state_changed, server->lock);

	if (task_port != server->executable_task) {
		server_unlock(server);
		return KERN_LOADER_NO_PERMISSION;
	}

	*server_task_port = server->server_task;
	server_unlock(server);

	return KERN_SUCCESS;
}

/*
 * Get server communications port.
 */
static kern_return_t kl_server_com_port (
	void		*arg,
	port_t		task_port,
	server_name_t	server_name,
	port_t		*server_com_port)
{
	server_t *server;

	server = server_by_name(server_name);
	if (server == NULL || server->state == Deallocated)
		return KERN_LOADER_UNKNOWN_SERVER;

	server_lock(server);
	while (server->state == Allocating)
		condition_wait(server->state_changed, server->lock);

	if (task_port != server->executable_task) {
		server_unlock(server);
		return KERN_LOADER_NO_PERMISSION;
	}

	*server_com_port = server->server_port;
	server_unlock(server);

	return KERN_SUCCESS;
}

/*
 * Get port to send status info on.
 */
static kern_return_t kl_status_port (
	void		*arg,
	port_t		listen_port)
{
	kladdport(listen_port);
	return KERN_SUCCESS;
}

static kern_return_t kl_ping (
	void		*arg,
	port_t		ping_port,
	int		id)
{
	return kern_loader_reply_ping(ping_port, id);
}

static kern_return_t kl_log_level (
	void *		arg,
	port_t		server_com_port,
	int		log_level)
{
	kern_return_t r;
	r = kern_serv_log_level(server_com_port, log_level);
	if (r != KERN_SUCCESS) {
		server_t *server = server_by_port(server_com_port);
		if (!server)
			r =  KERN_LOADER_UNKNOWN_SERVER;
		else
			r =  KERN_LOADER_SERVER_UNLOADED;
	}
	return r;
}

static kern_return_t kl_get_log (
	void *		arg,
	port_t		server_com_port,
	port_t		reply_port)
{
	server_t *server = server_by_port(server_com_port);
	
	if (server == NULL || server->state == Deallocated)
		return KERN_LOADER_UNKNOWN_SERVER;

	if (server->log_reply_port != PORT_NULL)
		return KERN_LOADER_PORT_EXISTS;

	if (server->state != Loaded)
		return KERN_LOADER_SERVER_UNLOADED;

	server->log_reply_port = reply_port;

	return kern_serv_get_log(server_com_port, server->boot_port);
}

static kern_return_t kl_server_list (
	void *		arg,
	server_name_t	**servers,
	unsigned int	*serversCnt)
{
	return server_list(servers, serversCnt);
}

static kern_return_t kl_server_info (
	void *		arg,
	port_t		task_port,
	server_name_t	server_name,
	server_state_t	*server_state,
	vm_address_t	*load_address,
	vm_size_t	*load_size,
	server_reloc_t	relocatable,
	server_reloc_t	loadable,
	port_name_t	**port_list,
	unsigned int	*port_listCnt,
	port_name_string_array_t *names,
	unsigned int	*namesCnt,
	boolean_array_t	*advertised,
	unsigned int	*advertisedCnt)
{
	kern_return_t r;
	int i;
	server_t *server = server_by_name(server_name);

	if (server == NULL)
		return KERN_LOADER_UNKNOWN_SERVER;

	server_lock(server);
	*server_state = server->state;
	*load_address = server->vm_addr;
	*load_size = server->vm_size;
	strcpy(relocatable, server->reloc);
	if (server->loadable)
		strcpy(loadable, server->loadable);
	else
		*loadable = '\0';

	r = server_info(server, port_list, names, advertised, port_listCnt);
	*namesCnt = *advertisedCnt = *port_listCnt;

	/*
	 * If this guy didn't pass the real task_port, then don't return the
	 * real ports to him.
	 */
	if (task_port != server->executable_task)
		for (i = *namesCnt - 1; i >= 0; i--)
			(*port_list)[i] = PORT_NULL;
	
	server_unlock(server);
	return r;
}

static kern_return_t ks_panic(
	void		*arg,
	panic_msg_t	panic_msg)
{
	server_t *server = (server_t *)arg;

	kllog(LOG_WARNING, "server %s paniced: %s\n",
		server->name, panic_msg);

	server_lock(server);

	/*
	 * If the guy's loading, fork something off to unload him.
	 */
	if (server->state == Loading)
		cthread_detach(cthread_fork((cthread_fn_t)server_lock_unload,
			(void *)server));
	else
		server_unload(server);

	server_unlock(server);
	return KERN_SUCCESS;
}

static kern_return_t ks_section_by_name(
	void		*arg,
	const char	*segname,
	const char	*sectname,
	vm_address_t	*addr,
	vm_size_t	*size)
{
	server_t *server = (server_t *)arg;

	*addr = (vm_address_t)getSectDataFromHeader(
		server, segname, sectname, (int *)size);
	return KERN_SUCCESS;
}

static kern_return_t ks_log_data(
	void		*arg,
	log_entry_t	*log,
	unsigned int	logCnt)
{
	log_entry_t	*logp = log;
	unsigned int	log_cnt = logCnt;
	server_t	*server = (server_t *)arg;
	char		*outbuf, *s;
	int		outbuf_size = 1024*1024;
	kern_return_t	r;

	r = vm_allocate(task_self(), (vm_address_t *)&outbuf, outbuf_size, TRUE);
	if (r != KERN_SUCCESS)
		goto out;

	s = outbuf;

	/*
	 * Format the data and send it on to the requestor.
	 */
	while (log_cnt--) {
		s += sprintf(s, "%10u:\t", logp->timestamp);
		s += sprintf(s, logp->msg, logp->arg1, logp->arg2, logp->arg3,
			logp->arg4, logp->arg5);
		logp++;
	}
	vm_deallocate(task_self(), (vm_address_t)log, logCnt*sizeof(*log));

	r = kern_loader_reply_log_data(server->log_reply_port,
		outbuf, s - outbuf);
	vm_deallocate(task_self(), (vm_address_t)outbuf, outbuf_size);
    out:
	port_deallocate(task_self(), server->log_reply_port);
	server->log_reply_port = PORT_NULL;

	return r;
}

static kern_return_t ks_notify(
	void		*arg,
	port_t		reply_port,
	port_t		req_port)
{
	server_t *server = (server_t *)arg;
	
	server_req_notify(server, reply_port, req_port);
	
	return KERN_SUCCESS;
}

/*
 * Unload stuff we've loaded into the kernel.
 */
void sigint(int sig)
{
	kllog(LOG_WARNING, "shutting down on signal %d\n", sig);
	server_loop_status = Abort;
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

/*
 * Unload stuff we've loaded into the kernel and re-initialized.
 */
void sighup(int sig)
{
	kllog(LOG_WARNING, "restarting down on signal %d\n", sig);
	server_loop_status = Restart;
}
