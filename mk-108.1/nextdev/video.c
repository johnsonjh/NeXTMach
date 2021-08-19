/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 *
 * 12-Sep-90  Mike Paquette (mpaque) at NeXT
 *	Updated to reflect final Warp 9C hardware defs.
 *
 * 6-Jul-90  Mike Paquette (mpaque) at NeXT
 *	Reworked to support multiple built-in video devices.
 *
 * 13-Jun-90  Ted Cohn (tcohn) at NeXT
 *	Added argument NXCursorData to cursor routines.
 *	
 * 22-May-90  Ted Cohn (tcohn) at NeXT
 *	vidDisplayCursor, vidMoveCursor, and vidSetCursor now take wait cursor
 *	frame argument to put up the wait cursor.
 *	
 *  2-Apr-90  Ted Cohn (tcohn) at NeXT
 *	Modified vidSetCursor to take corrected sense of cursor data.
 *	That is, from premultiplied towards white to black.
 *
 * 15-Mar-90  John Seamons (jks) at NeXT
 *	Added support for auto poweron mode of new clock chip.
 *
 * 12-Mar-90  Ted Cohn (tcohn) at NeXT
 *	Call ev_unregister_screen upon closing video driver.  Added static
 *	vid_screen_token to record screen registration id around.
 *
 * 01-Mar-90  Ted Cohn (tcohn) at NeXT
 *	New device-specific (MegaPixel) cursor drawing routines added.
 *	New DKIOCREGISTER ioctl call added to register the screen to the
 *	ev driver.
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	New flag to DKIOCGADDR ioctl (STOP_ANIM) that temporarily stops boot
 *	icon animation.  Use CONTINUE_ANIM to start animation going again.
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Pass thread as first argument to pmap_tt().
 *
 * 12-Dec-88  Leo Hourvitz (leo) at NeXT
 *	Old bright_set procedure now renamed to SetBrightness
 *
 * 15-Aug-88  Avadis Tevanian (avie) at NeXT
 *	Don't allocate virtual space for video memory and MWF space.
 *	Just reset the maximum allowable size for the map that needs
 *	such access.
 *
 * 21-Sep-87  John Seamons (jks) at NeXT
 *	Created.
 */ 

/*
 * Video device driver
 *
 * This device driver is used to control and map into virtual memory the built-in
 * frame buffer on the CPU board.  There are several possible frame buffers supported
 * in our products, including the 2 bit grey and the 16 bit color frame buffers.  Since
 * there is at most one frame buffer on a CPU board, access to the frame buffer is
 * always through "/dev/vid0".  The characteristics of the installed frame buffer can
 * be read through an ioctl() call.  This provides backwards compatibility with certain 
 * programs (blit, fbshow, WindowServer) that explicitly open /dev/vid0 for the purpose
 * of controlling the boot animation.
 *
 * Maps the video RAM into virtual space with the MMU transparent translation
 * register #1.  This register is timeshared among multiple threads by
 * code in load_context() which reloads it from pcb->pcb_mmu_tt1.
 * The virtual space used is slot dependent because transparent translation
 * does not offset the PA and VA (it's always 1:1).
 *
 * Additional ioctl() calls support unmapping the video RAM, registering the cursor
 * functions with the event system.
 *
 * A generic procedural interface is provided for setting up vertical retrace event
 * handlers, as the hardware interface differs drastically for the different frame
 * buffers.
 *
 * Special autoconfiguration code is provided to support the startup Mach console
 * display (see nextdev/km.c).
 */

#import <sys/errno.h>
#import <sys/types.h>
#import <sys/callout.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <sys/ioctl.h>
#import <sys/user.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <vm/vm_object.h>
#import <vm/pmap.h>
#import <next/cpu.h>
#import <next/pcb.h>
#import <next/scr.h>
#import <next/trap.h>
#import <next/vmparam.h>
#import <nextdev/dma.h>
#import <nextdev/video.h>
#import <nextdev/ev_vars.h>
#import <nextdev/kmreg.h>
#import <mon/global.h>

#define COLOR_FB 1	/* Support the color Warp 9C frame buffer in standard release*/

/* Forward declarations */
void	vidStopAnimation();
void vidMoveCursor();
int vidProbeForFB();
static int vidConfigureFB();
/*
 * We have our own private probe package that can be run before the full Unix/Mach
 * trap handler is set up.  This is used to test for the presence of frame buffers
 * VERY EARLY in the startup sequence.
 */
static int	vid_early_probe( int );
static void	vid_probe_faulted();

/* 2 bit frame buffer */
static int vid_BW2_present();
static void vid_BW2_config();
static void vid_BW2_init();
static vm_offset_t vid_BW2_MapFB();
static void vid_BW2_SetCursor();
static void vid_BW2_DisplayCursor();
static void vid_BW2_MoveCursor();
static void vid_BW2_RemoveCursor();
static void vid_BW2_SetBrightness();
static void vid_BW2_installIntr();
static void vid_BW2_intr();
static void vid_BW2_SuspendAnim();
static void vid_BW2_ResumeAnim();

static unsigned int waitData2[16];
static unsigned int waitMask2[16];

/* Global Variables */
static int vid_screen_token;
#define VID_NO_FB	-1
static int vidFBnum = VID_NO_FB;
static void (*vidIntrFunc)() = (void (*)()) 0;		/* Vert retrace intr func. */
static void * vidIntrFuncArg;				/* Arg to retrace func. */
typedef	void (*voidfptr_t)();

#if defined(COLOR_FB)
/*	16 Bit Color frame buffer */
#define RBMASK	0xF0F0		/* Short, or 16 bit format */
#define GAMASK	0x0F0F		/* Short, or 16 bit format */
#define AMASK	0x000F		/* Short, or 16 bit format */
static int vid_C16_present();
static void vid_C16_config();
static void vid_C16_init();
static vm_offset_t vid_C16_MapFB();
static void vid_C16_SetCursor();
static void vid_C16_DisplayCursor();
static void vid_C16_MoveCursor();
static void vid_C16_RemoveCursor();
static void vid_C16_SetBrightness();
static void vid_C16_installIntr();
static void vid_C16_intr();
static void vid_C16_SuspendAnim();
static void vid_C16_ResumeAnim();

