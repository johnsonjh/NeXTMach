/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	sys/version.h
 *
 * HISTORY
 * 29-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */ 

/*
 *	Each kernel has a major and minor version number.  Changes in
 *	the major number in general indicate a change in exported features.
 *	Changes in minor number usually correspond to internal-only
 *	changes that the user need not be aware of (in general).  These
 *	values are stored at boot time in the machine_info strucuture and
 *	can be obtained by user programs with the host_info kernel call.
 *	This mechanism is intended to be the formal way for Mach programs
 *	to provide for backward compatibility in future releases.
 *
 *	Following is an informal history of the numbers:
 *
 *	04-Mar-90 Avadis Tevanian, Jr.
 *		Major 2, minor 0 for NeXT release 2.0.
 *
 *	11-May-89 Avadis Tevanian, Jr.
 *		Advance version to major 1, minor 0 to conform to NeXT
 *		release 1.0.
 *
 *	05-December-88 Avadis Tevanian, Jr.
 *		Aborted previous numbering, set major to 0, minor to 9
 *		to conform to NeXT's 0.9 release.
 *
 *	25-March-87  Avadis Tevanian, Jr.
 *		Created version numbering scheme.  Started with major 1,
 *		minor 0.
 */

#define KERNEL_MAJOR_VERSION	2
#define KERNEL_MINOR_VERSION	0

