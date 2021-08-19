/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 ****************************************************************
 * HISTORY
 * 13-Apr-90	Brian Pinkerton at NeXT
 *	Split structure definitions out of vnode_pager.c.
 *
 *  6-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Removed inode_pager_t structure declaration from this file...
 *	it should not be exported from the inode_pager implementation.
 *
 *  4-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added inode_pager_lookup_external.  Removed old history.
 *
 ****************************************************************
 */

#ifndef	_VNODE_PAGER_
#define	_VNODE_PAGER_	1

#import <sys/kern_return.h>
#import <sys/types.h>
#import <kern/lock.h>
#import <kern/queue.h>

kern_return_t	mach_swapon();

#ifdef	KERNEL

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <mach_xp.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/boolean.h>
#import <vm/vm_pager.h>

void		vnode_pager_init();

vm_pager_t	vnode_pager_setup();
boolean_t	vnode_has_page();
boolean_t	vnode_pager_active();

#if	MACH_XP
#import <sys/mfs.h>
#import <kern/task.h>

extern
task_t			vnode_pager_task;
extern
simple_lock_data_t	vnode_pager_init_lock;


void		vnode_pager();
boolean_t	vnode_pageout();
vm_pager_t	vnode_pager_setup();
#define		vnode_uncache(vp)	pager_cache(vm_object_lookup(vp->vm_info->pager), FALSE)

#else	MACH_XP
/*
 *  Vstructs are the internal (to us) description of a unit of backing store.
 *  The are the link between memory objects and the backing store they represent.
 *  For the vnode pager, backing store comes in two flavors: normal files and
 *  swap files.
 *
 *  For objects that page to and from normal files (e.g. objects that represent
 *  program text segments), we maintain some simple parameters that allow us to
 *  access the file's contents directly through the vnode interface.
 *
 *  Data for objects without associated vnodes is maintained in the swap files.
 *  Each object that uses one of these as backing store has a vstruct indicating
 *  the swap file of preference (vs_pf) and a mapping between contiguous object
 *  offsets and swap file offsets (vs_pmap).  Each entry in this mapping specifies
 *  the pager file to use, and the offset of the page in that pager file.  These
 *  mapping entries are of type pfMapEntry.
 */

/*
 * Pager file structure.  One per swap file.
 */
typedef struct pager_file {
	queue_chain_t	pf_chain;	/* link to other paging files */
	struct	vnode	*pf_vp;		/* vnode of paging file */
	u_int		pf_count;	/* Number of vstruct using this file */
	u_char		*pf_bmap; 	/* Map of used blocks */
	long		pf_npgs;	/* Size of file in pages */
	long		pf_pfree;	/* Number of unused pages */
	long		pf_lowat;	/* Low water page */
	long		pf_hipage;	/* Highest page allocated */
	char		*pf_name;	/* Filename of this file */
	boolean_t	pf_prefer;
	int		pf_index;	/* index into the pager_file array */
	lock_data_t	pf_lock;	/* Lock for alloc and dealloc */
} *pager_file_t;

#define	PAGER_FILE_NULL	(pager_file_t) 0
#define	MAXPAGERFILES 256


/*
 * Pager file data structures.
 */
typedef struct _pfMapEntry {
	unsigned int pagerFileIndex:8;	/* paging file this block is in */
	unsigned int pageOffset:24;	/* page number where block resides */
} pfMapEntry;

typedef enum {
		IS_INODE,	/* Local disk */
		IS_RNODE	/* NFS */
	} vpager_fstype;

/*
 *  Basic vnode pager structure.  One per object, backing-store pair.
 */
typedef struct vstruct {
	boolean_t	is_device;	/* Must be first - see vm_pager.h */
	pager_file_t	vs_pf;		/* Pager file this uses */
	pfMapEntry	**vs_pmap;	/* Map of pages into paging file */
	unsigned int
	/* boolean_t */	vs_swapfile:1;	/* vnode is a swapfile */
	short		vs_count;	/* use count */
	int		vs_size;	/* size of this chunk in pages*/
	struct vnode	*vs_vp;		/* vnode to page to */
} *vnode_pager_t;

#define	VNODE_PAGER_NULL	((vnode_pager_t) 0)



boolean_t	vnode_pagein();
boolean_t	vnode_pageout();
boolean_t	vnode_dealloc();
vm_pager_t	vnode_alloc();
void		vnode_uncache();

#endif	MACH_XP
#endif	KERNEL

#endif	_VNODE_PAGER_


