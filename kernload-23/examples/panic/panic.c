/*
 * Copyright 1989, 1990 NeXT, Inc.  All rights reserved.
 *
 * Loadable kernel server example showing how to panic.
 */

#include "panic.h"

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
	port_name_t panic_port;
	char buf[80];

	/*
	 * Look up the advertized port of the loadable server.
	 */
	r = netname_look_up(name_server_port, "",
			    "panic", &panic_port);
	if (r != KERN_SUCCESS) {
		mach_error("panic: can't find panic server", r);
		exit(1);
	}

	r = panic_panic(panic_port);
	if (r != KERN_SUCCESS) {
		mach_error("panic", r);
		exit(1);
	}
	exit(0);
}
