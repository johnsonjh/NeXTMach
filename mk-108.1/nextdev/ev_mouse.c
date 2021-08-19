/******************************************************************************

    ev_mouse.c
    Mouse-specific portion of events driver
    
    Created by Leo Hourvitz 22Dec87 from ev_routines.c
    
    Copyright 1987 NeXT, Inc.
    
    Modified:
    
    10Dec88 Leo  Added old ev_interrupt and ev_common.
    06Jan88 Ted  Modified for multiple screen testing.
    13Jan88 Ted  Modified for PostScript sharing.
    23Aug89 Ted  ANSI.
    19Feb90 Ted  Standardized with "Bounds" instead of "DevBounds"
    19Feb90 Ted  Modifed for multiple driver support.
    05Mar90 Ted  Dave and I added Dave's mouse tablet code.
    12Mar90 Ted  Added ev_unregister_screen() function.
    02Apr90 Ted  Reversed arrowData for premult. towards black framebuffers
    05Apr90 Ted  Mods to ev_unregister_screen.
    10May90 Ted  Three wait cursor data and mask images for animation.
    20May90 Ted  hideWaitCursor() and showWaitCursor().
    18Jun90 Ted  animateWaitCursor().

******************************************************************************/

#import <next/cpu.h>
#import <nextdev/kmreg.h>
#import <machine/machparam.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <nextdev/ev_vars.h>

/* Implementation Constants */

#define INITX		100	/* Initial mouse position */
#define INITY		100

#define INITEVENTNUM	13	/* Where event numbers start */

#define	DCLICKTIME	30	/* Default ticks for a click */
#define	DCLICKSPACE	3	/* Default pixel threshold for clicks */

#define DAUTODIMPERIOD	(67*60*30)	/* Default dim time: Half an hour */


/* The bits for the default cursors */
static int arrowData[] =
{
    0xf0000000, 0xcc000000, 0xc3000000, 0xc0c00000,
    0xc0300000, 0xc00c0000, 0xc0030000, 0xc000c000,
    0xc0003000, 0xc00ffc00, 0xc30c0000, 0xccc30000,
    0xf0c30000, 0xc030c000, 0x0030c000, 0x000fc000
};

static int arrowAlpha[] =
{
    0xf0000000, 0xfc000000, 0xff000000, 0xffc00000,
    0xfff00000, 0xfffc0000, 0xffff0000, 0xffffc000,
    0xfffff000, 0xfffffc00, 0xfffc0000, 0xfcff0000,
    0xf0ff0000, 0xc03fc000, 0x003fc000, 0x000fc000
};

static int waitData1[] =
{
    0x003FFC00, 0x03E94280, 0x0FA94160, 0x3FE90554,
    0x3BF915A4, 0xEAB916BC, 0xD66D1BFC, 0xD5556EA8,
    0xD1015598, 0xC0165054, 0xC15B9404, 0x256BA500,
    0x25ABE540, 0x09AFA540, 0x017FE400, 0x00000000
};

static int waitData2[] =
{
    0x003FFC00, 0x03EFEA80, 0x0EAFA960, 0x35ABA504,
    0x316B9404, 0xC05B9054, 0xD0068158, 0xD54116A8,
    0xD9941FF8, 0xEAAD1AFC, 0xEBF906A4, 0x2FE905A0,
    0x2FA90560, 0x0BA54140, 0x02954000, 0x00000000
};

static int waitData3[] =
{
    0x003FFC00, 0x03C56BC0, 0x0D056BF0, 0x3941AFF4,
    0x3941AFA4, 0xEA51AA94, 0xFE949554, 0xFFF90000,
    0xFBA45500, 0xEA94A954, 0xE951BA54, 0x2541BA90,
    0x2501AE80, 0x0905AF40, 0x01556400, 0x00000000
};

static int waitAlpha1[] =
{
    0x003FFC00, 0x03FFFFC0, 0x0FFFFFF0, 0x3FFFFFFC,
    0x3FFFFFFC, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x3FFFFFFC,
    0x3FFFFFFC, 0x0FFFFFF0, 0x03FFFFC0, 0x003FFC00
};

