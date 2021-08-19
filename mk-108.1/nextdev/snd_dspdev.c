/* 
 * Copyright (c) 1988,1989,1990 NeXT, Inc.
 */
/*
 * HISTORY
 * 24-Oct-90  Julius Smith (jos) at NeXT
 *	Add condition to wait for HC to clear before sending DSP_hc_HOST_RD
 *	host message.
 *
 * 22-Oct-90  Julius Smith (jos) at NeXT
 *	Fixed enabling of DSP interrupt for conditions already satisfied.
 *
 * 17-Sep-90  Gregg Kellogg (gk) at NeXT
 *	Fixed dsp_dma() to startup DMA before telling the DSP to go.
 *
 * 23-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Changed support for TXD interrupt to be based off a protocol bit.
 *
 * 22-Aug-90  Julius Smith (jos) at NeXT
 *	Set TREQ in the DSP interface to force an interrupt after
 *	a region is enqueued for DMA transfer
 *
 * 22-Aug-90  Julius Smith (jos) at NeXT
 *	Break out of the device look when waiting for a DMA buffer.
 *
 * 21-Aug-90  Julius Smith (jos) at NeXT
 *	Added protocol message type for setting the protocol from the
 *	dsp command queue.
 *
 *  2-Jul-90  Julius Smith (jos) and Matt Christiano (mchristo) at NeXT
 *	HF2 and HF3 now imply DSP abort only in HFABORT protocol mode.
 *		Instead of DSP reset, goes to dsp_aborted state.
 *	Added new state variable dsp_var.ui_dma_read_state.
 *		This was necessary to prevent UI,DI DMA collisions.
 *	Cleaned up DMA state sequence.
 *	Removed assumptions on HF2.
 *	"Host Message" protocol is now DSPMSG|DSPERR protocol.
 *
 *  7-Jun-90  Matt Christiano (mchristo) at NeXT
 *	More DMA changes.
 *
 *  9-May-90  Matt Christiano (mchristo) at NeXT
 *	Miscellaneous changes for DMA out to DSP and user initiated DMA.
 *
 * 15-Mar-90  John Seamons (jks) at NeXT
 *	Added dsp_interrupt() routine for new interrupt enable structure
 *	using BMAP chip.
 *
 * 12-Mar-90  John Seamons (jks) at NeXT
 *	Must only do byte accesses on the 68040 to the byte-wide DSP device.
 *
 * 21-Jul-89  Gregg Kellogg (gk) at NeXT
 *	Don't execute device loop if we're pending a dma input
 *	while awaiting a buffer if there's something to read.  We may
 *	still execute the buffer if one of the other conditions is true
 *	(like we now have a buffer, or we can process the output queue).
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <dsp.h>
#if	NDSP > 0

/*
 * Low level interface to dsp driver.  Routines here do low-level
 * interrupt and dma activity and move device through different states.
 */

#import <kern/xpr.h>
#import <sys/buf.h>
#import <sys/param.h>
#import <kern/thread.h>
#import <sys/callout.h>
#import <sys/machine.h>
#import <vm/vm_param.h>
#import <next/autoconf.h>
#import <next/cpu.h>
#import <next/pmap.h>
#import <next/scr.h>
#import <next/bmap.h>
#import <nextdev/snd_var.h>
#import <nextdev/snd_dsp.h>
#import <nextdev/snd_dspreg.h>

#import <machine/spl.h>

extern snd_var_t snd_var;
dsp_var_t dsp_var;
enum dsp_int_en { DSP_INT_ENABLE, DSP_INT_DISABLE };

#if	DEBUG
volatile struct dsp_regs *regs;
#else	DEBUG
#define regs ((volatile struct dsp_regs *)P_DSP)
#endif	DEBUG

/* interrupt vectors */
static void dsp_dev_intr(void);
static void dsp_dma_intr(void);
static void dsp_interrupt(enum dsp_int_en dsp_int_en);

/* dma routines */
static void dsp_dma_init(int address, int skip, int space, int mode,
	int direction);
static void dsp_dma(register int size, int channel, int mode, int direction);
static void dsp_dma_cleanup(register int size, int channel, int mode,
	int direction);

/*
 * autoconfiguration
 */
static int dsp_probe(caddr_t addr, register int unit);
static int dsp_attach(register struct bus_device *bd);

struct bus_device	*dsp_dinfo[NDSP];
struct bus_driver	 dspdriver = {
	dsp_probe, 0, dsp_attach, 0, 0, (int (*)())dsp_dev_intr,
	0, sizeof(struct dsp_regs), "dsp", dsp_dinfo
};

/* Register Locations */
extern volatile int *scr2;
extern volatile int *intrstat;

#if	DEBUG || XPR_DEBUG
int dspdbgflag = 0;
#endif	DEBUG || XPR_DEBUG
#if	XPR_DEBUG

#define dspdbug(f) {if (dspdbgflag&1)printf f; else XPR(XPR_DSP, f);}
#define dspdbug2(f) { if (dspdbgflag&2) { \
			if (dspdbgflag&1)printf f; else XPR(XPR_DSP, f);} \
		    }
#define dspdmadbug(f) { if (dspdbgflag&4) XPR(XPR_SOUND, f);}
#define DSPREGSFMT "icr %x cvr %x isr %x ivr %x\n", \
		regs->icr, regs->cvr, regs->isr, regs->ivr
#else	XPR_DEBUG
#define dspdbug(f)
#define dspdbug2(f)
#define dspdmadbug(f)
#endif	XPR_DEBUG

/*
 * Initialize dsp specific stuff.
 */
void dsp_dev_init(void)
{
	register int i;
	register struct chan_data *cdp;

	/*
	 * Per-device device dma initialization.
	 */
	dsp_var.dma_chan.dc_ddp = (struct dma_dev *)P_DSP_CSR;
	dsp_var.dma_chan.dc_flags = DMACHAN_AUTOSTART|DMACHAN_INTR;
	dsp_var.dma_chan.dc_hndlrarg = 0;
#if	XPR_DEBUG
	if (xprflags&XPR_DSP)
		dsp_var.dma_chan.dc_flags |= DMACHAN_DBG;
#endif	XPR_DEBUG

	dsp_var.flags = 0;
	dsp_var.msg_port = PORT_NULL;
	dsp_var.err_port = PORT_NULL;

	dsp_var.errbuf = 0;

	dspq_init_lmsg();

	/*
	 * Enable all interrupts.
	 */
	dsp_interrupt(DSP_INT_ENABLE);
}

/*
 * Reset the dsp back to un-used state.
 */
