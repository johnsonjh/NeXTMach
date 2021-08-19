/******************************************************************************
    ev_kbd.c

    Keyboard-only portions of the ev driver, in particular
    the key mapping code.
    
    Created 31Dec87 Leo
    
    Copyright 1987 NeXT, Inc.
    
    Modified:
    
    10Dec88 Leo  Merged ev_kbd and ev_keymap; moved all variables to ev_vars
    11Dec89 Ted  Added Leo's fix to kmem_free call for keyboard mapping.
    13Dec89 Ted  ANSI.
    19Feb90 Ted  Modified SetCurBrightness to call all connected drivers.
    17Apr90 Ted  Made only one key repeatable at a time. (downKey mods)
    17Apr90 Ted  New defaultMapping changes Shift-Tab from 0x09 to 0x19.
    03May90 Ted  Fixed bug 3697 in DoSpecialKey(): UndoAutoDim() called.
    06Jul90 Mike Modified SetCurBrightness to get rid of two bit dspy special case.
******************************************************************************/

#import <sys/types.h>
#import <nextdev/monreg.h>
#import <nextdev/kmreg.h>
#import <nextdev/video.h>
#import <mon/global.h>
#import <next/scr.h>
#import <sys/param.h>
#import <sys/callout.h>
#import <kern/thread.h>
#import <vm/vm_kern.h>
#import <nextdev/snd_snd.h>
#import <nextdev/ev_vars.h>

void AllKeysUp();

/* Key repeat parameters */
#define DEFAULTINITIALREPEAT 35
#define DEFAULTKEYREPEAT 8

/******************************************************************************
    downKey and downCount keep a record of the key that is currently down.
    This is used by DoKbdRepeat, the routine called once per vertical retrace,
    to generate key-repeated events.  This is maintained in the routine
    DoCharGen, which is called when a physical key goes down or up.
******************************************************************************/

static short downCount = -1;
static short downKey = -1;

struct unmapped_key_event {
    u_int : 16,
    valid : 1,
    deviceMods : 7,
    up_down : 1,
    key_code : 7;
};

/******************************************************************************
    pseudo-key codes for the keys whose transitions are reported to
    the driver only via changes in deviceMods.  That is, the hardware
    reports these changes to the driver as changes in deviceMods; the
    driver synthesizes keyDown/keyUp transitions from that, and feeds
    that to process_kbd_event like any other key transition; and 
    process_kbd_event often turns that transition into a Flags-changed
    event and updates evGlobals->eventFlags.  The reason, though, for
    going through the key transition step is to allow the key mapping
    to remap what those keys do.  This array is referenced by
    process_device_mods, below.
******************************************************************************/

static unsigned char modKeys[] = { 0x51,0x52,0x57,0x54,0x55,0x53,0x56 };

/******************************************************************************
    These are the key codes for the device control keys that affect sound and
    volume.
******************************************************************************/

static unsigned char specialKeyCodes[] = { SOUND_UP_KEY, SOUND_DOWN_KEY,
    BRIGHTNESS_UP_KEY, BRIGHTNESS_DOWN_KEY } ;


void process_kbd_event(struct unmapped_key_event *ke)
{
    unsigned char dMods;
    
    dMods = ke->deviceMods & VALIDMODSMASK;
    if (!ke->up_down) /* If going down */
    	/* Process meta bits before */
    	if (dMods != deviceMods)
	    (void)process_device_mods(dMods);
    /* Post key event to low-level queue */
    if (ke->valid) DoKbdEvent(ke->key_code,!ke->up_down,dMods);
    if (ke->up_down) /* If going up */
    	/* Process meta bits after */
    	if (dMods != deviceMods)
	    (void)process_device_mods(dMods);
    deviceMods = dMods;
}

void process_device_mods(dMods)
    unsigned char dMods;
{
    int	i;
    unsigned char cm;

    for(i=0,cm = 1;i<7;i++,cm <<= 1)
	if ((dMods ^ deviceMods) & cm)
	{
	    deviceMods ^= cm; /* Do the update first */
	    DoKbdEvent(modKeys[i],((deviceMods & cm) != 0),deviceMods);
	}
}

