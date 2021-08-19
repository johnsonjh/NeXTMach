/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 *  4-April-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <mach.h>
#import <kernserv/kern_loader.h>
#import <servers/bootstrap.h>

/*
 * Get port for communication with kern_loader.
 */
kern_return_t kern_loader_look_up(port_name_t *kern_loader_port)
{
	return bootstrap_look_up(bootstrap_port, KERN_LOADER_NAME,
				 kern_loader_port);
}


