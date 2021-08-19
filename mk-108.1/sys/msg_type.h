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
 * $Log:	msg_type.h,v $
 * Revision 2.4  89/03/09  20:22:18  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:39:26  gm0w
 * 	Changes for cleanup.
 * 
 *  4-Mar-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added MSG_TYPE_RPC.
 *
 *  22-Dec-86 Mary Thompson
 *	defined MSG_TYPE_CAMELOT, and MSG_TYPE_ENCRYPTED
 *
 */
/*
 *    This file defines user msg types that may be ored into
 *    the msg_type field in a msg header. Values 0-5 are reserved
 *    for use by the kernel and are defined in message.h. 
 *
 */

#ifndef	_SYS_MSG_TYPE_H_
#define _SYS_MSG_TYPE_H_

#define MSG_TYPE_CAMELOT	(1 << 6)
#define MSG_TYPE_ENCRYPTED	(1 << 7)
#define MSG_TYPE_RPC		(1 << 8)	/* Reply expected */

#import <sys/message.h>

#endif	_SYS_MSG_TYPE_H_