void AlphaLockFeedback(int state)
{
    km_send(MON_KM_USRREQ,KM_SET_LEDS(0,(state?(KM_LED_LEFT|KM_LED_RIGHT):0)));
}

/******************************************************************************
    SetCurBrightness clips the given brightness level to be in range, and set
    the current brightness to that level. It returns the level actually set.
******************************************************************************/

int SetCurBrightness(int level)
{
    int	i;
    void (*proc)();

    level = MAX(level, BRIGHT_MIN);
    level = MIN(level, BRIGHT_MAX);
    
    /* Generic Driver Code */
    vidSetBrightness( level );		/* Always set the built-in display. */
    /* Tell all drivers to change brightness level */
    if (evOpenCalled)
	for (i=0; i<evp->screens; i++)
	    if (proc = evp->screen[i].SetBrightness)
		(*proc)(&evp->screen[i], level);
    return(level);
}

/******************************************************************************
    DoAutoDim reduces the current brightness to a fraction of its previous
    value.  It stores the old value in undimmedBrightness and sets autoDimmed.
******************************************************************************/

#define BRIGHT_NUM 1
#define BRIGHT_DENOM 3

void DoAutoDim()
{
    SetCurBrightness((curBright*BRIGHT_NUM)/BRIGHT_DENOM);
    autoDimmed = 1;
}

/******************************************************************************
    UndoAutoDim restores full brightness and clears autoDimmed.
******************************************************************************/

void UndoAutoDim()
{
    SetCurBrightness(curBright);
    autoDimTime = eventGlobals->VertRetraceClock + autoDimPeriod;
    autoDimmed = 0;
}

/******************************************************************************
    RecordBrightness
    records the current brightness in nvram.
******************************************************************************/

void RecordBrightness()
{
    int new;
    struct nvram_info ni;

    nvram_check(&ni);
    ni.ni_brightness = curBright;
    nvram_set(&ni);
} 

void DoBrightnessChange(int change)
{
    curBright = SetCurBrightness(curBright+change);
} 

#define LEFT_CHANNEL 1
#define RIGHT_CHANNEL 2
#define BOTH_CHANNELS 3

void SetAttenuation(int whichOnes,int newLevel)
{
    if (whichOnes & LEFT_CHANNEL)
    	vol_l = MAX(MIN(newLevel,VOLUME_MAX),VOLUME_MIN);
    if (whichOnes & RIGHT_CHANNEL)
    	vol_r = MAX(MIN(newLevel,VOLUME_MAX),VOLUME_MIN);
}

void DoSoundKey(int key, int direction, int flags)
{
    int change;
    
    /* Note that what we really set is attenuation, so if the
       guy hits the up key we decrease attentuation, and vice versa */
    if (key == SOUND_UP_KEY)
    	change = -1;
    else
    	change = 1;
    if (flags & NX_COMMANDMASK)
    	if (flags & NX_ALTERNATEMASK)
	    if (key == SOUND_UP_KEY)
	    	/* Raise volume to maximum of two channels */
	    	/* or, lower attenuation to minimum of the two channels */
		SetAttenuation(BOTH_CHANNELS,MIN(vol_l,vol_r));
	    else
	    	/* Opposite: lower volume to minimum of l and r */
		/* or, raise attentuation to maximum of l and r */
		SetAttenuation(BOTH_CHANNELS,MAX(vol_l,vol_r));
	else /* command key is down, but alternate is not */
	    if (key == SOUND_UP_KEY) /* toggle lowpass filter */
	    	gpflags ^= SGP_LOWPASS;
	    else /* toggle speaker enable */
	    	gpflags ^= SGP_SPKREN;
    else /* No command key */
    	/* We will change the volume of one or both channels */
    	switch(flags & (NX_NEXTLALTKEYMASK|NX_NEXTRALTKEYMASK)) {
	case NX_NEXTLALTKEYMASK:
	    SetAttenuation(LEFT_CHANNEL,vol_l+change);
	    break;
	case 0:
	default:
	    SetAttenuation(LEFT_CHANNEL,vol_l+change);
	case NX_NEXTRALTKEYMASK:
	    SetAttenuation(RIGHT_CHANNEL,vol_r+change);
	    break;
	} /* switch() */	
    snd_device_vol_set();
}

