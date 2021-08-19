/*
 * Copyright 1989, 1990 NeXT, Inc.  All rights reserved.
 *
 * Loadable kernel server example showing how to log events.
 *
 * This server accepts two messages:
 *	log_msg() Logs the specified message using the level provided.
 *	log_async() Logs the specified message asynchronusly as specified.
 */
#include "log.h"
#include <kernserv/kern_server_types.h>
#include <sys/mig_errors.h>

/*
 * Allocate an instance variable to be used by the kernel server interface
 * routines for initializing and accessing this service.
 */
kern_server_t instance;

/*
 * Stamp our arival.
 */
void log_init(void)
{
	printf("Log test kernel server initialized\n");
}

/*
 * Notify the world that we're going away.
 */
void log_signoff(void)
{
	printf("Log test kernel server unloaded\n");
}

/*
 * Print the passed string on the console.
 */
kern_return_t log_msg (
	port_t		log_port,
	int		level)
{
	kern_serv_log(&instance, level, "logged message", 0, 0, 0, 0, 0);
	return KERN_SUCCESS;
}

/*
 * Set this message up to be logged asynchronously.
 */
kern_return_t log_async (
	port_t		log_port,
	int		level,
	int		interval,
	int		iterations)
{
}
