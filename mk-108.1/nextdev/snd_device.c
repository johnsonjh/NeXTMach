/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 *
 *  20-Jul-90 Matt Christiano (mchristo) at NeXT
 *	Fixed ramping so that it would happen.
 *	Fixed snd_ramp() to ramp in the proper direction.
 *	Added snd_device_set_ramp function to selectively disable up/down
 *	ramps.
 *
 *  3-Oct-89  Gregg Kellogg (gk) at NeXT
 *	If the ramp buffer wasn't allocated a ramp can't be computed.  Changes
 *	were made to ensure that a ramp is calculated only if a buffer was
 *	allocated.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <sound.h>
#if	NSOUND > 0

/*
 * Sound Out/In device driver
 */

#import <kern/xpr.h>
#import <sys/buf.h>
#import <sys/param.h>
#import <kern/thread.h>
#import <sys/callout.h>
#import <vm/vm_param.h>
#import <next/cpu.h>
#import <next/pmap.h>
#import <nextdev/busvar.h>
#import <nextdev/snd_snd.h>
#import <nextdev/monreg.h>
#import <mon/nvram.h>

#import <machine/spl.h>

/* Let everyone else know our status */
extern int sound_active;

extern snd_var_t snd_var;
ll_snd_var_t ll_snd_var;

/* interrupt vectors */
static void snd_dev_intr(void);

static void snd_ovr(int dir, int ovr, int dmaen, int cmd);

/* misclanioius routines */
static int snd_start(int dir, snd_dma_desc_t *ddp, int startup);
static void snd_shutdown(int dir);
static void mon_send_nodma(int mon_cmd, int data);
static void snd_vol_set(void);
static void snd_vol_save(void);
static void snd_ramp(int left, int right, short *buf, boolean_t from);

/* Local Storage */
static volatile struct monitor *mon;
volatile int vol_r = -1, vol_l = -1, gpflags;
static int ni_spkren, ni_lowpass, ni_vol_r, ni_vol_l;

/* autoconfiguration */
int snd_device_probe(caddr_t addr, register int unit);
int snd_device_attach(register struct bus_device *bd);

struct bus_device	*sound_dinfo[NSOUND];
struct bus_driver	 sounddriver = {
	snd_device_probe, 0, snd_device_attach, 0, 0,
	(int (*)())snd_dev_intr, 0, 0, "sound", sound_dinfo
};

#if	XPR_DEBUG
int sdbgflag;
#define snd_dbug(f) {if (sdbgflag&1)printf f; else XPR(XPR_SOUND, f);}
#define snd_dbug2(f) if (sdbgflag&2){if (sdbgflag&1)printf f; else XPR(XPR_SOUND, f);}
#else	XPR_DEBUG
#define snd_dbug(f)
#define snd_dbug2(f)
#endif	XPR_DEBUG

#define SO_JUMP_WAIT	1000		/* Number of cycles to wait for dma */

/*
 * Open the device.
 * Exclusive access to the SOUND.
 * Reset the chip.
 */
void snd_device_init(int direction)
{
	register int s, i;
	register snd_queue_t *q;
	struct nvram_info ni;

#if	XPR_DEBUG
	if (xprflags&XPR_SOUND)
		ll_snd_var.dma[direction].chan.dc_flags |= DMACHAN_DBG;
	else
		ll_snd_var.dma[direction].chan.dc_flags &= ~DMACHAN_DBG;
#endif	XPR_DEBUG
	/*
	 * Initialize volume/speaker from nvram (first time)
	 */
	if (direction == SND_DIR_PLAY && vol_r == -1) {
		/*
		 * Make sure volume is properly initialized.
		 */
		nvram_check(&ni);
		vol_r = ni.ni_vol_r;
		vol_l = ni.ni_vol_l;
		ni_vol_r = -1;
		ni_vol_l = -1;

		gpflags = 0;
		if (ni.ni_spkren) {
			gpflags |= SGP_SPKREN;
			ni_spkren = 0;
		} else
			ni_spkren = 1;
		if (ni.ni_lowpass) {
			gpflags |= SGP_LOWPASS;
			ni_lowpass = 0;
		} else
			ni_lowpass = 1;

		snd_vol_set();
	}

	snd_stream_queue_init(&ll_snd_var.dma[direction].sndq,
		(caddr_t)&ll_snd_var, snd_device_start);
	ll_snd_var.dma[direction].sndq.dmasize =
		snd_device_def_dmasize(direction);
	ll_snd_var.dma[direction].sndq.def_high_water =
		snd_device_def_high_water(direction);
	ll_snd_var.dma[direction].sndq.def_low_water =
		snd_device_def_low_water(direction);
	ll_snd_var.dma[direction].shutdown = 0;

	if (direction == SND_DIR_PLAY) {
		/*
		 * Allocate buffer for ramping starts to a non-zero
		 * value or underflow ends from a non-zero value.
		 */
		ll_snd_var.ramp_buf = (short *)kalloc(PAGE_SIZE);
	}
}

