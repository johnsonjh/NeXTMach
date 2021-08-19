/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 * 14-May-90  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */
#import <sys/boolean.h>
#import <sys/callout.h>
#import <sys/time.h>
#import <kern/mach_types.h>
#import <next/eventc.h>

#ifndef _NEXT_US_TIMER_H
#define	_NEXT_US_TIMER_H
#if	KERNEL
extern struct callout *us_callfree, *us_callout, us_calltodo;

void us_timer_init(void);
void us_timeout(func proc, vm_address_t arg, struct timeval *tvp, int pri);
void us_abstimeout(func proc, vm_address_t arg, struct timeval *tvp, int pri);
boolean_t us_untimeout(func proc, vm_address_t arg);
unsigned int usec_elapsed(struct usec_mark *ump);
void microtime(struct timeval * tvp);
void microboot(struct timeval * tvp);
void delay(unsigned int n);
void us_delay(unsigned usecs);
#endif	KERNEL
#endif	_NEXT_US_TIMER_H