/******************************************************************************
    DoSpecialKey treats those keys that are flagged as requiring special
    device processing.  This is used for brightness and volume keys.  It
    accepts direction (0 up, 1 down, 2 repeat).
******************************************************************************/

void DoSpecialKey(int key, int direction, int flags)
{
    if (direction != 0) /* If not an up event */
	switch(key) {
	case SOUND_UP_KEY:
	case SOUND_DOWN_KEY:
	    DoSoundKey(key,direction,flags);
	    break;
	case BRIGHTNESS_UP_KEY:
	case BRIGHTNESS_DOWN_KEY:
	    if (autoDimmed)
	        UndoAutoDim();
	    curBright = SetCurBrightness(curBright + 
	    	((key == BRIGHTNESS_UP_KEY) ? 1 : -1) );
	    break;
	} /* switch(key) */
    else /* it is an up key */
    	switch(key) {
	case SOUND_UP_KEY:
	case SOUND_DOWN_KEY:
	    snd_device_vol_save();
	    break;
	case BRIGHTNESS_UP_KEY:
	case BRIGHTNESS_DOWN_KEY:
	    /* Here, we launch a softint to record the current brightness.
	       That way, the nvram code called by RecordCurBrightness won't
	       be called from interrupt level (which is where we are now). */
	    softint_sched(CALLOUT_PRI_THREAD, RecordBrightness, 0);
	    break;
	} /* switch(key) */	
}