static int   *waitCursorIndex[] = {waitData1, waitData2, waitData3};
static int   initNumMouseScales = 5;
static short initMouseScaleThresholds[] = { 2, 3, 4, 5, 6 };
static short initMouseScaleFactors[] = { 2, 4, 6, 8, 10 };

/******************************************************************************
	process_mouse_event

	is called when a mouse event is ready to be taken
	out of the preQueue.  It can only be called when
	the cursorSema has been verified to be clear,
	and can only be called from interrupt level (i.e.,
	it cannot itself be interrupted by a mouse interrupt).
	
	A mouse event in the preQueue is basically a status
	report; it so happens we only receive it when something
	(button state or position) has changed.  Thus, this routine goes
	through and tries to find what has changed, and then
	acts on it.  It acts on button state changes by immediately
	posting the appropriate event; for motion, however,
	all it does is accumulate the motion into the globals
	mouseDelX and mouseDelY.  The vertical retrace routine
	will come along later and translate those variables into
	cursor motion.
******************************************************************************/

void process_mouse_event(struct mouse *me)
{
    register int cursorDeltaX, cursorDeltaY;
    
    if (buttonsTied) {
    	me->button_left &= me->button_right;
	me->button_right = 1;
    }
    else
	if (mouseHandedness) {
	    int temp = me->button_left;
	    me->button_left = me->button_right;
	    me->button_right = temp;
	}

    /* Generate any button-related events */
    if (((eventGlobals->buttons & LB)!=0) != (!me->button_left)) {
	if (!me->button_left) {
	    LLEventPost(NX_LMOUSEDOWN, eventGlobals->cursorLoc, NULL);
	    eventGlobals->buttons |= LB;
	} else {
	    LLEventPost(NX_LMOUSEUP, eventGlobals->cursorLoc, NULL);
	    eventGlobals->buttons &= ~LB;
	}
    }

    if (((eventGlobals->buttons & RB)!=0) != (!me->button_right)) {
    	if (!me->button_right) {
	    LLEventPost(NX_RMOUSEDOWN, eventGlobals->cursorLoc, NULL);
	    eventGlobals->buttons |= RB;
	} else {
	    LLEventPost(NX_RMOUSEUP, eventGlobals->cursorLoc, NULL);
	    eventGlobals->buttons &= ~RB;
	}
    }

    /* Figure cursor movement */
    EVSETSEMA();
    if (me->delta_x || me->delta_y) {
	cursorDeltaX = me->delta_x;
	/* Sign-extend by hand */
	if (cursorDeltaX & ((int)0x00000040))
	    cursorDeltaX |= (int)0xffffff80;
	mouseDelX -= cursorDeltaX;
	cursorDeltaY = me->delta_y;
	/* Sign-extend by hand */
	if (cursorDeltaY & ((int)0x00000040))
	    cursorDeltaY |= (int)0xffffff80;
	mouseDelY -= cursorDeltaY;
    }
    EVCLEARSEMA();
} /* process_mouse_event */


/******************************************************************************
	mouse_motion

	is called from the vertical retrace handler if either
	of mouseDelX or mouseDelY is nonzero.  It moves the cursor,
	and generates any appropriate events (mouse-moved,
	mouse-dragged, and mouse-exited events may all be generated).
******************************************************************************/