static char C16_waitCache[3] = {0, 0, 0};
static unsigned short C16_waitCursor1[256], C16_waitCursor2[256], C16_waitCursor3[256];
static unsigned short *C16_waitCursors[] =
		{C16_waitCursor1, C16_waitCursor2, C16_waitCursor3};
#endif
/*
 * The following structure is used to autoconfigure the correct 
 * video device for this CPU board.  The 'present' function snoops 
 * about and returns TRUE if the 'init' function is appropriate for the board.
 * The 'config' function fills in the km_coni data structure for the specified display.
 * The 'init' function puts the display in a sane state.
 */

static struct
{
	int	(*present)();		/* returns TRUE or FALSE */
	void	(*config)();
	void	(*init)();
	vm_offset_t (*MapFB)();		/* Map the frame buffer into user space */
	void	(*SetCursor)();		/* Cursor control for events system */
	void	(*DisplayCursor)();
	void	(*RemoveCursor)();
	void	(*SetBrightness)();
	
	void	(*installIntr)();	/* Install and enable VBL interrupts */

	void	(*suspendAnim)();	/* Funcs used to control boot animation */
	void	(*resumeAnim)();
} video_dev[] =
{
    {	vid_BW2_present, vid_BW2_config, vid_BW2_init, vid_BW2_MapFB,
	vid_BW2_SetCursor, vid_BW2_DisplayCursor, vid_BW2_RemoveCursor,
	vid_BW2_SetBrightness,
	vid_BW2_installIntr,
	vid_BW2_SuspendAnim, vid_BW2_ResumeAnim
    },
#if COLOR_FB
    {
    	vid_C16_present, vid_C16_config, vid_C16_init, vid_C16_MapFB,
	vid_C16_SetCursor, vid_C16_DisplayCursor, vid_C16_RemoveCursor,
	vid_C16_SetBrightness,
	vid_C16_installIntr,
	vid_C16_SuspendAnim, vid_C16_ResumeAnim
    }
#endif
};

#define NUM_VIDEO_DEVS (sizeof video_dev / sizeof video_dev[0])



vidopen(dev_t dev)
{
	extern int probe_rb();

	if ( vidFBnum == VID_NO_FB )
	{
		if ( vidConfigureFB() == -1 )
			return ENODEV;
	}
        return 0;
}

vidclose(dev_t dev)
{
    ev_unregister_screen(vid_screen_token);
    vid_screen_token = 0;
}

vidioctl(dev, cmd, data, flag)
    dev_t dev;
    caddr_t data;
{
    struct pcb		*pcb = current_thread()->pcb;
    vm_map_t		map = current_task()->map;
    struct pmap		*pmap = vm_map_pmap (current_task()->map);
    vm_offset_t		addr;
    extern		struct mon_global *mon_global;
    struct		mon_global *mg = mon_global;
    struct		nvram_info ni, *nip;
    int			*bp;
    struct dma_dev	*vddp = (struct dma_dev*) P_VIDEO_CSR;

    switch (cmd) {
    
    case DKIOCGADDR:
    
	/* stop boot icon animation (hook is for bootblit)  */
	if (*(int*)data == STOP_ANIM) {
	    /* stop animation temporarily */
	    (*video_dev[vidFBnum].suspendAnim)();
	} else
	if (*(int*)data == CONTINUE_ANIM) {
	    /* continue animation after being stopped temporarily */
	    (*video_dev[vidFBnum].resumeAnim)();
	} else {
	    /* stop animation permamently */
	    vidStopAnimation();
	}
	/*
	 *  Do this here instead of vidopen() so the unrelated
	 *  ioctls can be used without actually mapping in
	 *  the video spaces.
	 */
	if (pcb->pcb_flags & PCB_TT1)
	    return EBUSY;		/* already open */

	/* stop boot icon animation (hook is for bootblit)  */
	if (*(int*)data != CONTINUE_ANIM) {
	    /* stop animation permamently */
	    vidStopAnimation();
	}
	
	*(vm_offset_t*) data = (*video_dev[vidFBnum].MapFB)();
	break;

    case DKIOCBRIGHT:
	bp = (int *)data;
	nvram_check(&ni);
	*bp = ni.ni_brightness = SetCurBrightness(*bp);
	nvram_set(&ni);
	return 0;

    case DKIOCGNVRAM:
	if (!suser())
	    return u.u_error;
	nvram_check (data);
	return 0;

    case DKIOCSNVRAM:
	if (!suser())
	    return u.u_error;
	nip = (struct nvram_info*) data;
	nvram_check (&ni);
	
	/* change reboot arg if nvram boot cmd changed */
	if (strncmp (ni.ni_bootcmd, nip->ni_bootcmd, NVRAM_BOOTCMD)) {
	    extern char boot_dev[], boot_info[], boot_file[];
	    
	    strcpy (boot_dev, nip->ni_bootcmd);
	    boot_info[0] = 0;
	    boot_file[0] = 0;
	}
	/* flip state of auto poweron if requested */
	if (ni.ni_auto_poweron != nip->ni_auto_poweron)
		rtc_set_auto_poweron (nip->ni_auto_poweron);
	nvram_set (nip);
	return 0;
	
    case DKIOCGALARM:
    	return (rtc_alarm (0, data));
	
    case DKIOCSALARM:
    	if (!suser())
	    return u.u_error;
	return (rtc_alarm (data, 0));

    case DKIOCREGISTER: {
    	evioScreen *es = (evioScreen *)data;
	es->SetCursor = video_dev[vidFBnum].SetCursor;
	es->DisplayCursor = video_dev[vidFBnum].DisplayCursor;
	es->RemoveCursor = video_dev[vidFBnum].RemoveCursor;
	es->MoveCursor = vidMoveCursor;
	es->SetBrightness = NULL;
	vid_screen_token = ev_register_screen(es);
	if (vid_screen_token<0)
	    return(ENXIO);
	return(0);
    }
    /* can't do this in a close routine because it's needed per-process */
    case DKIOCDISABLE:
	if ((pcb->pcb_flags & PCB_TT1) == 0)
	    return ENXIO;
		
	/* disable transparent translation */
	pmap_tt (current_thread(), PMAP_TT_DISABLE, 0, 0, 0);
	return 0;

    /* Fetch and copy out the configuration information for the built-in frame buffer*/
    case DKIOCGFBINFO: {
	struct km_console_info kmcons;
	struct km_console_info *kp = *((struct km_console_info **) data);
	(*video_dev[vidFBnum].config)(&kmcons);
	return ( copyout( &kmcons, kp, sizeof (struct km_console_info) ) );
    }
    default:
	return ENOTTY;
    }
    return 0;
}

