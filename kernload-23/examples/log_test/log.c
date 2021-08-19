/*
 * Copyright 1989, 1990 NeXT, Inc.  All rights reserved.
 *
 * Loadable kernel server example showing how to log events.
 */

#include "log.h"

#import <mach.h>
#import <stdlib.h>
#import <libc.h>
#import <stdio.h>
#import <strings.h>
#import <mach_error.h>
#import <mig_errors.h>
#import <servers/netname.h>

/*
 * Communication with the kernel server loader.
 */
main(int ac, char **av)
{
	kern_return_t r;
	port_name_t log_port;
	char buf[80];

	/*
	 * Look up the advertized port of the loadable server.
	 */
	r = netname_look_up(name_server_port, "",
			    "log_test", &log_port);
	if (r != KERN_SUCCESS) {
		mach_error("log_test: can't find log_test server", r);
		exit(1);
	}

	r = log_msg(log_port, 1);
	if (r != KERN_SUCCESS) {
		mach_error("log_test: log_msg", r);
		exit(1);
	}
	exit(0);
}
