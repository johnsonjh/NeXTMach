/*
 * Simple loadable kernel server example.
 *
 * This server accepts two messages:
 *	simple_puts() prints the (inline string) argument on the console.
 *	simple_vers() returns the running kernel's version string.
 */
#import "simple_types.h"
#import <kernserv/kern_server_types.h>
#import <sys/mig_errors.h>

/*
 * Allocate an instance variable to be used by the kernel server interface
 * routines for initializing and accessing this service.
 */
kern_server_t instance;

/*
 * Stamp our arival.
 */
void simple_init(void)
{
	printf("Simple kernel server initialized\n");
}

/*
 * Notify the world that we're going away.
 */
void simple_signoff(void)
{
	printf("Simple kernel server unloaded\n");
}

/*
 * Print the passed string on the console.
 */
kern_return_t simple_puts (
	void		*arg,
	simple_msg_t	string)
{
	printf(string);
	return KERN_SUCCESS;
}

/*
 * Return the kernel version string to the caller.
 */
kern_return_t simple_vers (
	void		*arg,
	simple_msg_t	string)
{
	extern char version[];
	strcpy(string, version);
	return KERN_SUCCESS;
}
