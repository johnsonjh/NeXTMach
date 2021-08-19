/*	@(#)dma.c	1.0	08/12/87	(c) 1987 NeXT	*/
#define	CHAIN_READ	0

/* 
 **********************************************************************
 * HISTORY
 * 22-Aug-90  John Seamons (jks) at NeXT
 *	Added direction argument to dma_sync_cache.  It now calls
 *	cache_push_page for writes and cache_inval_page for reads.
 *	This is the same as it was a few versions ago.
 *
 * 01-Mar-90	Doug Mitchell at NeXT
 *	Added dma_close(); kalloc'd dc_tail in dma_init in case channel's
 *		tailbuf not physically contiguous.
 *
 * 20-Jun-89	Mike DeMoney (mike) at NeXT
 *	Removed last vestiges of DMA_HWIPL and DMA_SWIPL.  Changed all
 *	softint_sched's to sched at dc_hndlrpri.
 *
 * 24-May-89	Doug Mitchell at NeXT
 *	Delayed calling install_scanned_intr() until after DMA h/w is 
 *	initialized in dma_init().
 *
 * 25-May-88  John Seamons (jks) at NeXT
 *	Changes to dma_list that support padding DMA transfers before
 *	and after the actual data.
 *
 * 12-Apr-88  John Seamons (jks) at NeXT
 *	Changes to support DMA chip 313.
 *
 * 12-Aug-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 * FIXME:
 *	DMA chip 313: BUSEXC in csr still seems to be broken.
 *
 **********************************************************************
 */

#import <iplmeas.h>

#import <sys/param.h>
#import <kern/xpr.h>
#import <sys/callout.h>
#import <next/cpu.h>
#import <next/psl.h>
#import <next/autoconf.h>
#import <next/pmap.h>
#import <next/event_meter.h>
#import <nextdev/dma.h>
#import <nextif/if_enreg.h>
#import <machine/spl.h>

/*
 * dma.c -- support routines for NeXT DMA hardware
 */

#if	DEBUG
int	dmadbug;
#define	dbug(dcp, f) { \
	if ((dcp->dc_flags & DMACHAN_DBG) || (dmadbug & 2)) { \
		if (dmadbug) printf f; else XPR(XPR_DMA, f); \
	} \
}
#else	DEBUG
#define	dbug(dcp,f)
#endif	DEBUG

static inline int get_dma_state(volatile struct dma_dev *ddp)
{
	int state;
	
	if (dma_chip == 313) {
		while ((state = ddp->dd_csr) == 0)
			continue;
		state &= (DMASTATE_MASK & ~DMACSR_BUSEXC);
	} else
		state = ddp->dd_csr & DMASTATE_MASK;
	return state;
}

/*
 * dma_init -- initialize dma channel hardware
 * dma_chan struct must be initialized before calling
 */
void
dma_init(dcp, device)
register struct dma_chan *dcp;
unsigned device;
{
	register volatile struct dma_dev *ddp;
	vm_offset_t cp;

	/*
	 * install_scanned_intr() establishes interrupt linkage between 
	 * the indicated "device" and a handler/argument pair.
	 * "device" is the bit index of the corresponding
	 * mask bit in the interrupt mask register (see next/cpu.h).
	 */
	dcp->dc_flags &=~ (DMACHAN_BUSY | DMACHAN_CHAINING | DMACHAN_ERROR);
	ddp = dcp->dc_ddp;
	ddp->dd_csr = DMACMD_RESET|DMACMD_INITBUF;
	dcp->dc_tail = (char *)roundup((u_int)dcp->dc_tailbuf,
				       DMA_ENDALIGNMENT);
	dcp->dc_ptail = pmap_resident_extract(pmap_kernel(), dcp->dc_tail);
	install_scanned_intr (device, (func)dma_intr, dcp);
	if (pmap_resident_extract(pmap_kernel(), dcp->dc_tail + DMA_MAXTAIL)
	    != dcp->dc_ptail + DMA_MAXTAIL) {
	    	/*
		 * Should only happen to loadable devices...Get some
		 * contiguous memory.
		 */
	    	dcp->dc_tail = (char *)kalloc(DMA_MAXTAIL);
		dcp->dc_ptail = pmap_resident_extract(pmap_kernel(),
			dcp->dc_tail);
	}
	dcp->dc_current = NULL;
}

