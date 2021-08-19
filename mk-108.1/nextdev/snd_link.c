/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 *  9-Oct-90  Gregg Kellogg (gk) at NeXT
 *	Fixed bug causing linked streams of different dma sizes to
 *	properly pause and resume.
 *
 * 25-Sep-90  Gregg Kellogg (gk) at NeXT
 *	Made pausing a stream work for streams that are linked.
 *
 *  7-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Re-worked the interface so that the size of the DSP DMA buffers can be
 *	greater or smaller than the sound buffers.  Before this only worked
 *	for DSP DMA buffers smaller than the sound buffers.
 *
 *  1-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Take number of dma descriptors from stream, not constant.
 *
 * 14-May-90  Gregg Kellogg (gk) at NeXT
 *	Changed to use us_timeout instead of timeout, as these can be
 *	called at levels > spl3().
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <sound.h>
#import <dsp.h>
#if	NSOUND > 0 && NDSP > 0

/*
 * Special interface for sending dsp output directly to sound out.
 */

#import <kern/xpr.h>
#import <sys/buf.h>
#import <sys/param.h>
#import <vm/vm_param.h>
#import <sys/callout.h>
#import <sys/kernel.h>
#import <kern/sched_prim.h>
#import <next/pmap.h>
#import <next/us_timer.h>
#import <nextdev/snd_var.h>
#import <nextdev/snd_dsp.h>
#import <nextdev/snd_snd.h>

#import <machine/spl.h>

#if	XPR_DEBUG
extern int sdbgflag;
#define snd_dbug(f) {if (sdbgflag&1)printf f; else XPR(XPR_SOUND, f);}
#define snd_dbug2(f) if (sdbgflag&2){if (sdbgflag&1)printf f; else XPR(XPR_SOUND, f);}
#else	XPR_DEBUG
#define snd_dbug(f)
#define snd_dbug2(f)
#endif	XPR_DEBUG

extern snd_var_t snd_var;
extern dsp_var_t dsp_var;
extern ll_snd_var_t ll_snd_var;

static void snd_link_init_buf (
	snd_dma_desc_t	*ddp,
	vm_address_t	bufs,
	int		nbufs,
	int		buf_size,
	int		(*free)(snd_dma_desc_t *),
	caddr_t		ptr,
	queue_t		q);
static void snd_link_reset(int direction);
static int snd_link_dsp_complete(register snd_dma_desc_t *ddp);
static int snd_link_snd_complete(register snd_dma_desc_t *ddp);

static queue_head_t snd_ddp_q[2], dsp_ddp_q[2];
static queue_head_t snd_pause_q[2], dsp_pause_q[2];
static snd_region_t snd_link_dsp_reg[2];
static thread_t link_thread[2];
static vm_address_t bufs[2];
static vm_size_t buf_size[2];
static u_int snd_buf_count[2];
static u_int dsp_buf_count[2];

/*
 * Link between the DSP and sound devices is used for sound-out only.
 * 
/*
 * Initialize the dsp - sound link.
 */
