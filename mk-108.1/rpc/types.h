/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *      @(#)types.h 1.20 88/02/08 SMI      
 */


/*
 * Rpc additions to <sys/types.h>
 */
#ifndef __TYPES_RPC_HEADER__
#define __TYPES_RPC_HEADER__

#define	bool_t	int
#define	enum_t	int
#if	!defined(FALSE) || !defined(KERNEL)
#define	FALSE	(0)
#endif	FALSE
#if	!defined(TRUE) || !defined(KERNEL)
#define	TRUE	(1)
#endif	TRUE
#define __dontcare__	-1
#ifndef NULL
#	define NULL 0
#endif

#ifndef KERNEL
extern char *malloc();
#define mem_alloc(bsize)	malloc(bsize)
#define mem_free(ptr, bsize)	free(ptr)
#else
#import <kern/kalloc.h>
#define mem_alloc(bsize)	(char *)kalloc((u_int)bsize)
#define mem_free(ptr, bsize)	kfree((void *)(ptr), (u_int)(bsize))
#endif

#ifdef KERNEL
#import <sys/types.h>

#ifndef _TIME_
#import <sys/time.h>
#endif 

#else
#import <sys/types.h>

#ifndef _TIME_
#import <sys/time.h>
#endif

#endif KERNEL

#endif /* ndef __TYPES_RPC_HEADER__ */


