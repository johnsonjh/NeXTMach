/*
 * Simple loadable kernel server example.
 *
 * This server accepts two messages:
 *	simple_puts() prints the (inline string) argument on the console.
 *	simple_vers() returns the running kernel's version string.
 */
#import <kernserv/kern_server_types.h>

/*
 * Allocate an instance variable to be used by the kernel server interface
 * routines for initializing and accessing this service.
 */
kern_server_t instance;

/*
 * Stamp our arival.
 */
void panic_signon(int why)
{
	printf("Panic kernel server initialized\n");
	if (why)
		kern_serv_panic(kern_serv_bootstrap_port(&instance),
			"I'm tired of life");
}

/*
 * Notify the world that we're going away.
 */
void panic_signoff(void)
{
	printf("Panic kernel server unloaded\n");
}

kern_return_t panic_panic(port_t panic_port)
{
	kern_serv_panic(kern_serv_bootstrap_port(&instance),
		"I'm tired of life");
}