/*
 * Sound device level reset.  Release all resources.
 */
void snd_device_reset(int direction)
{
	register int s;

	s = splsound();
	if (direction == SND_DIR_PLAY) {
		ll_snd_var.dmaflags = 0;
		kfree(ll_snd_var.ramp_buf, PAGE_SIZE);
		ll_snd_var.ramp_buf = 0;
		ll_snd_var.ramp = SND_PARM_RAMPUP | SND_PARM_RAMPDOWN;
	}
	ll_snd_var.dma[direction].sndq.paused = 1;
	snd_shutdown(direction);
	splx(s);
	snd_stream_queue_reset(&ll_snd_var.dma[direction].sndq);
}

/*
 * Set sound device parameters, such as zero-fill, low-pass filter, and
 * speaker enable.
 */
void snd_device_set_parms(u_int parms)
{
	/* zero fill double samples */
	if (parms&SND_PARM_ZEROFILL)
		ll_snd_var.dmaflags |= SOUT_ZERO;
	else
		ll_snd_var.dmaflags &= ~SOUT_ZERO;

	mon_send(MON_SNDOUT_CTRL(ll_snd_var.dmaflags), 0);

	/* enable the speaker (bit-sense backwards) */
	if (parms&SND_PARM_SPEAKER)
		gpflags &= ~SGP_SPKREN;
	else
		gpflags |= SGP_SPKREN;

	/* Enable Low Pass Filter */
	if (parms&SND_PARM_LOWPASS)
		gpflags |= SGP_LOWPASS;
	else
		gpflags &= ~SGP_LOWPASS;

	/*
	 * If the parameters have actually change, reflect the change.
	 */
	if (ll_snd_var.gpflags != gpflags) {
		snd_vol_set();
		snd_vol_save();
	}
}

/*
 * Set sound out ramping on/off
 */
void snd_device_set_ramp(u_int parms)
{
	ll_snd_var.ramp = parms;
}

/*
 * Return sound device parameters.
 */
u_int snd_device_get_parms(void)
{
	register u_int parms;

	parms = (gpflags&SGP_LOWPASS) ? SND_PARM_LOWPASS : 0;
	parms |= (gpflags&SGP_SPKREN) ? 0 : SND_PARM_SPEAKER;
	parms |= (ll_snd_var.dmaflags&SOUT_ZERO) ? SND_PARM_ZEROFILL : 0;

	return parms;
}

/*
 * Set the volume of both right and left channels (simplification if they're
 * the same).  The value(s) passed is inverted from the sense the hardware
 * understands (attinuation, rather than volume).
 */
void snd_device_set_volume(u_int volume)
{
	vol_l=  VOLUME_MAX
	      - ((volume&SND_VOLUME_LCHAN_MASK)>>SND_VOLUME_LCHAN_SHIFT);
	vol_r=  VOLUME_MAX
	      - ((volume&SND_VOLUME_RCHAN_MASK)>>SND_VOLUME_RCHAN_SHIFT);
	snd_vol_set();
	snd_vol_save();
}


/*
 * Return volume of both right and left channels.
 */
