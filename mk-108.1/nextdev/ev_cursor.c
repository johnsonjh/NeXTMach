/******************************************************************************

	ev_cursor.c
	cursor-specific portion of the NeXT events driver.
	
	Created by Leo Hourvitz 29Dec87 from ev_common.c
	
	Copyright 1987 NeXT, Inc.
	
	Modified:
	
	10Dec88 Leo  LLEventPost moved to ev.c
	18Jun89 Leo  Removed unnecessary volatiles in Display and Remove
	06Jan88 Ted  Added PointToScreen, modified MoveCursor.
	23Aug89 Ted  ANSI.
	19Feb90 Ted  Major revision for multiple cursor drivers.
	13Jun90 Ted  Pass NXCursorData by reference in cursor vectors.

******************************************************************************/

#import <sys/types.h>
#import <nextdev/ev_vars.h>
#import <machine/spl.h>
#import <machine/machparam.h>

/* Forward Declarations */

void InvalidateMouseRect();
void RepositionCursor();
void DisplayCursor();
void RemoveCursor();
void CheckShield();
void LLEventPost();

short UniqueEventNum()
{
    while (++eventGlobals->eNum == NULLEVENTNUM)
	; /* sic */
    return(eventGlobals->eNum);
}

/******************************************************************************
	MoveCursor

	sets the cursor position (in the global cursorLoc) to the given
	location.  The location is clipped against the cursor pin rectangle,
	mouse moved/dragged events are generated using the given event mask,
	and a mouse exited event may be generated. The cursor image is
	redisplayed.
******************************************************************************/

void MoveCursor(Point newLoc, int myMovedMask)
{
    int newscreen = -1;

    if (!evp->screens) /* If no screens registered then nothing to do! */
    	return;

    /* Check to see if cursor ventured outside of current screen bounds
       before worrying about which screen it may have gone to. */

    if (!PtInRect(&newLoc, &evp->screen[evp->prevScreen].bounds)) {
	/* At this point cursor has gone off screen.  Check to see if moved
	   to another screen.  If not, just clip it to current screen. */

	if ((newscreen = PointToScreen(newLoc)) < 0) {
	    /* Pin new cursor position to cursorPin rect */
	    newLoc.x = (newLoc.x < cursorPin.minx) ?
		cursorPin.minx : ((newLoc.x > cursorPin.maxx) ?
		cursorPin.maxx : newLoc.x);
	    newLoc.y = (newLoc.y < cursorPin.miny) ?
		cursorPin.miny : ((newLoc.y > cursorPin.maxy) ?
		cursorPin.maxy : newLoc.y);
	}
    }
    /* Catch the no-move case */
    if (*((int *)&eventGlobals->cursorLoc) == *((int *)&newLoc))
	return;

    EVSETSEMA(); /* Critical section begins */

    /* If newscreen is zero or positive, then cursor crossed screens */
    if (newscreen >= 0) {
	/* cursor crossed screens */
	evp->crsrScreen = newscreen;
	cursorPin = evp->screen[evp->crsrScreen].bounds;
	cursorPin.maxx--;
	cursorPin.maxy--;
    }
    eventGlobals->cursorLoc = newLoc;
    
    /* See if anybody wants the mouse moved or dragged events */
    if (evp->movedMask) {
	if ((evp->movedMask&NX_LMOUSEDRAGGEDMASK)&&(eventGlobals->buttons& LB))
	    LLEventPost(NX_LMOUSEDRAGGED, newLoc, NULL);
	else
	    if ((evp->movedMask&NX_RMOUSEDRAGGEDMASK) &&
	    (eventGlobals->buttons&RB))
		LLEventPost(NX_RMOUSEDRAGGED, newLoc, NULL);
	    else
		if (evp->movedMask & NX_MOUSEMOVEDMASK)
		    LLEventPost(NX_MOUSEMOVED, newLoc, NULL);
    }

    /* check new cursor position for leaving evp->mouseRect */
    if (evp->mouseRectValid&&(!PtInRect(&newLoc,&evp->mouseRect)))
	InvalidateMouseRect();
    
    /* check new cursor postion for evp->shieldRect crossing */
    if (evp->shieldFlag)
	CheckShield();

    if (!evp->cursorShow) {
        if (evp->prevScreen == evp->crsrScreen) {
	    /* Optimization: If movement is on same screen, just send one
	     * message to device driver to "move" the cursor (remove followed
	     * by a display).
	     */
	    RepositionCursor();
	} else {
	    RemoveCursor();
	    DisplayCursor();
	}
    }
    EVCLEARSEMA();
} /* MoveCursor */

