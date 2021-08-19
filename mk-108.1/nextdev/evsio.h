/******************************************************************************
	evsio.h
	Ioctl calls for the events status driver
	Leovitch 09Dec88
	
	Copyright 1988 NeXT, Inc.
	
	Modified:
	
	24Aug89 Ted  ANSI prototyping
	07Mar90 Ted  Added Morris' new EVSIOCADS ioctl.
	22May90 Ted  Added wait cursor-related ioctls.
******************************************************************************/

#ifndef EVSIO_H
#define EVSIO_H

#import <sys/ioctl.h>
#import <sys/port.h>
#import <sys/message.h>
#import <nextdev/event.h>

#define MAXMOUSESCALINGS 20 /* Maximum length of SetMouseScaling arrays */

struct evsioPoint {
    short x, y;
};

struct evsioKeymapping {
    int size;
    char *mapping;
};

struct evsioMouseScaling {
    int numScaleLevels;
    short scaleThresholds[MAXMOUSESCALINGS];
    short scaleFactors[MAXMOUSESCALINGS];
};

struct evsioLLEvent {
    int type;
    struct evsioPoint location;
    NXEventData data;
};

/* General ioctls */

#define EVSIOLLPE _IOW('e', 2, struct evsioLLEvent) /* Low-level Post Event */

/* Keyboard-related ioctls */

#define EVSIOSKR  _IOW('e', 10, int)	/* SetKeyRepeat */
#define EVSIOCKR  _IOR('e', 11, int)	/* CurrentKeyRepeat */
#define EVSIOSIKR _IOW('e', 12, int)	/* SetInitialKeyRepeat */
#define EVSIOCIKR _IOR('e', 13, int)	/* CurrentInitialKeyRepeat */
#define EVSIOSKM  _IOW('e', 14, struct evsioKeymapping)
					/* SetKeyMapping */
#define	EVSIOCKM  _IOWR('e', 15, struct evsioKeymapping)
					/* CurrentKeyMapping */
#define	EVSIOCKML _IOR('e', 16, int)	/* CurrentKeyMappingLength */
#define EVSIORKBD _IO('e', 17)		/* ResetKeyboard */

/* WaitCursor-related ioctls */

#define EVSIOSWT _IOW('e', 40, int)	/* SetWaitThreshold */
#define EVSIOCWT _IOR('e', 41, int)	/* CurrentWaitThreshold */
#define EVSIOSWS _IOW('e', 42, int)	/* SetWaitSustain */
#define EVSIOCWS _IOR('e', 43, int)	/* CurrentWaitSustain */
#define EVSIOSWFR _IOW('e', 44, int)	/* SetWaitFrameRate */
#define EVSIOCWFR _IOR('e', 45, int)	/* CurrentWaitFrameRate */

/* Mouse-related ioctls */

#define	EVSIOSCT  _IOW('e', 70, int)	/* SetClickTime */
#define	EVSIOCCT  _IOR('e', 71, int)	/* CurrentClickTime */
#define	EVSIOSCS  _IOW('e', 72, struct evsioPoint) /* SetClickSpace */
#define	EVSIOCCS  _IOR('e', 73, struct evsioPoint) /* CurrentClickSpace */
#define	EVSIOSMS  _IOW('e', 74, struct evsioMouseScaling) /* SetMouseScaling */
#define	EVSIOCMS  _IOR('e', 75, struct evsioMouseScaling) /* CurrentMouseScaling */
#define EVSIORMS  _IO('e', 76)		/* ResetMouse */
#define EVSIOSADT _IOW('e', 77, int)	/* SetAutoDimTime */
#define EVSIOCADT _IOR('e', 78, int)	/* CurrentAutoDimTime */
#define EVSIOSMH  _IOW('e', 79, int)	/* SetMouseHandedness */
#define EVSIOCMH  _IOR('e', 80, int)	/* CurrentMouseHandedness */
#define EVSIOSMBT _IOW('e', 81, int)	/* SetMouseButtonsTied */
#define EVSIOCMBT _IOR('e', 82, int)	/* CurrentMouseButtonsTied */
#define EVSIOCADS _IOR('e', 83, int)	/* AutoDimmed */
#define EVSIOGADT _IOR('e', 84, int)	/* GetAutoDimTime */

/* Device control ioctls.  Note that these set the current value
   but do not affect parameter RAM.  */

#define EVSIOSB	  _IOW('e', 100, int)	/* Set Brightness */
#define EVSIOCB	  _IOR('e', 101, int)	/* Current Brightness */
#define EVSIOSA	  _IOW('e', 102, int)	/* Set Attenuation */
#define EVSIOCA   _IOR('e', 103, int)	/* Current Attenuation */

#endif EVSIO_H


