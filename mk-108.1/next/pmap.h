/* 
 * Copyright (c) 1987, 1988, 1989, 1990 NeXT, Inc.
 */
/* 
 * HISTORY
 * 06-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Defined PMAP_CONTEXT() macro.  (needed by kern/sched_prim).
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Defined all cache modes used by the page table entries.
 *
 */ 

#ifndef	_PMAP_MACHINE_
#define	_PMAP_MACHINE_

#ifndef	ASSEMBLER
#import <sys/types.h>
#import <kern/zalloc.h>
#import <kern/lock.h>
#import <next/vm_param.h>
#import <next/mmu.h>
#import <vm/vm_statistics.h>

/* first level page table entry */
struct pt1_030_entry {
u_long		lower:1,
		limit:15,
		:7,
		super:1,
		:4,
		refer:1,		/* entry has been used */
		write_prot:1,		/* write protection */
		desctype:2;		/* descriptor type */
u_long		l2ptr:28,		/* pointer to level 2 table */
		:4;
};

#define	PT1_030_SHIFT	3
#define	PT1_030_L2PTR	4

struct pt1_040_entry {
u_long		l2ptr:23,		/* pointer to level 2 table */
		: 5,
		refer:1,		/* entry has been used */
		write_prot:1,		/* write protection */
		desctype:2;		/* descriptor type */
};

#define	PT1_040_SHIFT	2
#define	PT1_040_L2PTR	9

union pt1 {
	struct pt1_030_entry	pt1_030;
	struct pt1_040_entry	pt1_040;
};
typedef	union pt1	pt1_t;

/* fields that occupy similar bit positions in both page table types */
#define	pt1_lower	pt1_030.lower
#define	pt1_limit	pt1_030.limit
#define	pt1_refer	pt1_030.refer
#define	pt1_write_prot	pt1_030.write_prot
#define	pt1_desctype	pt1_030.desctype

#define	phys_to_l2ptr(p)	((int)(p) >> NeXT_pt1_l2ptr)
#define	l2ptr_to_phys(p)	((int)(p) << NeXT_pt1_l2ptr)

/* second level page table entry (68040 only) */
struct pt2_040_entry {
u_long		pt2_l3ptr:25,		/* pointer to level 3 table */
		: 3,
		pt2_refer:1,		/* entry has been used */
		pt2_write_prot:1,	/* write protection */
		pt2_desctype:2;		/* descriptor type */
};
typedef struct pt2_040_entry pt2_t;

#define	PT2_040_SHIFT	2
#define	PT2_040_L3PTR	7

#define	phys_to_l3ptr(p)	((int)(p) >> NeXT_pt2_l3ptr)
#define	l3ptr_to_phys(p)	((int)(p) << NeXT_pt2_l3ptr)

/* highest level page table entry */
struct pt2_030_entry {
u_long		pfn:24,			/* page frame number */
		reserved:1,		/* reserved */
		cm:1,			/* cache mode */
#define	CM_030_CACHED		0
#define	CM_030_CACHE_INH	1
		wired:1,		/* wired down (software-only field) */
		modify:1,		/* page has been modified */
		refer:1,		/* page has been referenced (used) */
		write_prot:1,		/* write protection */
		desctype:2;		/* descriptor type */
};

#define	PT2_030_SHIFT	2
#define	PT2_030_PFN	8

struct pt3_040_entry {
u_long		pfn:20,			/* page frame number */
		wired:1,		/* wired down (software-only field) */
		global:1,		/* global (kernel) pte */
		u1:1,			/* user pte attribute */
		reserved:1,		/* reserved */
		super:1,		/* super mode access only */
		cm:2,			/* cache mode */
#define	CM_040_WRITETHRU	0
#define	CM_040_COPYBACK		1	/* copyback mode */
#define	CM_040_INH_SERIAL	2	/* serialized mode so devices will work */
#define	CM_040_INH_NONSERIAL	3
		modify:1,		/* page has been modified */
		refer:1,		/* page has been referenced (used) */
		write_prot:1,		/* write protection */
		desctype:2;		/* descriptor type */
};

#define	PT3_040_SHIFT	2
#define	PT3_040_PFN	12

