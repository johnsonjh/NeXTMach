/******************************************************************************

    ev.c
    Implementation of NeXT events driver
    
    Created by Leo Hourvitz 22Dec87 from nextmouse_routines.c
    
    Copyright 1987 NeXT, Inc.
    
    Modified:

    08Mar90 jks  Force km.showcount = 0 in evmmap() to keep alert panels
		 from hanging.
    01Feb87 Leo  Only select on read, not exceptional conditions
    15Feb88 Leo  Handle bright and dim
    07Mar88 Leo  Use vertical retrace features
    14Mar88 gk   Add sound key support
    22Jul88 avie Power key support.
    22Aug88 Leo  Make power key generate Sysdefined event subType 1
    19Sep88 Leo  Mach IPC changes
    07Nov88 avie Use sizeof(EvVars) instead of PAGE_SIZE for shared mem
    23Nov88 avie Update to port set technology.
    10Dec88 Leo  Rearrange driver, add evs driver, remove most ioctls
			     from ev driver
    15Dec88 Leo  Filter events with device address 15; workaround for bug
    06Mar89 dmitch Called AllKeysUp() in case of keyboard overflow in
		 evintr()
    08Mar89 Leo  Undo autoDim in evclose if autodimmed
    18Jun89 Leo  Make new version of shmem layout available
    07Jul89 dmitch Used KB_POLL_SETUP for MON_KM_POLL command
    24Aug89 Ted  ANSI Prototyping and some formatting.
    14Nov89 Ted  Added code to turn off boot animation.
    19Feb90 Ted  Modified for multiple driver support.
    23Feb90 Ted  Fixed evclose to test evOpenCalled instead of eventsOpen.
    26Feb90 Dave Added support for `absolute' and non-scaled mouse devices.
    05Mar90 Brian Free event port on close, use kern_port_t for event port.
    05Apr90 Ted  PMON support PMON_SOURCE_EV in LLEventPost.
    22May90 Ted  Added wait cursor support in evvert() and evopen().
    26May90 Ted  Fixed bug: UndoAutoDim only for user events in LLEventPost.
    18Jun90 Ted  Call to animateWaitCursor() to fix wait cursor sustain bug.

******************************************************************************/

#import <sys/types.h>
#import <sys/param.h>
#import <sys/user.h>
#import <sys/ioctl.h>
#import <sys/systm.h>
#import <sys/kernel.h>
#import <sys/file.h>
#import <sys/proc.h>
#import <sys/conf.h>
#import <sys/callout.h>
#import <sys/reboot.h>
#import <kern/thread.h>
#import <kern/task.h>
#import <next/autoconf.h>
#import <next/clock.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/event_meter.h>
#import <next/xpr.h>
#import <next/kernel_pmon.h>
#import <nextdev/monreg.h>
#import <nextdev/kmreg.h>
#import <nextdev/video.h>
#import <nextdev/td.h>
#import <nextdev/dma.h>
#import <mon/global.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <nextdev/ev_vars.h>
#import <kern/xpr.h>
#import <machine/spl.h>
#import <machine/cpu.h>	/* SCR 2 address definition. */
#import <machine/mmu.h>	/* struct mmu_030_tt */

union hw_event {
    int data;
    struct mouse mouse_event;
    struct keyboard keyboard_event;
};

union hw_event curEvent; /* Why shouldn't this be a local in evintr? */

/* Forward Declarations */

extern evvert();
extern void evnewevents();
extern void LLEventPost();
extern void evMsgThreadProc();
extern void AllKeysUp();
void power_intr();
void power_check();

/* Local state */

static struct proc *evSelProcess;	/* Process blocked on us, if one */
static int needsMsg;			/* A lock used between the driver code
					   and a kernel thread running
					   evMsgThreadFunc.  */
static char *sharedMem;		/* The page of memory shared between driver
				   and driver client */
				/* ...and how big that memory is */
#define SHMEMSIZE (sizeof(EvOffsets)+sizeof(EvVars)+sizeof(EventGlobals))

