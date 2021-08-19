/******************************************************************************

    evio.h
    Ioctl calls for the events driver
    Leovitch 02Jan88
    
    Copyright 1988 NeXT, Inc.
    
    Modified:
    
    09Dec88 Leo  Broken out from evsio.h
    24Aug89 Ted  ANSI function prototyping.
    19Feb90 Ted  Major revision for multiple driver support.
    26Feb90 Ted  New evioScreen structure and EVIOST ioctl.
    12Mar90 Ted  New ev_unregister_screen function, SCREENTOKEN constant.
    06May90 Ted  Added AALastEventSent and AALastEventConsumed to EvVars.
    22May90 Trey More wait cursor vars in EvVars.
    13Jun90 Ted  NXCursorData structure.
    18Jun90 Ted  Default wait cursor constants.

******************************************************************************/

#ifndef EVIO_H
#define EVIO_H

#import <sys/ioctl.h>
#import <sys/port.h>
#import <sys/message.h>
#import <nextdev/event.h>

/* Default Wait Cursor Contants (in 68Hz ticks) */
#define DefaultWCThreshold 68
#define DefaultWCFrameRate 5
#define DefaultWCSustain 10	

/* Mouse Button Constants  */
#define RB		(0x01)
#define LB		(0x04)
#define MOUSEBUTTONMASK	(LB | RB)

#ifndef MAXMOUSESCALINGS
#define MAXMOUSESCALINGS 20	/* Maximum length of SetMouseScaling arrays */
#endif MAXMOUSESCALINGS

#define MAXSCREENS 16

typedef struct _NXEQElStruct {
    short next;		/* Slot of lleq for next event */
    short sema;		/* Is high-level code is reading this event now? */
    NXEvent event;	/* The event itself */
} NXEQElement;

#ifndef BINTREE_H	/* For PostScript compatibility */
typedef struct _point {
    short x;
    short y;
} Point;

typedef struct _bounds {
    short minx, maxx, miny, maxy;
} Bounds;
#endif BINTREE_H

/* NXCursorData is a structure that is passed by reference to screen drivers'
 * cursor procedures: SetCursor, DisplayCursor, RemoveCursor, and MoveCursor.
 * The cursor image consists of data and alpha (mask) components.  Cursor
 * storage for four standard formats has been allocated in shared memory
 * between the kernel and PostScript.  This structure gives drivers access to
 * this storage.
 *
 * The "saveData" and "saveRect" fields are to be used only by the screen with
 * the cursor on it.  "saveData" is where the screen driver stores screen data
 * to be restored when the cursor moves later.  "saveRect" defines the bounds
 * of this saved data in device coordinates.
 *
 * Following "saveData" are pointers to the standard cursor data buffers.
 * "cursorData2W" and "cursorData2B" refer to two different pixel formats,
 * the first being data where 0x11 is white and the second where 0x11 is black.
 * "cursorAlpha2" is the alpha storage for both of these data formats since it
 * is the same for each.
 *
 * WARNING: The cursor buffer formats are fixed. You cannot store any other
 * format here since other drivers depend on these being standard. If you need
 * to store cursor data in other formats, you need to find other shared memory.
 *
 * NOTE: This structure will never be rearranged or altered, but may grow in
 * future releases, so don't depend on this structure's size.
 */
typedef struct _NXCursorData {
    Bounds *saveRect;		/* Bounds of screen pixels saved in saveData */
    unsigned int *saveData;	/* Saved screen data (1024 max bytes) */
    unsigned int *cursorData2W;	/* 2bpp, 1 is white, gray plane (64 bytes) */
    unsigned int *cursorData2B;	/* 2bpp, 1 is black, gray plane (64 bytes) */
    unsigned int *cursorAlpha2; /* 2bpp, alpha plane (64 bytes) */
    unsigned int *cursorData8;	/* 16bpp, ga meshed (512 bytes) */
    unsigned int *cursorData16;	/* 16bpp, rgba meshed (512 bytes) */
    unsigned int *cursorData32;	/* 32bpp, rgba meshed (1024 bytes) */
} NXCursorData;

