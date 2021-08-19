/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 *  4-Apr-90  Gregg Kellogg (gk) at NeXT
 *	Changed from using netname_look_up to kern_loader_lookup.
 *
 *  7-Jul-89  Gregg Kellogg (gk) at NeXT
 *	Made path name specifying server to load (-a option) relative
 *	to the current working directory if it's not absolute.
 *
 * 17-Jun-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <kern/mach_traps.h>
#import <mach.h>
#import <stdlib.h>
#import <libc.h>
#import <stdio.h>
#import <strings.h>
#import <mach_error.h>
#import <mig_errors.h>
#import <cthreads.h>
#import <servers/bootstrap.h>
#import <kernserv/kern_loader_error.h>
#import <kernserv/kern_loader.h>
#import <kernserv/kern_loader_reply_handler.h>
#import <sys/param.h>
#import <sys/notify.h>
#import <sys/message.h>

void usage(const char *name);
void kl_list(void);
void kl_info(const char *server);
void log_thread(port_name_t port);
kern_return_t print_string (
	void		*arg,
	printf_data_t	string,
	u_int		stringCnt,
	int		level);
kern_return_t ping (
	void		*arg,
	int		id);

port_name_t kl_port;

/*
 * Communication with the kernel server loader.
 */
main(int ac, char **av)
{
	kern_return_t r;
	port_name_t kernel_task, reply_port;
	char c;
	extern char *optarg;
	extern int optind;
	enum {none, add, delete, load, unload, status, Abort, restart}
		command = none;
	boolean_t must_be_root = FALSE;
	boolean_t Listen = FALSE;
	boolean_t isactive;
	char path[MAXPATHLEN];

	while ((c = getopt(ac, av, "aAdlursL")) != EOF) {
		if (command != none) {
			usage("kl_util");
			exit(1);
		}
		switch (c) {
		case 'a':
			command = add;
			must_be_root = TRUE;
			break;
		case 'A':
			command = Abort;
			must_be_root = TRUE;
			break;
		case 'd':
			command = delete;
			must_be_root = TRUE;
			break;
		case 'l':
			command = load;
			break;
		case 'r':
			command = restart;
			must_be_root = TRUE;
			break;
		case 's':
			command = status;
			break;
		case 'u':
			command = unload;
			must_be_root = TRUE;
			break;
		case 'L':
			Listen++;
			break;
		case '?':
		default:
			usage("kl_util");
			exit(1);
		}
	}

	if (   optind == ac
	    && command != status
	    && command != restart
	    && command != Abort
	    && !Listen)
	{
		usage("kl_util");
		exit(1);
	}

	if (must_be_root && getuid()) {
		fprintf(stderr, "kl_util: Must be root\n");
		exit(1);
	}

	r = kern_loader_look_up(&kl_port);
	if (r != KERN_SUCCESS) {
		mach_error("kl_util: can't find kernel loader", r);
		exit(1);
	}

	if (must_be_root) {
		r = task_by_unix_pid(task_self(), 0, &kernel_task);
		if (r != KERN_SUCCESS) {
			mach_error("kl_util: cannot get kernel "
				"task port: %s\n",
				r);
			exit(1);
		}
	}

	r = port_allocate(task_self(), &reply_port);
	if (r != KERN_SUCCESS) {
		mach_error("kl_util: can't allocate reply port", r);
		exit(1);
	}

	r = bootstrap_status(bootstrap_port, KERN_LOADER_NAME, &isactive);
	if (r != KERN_SUCCESS) {
		mach_error("kl_util: can't find kernel loader status", r);
		exit(1);
	}

	if (!isactive)
		printf("kl_util: kernel loader inactive, pausing\n");

	/*
	 * Handle the status request separately.
	 */
	if (command == status) {
		if (optind == ac)
			kl_list();
		else {
			while (optind < ac)
				kl_info(av[optind++]);
		}
		exit(0);
	}

	r = kern_loader_status_port(kl_port, reply_port);
	switch (r) {
	case KERN_SUCCESS:
		/*
		 * Create a thread to listen on reply_port
		 * and print out anything that comes back.
		 */
		cthread_init();
		cthread_set_name(cthread_self(), "command thread");
		cthread_detach(cthread_fork((cthread_fn_t)log_thread,
			(any_t)reply_port));
		break;

	default:
		mach_error("kl_util: can't get status back\n", r);
		break;
	}

	for (;     command == status
		|| command == Abort
		|| command == restart
		|| optind < ac
	     ; optind++)
	{
		switch (command) {
		case restart:
		case Abort:
			r = kern_loader_abort(kl_port, host_priv_self(),
				command == restart);
			command = none;
			break;
		case add:
			/*
			 * If the file given didn't have an absolute
			 * pathname, prepend the current directory to it.
			 */
			if (*av[optind] != '/') {
				getwd(path);
				strcat(path, "/");
			} else
				path[0] = '\0';
			strcat(path, av[optind]);
			r = kern_loader_add_server(kl_port, kernel_task,
				path);
			break;

		case delete:
			r = kern_loader_delete_server(kl_port, kernel_task,
				av[optind]);
			break;
		case load:
			r = kern_loader_load_server(kl_port, av[optind]);
			break;
		case unload:
			r = kern_loader_unload_server(kl_port, kernel_task,
				av[optind]);
			break;
		case status:
			/*NOTREACHED*/
			break;
		}
		if (r != KERN_SUCCESS) {
			kern_loader_error("kl_util", r);
			exit(1);
		}
	}

	/*
	 * Get a ping message sent back to us so we know when to exit.
	 */
	if (!Listen)
		kern_loader_ping(kl_port, reply_port, 0);

	pause();
	exit(0);
}

