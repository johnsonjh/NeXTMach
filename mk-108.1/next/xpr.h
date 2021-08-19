/* 
 * Copyright (c) 1987, 1988, 1989 NeXT, Inc.
 *
 * HISTORY
 * 02-Mar-89	Doug Mitchell at NeXT
 *	Added XPR_SCCMD.
 *
 * 25-Feb-88  Gregg Kellogg (gk) at NeXT
 *	Changed timestamp to use event counter
 */ 

/*
 *	File:	next/xpr.h
 *
 *	Machine dependent module for the XPR tracing facility.
 */

#import <next/eventc.h>

#define XPR_TIMESTAMP	event_get()

#define	XPR_OD		0x00010000
#define	XPR_LDD		0x00020000
#define	XPR_FS		0x00040000
#define	XPR_PRINTER	0x00080000
#define XPR_SOUND	0x00100000
#define XPR_DSP		0x00200000
#define XPR_MIDI	0x00400000
#define XPR_DMA		0x00800000
#define XPR_EVENT	0x01000000
#define XPR_TIMER	0x02000000
#define	XPR_SCSI	0x04000000
#define XPR_SCC		0x08000000
#define	XPR_MEAS	0x10000000
#define	XPR_ALLOC	0x20000000
#define XPR_FD		0x40000000
#define	XPR_KM		0x80000000


