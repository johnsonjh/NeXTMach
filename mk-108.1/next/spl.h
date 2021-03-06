/*
 *	File:	spl.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Define inline macros for spl routines.
 *	
 * HISTORY
 * 14-May-90  Gregg Kellogg (gk) at NeXT
 *	Changed SPLCLOCK from 6 to 3, as much scheduling code expects
 *	splclock() == splsched().  Added splusclock().
 *
 * 19-Jun-89  Mike DeMoney (mike) at NeXT
 *	Modified to allow spl assertions in spl_measured.h
 */

#ifndef	_SPL_
#define	_SPL_

#if	defined (KERNEL) && !defined(KERNEL_FEATURES)
#import <iplmeas.h>
#else	defined (KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined (KERNEL) && !defined(KERNEL_FEATURES)

#if	NIPLMEAS && !defined(NO_IPLMEAS)
#import <next/spl_measured.h>
#else	NIPLMEAS && !defined(NO_IPLMEAS)

#ifdef	ASSEMBLER
#define	SPLU_MACRO(ipl) \
	movw	sr,d0; \
	movw	\#((ipl)*256 + 0x2000),sr;

#define	splx(nsr) \
	movw	sr,d0; \
	movw	nsr,sr;

#else	ASSEMBLER

#define SPLU_MACRO(x) \
({ register int ret; \
	asm volatile ("movw	sr,%0" : "=dm" (ret)); \
	asm volatile ("movw	%1,sr" : "=m" (*(char *)0): "Jdm" ((x)*256+0x2000)); \
	ret; \
})

#define splx(x) \
({ register int ret; \
	asm volatile ("movw	sr,%0" : "=dm" (ret)); \
	asm volatile ("movw	%1,sr" : "=m" (*(char *)0): "Jdm" ((short)x)); \
	ret; \
})

#endif	ASSEMBLER

#define	SPLD_MACRO(ipl)	SPLU_MACRO(ipl)
#define	spln(x)		splx(x)

#endif	NIPLMEAS && !defined(NO_IPLMEAS)

/*
 *	Define spls as the usual numbers (which should never be used
 *	directly.
 */

#define spl0()	SPLD_MACRO(0)
#define spl1()	SPLU_MACRO(1)
#define spl2()	SPLU_MACRO(2)
#define spl3()	SPLU_MACRO(3)
#define spl4()	SPLU_MACRO(4)
#define spl5()	SPLU_MACRO(5)
#define spl6()	SPLU_MACRO(6)
#define spl7()	SPLU_MACRO(7)

/*
 *	Define spl mnemonics.
 */
#define IPLHIGH		7
#define IPLDMA		6
#define IPLUSCLOCK	6
#define IPLSCC		5
#define IPLCLOCK	3
#define IPLBIO		3
#define IPLSCHED	3
#define IPLIMP		3
#define IPLVM		3
#define IPLNET		2
#define IPLTTY		1
#define IPLSOFTCLOCK	1

#define splhigh()	SPLU_MACRO(IPLHIGH)
#define splusclock()	SPLU_MACRO(IPLUSCLOCK)
#define spldma()	SPLU_MACRO(IPLDMA)
#define splscc()	SPLU_MACRO(IPLSCC)
#define splclock()	SPLU_MACRO(IPLCLOCK)
#define splbio()	SPLU_MACRO(IPLBIO)
#define splsched()	SPLU_MACRO(IPLSCHED)
#define splimp()	SPLU_MACRO(IPLIMP)
#define splvm()		SPLU_MACRO(IPLVM)
#define splnet()	SPLU_MACRO(IPLNET)
#define spltty()	SPLU_MACRO(IPLTTY)
#define splsoftclock()	SPLU_MACRO(IPLSOFTCLOCK)

#endif	_SPL_