void mouse_motion()
{
    /* Observe evp->cursorSema; if we can't run now because of it,
     * don't worry, we'll get called again next vertical retrace. */

    if (!evp->cursorSema) {
	/* Changed to support deviced that don't set mouseDel[XY]
		26Feb90 Dave S. */
	if ((evp->movedMask & FORCEMOUSEMOVEDMASK)||(mouseDelX || mouseDelY)) {
	    Point newCursorLoc;
	    int absDelX, absDelY, index;
	    
	    /* Undo obscurecursor (code copied from SysRevealCursor) */
	    if (evp->cursorObscured) {
		evp->cursorObscured = 0;
		SysShowCursor();
	    }
	    
	    /* See if this event want mouse scaling applied to it.
	    	26Feb90 Dave S.
	     */
	    if (!(evp->movedMask & NOMOUSESCALINGMASK)) {
		/* Mouse scaling */
		absDelX = (mouseDelX < 0) ? -mouseDelX : mouseDelX;
		absDelY = (mouseDelY < 0) ? -mouseDelY : mouseDelY;
	   
		/* Use sum of distances as threshold criterion */
		absDelX += absDelY;
		if (absDelX > mouseScaleThresholds[0]) {
		    for (index=1; index<numMouseScales; index++)
			if (absDelX <= mouseScaleThresholds[index])
			    break;
		    index--;
		    mouseDelX *= mouseScaleFactors[index];
		    mouseDelY *= mouseScaleFactors[index];
		    XPR(XPR_EVENT, ("mouse_motion: scaling with index %d\n",
			index));
		}
	    }
	    /* Remove any trace of special device support -- 26Feb90 Dave S. */
	    evp->movedMask &= ~(FORCEMOUSEMOVEDMASK | NOMOUSESCALINGMASK);
	    XPR(XPR_EVENT, ("mouse_motion: delX = %d, delY = %d\n",
		mouseDelX, mouseDelY));
	    /* Finally... update the cursor position */
	    newCursorLoc.x = eventGlobals->cursorLoc.x + mouseDelX;
	    newCursorLoc.y = eventGlobals->cursorLoc.y + mouseDelY;
	    MoveCursor(newCursorLoc, evp->movedMask);
	    mouseDelX = mouseDelY = 0;
	}
    }
} /* mouse_motion */

void SetCursor(int *newData, int dataBytes, int *newMask, int maskBytes,
    int hotSpotX, int hotSpotY)
{
    int i;
    void (*proc)();
    
    EVSETSEMA();
    SysHideCursor();

    /* Copy new data and mask into evp->cursorData and evp->cursorMask */    
    for (i=0; i<evp->screens; i++) {
        if (proc = evp->screen[i].SetCursor)
	    (*proc)(&evp->screen[i], newData, newMask, dataBytes, maskBytes,
	    waitFrameNum, &nxCursorData);
    }
    /* Calculate new cursorPin rect */
    evp->cursorSpot.x = hotSpotX;
    evp->cursorSpot.y = hotSpotY;
    
    /* Calculate new evp->cursorRect */
    evp->cursorRect.maxx = (evp->cursorRect.minx =
	eventGlobals->cursorLoc.x - hotSpotX) + CURSORWIDTH;
    evp->cursorRect.maxy = (evp->cursorRect.miny =
	eventGlobals->cursorLoc.y - hotSpotY) + CURSORHEIGHT;
    
    SysShowCursor();
    EVCLEARSEMA();
} /* SetCursor */

void HideCursor()
{
    EVSETSEMA();
    SysHideCursor();
    EVCLEARSEMA();
}

void ShowCursor()
{
    EVSETSEMA();
    SysShowCursor();
    EVCLEARSEMA();
}

void ObscureCursor()
{
    EVSETSEMA();
    SysObscureCursor();
    EVCLEARSEMA();
}

void RevealCursor()
{
    EVSETSEMA();
    SysRevealCursor();
    EVCLEARSEMA();
}

void ShieldCursor(Bounds *bounds)
{
    EVSETSEMA();
    evp->shieldFlag = 1;
    evp->shieldRect = *bounds;
    evp->shielded = 0;
    CheckShield();
    EVCLEARSEMA();
}

void UnShieldCursor()
{
    EVSETSEMA();
    if (evp->shielded)
	SysShowCursor();
    evp->shielded = 0;
    evp->shieldFlag = 0;
    EVCLEARSEMA();
}

/* The "waitFrameNum" global is passed to devices to indiacte the state of the
 * wait cursor.  If zero, it indicates the wait cursor to be "off".  Above
 * zero indicates the frame number of the cursor being set.  Some devices may
 * wish to cache these cursor images the first time they are seen to save
 * cpu cycles converting the cursor from 2 bits/pixel.
 */
void showWaitCursor()
{
    /* Put up wait cursor */
    evp->waitCursorUp = TRUE;
    evp->oldCursorSpot = evp->cursorSpot;
    waitFrameNum = 1;
    SetCursor(waitData1, 4, waitAlpha1, 4, 8, 8);
    (void)RevealCursor();
    waitFrameTime = waitFrameRate + 1; /* One more at start */
    waitSusTime = waitSustain;
}