void
dma_close(dcp)
register struct dma_chan *dcp;
{
	/*
	 * Called by loadable device drivers when they are being unloaded.
	 * We have to check if we kalloc'd the tailbuf in dma_init; if 
	 * so, free it.
	 */
	if((dcp->dc_tail < dcp->dc_tailbuf) || 
	   (dcp->dc_tail >= (dcp->dc_tailbuf + DMA_MAXTAIL)))
		kfree(dcp->dc_tail, DMA_MAXTAIL);
}

/*
 * dma_enqueue -- enqueue dma_hdr on tail of channel queue
 */
void
dma_enqueue(dcp, dhp)
register struct dma_chan *dcp;
register struct dma_hdr *dhp;
{
	register int s;

	dhp->dh_state = 0;
	dhp->dh_link = NULL;

	s = spldma();
	if (dcp->dc_queue.dq_head == NULL) {	/* empty queue */
		dcp->dc_queue.dq_head = dcp->dc_queue.dq_tail = dhp;
dbug (dcp, ("enq: dcp %x dhp %x empty flags %x\n", dcp, dhp, dcp->dc_flags));
	} else {				/* enqueue on tail */
		dcp->dc_queue.dq_tail->dh_link = dhp;
		dcp->dc_queue.dq_tail = dhp;
dbug (dcp, ("enq: dcp %x dhp %x tail flags %x\n", dcp, dhp, dcp->dc_flags));
	}
	if ((dcp->dc_flags & (DMACHAN_AUTOSTART|DMACHAN_BUSY|DMACHAN_ERROR))
	    == DMACHAN_AUTOSTART)
		dma_start(dcp, dhp, dcp->dc_direction);
	splx(s);
}

/*
 * dma_dequeue -- dequeue completed dma_hdr's from
 *    head of channel queue.
 * if doneflag is DMADQ_DONE, only dequeue buffers which have
 *   completed their dma (dh_state != 0)
 * if doneflag is DMADQ_ALL, DMA MUST BE DISABLED
 *   and all buffers are dequeued
 */
struct dma_hdr *
dma_dequeue(dcp, doneflag)
register struct dma_chan *dcp;
{
	register int s;
	register struct dma_hdr *dhp;

	/*
	 * Its only legal to pickup "undone" buffers if dma is disabled
	 */
	ASSERT(doneflag == DMADQ_DONE || (dcp->dc_flags & DMACHAN_BUSY) == 0);

	s = spldma();
	if ((dhp = dcp->dc_queue.dq_head) == NULL	/* empty queue */
	    || (doneflag == DMADQ_DONE && dhp->dh_state == 0)) {
		splx(s);
dbug (dcp, ("deq: dcp %x NULL\n", dcp));
		return(NULL);
	}
	dcp->dc_queue.dq_head = dhp->dh_link;
	splx(s);
dbug (dcp, ("deq: dcp %x dhp %x\n", dcp, dhp));
	return(dhp);
}

/*
 * Given a virtual address and count, costruct a DMA chain list by
 * looking for contiguous groups of physical pages.  No chain is
 * made larger than the DMA hardware can handle.
 *
 * SHOULD ONLY BE USED FOR LASER PRINTER, DISK, SCSI, and DSP DMA
 */
void
dma_list(dcp, fdhp, vaddr, bcount, pmap, direction, ndmahdr,
	before, after, secsize, rathole_va)