/*
 * Published kernel entry points for manipulating the console video device.
 */
 
/*
 * Scan down the list of video devices.  Return the FB number of the video
 * device found, or -1 if none is found.  This routine should only be called
 * from km.c, very early in the startup process, before probe_rb() is available.
 */
 int
vidProbeForFB()
{
	int fb_num;
	
	for ( fb_num = NUM_VIDEO_DEVS - 1; fb_num >= 0; --fb_num) {
	    if ( (*video_dev[fb_num].present)(vid_early_probe) == TRUE ){
		vidFBnum = fb_num;
		(*video_dev[fb_num].init)();
		return fb_num;
	    }
	}
	return -1;
}

/*
 * Called from open routine, if we haven't configured a frame buffer yet.
 * This routine uses the standard probe_rb() function to probe hardware.
 */
 static int
vidConfigureFB()
{
	int fb_num;
	extern int probe_rb();
	
	for ( fb_num = NUM_VIDEO_DEVS - 1; fb_num >= 0; --fb_num) {
	    if ( (*video_dev[fb_num].present)(probe_rb) == TRUE ){
		vidFBnum = fb_num;
		(*video_dev[fb_num].init)();
		return fb_num;
	    }
	}
	return -1;
}

/*
 * Fill in the console info structure for the current video device, if any.
 * vidProbeForFB() must have been called succesfully for this to work.  This must be 
 * done procedurally, due to the presence of the 'slotid' variable in address macros.
 */
 void
vidGetConsoleInfo(struct km_console_info *kp)
{
	if ( vidFBnum != VID_NO_FB )
		(*video_dev[vidFBnum].config)(kp);
}

/*
 * Generic 'stop animation' call.  Flag the boot ROM to stop the animation
 * after the next retrace interrupt.
 */
 void
vidStopAnimation()
{
	extern		struct mon_global *mon_global;
	struct		mon_global *mg = mon_global;

	/* stop animation permamently */
	*MG (char*, MG_flags) &= ~MGF_ANIM_RUN;
	DELAY (100000);
}

 void
vidSuspendAnimation()
{
	if ( vidFBnum != VID_NO_FB )
		(*video_dev[vidFBnum].suspendAnim)();
}
 void
vidResumeAnimation()
{
	if ( vidFBnum != VID_NO_FB )
		(*video_dev[vidFBnum].resumeAnim)();
}



/*
 * Set the brightness of the built-in frame buffer, if any.
 * This is called from SetCurBrightness(), in ev_kbd.c
 */
 void
vidSetBrightness( int level )
{
	if ( vidFBnum != VID_NO_FB )
		(*video_dev[vidFBnum].SetBrightness)((evioScreen *)0, level);
}

/*
 * Install a function as the vertical retrace interrupt handler.  The interrupt
 * passes control to our device specific code, which clears the interrupt.  Our
 * interrupt code then passes control to the specified function, to drive retrace
 * time events.
 */
 void
vidInterruptEnable( voidfptr_t func, void *arg )
{
	/* Make sure that a display device is configured */
	if ( vidFBnum == VID_NO_FB )
	{
		if ( vidConfigureFB() == -1 )
			return;
	}
	/* Save the function to be called and it's arg in a safe place. */
	vidIntrFunc = func;
	vidIntrFuncArg = arg;
	(*video_dev[vidFBnum].installIntr)();
}
/*
 * Disable vertical retrace interrupts.
 * Clear the interrupt handler pointer.
 */
 void
vidInterruptDisable()
{
	int s;
	/* Make sure that a display device is configured */
	if ( vidFBnum == VID_NO_FB )
		return;
	/* Clear out the function and it's arg. */
	s = splhigh();
	vidIntrFunc = (voidfptr_t) 0;
	vidIntrFuncArg = (void *) 0;
	splx(s);
}

/*
 * vidMoveCursor():
 *
 *	Move a cursor by calling the RemoveCursor function at the old position, and
 *	calling the DisplayCursor function at the new position.
 */
 void vidMoveCursor(evioScreen *es, Bounds oldCursorRect, Bounds cursorRect,
    int waitFrame, NXCursorData *nxCursorData)
{
    (*video_dev[vidFBnum].RemoveCursor)(es, oldCursorRect, nxCursorData);
    (*video_dev[vidFBnum].DisplayCursor)(es, cursorRect, waitFrame, nxCursorData);
}

/*
 * Probe routine which can be called very early in the startup sequence, before the
 * Unix/Mach trap handler and probe_rb() is available for use.
 */
static label_t vid_jmpbuf;

 static void
vid_probe_faulted()
{
	longjmp( &vid_jmpbuf );
}

 static int
vid_early_probe( int addr )
{
	void vid_probe_faulted();
	volatile int oldbuserr;
	volatile int oldadrerr;
	int * volatile vbp;
	volatile char *cp = (volatile char *) addr;
	int s;
	
	s = splhigh();
	vbp = (int *)get_vbr();		/* Get the vector base register */
	oldbuserr = vbp[(T_BUSERR/sizeof (int))];
	oldadrerr = vbp[(T_ADDRERR/sizeof (int))];
	vbp[(T_BUSERR/sizeof (int))] = (int) vid_probe_faulted;
	vbp[(T_ADDRERR/sizeof (int))] = (int) vid_probe_faulted;
	
	if ( setjmp( &vid_jmpbuf ) != 0 )
	{
	    vbp[(T_BUSERR/sizeof (int))] = (int) oldbuserr;
	    vbp[(T_ADDRERR/sizeof (int))] = (int) oldadrerr;
	    splx(s);
	    return 0;			/* The probe failed. */
	}
	
	addr = *cp;		/* Read the location in question. */
	
	vbp[(T_BUSERR/sizeof (int))] = (int) oldbuserr;
	vbp[(T_ADDRERR/sizeof (int))] = (int) oldadrerr;
	splx(s);
	return 1;			/* The probe succeeds */
}