/* Struct evioScreen
 *
 * This structure holds information on active framebuffers and is used when
 * directing cursor activity.  Each entry represents one screen and stores
 * the global screen bounds, the PostScript NXSDevice pointer (cast to int)
 * a priv field to be used by the kern-loaded driver and a series of
 * procedure vectors. Each screen needs to register itself with the ev
 * driver by filling in all the field of this structure and passing it
 * to "ev_register_screens".  Explanation of vector routines:
 *
 * SetCursor(evioScreen *es, int *data, int *mask, int drow, int mrow,
 *	     waitFrame, NXCursorData *nxCursorData)
 *	SetCursor is only called by the kernel to set the cursor from a 2
 *	bits/pixel source.  "data" points to a bitmap of gray samples,
 *	and "mask" points to a bitmap of alpha samples. "drow" and "mrow"
 *	are their respective row bytes.  "waitFrame" is a number from 0 to 3
 *	and indicates whether or not the wait cursor should be displayed.
 *	If 0, this indicates that the regular cursor should be set. Otherwise,
 *	it indicates the frame number in a small animation sequence.  You
 *	have the option of caching these cursor images to save cpu cycles.
 *	If you've cached these frames, then you simply return when you get
 *	future SetCursors during wait mode.
 *
 * DisplayCursor(evioScreen *es, Bounds cursorRect, int waitFrame,
 *		 NXCursorData *nxCursorData)
 *	The device must draw the cursor within the given cursorRect. It may
 *	NOT draw outside this rectangle.  It must also save the screen bits
 *	beneath this rectangle before drawing the cursor. "waitFrame" is
 *	described above and indicates the current waitcursor frame to display.
 *
 * RemoveCursor(evioScreen *es, Bounds cursorRect, NXCursorData *nxCursorData)
 *	Instructs the device to remove the cursor from the screen, thus
 *	replacing the screen data saved by the previous DisplayCursor.
 *	Again, it MUST NOT restore bits outside of this specified bounds.
 *
 * MoveCursor(evioScreen *es, Bounds oldCursorRect, Bounds cursorRect,
 *	      int waitFrame, NXCursorData *nxCursorData)
 *	When the device receives this call, it is expected to "move" the
 *	cursor on the same screen. Thus, a RemoveCursor(es, oldCursorRect)
 *	followed by a DisplayCursor(rs, cursorRect).  This was done to make
 *	moving the cursor more efficient when devices need to message over
 *	the NextBus. "waitFrame" is described above.
 *
 * SetBrightness(evioScreen *es, int level)
 *	The device should (try) to set the brightness level of its display.
 *	Level is in the range [0,61] inclusive where 0 is dark and 61 is
 *	brightest.
 */
typedef struct {
    Bounds bounds;		/* Screen's (global) bounds */
    int device;			/* WindowServer device ptr */
    void (*SetCursor)();	/* Vector to kernel driver */
    void (*DisplayCursor)();	/* Vector to kernel driver */
    void (*RemoveCursor)();	/* Vector to kernel driver */
    void (*MoveCursor)();	/* Vector to kernel driver */
    void (*SetBrightness)();	/* Vector to kernel driver */
    int priv;			/* For kernel driver's private use */
} evioScreen;

#define SCREENTOKEN 256	/* Some non-zero token to or with screen number */
#define	LLEQSIZE 80	/* Entries in low-level event queue */

/******************************************************************************
	EventGlobals and EvVars
	These two structures define the portion of the events
	driver data structure that is exported to the PostScript
	server.  They are separated into two structures for
	modularity reasons.  The EventGlobals structure contains
	the event queue which is in memory shared between the
	driver and the PostScript server.  All the variables
	necessary to read and process events from the queue are
	contained in it.
	
	The EvVars structure contains all the state of the event
	driver necessary to enable the server itself to remove the
	cursor from the screen and replace it later.  This is
	important, since the server needs to perform this on
	every graphic operation, and we must avoid a system call
	when these operations occur.  Thus, note that the EvVars
	structure as specific to the device(s) being used as
	framebuffers on this system; the EventGlobals structure,
	on the other hand, should be the same on any system running
	NextStep.
	
	When you make an mmap call on the ev driver, the address
	of a page of memory is returned.  That page of memory
	contains (at the beginning) an EvOffsets structure.  The
	components of the EvOffsets structure give the offset in
	bytes from the beginning of the EvOffsets structure to the
	beginnings of the other structures.
	
	SPECIAL 0.9 compoatibility note:  The above is true only
	if, before making the mmap call on the ev driver, the
	user program makes an EVIOSV ioctl calls, setting the 
	version to any nonzero value.  If this call is never made,
	then the page of memory returned from mmap contains a
	EvVars0_9 structure.  That structure is defined below
	as well.
******************************************************************************/

typedef struct _evOffsets {
    int	evGlobalsOffset;	/* Offset to EventGlobals structure */
    int	evVarsOffset;		/* Offset to EvVars structure */
} EvOffsets;