struct _eventMsg eventMsg	/* Message to send to client */
	= { { 0, 1, sizeof(msg_header_t)+sizeof(msg_type_t), 
	      MSG_TYPE_NORMAL, (port_t)0, (port_t)0, 0 },
	    { MSG_TYPE_UNSTRUCTURED, 0, 0, 1, 0, 0 } };

#define MAXQUEUEDEVENTS 5

/* Sometimes we have to hold off on processing events 
 * because the semaphore governing their particular variables 
 * is set.  If so, we queue them up in these arrays and look to
 * see if these have events in them during future vertical retraces.
 */

typedef struct _preQueued {
    int numQueued;		/* Number of unprocessed events */
    int *queueFlag;		/* Flag that must be 0 to process them */
    void (*queueFunc)();	/* Function to call to process them */
    union hw_event queuedUp[MAXQUEUEDEVENTS];
} PreQueued;

static PreQueued kbdQueued;
static PreQueued mouseQueued;
static int lleqSize;	/* number of entries in low-level queue */

/* Device vector routines for ev driver */

void evinit()
{
    extern int call_nmi();
    register int i;
    volatile int *scr2 = (int *) P_SCR2;

    *scr2 |= 0x80;	/* initialize dma chip to write over NextBus */
    /* for now, hardwire one keyboard and mouse */
    /* FIXME: t/o and switch to SCC */
    km_send (MON_KM_USRREQ, KM_SET_ADRS(0));
    km_send (MON_KM_USRREQ, KM_SET_LEDS(0, 0));
    mon_send (MON_KM_POLL, KB_POLL_SETUP);

    /* enable interrupts */
    install_scanned_intr(I_POWER, (func)power_intr, 0); /*before retrace!*/
    install_scanned_intr(I_NMI, call_nmi, 0);
    install_scanned_intr(I_KYBD_MOUSE, (func)evintr, 0);
}

/******************************************************************************
    evopen
    
    Opens the ev driver.  This routine allocates the shared memory to be
    used between itself and the WindowServer.
******************************************************************************/

int evopen(dev_t dev, int flag)
{
    EvOffsets *eop;
    extern struct km km;
    extern int keySema;
    static int evInitialized;
    extern struct mon_global *mon_global;	/* Turn off boot anim */
    struct mon_global *mg = mon_global;		/* Turn off boot anim */

    /* TURN OFF BOOT ANIMATION */
    *MG(char*, MG_flags) &= ~MGF_ANIM_RUN;

    /* Can't do this with pseudo-inits because it's needed before then. */
    if ((km.flags & KMF_INIT) == 0)
	kminit();	/* km.c: init keyboard/mouse device hardware  */

    /* This only needs to be done once. */
    if (!evInitialized) {
	evInitialized = 1;
	kernel_thread(kernel_task, evMsgThreadProc);
    }
    /* We can only be opened once */
    if (evOpenCalled)
	return(EBUSY);
    /*
     * Now allocate shared memory.  Allow the WindowServer to gain access to
     * it via the evmmap call below.
     */
    sharedMem =  (char *) kmem_alloc(kernel_map, round_page(SHMEMSIZE));
    if (!sharedMem)
	panic("ev: No space!\n");

    km.flags &= ~KMF_SEE_MSGS;
    km.save = 0;	
    km.showcount = 0;
    evSelProcess = NULL;
    eventPort = KERN_PORT_NULL;
	
    /* Set up 2.0 structures */
    eop = (EvOffsets *) sharedMem;
    evp = (EvVars *) (sharedMem + sizeof(EvOffsets));
    eventGlobals = (EventGlobals *) (((char *) evp) + sizeof(EvVars));
    eop->evVarsOffset = ((char *) evp) - sharedMem;
    eop->evGlobalsOffset = ((char *) eventGlobals) - sharedMem;
    evp->waitCursorEnabled = TRUE;
    evp->globalWaitCursorEnabled = TRUE;
    /* Set wait cursor defaults */
    evp->waitThreshold = DefaultWCThreshold;
    waitFrameRate = DefaultWCFrameRate;
    waitSustain = DefaultWCSustain;	
    waitSusTime = 0;
    /* End of 2.0 structures init */

    lleqSize = LLEQSIZE;
    mouseQueued.queueFlag = (int *) &evp->cursorSema;
    mouseQueued.queueFunc = process_mouse_event;
    kbdQueued.queueFlag = (int *) &keySema;
    kbdQueued.queueFunc = process_kbd_event;

    evOpenCalled = 1;	/* Driver now marked as open */
    return(0);		/* Successful open call */
}