u_int snd_device_get_volume(void)
{
	
	return    ((VOLUME_MAX-vol_l)<<SND_VOLUME_LCHAN_SHIFT)
		| ((VOLUME_MAX-vol_r)<<SND_VOLUME_RCHAN_SHIFT);
}

/*
 * Software dma completion, hand off the completed buffer to higher-level
 * software.
 */
static void snd_dma_intr(int direction)
{
	register struct ll_snd_dma *sd = &ll_snd_var.dma[direction];
	register struct dma_hdr *dhp;
	register snd_dma_desc_t *ddp;
	register int s;
	int dmaen;

	dmaen = (direction == D_WRITE ? DMAOUT_DMAEN : DMAIN_DMAEN);

	/*
	 * If the dma completion preceeds the underrun interrupt
	 * (as it likely does with sound in) disable dma in the monitor
	 * registers so that dma will be re-started in snd_start.
	 */
	s = splsound();
	if ((sd->chan.dc_flags&DMACHAN_BUSY) == 0 && (*(int *)mon&dmaen))
	    	mon_csr_and(~(dmaen)>>24);
	splx(s);

	/*
	 * Check for errors.
	 */
	if (sd->chan.dc_flags&DMACHAN_ERROR) {
		snd_dbug(("sound_dma_intr dir %s shutdown\n",
			direction == D_WRITE ? "write" : "read"));
		snd_shutdown(direction);
	}

	snd_dbug2(("sound_dma_intr ndma %d\n", sd->ndma));

	/*
	 * Release the buffer.
	 */
	s = spldma();
	while (dhp = dma_dequeue(&sd->chan, DMADQ_DONE)) {
		splx(s);
		ddp = (snd_dma_desc_t *)dhp->dh_drvarg;
		snd_dbug2(("sound_dma_intr dequeue dhp %x ddp %x\n",
			dhp, ddp));

		/*
		 * Might be a split buffer or a ramp.
		 */
		if (!ddp)
			continue;

		/*
		 * Signal an underrun condition when freeing.
		 */
		if (--sd->ndma == 0) {
			ddp->flag |= SDD_OVERFLOW;
			sd->shutdown = 0;
		}
		
		/*
		 * Keep track of number of samples transfered.
		 */
		sd->sndq.nxfer += ddp->size;

		/*
		 * Reset dh_drvarg as we may have modified it to do
		 * ramp downs.
		 */
		ddp->hdr.dh_drvarg = (int)ddp;

		if ((*ddp->free)(ddp) == 0
		    && sd->shutdown == 0
		    && sd->ndma)
		{
			register struct dma_hdr *dhp;
			sd->shutdown = 1;

			/*
			 * Calculate ramp down from last sample
			 * enqueued for output so that we won't
			 * get a click.
			 */
			s = spldma();
			dhp = sd->chan.dc_queue.dq_tail;
			splx(s);
			ASSERT(dhp);
			if (   direction == D_WRITE
			    && ((int *)dhp->dh_stop)[-1]
			    && (ll_snd_var.ramp & SND_PARM_RAMPDOWN))
			{
				snd_dbug2(("snd_dma_intr: ramp from "
					    "0x%x to 0\n",
					   ((int *)dhp->dh_stop)[-1]));
				snd_ramp(((short *)dhp->dh_stop)[-2],
					    ((short *)dhp->dh_stop)[-1],
					    ll_snd_var.ramp_buf, TRUE);

				/*
				 * Fake out the higher level software into
				 * thinking that we're not freeing dhp
				 * until the ramp buffer's finished.
				 */
				sd->spare.dh_drvarg = dhp->dh_drvarg;
				dhp->dh_drvarg = 0;

				sd->spare.dh_start =
					(char *)pmap_resident_extract(
						kernel_pmap,
						ll_snd_var.ramp_buf);
				sd->spare.dh_stop = sd->spare.dh_start
							+ PAGE_SIZE;
				sd->spare.dh_flags = DMADH_INITBUF;
				dma_enqueue(&sd->chan, &sd->spare);
			}
		}
		s = spldma();
	}

	/*
	 * It's possible that we missed a chain and that sound was
	 * turned off in the device interrupt.  If this is the case
	 * then there will be enqueued buffers, but dma won't be
	 * enabled.  If so, start up dma manually.
	 */
	if (   direction == D_WRITE
	    && !(*(int *)mon&dmaen)
	    && sd->ndma)
	{
		snd_dbug(("snd_dma_intr: kickstart stoped dma\n"));
		(void) snd_start(D_WRITE, NULL, TRUE);
	}
	splx(s);
}

