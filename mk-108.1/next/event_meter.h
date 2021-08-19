/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 28-Sep-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Made conditional on eventmeter option.
 *
 * 07-Mar-88  John Seamons (jks) at NeXT
 *	Created.
 */

#ifndef	_EM_
#define	_EM_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <eventmeter.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#if	EVENTMETER

#import <vm/vm_statistics.h>
#import <nextdev/video.h>

/* key codes */
#define	EM_KEY_UP	1
#define	EM_KEY_DOWN	-1

/* events */
#define	EM_INTR		6
#define	EM_DMA		5
#define EM_DISK		4
#define	EM_ENET		3
#define	EM_SYSCALL	2
#define	EM_PAGER	1
#define	EM_PAGE_FAULT	0

/* display geometry */
#define	EM_L		30
#define	EM_R		(EM_L + 1026)
#define	EM_W		1024
#define	EM_Y_OD		(VIDEO_H + 43)
#define	EM_Y_SD		(VIDEO_H + 59)
#define	EM_Y_EVTOP	(VIDEO_H + 25)
#define	EM_Y_EVBOT	(VIDEO_H + 25 + 15)

/* states */
#define	EM_OFF		0
#define	EM_MISC		1
#define	EM_VM		2
#define	EM_NSTATE	2

/* video "start" values */
#define	EM_VID_NORM	0x0
#define	EM_VID_UP	0x1b
#define	EM_VID_DOWN	0xe5

/* log graph */
#define	EM_VMPF		0
#define	EM_VMPA		1
#define	EM_VMPI		2
#define	EM_VMPW		3
#define	EM_FAULT	4
#define	EM_PAGEIN	5
#define	EM_PAGEOUT	6
#define	EM_NEVENT	7

/* disk */
#define	EM_OD		0
#define	EM_SD		1
#define	EM_NDISK	2
#define	EM_READ		0x30000000
#define	EM_WRITE	0x03000000

/* vm */
#define	EM_NVM		3

/* misc */
#define	EM_RATE		1		/* update rate 1 sec */
#define	EM_UPDATE	(hz/EM_RATE)

struct em {
	int	state;
	int	flags;
	int	event_x;
	int	last[EM_NEVENT];
	int	disk_last[EM_NDISK];
	int	disk_rw[EM_NDISK];
	vm_statistics_data_t vm_stat;
	int	pid[EM_NVM];
} em;

/* flags */
#define	EMF_LOCK	0x00000001
#define	EMF_LEDS	0x00000002

int em_chirp;

#define	event_meter(event) \
	if (em.state == EM_MISC && (em.flags & EMF_LOCK) == 0) { \
		em.flags |= EMF_LOCK; \
		event_vline (em.event_x, EM_Y_EVTOP, EM_Y_EVBOT, \
			0x44444444 | (3 << (((event) << 2) + 4))); \
		event_vline (em.event_x + 1, EM_Y_EVTOP, EM_Y_EVBOT, WHITE); \
		if (++em.event_x > EM_R) \
			em.event_x = EM_L; \
		/* if (em_chirp & (1 << (event))) \
			event_chirp(); */ \
		em.flags &= ~EMF_LOCK; \
	}

#else	EVENTMETER
#define	event_meter(event)
#endif	EVENTMETER
#endif	_EM_
