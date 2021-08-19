/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	zone_info.h,v $
 * Revision 2.2  89/05/06  12:36:08  rpd
 * 	Created.
 * 	[89/05/06  12:35:19  rpd]
 * 
 */ 

#ifndef	_KERN_ZONE_INFO_H_
#define _KERN_ZONE_INFO_H_

#import <sys/boolean.h>
#import <machine/vm_types.h>

/*
 *	Remember to update the mig type definition
 *	in mach_debug_types.defs when adding/removing fields.
 */

#define ZONE_NAME_MAX_LEN		80

typedef struct zone_name {
	char		name[ZONE_NAME_MAX_LEN];
} zone_name_t;

typedef zone_name_t *zone_name_array_t;


typedef struct zone_info {
	int		count;		/* Number of elements used now */
	vm_size_t	cur_size;	/* current memory utilization */
	vm_size_t	max_size;	/* how large can this zone grow */
	vm_size_t	elem_size;	/* size of an element */
	vm_size_t	alloc_size;	/* size used for more memory */
	boolean_t	pageable;	/* zone pageable? */
	boolean_t	sleepable;	/* sleep if empty? */
	boolean_t	exhaustible;	/* merely return if empty? */
} zone_info_t;

typedef zone_info_t *zone_info_array_t;

#endif	_KERN_ZONE_INFO_H_

