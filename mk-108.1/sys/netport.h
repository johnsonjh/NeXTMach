/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	netport.h,v $
 * Revision 2.2  89/03/10  13:42:27  rpd
 * 	Created from kern/ipc_netport.h.
 * 
 */

#ifndef	_SYS_NETPORT_H_
#define _SYS_NETPORT_H_

typedef unsigned long	netaddr_t;

/*
 * Network Port structure.
 */
typedef struct {
    long	np_uid_high;
    long	np_uid_low;
} np_uid_t;

typedef struct {
    netaddr_t	np_receiver;
    netaddr_t	np_owner;
    np_uid_t	np_puid;
    np_uid_t	np_sid;
} network_port_t;

#endif	_SYS_NETPORT_H_