struct dma_chan *dcp;
dma_list_t fdhp;
vm_offset_t vaddr, rathole_va;
long bcount, secsize;
pmap_t pmap;
int direction, before, after;
{
	struct dma_hdr *dhp;
	caddr_t paddr, start, stop;
	unsigned offset;
	long rem, sect_rem;
	int during, sectchain = dcp->dc_flags & DMACHAN_SECTCHAIN;
	vm_offset_t va, during_va;

	if (! DMA_BEGINALIGNED(vaddr))
		panic("dma_list: bad alignment");
	during = bcount - before - after;
	during_va = vaddr;
	sect_rem = secsize;
	for (dhp = fdhp; bcount > 0; dhp++) {
		if (before || (after && (during == 0)))

			/* FIXME: should probably DMA zeros instead */
			va = vaddr & ~NeXT_page_mask;
		else
			va = during_va;
		if (sectchain)
			va = rathole_va;
		start = (caddr_t)pmap_resident_extract(pmap, va);
		if (start == 0)
			panic("dma_list: zero pfnum");
		rem = NeXT_page_size - (va & NeXT_page_mask);
		if (before) {
			if (rem > before)
				rem = before;
			if (sectchain && rem > sect_rem)
				rem = sect_rem;
			before -= rem;
		} else
		if (during) {
			if (rem > bcount)
				rem = bcount;
			if (sectchain && rem > sect_rem)
				rem = sect_rem;
			during -= rem;
			during_va += rem;
		} else {
			if (rem > after)
				rem = after;
			if (sectchain && rem > sect_rem)
				rem = sect_rem;
			after -= rem;
		}
		stop = start + rem;
		bcount -= rem;
		sect_rem -= rem;
		dhp->dh_flags = 0;
		if (dhp != fdhp && (dhp-1)->dh_stop == start &&
		    stop - (dhp-1)->dh_start < MAX_DMA_LEN &&
		    ((sectchain && sect_rem != 0) || sectchain == 0)) {
			dhp--;
		} else {
			dhp->dh_start = start;
			if (sectchain && sect_rem == 0) {
				dhp->dh_flags |= DMADH_SECTCHAIN;
				sect_rem = secsize;
			}
		}
		dhp->dh_stop = stop;
		dhp->dh_link = bcount > 0 ? dhp + 1 : NULL;
		dhp->dh_state = 0;
	}
	dcp->dc_taillen = 0;
	dhp--;
	/*
	 * Deal with unaligned endpoints by rounding up transfer
	 * and placing tail in kernel buffer.  Dma_cleanup should
	 * be called after dma is complete to move anything in the
	 * tail to its proper destination.
	 */
	if (! DMA_ENDALIGNED(dhp->dh_stop) || (dcp->dc_flags & DMACHAN_PAD)) {
		/* can't guarantee completion interrupts */
dbug (dcp, ("DMA: stop %x\n", dhp->dh_stop));
		ASSERT((dcp->dc_flags & DMACHAN_INTR) == 0);
		if (direction == DMACSR_READ) { /* FROM DEVICE TO MEM */
			rem = dhp->dh_stop - dhp->dh_start;
			rem = (rem&~(DMA_ENDALIGNMENT-1)) + 2*DMA_ENDALIGNMENT;
			if (rem <= DMA_MAXTAIL) {
				/* move entire last seg to tail buffer */
				dcp->dc_tailstart = dhp->dh_start;
				dcp->dc_taillen = dhp->dh_stop - dhp->dh_start;
			} else {
				dcp->dc_tailstart = (char *)
				 ((u_int)dhp->dh_stop &~ (DMA_ENDALIGNMENT-1));
				dcp->dc_taillen =
				    dhp->dh_stop - dcp->dc_tailstart;
				rem = 2 * DMA_ENDALIGNMENT;
				dhp->dh_stop = dcp->dc_tailstart;
				dhp->dh_link = dhp + 1;
				dhp++;
			}
			dhp->dh_start = (caddr_t)dcp->dc_ptail;
			dhp->dh_stop = dhp->dh_start + rem;
			dhp->dh_link = NULL;
			dhp->dh_state = 0;
			dhp->dh_flags = 0;
		} else {			/* FROM MEM TO DEVICE */
			dhp->dh_stop = (char *)
			    roundup((u_int)dhp->dh_stop, DMA_ENDALIGNMENT);
			dhp->dh_link = dhp + 1;
			/* guarantee that pre-fetches don't exhaust dma */
			dhp++;
			dhp->dh_start = (caddr_t)dcp->dc_ptail;
			dhp->dh_stop = dhp->dh_start + 3*DMA_ENDALIGNMENT;
			dhp->dh_link = NULL;
			dhp->dh_state = 0;
			dhp->dh_flags = 0;
		}
	}
	fdhp->dh_flags = DMADH_INITBUF;
	if (dhp - fdhp >= ndmahdr)
		panic("dma_list: dma_list_t overflow");
}