void hideWaitCursor()
{
    evp->waitCursorUp = 0;
    waitFrameNum = 0;
    /* Hide wait cursor */
    HideCursor();
    evp->cursorSpot = evp->oldCursorSpot;
    /* Show normal cursor */
    ShowCursor();
}

void animateWaitCursor()
{
    /* Show next wait cursor frame */
    if (++waitFrameNum > 3)
	waitFrameNum = 1;
    SetCursor(waitCursorIndex[waitFrameNum-1], 4, waitAlpha1, 4, 8, 8);
    waitFrameTime = waitFrameRate;
}

void SetMouseMoved(int eMask)
{
    evp->movedMask = eMask & MOVEDEVENTMASK;
}

void ClearMouseRect()
{
    evp->mouseRectValid = 0;
}

void SetMouseRect(Bounds *bounds)
{
    EVSETSEMA();
    evp->mouseRectValid = 1;
    evp->mouseRect = *bounds;
    EVCLEARSEMA();
}

/******************************************************************************
	ev_register_screen adds a new screen to the screen list.  Returns
	token to be used when unregistering the driver via
	ev_unregister_screen.
******************************************************************************/

int ev_register_screen(evioScreen *evsi)
{
    int s;
    void (*proc)();

    if (!evp) return 0;
    if ((s = evp->screens) == MAXSCREENS)
	return(-1);	/* No more space */
    evp->screens++;
    evp->screen[s] = *evsi;
    /* Update (set or enlarge) the workBounds */
    if (s==0) /* First Screen! */
    	evp->workBounds = evsi->bounds;
    else
        boundBounds(&evp->workBounds, &evsi->bounds, &evp->workBounds);
    /* Immediately set current brightness level on registered device */
    if (proc = evp->screen[s].SetBrightness)
	(*proc)(&evp->screen[s], curBright);
    return(s+SCREENTOKEN);
}

/******************************************************************************
	ev_unregister_screen is called by device drivers when they are about
	to close.  This sets all procedure vectors in the screen array to
	NULL so they won't be called.  If the screen being disabled has the
	cursor on it, the cursor is first hidden.
******************************************************************************/

int ev_unregister_screen(int token)
{
    volatile evioScreen *es;

    if (!evp) return 0;
    token -= SCREENTOKEN;
    if (token<0 || token>MAXSCREENS)
	return -1;

    HideCursor();
    /* Critcal Section */
    EVSETSEMA();
    es = &evp->screen[token];
    es->device = 0;
    es->SetCursor = NULL;
    es->DisplayCursor = NULL;
    es->RemoveCursor = NULL;
    es->MoveCursor = NULL;
    es->SetBrightness = NULL;

    /* We won't subtract one from evp->screens because this is a boundary
     * on the number of entries in the matrix.  When a screen goes away,
     * we just mark it inactive so the driver is never called.
     */
     EVCLEARSEMA();
     ShowCursor();
     return 0;
}

void InitMouseVars()
{
    int i;
    void (*proc)();
    
    /* Set cursor on all devices to the ARROW cursor */
    for (i=0; i<evp->screens; i++) {
    	if (proc = evp->screen[i].SetCursor)
	    (*proc)(&evp->screen[i], arrowData, arrowAlpha, 4, 4,
	    0 /*regular cursor*/, &nxCursorData);
    }
    evp->cursorSpot.x = evp->cursorSpot.y = 1;
    /* End Arrow Cursor Setup */

    /* Calculate new evp->cursorRect */
    evp->cursorRect.maxx = (evp->cursorRect.minx =
	eventGlobals->cursorLoc.x - evp->cursorSpot.x) + CURSORWIDTH;
    evp->cursorRect.maxy = (evp->cursorRect.miny =
	eventGlobals->cursorLoc.y - evp->cursorSpot.y) + CURSORHEIGHT;
    cursorPin = evp->screen[evp->crsrScreen].bounds;
    cursorPin.maxx--; cursorPin.maxy--;
    evp->cursorShow = 1;
    clickTimeThresh = DCLICKTIME;
    clickSpaceThresh.x = clickSpaceThresh.y = DCLICKSPACE;
    clickTime = -DCLICKTIME;
    clickLoc.x = clickLoc.y = -DCLICKSPACE;
    clickState = 1;
    autoDimTime = eventGlobals->VertRetraceClock + DAUTODIMPERIOD;
    autoDimPeriod = DAUTODIMPERIOD;
    buttonsTied = 1;
    mouseHandedness = 0;
    numMouseScales = initNumMouseScales;
    for (i=0; i<initNumMouseScales; i++) {
	mouseScaleThresholds[i] = initMouseScaleThresholds[i];
	mouseScaleFactors[i] = initMouseScaleFactors[i];
    }
    mouseDelX = mouseDelY = 0;
}

