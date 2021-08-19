/******************************************************************************

    ev_vars.h

    This file defines all the variables for the ev driver.
    Since the ev driver is implemented across four files,
    this file was used to declare all of its global state.
	    
    Created by Leo Hourvitz 22Jun87
    
    Copyright 1987 NeXT, Inc.
    
    Modified:
    
    23Dec87 Leo   Independent of all include files except event*.h
    03Oct88 Leo   Add non-incrementing cursorSema stuff
    10Dec88 Leo   Move all state of driver into this file
    19Feb90 Ted   Major revision for multiple cursor driver support
    26Feb90 Dave  Added support for `absolute' and non-scaled mouse devices
    05Mar90 Brian Use kern_port_t for event port
    22May90 Ted   Added wait cursor globals
    13Jun90 Ted   Added nxCursorData global

******************************************************************************/

#ifndef EV_VARS_H
#define EV_VARS_H

#import <kern/task.h>
#import <kern/kern_port.h>
#import <nextdev/event.h>
#import <nextdev/evio.h>
#import <nextdev/evsio.h>
#import <kern/xpr.h>

/* ev.c */
extern void evintr();

/* ev_mouse */
extern void process_mouse_event();
extern void mouse_motion();
extern void boundBounds();

/* ev_cursor */
extern int PointToScreen();

/* ev_kbd.c */
extern void DoKbdRepeat();
extern void DoKbdEvent();
extern void ResetKbd();
extern void process_device_mods();
extern void process_kbd_event();
extern void DoAutoDim();
extern void UndoAutoDim();
extern void CalcModBit();
extern unsigned char *SetKeyMapping();

/* forward */
extern void mini_mon();

#define EVASSERT(c) if (!(c)) mini_mon("cursor","Get Ted! (or reboot)")

#define PtInRect(ptp,rp) \
	((ptp)->x >= (rp)->minx && (ptp)->x <  (rp)->maxx && \
	(ptp)->y >= (rp)->miny && (ptp)->y <  (rp)->maxy)

/* Implementation Constants */

#define CURSORWIDTH  16		/* width in pixels */
#define CURSORHEIGHT 16		/* height in pixels */

#define	NULLEVENTNUM 0		/* The event number that never was */

#define MOVEDEVENTMASK \
	(NX_MOUSEMOVEDMASK | NX_LMOUSEDRAGGEDMASK | NX_RMOUSEDRAGGEDMASK )
#define COALESCEEVENTMASK \
	(MOVEDEVENTMASK | NX_MOUSEEXITEDMASK)
#define MOUSEEVENTMASK \
	(NX_LMOUSEDOWNMASK|NX_RMOUSEDOWNMASK|NX_LMOUSEUPMASK|NX_RMOUSEUPMASK)

/*
 * Bits for supporting absolute and non-scaled devices (|'ed into mouseMotion
 * field in evp global).  Added 26Feb90 Dave S.
 */
#define FORCEMOUSEMOVED		16
#define NOMOUSESCALING		17
#define FORCEMOUSEMOVEDMASK	(1 << FORCEMOUSEMOVED)
#define NOMOUSESCALINGMASK	(1 << NOMOUSESCALING)

/* Mouse driver private variables */

EvVars *evp;			/* Pointer to all my shared variables */
EventGlobals *eventGlobals;	/* Pointer to globals */
int evOpenCalled;		/* Has the driver been opened? */
int eventsOpen;			/* Boolean: has evmmap been called yet? */
short waitSustain;		/* Sustain time before removing cursor */
short waitSusTime;		/* Sustain counter */
short waitFrameRate;		/* Ticks per wait cursor frame */
short waitFrameTime;		/* Wait cursor frame timer */
short waitFrameNum;		/* Current wait cursor frame number [0..2] */
int versionSet;			/* This is normally zero. If the server makes a
				   SetVersion ioctl, then this records the
				   version passed in. */
kern_port_t eventPort;		/* Port to send messages to.  If this is set,
				   then when the first event is put in a empty
				   event queue, a null message will be sent to
				   this port. */
task_t eventTask;		/* handle to task using events */
NXCursorData nxCursorData;	/* Structure of cursor buffer pointers */

struct _eventMsg {
    msg_header_t h;
    msg_type_t   t;
};

extern struct _eventMsg eventMsg;
		/* The message to be sent to in the above case.
		   Declared extern here so that it can be statically
		   initialized in ev.c. */

#define EventsInQueue() \
	(eventsOpen && (eventGlobals->LLEHead != eventGlobals->LLETail))

/* Setting and clearing the cursor semaphore.  Note that you should
   never be setting a set semaphore, or clearing a clear one. */

#if TROLLING_FOR_BUGS

#define EVSETSEMA() \
	EVASSERT(!evp->cursorSema); \
	XPR(XPR_EVENT, ("EVSET line %d in %s", __LINE__, __FILE__)); \
	evp->cursorSema = 1;
#define EVCLEARSEMA() \
	EVASSERT(evp->cursorSema); \
	XPR(XPR_EVENT, ("EVCLEAR line %d in %s", __LINE__, __FILE__)); \
	evp->cursorSema = 0

#else TROLLING_FOR_BUGS

#define EVSETSEMA() evp->cursorSema = 1
#define EVCLEARSEMA() evp->cursorSema = 0

#endif TROLLING_FOR_BUGS


/* Variables that are private to the driver portions of the code */

Bounds cursorPin;		/* Rect to keep cursor in */
short leftENum;			/* Unique id for last left down event */
short rightENum;		/* Unique id for last right down event */
long clickTime;			/* Starting tick count of this click series */
Point clickLoc;			/* Starting location of this click series */
int clickState;			/* Current state of click detection */
long clickTimeThresh;		/* No. of clicks before it's not a click */
Point clickSpaceThresh;		/* No. of pixels before it's not a click */
int buttonsTied;		/* Whether both mouse buttons are tied */
int mouseHandedness;		/* Whether to swap the mouse buttons or not */
				/* (for left-handed people) */
