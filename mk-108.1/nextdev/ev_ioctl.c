/******************************************************************************
	ev_ioctl.c
	System calls for NeXT mouse driver
	
	Created by Leo Hourvitz 22Dec87 from nextmouse_routines.c
	
	Copyright 1987 NeXT, Inc.
	
	Modified:

	01Feb87 Leo  Only select on read, not exceptional conditions
	15Feb88 Leo  Handle bright and dim
	07Mar88 Leo  Use vertical retrace features
	14Mar88 gk   Add sound key support
	22Jul88 avie Power key support.
	22Aug88 Leo  Make power key generate Sysdefined event subType 1
	19Sep88 Leo  Mach IPC changes
	07Nov88 avie Use sizeof(EvVars) instead of PAGE_SIZE for shared mem
	23Nov88 avie Update to port set technology.
	12Dec88 Leo  Autodim
	15Dec88 Leo  mouseHandedness
	24Aug89 Ted  ANSI
	31Jan90 Morris	Added EVSIOCADS ioctl.
	19Feb90 Ted  Major revision for multiple driver support.
	05Mar90 Brian Fix possible port leak on multiple calls to EVIOSEP
	22May90 Ted  Added wait cursor ioctl calls to evs driver
	25Jul90 Ted  Added EVSIOLLPE (was documented but didn't exist)
	08Aug90 Ted  Added EVSIOGADT and EVIODC ioctls.

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
#import <sys/vm.h>
#import <sys/reboot.h>
#import <kern/thread.h>
#import <kern/task.h>
#import <next/clock.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/event_meter.h>
#import <kern/xpr.h>
#import <nextdev/monreg.h>
#import <nextdev/kmreg.h>
#import <nextdev/snd_snd.h>
#import <nextdev/video.h>
#import <nextdev/td.h>
#import <nextdev/dma.h>
#import <mon/global.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <nextdev/ev_vars.h>
#import <machine/spl.h>

/******************************************************************************
 	evioctl
	This is the ioctl routine for the ev driver itself.
******************************************************************************/

evioctl(dev_t dev, int cmd, int *datap, int flag)
{
    int wereEvents;
    
    /* Only EVIOSV is legal before the mmap call is made */
    if ((!eventsOpen) && (cmd != EVIOSV))
	return(ENXIO);
    wereEvents = EventsInQueue();
    
    switch(cmd) {
    case EVIOSV:	/* Set Version (to nonzero) */
	versionSet = *datap;
	/*(void)set_new_ev_structs();*/
	break;
    case EVIOSEP:	/* Set event port */
	if (object_copyin(current_task(), * (port_t *) datap, MSG_TYPE_PORT,
			  FALSE, (kern_obj_t *) datap)) {
		if (eventPort != KERN_PORT_NULL)
			port_release(eventPort);
		eventPort = * (kern_port_t *) datap;
		eventMsg.h.msg_remote_port = (port_t) eventPort;
	} else
		return EINVAL;
  	break;
    case EVIOLLPE:	/* Low-level Post Event */
    {
	struct evioLLEvent *edp = (struct evioLLEvent *)datap;

	LLEventPost(edp->type, edp->location, &edp->data);
	break;
    }
    case EVIOSD:	/* StillDown */
	*datap = (leftENum == (*datap));
	break;
    case EVIORSD:	/* RightStillDown */
	*datap = (rightENum == (*datap));
	break;
    case EVIOSM:	/* SetMouse (position) */
    {
	Point *pp = (Point *)datap;
	
	MoveCursor(*pp, 0);
	break;
    }
    case EVIOST:	/* StartCursor (position) */
    {
	Point *pp = (Point *)datap;
	
	StartCursor(*pp);
	break;
    }
    case EVIOCM:	/* CurrentMouse (position) */
    {
	Point *pp = (Point *)datap;

	pp->x = eventGlobals->cursorLoc.x;
	pp->y = eventGlobals->cursorLoc.y;
	break;
    }
    case EVIOSC:	/* SetCursor (image) */
    {
	struct evioCursor *ecp = (struct evioCursor *)datap;
	unsigned int cdata[16], cmask[16];
	
	if (copyin(ecp->data, cdata, sizeof(cdata)))
	    return(EINVAL);
	if (copyin(ecp->mask, cmask, sizeof(cmask)))
	    return(EINVAL);
	SetCursor(cdata, 4, cmask, 4, ecp->hotSpot.x, ecp->hotSpot.y);
	break;
    }
    case EVIODC:	/* DisplayCursor */
    {
	/* This is unfortunate, but necessary only while the waitcursor is spinning.
	 * PostScript doesn't know about wait cursor images (aren't stored in the shmem
	 * to save wired-pages and setcursor overhead) so it calls the kernel to display.
	 */
	DisplayCursor();
	break;
    }
    default:
	return(EINVAL);
    } /* switch(cmd) */

    if ((!wereEvents) && EventsInQueue())
	evnewevents();
    return 0;
}