void dsp_dev_reset(void)
{
	register int s, i, j;
	register struct dma_hdr *dhp;
	register snd_dma_desc_t *ddp;
	register struct chan_data *cdp;
	register queue_t ddpq;

	if (dsp_var.event_msg) {
		dsp_var.event_mask = 0;
		dsp_var.event_condition = 0;
		dspq_free_msg(dsp_var.event_msg);
	}

	dsp_var.state = normal;
	dsp_var.ui_dma_read_state = 0;
	dsp_var.flags |= F_SHUTDOWN;

	dsp_dev_new_proto(0);

	dspq_reset_lmsg();

	/*
	 * Disable all interrupts.
	 */
	dsp_interrupt(DSP_INT_DISABLE);
}

/*
 * Change the protocol from one setup to another.
 */
void dsp_dev_new_proto(int new_proto)
{
        register snd_dma_desc_t *ddp;
	register queue_t q;
	register struct dma_hdr *dhp;
	register int i;
	int s, s1;

	dspdbug2(("dsp_dev_new_proto: (from %x to %x)\n",
		dsp_var.flags, new_proto));

	/*
	 * Can't free or allocate memory at spl > 0
	 */
	if (!(new_proto&SND_DSP_PROTO_LINKOUT) && (dsp_var.flags&F_LINKEDOUT))
	{
  	        dsp_var.flags &= ~F_LINKEDOUT;
		snd_link_shutdown(SND_DIR_PLAY);
	}
	
	if (!(new_proto&SND_DSP_PROTO_LINKIN) && (dsp_var.flags&F_LINKEDIN))
	{
	        dsp_var.flags &= F_LINKEDIN;
		snd_link_shutdown(SND_DIR_RECORD);
	}
	
	if ( new_proto&SND_DSP_PROTO_DSPERR ) {
	    if ( !(dsp_var.flags&F_MODE_DSPERR) ) {
	        dsp_var.flags |= F_MODE_DSPERR;
		if (!dsp_var.errbuf) {
		    dsp_var.errbuf = 
			(u_int *)kalloc(DSP_DEF_EBUFSIZE*sizeof(int));
		    dsp_var.eerrbuf = dsp_var.errbuf+DSP_DEF_EBUFSIZE;
		    dsp_var.errp = dsp_var.errbuf;
		}
	    }
	} else {
	    if ( dsp_var.flags&F_MODE_DSPERR ) {
		dsp_var.flags &= ~F_MODE_DSPERR;	// suspend errors
	    }
	}

	/* msg buffer always allocated since it receives
	   spurious words in complex DMA mode */
	if ( new_proto&SND_DSP_PROTO_DSPMSG )
	  dsp_var.flags |= F_MODE_DSPMSG;
	else
	  dsp_var.flags &= ~F_MODE_DSPMSG;
	    
	if ( new_proto&SND_DSP_PROTO_RAW )
	  dsp_var.flags |= F_MODE_RAW;
	else
	  dsp_var.flags &= ~F_MODE_RAW;
	    
	if ( new_proto&SND_DSP_PROTO_HFABORT )
	  dsp_var.flags |= F_MODE_HFABORT;
	else
	  dsp_var.flags &= ~F_MODE_HFABORT;
	    
	s = spldsp();

	if (new_proto&SND_DSP_PROTO_HIGH)
	  dsp_var.flags |= F_MODE_SND_HIGH;
	else
	  dsp_var.flags &= ~F_MODE_SND_HIGH;

	if ((new_proto&SND_DSP_PROTO_C_DMA) && !(dsp_var.flags&F_MODE_C_DMA)) {
	        dsp_var.flags |= F_MODE_C_DMA;
		
		/*
		 * If buffers are ready for the DSP, start
		 * up each of the channels.
		 */
		for (i = DSP_N_CHAN-1; i >= 0; i--) {
		        /*
			 * Sounddata always must go via programmed io.
			 */
		        if (   i == DSP_SI_CHAN
			    && (new_proto&SND_DSP_PROTO_SOUNDDATA))
				continue;
			if (   dsp_var.chan[i]
			    && !queue_empty(&dsp_var.chan[i]->ddp_q))
			{
				register snd_region_t *reg;
				q = &dsp_var.chan[i]->ddp_q;
				ddp = (snd_dma_desc_t *)queue_last(q);
				queue_remove(q, ddp, snd_dma_desc_t *, link);
				reg = (snd_region_t *)ddp->ptr;
				dspq_start_complex(ddp, reg->direction, 0);
			}
			/*
			 * Make sure that the appropriate protocol
			 * is used for dma.
			 */
			if (dsp_var.chan[i])
				dsp_var.chan[i]->sndq.start
					= dspq_start_complex;
		}
		dsp_var.dma_chan.dc_flags |= DMACHAN_DMAINTR;
		dsp_var.dma_chan.dc_hndlrpri = CALLOUT_PRI_DSP;
		dsp_var.dma_chan.dc_handler = dsp_dev_intr;
		dsp_var.dma_chan.dc_dmaintr = dsp_dma_intr;
		dma_init(&dsp_var.dma_chan, I_DSP_DMA);

	} else if (   !(new_proto&SND_DSP_PROTO_C_DMA)
		   && (dsp_var.flags&F_MODE_C_DMA))
	{
		dsp_var.flags &= ~F_MODE_C_DMA;
		
		/*
		 * Abort any dma.
		 */
		dma_abort(&dsp_var.dma_chan);
		while (dhp = dma_dequeue(&dsp_var.dma_chan, DMADQ_ALL)) {
			snd_dma_desc_t *ddp;
			ddp = (snd_dma_desc_t *)dhp->dh_drvarg;
	
			dspdbug2(("dsp_dev_newproto dequeue %x\n", dhp));
			if (ddp) {
				int chan = dsp_var.pend_dma_chan;
				if (   !dsp_var.dma_chan.dc_queue.dq_head
				    && queue_empty(&dsp_var.chan[chan]->ddp_q))
					ddp->flag |= SDD_OVERFLOW;
				(*ddp->free)(ddp);
			}
		}

		/*
		 * Return any buffered data.
		 */
		for (i = DSP_N_CHAN-1; i >= 0; i--) {
			/*
			 * Sounddata always must go via programmed io.
			 */
			if (   i == DSP_SI_CHAN
			    && (new_proto&SND_DSP_PROTO_SOUNDDATA))
				continue;
			if (dsp_var.chan[i]) {
				q = &dsp_var.chan[i]->ddp_q;
				while (!queue_empty(q)) {
					queue_remove_first(q, ddp,
						snd_dma_desc_t *, link);
					if (queue_empty(q))
						ddp->flag |= SDD_OVERFLOW;
					(*ddp->free)(ddp);
				}
				snd_stream_queue_reset(&dsp_var.chan[i]->sndq);
				kfree(dsp_var.chan[i],
					sizeof(*dsp_var.chan[i]));
				dsp_var.chan[i] = 0;
			}
		}
	}
	if ((new_proto&SND_DSP_PROTO_S_DMA) && !(dsp_var.flags&F_MODE_S_DMA)) {
		dsp_var.flags |= F_MODE_S_DMA;

		/*
		 * Simple mode uses IREQB (gated by BLOCK_END_ENABLE and
		 * DMACOMPLETE) to notify DSP of the end of a dma transfer.
		 * FIXME: only set up for reads from dma
		 */
		s1 = splhigh();
		*scr2 |= DSP_BLOCK_E_E;
		dsp_interrupt(DSP_INT_DISABLE);
		splx(s1);

		dsp_var.dma_chan.dc_direction = DMACSR_READ;
		dsp_var.state = dmain;

		/*
		 * Set up the host interface for dma, in the appropriate
		 * mode, to the DSP.
		 */
		switch (dsp_var.chan[DSP_SO_CHAN]->mode) {
		case DSP_MODE8:
			regs->icr |= (ICR_INIT|ICR_RREQ|ICR_HM0|ICR_HM1);
			break;
		case DSP_MODE16:
			regs->icr |= (ICR_INIT|ICR_RREQ|ICR_HM1);
			break;
		case DSP_MODE32:
			s1 = splhigh();
			*scr2 |= DSP_UNPACK_E;
			splx(s1);
			regs->icr |= (ICR_INIT|ICR_RREQ|ICR_HM0);
			break;
		}
		
		dsp_var.dma_chan.dc_flags |= DMACHAN_CHAININTR;// chain compls.
		dsp_var.dma_chan.dc_hndlrpri = CALLOUT_PRI_SOFTINT1;
		dsp_var.dma_chan.dc_handler = dsp_dev_loop;
		dma_init(&dsp_var.dma_chan, I_DSP_DMA);

		/*
		 * Start up any pending dma's
		 */
		if (dsp_var.chan[DSP_SO_CHAN]) {
			q = &dsp_var.chan[DSP_SO_CHAN]->ddp_q;
			while (!queue_empty(q)) {
				queue_remove_first(q, ddp, snd_dma_desc_t *,
					link);
				dma_enqueue(&dsp_var.dma_chan, &ddp->hdr);
			}
		}
		for (i = DSP_N_CHAN-1; i >= 0; i--) {
			/*
			 * Make sure that the appropriate protocol
			 * is used for dma.
			 */
			if (dsp_var.chan[i])
				dsp_var.chan[i]->sndq.start
					= dspq_start_simple;
		}
	} else if (   !(new_proto&SND_DSP_PROTO_S_DMA)
		   && (dsp_var.flags&F_MODE_S_DMA) )
	{
		dsp_var.flags &= ~F_MODE_S_DMA;
		dsp_var.state = normal;

		s1 = splhigh();
		*scr2 &= ~(DSP_BLOCK_E_E|DSP_UNPACK_E);
		dsp_interrupt(DSP_INT_ENABLE);
		splx(s1);
		
		/*
		 * Abort any dma.
		 */
		dma_abort(&dsp_var.dma_chan);
		while (dhp = dma_dequeue(&dsp_var.dma_chan, DMADQ_ALL)) {
			ddp = (snd_dma_desc_t *)dhp->dh_drvarg;
	
			dspdbug2(("dsp_dev_newproto dequeue %x\n", dhp));
			if (ddp) {
				if (!dsp_var.dma_chan.dc_queue.dq_head)
					ddp->flag |= SDD_OVERFLOW;
				(*ddp->free)(ddp);
			}
		}
		snd_stream_queue_reset(&dsp_var.chan[DSP_SO_CHAN]->sndq);
		kfree(dsp_var.chan[DSP_SO_CHAN],
			sizeof(*dsp_var.chan[DSP_SO_CHAN]));
		dsp_var.chan[DSP_SO_CHAN] = 0;
	}
	if (new_proto&SND_DSP_PROTO_SOUNDDATA)
		dsp_var.flags |= F_SOUNDDATA;
	else
	/*
	 * Don't shutdown sound in channel for Complex DMA
	 * m.c. 3/19/90
	 */
	if ((new_proto & SND_DSP_PROTO_C_DMA) == 0) {
		dsp_var.flags &= ~F_SOUNDDATA;
		if (dsp_var.chan[DSP_SI_CHAN]) {
			q = &dsp_var.chan[DSP_SI_CHAN]->ddp_q;
			while (!queue_empty(q)) {
				queue_remove_first(q, ddp,
					snd_dma_desc_t *, link);
				if (queue_empty(q))
					ddp->flag |= SDD_OVERFLOW;
				(*ddp->free)(ddp);
			}
			snd_stream_queue_reset(
				&dsp_var.chan[DSP_SI_CHAN]->sndq);
			kfree(dsp_var.chan[DSP_SI_CHAN],
				sizeof(*dsp_var.chan[DSP_SI_CHAN]));
			dsp_var.chan[DSP_SI_CHAN] = 0;
		}
	}
	splx(s);

	if ((new_proto&SND_DSP_PROTO_LINKOUT) && !(dsp_var.flags&F_LINKEDOUT))
	{
		dsp_var.flags |= F_LINKEDOUT;
		snd_link_init(SND_DIR_PLAY);
	}
	if ((new_proto&SND_DSP_PROTO_LINKIN) && !(dsp_var.flags&F_LINKEDIN))
	{
		dsp_var.flags |= F_LINKEDIN;
		snd_link_init(SND_DIR_RECORD);
	}

	if (bmap_chip) {
		if (new_proto&SND_DSP_PROTO_TXD)
			bmap_chip->bm_dsp_txd_int = 1;
		else
			bmap_chip->bm_dsp_txd_int = 0;
	}
	if ( dsp_var.flags & (F_MODE_DSPMSG | F_MODE_DSPERR | F_MODE_C_DMA )
	    || !(dsp_var.flags & F_MODE_RAW))
	  regs->icr |= ICR_RREQ;
	else
	  regs->icr &= ~ICR_RREQ;

	dspdbug2(("dsp_dev_new_proto: (%x->%x)\n", dsp_var.flags, new_proto));
	dspdbug2(("dsp_dev_new_proto: exit: " DSPREGSFMT));
}