#if	0
void
dma_va_list(dcp, fdhp, vaddr, vcnt, bcnt, pmap, direction, ndmahdr)
struct dma_chan *dcp;
dma_list_t fdhp;
vm_offset_t *vaddr;
long *vcnt, bcnt;
pmap_t pmap;
int direction;
{
	struct dma_hdr *dhp;
	caddr_t paddr, start, stop;
	unsigned offset;
	long rem, bcount = *vcnt;
	vm_offset_t va = *vaddr;

	if (! DMA_BEGINALIGNED(vaddr))
		panic("dma_list: bad alignment");
	for (dhp = fdhp; bcnt > 0; dhp++) {
		start = (caddr_t)pmap_resident_extract(pmap, va);
		if (start == 0)
			panic("dma_list: zero pfnum");
		rem = NeXT_page_size - (va & NeXT_page_mask);
		if (rem > bcount)
			rem = bcount;
		if (rem > bcnt)
			rem = bcnt;
		va += rem;
		stop = start + rem;
		bcount -= rem;
		bcnt -= rem;
		if (dhp != fdhp && (dhp-1)->dh_stop == start &&
		    stop - (dhp-1)->dh_start < MAX_DMA_LEN)
			dhp--;
		else
			dhp->dh_start = start;
		dhp->dh_flags = 0;
		dhp->dh_stop = stop;
#if	CHAIN_READ
		dhp->dh_count = stop - start;
#endif	CHAIN_READ
		dhp->dh_link = bcnt > 0 ? dhp + 1 : NULL;
		dhp->dh_state = 0;
		if (bcount == 0) {
			vaddr++;
			va = *vaddr;
			vcnt++;
			bcount = *vcnt;
		}
	}
	dcp->dc_taillen = 0;
	dhp--;
	fdhp->dh_flags = DMADH_INITBUF;
	if (dhp - fdhp >= ndmahdr)
		panic("dma_list: dma_list_t overflow");
}
#endif

void
dma_cleanup(dcp, resid)
struct dma_chan *dcp;
long resid;
{
	/*
	 * Before calling, driver must have flushed tail of 
	 * transfer.  We're called with what the driver believes is the
	 * remaining byte count for the transfer (resid).  We have
	 * to transfer any part of the tail that come before that
	 * residual.  It should be sitting in dc_tail and should be
	 * copied to physical addresses starting at dc_tailstart.
	 */
	dma_abort(dcp);
	if (resid < 0)
		panic("dma_cleanup: negative resid");
	if (resid < dcp->dc_taillen)
		bcopy(dcp->dc_tail, dcp->dc_tailstart, dcp->dc_taillen-resid);
}

/*
 * dma_start -- start dma at a particular dma header
 */