/******************************************************************************
	evsioctl
	
	is the ioctl routine for the evs (event status) driver.
	It is what allows any program -- not just the one program
	that can open the ev driver -- to set user parameters, such
	as key repeat.
******************************************************************************/

evsioctl(dev_t dev, int cmd, int *datap, int flag)
{
    int wereEvents;
    
    if (!eventsOpen)
	return(ENXIO);	/* Then can't change its status */
    wereEvents = EventsInQueue();
    
    switch(cmd) {
    case EVSIOLLPE:	/* Low-level Post Event */
    {
	struct evioLLEvent *edp = (struct evioLLEvent *)datap;
	keySema++;
	evp->cursorSema++;
	LLEventPost(edp->type, edp->location, &edp->data);
	evp->cursorSema--;
	keySema--;
	break;
    }
    case EVSIOSWT:	/* SetWaitThreshold */
	evp->waitThreshold = (*datap < 0) ? 0 : *datap;
	break;
    case EVSIOCWT:	/* CurrentWaitThreshold */
	*datap = evp->waitThreshold;
	break;
    case EVSIOSWS:	/* SetWaitSustain */
    	waitSustain = (*datap < 0) ? 0 : *datap;
	break;
    case EVSIOCWS:	/* CurrentWaitSustain */
	*datap = waitSustain;
	break;
    case EVSIOSWFR:	/* SetWaitFrameRate */
    	waitFrameRate = (*datap < 2) ? 0 : *datap;
	break;
    case EVSIOCWFR:	/* CurrentWaitFrameRate */
    	*datap = waitFrameRate;
	break;
    case EVSIOCKR:	/* CurrentKeyRepeat */
	*datap = keyRepeat;
	break;
    case EVSIOSKR:	/* SetKeyRepeat */
	if (((*datap) > 0) && ((*datap) <= MAXKEYREPEAT))
	    keyRepeat = (*datap);
	break;
    case EVSIOCIKR:	/* CurrentInitialKeyRepeat */
	*datap = initialKeyRepeat;
	break;
    case EVSIOSIKR:	/* SetInitialKeyRepeat */
	if (((*datap) > 0) && ((*datap) <= MAXINITIALKEYREPEAT))
		initialKeyRepeat = (*datap);
	break;
    case EVSIOCKM:	/* CurrentKeyMapping */
    {
	struct evsioKeymapping *ekmp = (struct evsioKeymapping *)datap;
	struct evsioKeymapping ekm;
	
	ekm.mapping = (char *)(curMapping->mapping);
	ekm.size = curMapLen;
	if (ekm.size < ekmp->size)
		ekmp->size = ekm.size;
	if (copyout(ekm.mapping, ekmp->mapping, ekmp->size))
		return(EINVAL);
	break;
    }
    case EVSIOCKML:	/* CurrentKeyMappingLength */
	*datap = curMapLen;
	break;
    case EVSIOSKM:	/* SetKeyMapping */
    {
	struct evsioKeymapping *ekmp = (struct evsioKeymapping *)datap;
	unsigned char *oldMapping;
	char *mappingData;
	int oldMapSize;
	
	mappingData = (char *) kmem_alloc(kernel_map, ekmp->size);
	oldMapSize = curMapLen;
	/* Note order dependency in the if statement */
	if ((copyin(ekmp->mapping,mappingData,ekmp->size))  || 
	((oldMapping = SetKeyMapping(mappingData,ekmp->size)) == NULL)) {
	    kmem_free(kernel_map, mappingData, ekmp->size);
	    return(EINVAL);
	}
	/* Take care of freeing the old ampping data space if it was
	   allocated here previously, or of updating the mapNotDefault
	   variable if this is the first non-default map. */
	if (mapNotDefault)
	    kmem_free(kernel_map, oldMapping, oldMapSize);
	else
	    mapNotDefault = 1;
	break;
    }
    case EVSIORKBD:	/* ResetKeyboard */
	ResetKbd();
	break;

/* Mouse-related ioctls */

    case EVSIOSCT:	/* SetClickTime */
	clickTimeThresh = *datap;
	break;
    case EVSIOCCT:	/* CurrentClickTime */
	*datap = clickTimeThresh;
	break;
    case EVSIOSCS:	/* SetClickSpace */
    {
	Point *pp = (Point *)datap;

	clickSpaceThresh.x = pp->x;
	clickSpaceThresh.y = pp->y;
	break;
    }
    case EVSIOCCS:	/* CurrentClickSpace */
    {
	Point *pp = (Point *)datap;
	
	pp->x = clickSpaceThresh.x;
	pp->y = clickSpaceThresh.y;
	break;
    }
    case EVSIOSMS:	/* SetMouseScaling */
    {
	struct evsioMouseScaling *emsp = (struct evsioMouseScaling *)datap;
	int i;
	
	EVSETSEMA();
	numMouseScales = emsp->numScaleLevels;
	for (i=0; i<emsp->numScaleLevels; i++) {
	    mouseScaleThresholds[i] = emsp->scaleThresholds[i];
	    mouseScaleFactors[i] = emsp->scaleFactors[i];
	}
	EVCLEARSEMA();
	break;
    }
    case EVSIOCMS:	/* CurrentMouseScaling */
    {
	struct evsioMouseScaling *emsp = (struct evsioMouseScaling *)datap;
	int i;
		
	emsp->numScaleLevels = ((numMouseScales > MAXMOUSESCALINGS) ?
	    MAXMOUSESCALINGS : numMouseScales);
	for (i=0; i<emsp->numScaleLevels; i++) {
	    emsp->scaleThresholds[i] = mouseScaleThresholds[i];
	    emsp->scaleFactors[i] = mouseScaleFactors[i];
	}
	break;
    }
    case EVSIORMS:	/* ResetMouse */
    	ResetMouse();
	break;
    case EVSIOSADT:	/* SetAutoDimTime */
    	autoDimTime = autoDimTime - autoDimPeriod + (*((int *)datap));
    	autoDimPeriod = (*((int *)datap));
	break;
    case EVSIOCADT:	/* CurrentAutoDimTime (this should be CurentAutoDimPeriod...) */
    	(*((int *)datap)) = autoDimPeriod;
	break;
    case EVSIOGADT:	/* GetAutoDimTime */
    	(*((int *)datap)) = autoDimTime;
	break;
    case EVSIOCADS:
    	(*((int *)datap)) = autoDimmed;
	break;
    case EVSIOSMH:	/* SetMouseHandedness */
    	mouseHandedness = (*((int *)datap));
	break;
    case EVSIOCMH:	/* CurrentMouseHandedness */
    	(*((int *)datap)) = mouseHandedness;
	break;
    case EVSIOSMBT:	/* SetMouseButtonsTied */
    	buttonsTied = (*((int *)datap));
	break;
    case EVSIOCMBT:	/* CurrentMouseButtonsTied */
    	(*((int *)datap)) = buttonsTied;
	break;
    case EVSIOSB:	/* Set Brightness */
    	/* SetCurBrightness is in ev_kbd.c */
    	curBright = SetCurBrightness(*((int *)datap));
    	break;
    case EVSIOCB:	/* Current Brightness */
    	(*((int *)datap)) = curBright;
    	break;
    case EVSIOSA:	/* Set Attenuation */
    	/* SetAttenuation is in ev_kbd.c, where the volume keys are
	 * handled.  The '3' means to set both channels' attenuation.
	 * All that SetAttenuation actually does is set the globals
	 * vol_l and vol_r to the clipped values.  snd_device_vol_set
	 * actually implements changing the volume.
	 */
	SetAttenuation(3, *((int *)datap));
	snd_device_vol_set();
    	break;
    case EVSIOCA:	/* Current Attenuation */
    	(*((int *)datap)) = (vol_l + vol_r)/2;
    	break;
    default:
	return(EINVAL);
    } /* switch(cmd) */
    
    if ((!wereEvents)&&EventsInQueue())
	evnewevents();
    return 0;
}








