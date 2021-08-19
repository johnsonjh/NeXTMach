/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	ipc_host.h,v $
 * Revision 2.3  89/10/15  02:04:36  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:07:24  dlb
 * 	Merge.
 * 	[89/09/01  17:25:42  dlb]
 * 
 * Revision 2.1.1.2  89/08/02  22:55:33  dlb
 * 	Merge to X96
 * 
 * Revision 2.1.1.1  89/01/30  17:05:50  dlb
 * 	Created.
 * 	[88/12/01            dlb]
 * 
 */ 

#ifndef	_KERN_IPC_HOST_H_
#define	_KERN_IPC_HOST_H_

extern void ipc_host_init();

extern void ipc_processor_init();
extern void ipc_processor_enable();
extern void ipc_processor_disable();
extern void ipc_processor_terminate();

extern void ipc_pset_init();
extern void ipc_pset_enable();
extern void ipc_pset_disable();
extern void ipc_pset_terminate();

#endif	_KERN_IPC_HOST_H_
