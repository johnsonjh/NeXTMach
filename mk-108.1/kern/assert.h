/* 
 * HISTORY
 * 15-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Updated to machine independent based on mach sources.
 *
 * 21-Nov-88  Avadis Tevanian (avie) at NeXT.
 *	Created based on machine dependent version.
 */ 

#ifndef	_KERN_ASSERT_H_
#define	_KERN_ASSERT_H_

#if	KERNEL
#if	!KERNEL_FEATURES
#import <mach_assert.h>
#else	!KERNEL_FEATURES
#import <sys/features.h>
#endif	!KERNEL_FEATURES

#if	MACH_ASSERT
#import <sys/printf.h>

#ifdef __GNU__
#define	ASSERT(e) \
	if ((e) == 0) { \
		printf ("ASSERTION " #e " failed at line %d in %s\n", \
		    __LINE__, __FILE__); \
		panic ("assertion failed"); \
	}
#else !__GNU__
#define	ASSERT(e) \
	if ((e) == 0) { \
		printf ("ASSERTION e failed at line %d in %s\n", \
		    __LINE__, __FILE__); \
		panic ("assertion failed"); \
	}
#endif !__GNU__
#else	MACH_ASSERT
#define	ASSERT(e)
#endif	MACH_ASSERT

/*
 *	For compatibility with the code they are writing at CMU.
 */
#define assert(e)	ASSERT(e)
#ifdef	lint
#define assert_static(x)
#else	lint
#define assert_static(x)	assert(x)
#endif	lint

#endif	KERNEL
#endif	_KERN_ASSERT_H_