void
dma_start(dcp, dhp, direction)
register struct dma_chan *dcp;
register struct dma_hdr *dhp;
register int direction;
{
	register volatile struct dma_dev *ddp;
	register int s, resetbits;

	dcp->dc_direction = direction;
	ddp = dcp->dc_ddp;

	/* Newline must be back slashed due to ASSERT macro */
	ASSERT((dcp->dc_flags & (DMACHAN_BUSY|DMACHAN_ERROR|DMACHAN_CHAINING))
	       == 0);

	dma_sync_cache(dhp->dh_start, dhp->dh_stop, direction);
	if (dhp->dh_link != NULL)
		dma_sync_cache(dhp->dh_link->dh_start, dhp->dh_link->dh_stop,
			direction);
	s = spldma();

	/* direction must be set before next & limit written */
	ddp->dd_csr = DMACMD_RESET | direction;
	if (dhp->dh_flags & DMADH_INITBUF) {
		DMA_W(ddp->dd_next_initbuf, dhp->dh_start);
	} else {
		DMA_W(ddp->dd_next, dhp->dh_start);
	}
	DMA_W(ddp->dd_limit, dhp->dh_stop);
	if (dcp->dc_flags & DMACHAN_ENETX) {
		DMA_W(ddp->dd_saved_next, dhp->dh_start);
		DMA_W(ddp->dd_saved_limit, dhp->dh_stop);
	}
	dcp->dc_current = dhp;

dbug (dcp, ("start %s: dcp %x dhp/bp %x/%x\n",
	    direction == DMACSR_WRITE? "write" : "read",
	    dcp, dhp, dhp->dh_start));
	if ((dhp = dhp->dh_link) != NULL) {
		DMA_W(ddp->dd_start, dhp->dh_start);
		DMA_W(ddp->dd_stop, dhp->dh_stop);
		if (dcp->dc_flags & DMACHAN_ENETX) {
			DMA_W(ddp->dd_saved_start, dhp->dh_start);
			DMA_W(ddp->dd_saved_stop, dhp->dh_stop);
		}
		ddp->dd_csr = DMACMD_STARTCHAIN | direction;
		dcp->dc_flags |= DMACHAN_CHAINING;
dbug (dcp, ("& %x/%x\n", dhp, dhp->dh_start));
	} else {
		ddp->dd_csr = DMACMD_START | direction;
	}
	dcp->dc_flags |= DMACHAN_BUSY;
	splx(s);
}

/*
 * dma_abort -- shutdown dma on channel
 * SIDE-EFFECT: saves next and dma state in dma_chan struct.
 * NOTE: This does NOT clear DMACHAN_ERROR -- if you do an abort,
 * you'll probably want to clear it, also.  (It doesn't clear it, because
 * you may want to look for errors after the abort.)
 */
void
dma_abort(dcp)
register struct dma_chan *dcp;
{
	register volatile struct dma_dev *ddp;
	register int s;

	ddp = dcp->dc_ddp;

	s = spldma();
	/*
	 * ALLEN & DAVE:
	 *
	 * Assuming the dma channel is connected
	 * to the SCC transmitter and the channel is active,
	 * now I issue this reset, how will the NEXT register
	 * relate to the last byte that the SCC will actually
	 * transmit?  HOPEFULLY, RESET does not trash next!
	 *
	 * Nothin' to do if the completion interrupt beat us.
	 */
	if (dcp->dc_flags & DMACHAN_BUSY) {
		/*
		 * The idea here is to fake an early completion
		 * so the serial line stop code will work.
		 * Since the zs driver doesn't chain buffers, this
		 * has got a chance.  I'd don't think there's enough
		 * info to do this if you're chaining unless you
		 * look at next to determine if the update occurred.
		 *
		 * If RESET doesn't clear BUSEXC, we could save
		 * the state after giving the RESET and be a
		 * a little more accurate about the terminating
		 * state.
		 */
		dcp->dc_state = get_dma_state(ddp);
		ddp->dd_csr = DMACMD_RESET;
		dcp->dc_next = ddp->dd_next;
		dcp->dc_current->dh_next = ddp->dd_next;
	} else
		ddp->dd_csr = DMACMD_RESET;
	dcp->dc_flags &=~ (DMACHAN_BUSY | DMACHAN_CHAINING);
	/*
	 * FIXME: handle the flush the crap out of the 4 byte
	 * dma stage 1 buffer here.
	 */
	splx(s);
	return;
}

/*
 * dma_intr -- handle dma interrupts
 * MUST BE CALLED AT spldma!
 * SIDE-EFFECT: saves next and dma state in dma_chan struct.
 */