int evclose(dev_t dev, int flag)
{
    if (!evOpenCalled)
	return(ENXIO);	/* Then how'd we get here? */
    evOpenCalled = 0;
    eventsOpen = 0;
    if (autoDimmed)
	UndoAutoDim();
    evSelProcess = NULL;
    TermMouse();
    if (sharedMem)
	kmem_free(kernel_map, sharedMem, round_page(SHMEMSIZE));
    evp = NULL;
    eventGlobals = NULL;
    sharedMem = NULL;
    eventTask = 0;
    if (eventPort != KERN_PORT_NULL) {
	port_release(eventPort);
	eventPort = KERN_PORT_NULL;
    }
    /* kmioctl (0, KMIOCPOPUP, mach_title, POPUP_NOM, 0, 0); */
    return(0);
}

int evselect(dev_t dev, int flag)
{
    int s;

    XPR(XPR_EVENT, ("evselect: thread 0x%x\n", current_thread()));
    if (flag == FREAD) {
	if (eventGlobals->LLEHead != eventGlobals->LLETail) {
	    XPR(XPR_EVENT, ("evselect: head != tail\n"));
	    return(1);
	}
	s = splhigh();
	if (evSelProcess) {
	    if (evSelProcess != (struct proc *)current_thread()) {
		splx(s);
		printf("Double select!\n");
		return(-1);
	    }
	}
	else
	    evSelProcess = (struct proc *)current_thread();
	splx(s);
    }
    return(0);
}

int evmmap(dev_t dev, int off, int prot)
{
    static int returnValue;
    /* We save this since the kernel calls us multiple times: first to set up
     * the map, subsequent times for each page. Since we only map one page,
     * we're called twice. The second time, we just return the previous value.
     */
    
    if (eventsOpen)
	return atop(pmap_resident_extract(pmap_kernel(), sharedMem+off));

    if (off >= round_page(SHMEMSIZE))
	return(-1);

    /* Get those events coming through! */
    InitMouse(lleqSize);
    
    /* Get ready fer them key events */
    InitKbd(0);
    
    /* Set eventsOpen last to avoid race conditions (all the interrupt-based
     * stuff doesn't run unless eventsOpen is nonzero).
     */
    eventsOpen = 1;
    
    /* Start vbl interrupts */
    softint_sched(CALLOUT_PRI_RETRACE, evvert, 0);
    eventTask = current_task();

    /* OK, remember the value we're returning this time */
    return atop(pmap_resident_extract(pmap_kernel(), sharedMem+off));
}

/*
void set_new_ev_structs() {}
*/

void add_queued_event(PreQueued *preQueue)
{
    if (preQueue->numQueued < MAXQUEUEDEVENTS) {
	preQueue->queuedUp[preQueue->numQueued] = curEvent;
	preQueue->numQueued++;
    }
    else {
	/* Yow!  What can we do??? */ ;
#if PMON
	/* tcohn: Well, let's tell the PMON server, that's what! */
	pmon_log_event( PMON_SOURCE_EV,
			KP_EV_PREQUEUE_FULL,
			(preQueue == &mouseQueued) ? 0 : 1,
			curEvent.data,
			0);
#endif
    }
}

void process_queued_events(PreQueued *preQueue)
{
    if (preQueue->numQueued&&(!*preQueue->queueFlag)) {
	int i;

	for (i=0; i<preQueue->numQueued; i++)
	    (*preQueue->queueFunc)(&preQueue->queuedUp[i]);
	preQueue->numQueued = 0;
    }
}