void snd_link_init(int direction)
{
	snd_dma_desc_t *ddp, *ddp2;
	int s, dsp_buf_size, snd_buf_size;
	int small_buf_size, large_buf_size;
	struct chan_data *cdp = dsp_var.chan
		[direction == SND_DIR_PLAY ? DSP_SO_CHAN : DSP_SI_CHAN];
	int nbufs, buf_ratio;

	ASSERT(curipl() == 0);
	snd_link_dsp_reg[direction].sound_q = &cdp->sndq;

	ASSERT(cdp);
	dsp_buf_size = cdp->sndq.dmasize;
	snd_buf_size = ll_snd_var.dma[direction].sndq.dmasize;

	cdp->sndq.linked = TRUE;
	ll_snd_var.dma[direction].sndq.linked = TRUE;

	if (dsp_buf_size < snd_buf_size) {
		small_buf_size = dsp_buf_size;
		large_buf_size = snd_buf_size;
	} else {
		small_buf_size = snd_buf_size;
		large_buf_size = dsp_buf_size;
	}
	buf_ratio = large_buf_size/small_buf_size;

	if (direction == SND_DIR_PLAY) {
		/*
		 * Take nbufs from sound-out stream.
		 */
		nbufs = ll_snd_var.dma[direction].sndq.ndma;
	} else {
		/*
		 * Take nbufs from dsp stream.
		 */
		nbufs = cdp->sndq.ndma;
	}
	buf_size[direction] = large_buf_size * nbufs;

	ASSERT(snd_buf_size <= PAGE_SIZE && snd_buf_size <= PAGE_SIZE);
	queue_init(&snd_ddp_q[direction]);
	queue_init(&dsp_ddp_q[direction]);
	queue_init(&snd_pause_q[direction]);
	queue_init(&dsp_pause_q[direction]);
	link_thread[direction] = 0;
	ddp = (snd_dma_desc_t *)kalloc(sizeof(snd_dma_desc_t)*nbufs);
	snd_dbug(("snd_link: ddp %d bytes at 0x%x\n", 
		sizeof(snd_dma_desc_t)*nbufs, ddp));
	ddp2 = (snd_dma_desc_t *)
		kalloc(sizeof(snd_dma_desc_t)*buf_ratio*nbufs);
	snd_dbug(("snd_link: ddp2 %d bytes at 0x%x\n", 
		sizeof(snd_dma_desc_t)*buf_ratio*nbufs, ddp2));
	bufs[direction] = (vm_address_t)
		kalloc(round_page(buf_size[direction]));
	snd_dbug(("snd_link: bufs[%d] %d bytes at 0x%x\n", direction,
		buf_size[direction], bufs[direction]));

	/*
	 * Initialize queued ddps and enqueue them.
	 */
	if (snd_buf_size == large_buf_size) {
		snd_link_init_buf(ddp, bufs[direction], nbufs, snd_buf_size,
			snd_link_snd_complete, (caddr_t)direction,
			&snd_ddp_q[direction]);
		snd_buf_count[direction] = nbufs;
		snd_link_init_buf(ddp2, bufs[direction], nbufs*buf_ratio,
			dsp_buf_size, snd_link_dsp_complete,
			(caddr_t)&snd_link_dsp_reg[direction],
			&dsp_ddp_q[direction]);
		dsp_buf_count[direction] = nbufs*buf_ratio;
	} else {
		snd_link_init_buf(ddp, bufs[direction], nbufs, dsp_buf_size,
			snd_link_dsp_complete,
			(caddr_t)&snd_link_dsp_reg[direction],
			&dsp_ddp_q[direction]);
		dsp_buf_count[direction] = nbufs;
		snd_link_init_buf(ddp2, bufs[direction], nbufs*buf_ratio,
			snd_buf_size, snd_link_snd_complete,
			(caddr_t)direction, &snd_ddp_q[direction]);
		snd_buf_count[direction] = nbufs*buf_ratio;
	}

	s = spldma();
	/*
	 * Start up dma.
	 */
	if (direction == SND_DIR_RECORD) {
		/*
		 * Start up sound.
		 */
		while (!queue_empty(&snd_ddp_q[direction])) {
			queue_remove_first(&snd_ddp_q[direction], ddp,
				snd_dma_desc_t *, link);
			snd_device_start(ddp, SND_DIR_RECORD,
				(dsp_var.flags&F_MODE_SND_HIGH)
					? SND_RATE_44
					: SND_RATE_22);
		}
	} else {
		/*
		 * Start up the dsp.
		 */
		while (!queue_empty(&dsp_ddp_q[direction])) {
			queue_remove_first(&dsp_ddp_q[direction], ddp,
				snd_dma_desc_t *, link);
			(*cdp->sndq.start)(ddp, SND_DIR_RECORD, 0);
		}
	}
	splx(s);
}

static void snd_link_init_buf (
	snd_dma_desc_t	*ddp,
	vm_address_t	bufs,
	int		nbufs,
	int		buf_size,
	int		(*free)(snd_dma_desc_t *),
	caddr_t		ptr,
	queue_t		q)
{
	pmap_t k_pmap = (pmap_t)pmap_kernel();

	while (nbufs--) {
		ddp->vaddr = bufs;
		ddp->size = buf_size;
		ddp->ptr = ptr;
		ddp->free = free;
		ddp->hdr.dh_start = (char *)pmap_resident_extract(k_pmap,
						trunc_page(bufs))
				    + bufs%PAGE_SIZE;
		ddp->hdr.dh_stop = ddp->hdr.dh_start + buf_size;
		ddp->hdr.dh_drvarg = (int)ddp;
		queue_enter(q, ddp, snd_dma_desc_t *, link);
		ddp++;
		bufs += buf_size;
	}
}

/*
 * Release linkage resources
 */
