/*	@(#)kl_com.c	2.0	27/07/90	(c) 1990 NeXT	*/

/* 
 * kl_com.c -- kern_loader communication module for msdos.util
 *
 * HISTORY
 * 27-Jul-90	Doug Mitchell at NeXT
 *	Cloned from kl_util source.
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
#import <kernserv/kern_loader_types.h>
#import <kernserv/kern_loader.h>
#import <kernserv/kern_loader_reply_handler.h>
#import <sys/param.h>
#import <sys/notify.h>
#import <sys/message.h>
#import <next/loadable_fs.h>
#import "cfs.util.h"

static int kl_init();
static int kl_com_log(port_name_t port);
static kern_return_t print_string (
	void		*arg,
	printf_data_t	string,
	u_int		stringCnt,
	int		level);
static kern_return_t ping (
	void		*arg,
	int		id);
static void kl_com_error(char *err_str, kern_return_t rtn);
static klc_server_state kl_com_get_state(char *server_name);

port_name_t kl_port;
port_name_t kernel_task, reply_port;
boolean_t kl_init_flag = FALSE;

/*
 * Set up ports, etc. for kern_loader communication.
 */
static int kl_init() {
	kern_return_t r;
	boolean_t isactive;

	if(kl_init_flag)
		return(0);
	r = kern_loader_look_up(&kl_port);
	if (r != KERN_SUCCESS) {
		kl_com_error("can't find kernel loader", r);
		return(FSUR_LOADERR);
	}

	r = task_by_unix_pid(task_self(), 0, &kernel_task);
	if (r != KERN_SUCCESS) {
		kl_com_error("cannot get kernel task port: %s\n", r);
		return(FSUR_LOADERR);
	}

	r = port_allocate(task_self(), &reply_port);
	if (r != KERN_SUCCESS) {
		kl_com_error("can't allocate reply port", r);
		return(FSUR_LOADERR);
	}

	r = bootstrap_status(bootstrap_port, KERN_LOADER_NAME, &isactive);
	if (r != KERN_SUCCESS) {
		kl_com_error("can't find kernel loader status", r);
		return(FSUR_LOADERR);
	}
	/*
	 * Create a thread to listen on reply_port
	 * and print out anything that comes back.
	 */
	cthread_init();
	cthread_set_name(cthread_self(), "command thread");
	cthread_detach(cthread_fork((cthread_fn_t)kl_com_log,
		(any_t)reply_port));

	if (!isactive)
		printf("kernel loader inactive, pausing\n");

	r = kern_loader_status_port(kl_port, reply_port);
	if(r) {
		kl_com_error("can't get status back\n", r);
		return(FSUR_LOADERR);
	}
	kl_init_flag = TRUE;
	return(0);
}


kern_loader_reply_t kern_loader_reply = {
	0,
	0,
	print_string,
	ping
};

/*
 * wait for reply messages.
 */
static int kl_com_log(port_name_t port)
{
	char msg_buf[KERN_LOADER_REPLY_INMSG_SIZE];
	msg_header_t *msg = (msg_header_t *)msg_buf;
	port_name_t notify_port;
	port_set_name_t port_set;
	kern_return_t r;

	dprintf(("kl_com_log: TOP\n"));
	
	r = port_allocate(task_self(), &notify_port);
	if (r != KERN_SUCCESS) {
		kl_com_error("allocating notify port", r);
		return(FSUR_LOADERR);
	}

	r = task_set_notify_port(task_self(), notify_port);
	if (r != KERN_SUCCESS) {
		kl_com_error("cannot set notify port", r);
		return(FSUR_LOADERR);
	}

	r = port_set_allocate(task_self(), &port_set);
	if (r != KERN_SUCCESS) {
		kl_com_error("allocating port set", r);
		return(FSUR_LOADERR);
	}

	r = port_set_add(task_self(), port_set, notify_port);
	if (r != KERN_SUCCESS) {
		kl_com_error("adding notify port to port set", r);
		return(FSUR_LOADERR);
	}

	r = port_set_add(task_self(), port_set, port);
	if (r != KERN_SUCCESS) {
		kl_com_error("adding listener port to port set", r);
		return(FSUR_LOADERR);
	}
	while(1) {
		msg->msg_size = KERN_LOADER_REPLY_INMSG_SIZE;
		msg->msg_local_port = port_set;
		r = msg_receive(msg, MSG_OPTION_NONE, 0);
		if (r != KERN_SUCCESS) {
			kl_com_error("log_thread receive", r);
			return(FSUR_LOADERR);
		}
		/* dprintf(("kl_com_log: msg received\n")); */
		if (msg->msg_local_port == notify_port) {
			notification_t *n = (notification_t *)msg;
			
			if ((msg->msg_id == NOTIFY_PORT_DELETED) &&
			    (n->notify_port == kl_port)) {
				printf("kl_com_wait: kern_loader port "
					"deleted\n");
				continue;
			}
			else {
				printf("kl_com_wait: weird notification\n");
				continue;
			}
		} 
		else 
			kern_loader_reply_handler(msg, &kern_loader_reply);
	}
	/*
	 * FIXME - when do we exit? 
	 */
	return(0);
}