/*
 * Hard reset
 */
void dsp_dev_reset_hard(void)
{
	dsp_dev_reset_chip();
	dsp_dev_reset();
}

/*
 * Do a hard reset of the chip.
 */
void dsp_dev_reset_chip(void)
{
	register int s;

	/*
	 * Reset the DSP interface.
	 * CPU revs > 1 must disable the dsp external memory before
	 * doing the reset and re-enable it after the reset.
	 */
	regs->icr = 0;
	s = spldma();
	*scr2 &= ~DSP_SCR2_MASK;
	if (!(machine_type == NeXT_CUBE && board_rev <= 1))
		*scr2 |= DSP_SCR2_MEMEN;
	*scr2 |= DSP_MODE_B;
	*scr2 |= DSP_RESET;
	*scr2 &= ~DSP_MODE_B;
	if (!(machine_type == NeXT_CUBE && board_rev <= 1))
		*scr2 &= ~DSP_SCR2_MEMEN;
	splx(s);
}

/*
 * Field the dsp device interrupt.
 */
static void dsp_dev_intr(void)
{
	register int s;

	/*
	 * Make sure that this is a legitimate device interrupt.
	 */
	if ((!snd_var.dspowner && dsp_var.state == normal) || 
	    (dsp_var.state == dsp_aborted)){
		
		/*
		 * We haven't been opened yet, must be some spurious
		 * interrupt or DSP is in the aborted state.
		 */
		regs->icr &= ~(ICR_TREQ|ICR_RREQ);
		return;
	}

#if 	DEBUG
	{ int i;
	    if ((i = (vm_offset_t)&i - trunc_page(&i)) < 1000) {
	      dspdbug(("dsp_dev_intr: stack getting low (%d bytes)\n", i));
	    }
	}
#endif	DEBUG

	if (regs->icr & ICR_TREQ) {
	    regs->icr &= ~ICR_TREQ;
	    dspdbug2(("dsp_dev_intr: " DSPREGSFMT));

	    if (dsp_var.flags&F_MODE_C_DMA) {
		/*
		 * Schedule any software interrupts at this level
		 * (including dsp_dev_loop()).
		 */
		softint_run(CALLOUT_PRI_DSP);
	    }
	}
	
	if (regs->icr & ICR_RREQ) {
	    /* If rcode == F_AWAITRXDF ran us, we may need to clear RREQ */
	    /* Also, RREQ is set by snd_dspqueue.c when waiting for rdata */
	    if (dsp_var.flags & (F_MODE_DSPMSG|F_MODE_DSPERR|F_MODE_C_DMA)
		|| !(dsp_var.flags & F_MODE_RAW))
	      regs->icr |= ICR_RREQ;
	    else
	      regs->icr &= ~ICR_RREQ;
	}

	/*
	 * If we're in postdma mode, something's gone wrong.  Prevent ourselves
	 * from getting interrupted and quit.
	 */
	if ( dsp_var.state == postdmain )
	{
		dspdbug2(("dsp_dev_intr reset DSP: " DSPREGSFMT));
		dsp_dev_reset_chip();
	}
	if ((regs->isr&(ISR_HF2|ISR_HF3)) == (ISR_HF2|ISR_HF3) 
	    && (dsp_var.flags & F_MODE_HFABORT) )
	{
		dspdbug2(("DSP ABORTED: " DSPREGSFMT));
		dsp_var.state = dsp_aborted;
		printf("DSP ABORTED\n");
	}

	if (regs->isr&ISR_DMA) {
		/*
		 * If we're still in dma mode we must make sure
		 * that we won't receive any device interrupts until
		 * the dma is done or it's specifically requested from
		 * a software interrupt.
		 */
		dsp_interrupt(DSP_INT_DISABLE);
	}

	if ((regs->isr&(ISR_RXDF|ISR_TXDE)) || dspq_check())
		dsp_dev_loop();
}

