/*
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 */
/*
 *  This file is used to pick up the various configuration options that affect
 *  the use of kernel include files by special user mode system applications.
 *
 *  The entire file (and hence all configuration symbols which it indirectly
 *  defines) is enclosed in the KERNEL_FEATURES conditional to prevent
 *  accidental interference with normal user applications.  Only special system
 *  applications need to know the precise layout of internal kernel structures
 *  and they will explicitly set this flag to obtain the proper environment.
 */


#ifdef	KERNEL_FEATURES
#import <machine/DEFAULT.h>
#endif	KERNEL_FEATURES
