/* 
 * HISTORY
 *  3-Jan-90 Gregg Kellogg (gk) at NeXT, Inc.
 *	Modified parameters to use void * instead of caddr_t.
 *
 * 26-Oct-87 Peter King (king) at NeXT, Inc.
 *	Created.
 */ 

#ifndef	_KERN_KALLOC_H_
#define _KERN_KALLOC_H_

#import <machine/machparam.h>

#define MINSIZE 32

#ifdef	KERNEL
void kallocinit(void);
void *kalloc(int size);
void *kget(int size);
void kfree(void *data, long size);


void *malloc(unsigned int size);
void *calloc(unsigned int num, unsigned int size);
void *realloc(void *addr, unsigned int size);
void free(void *data);
void malloc_good_size(unsigned int size);
#endif	KERNEL
#endif	_KERN_KALLOC_H_



