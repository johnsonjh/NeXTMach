/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/vm_statistics.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young,
 *				David Golub
 *
 *	Virtual memory statistics structure.
 *
 ************************************************************************
 * HISTORY
 * 30-Sep-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Changed "reclaim" to "inactive."
 *
 * 22-Aug-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Made vm_stat structure kernel-only.
 *
 * 22-May-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Defined vm_statistics_data_t as a real typedef so that
 *	MatchMaker can deal with it.
 *
 * 14-Feb-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 *
 ************************************************************************
 */

#ifndef	_VM_STATISTICS_
#define	_VM_STATISTICS_

struct vm_statistics {
	long	pagesize;		/* page size in bytes */
	long	free_count;		/* # of pages free */
	long	active_count;		/* # of pages active */
	long	inactive_count;		/* # of pages inactive */
	long	wire_count;		/* # of pages wired down */
	long	zero_fill_count;	/* # of zero fill pages */
	long	reactivations;		/* # of pages reactivated */
	long	pageins;		/* # of pageins */
	long	pageouts;		/* # of pageouts */
	long	faults;			/* # of faults */
	long	cow_faults;		/* # of copy-on-writes */
	long	lookups;		/* object cache lookups */
	long	hits;			/* object cache hits */
};

typedef struct vm_statistics	*vm_statistics_t;
typedef struct vm_statistics	vm_statistics_data_t;

#ifdef	KERNEL
vm_statistics_data_t	vm_stat;
#endif	KERNEL

/*
 *	Each machine dependent implementation is expected to
 *	keep certain statistics.  They may do this anyway they
 *	so choose, but are expected to return the statistics
 *	in the following structure.
 */

struct pmap_statistics {
	long		resident_count;	/* # of pages mapped (total)*/
	long		wired_count;	/* # of pages wired */
};

typedef struct pmap_statistics	*pmap_statistics_t;
#endif	_VM_STATISTICS_