/* This variable generated by keymap except for the one edit noted below.  */
const unsigned char defaultMapping[] = {
	0x00,0x00,0x05,0x01,0x02,0x52,0x57,0x02,0x01,0x51,
	0x03,0x02,0x53,0x56,0x04,0x02,0x54,0x55,0x05,0x16,
	0x0b,0x0c,0x0d,0x11,0x17,0x14,0x15,0x13,0x18,0x12,
	0x21,0x22,0x23,0x24,0x25,0x28,0x27,0x26,0x09,0x0f,
	0x16,0x10,0x51,0xff,0xff,0xff,0x0e,0x00,0x5c,0x00,
	0x7c,0x00,0x1c,0x00,0x1c,0x00,0xe3,0x00,0xeb,0x00,
	0x1c,0x00,0x1c,0x0e,0x00,0x5d,0x00,0x7d,0x00,0x1d,
	0x00,0x1d,0x00,0x27,0x00,0xba,0x00,0x1d,0x00,0x1d,
	0x0e,0x00,0x5b,0x00,0x7b,0x00,0x1b,0x00,0x1b,0x00,
	0x60,0x00,0xaa,0x00,0x1b,0x00,0x1b,0x0d,0x00,0x69,
	0x00,0x49,0x00,0x09,0x00,0x09,0x00,0xc1,0x00,0xf5,
	0x00,0x09,0x00,0x09,0x0d,0x00,0x6f,0x00,0x4f,0x00,
	0x0f,0x00,0x0f,0x00,0xf9,0x00,0xe9,0x00,0x0f,0x00,
	0x0f,0x0d,0x00,0x70,0x00,0x50,0x00,0x10,0x00,0x10,
	0x01,0x70,0x01,0x50,0x00,0x10,0x00,0x10,0x00,0x01,
	0xac,0xff,0x00,0x00,0x30,0x00,0x00,0x2e,0x00,0x00,
	0x03,0xff,0x00,0x01,0xaf,0x00,0x01,0xae,0x00,0x00,
	0x31,0x00,0x00,0x34,0x00,0x00,0x36,0x00,0x00,0x33,
	0x00,0x00,0x2b,0x00,0x01,0xad,0x00,0x00,0x32,0x00,
	0x00,0x35,0xff,0xff,0x02,0x00,0x7f,0x00,0x08,0x0a,
	0x00,0x3d,0x00,0x2b,0x01,0xb9,0x01,0xb1,0x0e,0x00,
	0x2d,0x00,0x5f,0x00,0x1f,0x00,0x1f,0x00,0xb1,0x00,
	0xd0,0x00,0x1f,0x00,0x1f,0x0a,0x00,0x38,0x00,0x2a,
	0x01,0xb0,0x00,0xb4,0x0a,0x00,0x39,0x00,0x28,0x00,
	0xac,0x00,0xab,0x0a,0x00,0x30,0x00,0x29,0x00,0xad,
	0x00,0xbb,0x00,0x00,0x37,0x00,0x00,0x38,0x00,0x00,
	0x39,0x00,0x01,0x2d,0x00,0x00,0x2a,0x0a,0x00,0x60,
	0x00,0x7e,0x00,0x60,0x01,0xbb,0x02,0x00,0x3d,0x00,
	0x7c,0x02,0x00,0x2f,0x00,0x5c,0xff,
	/* This next byte was edited by hand.  KeyMap generated
	   0x04, indicating that the return key (the key
	   we're defining) should generate stuff depending
	   on the setting of the control key.  Since what
	   we want is to correspond to the physical
	   keyboard, which is labelled to indicate that
	   command-return will generate enter, we changed
	   the 0x04 to 0x10.  */
	0x10,0x00,0x0d,
	0x00,0x03,0x0a,0x00,0x27,0x00,0x22,0x00,0xa9,0x01,
	0xae,0x0a,0x00,0x3b,0x00,0x3a,0x01,0xb2,0x01,0xa2,
	0x0d,0x00,0x6c,0x00,0x4c,0x00,0x0c,0x00,0x0c,0x00,
	0xf8,0x00,0xe8,0x00,0x0c,0x00,0x0c,0x0a,0x00,0x2c,
	0x00,0x3c,0x00,0xcb,0x01,0xa3,0x0a,0x00,0x2e,0x00,
	0x3e,0x00,0xbc,0x01,0xb3,0x0a,0x00,0x2f,0x00,0x3f,
	0x01,0xb8,0x00,0xbf,0x0d,0x00,0x7a,0x00,0x5a,0x00,
	0x1a,0x00,0x1a,0x00,0xcf,0x01,0x57,0x00,0x1a,0x00,
	0x1a,0x0d,0x00,0x78,0x00,0x58,0x00,0x18,0x00,0x18,
	0x01,0xb4,0x01,0xce,0x00,0x18,0x00,0x18,0x0d,0x00,
	0x63,0x00,0x43,0x00,0x03,0x00,0x03,0x01,0xe3,0x01,
	0xd3,0x00,0x03,0x00,0x03,0x0d,0x00,0x76,0x00,0x56,
	0x00,0x16,0x00,0x16,0x01,0xd6,0x01,0xe0,0x00,0x16,
	0x00,0x16,0x0d,0x00,0x62,0x00,0x42,0x00,0x02,0x00,
	0x02,0x01,0xe5,0x01,0xf2,0x00,0x02,0x00,0x02,0x0d,
	0x00,0x6d,0x00,0x4d,0x00,0x0d,0x00,0x0d,0x01,0x6d,
	0x01,0xd8,0x00,0x0d,0x00,0x0d,0x0d,0x00,0x6e,0x00,
	0x4e,0x00,0x0e,0x00,0x0e,0x00,0xc4,0x01,0xaf,0x00,
	0x0e,0x00,0x0e,0x0c,0x00,0x20,0x00,0x00,0x00,0x80,
	0x00,0x00,0x0d,0x00,0x61,0x00,0x41,0x00,0x01,0x00,
	0x01,0x00,0xca,0x00,0xc7,0x00,0x01,0x00,0x01,0x0d,
	0x00,0x73,0x00,0x53,0x00,0x13,0x00,0x13,0x00,0xfb,
	0x00,0xa7,0x00,0x13,0x00,0x13,0x0d,0x00,0x64,0x00,
	0x44,0x00,0x04,0x00,0x04,0x01,0x44,0x01,0xb6,0x00,
	0x04,0x00,0x04,0x0d,0x00,0x66,0x00,0x46,0x00,0x06,
	0x00,0x06,0x00,0xa6,0x01,0xac,0x00,0x06,0x00,0x06,
	0x0d,0x00,0x67,0x00,0x47,0x00,0x07,0x00,0x07,0x00,
	0xf1,0x00,0xe1,0x00,0x07,0x00,0x07,0x0d,0x00,0x6b,
	0x00,0x4b,0x00,0x0b,0x00,0x0b,0x00,0xce,0x00,0xaf,
	0x00,0x0b,0x00,0x0b,0x0d,0x00,0x6a,0x00,0x4a,0x00,
	0x0a,0x00,0x0a,0x00,0xc6,0x00,0xae,0x00,0x0a,0x00,
	0x0a,0x0d,0x00,0x68,0x00,0x48,0x00,0x08,0x00,0x08,
	0x00,0xe3,0x00,0xeb,0x00,0x00,0x18,0x00,0x02,0x00,
	0x09,0x00,0x19,0x0d,0x00,0x71,0x00,0x51,0x00,0x11,
	0x00,0x11,0x00,0xfa,0x00,0xea,0x00,0x11,0x00,0x11,
	0x0d,0x00,0x77,0x00,0x57,0x00,0x17,0x00,0x17,0x01,
	0xc8,0x01,0xc7,0x00,0x17,0x00,0x17,0x0d,0x00,0x65,
	0x00,0x45,0x00,0x05,0x00,0x05,0x00,0xc2,0x00,0xc5,
	0x00,0x05,0x00,0x05,0x0d,0x00,0x72,0x00,0x52,0x00,
	0x12,0x00,0x12,0x01,0xe2,0x01,0xd2,0x00,0x12,0x00,
	0x12,0x0d,0x00,0x75,0x00,0x55,0x00,0x15,0x00,0x15,
	0x00,0xc8,0x00,0xcd,0x00,0x15,0x00,0x15,0x0d,0x00,
	0x79,0x00,0x59,0x00,0x19,0x00,0x19,0x00,0xa5,0x01,
	0xdb,0x00,0x19,0x00,0x19,0x0d,0x00,0x74,0x00,0x54,
	0x00,0x14,0x00,0x14,0x01,0xe4,0x01,0xd4,0x00,0x14,
	0x00,0x14,0x02,0x00,0x1b,0x00,0x7e,0x0a,0x00,0x31,
	0x00,0x21,0x01,0xad,0x00,0xa1,0x0e,0x00,0x32,0x00,
	0x40,0x00,0x00,0x00,0x00,0x00,0xb2,0x00,0xb3,0x00,
	0x00,0x00,0x00,0x0a,0x00,0x33,0x00,0x23,0x00,0xa3,
	0x01,0xba,0x0a,0x00,0x34,0x00,0x24,0x00,0xa2,0x00,
	0xa8,0x0a,0x00,0x37,0x00,0x26,0x00,0xb7,0x01,0xab,
	0x0e,0x00,0x36,0x00,0x5e,0x00,0x1e,0x00,0x1e,0x00,
	0xb6,0x00,0xc3,0x00,0x1e,0x00,0x1e,0x0a,0x00,0x35,
	0x00,0x25,0x01,0xa5,0x00,0xbd,0x03,0x01,0x00,0x61,
	0x01,0x00,0x61,0x01,0x00,0x61
};