/*
 * Clear dma mode so we can get a dsp dev intr.
 */
static void dsp_dma_intr(void)
{
	register int i;

	ASSERT(dsp_var.flags&F_MODE_C_DMA);
	/*
	 * Wait for any data being written to the dsp gets
	 * through the pipe before init'ing the interface.
	 */
	if (regs->icr&ICR_TREQ) { /* If writing TO the DSP */

	    for (i = 10; i && !(regs->isr&ISR_TRDY); i--)
	      DELAY(1);

	    if (i == 0) {
		printf ("DSP DMA write truncated by early complete interrupt!\n");
		dspdbug(("dsp_dma_intr: WRITE TRUNCATED BY DMA COMPLETE!\n"));
	    }
	}

	/*
	 * Set up the dsp interface in the
	 * appropriate interface mode.
	 */
	regs->icr &= ~(ICR_HM0|ICR_HM1|ICR_TREQ|ICR_RREQ);
	regs->icr |= ICR_INIT;
	while (regs->icr&ICR_INIT)
		DELAY(1);
	regs->icr |= ICR_RREQ;		/* Complex DMA must listen to DSP */
	regs->icr |= ICR_TREQ; 		/* Force dsp_dev_intr() */
	dsp_interrupt(DSP_INT_ENABLE);	/* enable device interrupts */
}

/*
 * Main event loop run on interrupt or head of message queue changed.
 */
