/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 *	File:	vm/device_pager.h
 *
 *	Exported things from the device pager.
 */

/*
 * HISTORY
 *  8-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Created.
 */

#ifndef	_DEVICE_PAGER_
#define	_DEVICE_PAGER_	1

#import <vm/vm_pager.h>

void		device_pager_init();
void		device_pager();
vm_pager_t	device_pager_create();

#endif	_DEVICE_PAGER_