void
dma_intr(dcp, pc, ps)
register struct dma_chan *dcp;
caddr_t pc;
int ps;
{
	register volatile struct dma_dev *ddp;
	register struct dma_hdr *dhp, *ndhp;
	register int state, busexc;

	ASSERT(curipl() >= IPLDMA);
	ddp = dcp->dc_ddp;
	
	/* shouldn't see interrupts if sw has started channel */
	if ((dcp->dc_flags & DMACHAN_BUSY) == 0) {
		extern int *intrstat;

		printf ("spurious DMA interrupt\n");
#if	DEBUG
		printf ("intrstat 0x%x intrmask 0x%x intr_mask 0x%x\n",
		    *intrstat, *intrmask, intr_mask);
		printf("dcp=0x%x pc=0x%x ps=0x%x\n", dcp, pc, ps);
#endif	DEBUG
		ddp->dd_csr = DMACMD_RESET;
		return;
	}

	/* dhp pts to last buffer sw thought was in next/limit */
	dhp = dcp->dc_current;
	ASSERT (dhp != NULL);

	state = get_dma_state(ddp);
#ifdef DEBUG
	if (state == DMASTATE_SINGLE || state == DMASTATE_CHAINING) {
		/* This isn't supposed to happen!  There's no reason for the
		 * interrupt.  Try re-reading the state, if it's still bogus
		 * just return and hope we get another interrupt.
		 */
		state = get_dma_state(ddp);
		if (state == DMASTATE_SINGLE || state == DMASTATE_CHAINING) {
			printf("spurious busy dma intr, ddp=0x%x\n", ddp);
			return;
		}
		printf("bad dma csr read, ddp=0x%x\n", ddp);
	}
#endif DEBUG

	dbug (dcp, ("intr: dcp %x state %x\n", dcp, state));
	dhp->dh_state = state;

	if (state == DMASTATE_1STDONE) {
		dbug (dcp, ("1STDONE\n"));
		if (dcp->dc_flags & DMACHAN_ENETR) {
			/* "saved_limit" is really "last byte count" reg */
			dhp->dh_next = ddp->dd_saved_limit;
		}
		/*
		 * hw has advanced start/stop to next/limit,
		 * update dc_current and dhp to reflect that
		 */
		dcp->dc_current = dhp = dhp->dh_link;
		ASSERT(dcp->dc_current != NULL);
		ASSERT(dcp->dc_flags & DMACHAN_CHAINING);

		/* see if we have yet another buffer to chain */
		if ((ndhp = dhp->dh_link) != NULL) {
			dbug (dcp, ("chain: dhp %x bp %x\n", ndhp,
			    ndhp->dh_start));
			dma_sync_cache(ndhp->dh_start, ndhp->dh_stop,
				dcp->dc_direction);
			DMA_W(ddp->dd_start, ndhp->dh_start);
			DMA_W(ddp->dd_stop, ndhp->dh_stop);
			if (dcp->dc_flags & DMACHAN_ENETX) {
				DMA_W(ddp->dd_saved_start, ndhp->dh_start);
				DMA_W(ddp->dd_saved_stop, ndhp->dh_stop);
			}
			ddp->dd_csr = DMACMD_CHAIN | dcp->dc_direction;
		} else {
			ddp->dd_csr = DMACMD_WRAPUP | dcp->dc_direction;
			dcp->dc_flags &=~ DMACHAN_CHAINING;
		}
		if (dhp->dh_flags & DMADH_SECTCHAIN)
			(*dcp->dc_sectchain) (dcp->dc_hndlrarg);
		if (dcp->dc_flags & DMACHAN_CHAININTR)
			softint_sched (dcp->dc_hndlrpri, dcp->dc_handler,
				       dcp->dc_hndlrarg);

		/* check to see if update was made in time */
		state = get_dma_state(ddp);
		if (state != DMASTATE_XDONE && state != DMASTATE_XERR2) {
			/* We made update in time */
			event_meter (EM_DMA);
			return;
		}

		/*
		 * we missed update,
		 * just pretend we didn't even try
		 */
		state &= ~DMACSR_SUPDATE;
		dcp->dc_flags &=~ DMACHAN_CHAINING;
		dhp->dh_state = state;
	}

	if (dcp->dc_flags & DMACHAN_CHAINING) {
		/*
		 * second buffer completed before we could service
		 * interrupt for first; that means we missed a 1st_done
		 * so fake it's actions
		 */
		ASSERT(state != DMASTATE_XDONE && state != DMASTATE_XERR2);
		if (dcp->dc_flags & DMACHAN_ENETR)
			dhp->dh_next = ddp->dd_saved_limit;
		dhp->dh_state = DMASTATE_1STDONE;
		dhp = dcp->dc_current = dhp->dh_link;
		ASSERT (dhp != NULL);
		dhp->dh_state = state;
	}

	/*
	 * We're either in a done state, or an error state.
	 * If there's no error and AUTOSTART is requested,
	 * just fire things back up if there's more to do.
	 */
	if((state == DMASTATE_SINGLE) || (state == DMASTATE_CHAINING))
		printf("Spurious DMA interrupt - state = %X @ %X\n",
			state, &ddp->dd_csr);
	dbug (dcp, (">>>dhp %x dh_link %x head %x tail %x "
	    "state %x flags %x <<<\n",
	    dhp, dhp->dh_link, dcp->dc_queue.dq_head, dcp->dc_queue.dq_tail,
	    state, dcp->dc_flags));

	dcp->dc_flags &= ~ (DMACHAN_BUSY | DMACHAN_CHAINING);
	dhp->dh_next = ddp->dd_next;
	if (dma_chip == 313)
		busexc = 0;
	else
		busexc = (state & DMACSR_BUSEXC);
	if (!busexc && (dcp->dc_flags & DMACHAN_AUTOSTART) &&
	    (ndhp = dhp->dh_link) != NULL) {
		if (dhp->dh_flags & DMADH_SECTCHAIN)
			(*dcp->dc_sectchain) (dcp->dc_hndlrarg);
		dbug (dcp, ("DONE -> AUTOSTART\n"));
		dma_start(dcp, ndhp, dcp->dc_direction);
		if (dcp->dc_flags & DMACHAN_CHAININTR)
			softint_sched (dcp->dc_hndlrpri, dcp->dc_handler,
				       dcp->dc_hndlrarg);
		event_meter (EM_DMA);
		return;
	}
	dbug (dcp, ("*STOP*\n"));
	dcp->dc_current = NULL;

	/*
	 * Nothing more we can do....
	 * Reset the dma machine, save the terminating
	 * state in the dc_state and final next value
	 * in dc_next, show that dma is not busy.
	 *
	 * If an error has occurred or the device driver
	 * wants an interrupt, post a software interrupt.
	 */
	ddp->dd_csr = DMACMD_RESET;
	dcp->dc_state = state;
	dcp->dc_next = ddp->dd_next;
	if (state == DMASTATE_XDONE)
		dcp->dc_flags |= DMACHAN_ERROR;
	if (dhp->dh_flags & DMADH_SECTCHAIN)
		(*dcp->dc_sectchain) (dcp->dc_hndlrarg);
	if (dcp->dc_flags & DMACHAN_DMAINTR)
		(*dcp->dc_dmaintr) (dcp->dc_hndlrarg);
	event_meter (EM_DMA);
	if (dcp->dc_flags & (DMACHAN_INTR|DMACHAN_ERROR)) {
		softint_sched (dcp->dc_hndlrpri, dcp->dc_handler,
				dcp->dc_hndlrarg);
#if NIPLMEAS == 0
		/*
		 * (Stollen from hardclock).
		 * If our previous ipl is less than the priority of the
		 * softint we just set then call softing_run directly
		 * after just lowering our ipl.
		 *
		 * The +1 stuff is because callout priorities are
		 * (unfortunately) 1 less than the hw ipl's that they
		 * run at.
		 */
		if (srtoipl(ps) < (dcp->dc_hndlrpri+1)
		    && dcp->dc_hndlrpri != CALLOUT_PRI_THREAD) {
			SPLD_MACRO(dcp->dc_hndlrpri+1);
			/* Now running at < spldma() */
			softint_run(dcp->dc_hndlrpri);
			/* go back to interrupt level for return */
			spldma();
		}
#endif NIPLMEAS == 0
	}
}

