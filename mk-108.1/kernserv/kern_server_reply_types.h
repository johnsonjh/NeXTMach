/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 24-May-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */

#ifndef _KERN_SERVER_REPLY_TYPES_
#define _KERN_SERVER_REPLY_TYPES_
#import <kern/xpr.h>
#import <kern/mach_types.h>

typedef port_name_t server_ref_t;
typedef char	panic_msg_t[256];
typedef char	macho_header_name_t[16];

/*
 * Log structure
 */
typedef struct xprbuf log_entry_t;
typedef log_entry_t *log_entry_array_t;
typedef struct {
	log_entry_t	*base;
	log_entry_t	*ptr;
	log_entry_t	*last;
	int		level;		// 0 == no logging
} log_t;

#endif _KERN_SERVER_REPLY_TYPES_