/*
 *	2 Bit Display Device code.
 */
 
 static int
vid_BW2_present( int (*probe)() )
{
	switch (machine_type) {
	
	case NeXT_WARP9C:
		return FALSE;
	case NeXT_CUBE:
	case NeXT_WARP9:
	case NeXT_X15:
		return TRUE;
	}
	return FALSE;
}

 static void
vid_BW2_config(struct km_console_info * kp)
{
	/*
	 * If no other frame buffer is found, default to the 2 bit display.
	 */
	kp->pixels_per_word = NPPB * sizeof (int);
	kp->bytes_per_scanline = VIDEO_NBPL;
	kp->dspy_w = VIDEO_W;
	kp->dspy_max_w = VIDEO_MW;
	kp->dspy_h = VIDEO_H;
	kp->flag_bits = 0;
	kp->color[0] = WHITE;
	kp->color[1] = LT_GRAY;
	kp->color[2] = DK_GRAY;
	kp->color[3] = BLACK;
	
	kp->map_addr[KM_CON_FRAMEBUFFER].virt_addr = P_VIDEOMEM;
	kp->map_addr[KM_CON_FRAMEBUFFER].phys_addr = P_VIDEOMEM;
	kp->map_addr[KM_CON_FRAMEBUFFER].size = VIDEO_NBPL * VIDEO_H;
	kp->map_addr[KM_CON_BACKINGSTORE].virt_addr =
					P_VIDEOMEM + (VIDEO_NBPL * VIDEO_H);
	kp->map_addr[KM_CON_BACKINGSTORE].phys_addr =
					P_VIDEOMEM + (VIDEO_NBPL * VIDEO_H);
	kp->map_addr[KM_CON_BACKINGSTORE].size = (VIDEO_MH - VIDEO_H) * VIDEO_NBPL;
}

 static void
vid_BW2_init()
{
	struct nvram_info ni;
    
	nvram_check(&ni);		/* Unblank screen, in case ROM missed dspy. */
	vid_BW2_SetBrightness((evioScreen *)0, ni.ni_brightness);
}
/*
 * Transparently map the frame buffer into the user task's virtual memory.
 * Limit the maximum virtual address of the calling task to be less than the
 * mapped region, to avoid the chaos that would otherwise occur should the VM grow
 * to overlap the hardware addresses.
 *
 * This hardware includes a memory write function space to perform compositing.  In
 * this space, several physical addresses are aliased to the same video RAM through the
 * compositing hardware.  This forces us to serialize all accesses, so we can't cache
 * this memory.  This means we don't normally do burst reads or writes to the 2 bit
 * frame buffer!
 */
 static vm_offset_t
vid_BW2_MapFB()
{
	vm_offset_t addr;
	/*
	 *  Set the maximum address that can be used to the beginning
	 *  of video memory space.  XXX - Do some checking here?
	 */
	addr = P_VIDEOMEM & ~(VID_SIZE - 1);
	vm_map_max(current_task()->map) = addr;

	/* setup transparent translation, UNCACHED! */
	pmap_tt (current_thread(), PMAP_TT_ENABLE, addr, VID_SIZE,
		PMAP_TT_NON_CACHEABLE);
	return ( (vm_offset_t) P_VIDEOMEM );
}
/*
 * vid_BW2_DisplayCursor displays the cursor on the MegaPixel framebuffer by first
 * saving what's underneath the cursor, then drawing the cursor there.
 * NOTE: The topleft of the cursorRect passed in is not necessarily the
 * cursor location. The cursorRect is adjusted to compensate for
 * the cursor hotspot.
 */

void vid_BW2_DisplayCursor(evioScreen *es, Bounds cursorRect, int waitFrame,
		      NXCursorData *nxCursorData)
{
    Bounds bounds;			/* screen bounds */
    Bounds *saveRect;			/* screen rect saved */
    unsigned int vramRow;		/* screen row longs (4-byte chunks) */
    volatile unsigned int *cursPtr;	/* cursor data pointer */
    volatile unsigned int *vramPtr;	/* screen data pointer */
    volatile unsigned int *savePtr;	/* saved screen data pointer */
    volatile unsigned int *maskPtr;	/* cursor mask pointer */
    int i, doLeft, doRight, skew, rSkew;

    vramPtr = (unsigned int *)P_VIDEOMEM;
    vramRow = 72;
    bounds = es->bounds;

    saveRect = nxCursorData->saveRect;
    *saveRect = cursorRect;
    /* Clip saveRect vertical within screen bounds */
    if (saveRect->miny < bounds.miny) saveRect->miny = bounds.miny;
    if (saveRect->maxy > bounds.maxy) saveRect->maxy = bounds.maxy;
    i = cursorRect.minx-bounds.minx;
    saveRect->minx = i-(i & PPXMASK)+bounds.minx;
    saveRect->maxx = saveRect->minx + CURSORWIDTH*2;
    
    vramPtr += (vramRow * (saveRect->miny - bounds.miny)) +
	       ((saveRect->minx - bounds.minx) >> 4);

    skew = (cursorRect.minx & PPXMASK)<<1; /* skew is in bits */
    rSkew = 32-skew;

    savePtr = nxCursorData->saveData;
    if (waitFrame) {
	cursPtr = waitData2;
	maskPtr = waitMask2;
    } else {
	cursPtr = nxCursorData->cursorData2W;
	maskPtr = nxCursorData->cursorAlpha2;
    }
    cursPtr += saveRect->miny - cursorRect.miny;
    maskPtr += saveRect->miny - cursorRect.miny;

    doLeft = (saveRect->minx >= bounds.minx);
    doRight = (saveRect->maxx <= bounds.maxx);

    for (i = saveRect->maxy - saveRect->miny-1; i>=0; i--) {
	register unsigned int workreg;

	if (doLeft) {
	    *savePtr++ = workreg = *vramPtr;
	    *vramPtr = (workreg&(~((*maskPtr)>>skew))) | ((*cursPtr)>>skew);
	}
	if (doRight) {
	    *savePtr++ = workreg = *(vramPtr+1);
	    *(vramPtr+1) = (workreg & (~((*maskPtr)<<rSkew)))
			   | ((*cursPtr)<<rSkew);
	}
	vramPtr += vramRow;
	cursPtr++;
	maskPtr++;
    }
}