/*
 * pcopy -- copy cnt bytes from physical address src to physical address
 * dst
 * NOTE: src, dst, and cnt must all be multiples of 16 bytes (quad words)
 * ASSUMES: that mem-reg and reg-mem dma interrupts are masked off
 */
pcopy(src, dst, cnt)
char *src;
char *dst;
unsigned cnt;
{
	register volatile struct dma_dev *mrddp = (struct dma_dev *) P_M2R_CSR;
	register volatile struct dma_dev *rmddp = (struct dma_dev *) P_R2M_CSR;
	static int copying = 0;

	if (copying)
		panic("NO! Recursive copying!");
	copying = 1;
	/*
	 * make sure the caller is following the rules!
	 */
	ASSERT(((unsigned)src & 0xf) == 0);
	ASSERT(((unsigned)dst & 0xf) == 0);
	ASSERT((cnt & 0xf) == 0);

	/*
	 * get both feet on the ground....
	 */
	mrddp->dd_csr = DMACMD_RESET;
	rmddp->dd_csr = DMACMD_RESET;

	/* set-up memory to reg dma channel */
	DMA_W(mrddp->dd_next_initbuf, src);
	DMA_W(mrddp->dd_limit, src + cnt);

	/* set-up reg to memory dma channel */
	DMA_W(rmddp->dd_next_initbuf, dst);
	DMA_W(rmddp->dd_limit, dst + cnt);

	/*
	 * enable mem to reg, then reg to mem channel
	 */
	rmddp->dd_csr = DMACMD_START;
	mrddp->dd_csr = DMACMD_START;

	/*
	 * Keep restarting the reg-mem channel until either channel
	 * gets a bus error or completes (to handle m2m interrupt
	 * inhibits).
	 */
	while ((rmddp->dd_csr & DMACSR_ENABLE) == DMACSR_ENABLE
	    || (mrddp->dd_csr & DMACSR_ENABLE) == DMACSR_ENABLE)
		rmddp->dd_csr = DMACMD_START;
	copying = 0;
	return (0);
}

