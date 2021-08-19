/*
 *	File:	spl_measured.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Define inline macros for spl routines (these include measurement).
 *
 * HISTORY
 * 19-Jun-89  Mike DeMoney (mike) at NeXT
 *	Modifications for assertions re: spl level changes.  splu_*
 *	is asserted to only raise spl level, spld_* to only lower
 *	spl level.  splx() is assumed to lower spl level, spln() is
 *	provide in the few cases where an variable spl is desired to
 *	raise the spl level.
 */

#ifndef	_SPL_MEASURED_
#define	_SPL_MEASURED_

#ifdef	ASSEMBLER

#import <next/psl.h>

#define	SPLU_MACRO(ipl) 			\
	moveml	\#0x40c0,sp@-;			\
	movl	\#((ipl)*256)+0x2000, sp@-;	\
	jsr	_splu_measured;			\
	addql	\#4,sp;				\
	moveml	sp@+,\#0x0302;

#define	SPLD_MACRO(ipl) 			\
	moveml	\#0x40c0,sp@-;			\
	movl	\#((ipl)*256)+0x2000, sp@-;	\
	jsr	_spld_measured;			\
	addql	\#4,sp;				\
	moveml	sp@+,\#0x0302;

#define	splx(nsr) 				\
	moveml	\#0x40c0,sp@-;			\
	movl	nsr, sp@-;			\
	jsr	_spld_measured;			\
	addql	\#4,sp;				\
	moveml	sp@+,\#0x0302;

#define	spln(nsr) 				\
	moveml	\#0x40c0,sp@-;			\
	movl	nsr, sp@-;			\
	jsr	_splu_measured;			\
	addql	\#4,sp;				\
	moveml	sp@+,\#0x0302;

#else	ASSEMBLER

#define SPLU_MACRO(x)	splu_measured(((x)*256)+0x2000)
#define SPLD_MACRO(x)	spld_measured(((x)*256)+0x2000)

#define splx(x)	spld_measured(x)
#define spln(x)	splu_measured(x)

#endif	ASSEMBLER

#endif	_SPL_MEASURED_