static kern_return_t print_string (
	void		*arg,
	printf_data_t	string,
	u_int		stringCnt,
	int		level)
{
	/* dprintf(("kl_com: print_string\n")); */
#ifdef	DEBUG
	if (stringCnt == 0 || !string)
		return KERN_SUCCESS;
	fputs(string, stdout);
	fflush(stdout);
#endif	DEBUG
	return KERN_SUCCESS;
}

static kern_return_t ping (
	void		*arg,
	int		id)
{
	dprintf(("kl_com: ping - exit_code %d\n", exit_code));
	exit(exit_code);
}

int kl_com_add(char *path_name, char *server_name) {
	int rtn;
	kern_return_t r;
	
	dprintf(("kl_com_add: %s\n", path_name));
	if(rtn = kl_init())
		return(rtn);
	switch(kl_com_get_state(server_name)) {
	    case KSS_LOADED:
	    case KSS_ALLOCATED:
	    	dprintf(("kl_com_add: server already exists\n"));
	    	return(0);		/* this function is a nop */
	   default:
	   	break;
	}
	r = kern_loader_add_server(kl_port, kernel_task, path_name);
	if((r != KERN_SUCCESS) && 
	   (r != KERN_LOADER_SERVER_EXISTS) &&
	   /* bogus - covers kern_loader bug #7190 */
	   (r != KERN_LOADER_SERVER_WONT_LOAD)) {
		kl_com_error("kern_loader_add_server", r);
		rtn = FSUR_LOADERR;
	}
	dprintf(("kl_com_add: returning %d\n", rtn));
	return(rtn);
}

int kl_com_delete(char *server_name) {
	int rtn;
	kern_return_t r;

	dprintf(("kl_com_delete: %s\n", server_name));
	if(rtn = kl_init())
		return(rtn);
	r = kern_loader_delete_server(kl_port, kernel_task, server_name);
	if(r) {
		kl_com_error("kern_loader_delete_server", r);
		rtn = FSUR_LOADERR;
	}
	dprintf(("kl_com_delete: returning %d\n", rtn));
	return(rtn);
}

int kl_com_load(char *server_name) {
	int rtn;
	kern_return_t r;

	dprintf(("kl_com_load: %s\n", server_name));
	if(rtn = kl_init())
		return(rtn);
	switch(kl_com_get_state(server_name)) {
	    case KSS_LOADED:
	    	dprintf(("kl_com_loaded: server already loaded\n"));
	    	return(0);		/* this function is a nop */
	   default:
	   	break;
	}
	r = kern_loader_load_server(kl_port, server_name);
	if(r) {
		kl_com_error("kern_loader_load_server", r);
		rtn = FSUR_LOADERR;
	}
	dprintf(("kl_com_load: returning %d\n", rtn));
	return(rtn);
}

int kl_com_unload(char *server_name) {
	int rtn = 0;
	kern_return_t r;

	dprintf(("kl_com_unload: %s\n", server_name));
	if(rtn = kl_init())
		return(rtn);
	r = kern_loader_unload_server(kl_port, kernel_task, server_name);
	if(r) {
		kl_com_error("kern_loader_unload_server", r);
		rtn = FSUR_LOADERR;
	}
	dprintf(("kl_com_unload: returning %d\n", rtn));
	return(rtn);
}

static klc_server_state kl_com_get_state(char *server_name)
{
	server_state_t state;
	/*
	 * remainder are unused...
	 */
	vm_address_t load_address;
	vm_size_t load_size;
	server_reloc_t reloc, loadable;
	port_name_t *ports;
	port_name_string_array_t port_names;
	u_int cnt;
	boolean_t *advertised;
	kern_return_t rtn;
	
	dprintf(("kl_com_get_state: %s\n", server_name));
	if(rtn = kl_init())
		return(rtn);
	rtn = kern_loader_server_info(kl_port, PORT_NULL, server_name,
		&state, &load_address, &load_size, reloc, loadable,
		&ports, &cnt, &port_names, &cnt, &advertised, &cnt);
	if(rtn != KERN_SUCCESS) {
		dprintf(("\tstate KSS_UNKNOWN\n"));
		return(KSS_UNKNOWN);
	}
	switch(state) {
	    case Allocating:
	    case Allocated:
	    	dprintf(("\tstate KSS_ALLOCATED\n"));
	    	return(KSS_ALLOCATED);
	    case Loading:
	    case Loaded:
	    	dprintf(("\tstate KSS_LOADED\n"));
	    	return(KSS_LOADED);
	    case Zombie:
	    case Unloading:
	    case Deallocated:
	    default:
	    	dprintf(("\tstate KSS_UNKNOWN\n"));
	    	return(KSS_UNKNOWN);
	}

}

void kl_com_error(char *err_str, kern_return_t rtn)
{
#ifdef	DEBUG
	char err_string[100];
	
	strcpy(err_string, "kl_com: ");
	strcat(err_string, err_str);
	mach_error(err_str, rtn);
#endif	DEBUG
}

void kl_com_wait()
{
	dprintf(("kl_com_wait\n"));
	kern_loader_ping(kl_port, reply_port, 0);
	pause();
}

