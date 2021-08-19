/*
 * Simple loadable kernel server example.
 *
 * This server accepts two messages:
 *	simple_puts() prints the (inline string) argument on the console.
 *	simple_vers() returns the running kernel's version string.
 */
#include "simple_handler.h"
#include <kernserv/kern_server_types.h>
#include <sys/mig_errors.h>

kern_return_t s_puts (
	void		*arg,
	simple_msg_t	string);

kern_return_t s_vers (
	void		*arg,
	simple_msg_t	string);

simple_t simple = {
	0,
	100,
	s_puts,
	s_vers
};

death_pill_t *simple_outmsg;

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
	simple_outmsg = kalloc(SIMPLE_OUTMSG_SIZE);
	printf("Simple kernel server initialized\n");
}

/*
 * Notify the world that we're going away.
 */
void simple_signoff(void)
{
	kfree(simple_outmsg, SIMPLE_OUTMSG_SIZE);
	simple_outmsg = 0;
	printf("Simple kernel server unloaded\n");
}

/*
 * Print the passed string on the console.
 */
kern_return_t s_puts (
	void		*arg,
	simple_msg_t	string)
{
	printf(string);
	return KERN_SUCCESS;
}

/*
 * Return the kernel version string to the caller.
 */
kern_return_t s_vers (
	void		*arg,
	simple_msg_t	string)
{
	extern char version[];
	strcpy(string, version);
	return KERN_SUCCESS;
}

