/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 *  History:
 *
 *	18-Jul-90: Brian Pinkerton at NeXT
 *		created
 */
#import <machine/vm_types.h>
#import <kern/queue.h>

/*
 *  Public procedures
 */
void initKernelStacks();		/* initialize the module */
vm_offset_t allocStack();		/* allocate a new kernel stack */
void freeStack(vm_offset_t stack);	/* free a kernel stack */
void swapOutStack(vm_offset_t stack);   /* try to swap out a stack */
void swapInStack(vm_offset_t stack);	/* swap in a stack */


/*
 *  The struct _kernelStack sits at the beginning of a kernel stack (under the
 *  pcb & u area).  We declare it here because the size of the kernel stack depends
 *  on it.
 */
enum stackStatus { STACK_FREE, STACK_SWAPPED, STACK_IN_USE };

typedef struct _kernelStack {
	queue_chain_t		freeList;
	enum stackStatus	status;
} *kernelStack;

#define	KERNEL_STACK_SIZE	(4096 - sizeof(struct _kernelStack))