static int	zeros[8];		/* big enough for alignment */
					/* declare here to get phys addr */

/*
 * pfill -- fill cnt bytes from physical address dst with pattern
 * NOTE: dst and cnt must be multiples of 16 bytes (quad words)
 * ASSUMES: that mem-reg and reg-mem dma interrupts are masked off
 */
pfill(pattern, dst, cnt)
int pattern;
char *dst;
unsigned cnt;
{
	register volatile struct dma_dev *mrddp = (struct dma_dev *) P_M2R_CSR;
	register volatile struct dma_dev *rmddp = (struct dma_dev *) P_R2M_CSR;
	int	*zp;
	static	filling = 0;

	if (filling)
		panic("NO!  Recursive pfill");
	
	filling = 1;
	ASSERT(((unsigned)dst & 0xf) == 0);
	ASSERT((cnt & 0xf) == 0);

	/*
	 * get both feet on the ground....
	 */
	mrddp->dd_csr = DMACMD_RESET;
	rmddp->dd_csr = DMACMD_RESET;

	/* set-up reg to memory dma channel */
	DMA_W(rmddp->dd_next_initbuf, dst);
	DMA_W(rmddp->dd_limit, dst + cnt);

	/* fill memory to memory copy buffer */
	zp = zeros;
	zp = (int *)(((int)((char *)zp + 0xf)) & ~0xf);		/* align */
	zp[0] = pattern;
	zp[1] = pattern;
	zp[2] = pattern;
	zp[3] = pattern;

	DMA_W(mrddp->dd_next_initbuf, (char *)zp);
	DMA_W(mrddp->dd_limit, (char *)(zp + 4));

	rmddp->dd_csr = DMACMD_START;
	mrddp->dd_csr = DMACMD_START;

	/*
	 * Keep restarting the reg-mem channel until the reg-mem channel
	 * gets a bus error or completes (to handle m2m interrupt
	 * inhibits).
	 */
	while ((rmddp->dd_csr & DMACSR_ENABLE) == DMACSR_ENABLE)
		rmddp->dd_csr = DMACMD_START;
	filling = 0;
	return (0);
}

/* hardware uses upper bits of DMA addresses for flags */
#define	ADDR(a)		((char*)((int)(a) & ~(ENRX_EOP | ENRX_BOP)))

/* make caches consistent before starting DMA */
dma_sync_cache (start, stop, direction)
	char *start, *stop;
{
	char *pa;
	
	for (pa = ADDR(start); pa < ADDR(stop); pa += NeXT_page_size) {
		if (direction == DMACSR_WRITE)
			cache_push_page(pa);
		else
			cache_inval_page(pa);
	}
}