int numMouseScales;		/* Number of threshold/factor pairs */

/* Arrays for thresholds and scale factors */

short mouseScaleThresholds[MAXMOUSESCALINGS];
short mouseScaleFactors[MAXMOUSESCALINGS];
				/* Max possible length is MAXMOUSESCALINGS */

/* Variables used for communication between various interrupt routines */

int mouseDelX;			/* The mouse interrupt routine accumulates */
int mouseDelY;			/* offsets in here, and the vertical retrace
				 * interrupt routine takes them off of here. */

/* Keyboard-related variables */

int keySema;			/* Used to lock out interrupt code while
				 * modifying key-related variables. */
unsigned char deviceMods;	/* The last deviceMods we saw. */


#define VALIDMODSMASK	0x7F	/* Low seven bits of deviceMods */

#define	NUMKEYCODES	128	/* Highest key code is 0x7f */
#define	NUMMODIFIERS	16	/* Maximum number of modifier bits */
#define	BYTES		0	/* If first short 0, all are bytes
				 * (else shorts) */
#define	WHICHMODMASK	0x0F 	/* Bits out of keyBits for bucky bits. */
#define	MODMASK		0x10	/* Bit out of keyBits that gives
				 * existence of modifier bit. */
#define	CHARGENMASK	0x20	/* Bit out of keyBits for char gen. */
#define SPECIALKEYMASK	0x40	/* Bit out of keyBits that identifies this
				 * key as requiring special device processing
				 * (used for sound and brightness). */
#define	KEYSTATEMASK	0x80	/* Bit out of keyBits for cur state. */

#ifndef NULL
#define NULL		0	/* Avoid stdio.h in kernel */
#endif NULL

/* A KeyMapping defines a way to map a series of down/up transitions for
 * numbered keys to any combination of emitted KeyDown and KeyUp events, as
 * well as changes in the deviceMods variable.  See the documentation for the
 * evs driver for the exact format of a KeyMapping string.  This structure
 * holds information resulting from the parse of that string, so that the
 * string does not have to be re-parsed on every event, as well as references
 * into the string itself.
 */

typedef struct KeyMappingStruct {
    short shorts;		/* if nonzero, all number are shorts;
				 * if zero, all numbers are bytes. */
    char keyBits[NUMKEYCODES];	/* For each keycode, low order bit says if the
				 * key generates characters. High order bit
				 * says if the key is assigned to a modifier
				 * bit. The second to low order bit give
				 * the current state of the key. */
    int	maxMod;			/* Bit number of highest numbered modifier
    				 * bit. */
    unsigned char *modDefs[NUMMODIFIERS];
				/* Pointers to where the list of keys for that
				 * modifiers bit begins, or NULL. */
    unsigned char *keyDefs[NUMKEYCODES];
				/* Pointer into the keyMapping where
			         * this key's definitions begin. */
    int	numSeqs;		/* Number of sequence definitions. */
    unsigned char *seqDefs;	/* Pointer into the keyMapping where the
    				 * sequence defs begin. */
    unsigned char *mapping;	/* Pointer to the original space. */
} KeyMapping;

/* The current keyMapping is usually kept in curMappingStorage and pointed to
 * by curMapping. It should always be accessed through the pointer, since that
 * allows keySema to be set for the minimum time when installing a new mapping.
 */

KeyMapping *curMapping;
KeyMapping curMappingStorage;
int curMapLen;
int mapNotDefault;		/* Set if the map is NOT the default one.
				   This implies the installed map is allocated
				   memory. */

/* Here are some special key codes that apply specifically to the NeXT
 * keyboard, first revision.
 */

#define SOUND_UP_KEY		0x1A
#define SOUND_DOWN_KEY		0x02
#define BRIGHTNESS_UP_KEY	0x19
#define BRIGHTNESS_DOWN_KEY	0x01
#define POWER_KEY		0x58
#define	UP_ARROW_KEY		0x16	/* Special to event meter code */
#define	DOWN_ARROW_KEY		0x0F	/* Special to event meter code */


/* These masks apply to the 32-bit event flags word in evGlobals that we
   share with the user level code */

#define DEVICEMODMASK  0x0000FFFF /* Modifier bits determined by device */
#define MAPPINGMODMASK 0xFFFF0000 /* Modifier bits determined by keyMapping */

int	alphaLock;		/* Whether we're in alpha lock mode */
int	keyPressed;		/* Whether a key has been pressed since 
				   cmd-shift */
int	initialKeyRepeat;	/* how many retraces 'til first key repeat? */
int	keyRepeat;		/* how many retraces 'tween subsequent key
				   repeat? */
int	autoDimPeriod;		/* How long since last user action before
				   we autodim screen?  User preference item,
				   set by InitMouse and evsioctl */
int	autoDimmed;		/* Is screen currently autodimmed? */
int	autoDimTime;		/* Time value when we will autodim screen,
				   if autoDimmed is 0.
				   Set in LLEventPost.   */
int	undimmedBrightness;	/* Undimmed value of screen brightness,
				   if autoDimmed is nonzero */
int	curBright;		/* The current brightness is cached here while
				   the driver is open.  This number is always
				   the user-specified brightness level; if the
				   screen is autodimmed, the actual brightness
				   level in the monitor will be less */
#define MAXINITIALKEYREPEAT 1000 /* Sanity checks when setting the above */
#define MAXKEYREPEAT 1000

#endif EV_VARS_H