void InvalidateMouseRect()
{
    if (evp->mouseRectValid) {
	LLEventPost(NX_MOUSEEXITED, eventGlobals->cursorLoc, NULL);
	evp->mouseRectValid = 0;
    }
}

void RepositionCursor()
{
    void (*proc)();
    evioScreen *es;
    Bounds oldCursorRect;

    oldCursorRect = evp->cursorRect;

    /* Calculate new evp->cursorRect */
    evp->cursorRect.maxx = (evp->cursorRect.minx =
	eventGlobals->cursorLoc.x - evp->cursorSpot.x) + CURSORWIDTH;
    evp->cursorRect.maxy = (evp->cursorRect.miny =
	eventGlobals->cursorLoc.y - evp->cursorSpot.y) + CURSORHEIGHT;

    /* FIX: THE FOLLOWING WILL BECOME OBSOLETE */
    evp->oldCursorLoc = eventGlobals->cursorLoc;
    
    es = (evioScreen *)&evp->screen[evp->crsrScreen];
    if (proc = es->MoveCursor)
	(*proc)(es, oldCursorRect, evp->cursorRect, waitFrameNum,
		&nxCursorData);
    /* Screen index stays the same because we are "repositioning".
     * Only the cursorRect and locations need to change.
     */
}

void DisplayCursor()
{
    void (*proc)();
    evioScreen *es;
    
    /* Calculate new evp->cursorRect */
    evp->cursorRect.maxx = (evp->cursorRect.minx =
	eventGlobals->cursorLoc.x - evp->cursorSpot.x) + CURSORWIDTH;
    evp->cursorRect.maxy = (evp->cursorRect.miny =
	eventGlobals->cursorLoc.y - evp->cursorSpot.y) + CURSORHEIGHT;
    
    /* FIX: THE FOLLOWING WILL BECOME OBSOLETE */
    evp->oldCursorLoc = eventGlobals->cursorLoc;

    /* If screen's display vector present, then display the cursor! */
    es = (evioScreen *)&evp->screen[evp->crsrScreen];
    if (proc = es->DisplayCursor)
	(*proc)(es, evp->cursorRect, waitFrameNum, &nxCursorData);
    evp->prevScreen = evp->crsrScreen;
}

void RemoveCursor()
{
    void (*proc)();
    evioScreen *es;

    /* If screen's remove vector present, then remove the cursor! */
    es = (evioScreen *)&evp->screen[evp->prevScreen];
    if (proc = es->RemoveCursor)
	(*proc)(es, evp->cursorRect, &nxCursorData);
}

void SysHideCursor()
{
    if (!evp->cursorShow++)
	RemoveCursor();
}

void SysShowCursor()
{
    if (evp->cursorShow)
	if (!--evp->cursorShow)
	    DisplayCursor();
}

void SysObscureCursor()
{
    if (!evp->cursorObscured) {
	evp->cursorObscured = 1;
	SysHideCursor();
    }
}

void SysRevealCursor()
{
    if (evp->cursorObscured) {
	evp->cursorObscured = 0;
	SysShowCursor();
    }
}

/* CheckShield Decision Table:
 *
 * Shielded:	0	0	1	1
 * Intersect:	0	1	0	1
 * Action:	NOP	HIDE	SHOW	NOP
 */
void CheckShield()
{
    int intersect;
    Bounds tempRect;

    /* Calculate new evp->cursorRect */
    tempRect.maxx = (tempRect.minx =
	eventGlobals->cursorLoc.x - evp->cursorSpot.x) + CURSORWIDTH;
    tempRect.maxy = (tempRect.miny =
	eventGlobals->cursorLoc.y - evp->cursorSpot.y) + CURSORHEIGHT;

    intersect = touchBounds(&tempRect, &evp->shieldRect);
    if (intersect != evp->shielded) {
	(evp->shielded = intersect) ? SysHideCursor() : SysShowCursor();
    }
}

/******************************************************************************
	PointToScreen

	Locate the screen device on which the given point is located.
	All points must be specified in global coordinates.  The screen
	list is searched linearly for a match.
******************************************************************************/

int PointToScreen(Point p)
{
    int i;

    for (i=0; i<evp->screens; i++) {
	if ((p.x >= evp->screen[i].bounds.minx)
	&& (p.x <  evp->screen[i].bounds.maxx)
	&& (p.y >= evp->screen[i].bounds.miny)
	&& (p.y <  evp->screen[i].bounds.maxy))
	    return i;
    }
    return(-1);	/* Cursor outside of known screen boundary */
}











