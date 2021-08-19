/*	@(#)monparam.h	1.0	10/06/86	(c) 1986 NeXT	*/

#ifndef _MONPARAM_
#define _MONPARAM_

#import <next/cpu.h>
#import <nextdev/video.h>

#define	CPU_TYPE	NeXT_1

#define	STACK_SIZE	(8192 - 2048)

#define	N_SIMM		4		/* number of SIMMs in machine */

/* SIMM types */
#define	SIMM_EMPTY	0
#define	SIMM_16MB	1
#define	SIMM_4MB	2
#define	SIMM_1MB	3
#define	SIMM_PAGE_MODE	4

#define	USERENTRY	0x04000000
#define	RAM_MINLOADADDR	0x04000000
#define	RAM_MAXLOADADDR	0x04400000

#endif _MONPARAM_