static void snd_dev_intr(void)
{
	register int s;
	register struct dma_hdr *dhp, *dhp2;

	/*
	 * This should only be called when we have an overrun or
	 * an underrun.
	 */
	snd_dbug(("snd_dev_intr mon_csr == %x\n",mon->mon_csr));
	snd_dbug2(("     read_dma: state %x, flags %x, csr %x\n",
		   ll_snd_var.dma[D_READ].chan.dc_state,
		   ll_snd_var.dma[D_READ].chan.dc_flags,
		   ll_snd_var.dma[D_READ].chan.dc_ddp->dd_csr));
	snd_dbug2(("     write_dma: state %x, flags %x, csr %x\n",
		   ll_snd_var.dma[D_WRITE].chan.dc_state,
		   ll_snd_var.dma[D_WRITE].chan.dc_flags,
		   ll_snd_var.dma[D_WRITE].chan.dc_ddp->dd_csr));

	/*
	 * Disable dma, we'll pick up any completions in the
	 * dma interrupt routine.
	 */
	if (mon->mon_csr.dmaout_ovr) {
		sound_active = 0;
		ll_snd_var.dmaflags &= ~SOUT_ENAB;
		snd_ovr(D_WRITE, DMAOUT_OVR>>24, DMAOUT_DMAEN>>24,
			  MON_SNDOUT_CTRL(ll_snd_var.dmaflags));
	}

	if (mon->mon_csr.dmain_ovr)
		snd_ovr(D_READ, DMAIN_OVR>>24, DMAIN_DMAEN>>24,
			  MON_SNDIN_CTRL(0));

	snd_dbug2(("sound_dev_intr: exit mon_csr %x, in_csr %x,"
		   " out_csr %x\n", mon->mon_csr,
		   ll_snd_var.dma[D_READ].chan.dc_ddp->dd_csr,
		   ll_snd_var.dma[D_WRITE].chan.dc_ddp->dd_csr));
}

static void snd_ovr(int dir, int ovr, int dmaen, int cmd)
{
	register int s;

	snd_dbug2(("snd_ovr: %s\n",
		   dir == D_WRITE ? "output underrun" : "input overrun"));
	s = spldma();
	mon_csr_and(~dmaen);
	mon_send(cmd, 0);
	mon_csr_or(ovr);
	if (ll_snd_var.dma[dir].chan.dc_flags & DMACHAN_BUSY) {
		snd_dbug2(("sound_ovr: restart\n"));
		dma_abort(&ll_snd_var.dma[dir].chan);
		ll_snd_var.dma[dir].chan.dc_flags &= ~DMACHAN_ERROR;
		snd_start(dir, (snd_dma_desc_t *)NULL, TRUE);
	}
	splx(s);
}

boolean_t snd_device_start(snd_dma_desc_t *ddp, int direction, int rate)
{
	if (ll_snd_var.dma[direction].shutdown)
		return 0;

	ASSERT(rate == SND_RATE_22 || rate == SND_RATE_44);

	switch (rate|(direction<<8)) {
	case SND_RATE_22:
		if ((ll_snd_var.dmaflags&SOUT_DOUB))
			break;
		ll_snd_var.dmaflags |= SOUT_DOUB;
		mon_send(MON_SNDOUT_CTRL(ll_snd_var.dmaflags), 0);
		break;
	case SND_RATE_44:
		if (!(ll_snd_var.dmaflags&SOUT_DOUB))
			break;
		ll_snd_var.dmaflags &= ~SOUT_DOUB;
		mon_send(MON_SNDOUT_CTRL(ll_snd_var.dmaflags), 0);
		break;
	}

	return snd_start(direction, ddp, TRUE);
}