static void snd_link_reset(int direction)
{
	register snd_dma_desc_t *ddp, *first_ddp;
	struct chan_data *cdp = dsp_var.chan
		[direction == SND_DIR_PLAY ? DSP_SO_CHAN : DSP_SI_CHAN];
	int nbufs, req_bufs, s;

	ASSERT(curipl() == 0);

	s = spldma();
	/*
	 * Count up the number of dsp bufs.
	 */
	for (    nbufs = 0
	       , ddp = (snd_dma_desc_t *)queue_first(&dsp_ddp_q[direction])
	       , first_ddp = ddp
	     ; !queue_end(&dsp_ddp_q[direction], (queue_t)ddp)
	     ; nbufs++, ddp = (snd_dma_desc_t *)queue_next(&ddp->link))
		if (ddp < first_ddp)
			first_ddp = ddp;

	splx(s);

	ddp = first_ddp;
	req_bufs = dsp_buf_count[direction];
	if (nbufs && req_bufs == nbufs) {
		/*
		 * We can free the ddps, everything's in.
		 */
		kfree(ddp, nbufs * sizeof(*ddp));
		snd_dbug(("snd_link_reset: free %d bytes at 0x%x (dsp ddps)\n",
			nbufs * sizeof(*ddp), ddp));
		queue_init(&dsp_ddp_q[direction]);
		dsp_buf_count[direction] = 0;
	} else
		snd_dbug(("snd_link_reset: dsp bufs %d != req bufs %d\n",
			nbufs, req_bufs));

	s = spldma();
	/*
	 * Count up the number of sound bufs.
	 */
	for (    nbufs = 0
	       , ddp = (snd_dma_desc_t *)queue_first(&snd_ddp_q[direction])
	       , first_ddp = ddp
	     ; !queue_end(&snd_ddp_q[direction], (queue_t)ddp)
	     ; nbufs++, ddp = (snd_dma_desc_t *)queue_next(&ddp->link))
		if (ddp < first_ddp)
			first_ddp = ddp;

	splx(s);

	ddp = first_ddp;
	req_bufs = snd_buf_count[direction];
	if (nbufs && req_bufs == nbufs) {
		/*
		 * We can free the ddps, everything's in.
		 */
		kfree(ddp, nbufs * sizeof(*ddp));
		snd_dbug(("snd_link_reset: free %d bytes at 0x%x (snd ddps)\n",
			nbufs * sizeof(*ddp), ddp));
		queue_init(&snd_ddp_q[direction]);
		snd_buf_count[direction] = 0;
	} else
		snd_dbug(("snd_link_reset: snd bufs %d != req bufs %d\n",
			nbufs, req_bufs));

	/*
	 * Free up the buffer's when everything's in.
	 */
	if (snd_buf_count[direction] == 0 && dsp_buf_count[direction] == 0) {
		kfree(bufs[direction], round_page(buf_size[direction]));
		snd_dbug(("snd_link_reset: free %d bytes at 0x%x (bufs)\n",
			round_page(buf_size[direction]), bufs[direction]));
		bufs[direction] = 0;
		buf_size[direction] = 0;

		/*
		 * cdp may have been freed if we're shutting down.
		 */
		if (cdp)
			cdp->sndq.linked = FALSE;
		ll_snd_var.dma[direction].sndq.linked = FALSE;
	}
}

/*
 * Wait for sound to underrun/overrun
 */
void snd_link_shutdown(int direction)
{
	snd_queue_t *q = &ll_snd_var.dma[direction].sndq;
	int s, ndma;
	snd_dma_desc_t *ddp;

	ASSERT(curipl() == 0);
	snd_dbug(("snd_link_shutdown: reset dir %d\n", direction));
	snd_link_reset(direction);
	s = splsound();
	link_thread[direction] = current_thread();
	if (direction == SND_DIR_PLAY)
		ndma = ll_snd_var.dma[SND_DIR_PLAY].ndma;
	else
		ndma = dsp_var.chan[DSP_SI_CHAN]->sndq.ndma;

	/*
	 * Free ddps from paused queues.
	 */
	while (!queue_empty(&snd_pause_q[direction])) {
		queue_remove(&snd_pause_q[direction], ddp,
			snd_dma_desc_t *, link);
		queue_enter(&snd_ddp_q[direction], ddp,
			snd_dma_desc_t *, link);
	}
	while (!queue_empty(&dsp_pause_q[direction])) {
		queue_remove(&dsp_pause_q[direction], ddp,
			snd_dma_desc_t *, link);
		queue_enter(&dsp_ddp_q[direction], ddp,
			snd_dma_desc_t *, link);
	}

	if (ndma) {
		/*
		 * Wait for all dma's to complete.
		 */
		snd_dbug(("snd_link_shutdown: await %d dmas dir %d\n",
			ndma, direction));
		assert_wait((int)q, TRUE);
		thread_set_timeout(hz);
		thread_block();
		snd_dbug(("snd_link_shutdown: done waiting dir %d\n",
			ndma, direction));
	}
	link_thread[direction] = 0;
	splx(s);
}

