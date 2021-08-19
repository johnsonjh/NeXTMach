/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	mach_extra.h,v $
 * Revision 2.6  89/03/09  20:20:38  rpd
 * 	More cleanup.
 * 
 * Revision 2.5  89/02/25  18:13:51  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.4  89/02/19  12:57:33  rpd
 * 	Moved from kern/ to mach/.
 * 
 * Revision 2.3  89/02/07  01:02:51  mwyoung
 * Relocated from sys/mach_extra.h
 * 
 * Revision 2.2  88/07/20  16:50:42  rpd
 * Removed port_messages definition, because port_select is going away.
 * It spelled port_select wrong, so it's a safe change....
 * Also added copyright.
 * 
 * 11-Mar-87  Mary Thompson at Carnegie Mellon
 *	added definition for PORT_DEFAULT
 *
 *  8-Jan-87  Mary Thompson at Carnegie Mellon
 *	started
 *
 */
/*
 *
 *  This file defines new names for some of the Mach user interface primitives.
 *  It is imported by the user side of mach (mach.h and mach_user.c)
 *  and exported by mach.h to the user.
 *
 */

#ifndef	_SYS_MACH_EXTRA_H_
#define _SYS_MACH_EXTRA_H_

#define port_restrict	port_disable
#define port_unrestrict port_enable
#define PORT_DEFAULT	PORT_ENABLED

#endif	_SYS_MACH_EXTRA_H_