void usage(char *name)
{
	char Name[80];
	sprintf(Name, "%s [-L(isten)]", name);

	fprintf(stderr, "usage:	%s -s(tatus) [server_name ...]\n"
			"	%s -l(oad server) server_name ...\n"
			"	%s -u(nload server) server_name ...\n"
			"	%s -a(dd server) server_relocatable ...\n"
			"	%s -d(elete server) server_name ...\n"
			"	%s -r(estart)\n"
			"	%s -A(bort)\n",
			Name, Name, Name, Name, Name, Name, Name);
}

/*
 * List out some information on each server.
 */
void kl_list(void)
{
	unsigned int			server_cnt, cnt, i;
	server_name_t			*server_names;
	port_name_t			*ports;
	port_name_string_array_t	port_names;
	boolean_t			*advertised;
	server_state_t			state;
	server_reloc_t			relocatable, loadable;
	vm_address_t			load_address;
	vm_size_t			load_size;
	kern_return_t			r;

	r = kern_loader_server_list(kl_port, &server_names, &server_cnt);
	if (r != KERN_SUCCESS) {
		kern_loader_error("kern_loader_server_list", r);
		exit(1);
	}
	if (server_cnt == 0) {
		printf("No servers active\n");
		return;
	}

	for (i = 0; i < server_cnt; i++) {
		r = kern_loader_server_info(kl_port, PORT_NULL,
			server_names[i], &state, &load_address, &load_size,
			relocatable, loadable, &ports, &cnt,
			&port_names, &cnt, &advertised, &cnt);
		if (r != KERN_SUCCESS) {
			kern_loader_error("kern_loader_server_info", r);
			exit(1);
		}

		printf("SERVER: %s %s\n", server_names[i],
			server_state_string(state));
	}
}

/*
 * List out specified information on the named server.
 */
void kl_info(const char *server)
{
	unsigned int			cnt, i;
	port_name_t			*ports;
	port_name_string_array_t	port_names;
	boolean_t			*advertised;
	server_state_t			state;
	server_reloc_t			reloc, loadable;
	vm_address_t			load_address;
	vm_size_t			load_size;
	kern_return_t			r;

	r = kern_loader_server_info(kl_port, PORT_NULL, (char *)server,
		&state, &load_address, &load_size, reloc, loadable,
		&ports, &cnt, &port_names, &cnt, &advertised, &cnt);
	if (r != KERN_SUCCESS) {
		kern_loader_error("kern_loader_server_info", r);
		exit(1);
	}

	printf("SERVER: %s\n", server);
	if (state != Allocating) {
		printf("RELOCATABLE: %s\n", reloc);
		if (*loadable)
			printf("LOADABLE: %s\n", loadable);
	}

	printf("STATUS: %s ", server_state_string(state));
	switch (state) {
	default:
		printf("at address %#x for %#x bytes\n",
			load_address, load_size);
		printf("PORTS:");
		for (i = 0; i < cnt; i++)
			printf(" %s%s", port_names[i],
				advertised[i] ? "(advertised)" : "");
		break;
	case Allocating:
	case Deallocated:
	case Zombie:
		break;
	}

	printf("\n");
}

kern_loader_reply_t kern_loader_reply = {
	0,
	0,
	print_string,
	ping
};

void log_thread(port_name_t port)
{
	char msg_buf[KERN_LOADER_REPLY_INMSG_SIZE];
	msg_header_t *msg = (msg_header_t *)msg_buf;
	port_name_t notify_port;
	port_set_name_t port_set;
	kern_return_t r;

	r = port_allocate(task_self(), &notify_port);
	if (r != KERN_SUCCESS) {
		mach_error("allocating notify port", r);
		exit(1);
	}

	r = task_set_notify_port(task_self(), notify_port);
	if (r != KERN_SUCCESS) {
		mach_error("cannot set notify port", r);
		exit(1);
	}

	r = port_set_allocate(task_self(), &port_set);
	if (r != KERN_SUCCESS) {
		mach_error("allocating port set", r);
		exit(1);
	}

	r = port_set_add(task_self(), port_set, notify_port);
	if (r != KERN_SUCCESS) {
		mach_error("adding notify port to port set", r);
		exit(1);
	}

	r = port_set_add(task_self(), port_set, port);
	if (r != KERN_SUCCESS) {
		mach_error("adding listener port to port set", r);
		exit(1);
	}

	while (TRUE) {
		msg->msg_size = KERN_LOADER_REPLY_INMSG_SIZE;
		msg->msg_local_port = port_set;
		r = msg_receive(msg, MSG_OPTION_NONE, 0);
		if (r != KERN_SUCCESS)
			break;
		if (msg->msg_local_port == notify_port) {
			notification_t *n = (notification_t *)msg;
			if (   msg->msg_id == NOTIFY_PORT_DELETED
			    && n->notify_port == kl_port)
				exit(0);
		} else
			kern_loader_reply_handler(msg, &kern_loader_reply);
	}
	mach_error("kl_util: log_thread receive", r);
	exit(1);
}

kern_return_t print_string (
	void		*arg,
	printf_data_t	string,
	u_int		stringCnt,
	int		level)
{
	if (stringCnt == 0 || !string)
		return KERN_SUCCESS;
	fputs(string, stdout);
	fflush(stdout);
	return KERN_SUCCESS;
}

kern_return_t ping (
	void		*arg,
	int		id)
{
	exit(0);
}