void evintr()
{
    register volatile struct monitor *mon = (volatile struct monitor*) P_MON;
    register int dev;
    struct mon_status csr;
    extern int sound_active, reconnect, recon_poll, reconpoll();

    csr = mon->mon_csr;
    curEvent.data = mon->mon_km_data;	/* clears interrupt */

    /* reenable keyboard if it is not responding */
    if ((curEvent.data & MON_NO_RESP) &&
	(curEvent.data & MON_MASTER) == 0 && !sound_active && reconnect) {
	    km_send (MON_KM_USRREQ, KM_SET_ADRS(0));
	    return;
    }

    dev = MON_DEV_ADRS (curEvent.data);
    if (csr.km_ovr) {
	mon->mon_csr.km_ovr = 1;	/* clear it */
	XPR(XPR_EVENT, ("evintr(B): keyboard overrun"));
	AllKeysUp();	/* ensure auto-repeating keys are shut off */
	return;
    }
    XPR(XPR_EVENT, ("evintr(B): dev = 0x%x  data = 0x%x\n",
	 dev,curEvent.data));
    if (csr.km_dav == 0) {
	printf ("0x%x spurious keyboard/mouse interrupt\n", csr);
	return;
    }
    /* Here, we filter events with a device of 15.  This is a 
     * workaround for the fact that we seem to get such events
     * immediately following a transmission to the keyboard
     * LEDs.  FIX!!! We should not be getting these 'events'
     * in the first place.  [Leo 15Dec88]
     */
    
    /* With the fastest possible keyboard scanning rate (used in 1.0.58 and
     * beyond), this hardware failure appears as a No Response Error.
     * [dpm 07/20/89]
     */
    if ((dev == 0xf) || (curEvent.data & MON_NO_RESP))
	return;
    if (dev & 1) {	/* odd device addresses are mice */
	if (eventsOpen) {
	    int wereEvents;
	    
	    wereEvents = EventsInQueue();
	    add_queued_event(&mouseQueued);
	    process_queued_events(&mouseQueued);
	    if ((!wereEvents)&&EventsInQueue())
		evnewevents();
	}
    } else if (eventsOpen) {	/* then to the event queue with it */
	int wereEvents;
    
	wereEvents = EventsInQueue();
#if EVENTMETER
	/* Check for event meter keys */
	if ((curEvent.keyboard_event.up_down == KM_DOWN) &&
	curEvent.keyboard_event.command_left &&
	curEvent.keyboard_event.command_right)
	if (curEvent.keyboard_event.key_code == UP_ARROW_KEY) {
	    event_run(EM_KEY_UP, eventGlobals->eventFlags&NX_SHIFTMASK);
	    return;
	} else if (curEvent.keyboard_event.key_code == DOWN_ARROW_KEY) {
	    event_run(EM_KEY_DOWN);
	    return;
	} 
#endif EVENTMETER
	/* check for key event during alert popups */
	if ((curEvent.keyboard_event.up_down == KM_DOWN)
	&& (km.flags & KMF_ALERT_KEY)) {
	    alert_key = kybd_process(&curEvent.keyboard_event);
	    return;
	}
	add_queued_event(&kbdQueued);
	process_queued_events(&kbdQueued);
	if ((!wereEvents)&&EventsInQueue())
	    evnewevents();
    } else {		/* Revert to console driver */
	kmintr_process(&curEvent.keyboard_event);
	power_check();	/* better place to do this? */
    }

    if (!recon_poll && reconnect && callfree) {
	timeout (reconpoll, 0, hz*3);
	recon_poll = 1;
    }
    XPR(XPR_EVENT, ("evintr(B): exit"));
}