static int snd_start(int dir, snd_dma_desc_t *ddp, int startup)
{
	register int s, i;
	register struct ll_snd_dma *sd = &ll_snd_var.dma[dir];
	register struct dma_hdr *dhp = &ddp->hdr;
	int dmaen = (dir == D_WRITE ? DMAOUT_DMAEN : DMAIN_DMAEN);

	snd_dbug2(("snd_start dir %s: ndma %d\n",
		   dir == D_WRITE ? "write" : "read", sd->ndma));

	/*
	 * Enqueue a buffer for dma if given.
	 */
	if (ddp) {
		*(int *)&(dhp->dh_start) &= ~0x3;

		if (sd->ndma == 0 && startup) {
		    if (*(int *)dhp->dh_start
			&& dir == D_WRITE
			&& (ll_snd_var.ramp & SND_PARM_RAMPUP))
		    {
			/*
			 * Starting off at a non-zero offset, calculate
			 * a ramp and play that first.
			 */
			snd_dbug2(("ramp from 0 to 0x%x\n",
				   *(int *)dhp->dh_start));
			snd_ramp(((short *)dhp->dh_start)[0],
				    ((short *)dhp->dh_start)[1],
				    ll_snd_var.ramp_buf, FALSE);
			snd_dbug2(("snd_start spare %x"
				   "  start %x stop %x\n",
				   &sd->spare, sd->spare.dh_start,
				   sd->spare.dh_stop));
			sd->spare.dh_start = (char *)pmap_resident_extract(
				kernel_pmap, ll_snd_var.ramp_buf);
			sd->spare.dh_stop = sd->spare.dh_start + PAGE_SIZE;
			sd->spare.dh_drvarg = 0;
			sd->spare.dh_flags = DMADH_INITBUF;
			dma_enqueue(&sd->chan, &sd->spare);
		     } else {
			/*
			 * Split the buffer into
			 * two pieces so that we can chain.
			 * Make sure that both parts are aligned
			 * to chunk boundaries.
			 * We shouldn't need to spl here.
			 */
			sd->spare.dh_start = dhp->dh_start;
			sd->spare.dh_drvarg = 0;
			sd->spare.dh_stop = sd->spare.dh_start
				+ (((dhp->dh_stop - dhp->dh_start)/2)&~0x3);
			sd->spare.dh_flags = DMADH_INITBUF;
			dhp->dh_start = sd->spare.dh_stop;
			dma_enqueue(&sd->chan, &sd->spare);
			snd_dbug2(("snd_start spare %x"
				    "  start %x stop %x\n",
				    &sd->spare, sd->spare.dh_start,
				    sd->spare.dh_stop));
		    }
	
		}
	
		snd_dbug2(("snd_start dhp %x start %x stop %x\n",
			   dhp, dhp->dh_start, dhp->dh_stop));
	
		dhp->dh_flags = DMADH_INITBUF;
		s = spldma();
		dma_enqueue(&sd->chan, dhp);
	} else
		s = spldma();

	if (ddp)
		sd->ndma++;
	if (startup && (*(int *)mon&dmaen) == 0)
	{
		/*
		 * Now that we have more than one dma ready
		 * to go start it up.
		 * Make sure we don't start a done buffer
		 * (race with dma completion interrupt).
		 */
		for (  dhp = sd->chan.dc_queue.dq_head
		     ; dhp && dhp->dh_state
		     ; dhp = dhp->dh_link)
			;
		snd_dbug2(("snd_start: start\n"));
		dma_start(&sd->chan, dhp, DMACSR_WRITE);
		if (dir == D_READ) {
			mon_csr_or(DMAIN_DMAEN>>24);
			mon_csr_or(DMAIN_OVR>>24);
			mon_send(MON_SNDIN_CTRL(SIN_ENAB), 0);
		} else {
			sound_active = 1;

			mon->mon_csr.dmaout_dmaen = 1;
			mon->mon_csr.dmaout_ovr = 1;
			ll_snd_var.dmaflags |= SOUT_ENAB;
			mon_send_nodma(MON_SNDOUT_CTRL
				(ll_snd_var.dmaflags), 0);
/* BEGIN PROTO */
			/*
			 * Make sure that dma's really started and kick start
			 * it if it hasn't.  The problems seen so far indicate
			 * three possibilities: 1) the dma chip will see the
			 * data request and next will increment properly, 2)
			 * The first thing we'll recieve is an underrun
			 * packet, or 3) we won't recieve either, perhaps the
			 * mon_send didn't work.
			 */
			while (1) {
			    /*
			     * Wait just so long for next to start
			     * incrementing.
			     */
			    for (i = SO_JUMP_WAIT; i; i--)
				if (sd->chan.dc_ddp->dd_next > dhp->dh_start)
				{
				    snd_dbug2(("jumpstart: ok clr ovr\n"));
				    mon_csr_or(DMAOUT_OVR>>24);
				    break;
				}
			    snd_dbug2(("jumpstart: nx %x st %x csr %x i %d\n",
					sd->chan.dc_ddp->dd_next,
					dhp->dh_start, mon->mon_csr, i));
			    if (i)
				    break;
			    /*
			     * See if we recieved an underrun rather than
			     * a data request packet.
			     */
			    if (mon->mon_csr.dmaout_ovr) {
				if (sd->chan.dc_ddp->dd_next == dhp->dh_start)
				{
				    /*
				     * Underrun with no DMA activity!
				     * Assist the weak hardware by sending
				     * a word of sound data...
				     */
				    snd_dbug(("snd_start: kickstart"
						" sound out\n"));
				    mon_send_nodma(MON_SOUND_OUT,0);
				    mon_csr_or(DMAOUT_OVR>>24);
				    continue;
				} else {
				    /*
				     * Underrun, but the DMA did in fact start.
				     * This case is presently treated as a
				     * NO UNDERRUN case.
				     */
				    mon_csr_or(DMAOUT_OVR>>24);
				    break;
				}
			    }
			    snd_dbug(("snd_start: neither dma"
				      " nor underrun\n"));
			    printf("snd_start: neither dma"
				      " nor underrun, help!\n");
			    break;
			}
/* END PROTO */
		}
	}
	splx(s);
	return(1);
}

