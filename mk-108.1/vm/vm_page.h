/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_page.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Resident memory system definitions.
 *
 * HISTORY
 *  4-Feb-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added vm_page_t->error field; documented a few more things.
 *
 * 26-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Eliminate vm_page_t->fake field.
 *	Remove old history.
 *
 * 18-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added vm_page_t->unlock_request field.
 *
 * 11-Jan-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Add "interruptible" argument to PAGE_ASSERT_WAIT.
 *
 */

#ifndef	_VM_PAGE_
#define	_VM_PAGE_

#import <sys/boolean.h>
#import <kern/queue.h>
#import <vm/vm_param.h>
#import <vm/vm_object.h>
#import <vm/vm_prot.h>
#import <kern/lock.h>
#import <kern/sched_prim.h>

/*
 *	Management of resident (logical) pages.
 *
 *	A small structure is kept for each resident
 *	page, indexed by page number.  Each structure
 *	is an element of several lists:
 *
 *		A hash table bucket used to quickly
 *		perform object/offset lookups
 *
 *		A list of all pages for a given object,
 *		so they can be quickly deactivated at
 *		time of deallocation.
 *
 *		An ordered list of pages due for pageout.
 *
 *	In addition, the structure contains the object
 *	and offset to which this page belongs (for pageout),
 *	and sundry status bits.
 *
 *	Fields in this structure are locked either by the lock on the
 *	object that the page belongs to (O) or by the lock on the page
 *	queues (P).
 */

struct vm_page {
	queue_chain_t	pageq;		/* queue info for FIFO
					 * queue or free list (P) */
	queue_chain_t	hashq;		/* hash table links (O)*/
	queue_chain_t	listq;		/* all pages in same object (O)*/

	vm_object_t	object;		/* which object am I in (O,P)*/
	vm_offset_t	offset;		/* offset into that object (O,P) */

	unsigned int	wire_count:16,	/* how many wired down maps use me?
					   (P) */
	/* boolean_t */	inactive:1,	/* page is in inactive list (P) */
			active:1,	/* page is in active list (P) */
			laundry:1,	/* page is being cleaned now (P)*/
#if	NeXT
			free:1,		/* page is in free list (P) */
#endif	NeXT
			:0;		/* (force to 'long' boundary) */
#ifdef	ns32000
	int		pad;		/* extra space for ns32000 bit ops */
#endif	ns32000
	boolean_t	clean;		/* page has not been modified */
	unsigned int
	/* boolean_t */	busy:1,		/* page is in transit (O) */
			wanted:1,	/* someone is waiting for page (O) */
			tabled:1,	/* page is in VP table (O) */
			copy_on_write:1,/* Page must be copied before being
					 * changed (O) */
			fictitious:1,	/* Physical page doesn't exist - and -
					 *  page should not be returned to
					 *  the free list (O) */
			absent:1,	/* Data has been requested, but is
					 *  not yet available (O) */
			error:1,	/* Data manager was unable to provide
					 *  data due to error (O) */
			:0;

	vm_offset_t	phys_addr;	/* Physical address of page, passed
					 *  to pmap_enter */
	vm_prot_t	page_lock;	/* Uses prohibited by data manager (O) */
	vm_prot_t	unlock_request;	/* Outstanding unlock request (O) */
};

typedef struct vm_page	*vm_page_t;

#define	VM_PAGE_NULL		((vm_page_t) 0)

#if	VM_PAGE_DEBUG
#define	VM_PAGE_CHECK(mem) { \
		if ( (((unsigned int) mem) < ((unsigned int) &vm_page_array[0])) || \
		     (((unsigned int) mem) > ((unsigned int) &vm_page_array[last_page-first_page])) || \
		     (mem->active && mem->inactive) \
		    ) panic("vm_page_check: not valid!"); \
		}
#else	VM_PAGE_DEBUG
#define	VM_PAGE_CHECK(mem)
#endif	VM_PAGE_DEBUG

#ifdef	KERNEL
/*
 *	Each pageable resident page falls into one of three lists:
 *
 *	free	
 *		Available for allocation now.
 *	inactive
 *		Not referenced in any map, but still has an
 *		object/offset-page mapping, and may be dirty.
 *		This is the list of pages that should be
 *		paged out next.
 *	active
 *		A list of pages which have been placed in
 *		at least one physical map.  This list is
 *		ordered, in LRU-like fashion.
 */

extern
queue_head_t	vm_page_queue_free;	/* memory free queue */
extern
queue_head_t	vm_page_queue_active;	/* active memory queue */
extern
queue_head_t	vm_page_queue_inactive;	/* inactive memory queue */