/*
 * vidRemoveCursor erases the cursor by replacing the background image that
 * was saved by the previous call to vidDisplayCursor.
 */

void vid_BW2_RemoveCursor(evioScreen *es, Bounds cursorRect, NXCursorData
		     *nxCursorData)
{
    Bounds *saveRect = nxCursorData->saveRect;
    int i, doLeft, doRight;
    unsigned int vramRow, lmask, rmask;
    volatile unsigned int *vramPtr;
    volatile unsigned int *savePtr;
    Bounds bounds = es->bounds;
    static unsigned int vid_mask_array[16] = {
	0xFFFFFFFF,0x3FFFFFFF,0x0FFFFFFF,0x03FFFFFF,
	0x00FFFFFF,0x003FFFFF,0x000FFFFF,0x0003FFFF,
	0x0000FFFF,0x00003FFF,0x00000FFF,0x000003FF,
        0x000000FF,0x0000003f,0x0000000F,0x00000003};

    vramPtr = (unsigned int *)P_VIDEOMEM;
    vramRow = 72;
    vramPtr += (vramRow * (saveRect->miny - bounds.miny)) +
	       ((saveRect->minx - bounds.minx) >> 4);
    savePtr = nxCursorData->saveData;
    if (doLeft = (saveRect->minx >= bounds.minx))
    	lmask = vid_mask_array[cursorRect.minx-saveRect->minx];
    if (doRight = (saveRect->maxx <= bounds.maxx))
	rmask = ~vid_mask_array[16-(saveRect->maxx-cursorRect.maxx)];
    for (i = saveRect->maxy-saveRect->miny; i>0; i--) {
	if (doLeft) *vramPtr = (*vramPtr&(~lmask))|(*savePtr++&lmask);
	if (doRight) *(vramPtr+1)=(*(vramPtr+1)&(~rmask))|(*savePtr++&rmask);
	vramPtr += vramRow;
    }
}


void vid_BW2_SetCursor(evioScreen *es, unsigned int *data, unsigned int *mask,
    int drow, int mrow, int waitFrame, NXCursorData *nxCursorData)
{
    int i;
    unsigned int alpha;
    volatile unsigned int *cm, *cd;
    
    mrow >>= 2;
    drow >>= 2;
    if (waitFrame) {
	cd = waitData2;
	cm = waitMask2;
    } else {
	cd = nxCursorData->cursorData2W;
	cm = nxCursorData->cursorAlpha2;
    }
    for (i = CURSORHEIGHT; i > 0; i--) {
	*cm++ = alpha = *mask;
	*cd++ = alpha - *data;
	mask += mrow;
	data += drow;
    }
}

/*
 * Set the brightness of the MegaPixel display.
 */
static void
vid_BW2_SetBrightness(evioScreen *es, int level)
{
    extern volatile u_char *brightness;		/* See scr.h, next_init.c */
    static char	bright_table[BRIGHT_MAX+1] = {
	0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3e, 0x3d, 0x3b, 0x37, 0x2f,
	0x1e, 0x3c, 0x39, 0x33, 0x27, 0x0e, 0x1d, 0x3a, 0x35, 0x2b, 0x16,
	0x2c, 0x18, 0x30, 0x21, 0x02, 0x05, 0x0b, 0x17, 0x2e, 0x1c, 0x38,
	0x31, 0x23, 0x06, 0x0d, 0x1b, 0x36, 0x2d, 0x1a, 0x34, 0x29, 0x12,
	0x24, 0x08, 0x11, 0x22, 0x04, 0x09, 0x13, 0x26, 0x0c, 0x19, 0x32,
	0x25, 0x0a, 0x15, 0x2a, 0x14, 0x28, 0x10
    };

    level = bright_table[level];
    *brightness = level | BRIGHT_ENABLE;
}

/*
 * Install a retrace interrupt handler that knows how to ack interrupts from
 * the installed frame buffer.  This handler will then call softint_run().
 */
static void
vid_BW2_installIntr()
{
	struct dma_dev *vddp = (struct dma_dev*) P_VIDEO_CSR;

	install_scanned_intr(I_VIDEO, vid_BW2_intr, (void *)0);
	vddp->dd_limit = (char*) RETRACE_LIMIT;		/* Enable interrupt */
}
/*
 * Ack the interrupt from the 2 bit frame buffer, and then transfer control to
 * the soft int package to run retrace time tasks.
 */
static void
vid_BW2_intr()
{
	struct dma_dev *vddp = (struct dma_dev*) P_VIDEO_CSR;

	vddp->dd_csr = DMACMD_RESET;			/* ack interrupt */
	if ( vidIntrFunc != (voidfptr_t) 0 )
		(*vidIntrFunc)( vidIntrFuncArg );
}

static void
vid_BW2_SuspendAnim()
{
	struct dma_dev *vddp = (struct dma_dev*) P_VIDEO_CSR;

	/* stop animation temporarily */
	vddp->dd_limit = (char*) RETRACE_START;
	vddp->dd_csr = DMACMD_RESET;
	DELAY (100000);
}

static void
vid_BW2_ResumeAnim()
{
	struct dma_dev *vddp = (struct dma_dev*) P_VIDEO_CSR;

	/* continue animation after being stopped temporarily */
	vddp->dd_limit = (char*) RETRACE_LIMIT;
}