static void snd_shutdown(int dir)
{
	register int s, mon_cmd;
	struct dma_hdr *dhp;
	int dmaen = (dir == D_WRITE ? DMAOUT_DMAEN>>24 : DMAIN_DMAEN>>24);
	int ovr = (dir == D_WRITE ? DMAOUT_OVR>>24 : DMAIN_OVR>>24);

	snd_dbug(("snd_shutdown dir %s, mon_csr %x\n",
		   dir == D_WRITE ? "write" : "read",
		   mon->mon_csr));

	s = spldma();
	if (dir == D_WRITE) {
		sound_active = 0;
		ll_snd_var.dmaflags &= ~SOUT_ENAB;
		mon_cmd = MON_SNDOUT_CTRL(ll_snd_var.dmaflags);
	} else {
		mon_cmd = MON_SNDIN_CTRL(0);
	}

	/*
	 * Stop the sound out/in channel from sending any more data
	 * and abort any dma that's in progress.
	 */
	mon_send(mon_cmd, 0);
	mon_csr_and(~dmaen);
	mon_csr_or(ovr);

	ll_snd_var.dma[dir].shutdown = 1;	// no more dma reqs
	dma_abort(&ll_snd_var.dma[dir].chan);
	ll_snd_var.dma[dir].chan.dc_flags &= ~DMACHAN_ERROR;
	splx(s);

	while (dhp = dma_dequeue(&ll_snd_var.dma[dir].chan, DMADQ_ALL)) {
		register snd_dma_desc_t *ddp =
			(snd_dma_desc_t *)dhp->dh_drvarg;

		snd_dbug2(("snd_shutdown dequeue %x\n", dhp));
		if (ddp) {
			/*
			 * Setting the overflow flag causes the stream
			 * to recognize termination status.
			 */
			if (--ll_snd_var.dma[dir].ndma == 0)
				ddp->flag |= SDD_OVERFLOW;
			(*ddp->free)(ddp);
		}
	}
	ASSERT(ll_snd_var.dma[dir].ndma == 0);
}

