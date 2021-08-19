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
 * $Log:	kern_return.h,v $
 * Revision 2.8  89/10/11  14:37:06  dlb
 * 	Add KERN_ABORTED for internal use only.
 * 	[89/03/07            dlb]
 * 
 * Revision 2.7  89/03/09  20:20:17  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  18:13:36  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.5  89/02/07  00:52:16  mwyoung
 * Relocated from sys/kern_return.h
 * 
 * Revision 2.4  88/08/24  02:31:47  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:15:07  mwyoung]
 * 
 * Revision 2.3  88/07/20  16:48:31  rpd
 * Added KERN_NAME_EXISTS.
 * Added KERN_ALREADY_IN_SET, KERN_NOT_IN_SET.
 * Made comments legible.
 * 
 *  3-Feb-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added memory management error conditions.
 *	Documented.
 *
 * 23-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Deleted kern_return_t casts on error codes so that they may be
 *	used in assembly code.
 *
 * 17-Sep-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */
/*
 *	File:	h/kern_return.h
 *	Author:	Avadis Tevanian, Jr.
 *	Copyright (C) 1985, Avadis Tevanian, Jr.
 *
 *	Kernel return codes.
 *
 */

#ifndef	_SYS_KERN_RETURN_H_
#define _SYS_KERN_RETURN_H_

#import <machine/kern_return.h>

#define KERN_SUCCESS			0

#define KERN_INVALID_ADDRESS		1
		/* Specified address is not currently valid.
		 */

#define KERN_PROTECTION_FAILURE		2
		/* Specified memory is valid, but does not permit the
		 * required forms of access.
		 */

#define KERN_NO_SPACE			3
		/* The address range specified is already in use, or
		 * no address range of the size specified could be
		 * found.
		 */

#define KERN_INVALID_ARGUMENT		4
		/* The function requested was not applicable to this
		 * type of argument, or an argument
		 */

#define KERN_FAILURE			5
		/* The function could not be performed.  A catch-all.
		 */

#define KERN_RESOURCE_SHORTAGE		6
		/* A system resource could not be allocated to fulfill
		 * this request.  This failure may not be permanent.
		 */

#define KERN_NOT_RECEIVER		7
		/* The task in question does not hold receive rights
		 * for the port argument.
		 */

#define KERN_NO_ACCESS			8
		/* Bogus access restriction.
		 */

#define KERN_MEMORY_FAILURE		9
		/* During a page fault, the target address refers to a
		 * memory object that has been destroyed.  This
		 * failure is permanent.
		 */

#define KERN_MEMORY_ERROR		10
		/* During a page fault, the memory object indicated
		 * that the data could not be returned.  This failure
		 * may be temporary; future attempts to access this
		 * same data may succeed, as defined by the memory
		 * object.
		 */

#define KERN_ALREADY_IN_SET		11
		/* The port argument is already a member of a set.
		 */

#define KERN_NOT_IN_SET			12
		/* The port argument is not a member of a set.
		 */

#define KERN_NAME_EXISTS		13
		/* The task already has a translation for the name.
		 */

#define KERN_ABORTED			14
		/* The operation was aborted.  Ipc code will
		 * catch this and reflect it as a message error.
		 */

#endif	_SYS_KERN_RETURN_H_

