/*	@(#)cframe.h	1.0	10/28/86	(c) 1986 NeXT	*/

/* 
 * HISTORY
 * 19-Dec-89  John Seamons (jks) at NeXT
 *	GDB: force link instruction to be used so traceback will include
 *	routine before the one using PROCENTRY.
 *
 * 28-Oct-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#ifndef _CFRAME_
#define	_CFRAME_

#if	defined (KERNEL) && !defined(KERNEL_FEATURES)
#import <gdb.h>
#else	defined (KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined (KERNEL) && !defined(KERNEL_FEATURES)

#ifndef	ASSEMBLER
struct frame {
	struct frame *f_fp;
	int *f_pc;
	int f_arg;
};
#endif	!ASSEMBLER

#if	defined(GPROF) && !defined(GDB)
#define	PROCENTRY(name) \
	.globl _##name; \
_##name: \
	link	a6,\#0; \
	lea	name##_mcount_data,a0; \
	jbsr	mcount; \
	unlk	a6; \
	.data; \
	.even; \
name##_mcount_data:	.long 0; \
	.text;
#endif

#if	defined(GPROF) && defined(GDB)
#define	PROCENTRY(name) \
	.globl _##name; \
_##name: \
	link	a6,\#0; \
	link	a6,\#0; \
	lea	name##_mcount_data,a0; \
	jbsr	mcount; \
	unlk	a6; \
	.data; \
	.even; \
name##_mcount_data:	.long 0; \
	.text;
#endif

#if	!defined(GPROF) && !defined(GDB)
#define	PROCENTRY(name) \
	.globl _##name; \
_##name:
#endif

#if	!defined(GPROF) && defined(GDB)
#define	PROCENTRY(name) \
	.globl _##name; \
_##name: \
	link	a6,\#0;
#endif

#if	GDB
#define	PROCEXIT \
	unlk	a6; \
	rts
#else	GDB
#define	PROCEXIT \
	rts
#endif	GDB

#if	GDB
/* following the link/unlink protocol makes the stack look like this */
#define	a_fp	a6@
#define	a_ra	a6@(0x4)
#define	a_p0	a6@(0x8)
#define	a_p1	a6@(0xc)
#define	a_p2	a6@(0x10)
#define	a_p3	a6@(0x14)
#define	a_p4	a6@(0x18)
#define	a_p5	a6@(0x1c)
#define	a_p6	a6@(0x20)
#define	a_p7	a6@(0x24)
#define	a_p8	a6@(0x28)
#define	a_p9	a6@(0x2c)
#define	a_p10	a6@(0x30)
#define	a_p11	a6@(0x34)
#define	a_p12	a6@(0x38)
#define	a_p13	a6@(0x3c)
#define	a_p14	a6@(0x40)
#define	a_p15	a6@(0x44)
#define	a_p16	a6@(0x48)
#else	GDB
/* without a link in the prolog the C parameters look like this */
#define	a_ra	sp@
#define	a_p0	sp@(0x4)
#define	a_p1	sp@(0x8)
#define	a_p2	sp@(0xc)
#define	a_p3	sp@(0x10)
#define	a_p4	sp@(0x14)
#define	a_p5	sp@(0x18)
#define	a_p6	sp@(0x1c)
#define	a_p7	sp@(0x20)
#define	a_p8	sp@(0x24)
#define	a_p9	sp@(0x28)
#define	a_p10	sp@(0x2c)
#define	a_p11	sp@(0x30)
#define	a_p12	sp@(0x34)
#define	a_p13	sp@(0x38)
#define	a_p14	sp@(0x3c)
#define	a_p15	sp@(0x40)
#define	a_p16	sp@(0x44)
#endif	GDB
#endif _CFRAME_