/* note: significant assumption that pte is same size for both processors */
union pte {
	struct pt2_030_entry	pte_030;
	struct pt3_040_entry	pte_040;
	u_long			bits;
};
typedef	union pte	pte_t;

/* fields that occupy the same bit positions in both pte types */
#define	pte_wired	pte_030.wired
#define	pte_modify	pte_030.modify
#define	pte_refer	pte_030.refer
#define	pte_write_prot	pte_030.write_prot
#define	pte_desctype	pte_030.desctype

#define	phys_to_pfn(p)	((int)(p) >> NeXT_pte_pfn)
#define	pfn_to_phys(p)	((int)(p) << NeXT_pte_pfn)

/* residency codes */
#define	PT1_UPPER_NULL		-5	/* order sensitive */
#define	PT1_ENTRY_NULL		-4
#define	PT2_ENTRY_NULL		-3
#define	PTE_UPPER_NULL		-2
#define	PTE_LOWER_NULL		-1
#define	PTE_ENTRY_NULL		0

/* Descriptor type */
#define	DT_INVALID	0		/* out-of-bounds or not resident */
#define	DT_PTE		1		/* valid page table entry */
#define	DT_SHORT	2		/* valid short format page table */
#define	DT_LONG		3		/* valid long format page table */

struct pmap {
	struct mmu_rp	rp;		/* root pointer of pte map */
	pt1_t		*pt1;		/* address of level 1 page table */
	int		ref_count;	/* reference count */
	decl_simple_lock_data(, lock)	/* lock on map */
	struct pmap_statistics stats;	/* map statistics */
};

typedef struct pmap	*pmap_t;
#define	PMAP_NULL	((pmap_t) 0)

/*
 *	Machine dependent routines that are used only for NeXT.
 */
pte_t		*pmap_pte(), *pmap_pte_valid();
vm_offset_t	pmap_resident_extract();

/*
 *	For each vm_page_t, there is a list of all currently
 *	valid virtual mappings of that page.  An entry is
 *	a pv_entry_t; the list is the pv_table.  [Indexing on the
 *	pv_table is identical to the vm_page_array.]
 */
typedef struct pv_entry {
	struct pv_entry	*next;		/* next pv_entry */
	pmap_t		pmap;		/* pmap where mapping lies */
	vm_offset_t	va;		/* virtual address for mapping */
} *pv_entry_t;

#define	PV_ENTRY_NULL	((pv_entry_t) 0)

#ifdef	KERNEL
pv_entry_t	pv_head_table;		/* array of entries, one per page */
zone_t		pv_list_zone;		/* zone of pv_entry structures */

/*
 *	Each entry in the pv_head_table is locked by a bit in the 
 *	pv_lock_table.  The lock bits are accessed by the physical
 *	address of the page they lock.
 */
#define	pai_to_pvh(pai)		(&pv_head_table[pai])

char	*pv_lock_table;		/* pointer to array of bits */
#define	pv_lock_table_size(n)	(((n)+BYTE_SIZE-1)/BYTE_SIZE)
#if	!defined(KERNEL_FEATURES)
#import <cpus.h>
#else	!defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	!defined(KERNEL_FEATURES)
#if	NCPUS > 1
/* FIXME: replace while loop with something better? */
#define LOCK_PVH(pai) { while (bbssi (pai, pv_lock_table)); }
#define UNLOCK_PVH(pai) { bbcci (pai, pv_lock_table); }
#else	NCPUS > 1
#define LOCK_PVH(pai)
#define UNLOCK_PVH(pai)
#endif	NCPUS > 1
#endif	KERNEL

/*
 *	Macros for speed.
 */
#define	PMAP_ACTIVATE(pmap, th, cpu) \
	pmove_crp (&pmap->rp);

#define	PMAP_DEACTIVATE(pmap, th, cpu)

/*
 *	PMAP_CONTEXT: update any thread-specific hardware context that
 *		is managed by pmap module.
 */

#define PMAP_CONTEXT(pmap, thread)

#define	pmap_resident_count(pmap)	((pmap)->stats.resident_count)

#define		PMAP_TT_DISABLE		0
#define		PMAP_TT_ENABLE		1
#define		PMAP_TT_NON_CACHEABLE	0
#define		PMAP_TT_CACHEABLE	1
void		pmap_tt();

#endif	!ASSEMBLER

#endif	_PMAP_MACHINE_