typedef struct _NewMappingData {
    unsigned char *bp;
    unsigned char *endPtr;
    int shorts;
} NewMappingData;

static unsigned int NextNum(NewMappingData *nmd)
{
    if (nmd->bp > nmd->endPtr)
	return(0);
    if (nmd->shorts)
	return(*((unsigned short *)nmd->bp)++);
    else
	return(*((unsigned char *)nmd->bp)++);
}

/******************************************************************************
    SetKeyMapping parses through the given keyMapping and installs the
    description of it into the variable curMappingStorage and curMapLen. It
    does not affect mapNotDefault.  It returns the old value of
    curMappingStorage.mapping, or NULL if it failed.
******************************************************************************/

unsigned char *SetKeyMapping(unsigned char *mapping, int mappingLen)
{
    NewMappingData nmd;
    KeyMapping newMapping;	/* Where we build the new one */
    int i, j, k, l, m, n;
    int keyMask, mask, numMods;
    int maxSeqNum = -1;
    unsigned char *oldMapping;

    nmd.endPtr = mapping + mappingLen;
    nmd.bp = mapping;
    nmd.shorts = 1;
    /* Clear out the variable newMapping */
    bzero(&newMapping, sizeof(newMapping));
    newMapping.maxMod = -1;
    /* Start filling it in with the new data */
    newMapping.mapping = mapping;
    newMapping.shorts = nmd.shorts = NextNum(&nmd);
    /* Walk through the modifier definitions */
    numMods = NextNum(&nmd);
    for(i=0; i<numMods; i++)
    {
	/* Get bit number */
	if ((j = NextNum(&nmd)) >= NUMMODIFIERS) return(NULL);
	/* Check maxMod */
	if (j > newMapping.maxMod) newMapping.maxMod = j;
	/* record position of this def */
	newMapping.modDefs[j] = nmd.bp;
	/* Loop through each key assigned to this bit */
	for(k=0,n = NextNum(&nmd);k<n;k++)
	{
	    /* Check that key code is valid */
	    if ((l = NextNum(&nmd)) >= NUMKEYCODES)
		return(NULL);
	    /* Make sure the key's not already assigned */
	    if (newMapping.keyBits[l] & MODMASK) return(NULL);
	    /* Set bit for modifier and which one */
	    newMapping.keyBits[l] |= MODMASK | (j & WHICHMODMASK);
	}
    }
    /* Walk through each key definition */
    for(i=0, n=NextNum(&nmd); i<NUMKEYCODES; i++)
	if (i < n)
	{
	    newMapping.keyDefs[i] = nmd.bp;
	    if ((keyMask = NextNum(&nmd)) != 0x00ff)
	    {
		/* Set char gen bit for this guy */
		newMapping.keyBits[i] |= CHARGENMASK;
		/* Check key defs to find max sequence number */
		for(j=0, k=1; j<=newMapping.maxMod; j++, keyMask>>=1)
			if (keyMask & 0x01) k*= 2;
		for(j=0; j<k; j++)
		{
		    m = NextNum(&nmd);
		    l = NextNum(&nmd);
		    if (m == -1) if (((int)l) > maxSeqNum) maxSeqNum = l;
		}
	    }
	    else /* we don't set the CHARGEN bit, and also */
	    	newMapping.keyDefs[i] = NULL;
	}
	else
	{
	    newMapping.keyDefs[i] = NULL;
	    /* Leave CHARGEN bit of keyBits[i] as 0 */
	}
    /* Walk through sequence defs, making sure not recursive */
    newMapping.numSeqs = NextNum(&nmd);
    newMapping.seqDefs = nmd.bp;
    if (newMapping.numSeqs <= maxSeqNum)
	return(NULL);
    for(i=0; i<newMapping.numSeqs; i++)
	for(j=0, l=NextNum(&nmd); j<l; j++)
	{
	    if (NextNum(&nmd) == 0x00ff)
		return(NULL);
	    NextNum(&nmd);
	}
    /* Install bits for Special device keys */
    for(i=0; i<sizeof(specialKeyCodes); i++)
    	newMapping.keyBits[specialKeyCodes[i]] |= (CHARGENMASK|SPECIALKEYMASK);
    /* Great!  We are there, install new mapping */
    oldMapping = curMapping->mapping;
    /* BEGIN CRITICAL SECTION */ keySema = 1;
    /* Copy current key states */
    for(i=0; i<NUMKEYCODES; i++)
	newMapping.keyBits[i] |= (curMappingStorage.keyBits[i] & KEYSTATEMASK);
    /* Recalculate modifiers */
    eventGlobals->eventFlags = eventGlobals->eventFlags & ~MAPPINGMODMASK;
    for(i=0; i<=newMapping.maxMod; i++)
	CalcModBit(&newMapping, i);
    curMappingStorage = newMapping;
    curMapLen = mappingLen;
    /* END CRITICAL SECTION */ keySema = 0;
    return(oldMapping);
} /* SetKeyMapping */