void dsp_dev_loop(void)
{
	register struct chan_data *cdp;
	int s, bits, dsp_req_chan, size;
	static int dev_loop_ipl = -1;
	int old_ipl;

	if (dsp_var.state == dsp_aborted) return;

	if (dev_loop_ipl == curipl()) {
		dspdbug(("dsp_dev_loop: old ipl %d, cur %d exit\n",
			dev_loop_ipl, curipl()));
		return;
	}

	ASSERT(dev_loop_ipl < curipl());
	old_ipl = dev_loop_ipl;
	dev_loop_ipl = curipl();

	s = spldsp();
	dspdbug2(("dsp_dev_loop enter: " DSPREGSFMT));
	
	/*
	 * Check for completed dma's
	 */
	if (dsp_var.flags&F_MODE_C_DMA) {
	    if (   (dsp_var.state == dmaout || dsp_var.state == dmain)
		&& !(dsp_var.dma_chan.dc_current))
	    {
		int i;
		int oldstate = dsp_var.state;
		cdp = dsp_var.chan[dsp_var.pend_dma_chan];
		ASSERT(cdp);
		size = cdp->buf_size * mode_size(cdp->mode);
		dsp_dma_cleanup(size, dsp_var.pend_dma_chan, cdp->mode,
				dsp_var.state == dmain
				? DMACSR_READ : DMACSR_WRITE);
		/*
		 * Completion might be enqueued, so don't change
		 * states here.
		 */
		for (i = 20; i && dsp_var.state != normal; i--) {
		    dspdbug(("dsp_dev_loop: dma not complete\n"));
		    dspq_execute();
		}
		
		if (dsp_var.state == normal) {
		    dspdbug2(("dsp_dev_loop: %s successful.\n",
			      oldstate == dmain ? 
			      "R_REQ" : "W_REQ"));
		    regs->icr |= ICR_RREQ; /* CDMA mode */
		    dspdbug2(("dsp_dev_loop: RREQ set for CDMA mode:" 
			      DSPREGSFMT));
		}
	    }
	} else if (dsp_var.flags&F_MODE_S_DMA) {
	    register snd_dma_desc_t *ddp;
	    register struct dma_hdr *dhp;
	    
	    /*
	     * Complete this buffer, causing the next one to be enqueued.
	     */
	    while (dhp = dma_dequeue(&dsp_var.dma_chan, DMADQ_DONE)) {
		dspdmadbug(("dsp_dev_loop: complete ddp 0x%x\n", ddp));
		ddp = (snd_dma_desc_t *)dhp->dh_drvarg;
		dsp_var.chan[DSP_SO_CHAN]->sndq.nxfer += ddp->size;
		if (!dsp_var.dma_chan.dc_queue.dq_head)
		  ddp->flag |= SDD_OVERFLOW;
		(*ddp->free)(ddp);
	    }
	} else if (dsp_var.flags&(F_MODE_DSPMSG|F_MODE_DSPERR)
		   || !(dsp_var.flags & F_MODE_RAW))
	  regs->icr |= ICR_RREQ; /* set in DSPMSG,DSPERR or CDMA mode */
	
	if (dsp_var.ui_dma_read_state == UIR_PENDING) {
/*
 * True when a user-initiated DMA read is pending.
 * These are implemented as a DSP-initiated read on
 * channel DSP_USER_REQ_CHAN.  The UIR_PENDING bit set in case dma_in: of
 * snd_dspqueue.c:dspq_execute().  If a region to record into does not exist
 * on channel DSP_USER_REQ_CHAN (==0), we wait.  When the
 * region comes in, we init the DMA and set dsp_var.ui_dma_read_state |=
 * UIR_INITED.  Finally, the DSP advances us into state penddmain1 with a
 * DSP_dm_R_REQ message for channel 0.  Note that the DSP can request a DMA
 * read on any channel greater than 0 anytime up until state penddmain1.
 */
	    cdp = dsp_var.chan[dsp_var.pend_dma_chan];
	    ASSERT(cdp);
	    if (queue_empty(&cdp->ddp_q)) { /* no region for channel 0 */
		dspdmadbug(("dsp_dev_loop: UI read pending. await buf\n"));
	    } else {
		dspdmadbug(("dsp_dev_loop: dmain addr %x\n",
			    dsp_var.pend_dma_addr));
		dsp_dma_init(dsp_var.pend_dma_addr, cdp->skip,
			     cdp->space, cdp->mode, DMACSR_READ);
		/* We cannot go to state penddmain1.
		   The DSP initiates the transition into that state
		   with the "DSP message" DSP_dm_R_REQ+DSP_USER_REQ_CHAN */
		dsp_var.ui_dma_read_state |= UIR_INITED;
		dspdbug2(("dsp_dev_loop: UI read pending: dma inited\n"));
	    }
	}


	/*
	 * Loop sending and receiving messages.
	 */
	while(
	      ( (regs->isr&ISR_RXDF)
	       && ((dsp_var.flags & (F_MODE_DSPMSG|F_MODE_DSPERR|F_MODE_C_DMA))
		   || !(dsp_var.flags & F_MODE_RAW))
	       && dsp_var.msgp
	       && dsp_var.msgp < dsp_var.emsgbuf
	       && (dsp_var.errp == 0 ? 1 : (dsp_var.errp<dsp_var.eerrbuf))
	       && dsp_var.state != dmain
	       && dsp_var.state != dmaout
	       && dsp_var.state != penddmain1
	       && dsp_var.state != postdmain
	      )
	    || (dsp_var.state == penddmain1)
	    || (dsp_var.state == penddmaout0)
	    || dsp_var.state == penddmaout1
	    || (   (   !queue_empty(&dsp_var.cmd_q)
		    || (   dsp_var.flags&F_SOUNDDATA
			&& dsp_var.chan[DSP_SI_CHAN]
		        && !queue_empty(&dsp_var.chan[DSP_SI_CHAN]->ddp_q)))
		&& dspq_check()))
	{
	    /*
	     * Run everything that can be done on the queue.
	     */
	    if (   !queue_empty(&dsp_var.cmd_q)
		|| (   dsp_var.flags&F_SOUNDDATA
		    && dsp_var.chan[DSP_SI_CHAN]
		    && !queue_empty(&dsp_var.chan[DSP_SI_CHAN]->ddp_q)))
	    {
		dspdbug2(("dsp_dev_loop: execute queue\n"));
		dspq_execute();
	    }
	    
	    if (dsp_var.flags&F_MODE_C_DMA) {
		/*
		 * Dma state machine.
		 */
		cdp = dsp_var.chan[dsp_var.pend_dma_chan];
		bits = dsp_var.pend_dma_chan;
		switch (dsp_var.state) {
		case penddmain1:
		    ASSERT(cdp);
		    /*
		     * See if we can go from pending dmain to dmain
		     * states.  If not, we'll get there when a region
		     * arives on our queue.  (For channel 0, the user
		     * initiated channel, we will always have a region.)
		     */
		    dspdmadbug(("dsp_dev_loop: penddmain: channel %d "
				"cdp 0x%x bits 0x%x\n",dsp_var.pend_dma_chan,
				cdp,bits));
		    if (queue_empty(&cdp->ddp_q)) {
			dspdmadbug(("dsp_dev_loop: penddmain1 await buf\n"));
			ASSERT(dsp_var.pend_dma_chan != 0);
			goto break_devloop;
		    }
		    /*** FIXME: Change cdp->buf_size to BYTES instead of
		      words so that it can be PADDED for 24-bit mode! 
		      So far, only snd_*.c files (driver), snddriver
		      functions, and libdsp functions would have to
		      change internally to accommodate this. ***/
		    dsp_dma(cdp->buf_size*mode_size(cdp->mode),
			    dsp_var.pend_dma_chan, cdp->mode, DMACSR_READ);
		    if (dsp_var.pend_dma_chan == DSP_USER_REQ_CHAN)
		      dsp_var.ui_dma_read_state = 0;
		    dsp_var.state = dmain;
		    dspdbug2(("dsp_dev_loop: penddmain1 -> dmain\n"));
		    continue;
		    
		case penddmaout0:
		    ASSERT(cdp);
		    if (queue_empty(&cdp->ddp_q)) {
			dspdmadbug(("dsp_dev_loop: penddmaout0 await buf\n"));
			goto break_devloop;
		    }
		    dspdmadbug(("dsp_dev_loop: dmaout addr %x\n",
				dsp_var.pend_dma_addr));
		    dspdmadbug(("dsp_dev_loop: first 4 ints %x %x %x %x\n",
				* (int *) dsp_var.pend_dma_addr,
				* ((int *) dsp_var.pend_dma_addr+1),
				* ((int *) dsp_var.pend_dma_addr+2),
				* ((int *) dsp_var.pend_dma_addr+3)));
		    dsp_var.state = penddmaout1;
		    dsp_dma_init(dsp_var.pend_dma_addr, cdp->skip,
				 cdp->space, cdp->mode, DMACSR_WRITE);
		    dspdbug2(("dsp_dev_loop: -> penddmaout1\n"));
		    continue;
		    
		case penddmaout1:
		    ASSERT(cdp);
		    
		    dsp_dma(cdp->buf_size*mode_size(cdp->mode),
			    dsp_var.pend_dma_chan, cdp->mode,
			    DMACSR_WRITE);
		    dsp_var.state = dmaout;
		    dspdbug2(("dsp_dev_loop: pend -> dmaout\n"));
		    continue;
		}
	    }		/* if (dsp_var.flags&F_MODE_C_DMA) {} */
	    
	    
	    /*
	     * Now deal with input from the dsp.
	     */
	    if ((regs->isr&ISR_RXDF)
		&& ((dsp_var.flags &
		     (F_MODE_DSPMSG|F_MODE_DSPERR|F_MODE_C_DMA))
		    || !(dsp_var.flags & F_MODE_RAW))
		&& dsp_var.msgp 
		&& dsp_var.msgp < dsp_var.emsgbuf
		&& (dsp_var.errp==0?1:(dsp_var.errp<dsp_var.eerrbuf))
		&& dsp_var.state != penddmain1
		&& dsp_var.state != dmain
		&& dsp_var.state != dmaout
		&& dsp_var.state != postdmain)
	    {
		/*
		 * 68040 must read as bytes.
		 */
		if (cpu_type != MC68030)
		  bits = (regs->rxh << 16) | (regs->rxm << 8) |
		    regs->rxl;
		else
		  bits = regs->receive;
		dspdbug2(("dsp_dev_loop RXDF loop: rcvd %x\n", bits));
			
		dsp_req_chan = bits & DSP_CHANMASK;
		
		/*
		 * See if these are special messages from the DSP (if
		 * we're in the right mode).
		 */
		switch (bits&DSP_OPCODEMASK) {
		case DSP_dm_TMQ_LWM:
		    if (!(dsp_var.flags&F_MODE_DSPMSG))
		      break;
		    continue;	/* eat this message */
		case DSP_dm_KERNEL_ACK:
		    if ((dsp_var.flags & (F_MODE_DSPMSG|F_AWAITHC))
			== (F_MODE_DSPMSG|F_AWAITHC))
		    {
			dsp_var.flags &= ~F_AWAITHC;
			continue;
		    }
		    break;
		case DSP_dm_R_REQ:
		    if (!(dsp_var.flags&F_MODE_C_DMA))
		      break;
		    /*
		     * An R_REQ on user-req channel is treated
		     * as normal data if we are not in the process
		     * of doing a user-initiated read.
		     */
		    if ((dsp_req_chan == DSP_USER_REQ_CHAN) 
			&& !(dsp_var.ui_dma_read_state & UIR_INITED))
		      break; /* read as a "DSP message" */
		    if ((dsp_var.state != normal)
			|| (u_int)(dsp_req_chan) >DSP_N_CHAN-1
			|| !dsp_var.chan[dsp_req_chan])
		    {
			/*
			 * Protocol error, do something.
			 */
			dspdbug(("dsp_dev_loop: recieved R_REQ"
				 " and not in normal"
				 " state or bad chan\n"));
			break;
		    }
		    dspdmadbug(("dsp_dev_loop: R_REQ chan %d."
				" ->penddmain1\n", 
				(dsp_req_chan)));
		    dsp_var.pend_dma_chan = dsp_req_chan;
		    dsp_var.state = penddmain1;
		    
		    /*
		     * Pick up the rest of the set-up above.
		     */
		    continue;
		case DSP_dm_W_REQ:
		    if (!(dsp_var.flags&F_MODE_C_DMA))
		      break;
		    if (   dsp_var.state != normal
			|| (u_int)(dsp_req_chan) >DSP_N_CHAN-1
			|| !dsp_var.chan[dsp_req_chan])
		    {
			/*
			 * Protocol error, do something.
			 */
			dspdbug(("dsp_dev_loop: recieved W_REQ"
				 " and not in normal"
				 " state or bad chan\n"));
			break;
		    }
		    
		    dspdmadbug(("dsp_dev_loop: W_REQ chan %d. ->penddmaout0\n",
				dsp_req_chan));
		    dsp_var.pend_dma_chan = dsp_req_chan;
		    cdp = dsp_var.chan[dsp_var.pend_dma_chan];
		    ASSERT(cdp);
		    
		    /*
		     * See if we can satisfy this request.
		     */
		    dsp_var.state = penddmaout0;
		    dsp_var.pend_dma_addr = 0;
		    continue;
		} /* switch (bits&DSP_OPCODEMASK) */
		
		/*
		 * Add this message to the (apropriate) output queue
		 * and send the data up to the user if he's requested
		 * it.
		 */
		if ((dsp_var.flags&F_MODE_DSPERR)
		    && (bits&DSP_OPERROR)
		    && dsp_var.errp)
		{
		    *dsp_var.errp++ = bits;
		    if (dsp_var.err_port) {
			dspdbug(("dsp_dev_loop: reply err\n"))
			  softint_sched(CALLOUT_PRI_THREAD,
					snd_reply_dsp_err,
					dsp_var.err_port);
			dsp_var.err_port = PORT_NULL;
		    }
		} else {
		    /* 
		     * Send word from DSP to user as a "DSP message".
		     * We should only be here if the F_MODE_DSPMSG bit is set,
		     * but we'll also land here if F_MODE_{DSPERR|C_DMA} !=0,
		     * and an unrecognized DSP error (high-order bit not set),
		     * or unrecognized DMA request is received.
		     * Thus, {DSPERR|C_DMA} => {DSPMSG minus TMQ_LWM and 
		     * KERN_ACK recognition}.
		     */
		    *dsp_var.msgp++ = bits;
		    if (dsp_var.msg_port) {
			dspdbug(("dsp_dev_loop: reply msg\n"))
			  softint_sched(CALLOUT_PRI_THREAD,
					snd_reply_dsp_msg, 
					dsp_var.msg_port);
			dsp_var.msg_port = PORT_NULL;
		    }
		}
	    } /* end input from the DSP */
	    if (   (dsp_var.msgp && dsp_var.msgp == dsp_var.emsgbuf)
		|| (dsp_var.errp && dsp_var.errp == dsp_var.eerrbuf))
	    {
		dspdbug2(("dsp_dev_loop: no bufs -> disable RREQ\n"));
		regs->icr &= ~ICR_RREQ;
	    }
	} /* end loop sending and receiving messages */
    break_devloop:

	/*
	 * See if we need to set anything to get called when the
	 * appropriate condition arises.
	 */
	if (dsp_var.state == normal) {

	    int rcode = dspq_awaited_conditions();

	    if (rcode & F_AWAITHC)
	      rcode |= F_AWAITBIT;
	    /* The previous strategy
	     *  if (dsp_var.flags&F_MODE_DSPMSG) regs->cvr=(CVR_HC|DSP_hc_ACK);
	     * overwrites the user's pending host command!  To make this idea
	     * work, we have to read back the CVR and remember the user's 
	     * pending host command, and reissue it later.  Of course, then
	     * you have the race where it gets in before you can overwrite it
	     * with DSP_hc_ACK. Then the user sees it happening twice. For now,
	     * let's just use the AWAITBIT strategy which is already important
	     * because we definitely see deadlocks resulting from not setting
	     * a timer to run us periodically while awaiting some bit.
	     */

	    if ((rcode & F_AWAITRXDF) && !(regs->isr & ISR_RXDF)) {
		dspdbug2(("dsp_dev_loop: setting RREQ to await RXDF\n"));
		regs->icr |= ICR_RREQ; /* dsp_dev_intr() will clear RREQ */
	    }

	    if ((rcode & F_AWAITTXDE) && !(regs->isr & ISR_TXDE)) {
		if (old_ipl<spldsp()) {
		    dspdbug2(("dsp_dev_loop: setting TREQ for AWAITTXDE\n"));
		    regs->icr |= ICR_TREQ; /* dsp_dev_intr() will clear TREQ */
		} else {
		    dspdbug2(("dsp_dev_loop: "
			      "CANNOT set TREQ for AWAITTXDE\n"));
		}
	    }
	} else if (dsp_var.state == penddmaout0 || dsp_var.state == penddmain1)
	{
		/*
		 * Waiting for a region to be enqueued for DMA input or output
		 */
		regs->icr &= ~ICR_RREQ; /* cannot read DSP now */
		regs->icr &= ~ICR_TREQ; /* no need to run dev_loop yet */
	}

	/*
	 * If we don't have any room left in the buffers, don't look for
	 * new input from the DSP.
	 */
	if (   (dsp_var.msgp && dsp_var.msgp == dsp_var.emsgbuf)
	    || (dsp_var.errp && dsp_var.errp == dsp_var.eerrbuf))
	{
		regs->icr &= ~ICR_RREQ;
		dspdbug(("dsp_dev_loop: no bufs: disable RREQ\n"));
	}
			
	splx(s);
	if (old_ipl != -1)
	  dspdbug(("dsp_dev_loop: exiting to old ipl %d\n",old_ipl));
	dspdbug2(("dsp_dev_loop: exit: " DSPREGSFMT));
	dev_loop_ipl = old_ipl;
}