/*
 * Pause data on a linked stream.
 */
void snd_link_pause(snd_queue_t *q)
{
	/*
	 * This is done implicitly when before data's passed on to the
	 * queue by looking at the paused bit.
	 */
	snd_dbug(("sl_pause: pausing queue 0x%x\n", q));
}

/*
 * Resume a paused stream that's linked.
 */
void snd_link_resume(snd_queue_t *q)
{
	int direction, s;
	snd_dma_desc_t *ddp;

	snd_dbug(("sl_resume: resuming queue 0x%x\n", q));

	/*
	 * Start up data waiting to be sent to this stream.
	 * Only start buffers that have counter parts in both queues.
	 */
	s = spldma();
	if (q == &ll_snd_var.dma[SND_DIR_PLAY].sndq) {
		snd_dbug2(("sl_resume: q sound out\n", q));
		direction = SND_DIR_PLAY;
		while (!queue_empty(&snd_pause_q[direction])) {
			queue_remove_first(&snd_pause_q[direction], ddp,
				snd_dma_desc_t *, link);
			snd_device_start(ddp, direction,
				(dsp_var.flags&F_MODE_SND_HIGH
					? SND_RATE_44
					: SND_RATE_22));
		}
	} else if (q == &ll_snd_var.dma[SND_DIR_RECORD].sndq){
		snd_dbug2(("sl_resume: q sound in\n", q));
		direction = SND_DIR_RECORD;
		while (!queue_empty(&snd_pause_q[direction])) {
			queue_remove_first(&snd_pause_q[direction], ddp,
				snd_dma_desc_t *, link);
			snd_device_start(ddp, direction,
				(dsp_var.flags&F_MODE_SND_HIGH
					? SND_RATE_44
					: SND_RATE_22));
		}
	} else if (q == snd_link_dsp_reg[SND_DIR_PLAY].sound_q) {
		snd_dbug2(("sl_resume: q dsp play\n", q));
		direction = SND_DIR_PLAY;
		while (!queue_empty(&dsp_pause_q[direction])) {
			queue_remove_first(&dsp_pause_q[direction], ddp,
				snd_dma_desc_t *, link);
			(*q->start)(ddp, 1-direction, 0);
		}
	} else if (q == snd_link_dsp_reg[SND_DIR_RECORD].sound_q) {
		snd_dbug2(("sl_resume: q dsp record\n", q));
		direction = SND_DIR_RECORD;
		while (!queue_empty(&dsp_pause_q[direction])) {
			queue_remove_first(&dsp_pause_q[direction], ddp,
				snd_dma_desc_t *, link);
			(*q->start)(ddp, 1-direction, 0);
		}
	}
	splx(s);
}

/*
 * Completion routine called when a dsp buffer is complete.
 */
