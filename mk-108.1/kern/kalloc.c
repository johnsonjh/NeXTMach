/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	kern/kalloc.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr.
 *
 *	General kernel memory allocator.  This allocator is designed
 *	to be used by the kernel to manage dynamic memory fast.
 *
 * HISTORY
 * 15-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Updated to mach 2.5 version (2.7.
 *
 * 22-Jan-90  Gregg Kellogg (gk) at NeXT
 *	Added malloc() and friends.
 *	Changed kalloc and friends to use void * rather than caddr_t.
 *
 * 13-Feb-88  John Seamons (jks) at NeXT
 *	Updated to use kmem routines instead of vmem routines.
 *
 * 21-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */

#import <sys/types.h>
#import <vm/vm_param.h>

#import <kern/zalloc.h>
#import <kern/kalloc.h>
#import <vm/vm_kern.h>
#import <vm/vm_object.h>

#ifdef	DEBUG
#import <kern/xpr.h>
#endif	DEBUG
#if	NeXT
#import <next/malloc_debug.h>
#import <next/cframe.h>
#endif	NeXT

extern void bcopy(const void *from, void *to, size_t size);
extern void bzero(void *addr, size_t size);

/*
 *	All allocations of size less than PAGE_SIZE are rounded to the
 *	next highest power of 2.  This allocator is built on top of
 *	the zone allocator.  A zone is created for each potential size
 *	that we are willing to get in small blocks.
 *
 *	We assume that PAGE_SIZE is not greater than 64K;
 *	thus 16 is a safe array size for k_zone and k_zone_name.
 */

int numkallocs = 0; /* XXX */
int first_k_zone = -1;
struct zone *k_zone[16];
static char *k_zone_name[16] = {
	"kalloc.1",		"kalloc.2",
	"kalloc.4",		"kalloc.8",
	"kalloc.16",		"kalloc.32",
	"kalloc.64",		"kalloc.128",
	"kalloc.256",		"kalloc.512",
	"kalloc.1024",		"kalloc.2048",
	"kalloc.4096",		"kalloc.8192",
	"kalloc.16384",		"kalloc.32768"
};

/*
 *  Max number of elements per zone.  zinit rounds things up correctly
 *  Doing things this way permits each zone to have a different maximum size
 *  based on need, rather than just guessing; it also
 *  means its patchable in case you're wrong!
 */
unsigned long k_zone_max[16] = {
#if	NeXT
      1024*1024/1,	/*      1 Byte  */
      1024*1024/2,	/*      2 Byte  */
      1024*1024/4,	/*      4 Byte  */
      1024*1024/8,	/*      8 Byte  */
      1024*1024/16,	/*     16 Byte  */
      1024*1024/32,	/*     32 Byte  */
      1024*1024/64,	/*     64 Byte  */
      1024*1024/128,	/*    128 Byte  */
      1024*1024/256,	/*    256 Byte  */
      1024*1024/512,	/*    512 Byte  */
      1024*1024/1024,	/*   1024 Byte  */
      1024*1024/2048,	/*   2048 Byte  */
      1024*1024/4096,	/*   4096 Byte  */
      1024*1024/8192,	/*   8192 Byte  */
      1024*1024/16384,	/*  16384 Byte  */
      1024*1024/32768,	/*  32768 Byte  */
#else	NeXT
      1024,		/*      1 Byte  */
      1024,		/*      2 Byte  */
      1024,		/*      4 Byte  */
      1024,		/*      8 Byte  */
      1024,		/*     16 Byte  */
      1024,		/*     32 Byte  */
      1024,		/*     64 Byte  */
      1024,		/*    128 Byte  */
      4096,		/*    256 Byte  */
      256,		/*    512 Byte  */
      128,		/*   1024 Byte  */
      128,		/*   2048 Byte  */
      64,		/*   4096 Byte  */
      64,		/*   8192 Byte  */
      64,		/*  16384 Byte  */
      64,		/*  32768 Byte  */
#endif	NeXT
};

/*
 *	Initialize the memory allocator.  This should be called only
 *	once on a system wide basis (i.e. first processor to get here
 *	does the initialization).
 *
 *	This initializes all of the zones.
 */

void kallocinit(void)
{
	register int	i;
	register int	size;

	/*
	 *	Allocate a zone for each size we are going to handle.
	 *	We specify non-paged memory.
	 */
	for (i = 0, size = 1; size < PAGE_SIZE; i++, size <<= 1) {
		if (size < MINSIZE) {
			k_zone[i] = 0;
			continue;
		}
		if (size == MINSIZE) {
			first_k_zone = i;
		}
		k_zone[i] = zinit(size, k_zone_max[i] * size, PAGE_SIZE,
			FALSE, k_zone_name[i]);
	}
}

#if	defined(DEBUG) && NeXT
#define KDBG_BUF_SIZE 100
struct {
	int	addr;
	int	pc;
} kdbg_buf[KDBG_BUF_SIZE];
int kdbg_size = 0;

void kdbg_add(void *addr, int size, int pc)
{
	register int i;

	if (kdbg_size != size)
		return;
	for (  i = KDBG_BUF_SIZE-1; i >= 0; i--)
		if (kdbg_buf[i].addr == 0)
			break;
	if (i == KDBG_BUF_SIZE)
		return;
	kdbg_buf[i].addr = (int) addr;
	kdbg_buf[i].pc = pc;
}

void kdbg_rm(void *addr, int size)
{
	register int i;

	if (kdbg_size != size)
		return;
	for (  i = KDBG_BUF_SIZE-1; i >= 0; i--)
		if (kdbg_buf[i].addr == (int) addr)
			break;
	if (i == KDBG_BUF_SIZE)
		return;
	kdbg_buf[i].addr = 0;
}

void *getpc (void)
{
	register struct frame *fp, *parentfp;

	asm("	.text");
	asm("	movl a6,%0" : "=a" (fp));
	
	parentfp = fp->f_fp;
	return (parentfp->f_pc);
}
static int kalloc_dbg_size=0x40;
static char *kalloc_dbg_min_addrs = (char *)VM_MIN_KERNEL_ADDRESS;

#endif	defined(DEBUG) && NeXT

void *kalloc(int size)
{
	register zindex, allocsize;
	void *addr;
	
	numkallocs++;	/* XXX */

	assert (size > 0);
	
	/* compute the size of the block that we will actually allocate */

	allocsize = size;
	if (size < PAGE_SIZE) {
		allocsize = MINSIZE;
		zindex = first_k_zone;
		while (allocsize < size) {
			allocsize <<= 1;
			zindex++;
		}
	}

	/*
	 * If our size is still small enough, check the queue for that size
	 * and allocate.
	 */

	if (allocsize < PAGE_SIZE) {
		addr = (void *)zalloc(k_zone[zindex]);
	} else {
		addr = (void *)kmem_alloc(kernel_map, allocsize);
	}

#if	defined(DEBUG) && NeXT
	{
		register char *frompc = getpc();

		if((allocsize == kalloc_dbg_size) && 
	           (frompc < kalloc_dbg_min_addrs)) {
			XPR(XPR_ALLOC, ("kalloc(addr=0x%x, size=0x%x) frompc=0x%x\n", addr, size, frompc));
		}
		kdbg_add(addr, allocsize, (int) frompc);
		malloc_debug (addr, frompc, allocsize, 
			MTYPE_KALLOC, ALLOC_TYPE_ALLOC);
	}
#endif	defined(DEBUG) && NeXT

	return(addr);
}

void *kget(size)
{
	register zindex, allocsize;
	void *addr;

	assert (size > 0);

	/* compute the size of the block that we will actually allocate */

	allocsize = size;
	if (size < PAGE_SIZE) {
		allocsize = MINSIZE;
		zindex = first_k_zone;
		while (allocsize < size) {
			allocsize <<= 1;
			zindex++;
		}
	}

	/*
	 * If our size is still small enough, check the queue for that size
	 * and allocate.
	 */

	if (allocsize < PAGE_SIZE) {
		addr = (void *) zget(k_zone[zindex]);
	}
	else {
		/* This will never work, so we might as well panic */
		panic("kget: oversized request");
	}

#if	defined(DEBUG) && NeXT
	{
		register char *frompc = getpc();

		XPR(XPR_ALLOC, ("kalloc(addr=0x%x, size=0x%x) frompc=0x%x\n", addr, size, frompc));
		kdbg_add(addr, allocsize, (int) frompc);
		malloc_debug (addr, frompc, allocsize, 
			MTYPE_KALLOC, ALLOC_TYPE_ALLOC);
	}
#endif	defined(DEBUG) && NeXT

	return(addr);
}

void kfree(void *data, long size)
{
	register freesize, zindex;
	char *frompc;
	
	assert (data != (void *) 0 && size > 0);


	freesize = size;
	if (size < PAGE_SIZE) {
		freesize = MINSIZE;
		zindex = first_k_zone;
		while (freesize < size) {
			freesize <<= 1;
			zindex++;
		}
	}
#if	defined(DEBUG) && NeXT
	frompc = getpc();
	if((freesize == kalloc_dbg_size) &&
	   (frompc < kalloc_dbg_min_addrs)) {
		XPR(XPR_ALLOC, ("kfree(data=0x%x, size=0x%x) frompc=0x%x\n", data, size, frompc));
	}
#endif	defined(DEBUG) && NeXT

#if	defined(DEBUG) && NeXT
	kdbg_rm(data, freesize);
	malloc_debug (data, getpc(), freesize, 
		MTYPE_KALLOC, ALLOC_TYPE_FREE);
#endif	defined(DEBUG) && NeXT

	if (freesize < PAGE_SIZE)
		zfree(k_zone[zindex], (caddr_t)data);
	else
		kmem_free(kernel_map, (caddr_t)data, freesize);
}

/*
 * Trival implementation of malloc/free calls.  This version encodes the
 * length into the memory returned.  A better implementation would do this
 * only for memory which is allocated >= pagesize.  < pagesize would
 * check the address for the zone to free to.
 */
void *malloc(unsigned int size)
{
	unsigned int *addr;

	size += 8;
	addr = (unsigned int *)kalloc(size);
	if (!addr)
		return addr;
	bzero(addr, size);
	*addr = size;
	return (void *)(addr+2);
}

void *calloc(unsigned int num, unsigned int size)
{
	return malloc(num*size);
}

void *realloc(void *addr, unsigned int size)
{
	unsigned int *sizep = ((unsigned int *)addr) - 2;
	unsigned int *new;

	new = (unsigned int *)kalloc(size+8);
	if (!new)
		return new;
	*new = size+8;
	bcopy(addr, new+2, size);
	kfree((caddr_t)sizep, *sizep);
	return (void *)(new+2);
}

void free(void *data)
{
    unsigned int *sizep = (((unsigned int *)data)-2);
    kfree ((caddr_t)sizep, *sizep);
}

void malloc_good_size(unsigned int size) {}