#undef NEXTNUM
#define NEXTNUM() (shorts ? *((short *)mapping)++ : \
*((unsigned char *)mapping)++ )

/******************************************************************************
    CalcModBit recalculates the state of the given modifier bit from the
    states of all the keys assigned to it in the given mapping. Notice that
    its parameter, bit, is in the mapping's space of 0-14 but the bit of
    eventFlags that is changed is 16 higher than that (i.e., the
    mapping-dependent bits).
******************************************************************************/

void CalcModBit(KeyMapping *map, int bit)
{
	unsigned char	*mapping;
	short		shorts;
	int		bitMask,i,n;
	int		myFlags;

	shorts = map->shorts;
	mapping = map->modDefs[bit];
	bitMask = 1<<(bit+16);
	/* Initially clear bit */
	myFlags = eventGlobals->eventFlags & (~bitMask);
	if (mapping)
	    for(i=0, n=NEXTNUM(); i<n; i++)
		if (map->keyBits[NEXTNUM()] & KEYSTATEMASK)
		    myFlags |= bitMask;
	/* If this was the shift bit... */
	if (bit == 1)
	{
		if (myFlags & NX_COMMANDMASK)
		    if (myFlags&bitMask)
		    {
		    	keyPressed = 0;
		    }
		    else
		    	if (!keyPressed)
			{
			    alphaLock = !alphaLock;
			    AlphaLockFeedback(alphaLock);
			}
		/* And in any case, reset the alphaLock/shift bit */
		myFlags = (myFlags & ~NX_ALPHASHIFTMASK) |
			(alphaLock<<16) | ((myFlags & NX_SHIFTMASK)>>1);
	}
	eventGlobals->eventFlags = myFlags;
}

