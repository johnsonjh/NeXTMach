/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 *  2-Jul-90  Morris Meyer (mmeyer) at NeXT
 *	Created.
 */

#import <sys/time_stamp.h>

#define	MTYPE_KALLOC		0
#define MTYPE_ZALLOC		1
#define	MTYPE_KMEM_ALLOC	2

#define	ALLOC_TYPE_ALLOC	0
#define ALLOC_TYPE_FREE		1

void malloc_debug (void *addr, void *pc, int size, int which, int type);
void *getpc (void);

struct malloc_info {
	struct tsval time;	/* Time stamp    */
	short type;		/* Alloc or free */
	short which;		/* kalloc, zalloc or kmem_alloc */
	void *addr;		/* Allocated or free'd address */
	void *pc;		/* Caller of kalloc, kfree, etc */
	int size;		/* Size of allocated or free'd address */
};
