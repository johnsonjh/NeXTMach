/******************************************************************************
	event.h (PostScript side version)
	
	CONFIDENTIAL
	Copyright (c) 1988 NeXT, Inc. as an unpublished work.
	All Rights Reserved.

	Created Leo 01Mar88

	Modified:
	04May88 Leo  Final event types and record
	22Aug88 Leo  Change short -> int for window, add reserved
	26May90 Ted  Added NX_UNDIMMASK to correct triggering of UndoAutoDim

	The PostScript version of this file differs from the
	Window Kit version in that the coordinates here are
	ints instead of floats.
******************************************************************************/

#ifndef EVENT_H
#define EVENT_H

/* Event types */

#define NX_NULLEVENT		0	/* internal use */

/* mouse events */

#define NX_LMOUSEDOWN		1	/* left mouse-down event */
#define NX_LMOUSEUP		2	/* left mouse-up event */
#define NX_RMOUSEDOWN		3	/* right mouse-down event */
#define NX_RMOUSEUP		4	/* right mouse-up event */
#define NX_MOUSEMOVED		5	/* mouse-moved event */
#define NX_LMOUSEDRAGGED	6	/* left mouse-dragged event */
#define NX_RMOUSEDRAGGED	7	/* right mouse-dragged event */
#define NX_MOUSEENTERED		8	/* mouse-entered event */
#define NX_MOUSEEXITED		9	/* mouse-exited event */

/* keyboard events */

#define NX_KEYDOWN		10	/* key-down event */
#define NX_KEYUP		11	/* key-up event */
#define NX_FLAGSCHANGED		12	/* flags-changed event */

/* composite events */

#define NX_KITDEFINED		13	/* application-kit-defined event */
#define NX_SYSDEFINED		14	/* system-defined event */
#define NX_APPDEFINED		15	/* application-defined event */

#define NX_FIRSTEVENT		0
#define NX_LASTEVENT		15
#define NX_NUMPROCS		(NX_LASTEVENT-NX_FIRSTEVENT+1)

/* Event masks */

#define NX_LMOUSEDOWNMASK	(1 << NX_LMOUSEDOWN)	/* left mouse-down */
#define NX_LMOUSEUPMASK		(1 << NX_LMOUSEUP)	/* left mouse-up */
#define NX_RMOUSEDOWNMASK	(1 << NX_RMOUSEDOWN)	/* right mouse-down */
#define NX_RMOUSEUPMASK		(1 << NX_RMOUSEUP)	/* right mouse-up */
#define NX_MOUSEMOVEDMASK	(1 << NX_MOUSEMOVED)	/* mouse-moved */
#define NX_LMOUSEDRAGGEDMASK	(1 << NX_LMOUSEDRAGGED)	/* left-dragged */
#define NX_RMOUSEDRAGGEDMASK	(1 << NX_RMOUSEDRAGGED)	/* right-dragged */
#define NX_MOUSEENTEREDMASK	(1 << NX_MOUSEENTERED)	/* mouse-entered */
#define NX_MOUSEEXITEDMASK	(1 << NX_MOUSEEXITED)	/* mouse-exited */
#define NX_KEYDOWNMASK		(1 << NX_KEYDOWN)	/* key-down */
#define NX_KEYUPMASK		(1 << NX_KEYUP)		/* key-up */
#define NX_FLAGSCHANGEDMASK	(1 << NX_FLAGSCHANGED)	/* flags-changed */
#define NX_KITDEFINEDMASK 	(1 << NX_WINCHANGED)	/* kit-defined */
#define NX_SYSDEFINEDMASK 	(1 << NX_SYSDEFINED)	/* system-defined */
#define NX_APPDEFINEDMASK 	(1 << NX_APPDEFINED)	/* app-defined */

#define EventCodeMask(type)	(1 << (type))
#define NX_ALLEVENTS		-1	/* Check for all events */

/* Mask of events that cause screen to undim */

