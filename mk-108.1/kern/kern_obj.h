/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	kern_obj.h,v $
 * Revision 2.9  89/03/09  20:13:20  rpd
 * 	More cleanup.
 * 
 * Revision 2.8  89/02/25  18:05:03  gm0w
 * 	Kernel code cleanup.	
 * 	Put all the macros under #ifdef KERNEL
 * 	[89/02/15            mrt]
 * 
 * Revision 2.7  89/02/07  01:02:04  mwyoung
 * Relocated from sys/kern_obj.h
 * 
 * Revision 2.6  89/01/15  16:33:50  rpd
 * 	Removed the obj_traversing field.
 * 	Use decl_simple_lock_data.
 * 	[89/01/15  15:15:06  rpd]
 * 
 * Revision 2.5  88/10/18  03:18:06  mwyoung
 * 	Use MACRO_BEGIN, MACRO_END instead.
 * 	[88/10/11            mwyoung]
 * 	
 * 	lint: Use "NEVER" from <kern/macro_help.h> to avoid constants in
 * 	do/while loops.
 * 	[88/10/08            mwyoung]
 * 
 * Revision 2.4  88/09/25  22:16:04  rpd
 * 	Added KERN_OBJ_INVALID.
 * 	[88/09/24  18:08:54  rpd]
 * 
 * Revision 2.3  88/08/24  02:30:48  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:14:33  mwyoung]
 * 
 * Revision 2.2  88/07/20  16:47:08  rpd
 * Added obj_translations, obj_traversing fields.  They are used when multiple
 * tasks have capabilities for an object.  More assertions in the macros.
 * Created.  Extracts common fields from ports and sets.
 * 
 */
/*
 * Common fields for dynamically managed kernel objects
 * for which tasks have capabilities.
 *
 */

#ifndef	_KERN_KERN_OBJ_H_
#define _KERN_KERN_OBJ_H_

#import <sys/boolean.h>
#import <kern/lock.h>
#import <kern/zalloc.h>
#import <kern/queue.h>
#import <kern/assert.h>
#import <kern/macro_help.h>

typedef struct kern_obj {
	decl_simple_lock_data(,obj_data_lock)
	boolean_t obj_in_use;
	int obj_references;
	zone_t obj_home_zone;
	queue_head_t obj_translations;
} *kern_obj_t;

#define KERN_OBJ_NULL		((kern_obj_t) 0)
#define KERN_OBJ_INVALID	((kern_obj_t) -1)

#define obj_lock(obj) 						\
MACRO_BEGIN							\
	simple_lock(&(obj)->obj_data_lock);			\
	assert((obj)->obj_references > 0);			\
MACRO_END

#define obj_lock_try(obj)	simple_lock_try(&(obj)->obj_data_lock)

#define obj_unlock(obj) 					\
MACRO_BEGIN							\
	assert((obj)->obj_references > 0);			\
	simple_unlock(&(obj)->obj_data_lock);			\
MACRO_END

#define obj_check_unlock(obj) 					\
MACRO_BEGIN							\
	if ((obj)->obj_references <= 0)				\
		obj_free(obj);					\
	else							\
		simple_unlock(&(obj)->obj_data_lock);		\
MACRO_END

#define obj_free(obj) 	 					\
MACRO_BEGIN							\
	assert(!(obj)->obj_in_use);				\
	assert((obj)->obj_references == 0);			\
	assert(queue_empty(&(obj)->obj_translations));		\
	simple_unlock(&(obj)->obj_data_lock);			\
	ZFREE((obj)->obj_home_zone, (vm_offset_t) (obj)); 	\
MACRO_END

#define obj_reference(obj) 					\
MACRO_BEGIN							\
	obj_lock(obj);						\
	(obj)->obj_references++; 				\
	obj_unlock(obj);					\
MACRO_END

#define obj_release(obj) 					\
MACRO_BEGIN							\
	obj_lock(obj); 						\
	(obj)->obj_references--;				\
	obj_check_unlock(obj);					\
MACRO_END

#endif	_KERN_KERN_OBJ_H_

