#include "simple_handler.h"
#include "simpleServer.c"
#include <sys/mig_errors.h>

/*
 * The port argument in each of the following is actually a pointer
 * to a structure containing pointers to functions to call for each of
 * the following messages.
 */

kern_return_t simple_puts (
	port_t simple_port,
	simple_msg_t string)
{
	simple_t *simple = (simple_t *)simple_port;

	if (simple->puts == 0)
		return MIG_BAD_ID;
	return (*simple->puts)(simple->arg, string);
}

kern_return_t simple_vers (
	port_t simple_port,
	simple_msg_t string)
{
	simple_t *simple = (simple_t *)simple_port;

	if (simple->vers == 0)
		return MIG_BAD_ID;
	return (*simple->vers)(simple->arg, string);
}

kern_return_t simple_handler (
	msg_header_t *msg,
	simple_t *simple)
{
	kern_return_t ret_code;
	port_t local_port = msg->msg_local_port;
	extern death_pill_t *simple_outmsg;

	msg->msg_local_port = (port_t)simple;

	simple_server(msg, (msg_header_t *)simple_outmsg);
	ret_code = simple_outmsg->RetCode;

	if (simple_outmsg->RetCode == MIG_NO_REPLY)
		ret_code = KERN_SUCCESS;
	else
		ret_code = msg_send(
			&simple_outmsg->Head,
			  simple->timeout >= 0
			? SEND_TIMEOUT
			: MSG_OPTION_NONE,
			simple->timeout);

	return ret_code;
}
