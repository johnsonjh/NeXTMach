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
 * $Log:	kern_set.h,v $
 * Revision 2.7  89/03/09  20:13:33  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  18:05:16  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.5  89/02/07  01:02:27  mwyoung
 * Relocated from sys/kern_set.h
 * 
 * Revision 2.4  88/08/24  02:32:14  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:15:23  mwyoung]
 * 
 * Revision 2.3  88/08/06  19:21:29  rpd
 * Added macro forms of set_reference(), set_release().
 * Added declarations of set_alloc(), set_destroy(),
 * set_add_member(), set_remove_member().
 * 
 * Revision 2.2  88/07/20  16:49:01  rpd
 * Added set_* versions of the obj macros.
 * Use kern_obj for common fields.
 * Declare set_reference, set_release.
 * Created.  Declares the internal kernel structure associated
 * with a port set.
 * 
 */
/*
 * Kernel internal structure associated with a port set.
 *
 */

#ifndef	_KERN_KERN_SET_H_
#define _KERN_KERN_SET_H_

#import <sys/port.h>
#import <sys/kern_return.h>
#import <kern/kern_obj.h>
#import <kern/msg_queue.h>
#import <kern/queue.h>

typedef struct kern_set {
	struct kern_obj set_obj;

	struct task *set_owner;	/* not task_t, to avoid recursion */
	port_name_t set_local_name;

	msg_queue_t set_messages;
	queue_head_t set_members;
	struct kern_port *set_traversal; /* don't ask */
} *kern_set_t;

#define set_data_lock		set_obj.obj_data_lock
#define set_in_use		set_obj.obj_in_use
#define set_references		set_obj.obj_references
#define set_home_zone		set_obj.obj_home_zone
#define set_translations	set_obj.obj_translations

#define KERN_SET_NULL	((kern_set_t) 0)

#define set_lock(set)		obj_lock(&(set)->set_obj)
#define set_lock_try(set)	obj_lock_try(&(set)->set_obj)
#define set_unlock(set)		obj_unlock(&(set)->set_obj)
#define set_check_unlock(set)	obj_check_unlock(&(set)->set_obj)
#define set_free(set)		obj_free(&(set)->set_obj)

#define set_reference_macro(set)	obj_reference(&(set)->set_obj)
#define set_release_macro(set)		obj_release(&(set)->set_obj)

extern void set_reference();
extern void set_release();
extern kern_return_t set_alloc();
extern void set_destroy();
extern void set_add_member();
extern void set_remove_member();

#endif	_KERN_KERN_SET_H_