int evvert()
{
    int wereEvents;
    
    if (eventsOpen) {
    	wereEvents = EventsInQueue();
    	/* Increment event time stamp */
	if (!++eventGlobals->VertRetraceClock)
	    eventGlobals->VertRetraceClock++; /* skip 0 */
	/* Generate any repeat-key events */
	DoKbdRepeat(); 
	/* Move cursor for accumulated mouse motion */
	if ((evp->movedMask & FORCEMOUSEMOVEDMASK)
	|| (mouseDelX || mouseDelY))
	    mouse_motion();
	/* Check for leftover pre-queued events */
	if (mouseQueued.numQueued)
	    process_queued_events(&mouseQueued);
	if (kbdQueued.numQueued)
	    process_queued_events(&kbdQueued);
	/* If we put events in the queue this time, let waiters know */
	if ((!wereEvents) && EventsInQueue())
	    evnewevents();
	/* WAITCURSOR ACTION */
	if (waitSusTime) --waitSusTime; /* Do this every tick regardless */
	if (!evp->waitCursorSema && !evp->cursorSema)
	{
	    if ((evp->AALastEventSent != evp->AALastEventConsumed) &&
	    ((eventGlobals->VertRetraceClock - evp->AALastEventSent >
	    evp->waitThreshold)))
		evp->ctxtTimedOut = TRUE;
	    if (evp->waitCursorEnabled && evp->globalWaitCursorEnabled &&
	    evp->ctxtTimedOut)
	    {
		/* WAIT CURSOR SHOULD BE ON */
		if (!evp->waitCursorUp)
		    (void)showWaitCursor();
	    } else {
		/* WAIT CURSOR SHOULD BE OFF */
		if (evp->waitCursorUp && (waitSusTime == 0))
		    (void)hideWaitCursor();
	    }
	    /* Animate cursor */
	    if (evp->waitCursorUp)
		if (--waitFrameTime == 0)
		    (void)animateWaitCursor();
	}
	power_check();
	if ((eventGlobals->VertRetraceClock > autoDimTime) && (!autoDimmed))
	    DoAutoDim();
    }
    return(eventsOpen);
}

int force_power_down = 0;

void power_intr()
{
    extern volatile int	*intrstat;
    int bit;
    int wereEvents;

    /* The power key will continually interrupt until released (and even then,
     * after a short additional time period). So disable more interrupts.
     */
    if (!rtc_intr())
    	return;
    bit = I_BIT(I_POWER);
    *intrmask &= ~bit;
    intr_mask &= ~bit;
    if (force_power_down)
	rtc_power_down();
    if (eventsOpen) {
	NXEventData powerData;
	static Point powerPoint = { 0, 0 };

#define HARD_OFF_MASK (NX_NEXTLCMDKEYMASK|NX_NEXTLALTKEYMASK)

	if ((eventGlobals->eventFlags & HARD_OFF_MASK) == HARD_OFF_MASK) {
	    /*
	     * Immediate power down
	     */
	    rtc_power_down();
	}
	wereEvents = EventsInQueue();
	powerData.compound.subType = 1;
	LLEventPost(NX_SYSDEFINED,powerPoint,&powerData);
	if ((!wereEvents) && EventsInQueue())
	    evnewevents();
	/*
	 * Vertical retrace will reenable interrupts.
	 */
    } else {
	km_power_down();
    }
}

void power_check()
{
    extern volatile int	*intrstat;
    int bit;

    /*
     * Detect power key up motion.
     */
    bit = I_BIT(I_POWER);
    if ((intr_mask & bit) == 0) {
	/* Disabled ... means that we detected power key down */
	if ((*intrstat & bit))
	    return;	/* key still down */
	/*
	 * Reenable interrupt.
	 */
	*intrmask |= bit;
	intr_mask |= bit;
    }
}

static void evMsgThreadProc()
{
    int	s;

    (current_thread())->ipc_kernel = TRUE;
    s = splhigh();	/* could possibly be lower */
    while (1) {
	if (!needsMsg) {
	    assert_wait(&eventMsg, TRUE);
	    (void) splx(s);
	    thread_block();
	    s = splhigh();
	}
	needsMsg = 0;
	if (eventsOpen) {
	    /* Do the local equivalent of a msg_send_from_kernel, without
	     * the SEND_ALWAYS.
	     */
	    kern_msg_t	new_kmsg;

	    if (msg_copyin(kernel_task, &eventMsg, eventMsg.h.msg_size,
		&new_kmsg) == SEND_SUCCESS)
	    msg_queue(new_kmsg, 0, 0);
	}
    }
}