void InitMouse(int lleqsize)
{
    int i;
    
    /* Do one-time only initialization of mouse-related state */
    for (i=0; i<lleqsize; i++) {
	eventGlobals->lleq[i].event.type = 0;
	eventGlobals->lleq[i].event.time = 0;
	eventGlobals->lleq[i].event.flags = 0;
	eventGlobals->lleq[i].sema = 0;
	eventGlobals->lleq[i].next = i+1;
    }
    eventGlobals->LLELast = eventGlobals->lleq[lleqsize-1].next = 0;
    eventGlobals->LLEHead = eventGlobals->LLETail =
	eventGlobals->lleq[eventGlobals->LLELast].next;
    eventGlobals->buttons = 0;
    eventGlobals->eNum = INITEVENTNUM;
    eventGlobals->eventFlags = 0;
    eventGlobals->VertRetraceClock = 1;
    eventGlobals->cursorLoc.x = INITX;
    eventGlobals->cursorLoc.y = INITY;
    
    nxCursorData.saveRect     = (Bounds *)&evp->saveRect;
    nxCursorData.saveData     = (unsigned int *)evp->saveData;
    nxCursorData.cursorData2W = (unsigned int *)evp->cursorData2W;
    nxCursorData.cursorData2B = (unsigned int *)evp->cursorData2B;
    nxCursorData.cursorAlpha2 = (unsigned int *)evp->cursorAlpha2;
    nxCursorData.cursorData8  = (unsigned int *)evp->cursorData8;
    nxCursorData.cursorData16 = (unsigned int *)evp->cursorData16;
    nxCursorData.cursorData32 = (unsigned int *)evp->nd_cursorData;

    leftENum = rightENum = NULLEVENTNUM;
    evp->mouseRectValid = 0;
    evp->movedMask = 0;
    evp->cursorSema = 0;
    autoDimmed = 0;
    mouseDelX = mouseDelY = 0;
    InitMouseVars();
}

void TermMouse()
{
    if (evp->screens)
	HideCursor();
}

void ResetMouse()
{
    if (evp->screens) {
	if (!evp->cursorShow)
	    HideCursor();
	if (autoDimmed)
	    UndoAutoDim();
	InitMouseVars();
	ShowCursor();
    }
}

/* StartCursor is the first call to display the initial cursor. It should
 * never be called again.
 */
int StartCursor(Point pp)
{
    if (evp->screens) {
	evp->oldCursorLoc = eventGlobals->cursorLoc = pp;
	if ((evp->crsrScreen = evp->prevScreen = PointToScreen(pp)) < 0)
	    return(-1);
	InitMouseVars();
	ShowCursor();
	return(0);
    } else
    	return(-1);
}

void boundBounds(register Bounds *one, register Bounds *two,
    register Bounds *result)
{
    result->minx = two->minx < one->minx ? two->minx : one->minx;
    result->miny = two->miny < one->miny ? two->miny : one->miny;
    result->maxx = two->maxx > one->maxx ? two->maxx : one->maxx;
    result->maxy = two->maxy > one->maxy ? two->maxy : one->maxy;
}

int touchBounds(Bounds *one, Bounds *two)
{
    return(((one->minx < two->maxx) && (two->minx < one->maxx))
    && ((one->miny < two->maxy) && (two->miny < one->maxy)));
}

void offsetBounds(Bounds *one, short dx, short dy)
{
    one->minx += dx;
    one->maxx += dx;
    one->miny += dy;
    one->maxy += dy;
}

















