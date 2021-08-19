/* 
 * Mach Operating System
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	memory_object.h
 *	Author:	Michael Wayne Young
 *
 *	External memory management interface definition.
 */
/*
 * HISTORY
 * $Log:
 */

#ifndef	_MEMORY_OBJECT_
#define	_MEMORY_OBJECT_	1

/*
 *	User-visible types used in the external memory
 *	management interface:
 */

#import <sys/port.h>

typedef	port_t		memory_object_t;
					/* Represents a memory object ... */
					/*  Used by user programs to specify */
					/*  the object to map; used by the */
					/*  kernel to retrieve or store data */

typedef	port_t		memory_object_control_t;
					/* Provided to a memory manager; ... */
					/*  used to control a memory object */

typedef	port_t		memory_object_name_t;
					/* Used to describe the memory ... */
					/*  object in vm_regions() calls */

typedef	int		memory_object_copy_strategy_t;
					/* How memory manager handles copy: */
#define		MEMORY_OBJECT_COPY_NONE		0
					/* ... No special support */
#define		MEMORY_OBJECT_COPY_CALL		1
					/* ... Make call on memory manager */
#define		MEMORY_OBJECT_COPY_DELAY 	2
					/* ... Memory manager doesn't ... */
					/*     change data externally. */
						

#define		MEMORY_OBJECT_NULL	PORT_NULL

/* Obsolete forms of the above names: */
#ifndef	KERNEL
typedef	port_t		paging_object_t;
typedef	port_t		vm_pager_request_t;
typedef	port_t		vm_pager_t;
#define	vm_pager_null	((vm_pager_t) 0)
#endif	KERNEL

#ifdef	KERNEL
memory_object_t	memory_manager_default;
#endif	KERNEL
#endif	_MEMORY_OBJECT_
