/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	host.h,v $
 * 17-May-90  Gregg Kellogg (gk) at NeXT
 *	Defined host_t and host_priv_t for KERNEL branch of mach_types.h.
 *
 * Revision 2.3  89/10/15  02:04:17  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:05:08  dlb
 * 	Cleanup.
 * 	[89/08/02            dlb]
 * 
 * Revision 2.1.1.3  89/08/02  21:46:28  dlb
 * 	Cleanup changes.
 * 	[89/08/02            dlb]
 * 
 * Revision 2.1.1.2  89/02/07  00:50:11  dlb
 * 	extern realhost declaration.
 * 	[89/02/03            dlb]
 * 
 */

/*
 *	kern/host.h
 *
 *	Definitions for host data structures.
 *
 */

#ifndef	_KERN_HOST_H_
#define _KERN_HOST_H_

#import <sys/port.h>

struct	host {
    	port_t	host_self;
	port_t	host_priv_self;
};

typedef struct host	*host_t;
typedef struct host	host_data_t;
typedef host_t		host_priv_t;

#define HOST_NULL	(host_t)0

extern host_data_t	realhost;

#endif	_KERN_HOST_H_
