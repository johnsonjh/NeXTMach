/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	std_types.h,v $
 * 21-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Changed #import to #include except for boolean.h.  This needs
 *	to be an include so that TRUE and FALSE will be defined after
 *	we define EXPORT_BOOLEAN.  Otherwise, if boolean was included
 *	before this file we would never see the definitions.
 *
 * Revision 2.4  89/03/09  20:23:16  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/25  18:40:23  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  89/01/15  16:31:59  rpd
 * 	Moved from kern/ to mach/.
 * 	[89/01/15  14:34:14  rpd]
 * 
 * Revision 2.2  89/01/12  07:59:07  rpd
 * 	Created.
 * 	[89/01/12  04:15:40  rpd]
 * 
 */
/*
 *	Mach standard external interface type definitions.
 *
 */

#ifndef	_MACH_STD_TYPES_H_
#define _MACH_STD_TYPES_H_

#define EXPORT_BOOLEAN

#include <sys/boolean.h>
#import <sys/kern_return.h>
#import <sys/port.h>
#import <machine/vm_types.h>


typedef	vm_offset_t	pointer_t;

#endif	_MACH_STD_TYPES_H_