typedef volatile struct _eventGlobals {
    short LLEHead;		/* The next event to be read */
    short LLETail;		/* Where the next event will go */
    short LLELast;		/* The last event entered */
    short eNum;			/* Unique id for mouse events */
    int buttons;		/* State of the mouse buttons 1==down, 0==up */
    int eventFlags;		/* The current value of event.flags */
    int VertRetraceClock;	/* The current value of event.time */
    Point cursorLoc;		/* The current location of the cursor */
    NXEQElement lleq[LLEQSIZE];	/* The event queue itself */
} EventGlobals;

typedef volatile struct _evVars {
    int cursorSema; 		/* Set to disable intr-driven processing */
    Point cursorSpot;		/* Hot spot for current cursor */
    Bounds cursorRect;		/* Rectangle enclosing cursor */
    Point oldCursorLoc;		/* Previous cursor pos for DisplayCursor */
    int cursorShow;		/* Cursor hide-show level. 0 means the cursor
				 * is displayed now; positive numbers are
				 * number of times it has been hidden */
    int cursorObscured;		/* Boolean for whether cursor is hidden
				 * pending move; if true, there is a Hide that
				 * was called for this reason, which the
				 * driver should undo when the cursor next
				 * moves */
    int shieldFlag;		/* Easy test for if need to shield cursor */
    int shielded;		/* Is the cursor shielded? */
    Bounds shieldRect;		/* Rectangle to shield cursor from */
    Bounds mouseRect;		/* Rect for mouse-exited events */
    int mouseRectValid;		/* If nonzero, post a mouse-exited
				 * whenever mouse is outside mouseRect. */
    int movedMask;		/* This contains an event mask for the
				 * three events MOUSEMOVED,  LMOUSEDRAGGED,
				 * and RMOUSEDRAGGED.  It says whether driver
				 * should generate those events. */
    int screens;		/* Number of active screens in screen list */
    int crsrScreen;		/* Current screen on which cursor resides */
    int prevScreen;		/* Previous screen cursor was on */
    Bounds workBounds;		/* Bounds of all screens */
    Bounds saveRect;		/* Screen-dependent save area under cursor */
    unsigned int cursorData2W[16];	/* 2bpp, 2bps, 1spp, g planar */
    unsigned int cursorAlpha2[16];	/* 2bpp, 2bps, 1spp, a planar */
    unsigned int nd_cursorData[256];	/* 32bpp, 8bps, 4spp, rgba format */
    unsigned int saveData[256];		/* Saved screen data */
    evioScreen screen[MAXSCREENS];	/* Array of screen procedure vectors */
    int AALastEventSent;		/* timestamp for wait cursor */
    int AALastEventConsumed;		/* timestamp for wait cursor */	
    Point oldCursorSpot;		/* Normal cursor's hotspot */
    char waitCursorUp;			/* Is wait cursor up? */
    char ctxtTimedOut;			/* Has wait cursor timer expired? */
    char waitCursorEnabled;		/* Play wait cursor game (per ctxt)? */
    char globalWaitCursorEnabled;	/* Play wait cursor game (global)? */
    char waitCursorSema;		/* protects wait cursor fields */
    short waitThreshold;		/* time before wait cursor appears */
    unsigned int cursorData8[128];	/* 16bpp, 8bps, 2spp, ga meshed */
    unsigned int cursorData16[128];	/* 16bpp, 4bps, 4spp, rgba meshed */
    unsigned int cursorData2B[16];	/* 2bpp, 2bps, 1spp, g planar */
} EvVars;


/* The evio structures following are used in various ioctls
 * supported by the ev driver.
 */

struct evioCursor {
    unsigned int *data; /* Pointer to 16 bytes */
    unsigned int *mask; /* Pointer to 16 bytes */
    Point hotSpot;	/* Cursor point */
};

struct evioLLEvent {
    int type;
    Point location;
    NXEventData data;
};

/* General ioctls */

#define EVIOSEP	  _IOW('e', 1, port_t)		   /* Set event port */
#define EVIOLLPE  _IOW('e', 2, struct evioLLEvent) /* Low-level Post Event */
#define EVIOMM	  _IOR('e', 3, void) /* mini_mon (obsolete && disabled) */
#define EVIOSV	  _IOW('e', 4, int)	           /* Set Version */

/* Mouse-related ioctls */

#define	EVIOSC	  _IOW('e', 65, struct evioCursor) /* SetCursor */
#define EVIOSD	  _IOWR('e', 66, int)		   /* StillDown */
#define	EVIORSD	  _IOWR('e', 67, int)		   /* RightStillDown */
#define	EVIOSM	  _IOW('e', 68, Point)		   /* SetMouse */
#define	EVIOCM	  _IOR('e', 69, Point)		   /* CurrentMouse */
#define EVIOST    _IOW('e', 70, Point)		   /* StartCursor */
#define EVIODC    _IO('e', 71)			   /* DisplayCursor */

#endif EVIO_H








