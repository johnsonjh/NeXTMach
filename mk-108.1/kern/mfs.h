/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 *  3-Aug-90  Doug Mitchell at NeXT
 *	Added prototypes for exported functions.
 *
 * 18-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Merged in with Mach 2.5 stuff.
 *
 *  9-Mar-88  John Seamons (jks) at NeXT
 *	Allocate vm_info structures from a zone.
 *
 * 11-Jun-87  William Bolosky (bolosky) at Carnegie-Mellon University
 *	Changed pager_id to pager.
 *
 * 30-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */
/*
 *	File:	mfs.h
 *	Author:	Avadis Tevanian, Jr.
 *	Copyright (C) 1987, Avadis Tevanian, Jr.
 *
 *	Header file for mapped file system support.
 *
 */ 

#ifndef	_KERN_MFS_H_
#define	_KERN_MFS_H_

#import <vm/vm_pager.h>
#import <kern/lock.h>
#import <kern/queue.h>
#import <kern/zalloc.h>
#import <vm/vm_object.h>
#import <sys/types.h>
#import <sys/vnode.h>

/*
 *	Associated with each mapped file is information about its
 *	corresponding VM window.  This information is kept in the following
 *	vm_info structure.
 */
struct vm_info {
	vm_pager_t	pager;		/* [external] pager */
	short		map_count;	/* number of times mapped */
	short		use_count;	/* number of times in use */
	vm_offset_t	va;		/* mapped virtual address */
	vm_size_t	size;		/* mapped size */
	vm_offset_t	offset;		/* offset into file at va */
	vm_size_t	vnode_size;	/* vnode size (not reflected in ip) */
	lock_data_t	lock;		/* lock for changing window */
	vm_object_t	object;		/* object [for KERNEL flushing] */
	queue_chain_t	lru_links;	/* lru queue links */
	struct ucred	*cred;		/* vnode credentials */
	int		error;		/* holds error codes */
	int		queued:1,	/* on lru queue? */
			dirty:1,	/* range needs flushing? */
			close_flush:1,	/* flush on close */
#ifdef	NeXT
			invalidate:1,	/* is mapping invalid? */
#endif	NeXT
			mapped:1;	/* mapped into KERNEL VM? */
};

#define VM_INFO_NULL	((struct vm_info *) 0)

struct zone	*vm_info_zone;

/*
 * exported primitives for loadable file systems.
 */
int vm_info_init(struct vnode *vp);
void vm_info_free(struct vnode *vp);
vm_size_t vm_get_vnode_size(struct vnode *vp);
void vm_set_vnode_size(struct vnode *vp, vm_size_t vnode_size);
void vm_set_close_flush(struct vnode *vp, boolean_t close_flush);
void mfs_uncache(struct vnode *vp);
int mfs_trunc(struct vnode *vp, int length);
void vm_set_error(struct vnode *vp, int error);

#endif	_KERN_MFS_H_