#define NX_UNDIMMASK		(NX_KEYDOWNMASK | NX_KEYUPMASK | \
				 NX_LMOUSEDOWNMASK | NX_LMOUSEUPMASK | \
				 NX_RMOUSEDOWNMASK | NX_RMOUSEUPMASK | \
				 NX_MOUSEMOVEDMASK | NX_FLAGSCHANGEDMASK | \
				 NX_MOUSEENTEREDMASK | NX_MOUSEEXITEDMASK | \
				 NX_LMOUSEDRAGGEDMASK | NX_RMOUSEDRAGGEDMASK)

/* Masks for the bits in event.flags */

/* device-independent */

#define	NX_ALPHASHIFTMASK	0x00010000
#define	NX_SHIFTMASK		0x00020000
#define	NX_CONTROLMASK		0x00040000
#define	NX_ALTERNATEMASK	0x00080000
#define	NX_COMMANDMASK		0x00100000
#define	NX_NUMERICPADMASK	0x00200000

/* device-dependent */

#define	NX_NEXTCTLKEYMASK	0x00000001
#define	NX_NEXTLSHIFTKEYMASK	0x00000002
#define	NX_NEXTRSHIFTKEYMASK	0x00000004
#define	NX_NEXTLCMDKEYMASK	0x00000008
#define	NX_NEXTRCMDKEYMASK	0x00000010
#define	NX_NEXTLALTKEYMASK	0x00000020
#define	NX_NEXTRALTKEYMASK	0x00000040

/* click state values
 * If you have the following events in close succession, the click
 * field has the indicated value:
 *	
 *  Event	Click Value	Comments
 *  mouse-down	1		Not part of any click yet
 *  mouse-up	1		Aha! A click!
 *  mouse-down	2		Doing a double-click
 *  mouse-up	2		It's finished
 *  mouse-down	3		A triple
 *  mouse-up	3
 */

/* Values for the character set in event.data.key.charSet */

#define	NX_ASCIISET		0
#define NX_SYMBOLSET		1
#define	NX_DINGBATSSET		2

/* EventData type: defines the data field of an event */

typedef	union {
    struct {	/* For mouse-down and mouse-up events */
    	short	reserved;
	short	eventNum;	/* unique identifier for this button */
	int	click;		/* click state of this event */
	int	unused;
    } mouse;
    struct {	/* For key-down and key-up events */
    	short	reserved;
	short	repeat;		/* for key-down: nonzero if really a repeat */
	unsigned short charSet;	/* character set code */
	unsigned short charCode;/* character code in that set */
	unsigned short keyCode;	/* device-dependent key number */
	short	   keyData;	/* device-dependent info */
    } key;
    struct {	/* For mouse-entered and mouse-exited events */
    	short	reserved;
	short	eventNum;	/* unique identifier from mouse down event */
	int	trackingNum;	/* unique identifier from settrackingrect */
	int	userData;   /* uninterpreted integer from settrackingrect */
    } tracking;
    struct {	/* For window-changed, sys-defined, and app-defined events */
    	short	reserved;
	short	subType;	/* event subtype for compound events */
	union {
	    float	F[2];	/* for use in compound events */
	    long	L[2];	/* for use in compound events */
	    short	S[4];	/* for use in compound events */
	    char	C[8];	/* for use in compound events */
	} misc;
    } compound;
} NXEventData;

/* Finally! The event record! */
typedef struct _NXEvent {
    int			type;		/* An event type from above */
    struct {
	int		x, y;		/* Base coordinates in window, */
    }			location;	/* from bottom left */
    long		time;		/* vertical intervals since launch */
    int			flags;		/* key state flags */
    unsigned int	window;		/* window number of assigned window */
    NXEventData		data;		/* type-dependent data */
} NXEvent, *NXEventPtr;

/* How to pick window(s) for event (for PostEvent) */

#define NX_NOWINDOW		-1
#define NX_BYTYPE		0
#define NX_BROADCAST		1
#define NX_TOPWINDOW		2
#define NX_FIRSTWINDOW		3
#define NX_MOUSEWINDOW		4
#define NX_NEXTWINDOW		5
#define NX_LASTLEFT		6
#define NX_LASTRIGHT		7
#define NX_LASTKEY		8
#define NX_EXPLICIT		9
#define NX_TRANSMIT		10
#define NX_BYPSCONTEXT		11

#endif EVENT_H