void evnewevents()
{
    if (evSelProcess) {	/* Wakeup our sleeper */
	selwakeup(evSelProcess, 0);
	evSelProcess = NULL;
    }
    if (eventPort) {
    	needsMsg = 1;
	thread_wakeup(&eventMsg);
	/* evp->pendingMsg = 1; */
    }
}

static int myAbs(int a) { return(a > 0 ? a : -a); }

/* LLEventPost 
 *
 * This routine actually places events in the event queue which is in
 * the EventGlobals structure.  It is called from all parts of the ev
 * driver.
 */

void LLEventPost(int what, Point location, NXEventData *myData)
{
    int	theClock = eventGlobals->VertRetraceClock;
    NXEQElement *theHead = (NXEQElement *)
	&eventGlobals->lleq[eventGlobals->LLEHead];
    NXEQElement *theLast = (NXEQElement *)
	&eventGlobals->lleq[eventGlobals->LLELast];
    NXEQElement *theTail = (NXEQElement *)
	&eventGlobals->lleq[eventGlobals->LLETail];

    /* Some events affect screen dimming */
    if (EventCodeMask(what) & NX_UNDIMMASK) {
    	if (autoDimmed)
	    UndoAutoDim();
    	else
    	    autoDimTime = theClock + autoDimPeriod;
    }
    if ((theHead != theTail)
    && (!theLast->sema)
    && (theLast->event.type == what)
    && (EventCodeMask(what) & COALESCEEVENTMASK)) {
    /* coalesce events */
	theLast->event.location.x = location.x;
	theLast->event.location.y = location.y;
	theLast->event.time = theClock;
	if (myData != NULL)	/* dspring 22Mar90 */
	    theLast->event.data = *myData;
    } else if (theTail->next != eventGlobals->LLEHead) {
	/* store event in tail */
	theTail->event.type = what;
	theTail->event.location.x = location.x;
	theTail->event.location.y = location.y;
	theTail->event.flags = eventGlobals->eventFlags;
	theTail->event.time = theClock;
	theTail->event.window = 0;
	if (myData != NULL)
	    theTail->event.data = *myData;
	switch(what) {
	case NX_LMOUSEDOWN:
	    theTail->event.data.mouse.eventNum =
		leftENum = UniqueEventNum();
	    break;
	case NX_RMOUSEDOWN:
	    theTail->event.data.mouse.eventNum =
		rightENum = UniqueEventNum();
	    break;
	case NX_LMOUSEUP:
	    theTail->event.data.mouse.eventNum = leftENum;
	    leftENum = NULLEVENTNUM;
	    break;
	case NX_RMOUSEUP:
	    theTail->event.data.mouse.eventNum = rightENum;
	    rightENum = NULLEVENTNUM;
	    break;
	}
	if (EventCodeMask(what) & MOUSEEVENTMASK) { /* Click state */
	    if (((theClock - clickTime) <= clickTimeThresh)
	    && (myAbs(location.x - clickLoc.x) <= clickSpaceThresh.x)
	    && (myAbs(location.y - clickLoc.y) <= clickSpaceThresh.y)) {
		theTail->event.data.mouse.click = 
		    ((what == NX_LMOUSEDOWN)||(what == NX_RMOUSEDOWN)) ?
		    (clickTime=theClock,++clickState) : clickState ;
	    } else if ((what == NX_LMOUSEDOWN)||(what == NX_RMOUSEDOWN)) {
		clickLoc = location;
		clickTime = theClock;
		clickState = 1;
		theTail->event.data.mouse.click = clickState;
	    } else
		theTail->event.data.mouse.click = 0;
	}
#if PMON
	pmon_log_event(PMON_SOURCE_EV,
		       KP_EV_POST_EVENT,
		       what,
		       eventGlobals->eventFlags,
		       theClock);
#endif
	eventGlobals->LLETail = theTail->next;
	eventGlobals->LLELast = theLast->next;
    }
 /*
  * if queue is full, ignore event, too hard to take care of all cases 
  */
#if PMON
    else
	pmon_log_event( PMON_SOURCE_EV,
			KP_EV_QUEUE_FULL,
			what,
			eventGlobals->eventFlags,
			theClock);
#endif
}