#if defined(COLOR_FB)
static const unsigned char Gamma[] =
{	/* Gamma 2.0 CLUT, suitable for Trinitron tubes */
    0,      16,     23,     28,     32,     36,     39,     42,
    45,     48,     50,     53,     55,     58,     60,     62,
    64,     66,     68,     70,     71,     73,     75,     77,
    78,     80,     81,     83,     84,     86,     87,     89,
    90,     92,     93,     94,     96,     97,     98,     100,
    101,    102,    103,    105,    106,    107,    108,    109,
    111,    112,    113,    114,    115,    116,    117,    118,
    119,    121,    122,    123,    124,    125,    126,    127,
    128,    129,    130,    131,    132,    133,    134,    135,
    135,    136,    137,    138,    139,    140,    141,    142,
    143,    144,    145,    145,    146,    147,    148,    149,
    150,    151,    151,    152,    153,    154,    155,    156,
    156,    157,    158,    159,    160,    160,    161,    162,
    163,    164,    164,    165,    166,    167,    167,    168,
    169,    170,    170,    171,    172,    173,    173,    174,
    175,    176,    176,    177,    178,    179,    179,    180,
    181,    181,    182,    183,    183,    184,    185,    186,
    186,    187,    188,    188,    189,    190,    190,    191,
    192,    192,    193,    194,    194,    195,    196,    196,
    197,    198,    198,    199,    199,    200,    201,    201,
    202,    203,    203,    204,    204,    205,    206,    206,
    207,    208,    208,    209,    209,    210,    211,    211,
    212,    212,    213,    214,    214,    215,    215,    216,
    217,    217,    218,    218,    219,    220,    220,    221,
    221,    222,    222,    223,    224,    224,    225,    225,
    226,    226,    227,    228,    228,    229,    229,    230,
    230,    231,    231,    232,    233,    233,    234,    234,
    235,    235,    236,    236,    237,    237,    238,    238,
    239,    240,    240,    241,    241,    242,    242,    243,
    243,    244,    244,    245,    245,    246,    246,    247,
    247,    248,    248,    249,    249,    250,    250,    251,
    251,    252,    252,    253,    253,    254,    254,    255,
};
	
/*
 *	16 Bit Color Display Device code.
 */
 static int
vid_C16_present( int (*probe)() )
{
	switch (machine_type) {
	
	case NeXT_WARP9C:
		return TRUE;
	case NeXT_CUBE:
	case NeXT_WARP9:
	case NeXT_X15:
		return FALSE;
	}
	return FALSE;
}

 static void
vid_C16_config(struct km_console_info * kp)
{
	kp->pixels_per_word = C16_NPPW;
	kp->bytes_per_scanline = C16_VIDEO_NBPL;
	kp->dspy_w = C16_VIDEO_W;
	kp->dspy_max_w = C16_VIDEO_MW;
	kp->dspy_h = C16_VIDEO_H;
	kp->flag_bits = 0;
	kp->color[0] = C16_WHITE;
	kp->color[1] = C16_LT_GRAY;
	kp->color[2] = C16_DK_GRAY;
	kp->color[3] = C16_BLACK;
	kp->map_addr[KM_CON_FRAMEBUFFER].virt_addr = P_C16_VIDEOMEM;
	kp->map_addr[KM_CON_FRAMEBUFFER].phys_addr = P_C16_VIDEOMEM;
	kp->map_addr[KM_CON_FRAMEBUFFER].size =
		    C16_VIDEO_NBPL * C16_VIDEO_H;
	kp->map_addr[KM_CON_BACKINGSTORE].virt_addr =
		    P_C16_VIDEOMEM + (C16_VIDEO_NBPL * C16_VIDEO_H);
	kp->map_addr[KM_CON_BACKINGSTORE].phys_addr =
		    P_C16_VIDEOMEM + (C16_VIDEO_NBPL * C16_VIDEO_H);
	kp->map_addr[KM_CON_BACKINGSTORE].size =
		    (C16_VIDEO_MH - C16_VIDEO_H) * C16_VIDEO_NBPL;
}

/*
 * Initialize the RAMDAC to: blink off, all channels unmasked.
 * Load the LUT, via SetBrightness. 
 *
 * This is straight out of the BrookTree 463 manual.  Please
 * refer to the part manual for details.
 */
 static void
