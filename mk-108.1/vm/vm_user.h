/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_user.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Kernel memory management definitions.
 *
 * HISTORY
 * 15-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Simplified includes.
 *
 * 14-Oct-85  Michael Wayne Young (mwyoung) at Carnegie-Mellon University
 *	Created header file.
 */

#ifndef	_VM_USER_
#define	_VM_USER_

#import <sys/kern_return.h>

kern_return_t	vm_allocate();
kern_return_t	vm_deallocate();
kern_return_t	vm_inherit();
kern_return_t	vm_protect();
kern_return_t	vm_statistics();

#endif	_VM_USER_