/*
 * DMA out to dsp assumptions:
 *	All requests are equal to the buffer size on the DSP. 
 */
static void dsp_dma_init(int address, int skip, int space, int mode,
	int direction)
{
	register int cnt, s;
	int cmd = 0;

	/*
	 * Push on argmuents
	 */
	dspdmadbug(("dsp_dma_init dir %s: space %d, address 0x%x skip %d\n",
	    (direction==DMACSR_WRITE?"write":"read"), space, address, skip));
	
	ASSERT((dsp_var.state == normal 
		&& dsp_var.ui_dma_read_state == UIR_PENDING)
	       || dsp_var.state == penddmaout1);

/*
 *	DSP-Initiated reads do not require the syscall host command
 */
	if ((direction == DMACSR_READ) && (skip /* chan */ != 0)) return;

	s = spldsp();
	
	/*
	 * Send down a host command indicating the type of dma to
	 * be done.
	 *
	 * DMA chips <= 13 have a problem with DMA writes to the DSP:
	 * the first chunk (16 bytes == 4 words) needs to be thrown
	 * away, thus the special host command.
	 */
	/*
	 * Set command to chan
	 */
	if (address == 0 && space == 4)
		cmd = skip;
	cmd &= DSP_SYSCALL_m_CHANMASK;
	if (direction == DMACSR_READ) cmd |= DSP_SYSCALL_READ;
	else {
		cmd |= DSP_SYSCALL_WRITE;
		if (   (mode == DSP_MODE32 && dma_chip <= 313)
#if	DEBUG
		    && (dspdbgflag&0x100)
#endif	DEBUG
		)
		    cmd |= DSP_SYSCALL_m_SWFIX;
	}
	dsp_var.flags |= F_NOQUEUE;

	/*
	 * Send the command, then wait for HC to clear.
	 * We do not need to wait for HF2 because the SYSCALL
	 * host command is processed at the highest "HOST IPL" 
	 * within the DSP which means no future writes to TX can
	 * get in until the host command is fully processed.
	 */
	dspq_enqueue_syscall(cmd);
	dspq_enqueue_cond((CVR_HC<<16), 0);

	dsp_var.flags &= ~F_NOQUEUE;
	dspq_execute();
	splx(s);
}