vid_C16_init()
{
	volatile unsigned char *dac0 = ((unsigned char *)P_C16_DAC_0);
	volatile unsigned char *dac1 = ((unsigned char *)P_C16_DAC_1);
	volatile unsigned char *dac2 = ((unsigned char *)P_C16_DAC_2);
	volatile unsigned char *dac3 = ((unsigned char *)P_C16_DAC_3);
	int index;
	unsigned int level, scale;
	struct nvram_info ni;
    
	nvram_check(&ni);	/* Unblank screen, in case ROM missed dspy. */

	level = ni.ni_brightness;
	scale = (level*64)/BRIGHT_MAX;
	
	// Load up command register 0, 0x201
	*dac0 = 0x01;
	*dac1 = 0x02;
		*dac2 = 0x40;	// 1:1 interleave
	// Command register 1, 0x202
	*dac0 = 0x02;
	*dac1 = 0x02;
		*dac2 = 0x00;	// ??
	// Command register 2, 0x203
	*dac0 = 0x03;
	*dac1 = 0x02;
		*dac2 = 0x80;	// Enable sync, no pedestal
	// Pixel read mask, 0x205-0x208
	*dac0 = 0x05;
	*dac1 = 0x02;
		*dac2 = 0xF0;	// high nibble enabled
	*dac0 = 0x06;
	*dac1 = 0x02;
		*dac2 = 0xF0;	// high nibble enabled
	*dac0 = 0x07;
	*dac1 = 0x02;
		*dac2 = 0xF0;	// high nibble enabled
	*dac0 = 0x08;
	*dac1 = 0x02;
		*dac2 = 0xF0;	// high nibble enabled

	// Blink mask, 0x209-0x20c
	*dac0 = 0x09;
	*dac1 = 0x02;
		*dac2 = 0x0;	// All bits disabled
	*dac0 = 0x0a;
	*dac1 = 0x02;
		*dac2 = 0x0;	// All bits disabled
	*dac0 = 0x0b;
	*dac1 = 0x02;
		*dac2 = 0x0;	// All bits disabled
	*dac0 = 0x0c;
	*dac1 = 0x02;
		*dac2 = 0x0;	// All bits disabled

	// Tag table 0x300 - 0x30F. 24 bit word loaded w 3 writes
	for ( index = 0x300; index <= 0x30F; ++index )
	{
		*dac0 = index & 0xFF;
		*dac1 = (index & 0xFF00) >> 8;
			*dac2 = 0x0;
			*dac2 = 0x1;
			*dac2 = 0x0;
	}
	
	// Load a ramp in the palatte. Second 256 locations
	for (index = 0x100; index < 0x200; index++)
	{
	        level = (unsigned int)Gamma[index-0x100];
		*dac0 = index & 0xFF;
		*dac1 = (index & 0xFF00) >> 8;
		    *dac3 = level;
		    *dac3 = level;
		    *dac3 = level;
	}	
	// Last 4 locations 0x20C to 0x20F must be FF 
	for (index = 0x20C; index < 0x210; index++)
	{
		*dac0 = index & 0xFF;
		*dac1 = (index & 0xFF00) >> 8;
		    *dac3 = 0xFF;
		    *dac3 = 0xFF;
		    *dac3 = 0xFF;
	}
	/* Set the 1st 256 locations, according to current brightness. */
	vid_C16_SetBrightness((evioScreen *)0, ni.ni_brightness);
}	
/*
 * Transparently map the frame buffer into the user task's virtual memory.
 * Limit the maximum virtual address of the calling task to be less than the
 * mapped region, to avoid the chaos that would otherwise occur should the VM grow
 * to overlap the hardware addresses.
 * 
 * There's no magic hardware here to do compositing, and therefore no physical address
 * aliasing problem.  We can cache this region and do burst reads and writes.
 */
 static vm_offset_t
vid_C16_MapFB()
{
	vm_offset_t addr;
	/*
	 *  Set the maximum address that can be used to the beginning
	 *  of video memory space.  XXX - Do some checking here?
	 */
	addr = P_C16_VIDEOMEM & ~(C16_VID_SIZE - 1);
	vm_map_max(current_task()->map) = addr;

	/* setup transparent translation, CACHED! */
	pmap_tt (current_thread(), PMAP_TT_ENABLE, addr, C16_VID_SIZE,
		PMAP_TT_CACHEABLE);
	return ( (vm_offset_t) P_C16_VIDEOMEM );
}
/*
 * vid_C16_DisplayCursor displays the cursor on the Warp 9C framebuffer by first
 * saving what's underneath the cursor, then drawing the cursor there.
 * NOTE: The topleft of the cursorRect passed in is not necessarily the
 * cursor location. The cursorRect is adjusted to compensate for
 * the cursor hotspot.
 *
 * Since the frame buffer is cacheable, flush at the end of the drawing
 * operation.
 */

void vid_C16_DisplayCursor(evioScreen *es, Bounds cursorRect, int waitFrame,
		      NXCursorData *nxCursorData)
{
    Bounds bounds;			/* screen bounds */
    Bounds *saveRect;			/* screen rect saved */
    unsigned int vramRow;		/* screen row */
    volatile unsigned short *cursPtr;	/* cursor data pointer */
    volatile unsigned short *vramPtr;	/* screen data pointer */
    vm_offset_t startPtr;		/* Starting screen data pointer */
    volatile unsigned short *savePtr;	/* saved screen data pointer */
    volatile unsigned short *maskPtr;	/* cursor mask pointer */
    int width, cursRow;
    unsigned short s, d, f;
    int i, j;

    vramPtr = (unsigned short *)P_C16_VIDEOMEM;
    vramRow = C16_VIDEO_MW;	/* Scanline width in pixels */
    bounds = es->bounds;

    saveRect = nxCursorData->saveRect;
    *saveRect = cursorRect;
    /* Clip saveRect vertical within screen bounds */
    if (saveRect->miny < bounds.miny) saveRect->miny = bounds.miny;
    if (saveRect->maxy > bounds.maxy) saveRect->maxy = bounds.maxy;
    if (saveRect->minx < bounds.minx) saveRect->minx = bounds.minx;
    if (saveRect->maxx > bounds.maxx) saveRect->maxx = bounds.maxx;
    vramPtr += (vramRow * (saveRect->miny - bounds.miny)) +
	       (saveRect->minx - bounds.minx);
    width = saveRect->maxx - saveRect->minx;
    vramRow -= width;
    
    switch(waitFrame) {
	case 1:
	case 2:
	case 3:
	    cursPtr = C16_waitCursors[waitFrame-1];
	    break;
	default:
	case 0:
	    cursPtr = (volatile unsigned short *)nxCursorData->cursorData16;
	    break;
    }
    startPtr = (vm_offset_t) vramPtr;
    savePtr = (volatile unsigned short *)nxCursorData->saveData;
    /* Index into the cursor data by an appropriate amount */
    cursPtr += (saveRect->miny - cursorRect.miny) * CURSORWIDTH +
    	       (saveRect->minx - cursorRect.minx);
    cursRow = CURSORWIDTH - width;
    for (i = saveRect->maxy - saveRect->miny; i>0; i--) {
	for (j = width; j>0; j--) {
	    d = *savePtr++ = *vramPtr;
	    s = *cursPtr++;
	    f = (~s) & (unsigned int)AMASK;
	    d = s + (((((d & RBMASK)>>4)*f + GAMASK) & RBMASK)
		| ((((d & GAMASK)*f+GAMASK)>>4) & GAMASK));
	    *vramPtr++ = d;
	}
	cursPtr += cursRow; /* starting point of next cursor line */
	vramPtr += vramRow; /* starting point of next screen line */
    }
    /* Flush the pixels modified by the cursor drawing operation */
    while( startPtr < (vm_offset_t)vramPtr )
    {
	cache_push_page( startPtr );	/* Assumes phys addr == virt addr */
	startPtr += PAGE_SIZE;
    }
}