static void mon_send_nodma(int mon_cmd, int data)
{
	register int s;

	s = spldma();

	/*
	 * Wait for any previous command to complete transmission
	 */
	while (mon->mon_csr.ctx)
		;
	mon->mon_csr.cmd = mon_cmd;
	mon->mon_data = data;
	splx(s);
}

/*
 * Thread to set volume/attributs.  We do this here, as it can take
 * a very long time.
 */
static void snd_vol_set(void)
{
	register int flags, bits, i, s;

	ni_spkren = (gpflags&SGP_SPKREN) == SGP_SPKREN;
	ni_lowpass = (gpflags&SGP_LOWPASS) == SGP_LOWPASS;
	snd_dbug(("snd_vol_set vol_r %x, vol_l %x, gpflags %x\n",
		   vol_r, vol_l, gpflags, 0, 0, 0, 0, 0, 0));
	ll_snd_var.gpflags = gpflags;

	if (vol_r == ni_vol_r && vol_l == ni_vol_l)
		mon_send(MON_GP_OUT, ll_snd_var.gpflags<<24);
	else {
		vol_r &= VOLUME_MASK;
		vol_l &= VOLUME_MASK;
		flags = ll_snd_var.gpflags<<24;
	}

	while (vol_r != ni_vol_r || vol_l != ni_vol_l) {
		if (vol_r == vol_l) {
			bits = 0x700 | VOLUME_RCHAN
				| VOLUME_LCHAN | vol_r;
			ni_vol_r = vol_r;
			ni_vol_l = vol_l;
		} else if (vol_r != ni_vol_r) {
			bits = 0x700 | VOLUME_RCHAN | vol_r;
			ni_vol_r = vol_r;
		} else {
			bits = 0x700 | VOLUME_LCHAN | vol_l;
			ni_vol_l = vol_l;
		}

		/*
		 * Set volume control port to 0
		 */
		mon_send(MON_GP_OUT, flags);
		DELAY(200);

		/*
		 * shift out the bits one at a time.
		 */
		for (i = 10; i >= 0; i--) {
			mon_send(MON_GP_OUT,
				 flags
				 | ((bits&(1<<i))
					?(SGP_VSCD<<24):0));
			DELAY(200);
			mon_send(MON_GP_OUT,
				 flags
				 | ((bits&(1<<i))?(SGP_VSCD<<24):0)
				 | (SGP_VSCCK<<24));
			DELAY(200);
		}

		/*
		 * strobe the data
		 */
		mon_send(MON_GP_OUT, flags);
		DELAY(200);
		mon_send(MON_GP_OUT, flags | (SGP_VSCS<<24));
		DELAY(200);
		mon_send(MON_GP_OUT, flags);
		DELAY(200);
	}
}

void snd_device_vol_set(void)
{
	softint_sched(CALLOUT_PRI_THREAD, snd_vol_set, 0);
}

/*
 * Save out modified volume parameters.
 */
static void snd_vol_save(void)
{
	struct nvram_info ni;
	
	nvram_check(&ni);
	ni.ni_vol_r = ni_vol_r;
	ni.ni_vol_l = ni_vol_l;
	ni.ni_lowpass = ni_lowpass;
	ni.ni_spkren = ni_spkren;
	nvram_set(&ni);
}

void snd_device_vol_save(void)
{
	softint_sched(CALLOUT_PRI_THREAD, snd_vol_save, 0);
}

/*
 * Check to see if there's a device there (of course there is!)
 */
int snd_device_probe(caddr_t addr, register int unit)
{
	if (unit != 0) return (0);
	addr += slot_id;
	return((int)addr);
}

/*
 * Attach to this device.
 */
