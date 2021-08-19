/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 *	File:	vm/vm_fault.h
 *
 *	Page fault handling module declarations.
 *
 * HISTORY
 * 11-Feb-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Created.
 *
 */

#ifndef	_VM_FAULT_
#define	_VM_FAULT_	1

#import <sys/kern_return.h>

kern_return_t	vm_fault();
void		vm_fault_wire();
void		vm_fault_unwire();
void		vm_fault_copy_entry();

#endif	_VM_FAULT_