static void dsp_dma(register int size, int channel, int mode, int direction)
{
	register int i, n, s;
	int preinit = (direction==DMACSR_WRITE ? ICR_TREQ : ICR_RREQ)
			| ((regs->icr)&(ICR_HF1|ICR_HF0));
	int init = preinit | ICR_INIT;
	register snd_dma_desc_t *ddp;
	register struct chan_data *cdp = dsp_var.chan[channel];

	dspdmadbug(("dsp_dma dir %s: size %d, chan %d mode %d\n",
	    (direction==DMACSR_WRITE?"write":"read"), size, channel, mode));

	ASSERT(cdp);
	ASSERT(   dsp_var.state
	       == (direction == DMACSR_WRITE ? penddmaout1 : penddmain1));


	regs->icr = 0;		/* Blammo */

		
	/*
	 * Get the dma descriptor.
	 */
	ASSERT(!queue_empty(&cdp->ddp_q));
	queue_remove_first(&cdp->ddp_q, ddp, snd_dma_desc_t *, link);
	dsp_var.dma_chan.dc_direction = direction;
	ddp->hdr.dh_flags = DMADH_INITBUF;

	s = spldsp();
	/*
	 * set up the dsp interface in the appropriate dma mode.
	 */
	switch (mode) {
	case DSP_MODE8:
		regs->txh = 0;		/* Clear out 2 high bytes */
		regs->txm = 0;		/* Clear out 2 high bytes */
		regs->icr = (preinit|ICR_HM0|ICR_HM1);
		regs->icr = (init|ICR_HM0|ICR_HM1);
		break;
	case DSP_MODE16:
		regs->txh = 0;		/* Clear out high byte */
		regs->icr = (preinit|ICR_HM1);
		regs->icr = (init|ICR_HM1);
		break;
	case DSP_MODE24:
		i = splhigh();
		*scr2 &= ~DSP_UNPACK_E;
		splx(i);
		regs->icr = (preinit|ICR_HM0);
		regs->icr = (init|ICR_HM0);
		break;
	case DSP_MODE32:	/* This mode broken in hardware */
		i = splhigh();
		*scr2 |= DSP_UNPACK_E;
		splx(i);
		regs->icr = (preinit|ICR_HM0);
		regs->icr = (init|ICR_HM0);
		if (dma_chip <= 313)
		  printf("Attempt to use broken 32-bit unpacked DSP DMA\n");
		break;
        default:
		dspdmadbug(("UNKNOWN CASE!, mode = %d\n", mode));
	}

	dspdbug2(("dsp_dma after setup: " DSPREGSFMT));
	/* disable dsp interrupts */
	dsp_interrupt(DSP_INT_DISABLE);
	spldsp();
	/*
	 * enqueue the dma operation
	 */
	dma_enqueue(&dsp_var.dma_chan, &ddp->hdr);

	regs->icr |= ICR_HF1;	/* Tell the dsp we're ready */

	splx(s);
}

