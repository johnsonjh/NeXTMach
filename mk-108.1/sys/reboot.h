/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	reboot.h,v $
 * Revision 2.8  89/09/09  16:23:38  rvb
 * 	RB_KDB must ALWAYS be defined even if mach_kdb is off.
 * 	[89/09/07            rvb]
 * 
 * Revision 2.7  89/03/09  22:07:02  rpd
 * 	More cleanup.
 * 
 * Revision 2.6  89/02/25  17:55:48  gm0w
 * 	Changed to generic machine names and eliminated include
 * 	of cputypes.h. Made the MACH_KDB conditional always be
 * 	be used outside the kernel.
 * 	[89/02/14            mrt]
 * 
 * Revision 2.5  88/11/23  16:45:46  rpd
 * 	romp: Added RB_SUSPEND from Acis.
 * 	[88/11/04  18:05:05  rpd]
 * 
 * Revision 2.4  88/08/24  02:41:12  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:21:13  mwyoung]
 * 
 * Revision 2.3  88/08/09  17:59:57  rvb
 * Added RB_ALTBOOT and RB_UNIPROC.
 * 
 * 08-Jan-87  Robert Beck (beck) at Sequent Computer Systems, Inc.
 *	Add Sequent specific stuff under #ifdef BALANCE.
 *
 * 21-Oct-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Added RB_NOBOOTRC for SUN.
 *
 * 20-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	Added include of cputypes.h to pick up MULTIMAX definition
 *
 * 14-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Didn't special case RB_KDB for Multimax, this wasn't necessary
 *	since the Multimax code uses the RB_DEBUG mnemonic.
 *
 * 07-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	Added Multimax boot flag definitions.  Multimax uses a different
 *	debugger flag than other systems.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 22-Oct-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	CMUCS_KDB:  Added RB_KDB flag for kernel debugger.  This is currently
 *	defined at 0x04 (RB_NOSYNC) to be backward compatible, but
 *	should be changed in the future (when it is convenient to
 *	update the boot programs).
 *
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)reboot.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_REBOOT_H_
#define _SYS_REBOOT_H_

#ifdef	KERNEL
#import <mach_kdb.h>
#endif	KERNEL

/*
 * Arguments to reboot system call.
 * These are passed to boot program in r11,
 * and on to init.
 */

#define RB_AUTOBOOT	0	/* flags for system auto-booting itself */

#define RB_ASKNAME	0x01	/* ask for file name to reboot from */
#define RB_SINGLE	0x02	/* reboot to single user only */
#define RB_NOSYNC	0x04	/* dont sync before reboot */
#define RB_KDB		0x04	/* load kernel debugger */
#define RB_HALT		0x08	/* don't reboot, just halt */
#define RB_INITNAME	0x10	/* name given for /etc/init */
#define RB_DFLTROOT	0x20	/* use compiled-in rootdev */
#define RB_ALTBOOT	0x40	/* use /boot.old vs /boot */
#define RB_UNIPROC	0x80	/* don't start slaves */
#define RB_PANIC	0	/* reboot due to panic */
#define RB_BOOT		1	/* reboot due to boot() */

/*
 * Constants for converting boot-style device number to type,
 * adaptor (uba, mba, etc), unit number and partition number.
 * Type (== major device number) is in the low byte
 * for backward compatibility.  Except for that of the "magic
 * number", each mask applies to the shifted value.
 */
#define B_ADAPTORSHIFT	24
#define B_ADAPTORMASK	0x0f
#define B_UNITSHIFT	16
#define B_UNITMASK	0xff
#define B_PARTITIONSHIFT 8
#define B_PARTITIONMASK	0xff
#define B_TYPESHIFT	0
#define B_TYPEMASK	0xff
#define B_MAGICMASK	0xf0000000
#define B_DEVMAGIC	0xa0000000

#ifdef	NeXT
#import <machine/reboot.h>
#endif	NeXT
#ifdef	ibmrt
#define RB_SUSPEND	0x40	/* (6152) suspend unix */
#endif	ibmrt
#if	defined(sun3) || defined(sun4)
#define RB_NOBOOTRC	0x20	/* don't run '/etc/rc.boot' */
#endif	defined(sun3) || defined(sun4)
#if	multimax
/* Additional boot flags on multimax, plus bit defs for standard flags
	Note that multimax uses a different bit for debugger. */

#define RB_B_ASKNAME	0	/* Ask for file name to reboot from */
#define RB_B_SINGLE	1	/* Reboot to single user only */
#define RB_B_NOSYNC	2	/* Don't sync before reboot */
#define RB_B_HALT	3	/* Don't reboot, just halt */
#define RB_B_INITNAME	4	/* Name given for /etc/init */

#define RB_B_PROFILED	28	/* OS Profiling */
#define RB_PROFILED	(1 << RB_B_PROFILED)
#define RB_B_MULTICPU	29	/* Multiprocessor boot */
#define RB_MULTICPU	(1 << RB_B_MULTICPU)
#define RB_B_INTERACT	30	/* Interactive boot */
#define RB_INTERACT	(1 << RB_B_INTERACT)
#define RB_B_DEBUG	31	/* Debug mode boot */
#define RB_DEBUG	(1 << RB_B_DEBUG)
#endif	multimax

#if	balance
/*
 * Sequent specific "reboot" flags.
 */
#define RB_NO_CTRL	0x20	/* for FIRMWARE, don't start controller */
#define RB_NO_INIT	0x40	/* for FIRMWARE, don't init system */
#define RB_AUXBOOT	0x80	/* Boot auxiliary boot name */
#define RB_DUMP		RB_AUXBOOT
#define RB_CONFIG	0x100	/* for FIRMWARE, only build cfg table */
#endif	balance

#endif	_SYS_REBOOT_H_

