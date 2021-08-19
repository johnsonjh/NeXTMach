/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * This file contains the basic data structures that form the interface between
 * the loadable PMON server and the rest of the kernel.
 *
 * HISTORY
 *
 * 1-Feb-89	Brian Pinkerton at NeXT
 *	Created
 *
 */
#import <next/kernel_pmon.h>

#ifdef	PMON

int pmon_flags[PMON_KERNEL_SOURCES_MAX];	/* event masks for each source */

void kpmon_null();
void(*pmon_event_log_p)() = kpmon_null;	/* pointer to pmon_event_log() */

void kpmon_null() {
}


#endif	PMON