static void dsp_dma_cleanup(register int size, int channel, int mode,
	int direction)
{
	register struct dma_hdr *dhp;
	register snd_dma_desc_t *ddp;
	register int n, s;

	dspdmadbug(("dsp_dma_cleanup dir %s: size %d, chan %d, mode %d\n",
		direction==DMACSR_WRITE?"write":"read", size, channel, mode));

	ASSERT(dsp_var.state == (direction == DMACSR_WRITE ? dmaout : dmain));
	ASSERT(dsp_var.dma_chan.dc_current == NULL);

	dhp = dma_dequeue(&dsp_var.dma_chan, DMADQ_DONE);
	ASSERT(dhp && dhp->dh_drvarg);
	ddp = (snd_dma_desc_t *)dhp->dh_drvarg;
	dsp_var.chan[channel]->sndq.nxfer += ddp->size;
	dspdbug2(("dsp_dma_cleanup dequeue %x\n", dhp));
	
	/*
	 * Put the DSP back into normal operating mode.
	 */
	if (direction == DMACSR_WRITE) {
	        /* clear dma mode, HF1 & TREQ*/
		regs->icr &= ~(ICR_HM0|ICR_HM1|ICR_HF1|ICR_TREQ);

#if	DEBUG
		/*** FIXME: This is done in dma_intr() and should not be
		  needed again here (unless TREQ got cleared somehow) ***/
		/*
		 * Wait for any data to be fully absorbed by the DSP.
		 */
		for (  n = 100
		     ; n && !(regs->isr&ISR_TRDY)
		     ; n--)
			DELAY(1);

		if (n != 100) {
		    printf("dsp_dma_cleanup: TREQ GOT CLEARED DURING DMA!\n");
		    dspdbug(("dsp_dma_cleanup: "
			     "TREQ GOT CLEARED DURING DMA!\n"));
		}

		if (n == 0) {
		    printf("dsp_dma_cleanup: write to DSP looks hung!\n");
		    dspdbug(("dsp_dma_cleanup: write to DSP looks hung!\n"));
		}

		dspdbug(("dsp_dma_cleanup: DSP_hc_HOST_WD host command\n"));
#endif	DEBUG

		regs->cvr = DSP_hc_HOST_WD|CVR_HC; /* Tell DSP write is done */

		/*
		 * Wait for the DSP to complete the host command.
		 * We do not need to wait for HF2 because the HOST_WD
		 * host command is processed at the highest "HOST IPL" 
		 * within the DSP which means no future writes to TX can
		 * get in until the host command is fully processed.
		 */
		for (  n = 100
		     ; n && (regs->cvr&CVR_HC)
		     ; n--)
			DELAY(1);

		if (n == 0) {
		    printf("dsp_dma_cleanup: DSP_hc_HOST_WD looks hung!\n");
		    dspdbug(("dsp_dma_cleanup: DSP_hc_HOST_WD looks hung!\n"));
		}

		dsp_var.state = normal;

		regs->icr |= ICR_RREQ; /* We are in complex DMA mode */

		dspdbug2(("dsp_dma_cleanup: "
			  "INIT DSP: Set RREQ for CDMA mode: " DSPREGSFMT));
	} else {
		/*
		 * Let the dsp know we're done with `dma'.
		 */
		dsp_var.flags |= F_NOQUEUE;
		dsp_var.state = postdmain;
		dspq_enqueue_cond((CVR_HC<<16), 0);
		dspq_enqueue_hc(DSP_hc_HOST_RD);
		dspq_enqueue_cond((CVR_HC<<16), 0);
		dspq_enqueue_hf(
			(ICR_HM0|ICR_HM1|ICR_HF1|ICR_TREQ|ICR_RREQ)<<24,
			(ICR_INIT|ICR_RREQ)<<24); /* Complex DMA mode */
		dspq_enqueue_state(normal);
		dsp_var.flags &= ~F_NOQUEUE;
		dspdbug2(("dsp_dma_cleanup: "
			  "Enqueued HOST_RD host cmd "
			  "and normal state. xct Q: " DSPREGSFMT));
		dspq_execute();
	}

	if (queue_empty(&dsp_var.chan[channel]->ddp_q)) {
		ddp->flag |= SDD_OVERFLOW;
		dspdbug(("dsp_dma_cleanup: no more regions\n", dhp));
	}
	(*ddp->free)(ddp);
	dspdbug2(("dsp_dma_cleanup: free(ddp) done. exit: " DSPREGSFMT));
}

/*
 * Check to see if there's a device there (of course there is!)
 */
static int dsp_probe(caddr_t addr, register int unit)
{
	register int s, result;

	if (unit != 0) return (0);

	s = splhigh();
	/*
	 * See if the dsp is really there by trying to initialize
	 * and setting treq.  One of two interrupt lines should go
	 * high if it's there.
	 */
#if	DEBUG
	regs = (struct dsp_regs *)P_DSP;
#endif	DEBUG

	dsp_dev_reset_chip();
	dsp_interrupt(DSP_INT_ENABLE);		/* enable device interrupts */

	regs->icr = ICR_INIT;
	while (regs->icr&ICR_INIT)
		DELAY(1);
	regs->icr = ICR_TREQ;
	result = *intrstat & I_BIT(I_DSP_4);
#if	DEBUG
	if (!result) {
		printf("DSP not interrupting: intrstat 0x%x, mask 0x%x\n",
			*intrstat, I_BIT(I_DSP_4));
		result = 1;
	}
#endif	DEBUG
	regs->icr = 0;

	while (*intrstat&I_BIT(I_DSP_4)) {
#if	DEBUG
		printf("DSP still interrupting: 0x%x\n", *intrstat);
#endif	DEBUG
		DELAY(100);
	}

	splx(s);
	return(result? (int)regs : 0);
}

/*
 * Attach to this device.
 */
static int dsp_attach(register struct bus_device *bd)
{
	register struct dma_chan *dma = &dsp_var.dma_chan;
	register int i, s;

	install_scanned_intr (I_DSP_4, (func)dsp_dev_intr, 0);

	return(0);
}

/*
 * Enable/disable DSP interrupt
 */
static void dsp_interrupt(enum dsp_int_en dsp_int_en)
{
	register int s;
	
	s = splhigh();
	if (dsp_int_en == DSP_INT_ENABLE) {
		*scr2 |= DSP_SCR2_INTREN;
		if (bmap_chip)
			bmap_chip->bm_dsp_hreq_int = 1;
	} else {
		*scr2 &= ~DSP_SCR2_INTREN;
		if (bmap_chip)
			bmap_chip->bm_dsp_hreq_int = 0;
	}
	splx(s);
}
#endif	NDSP > 0
