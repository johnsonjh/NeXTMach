/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	mig_user_hack.h,v $
 * Revision 2.2  89/04/08  23:39:58  rpd
 * 	Created.
 * 	[89/04/08  22:59:45  rpd]
 * 
 */

#ifndef	_KERN_MIG_USER_HACK_H_
#define _KERN_MIG_USER_HACK_H_

/*
 * This file is meant to be imported with
 *	uimport <kern/mig_user_hack.h>;
 * by those interfaces for which the kernel (but not builtin tasks)
 * is the client.  See memory_object.defs and memory_object_default.defs.
 * It has the hackery necessary to make Mig-generated user-side stubs
 * usable by the kernel.
 *
 * The uimport places a
 *	#include <kern/mig_user_hack.h>
 * in the generated header file for the interface, so any file which
 * includes the interface header file will get these definitions,
 * not just the user stub file like we really want.
 */

#define msg_send	msg_send_from_kernel

#endif	_KERN_MIG_USER_HACK_H_