static int snd_link_dsp_complete(register snd_dma_desc_t *ddp)
{
	snd_dma_desc_t *ddp2;
	int direction;
	int r = 1;
	int s;
	snd_queue_t *snd_q;

	if (&snd_link_dsp_reg[SND_DIR_PLAY] == (snd_region_t *)ddp->ptr)
		direction = SND_DIR_PLAY;
	else
		direction = SND_DIR_RECORD;

	snd_q = &ll_snd_var.dma[direction].sndq;

	/*
	 * If this is the last buffer in a series, start up the
	 * sound-out buffer.
	 */
	snd_dbug2(("sl_dsp_comp: ddp 0x%x done\n",ddp));
	s = spldma();
	queue_enter(&dsp_ddp_q[direction], ddp, snd_dma_desc_t *, link);

	/*
	 * Closing down, don't send anything on to sound out.
	 */
	if (   (direction == SND_DIR_PLAY && !(dsp_var.flags&F_LINKEDOUT))
	    || (direction == SND_DIR_RECORD && !(dsp_var.flags&F_LINKEDIN)))
	{
		splx(s);
		snd_dbug(("sl_dsp_comp: ddp 0x%x reset\n", ddp));
		softint_sched(CALLOUT_PRI_THREAD, snd_link_reset, direction);
		return 0;
	}

	ASSERT(!queue_empty(&snd_ddp_q[direction]));
	ddp2 = (snd_dma_desc_t *)queue_first(&snd_ddp_q[direction]);

	if (ddp->size > ddp2->size) {
		/*
		 * Send down the corresponding sound descriptors
		 */
		while (!queue_empty(&snd_ddp_q[direction])) {
			queue_remove_first(&snd_ddp_q[direction], ddp2,
				snd_dma_desc_t *, link);
			if (  ddp2->hdr.dh_start < ddp->hdr.dh_start
			    || ddp2->hdr.dh_stop > ddp->hdr.dh_stop)
			{
				snd_dbug2(("sl_dsp_comp: don't start ddp2 "
					"0x%x\n", ddp2));
				queue_enter_first(&snd_ddp_q[direction], ddp2,
					snd_dma_desc_t *, link);
				break;
			}
			if (snd_q->paused) {
				snd_dbug2(("sl_dsp_comp: snd_q paused, "
					"add ddp %d to pause queue\n", ddp2));
				queue_enter(&snd_pause_q[direction],
					ddp2, snd_dma_desc_t *, link);
				break;
			}
			splx(s);

			snd_dbug2(("sl_dsp_comp: start ddp 0x%x\n",ddp2));
			if (!(r = snd_device_start(ddp2, direction,
				(dsp_var.flags&F_MODE_SND_HIGH)
					? SND_RATE_44
					: SND_RATE_22)))
			{
				snd_dbug2(("sl_dsp_comp: snd_device_start "
					"failed, req ddp2 0x%x\n", ddp2));
				s = spldma();
				queue_enter_first(&snd_ddp_q[direction], ddp2,
					snd_dma_desc_t *, link);
				break;
			}
			s = spldma();
		}
		splx(s);
	} else {
		queue_remove_first(&snd_ddp_q[direction], ddp2,
			snd_dma_desc_t *, link);

		/*
		 * If this is the last buffer in a series, send down
		 * the next sound descriptor.
		 */
		if (ddp2->hdr.dh_stop == ddp->hdr.dh_stop) {
		    if (snd_q->paused) {
			snd_dbug2(("sl_dsp_comp: snd_q paused, "
				"add ddp %d to pause queue\n", ddp2));
			queue_enter(&snd_pause_q[direction],
				ddp2, snd_dma_desc_t *, link);
		    } else {
			splx(s);
			snd_dbug2(("sl_dsp_comp: start ddp2 0x%x\n", ddp2));
			r = snd_device_start(ddp2, direction,
				(dsp_var.flags&F_MODE_SND_HIGH)
					? SND_RATE_44
					: SND_RATE_22);
			if (!r) {
				snd_dbug2(("sl_dsp_comp: snd_device_start "
					"failed, req ddp2 0x%x\n", ddp2));
				s = spldma();
				queue_enter_first(&snd_ddp_q[direction], ddp2,
						snd_dma_desc_t *, link);
				splx(s);
			}
		    }
		} else {
			queue_enter_first(&snd_ddp_q[direction], ddp2,
					snd_dma_desc_t *, link);
			splx(s);
			snd_dbug2(("sl_dsp_comp: requeue ddp2  0x%x\n", ddp2));
		}
	}

	return r;
}