/******************************************************************************
    DoModCalc recalculates all modifier bits that depend on the given key, if
    any, and posts a flags-changed event.
******************************************************************************/

void DoModCalc(KeyMapping *map, int key)
{
    int thisBits;
    NXEventData outData;

    thisBits = map->keyBits[key];
    if (thisBits & MODMASK)
    {
	CalcModBit(map,thisBits & WHICHMODMASK);
	/* The driver generates flags-changed events only when there is
	   no key-down or key-up event generated */
	if (!(thisBits & CHARGENMASK))
	{
	    outData.key.keyCode = key;
	    outData.key.keyData = outData.key.charSet =
	    outData.key.charCode = outData.key.repeat =
	    outData.key.reserved = 0;
	    LLEventPost(NX_FLAGSCHANGED,eventGlobals->cursorLoc,&outData);
	}
    }
}

/******************************************************************************
    DoCharGen does any required character generation for the given key.
    Accepts 3 values for direction: 0 (up), 1 (down), 2 (repeat)
******************************************************************************/

void DoCharGen(KeyMapping *map, int key, int direction)
{
    int	i, j, n, eventType, adjust, thisMask, shorts, modifiers;
    NXEventData	outData;
    unsigned char *mapping;

    keyPressed = 1;
    shorts = map->shorts;
    outData.key.repeat = (direction == 2);
    outData.key.keyCode = key;
    outData.key.keyData = outData.key.reserved = 0;
    eventType = direction ? NX_KEYDOWN : NX_KEYUP;
    modifiers = eventGlobals->eventFlags>>16;
    /* Get this key's key mapping */
    mapping = curMapping->keyDefs[key];
    if (mapping)
    {
	/* Build offset for this key */
	thisMask = NEXTNUM();
	if (thisMask)
	{
	    adjust = (shorts ? sizeof(short) : sizeof(char))*2;
	    for(i=0; i<=curMapping->maxMod; i++)
	    {
		if (thisMask & 0x01)
		{
		    if (modifiers & 0x01)
			mapping += adjust;
		    adjust *= 2;
		}
		thisMask >>= 1;
		modifiers >>= 1;
	    }
	}
	if (shorts)
	    outData.key.charSet = *((short *)mapping)++;
	else
	    outData.key.charSet = *((char *)mapping)++;
	outData.key.charCode = NEXTNUM();
	if (((short)outData.key.charSet) == -1)
	{ /* She's a sequence, captain */
	    mapping = curMapping->seqDefs;
	    for(i=0;i<outData.key.charCode;i++)
	    {
		j = NEXTNUM();
		mapping += j * (shorts ? 4 : 2);
	    }
	    for(i=0,n=NEXTNUM();i<n;i++)
	    {
		outData.key.charSet = NEXTNUM();
		outData.key.charCode = NEXTNUM();
		LLEventPost(eventType,eventGlobals->cursorLoc,&outData);
	    }
	}
	else
	    LLEventPost(eventType,eventGlobals->cursorLoc,&outData);
    } /* if (mapping) */
    
    /* Check for a device control keys: note that they always have CHARGEN
       bit set */
    if (curMapping->keyBits[key] & SPECIALKEYMASK)
    	DoSpecialKey(key,direction,eventGlobals->eventFlags);

    /* Deal with downKeys */
    if (direction == 1)
    {
	/* Set this key to repeat (push out last key if present) */
	downCount = initialKeyRepeat; 
	downKey = key;
#if NO_VERTICAL_RETRACE
	timeout(DoKbdRepeat, 0, 1);
#endif NO_VERTICAL_RETRACE
    } else if (direction == 0)
    {
	/* Remove from downKey */
        if (downKey == key)
	{
	    downCount = -1;
	    downKey = -1;
	}
    }
}

