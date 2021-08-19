/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 * 28-Aug-90  Morris Meyer (mmeyer) at NeXT
 *	Created.
 */

#import <sys/user.h>
#import <sys/errno.h>
#import <kern/kalloc.h>
#import <next/fptrace.h>

/*
 * fptrace system call
 */
 
fptrace ()
{
	register struct a {
		int 	pid;
		int	cmd;
		caddr_t value;
	} *uap;

	uap = (struct a *)u.u_ap;
	switch (uap->cmd)  {
		case FPTRACE_START:
			break;
		case FPTRACE_STOP:
			break;
		case FPTRACE_CLEAR:
			break;
		default:
			u.u_error = EINVAL;			
			return;
	}
}