static int snd_link_snd_complete(register snd_dma_desc_t *ddp)
{
	register snd_dma_desc_t *ddp2;
	int direction = (int)ddp->ptr;
	struct chan_data *cdp = dsp_var.chan
		[direction == SND_DIR_PLAY ? DSP_SO_CHAN : DSP_SI_CHAN];
	snd_queue_t *q = &ll_snd_var.dma[direction].sndq;
	int s;
	int r = 1;

	snd_dbug2(("sl_snd_comp: ddp 0x%x done\n", ddp));
	/*
	 * If this is the last buffer in a series, start up the
	 * sound-out buffer.
	 */
	s = spldma();
	queue_enter(&snd_ddp_q[direction], ddp, snd_dma_desc_t *, link);

	/*
	 * If someone's waiting for sound to over/underrun, then
	 * wake them up now.
	 */
	if ((ddp->flag&SDD_OVERFLOW) && link_thread[direction]) {
		/*
		 * Wait for all dma's to complete.
		 */
		softint_sched(CALLOUT_PRI_SOFTINT1, thread_wakeup, (int)q);
	}

	/*
	 * Closing down, don't send anything on to sound out.
	 */
	if (   (direction == SND_DIR_PLAY && !(dsp_var.flags&F_LINKEDOUT))
	    || (direction == SND_DIR_RECORD && !(dsp_var.flags&F_LINKEDIN)))
	{
		splx(s);
		snd_dbug(("sl_snd_comp: ddp 0x%x reset\n", ddp));
		softint_sched(CALLOUT_PRI_THREAD, snd_link_reset, direction);
		return 0;
	}

	/*
	 * The sound driver sometimes splits up buffers, in which
	 * case the starting address of the returned buffer will
	 * be half-way between the real start and the end.
	 */
	if (ddp->hdr.dh_stop - ddp->hdr.dh_start != ddp->size)
		ddp->hdr.dh_start = ddp->hdr.dh_stop - ddp->size;

	ASSERT(!queue_empty(&dsp_ddp_q[direction]));
	ddp2 = (snd_dma_desc_t *)queue_first(&dsp_ddp_q[direction]);

	if (ddp->size > ddp2->size) {
		/*
		 * Send down the corresponding dsp descriptors
		 */
		while (!queue_empty(&dsp_ddp_q[direction])) {
			queue_remove_first(&dsp_ddp_q[direction], ddp2,
				snd_dma_desc_t *, link);
			if (  ddp2->hdr.dh_start < ddp->hdr.dh_start
			    || ddp2->hdr.dh_stop > ddp->hdr.dh_stop)
			{
				snd_dbug2(("sl_snd_comp: don't start ddp2 "
					"0x%x\n", ddp2));
				queue_enter_first(&dsp_ddp_q[direction], ddp2,
					snd_dma_desc_t *, link);
				break;
			}
			/*
			 * Don't start up a paused stream.
			 */
			if (cdp->sndq.paused) {
				snd_dbug2(("sl_snd_comp: q paused, "
					"add ddp %d to pause queue\n", ddp2));
				queue_enter(&dsp_pause_q[direction],
					ddp2, snd_dma_desc_t *, link);
				break;
			}
			splx(s);
			snd_dbug2(("sl_snd_comp: start ddp 0x%x\n",ddp2));
			r = (*cdp->sndq.start)(ddp2, 1-direction, 0);
			if (!r) {
				snd_dbug2(("sl_snd_comp: (*cdp->sndq.start) "
					"failed, req ddp2 0x%x\n", ddp2));
				s = spldma();
				queue_enter_first(&dsp_ddp_q[direction], ddp2,
					snd_dma_desc_t *, link);
				break;
			}
			s = spldma();
		}
		splx(s);
	} else {
		/*
		 * If this is the last buffer in a series, send down
		 * the next dsp descriptor.
		 */
		queue_remove_first(&dsp_ddp_q[direction], ddp2,
			snd_dma_desc_t *, link);

		if (ddp2->hdr.dh_stop == ddp->hdr.dh_stop) {
		    if (cdp->sndq.paused) {
			snd_dbug2(("sl_snd_comp: q paused, "
				"add ddp %d to pause queue\n", ddp2));
			queue_enter(&dsp_pause_q[direction],
				ddp2, snd_dma_desc_t *, link);
		    } else {
			splx(s);
			snd_dbug2(("sl_snd_comp: start ddp2 0x%x\n", ddp2));
			r = (*cdp->sndq.start)(ddp2, 1-direction, 0);
			if (!r) {
				snd_dbug2(("sl_snd_comp: (*cdp->sndq.start) "
					"failed, req ddp2 0x%x\n", ddp2));
				s = spldma();
				queue_enter_first(&snd_ddp_q[direction], ddp2,
						snd_dma_desc_t *, link);
				splx(s);
			}
		    }
		} else {
			queue_enter_first(&dsp_ddp_q[direction], ddp2,
				snd_dma_desc_t *, link);
			splx(s);
			snd_dbug2(("sl_snd_comp: requeue ddp2 0x%x\n", ddp2));
		}
	}
	
	return r;
}

#endif	NSOUND > 0 && NDSP > 0