/******************************************************************************
    DoKbdEvent performs the key mapping for the key with keyCode key moving in
    the direction given (0 == up, 1 == down).
******************************************************************************/

void DoKbdEvent(int key, int direction, int deviceMods)
{
    char thisBits;

    /* Update device-dependent modifier bits */
    eventGlobals->eventFlags = (eventGlobals->eventFlags & MAPPINGMODMASK)
				    | (deviceMods & DEVICEMODMASK);
    /* Update bit in keyBits */
    thisBits = curMapping->keyBits[key];
    if (direction)
	thisBits |= KEYSTATEMASK;
    else
	thisBits &= ~KEYSTATEMASK;
    curMapping->keyBits[key] = thisBits;
    /* Do mod bit update and char generation in useful order */
    if (direction)
    {
	if (thisBits & MODMASK) DoModCalc(curMapping,key);
	if (thisBits & CHARGENMASK) DoCharGen(curMapping,key,direction);
    }
    else
    {
	if (thisBits & CHARGENMASK) DoCharGen(curMapping,key,direction);
	if (thisBits & MODMASK) DoModCalc(curMapping,key);
    }
}

/******************************************************************************
    DoKbdRepeat should be called once every retrace interval (or whatever
    the times recorded in downKeys are).  It will decrement all the valid down
    keys' counts, and generate KEYREPEATED events as appropriate.
******************************************************************************/

void DoKbdRepeat()
{
    if ((downCount != -1) && eventsOpen)
    {
	downCount--;
	if (downCount == 0)
	{
	    if (curMapping->keyBits[downKey] & CHARGENMASK)
		DoCharGen(curMapping, downKey, 2);
	    downCount = keyRepeat;
	}
    }
}

/******************************************************************************
    ResetKbd resets all keyboard status variables to their initial state.
******************************************************************************/

void ResetKbd()
{
    if (mapNotDefault)
	kmem_free(kernel_map, curMapping->mapping, curMapLen);
    InitKbd(1);
}

int InitKbd(int reset)
{
    char *mapping;
    int mappingLen, i;

    alphaLock = keyPressed = mapNotDefault = 0;
    curMapping = &curMappingStorage;
    AlphaLockFeedback(0);
    AllKeysUp();
    initialKeyRepeat = DEFAULTINITIALREPEAT;
    keyRepeat = DEFAULTKEYREPEAT;
    
    /* We don't check the error return from this because it actually
       will return an error, since the old mapping was NULL */
    SetKeyMapping((unsigned char *)defaultMapping, sizeof(defaultMapping));
    
    /* Get the current brightness level and cache it in curBright
       FIX!!! Do we want to do this for volume, too? */
    if (!reset) /* only do this on the real open */
    {
	struct nvram_info ni;
    
	nvram_check(&ni);
	curBright = ni.ni_brightness;
    }

    return(0);
}

void AllKeysUp()
{
    /* this is used in 2 situations: at boot time, and when we detect a
     * keyboard overrun. In the latter case, we want to ensure that any 
     * keys which may be autorepeating are forced into a "key up"
     * condition (the overrun might cause us to miss a key up event).
     */
     downKey = -1;
     downCount = -1;
}