#if	NeXT
typedef struct mem_region {
	vm_page_t	vm_page_array;	/* First resident page in table */
	long		first_page;	/* first physical page number */
					/* ... represented in vm_page_array */
	long		last_page;	/* last physical page number */
					/* ... represented in vm_page_array */
					/* [INCLUSIVE] */
	int		num_pages;	/* last_page - first_page */
	vm_offset_t	base_phys_addr;	/* fixed base of region for dump() */
	vm_offset_t	first_phys_addr;/* physical address for first_page */
	vm_offset_t	last_phys_addr;	/* physical address for last_page */
	int		region_type;	/* type of memory in region */
#define	REGION_MEM	0		/* main memory */
#define	REGION_DEV	1		/* device space */
} *mem_region_t;

extern		struct mem_region mem_region[];
int		num_regions;
#else	NeXT
extern
vm_page_t	vm_page_array;		/* First resident page in table */
extern
long		first_page;		/* first physical page number */
					/* ... represented in vm_page_array */
extern
long		last_page;		/* last physical page number */
					/* ... represented in vm_page_array */
					/* [INCLUSIVE] */
extern
vm_offset_t	first_phys_addr;	/* physical address for first_page */
extern
vm_offset_t	last_phys_addr;		/* physical address for last_page */
#endif	NeXT

extern
int	vm_page_free_count;	/* How many pages are free? */
extern
int	vm_page_active_count;	/* How many pages are active? */
extern
int	vm_page_inactive_count;	/* How many pages are inactive? */
extern
int	vm_page_wire_count;	/* How many pages are wired? */
extern
int	vm_page_free_target;	/* How many do we want free? */
extern
int	vm_page_free_min;	/* When to wakeup pageout */
extern
int	vm_page_inactive_target;/* How many do we want inactive? */
extern
int	vm_page_free_reserved;	/* How many pages reserved to do pageout */
extern
int	vm_page_laundry_count;	/* How many pages being laundered? */

#define VM_PAGE_TO_PHYS(entry)	((entry)->phys_addr)

#if	NeXT
#define IS_VM_PHYSADDR(pa) \
		(vm_which_region (pa) == -1? FALSE: TRUE)

#define PHYS_TO_VM_PAGE(pa) \
		vm_region_to_vm_page (pa)
#else	NeXT
#define IS_VM_PHYSADDR(pa) \
		((pa) >= first_phys_addr && (pa) <= last_phys_addr)

#define PHYS_TO_VM_PAGE(pa) \
		(&vm_page_array[atop(pa) - first_page ])
#endif	NeXT

extern
simple_lock_data_t	vm_page_queue_lock;	/* lock on active and inactive
						   page queues */
extern
simple_lock_data_t	vm_page_queue_free_lock;
						/* lock on free page queue */
vm_offset_t	vm_page_startup();
vm_page_t	vm_page_lookup(vm_object_t, vm_offset_t);
vm_page_t	vm_page_alloc(vm_object_t, vm_offset_t);
void		vm_page_init(vm_page_t, vm_object_t, vm_offset_t, vm_offset_t);
void		vm_page_free(vm_page_t);
#if	NeXT
void		vm_page_addfree(vm_page_t);
#endif	NeXT
void		vm_page_activate(vm_page_t);
void		vm_page_deactivate(vm_page_t);
void		vm_page_rename(vm_page_t, vm_object_t, vm_offset_t);
void		vm_page_insert(vm_page_t, vm_object_t, vm_offset_t);
void		vm_page_remove(vm_page_t);
#if	NeXT
vm_page_t	vm_region_to_vm_page();
#endif	NeXT

boolean_t	vm_page_zero_fill(vm_page_t);
void		vm_page_copy(vm_page_t, vm_page_t);

void		vm_page_wire(vm_page_t);
void		vm_page_unwire(vm_page_t);

void		vm_set_page_size();

/*
 *	Functions implemented as macros
 */

#import <kern/sched_prim.h>	/* definitions of wait/wakeup */

#define PAGE_ASSERT_WAIT(m, interruptible)	{ \
				(m)->wanted = TRUE; \
				assert_wait((int) (m), (interruptible)); \
			}

#define PAGE_WAKEUP(m)	{ \
				(m)->busy = FALSE; \
				if ((m)->wanted) { \
					(m)->wanted = FALSE; \
					thread_wakeup((int) (m)); \
				} \
			}

#define	VM_PAGE_FREE(p) { \
		vm_page_lock_queues();		\
		vm_page_free(p);		\
		vm_page_unlock_queues();	\
}

#define	vm_page_lock_queues()	simple_lock(&vm_page_queue_lock)
#define	vm_page_unlock_queues()	simple_unlock(&vm_page_queue_lock)

#define vm_page_set_modified(m)	{ (m)->clean = FALSE; }
#endif	KERNEL
#endif	_VM_PAGE_