int snd_device_attach(register struct bus_device *bd)
{
	register struct dma_chan *dma;
	register int i;

	mon = (struct monitor *)P_MON;
	/*
	 * Sound out initialization
	 */

	/* Initialize csr */
	snd_dbug2(("dmaencheck mon_csr %x\n", mon->mon_csr));
	mon_csr_and(~(DMAOUT_DMAEN>>24));
	mon_csr_or(DMAOUT_OVR>>24);
	mon_csr_and(~(DMAIN_DMAEN>>24));
	mon_csr_or(DMAIN_OVR>>24);
	snd_dbug2(("dmaencheck mon_csr %x\n", mon->mon_csr));

	mon_send(MON_SNDIN_CTRL(0), 0);
	mon_send(MON_SNDOUT_CTRL(0), 0);
	mon_send(MON_GP_OUT, 0xff);

	/*
	 * Sound out initialization
	 */
	dma = &ll_snd_var.dma[D_WRITE].chan;
	ll_snd_var.ramp = SND_PARM_RAMPUP | SND_PARM_RAMPDOWN;

	/* Initialize sound out DMA */
	dma->dc_handler = snd_dma_intr;
	dma->dc_hndlrarg = D_WRITE;
	dma->dc_hndlrpri = CALLOUT_PRI_SOFTINT1;
	dma->dc_direction = DMACSR_WRITE;
	dma->dc_ddp = (struct dma_dev *)P_SOUNDOUT_CSR;

	/*
	 * Give us an interrupt on completion and chaining.
	 */
	dma->dc_flags |= DMACHAN_INTR|DMACHAN_CHAININTR;
	dma_init(dma, I_SOUND_OUT_DMA);

	/*
	 * Sound in initialization
	 */
	dma = &ll_snd_var.dma[D_READ].chan;

	/* Initialize sound in DMA */
	dma->dc_handler = snd_dma_intr;
	dma->dc_hndlrarg = D_READ;
	dma->dc_hndlrpri = CALLOUT_PRI_SOFTINT1;
	dma->dc_direction = DMACSR_READ;
	dma->dc_ddp = (struct dma_dev *)P_SOUNDIN_CSR;

	dma->dc_flags |= DMACHAN_INTR|DMACHAN_CHAININTR;
	dma_init(dma, I_SOUND_IN_DMA);

	/* enable interrupts */
	install_scanned_intr (I_SOUND_OVRUN, (int (*)())snd_dev_intr, 0);

	return(0);
}

/*
 * Ramp from zero to the pair of values given.  The buffer is PAGE_SIZE/2
 * entries long.
 * If the from parameter is true ramp from these values towards zero, otherwise
 * ramp from zero to these values.
 * Based on Bresen line-drawing algorithm.
 */
static void snd_ramp(int left, int right, short *buf, boolean_t from)
{
	int x = PAGE_SIZE/(2*sizeof(short))-1;
	int dl, dr;
	int lc = (left < 0 ? 1 : 0), rc = (right < 0 ? 1 : 0);

	left = (left < 0 ? left-1 : left+1)<<16;
	right = (right < 0 ? right-1 : right+1)<<16;

	if (from == FALSE) {		/* Ramp from 0 to value */
		dl = left/x;
		dr = right/x;
		left = right = 0;
	} else {			/* Ramp from value to 0 */
		dl = -left/x;
		dr = -right/x;
	}

	while (x--) {
		left += dl;
		*buf++ = (left>>16) + lc;
		right += dr;
		*buf++ = (right>>16) + rc;
	}
}

int snd_device_def_dmasize(int direction)
{
	return direction == SND_DIR_PLAY ? PAGE_SIZE : 256;
}	
int snd_device_def_high_water(int direction)
{
	return direction == SND_DIR_PLAY
		? SND_DEF_HIGH_WATER_HIGH
		: SND_DEF_HIGH_WATER_LOW;
}

int snd_device_def_low_water(int direction)
{
	return direction == SND_DIR_PLAY
		? SND_DEF_LOW_WATER_HIGH
		: SND_DEF_LOW_WATER_LOW;
}
#endif	NSOUND > 0