/*
 * vid_C16_RemoveCursor erases the cursor by replacing the background image that
 * was saved by the previous call to vidDisplayCursor.
 *
 * Since the frame buffer is cacheable, flush at the end of the drawing
 * operation.
 */

void vid_C16_RemoveCursor(evioScreen *es, Bounds cursorRect, NXCursorData
		     *nxCursorData)
{
    Bounds *saveRect = nxCursorData->saveRect;
    int i, j, width;
    unsigned int vramRow;
    volatile unsigned short *vramPtr;
    volatile unsigned short *savePtr;
    vm_offset_t startPtr;		/* Starting screen data pointer */
    Bounds bounds = es->bounds;

    vramPtr = (unsigned short *)P_C16_VIDEOMEM;
    vramRow = C16_VIDEO_MW;	/* Scanline width in pixels */
    vramPtr += (vramRow * (saveRect->miny - bounds.miny)) +
	       (saveRect->minx - bounds.minx);
    vramRow -= (saveRect->maxx - saveRect->minx);
    savePtr = (volatile unsigned short *)nxCursorData->saveData;
    startPtr = (vm_offset_t) vramPtr;
    width = saveRect->maxx - saveRect->minx;
    for (i = saveRect->maxy - saveRect->miny; i>0; i--) {
	for (j = width; j>0; j--)
		*vramPtr++ = *savePtr++;
	vramPtr += vramRow;
    }
    /* Flush the pixels modified by the cursor erase operation */
    while( startPtr < (vm_offset_t)vramPtr )
    {
	cache_push_page( startPtr );	/* Assumes phys addr == virt addr */
	startPtr += PAGE_SIZE;
    }
}


void vid_C16_SetCursor(evioScreen *es, unsigned int *data, unsigned int *mask,
    int drow, int mrow, int waitFrame, NXCursorData *nxCursorData)
{
    volatile unsigned short *cp;
    unsigned int d, m;
    unsigned short e, f;
    int i, j;
    
    switch (waitFrame) {
	case 1:
	case 2:
	case 3:
	    if (C16_waitCache[waitFrame-1])	/* Already in cache? */
		return;
	    C16_waitCache[waitFrame-1]++;
	    cp = (volatile unsigned short *)C16_waitCursors[waitFrame-1];
	    break;
	default:
	case 0:
	    cp = (volatile unsigned short *)nxCursorData->cursorData16;
	    break;
    }
    mrow >>= 2;
    drow >>= 2;
    for (i=16; i>0; i--) {
	m = *mask; mask += mrow;	/* Get mask data and inc ptr */
        d = *data; data += drow;	/* Get grey data and convert */
	for (j=30; j>=0; j-=2) {	/* Grind through 16 packed 2 bit pixels */
	    e = (d>>j) & 3;		/* isolate single 2bit data */
	    e = (e << 2) | e;		/* And replicate over 12 bits */
	    e = (e << 4) | e;
	    e = ((e << 8) | e) & ~AMASK;
	    f = (m>>j) & 3;		/* isolate single 2bit mask (alpha) */
	    e |= (f << 2) | f;		/* Replicate to 4 bit alpha */
	    *cp++ = e;
	}
    }
}

/*
 * Set the brightness of the Warp 9C display.  Brightness is set by scaling the
 * contents of the LUT in the RAMDAC!
 *
 * This is derived from the BrookTree 463 manual.  Please
 * refer to the part manual for details on loading the CLUT.
 */
static void
vid_C16_SetBrightness(evioScreen *es, unsigned int level)
{
	volatile unsigned char *dac0 = ((unsigned char *)P_C16_DAC_0);
	volatile unsigned char *dac1 = ((unsigned char *)P_C16_DAC_1);
	volatile unsigned char *dac2 = ((unsigned char *)P_C16_DAC_2);
	volatile unsigned char *dac3 = ((unsigned char *)P_C16_DAC_3);
	int index;
	unsigned int scale;

	scale = (level*64)/BRIGHT_MAX;
	for (index = 0; index < 256; index++)
	{
	        level = (unsigned int)((Gamma[index]*scale)>>6) + 0x08;
		if ( level > 0xFF )
			level = 0xFF;
		*dac0 = index & 0xFF;
		*dac1 = (index & 0xFF00) >> 8;
		    *dac3 = level;
		    *dac3 = level;
		    *dac3 = level;
	}	
}
/*
 * Install a retrace interrupt handler that knows how to ack interrupts from
 * the installed frame buffer.  This handler will then call softint_run().
 */
static void
vid_C16_installIntr()
{
	volatile char *vcsr = (volatile char *) P_C16_CMD_REG;

	install_scanned_intr(I_C16_VIDEO, vid_C16_intr, (void *)0);
	*vcsr = C16_CMD_INTRENA | C16_CMD_UNBLANK; /* enable interrupt */
}
/*
 * Ack the interrupt from the color frame buffer, and then transfer control to
 * the soft int package to run retrace time tasks.
 */
static void
vid_C16_intr()
{
	volatile char *vcsr = (volatile char *) P_C16_CMD_REG;

	*vcsr = C16_CMD_CLRINTR | C16_CMD_UNBLANK;	/* Clear it. */
	if ( vidIntrFunc != (voidfptr_t) 0 )
		(*vidIntrFunc)( vidIntrFuncArg );
	*vcsr = C16_CMD_INTRENA | C16_CMD_UNBLANK;	/* enable it */
}
/*
 * Suspend and resume the boot animation, by disabling and enabling the
 * vertical retrace interrupt.
 */
static void
vid_C16_SuspendAnim()
{
	volatile char *vcsr = (volatile char *) P_C16_CMD_REG;

	/* Temporarily stop animation by disabling VBL interrupt. */
	*vcsr = C16_CMD_UNBLANK;
}

static void
vid_C16_ResumeAnim()
{
	volatile char *vcsr = (volatile char *) P_C16_CMD_REG;

	/* continue animation after being stopped temporarily */
	*vcsr = C16_CMD_INTRENA | C16_CMD_UNBLANK;
}


#endif /* COLOR_FB */
