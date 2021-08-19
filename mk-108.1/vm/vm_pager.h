/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm_pager.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Pager routine interface definition
 *
 * HISTORY
 * 31-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Be careful about what's defined under non-KERNEL compilation.
 *
 * 24-Feb-88  David Golub (dbg) at Carnegie-Mellon University
 *	Defined pager_return_t (non-XP systems only).
 *
 *  8-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Only include <sys/boolean.h> if not MACH_XP.
 *	Purged history.  Participants so far: (mwyoung, avie, dbg, bolosky).
 */

#ifndef	_VM_PAGER_
#define	_VM_PAGER_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_xp.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/port.h>

#if	MACH_XP || !defined(KERNEL)
typedef	port_t		vm_pager_t;
#define	vm_pager_null	(PORT_NULL)

#else	MACH_XP || !defined(KERNEL)
#import <sys/boolean.h>
/*
 *	For non-XP, a pager is a pointer to either an istruct or a
 *	dev_pager.  These two structures share the "is_device" boolean
 *	as the first field, which vm uses to dispatch to the correct
 *	routine.  The "pager_struct" defined here is only a convenience 
 *	for vm.
 */
struct	pager_struct {
		boolean_t	is_device;
	};
typedef	struct pager_struct *vm_pager_t;
#define	vm_pager_null	((vm_pager_t) 0)

#endif	MACH_XP || !defined(KERNEL)

typedef	port_t		paging_object_t;
typedef	port_t		vm_pager_request_t;

#define	PAGER_SUCCESS		0	/* page read or written */
#define	PAGER_ABSENT		1	/* pager does not have page */
#define	PAGER_ERROR		2	/* pager unable to read or write page */

typedef	int		pager_return_t;

#ifdef	KERNEL
#if	MACH_XP
extern
vm_pager_t	vm_pager_default;
void		vm_pager_init();

#define		vm_pager_has_page(pager,offset)	(vnode_has_page((pager),(offset)))
#else	MACH_XP
boolean_t	vm_pager_alloc();
boolean_t	vm_pager_dealloc();
pager_return_t	vm_pager_get();
pager_return_t	vm_pager_put();
boolean_t	vm_pager_has_page();
#endif	MACH_XP
#endif	KERNEL

#endif	_VM_PAGER_
