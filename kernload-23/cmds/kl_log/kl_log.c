/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 20-June-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 * 25-Jun-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	Removed the -h host argument, as we can't pass the kernel
 *	port to another machine anyway.
 */ 

#import <mach.h>
#import <stdlib.h>
#import <stdio.h>
#import <strings.h>
#import <mach_error.h>
#import <mig_errors.h>
#import <servers/netname.h>
#import <sys/notify.h>
#import <kernserv/kern_loader.h>
#import <kernserv/kern_loader_error.h>
#import <kernserv/kern_loader_reply_handler.h>

/*
 * These routines should be prototyped someplace in /usr/include!
 */
int getopt(int ac, char **av, char *optstring);
int getuid(void);
int sleep(int secs);

void usage(char *name);

kern_return_t log_data(void *arg, printf_data_t log_data, unsigned int log_dataCnt);

kern_loader_reply_t klrh = {
	0,
	0,
	0,
	0,
	log_data
};

port_t kl_port, kernel_port;
port_t com_port, reply_port;

/*
 * Communication with the kernel server loader.
 */
main(int ac, char **av)
{
	kern_return_t r;
	char c;
	extern char *optarg;
	extern int optind;
	int log_level = 0x7fffffff;
	char msg_buf[KERN_LOADER_REPLY_INMSG_SIZE];
	msg_header_t *in_msg = (msg_header_t *)msg_buf;
	port_name_t notify_port;
	port_set_name_t port_set;

	while ((c = getopt(ac, av, "l:")) != EOF) {
		switch (c) {
		case 'l':
			log_level = strtol(optarg, 0, 0);
			break;
		case '?':
		default:
			usage("kl_log");
			exit(1);
		}
	}

	if (optind == ac) {
		usage("kl_log");
		exit(1);
	}

	r = kern_loader_look_up(&kl_port);
	if (r != KERN_SUCCESS) {
		mach_error("kl_log: can't find kernel loader", r);
		exit(1);
	}

	r = task_by_unix_pid(task_self(), 0, &kernel_port);
	if (r != KERN_SUCCESS) {
		mach_error("Can't access kernel", r);
		exit(1);
	}

	r = kern_loader_server_com_port(kl_port, kernel_port, av[optind], &com_port);
	if (r != KERN_SUCCESS) {
		kern_loader_error("Can't get server communications port", r);
		exit(1);
	}

	/*
	 * Tell the server to start logging at the new level.
	 */
	r = kern_loader_log_level(kl_port, com_port, log_level);
	if (r != KERN_SUCCESS) {
		kern_loader_error("Can't set log level", r);
		exit(1);
	}

	if (log_level == 0)
		exit(0);

	r = port_allocate(task_self(), &reply_port);
	if (r != KERN_SUCCESS) {
		mach_error("Can't allocate reply port", r);
		exit(1);
	}

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

	r = port_set_add(task_self(), port_set, reply_port);
	if (r != KERN_SUCCESS) {
		mach_error("adding reply_port to port set", r);
		exit(1);
	}

	r = kern_loader_get_log(kl_port, com_port, reply_port);
	if (r != KERN_SUCCESS) {
		kern_loader_error("kern_loader_get_log", r);
		exit(1);
	}

	in_msg = malloc(KERN_LOADER_REPLY_INMSG_SIZE);

	while (1) {
		in_msg->msg_size = KERN_LOADER_REPLY_INMSG_SIZE;
		in_msg->msg_local_port = port_set;
		r = msg_receive(in_msg, MSG_OPTION_NONE, 0);
		if (r != KERN_SUCCESS) {
			mach_error("msg_receive", r);
			exit(1);
		}

		if (in_msg->msg_local_port == reply_port)
			kern_loader_reply_handler(in_msg, &klrh);
		else if (in_msg->msg_local_port == notify_port) {
			notification_t *n = (notification_t *)in_msg;
			if (   in_msg->msg_id == NOTIFY_PORT_DELETED
			    && n->notify_port == kl_port)
				exit(0);
		} else {
			fprintf(stderr, "Recieved message from unknown port\n");
			exit(1);
		}
	}

	exit(0);
}

kern_return_t log_data(void *arg, printf_data_t log_data, unsigned int log_dataCnt)
{
	kern_return_t r;

	printf(log_data);
	fflush(stdout);
	vm_deallocate(task_self(), (vm_address_t)log_data, log_dataCnt*sizeof(*log_data));

	r = kern_loader_get_log(kl_port, com_port, reply_port);
	if (r != KERN_SUCCESS) {
		kern_loader_error("kern_loader_get_log", r);
		exit(1);
	}

}

void usage(char *name)
{
	fprintf(stderr,
		"usage:	%s [-l log_level] server\n",
		name);
}
