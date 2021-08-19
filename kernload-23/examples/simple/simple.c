/*
 * Simple loadable kernel server example.
 */

#include "simple.h"

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
	port_name_t simple_port;
	char buf[80];

	/*
	 * Look up the advertized port of the loadable server.
	 */
	r = netname_look_up(name_server_port, "",
			    "simple", &simple_port);
	if (r != KERN_SUCCESS) {
		mach_error("simple: can't find simple server", r);
		exit(1);
	}

	r = simple_puts(simple_port, "Hello, World\n");
	if (r != KERN_SUCCESS) {
		mach_error("simple: simple_puts", r);
		exit(1);
	}
	r = simple_vers(simple_port, buf);
	if (r != KERN_SUCCESS) {
		mach_error("simple: simple_vers", r);
		exit(1);
	}
	printf("kernel returns version: %s\n", buf);
	exit(0);
}


