/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm_object.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Virtual memory object module definitions.
 *
 * HISTORY
 * 13-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Reduce structure sizes by combining booleans into bit fields.
 *
 * 29-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Removed meaningless ancient history.
 *
 * 26-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added pager_creating field, to synchronize on-demand pager
 *	creation for internal objects.
 *
 * 21-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added vm_object_destroy() declaration; moved
 *	vm_object_pager_associated() declaration.
 *
 * 18-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added vm_object_enter() declaration for non-XP case.
 *
 * 17-Aug-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added lock debugging.  Corrected pager_cache macro.
 *
 * 14-Jul-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	CAN NEVER have bit fields at end of structure on ns32000.
 *
 * 25-Jun-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Replace vm_object_{cache,uncache} with macros calling pager_cache.
 *
 *  1-Jun-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH_XP: Added vm_object_t->pager_ready field.
 *
 * 12-Mar-87  David Golub (dbg) at Carnegie-Mellon University
 *	Changed pageout_in_progress to a counter ('paging_in_progress')
 *	to prevent an object with busy and 'fake' pages (intermediate
 *	state in pagein) from being clobbered.
 *
 *  6-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */

#ifndef	_VM_OBJECT_
#define	_VM_OBJECT_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_xp.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <kern/queue.h>
#import <kern/lock.h>
#import <sys/boolean.h>
#import <vm/vm_pager.h>
#import <sys/port.h>

/*
 *	Types defined:
 *
 *	vm_object_t		Virtual memory object.
 */

struct vm_object {
	queue_chain_t		memq;		/* Resident memory */
	queue_chain_t		object_list;	/* list of all objects */
	simple_lock_data_t	Lock;		/* Synchronization */
#if	VM_OBJECT_DEBUG
	int			LockHolder;
#endif	VM_OBJECT_DEBUG
	vm_size_t		size;		/* Object size */
	short			ref_count;	/* How many refs?? */
	short			resident_page_count;
						/* number of resident pages */
	struct vm_object	*copy;		/* Object that holds copies of
						   my changed pages */
	vm_pager_t		pager;		/* Where to get data */
	port_t			pager_request;	/* Where data comes back */
	port_t			pager_name;	/* How to identify region */
	vm_offset_t		paging_offset;	/* Offset into paging space */
	struct vm_object	*shadow;	/* My shadow */
	vm_offset_t		shadow_offset;	/* Offset in shadow */
	unsigned int
				paging_in_progress:16,
						/* Paging (in or out) - don't
						   collapse or destroy */
	/* boolean_t */		can_persist:1,	/* Keep when ref_count == 0? */
	/* boolean_t */		internal:1,	/* Kernel-created object */
	/* boolean_t */		pager_ready : 1,/* Pager fields been filled? */
	/* boolean_t */		pager_creating :1; /* Pager being created? */
	queue_chain_t		cached_list;	/* for persistence */
	vm_offset_t		last_alloc;	/* last allocation offset */
};

typedef struct vm_object	*vm_object_t;

struct vm_object_hash_entry {
	queue_chain_t		hash_links;	/* hash chain links */
	vm_object_t		object;		/* object we represent */
};

typedef struct vm_object_hash_entry	*vm_object_hash_entry_t;

#ifdef	KERNEL
queue_head_t	vm_object_cached_list;	/* list of objects persisting */
int		vm_object_cached;	/* size of cached list */
simple_lock_data_t	vm_cache_lock;	/* lock for object cache */

queue_head_t	vm_object_list;		/* list of allocated objects */
long		vm_object_count;	/* count of all objects */
simple_lock_data_t	vm_object_list_lock;
					/* lock for object list and count */

vm_object_t	kernel_object;		/* the single kernel object */

#define	vm_object_cache_lock()		simple_lock(&vm_cache_lock)
#define	vm_object_cache_unlock()	simple_unlock(&vm_cache_lock)
#endif	KERNEL

#define	VM_OBJECT_NULL		((vm_object_t) 0)

/*
 *	Declare procedures that operate on VM objects.
 */

void		vm_object_init ();
void		vm_object_terminate();
vm_object_t	vm_object_allocate();
void		vm_object_reference();
void		vm_object_deallocate();
void		vm_object_pmap_copy();
void		vm_object_pmap_remove();
void		vm_object_page_remove();
void		vm_object_shadow();
void		vm_object_copy();
void		vm_object_collapse();
vm_object_t	vm_object_lookup();
port_t		vm_object_name();
#if	MACH_XP
vm_object_t	vm_object_enter();
void		vm_object_remove();
void		vm_object_pager_create();
void		vm_object_destroy();
#else	MACH_XP
void		vm_object_enter();
void		vm_object_setpager();
#endif	MACH_XP
#define		vm_object_cache(pager)		pager_cache(vm_object_lookup(pager),TRUE)
#define		vm_object_uncache(pager)	pager_cache(vm_object_lookup(pager),FALSE)

void		vm_object_cache_clear();
void		vm_object_print();

port_t		vm_object_request_port();
vm_object_t	vm_object_request_object();

#if	VM_OBJECT_DEBUG
#define	vm_object_lock_init(object)	{ simple_lock_init(&(object)->Lock); (object)->LockHolder = 0; }
#define	vm_object_lock(object)		{ simple_lock(&(object)->Lock); (object)->LockHolder = (int) current_thread(); }
#define	vm_object_unlock(object)	{ (object)->LockHolder = 0; simple_unlock(&(object)->Lock); }
#define	vm_object_lock_try(object)	(simple_lock_try(&(object)->Lock) ? ( ((object)->LockHolder = (int) current_thread()) , TRUE) : FALSE)
#define	vm_object_sleep(event, object, interruptible) \
					{ (object)->LockHolder = 0; thread_sleep((event), &(object)->Lock, (interruptible)); }
#else	VM_OBJECT_DEBUG
#define	vm_object_lock_init(object)	simple_lock_init(&(object)->Lock)
#define	vm_object_lock(object)		simple_lock(&(object)->Lock)
#define	vm_object_unlock(object)	simple_unlock(&(object)->Lock)
#define	vm_object_lock_try(object)	simple_lock_try(&(object)->Lock)
#define	vm_object_sleep(event, object, interruptible) \
					thread_sleep((event), &(object)->Lock, (interruptible))
#endif	VM_OBJECT_DEBUG

#endif	_VM_OBJECT_
