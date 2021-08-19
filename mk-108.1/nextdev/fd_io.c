/*	@(#)fd_io.c	2.0	01/24/90	(c) 1990 NeXT	*/

/* 
 * fd_io.c -- I/O routines for Floppy Disk driver
 * KERNEL VERSION
 *
 * HISTORY
 * 24-Jan-90	Doug Mitchell at NeXT
 *	Created.
 *
 */ 
 
#import "fd.h"
#import <next/cpu.h>
#import <kern/lock.h>
#import <sys/time.h>
#import <sys/callout.h>
#import <sys/dk.h>
#import <next/psl.h>
#import <kern/mach_types.h>
#import <next/machparam.h>
#import <nextdev/dma.h>
#import <nextdev/fd_extern.h>
#import	<nextdev/fd_reg.h>
#import	<nextdev/fd_vars.h>
#import <nextdev/sf_access.h>
#import <nextdev/odreg.h>
#import <next/scr.h>
#import "sc.h"
/*
 * prototypes of local functions
 */
static void fc_intr(fd_controller_t fcp);
static fd_return_t fc_wait_pio(fd_controller_t fcp, u_char poll_bits);
static void fc_update_stat(fd_controller_t fcp, fd_ioreq_t fdiop);
static void fc_dmaintr(fd_controller_t fcp);
static fd_return_t fc_wait_sf_access(fd_controller_t fcp, int polling_loops);
static void fc_grant_sf_access(fd_controller_t fcp);
static fd_return_t fc_perpend(fd_controller_t fcp, 
	boolean_t wgate, 
	boolean_t gap);

extern u_char disr_shadow;

/*
 * We're init'd before SCSI, so we have to do this...
 */
#define INIT_SFA	1		/* init scsi/floppy access struct */
/* #define DMA_PAD	1		/* assert DMACHAN_PAD in dma channel
					 * flags */
/* #define DMA_INTR	1		/* DMA complete interrupts enabled */

/* 	
 *	initialization
 */
int fc_probe(caddr_t hw_reg_ptr, int ctrl_num)
{
	fd_controller_t fcp = &fd_controller[ctrl_num];
	sf_access_head_t sfahp = &sf_access_head[ctrl_num];
	fd_cntrl_regs_t fcrp;
	u_char drive_sel;
	int i;
	int rtn;
	
	XCDBG(("fc_probe: ctrl_num %d\n", ctrl_num));
	/* 
	 * init fd_controller struct for this chip
	 */
#if	FLOP_BUG
	/* perm select SCSI */
	*(u_char*)0x02014108 = 0;
	/* when cpu clk = 20 MHz, & no FD chip installed, probe passes! */
	return (0);
#else	FLOP_BUG
	hw_reg_ptr += slot_id_bmap;
#endif	FLOP_BUG
	fcrp = fcp->fcrp = (fd_cntrl_regs_t)hw_reg_ptr;
	fcp->bcp = fc_bcp[ctrl_num];		/* (written by autoconfig) */
	queue_init(&fcp->req_q);
	queue_init(&fcp->holding_q);
	simple_lock_init(&fcp->req_lock);
	simple_lock_init(&fcp->flags_lock);
	simple_lock_init(&fcp->holding_lock);
	fcp->flags = 0;
	fcp->ipl = I_IPL(I_SCSI);
	fcp->flpctl_sh = fcp->fcrp->flpctl = 0;
	fcp->last_dens = FD_DENS_NONE;
	
	fcp->sfahp = sfahp;
	fcp->sfad.sfad_flags = SFDF_EXCL;
	
	/*
	 * init other stuff...but only first time thru. 
	 */
	if(fcp == &fd_controller[0]) {
		simple_lock_init(&sfahp->sfah_lock);
		queue_init(&sfahp->sfah_q);
		sfahp->sfah_wait_cnt = 0;
		sfahp->sfah_flags  = 0;
		sfahp->sfah_last_dev = SF_LD_NONE;
		sfahp->sfah_excl_q = 0;
		sfahp->sfah_busy = 0;
		queue_init(&disk_eject_q);
		simple_lock_init(&vol_check_lock);
		for(i=0; i<NUM_FVP; i++)
			fd_volume_p[i] = NULL;
		lock_init(&fd_open_lock, TRUE);		/* can_sleep true */
		simple_lock_init(&vol_abort_lock);
		queue_init(&vol_abort_q);
	}

	/*
	 * Arbitrate with SCSI for access to hardware. Call sfa_relinquish if
	 * we abort after this...
	 */
	fd_polling_mode = TRUE;
	if(fc_wait_sf_access(fcp, TO_SFA_ARB * LOOPS_PER_MS)) {
		XDBG(("fc_probe: timeout waiting for sfa_access\n"));
		fd_polling_mode = FALSE;
		return(0);
	}
	fd_polling_mode = FALSE;

	/*
	 * see if 82077 is there. First just try reading all of the readable 
	 * registers. 
	 */
	if(!probe_rb((char *)&fcrp->sra) || !probe_rb((char *)&fcrp->srb) || 
	   !probe_rb((char *)&fcrp->dor) || !probe_rb((char *)&fcrp->msr) ||
	   !probe_rb((char *)&fcrp->fifo) || !probe_rb((char *)&fcrp->dir)) {
		sfa_relinquish(fcp->sfahp, &fcp->sfad, SF_LD_FD);
		XDBG(("fc_probe: register probe failed\n"));
	   	return(0);
	}

	/*
	 * Try writing and reading back some innocuous w/r bits.
	 */
	fcrp->dor = DOR_RESET_NOT;
	if(fcrp->dor != DOR_RESET_NOT) {
	  	sfa_relinquish(fcp->sfahp, &fcp->sfad, SF_LD_FD);
		XDBG(("fc_probe: dor test failed\n"));
		return(0);
	}
	for(drive_sel=0; drive_sel<4; drive_sel++) {
		fcrp->dor = drive_sel | DOR_RESET_NOT;
		if(fcrp->dor != (drive_sel | DOR_RESET_NOT)) {
			sfa_relinquish(fcp->sfahp, &fcp->sfad, SF_LD_FD);
			XDBG(("fc_probe: dor test failed\n"));
			return(0); 
		}
	}
	
	fd_polling_mode = TRUE;
	rtn = fc_82077_reset(fcp, NULL);
	
	if(rtn) {
		XDBG(("fc_probe: fc_82077_reset returned %n\n", rtn, 
			fd_return_values));
		fd_polling_mode = FALSE;
		sfa_relinquish(fcp->sfahp, &fcp->sfad, SF_LD_FD);
		return(0);
	}
	fd_polling_mode = FALSE;

	/*
	 * initialize dma_chan and interrupt linkage
	 *
	 * Note; SCSI has already init'd the hardware; it's going to happen 
	 * again in dma_init...no way to avoid it, but there's no problem
	 * since we own the hardware at this point.
	 */
	fcp->dma_chan.dc_handler = fc_dmaintr;
	fcp->dma_chan.dc_hndlrarg = (int)fcp;
	fcp->dma_chan.dc_hndlrpri = CALLOUT_PRI_SOFTINT1;
	fcp->dma_chan.dc_flags = 0;
#ifdef	DMA_PAD
	fcp->dma_chan.dc_flags |= DMACHAN_PAD;
#endif	DMA_PAD
#ifdef	DMA_INTR
	fcp->dma_chan.dc_flags |= DMACHAN_INTR;
#endif	DMA_INTR
	fcp->dma_chan.dc_ddp = (struct dma_dev *)P_SCSI_CSR;
	dma_init(&fcp->dma_chan, I_SCSI_DMA);
	fcp->flags |= FCF_INITD;
	fcp->sczctl = (u_char *)SDC_ADDRS;
	fcp->sczfst = (u_char *)SFS_ADDRS;
	sfa_relinquish(fcp->sfahp, &fcp->sfad, SF_LD_FD);

	/*
	 * start up timer to get threads started.
	 */
	if (!fc_thread_timer_started) {
		timeout((int(*)())fc_thread_timer, 0, hz);
		fc_thread_timer_started = TRUE;
	}
	return ((int)hw_reg_ptr);
	
} /* fc_probe() */

int fc_slave(struct bus_device *bdp, volatile caddr_t hw_reg_ptr)
{
	fd_controller_t fcp = &fd_controller[bdp->bd_bc->bc_ctrl];
	
	/*
	 * have device level code sort out which drive goes with
	 * this unit.
	 */
	XCDBG(("fc_slave: ctrl_num %d\n", bdp->bd_bc->bc_ctrl));
	return(fd_slave(fcp, bdp));
}

void fc_go(struct bus_ctrl *bcp)
{
	/* probably not used...necesssary for bus_driver...*/
}

void fc_init(caddr_t hw_reg_ptr)
{
	/* FIXME: which controller? */
	fd_controller_t fcp = &fd_controller[0];
	
	XCDBG(("fc_init: hw_reg_ptr 0x%x\n", hw_reg_ptr));
	hw_reg_ptr += slot_id_bmap;
	fcp->fcrp = (fd_cntrl_regs_t)hw_reg_ptr;
	fcp->flpctl_sh = 0;
} /* fcinit() */

fd_return_t fc_start(fd_volume_t fvp)
{
	fd_controller_t fcp;
	int s;
	
	/*
	 * two paths: if the incoming request has a valid drive assigned, 
	 * enqueue it on the appropriate controller's req_q. Otherwise,
	 * have v2d_map figure out what to do with it.
	 */
	XDBG(("fc_start: volume %d drive %d ", 
		fvp->volume_num, fvp->drive_num));
	if(VALID_DRIVE(fvp->drive_num)) {
		XDBG(("(valid vol; queued)\n"));
		fcp = fd_drive[fvp->drive_num].fcp;
		/*
		 * If controller thread isn't alive yet, wait for it to wake
		 * up and initialize itself. This can happen if we are being
		 * mounted as root during boot; we're called before the 
		 * controller thread has a chance to run.
		 */
		if(!fd_polling_mode && !(fcp->flags & FCF_THR_ALIVE)) {
			fd_thread_block((int *)&fcp->flags,
				FCF_THR_ALIVE,
				&fcp->flags_lock);
		}
		s = splbio();
		simple_lock(&fcp->req_lock);
		queue_enter(&fcp->req_q, fvp, fd_volume_t, io_link); 	
		simple_unlock(&fcp->req_lock);
		simple_lock(&fcp->flags_lock);
		fcp->flags |= FCF_COMMAND;
		simple_unlock(&fcp->flags_lock);
		splx(s);
		if(fd_polling_mode) {
			fc_thread();
			return(fcp->fdiop->status);
		}
		else {
			thread_wakeup((int *)&fcp->flags);
			return(FDR_SUCCESS);
		}
	}
	else {
		if(fvp->dev_ioreq.command == FDCMD_EJECT) {
			/*
			 * Special case; this volume has been ejected for
			 * other reasons (probably by disk_eject_thread). 
			 * I/O complete.
			 */
			XDBG(("fc_start: eject request for unmounted "
				"volume\n"));
			fd_intr(fvp);
			return(FDR_SUCCESS);
		}
		XDBG(("(invalid vol; mapped)\n"));
		v2d_map(fvp);
		return(FDR_SUCCESS);
	}
} /* fc_start() */

/*
 * This function (and all the subsequent code in this module) normally runs
 * as a kernel thread. This thread is the only way the floppy controller is
 * ever accessed. The thread sleeps on fcp->flags, waiting for FCF_COMMAND.
 * At this time, the fd_volume at the head of fcp->req_q contains the command
 * to be executed in fvp->dev_ioreq; we then dispatch to the command. 
 * fd_intr(fvp) is called at command complete, with status in 
 * fvp->dev_ioreq.status.
 *
 * At autoconfig time fc_thread runs as a function returning at I/O complete.
 * (Threads aren't running; no blocking; fd_polling_mode is true in this
 * case.) Note we still call fd_intr() in the polling case before we return. 
 */

void fc_thread()
{
	int unit;
	fd_volume_t fvp;
	fd_controller_t fcp = 0;
	fd_drive_t fdp;	
	int i;
	fd_ioreq_t fdiop;
	fd_return_t rtn;
	int device;
	int s;
	u_char motor_bit;
	
	XDBG(("fc_thread: start\n"));

	/*
	 * Figure out which fd_controller is ours.  If polling mode,
	 * take the first one with a command pending; else take the
	 * first one without an attached thread.
	 */
	for(i=0; i<NFC; i++) {
		fcp = &fd_controller[i];
		if(fd_polling_mode) {
			if(fcp->flags & FCF_COMMAND)
				break;
		}
		else {
		    if(!(fcp->flags & FCF_THR_ALIVE))
			break;
		}
	}
	if(i == NFC) 
		panic("fc_thread(): no active fd_controller");
	if(!fd_polling_mode) {
		/* 
	 	 * Let others know we're alive.
		 */
		fc_flags_bset(fcp, FCF_THR_ALIVE);	/* we're alive */ 
		thread_wakeup((int *)&fcp->flags);
	}

	while(1) {
		/*
		 * Main Controller I/O thread loop. 
		 */
		while(queue_empty(&fcp->req_q)) {
			/*
			 * Wait for something to do. 
			 */
			fc_flags_bclr(fcp, FCF_COMMAND);
			XIODBG(("fc_thread: calling fd_thread_block\n"));
			fd_thread_block((int *)&fcp->flags,
				FCF_COMMAND,
				&fcp->flags_lock);
			XIODBG(("fc_thread: unblock: flags = 0x%x\n",
				fcp->flags));
		} /* waiting for input */

		/*
		 * Command to execute is at head of fcp->req_q.
		 */
		s = splbio();
		simple_lock(&fcp->req_lock);
		fvp = (fd_volume_t)queue_first(&fcp->req_q);
		queue_remove(&fcp->req_q,
			fvp,
			fd_volume_t,
			io_link);
		simple_unlock(&fcp->req_lock);
		splx(s);
		fdiop = fcp->fdiop = &fvp->dev_ioreq;
		XDBG(("fc_thread: command = %n\n", fdiop->command,
			fd_command_values));
			
		/*
		 * handle special (non-I/O) requests.
		 */
		switch(fdiop->command) {
		    queue_head_t *qhp;
		    fd_volume_t held_vol, next_vol;
		    
		    case FDCMD_NEWVOLUME:
		    	/*
			 * A new disk has been inserted. See if any fvp's in
			 * holding_q now have a valid drive assigned to them;
			 * if so, remove from holding_q and enqueue the
			 * appropriate controller's input queue - it might not
			 * be ours! (Valid drive_num's are assigned by
			 * the volume_check thread.)
			 */
			qhp = &fcp->holding_q;
			held_vol = (fd_volume_t)queue_first(qhp);
			while(!queue_end(qhp, (queue_t)held_vol)) {
			    next_vol =
				(fd_volume_t)(held_vol->io_link.next);
			    if(VALID_DRIVE(held_vol->drive_num)) {
				XIODBG(("fc_thread: dequeueing vol %d from "
				    "holding_q\n", held_vol->volume_num));
				queue_remove(qhp,
					held_vol,
					fd_volume_t,
					io_link);
				if(fd_drive[held_vol->drive_num].fcp != fcp) {
				    /* another controller's job */
				    fc_start(held_vol);
				}
				else {
				    s = splbio();
				    simple_lock(&fcp->req_lock);
				    queue_enter(&fcp->req_q,
						    held_vol,
						    fd_volume_t,
						    io_link);
				    simple_unlock(&fcp->req_lock);
				    splx(s);
				}
			    }
			    held_vol = next_vol;
			}
			goto cmd_done;		/* notify caller of I/O 
						 * complete */
		    case FDCMD_ABORT:
		    	/*
			 * User has aborted an alert-panel asking for new disk.
			 * Scan thru holding_q for all I/Os marked with 
			 * FVF_NOTAVAIL; pass each one found up to caller
			 * via fd_intr().
			 */
			qhp = &fcp->holding_q;
			held_vol = (fd_volume_t)queue_first(qhp);
			while(!queue_end(qhp, (queue_t)held_vol)) {
			    next_vol =
				(fd_volume_t)(held_vol->io_link.next);
			    if(held_vol->flags & FVF_NOTAVAIL) {
				XIODBG(("fc_thread: ABORTING vol %d from "
				    "holding_q\n", held_vol->volume_num));
				queue_remove(qhp,
					held_vol,
					fd_volume_t,
					io_link);
				held_vol->dev_ioreq.status = FDR_VOLUNAVAIL;
				fd_intr(held_vol);
			    }
			    held_vol = next_vol;
			}
			goto cmd_done;		/* now I/O complete for the
						 * abort command itself */
			
		    default:			/* others - normal I/O */
 			break;
		} /* switch command */
		if(!(VALID_DRIVE(fvp->drive_num))) {
			/*
			 * Result of a race condition in which this request
			 * was enqueued on req_q while we were executing
			 * an eject command for this volume. Remap the I/O.
			 */
	 		XDBG(("(invalid vol in fc_thread; mapped)\n"));
			v2d_map(fvp);
			goto loop_done;
		}

		/*
		 * assign controller-relative unit number.
		 */
		fdp = &fd_drive[fvp->drive_num];
		fdiop->unit = fdp->unit;
		ASSERT(fdiop->unit < NUM_UNITS);
		
		/*
		 * initialize return parameters and flags.
		 */
		fdiop->status         = -1;	/* must be written by
						 * command handler */
		fdiop->cmd_bytes_xfr  = 0;
		fdiop->bytes_xfr      = 0;
		fdiop->stat_bytes_xfr = 0;			
		fc_flags_bclr(fcp, FCF_TIMEOUT    | FCF_TIMER_RUN |
			 	   FCF_INT_EXPECT | FCF_INT_PEND  |
				   FCF_DMAERROR   | FCF_DMAING );

		/*
		 * arbitrate with SCSI for hardware.
		 */
		if(rtn = fc_wait_sf_access(fcp, TO_SFA_ARB * LOOPS_PER_MS)) 
		{
			fdiop->status = rtn;
			goto hw_done;
		}

		/*
		 * an optimization - the hardware can tell us if drive 1
		 * is present...
		 */
		if(fdiop->unit == 1) {
			if(fcp->fcrp->sra & SRA_DRV1_NOT) {
				fdiop->status = FDR_BADDRV;
				XIODBG(("fc_thread: bad request for drive "
					"1\n"));
				goto cmd_done;
			}
		}
		
		/*
		 * log io_stats
		 */
		if(((device = fdp->bdp->bd_dk) >= 0) && fdiop->byte_count) {
			dk_busy |= 1 << device;
			dk_xfer[device]++;
			dk_seek[device]++;
			dk_wds[device] += fdiop->byte_count >> 6;
		}
#ifdef	DEBUG
		/*
		 * Just to be sure: make sure controller is in command
		 * phase.
		 */
		if(!fd_polling_mode) {
			if((fcp->fcrp->msr & MSR_POLL_BITS) == 
				(MSR_RQM | DIO_READ)) {
				XIODBG(("fc_thread: status phase at start of "
				      "command\n"));
				/*
				 * FIXME: pass a real string when hardware
				 * is fixed.
				 */
				rtn = fc_82077_reset(fcp, NULL);
				if(rtn) {
					fdiop->status = rtn;
					goto hw_done;
				}
			}
		}
#endif	DEBUG
		/*
 		 * if the controller chip got into serious trouble last 
		 * time thru, reset it.
		 */
		if(fcp->flags & FCF_NEEDSINIT) {
			rtn = fc_82077_reset(fcp, NULL);
			if(rtn) {
				fdiop->status = rtn;
				goto hw_done;
			}
		}
		/*
		 * select drive.  
		 */
		if(fdiop->unit >= NUM_UNITS) {
			XIODBG(("fc_thread: bad fdiop->unit (%d)\n", 
				fdiop->unit));
			fdiop->status = FDR_BADDRV;
			goto abort;
		}
		fcp->fcrp->dor |= ((fcp->fcrp->dor & ~DOR_DRVSEL_MASK) | 
				  fdiop->unit);		
		/*
		 * Abort now if drive not present.
		 */	
#ifdef	notdef
		/* ...not 'til motor is on...*/
		switch (machine_type) {		
		    case NeXT_CUBE:
			break;			/* can't detect this */
		    default:
		    	if(fcp->fcrp->flpctl & FLC_DRIVEID) {
				XIODBG(("fc_thread: Drive %dNOT PRESENT\n", 
					fdiop->unit));
				fdiop->status = FDR_BADDRV;
				goto abort;
			}
			break;
		}
#endif	notdef
		/*
		 * dispatch to the appropriate command routine. 
		 */
		switch(fcp->fdiop->command) {
		    case FDCMD_CMD_XFR:
		    	fc_cmd_xfr(fcp, fdp);
			break;
		    case FDCMD_EJECT:
			simple_lock(&fdp->lock);
			fdp->flags |= FDF_EJECTING;
			simple_unlock(&fdp->lock);
		    	fc_eject(fcp);
			/*
			 * Decouple drive and volume.
			 */
			fd_drive[fvp->drive_num].fvp = NULL;
			fvp->drive_num = DRIVE_UNMOUNTED;
			simple_lock(&fdp->lock);
			fdp->flags &= ~FDF_EJECTING;
			simple_unlock(&fdp->lock);
			break;
		    case FDCMD_MOTOR_ON:
		    	fc_motor_on(fcp);
			fcp->fdiop->status = FDR_SUCCESS;
			break;
		    case FDCMD_MOTOR_OFF:
		    	fc_motor_off(fcp);
			fcp->fdiop->status = FDR_SUCCESS;
			break;
		    case FDCMD_GET_STATUS:
		    	/*
			 * nothing to do; we'll pick up the status below.
			 */
			fc_motor_on(fcp);
			fdiop->status = FDR_SUCCESS;
			break;
		    default:
		    	fdiop->status = FDR_REJECT;
			break;
		}
		/*
		 * Stop timer if running.
		 */
		if(fcp->flags & FCF_TIMER_RUN)
			fc_stop_timer(fcp);

		/*
		 * Update media_id and motor_on (the latter for both the
		 * I/O request and for fdp...).
		 */
		fc_update_stat(fcp, fcp->fdiop);
		simple_lock(&fdp->lock);
		motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
		if(fcp->fcrp->dor & motor_bit)
			fdp->flags |= FDF_MOTOR_ON;
		else
			fdp->flags &= ~FDF_MOTOR_ON;
	
		simple_unlock(&fdp->lock);
		
		/*
		 * command complete (or aborted).
		 */
abort:
		/*
	 	 * Reset hardware if we timed out or got into serious
		 * trouble.
		 */
		fc_flags_bclr(fcp, FCF_INT_EXPECT);
		if(fdiop->status == FDR_TIMEOUT) 
			fc_82077_reset(fcp, "Command Timeout");
		if(fdiop->status == FDR_BADPHASE) 
			fc_82077_reset(fcp, "Bad Controller Phase");
		if(fcp->flags & FCF_NEEDSINIT)
			fc_82077_reset(fcp, "Controller hang");
hw_done:
		/*
		 * Return hardware to SCSI.
		 */
		if(fcp->sfad.sfad_flags & SFDF_OWNER)
			sfa_relinquish(fcp->sfahp, &fcp->sfad, SF_LD_FD);
		/*
		 * Notify device level of I/O complete.
		 */
cmd_done:
		XIODBG(("fc_thread: cmd complete; status = %n\n", 
			fdiop->status, fd_return_values));
		fd_intr(fvp);
		if(fd_polling_mode)
			return;
loop_done:
		continue;
	} /* main loop */
	/* NOT REACHED */
} /* fc_thread() */

static void fc_intr(fd_controller_t fcp)
{	
	/*
	 * hardware-level interrupt handler. 
	 *
	 * If we are expecting an interrupt, we take one of two actions:
	 * -- If controller is asking for a command byte, give it a
	 *    FCCMD_INTSTAT. 
	 * -- If the controller is presenting a status byte, take it and
	 *    put it in fcp->fdiop_i->stat_blk[0]. 
	 *
	 * WARNING: If we're NOT expecint an interrupt, we shouldn't touch 
	 * the hardware, since we very well might not own it!
	 */
	int unit = fcp - &fd_controller[0];
	fd_return_t rtn;
	fd_cntrl_regs_t fcrp = fcp->fcrp;
	
	XIODBG(("fc_intr: cntrlr %d", unit));
	if(fcp->flags & FCF_INT_EXPECT) {
		int i;
		
		/*
		 * First, if we are DMAing, flush the DMA fifo and get hardware 
		 * back to PIO mode.
		 */
		if(fcp->flags & FCF_DMAING) {
			
			for(i=0; i<8; i++) {	/* fixme: how many flushes? */
				*fcp->sczctl |= SDC_FLUSH;
				DELAY(5);
				*fcp->sczctl &= ~SDC_FLUSH;
				DELAY(5);
			}
			*fcp->sczctl &= ~SDC_DMAMODE;
			XIODBG(("   sczctl now = 0x%x\n", *fcp->sczctl));
		}
			
		XIODBG(("  msr=0x%x sra = 0x%x\n", fcrp->msr, fcrp->sra));
		/*
		 * Wait a nominal length of time for RQM
		 */
		for(i=0; i<INTR_RQM_LOOPS; i++) {
			if(fcrp->msr & MSR_RQM) {
				XIODBG(("  RQM detected after %d loops; "
					"msr = 0x%x\n", i, fcrp->msr));
				break;
			}
		}
		if(i == INTR_RQM_LOOPS) {
			XIODBG(("  fc_intr: RQM TIMEOUT\n"));
			goto badint;
		}
		if((fcrp->msr & MSR_DIO) == DIO_WRITE) {
			/*
			 * A Sense Interrupt Status command is expected.
			 */
			rtn = fc_send_byte(fcp, FCCMD_INTSTAT);
			if(rtn) {
				XIODBG(("fc_intr: Sense Interrupt Status "
				   "command returned %n\n", rtn, 
				   fd_return_values));
				goto badint;
			}
		}
		else {
			/*
			 * First status byte is available.
			 */
			fcp->fdiop_i->stat_blk[0] = fcrp->fifo;
			fcp->fdiop_i->stat_bytes_xfr++;
			XIODBG(("  First status byte = 0x%x\n", 
				fcp->fdiop_i->stat_blk[0]));
		}
		if(fcrp->sra & SRA_INTPENDING) {
			XIODBG(("fc_intr: SRA_PENDING still true\n"));
		}
		simple_lock(&fcp->flags_lock);
		fcp->flags |= FCF_INT_PEND;
		simple_unlock(&fcp->flags_lock);
		if(!fd_polling_mode)
			thread_wakeup((int *)&fcp->flags);
	}
	else {
		printf("fc%d: Spurious Floppy Disk Interrupt\n", unit);
		XIODBG(("  SPURIOUS FLOPPY INTERRUPT\n"));
		/*
		 * To avoid lots of possible hassles trying to reset and
		 * configure, just take the chip offline.
		 */
badint:
		fc_flags_bset(fcp, FCF_NEEDSINIT);
		uninstall_scanned_intr(I_PHONE);
		fc_flags_bclr(fcp, FCF_INT_INSTALL);
		return;
	}
	
} /* fc_intr() */

fd_return_t fc_82077_reset(fd_controller_t fcp, char *error_str)
{
	/*
	 * Initialize the 82077.
	 * We must own hardware at this time.
	 */
	fd_return_t rtn;
	fd_cntrl_regs_t fcrp = fcp->fcrp;
	
	XDBG(("fd_82077_reset\n"));
	ASSERT(fcp->sfad.sfad_flags & SFDF_OWNER);
	
	if((error_str != NULL) && !fd_polling_mode)
		printf("fc%d: Controller Reset: %s\n", fcp - &fd_controller[0],
			error_str);
			
	/*
	 * reset 82077. We must get from here to the configure command in
	 * 250 us to avoid the interrupt and 4 "sense interrupt status" 
	 * commmands resulting from polling emulation.
	 */
	fcrp->dor = 0;			/* reset true */
	DELAY(FC_RESET_HOLD);
	fcrp->dor = DOR_RESET_NOT;	/* reset false */
	
	/* 
	 * Initialize 82077 registers.
	 */
	fcrp->dsr = 0;			/* default precomp; lowest data rate */
	fcrp->ccr = 0;			/* lowest data rate */
	fcrp->flpctl = fcp->flpctl_sh = FLC_82077_SEL;
	fcp->last_dens = FD_DENS_NONE; 	/* force specify command on next 
					 * I/O */
	fcp->dma_chan.dc_flags &= ~DMACHAN_ERROR;

	/*
	 * send a configure command, and a specify (w/default maximum data
	 * rate).
	 */
	rtn = fc_configure(fcp, FD_DENS_4);
	if(rtn == 0) {
		rtn = fc_specify(fcp, FD_DENS_4, &fd_drive_info[0]);
	}
	if(rtn == FDR_SUCCESS) {
		install_scanned_intr(I_PHONE, (func)fc_intr, fcp);
		fc_flags_bclr(fcp, FCF_NEEDSINIT);
		fc_flags_bset(fcp, FCF_INT_INSTALL);
	}
	else {
		if(fcp->flags & FCF_INT_INSTALL) {
			uninstall_scanned_intr(I_PHONE);
			fc_flags_bclr(fcp, FCF_INT_INSTALL);
		}
		fc_flags_bset(fcp, FCF_NEEDSINIT);
	}
	return(rtn);
} /* fc_82077_reset() */

fd_return_t fc_send_byte(fd_controller_t fcp, u_char byte) {
	/*
	 * send one command/parameter byte. Returns FDR_SUCCESS,
	 * FDR_TIMEOUT, or FDR_BADPHASE.
	 */
	fd_return_t rtn;
	
	XPDBG(("fc_send_byte: byte=0x%x\n", byte));
	if((rtn = fc_wait_pio(fcp, DIO_WRITE)) == FDR_SUCCESS)
		fcp->fcrp->fifo = byte;
	XPDBG(("fc_send_byte: returning %n\n", rtn , fd_return_values));
	return(rtn);
}

fd_return_t fc_get_byte(fd_controller_t fcp, u_char *bp) {
	/*
	 * get one status byte. Returns FDR_SUCCESS, FDR_TIMEOUT, or
	 * FDR_BADPHASE.
	 */
	fd_return_t rtn;
	
	XPDBG(("fc_get_byte\n"));
	if((rtn = fc_wait_pio(fcp, DIO_READ)) == FDR_SUCCESS)
		*bp = fcp->fcrp->fifo;
	XPDBG(("fc_get_byte: returning %n data=0x%x\n", 
		rtn , fd_return_values, *bp));
	return(rtn);
}

static fd_return_t fc_wait_pio(fd_controller_t fcp, u_char poll_bits)
{
	/* 
	 * common routine for polling MSR_POLL_BITS in msr. Timer assumed 
	 * to be running.
	 */

	fd_return_t rtn = FDR_SUCCESS;
	register fd_cntrl_regs_t fcrp = fcp->fcrp;
	
	if(fd_polling_mode) {
		int loop;
		/*
		 * no threads or timeouts - count loops.
		 */
		for(loop=0; loop<FIFO_RW_LOOP; loop++) {
			if(fcrp->msr & MSR_RQM) {
				if((fcrp->msr & MSR_POLL_BITS) == 
				    (poll_bits | MSR_RQM)) {
					break;
				}
				else {
					XIODBG(("fc_wait_pio: BAD PHASE "
						"(msr = 0x%x)\n", fcrp->msr));
					return(FDR_BADPHASE);
				}
			}
		}
		if(loop == FIFO_RW_LOOP) {
			XIODBG(("fc_wait_pio: polling timeout; msr = 0x%x\n",
				fcp->fcrp->msr));
			rtn = FDR_TIMEOUT;
		}
		else {
			XPDBG(("fc_wait_pio: loop = %d\n", loop));
		}
	}
	else {
		/*
		 * normal case.
		 */
		while(!(fcp->flags & FCF_TIMEOUT )) {
			if(fcrp->msr & MSR_RQM) {
				if((fcrp->msr & MSR_POLL_BITS) == 
				    (poll_bits | MSR_RQM)) {
					break;
				}
				else {
					XIODBG(("fc_wait_pio: BAD PHASE (msr "
						"= 0x%x)\n", fcrp->msr));
					rtn = FDR_BADPHASE;
					break;
				}
			}
		}
		if(fcp->flags & FCF_TIMEOUT) {
			XIODBG(("fc_wait_pio: timeout; msr = 0x%x\n",
				fcp->fcrp->msr));
			rtn = FDR_TIMEOUT;
		}
	}
	return(rtn);
} /* fc_wait_pio() */
		
void fc_flpctl_bset(fd_controller_t fcp, u_char bits)
{
	/*
	 * set bit(s) in flpctl and its shadow.
	 *
	XIODBG(("fc_flpctl_bset: fcrp = 0x%x bits = 0x%x\n", fcp->fcrp, bits));
	*/
	fcp->flpctl_sh |= bits;
	fcp->fcrp->flpctl = fcp->flpctl_sh;
}

void fc_flpctl_bclr(fd_controller_t fcp, u_char bits)
{
	/*
	 * clear bit(s) in flpctl and its shadow.
	 *
	XIODBG(("fc_flpctl_bclr: fcrp = 0x%x bits = 0x%x\n", fcp->fcrp, bits));
	 */
	fcp->flpctl_sh &= ~bits;
	fcp->fcrp->flpctl = fcp->flpctl_sh;
}

/*
 * Update motor_on, media_id, and write protect.
 */
static void fc_update_stat(fd_controller_t fcp, fd_ioreq_t fdiop)
{
	u_char motor_bit;
	fd_cntrl_regs_t fcrp = fcp->fcrp;
	
	/*
	 * First check for drive present.
	 */
	switch (machine_type) {		
	    case NeXT_CUBE:
		fdiop->drive_stat.drive_present = 1;	/* can't detect this */
	    default:
	    	fdiop->drive_stat.drive_present = 
			fcp->fcrp->flpctl & FLC_DRIVEID ? 0 : 1;
		break;
	}

	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	if(fcrp->dor & motor_bit)
		fdiop->drive_stat.motor_on = 1;
	else
		fdiop->drive_stat.motor_on = 0;
	fdiop->drive_stat.media_id = fcrp->flpctl & FLC_MID_MASK;
	if(fcrp->sra & SRA_WP_NOT)		/* true low! Also, this is
						 * apparently not valid unless
						 * the motor is on...*/
		fdiop->drive_stat.write_prot = 0;
	else
		fdiop->drive_stat.write_prot = 1;
}

/*
 * fifo values(debug only)
 */
int cf2_fifo_value=CF2_FIFO_DEFAULT;
int cf2_efifo=0;			/* enabled */

fd_return_t fc_configure(fd_controller_t fcp, u_char density)
{
	struct fd_ioreq ioreq;
	struct fd_configure_cmd *fccp;
	fd_return_t rtn;
	
	XIODBG(("fc_configure density = %d\n", density));
	/*
	 * build a configure command 
	 */
	bzero(&ioreq, sizeof(struct fd_ioreq));
	fccp = (struct fd_configure_cmd *)ioreq.cmd_blk;
	fccp->opcode = FCCMD_CONFIGURE;
	fccp->rsvd1 = 0;
	/*
	 * Enable implied seek, disable drive polling
	 */
	fccp->conf_2 = CF2_EIS | CF2_DPOLL | cf2_fifo_value | cf2_efifo;
	fccp->pretrk = CF_PRETRACK;
	ioreq.timeout = TO_SIMPLE;
	ioreq.command = FDCMD_CMD_XFR;
	ioreq.num_cmd_bytes = sizeof(struct fd_configure_cmd);
	rtn = fc_send_cmd(fcp, &ioreq);
	XIODBG(("fc_configure: returning %n\n", rtn, fd_return_values));
	return(rtn);
} /* fc_configure() */


#define HULT(time, max_time, mult) \
	(((time) >= (max_time)) ? 0 : (((time)+(mult)-1) / (mult)))

/*
 * Select specified density. This involves setting the data rate select bits
 * in dsr and ccr and sending a perpendicular command as well as sending
 * a specify command.
 */
fd_return_t fc_specify (fd_controller_t fcp,
	int density,			/* FD_DENS_4, etc. */
	struct fd_drive_info *fdip)
{
	struct fd_ioreq ioreq;
	struct fd_specify_cmd *fcsp;
	fd_return_t rtn;
	register fd_cntrl_regs_t fcrp = fcp->fcrp;
	int data_rate;
	boolean_t wgate, gap;
	
	XIODBG(("fc_specify: density %d   seek_rate %d\n", density, 
		fdip->seek_rate));
	XIODBG(("   unload_time %d   load_time %d\n", 
		fdip->head_unload_time, fdip->head_settle_time));
	
	bzero(&ioreq, sizeof(struct fd_ioreq));
	fcsp = (struct fd_specify_cmd *)ioreq.cmd_blk;
	fcsp->opcode = FCCMD_SPECIFY;
	switch(density) {
	    case FD_DENS_1:
		data_rate = DRATE_MFM_250;
		wgate = 1;
		gap = 0;
		/*
		 * Only cube prototypes need this...
		 */
		switch (machine_type) {		
		    case NeXT_CUBE:
			fc_flpctl_bset(fcp, FLC_DS0);
			break;
		}
		/* specify cmd block...*/
	    	fcsp->srt = 16 - ((fdip->seek_rate + 1) / 2);	/* round up */	
		fcsp->hlt = HULT(fdip->head_settle_time, 512, 4);
		fcsp->hut = HULT(fdip->head_unload_time, 512, 32);
		break;
	    case FD_DENS_2:
		data_rate = DRATE_MFM_500;
		wgate = 1;
		gap = 0;
		switch (machine_type) {		
		    case NeXT_CUBE:
			fc_flpctl_bclr(fcp, FLC_DS0);
			break;
		}
	    	fcsp->srt = 16 - fdip->seek_rate;
		fcsp->hlt = HULT(fdip->head_settle_time, 256, 2);
		fcsp->hut = HULT(fdip->head_unload_time, 256, 16);
		break;
	    case FD_DENS_4:
		data_rate = DRATE_MFM_1000;
		wgate = 1;
		gap = 1;
		switch (machine_type) {		
		    case NeXT_CUBE:
			fc_flpctl_bset(fcp, FLC_DS0);
			break;
		}
	    	fcsp->srt = 16 - 2 * fdip->seek_rate;
		fcsp->hlt = HULT(fdip->head_settle_time, 128, 1);
		fcsp->hut = HULT(fdip->head_unload_time, 128, 8);
		break;
	    default:
	    	printf("fd: Bogus density (%d) in fc_specify()\n", density);
		return(FDR_REJECT);
	}

	fcrp->dsr = (fdip->do_precomp ? PRECOMP_DEFAULT : PRECOMP_0) |
	 	data_rate;
	fcrp->ccr = data_rate;
	XIODBG(("   srt 0x%x   hlt 0x%x   hut 0x%x\n", fcsp->srt, fcsp->hlt,
		fcsp->hut));
	ioreq.timeout = TO_SIMPLE;
	ioreq.command = FDCMD_CMD_XFR;
	ioreq.num_cmd_bytes = SIZEOF_SPECIFY_CMD;
	rtn = fc_send_cmd(fcp, &ioreq);
	XIODBG(("fc_specify: returning %n\n", rtn, fd_return_values));
	if (fdip->is_perpendicular && rtn == FDR_SUCCESS) {
		rtn = fc_perpend(fcp, wgate, gap);
	}
	if(rtn == FDR_SUCCESS)
		fcp->last_dens = density;
	else
		fcp->last_dens = FD_DENS_NONE;
	return(rtn);

} /* fc_specify() */

int clear_dma_on_int=1;

static void fc_dmaintr(fd_controller_t fcp)
{
	/*
	 * Two cases to handle:
	 * -- always disable DMA mode on DMA complete for Read.
	 * -- post DMA error for all DMAs.
	 */
	XIODBG(("fc_dmaintr\n"));
	if(!(fcp->flags & FCF_DMAING)) {
		printf("fc%d: Spurious DMA Interrupt\n", fcp-fd_controller);
		XIODBG(("fc%d: Spurious DMA Interrupt\n", fcp-fd_controller));
		return;
	}
	if(fcp->dma_chan.dc_flags & DMACHAN_ERROR) {
		XIODBG(("  DMA CHANNEL ERROR; csr = 0x%x\n", 
			fcp->dma_chan.dc_ddp->dd_csr));
		fcp->dma_chan.dc_flags &= ~DMACHAN_ERROR;
		fc_flags_bset(fcp, FCF_DMAERROR | FCF_INT_PEND);
	}
	if(clear_dma_on_int) {
	    if(fcp->flags & FCF_DMAREAD) {
		    /*
			* Get hardware back to PIO mode
			*/  
		    if(fcp->flags & FCF_DMAING) {
			    *fcp->sczctl &= ~SDC_DMAMODE;
			    XIODBG(("   sczctl now = 0x%x\n", *fcp->sczctl));
		    }
	    }
	}
	thread_wakeup((int *)&fcp->flags);
}


static fd_return_t fc_wait_sf_access(fd_controller_t fcp, int polling_loops)
{
	/* 
	 * wait for access to be granted by sf_access mechanism. Could return
	 * immediately or block. Returns FDR_SUCCESS or FDR_TIMEOUT. If
	 * not polling, timer is started and stopped here.
	 */
	 
	fd_return_t rtn = FDR_SUCCESS;
	
	XIODBG(("fc_wait_sf_access\n"));
	fc_flags_bclr(fcp, FCF_FC_ACCESS | FCF_TIMEOUT);
	if(!fd_polling_mode) 
		fc_start_timer(fcp, TO_SFA_ARB*1000);
	fcp->sfad.sfad_start = (void *)fc_grant_sf_access;
	fcp->sfad.sfad_arg = fcp;
	if(sfa_arbitrate(fcp->sfahp, &fcp->sfad)) {
		/*
		 * we're enqueued; wait for access.
		 */
		if(fd_polling_mode) {
			int loop;
			/*
			 * no timeouts or threads - count loops.
			 */
			fcp->flags &= ~FCF_TIMEOUT;
			for(loop=0; loop<polling_loops; loop++) {
				if(fcp->flags & FCF_FC_ACCESS)
					break;
			}
			if(loop == polling_loops) {
				XIODBG(("fc_wait_sf_access: polling "
					"timeout\n"));
				fcp->flags |= FCF_TIMEOUT;
			}
			else {
				XIODBG(("fc_wait_sf_access: loop = %d\n", 
					loop));
			}
		}
		else {
			/*
			 * normal case.
			 */
			XIODBG(("fc_wait_sf_access: calling "
				"fd_thread_block\n"));
			fd_thread_block((int *)&fcp->flags,
				FCF_FC_ACCESS | FCF_TIMEOUT,
				&fcp->flags_lock);
			XIODBG(("fc_wait_sf_access: unblock; flags = 0x%x\n",
				fcp->flags));
		}
	} /* we had to wait */
	
	if(fcp->flags & FCF_TIMEOUT) {
		/* 
		 * we timed out. Remove our device from the queue.
		 */
		sfa_abort(fcp->sfahp, &fcp->sfad, SF_LD_FD);
		rtn = FDR_TIMEOUT;
	}
	else
		fc_stop_timer(fcp);
	if(rtn == FDR_SUCCESS) {
		/*
		 * Sucess. Grab the hardware.
		 */
		int s;
		
		s = spldma();
		switch(machine_type) {
		    case NeXT_CUBE:
		 	/* 
			 * old prototypes only 
			 */
			*(u_char *)DISR_ADDRS = disr_shadow = 
			  			disr_shadow | OMD_SDFDA;
			break;
		    default:
			fc_flpctl_bset(fcp, FLC_82077_SEL);
			break;
		}
		splx(s);
		
		/* 
		 * If we weren't the last owner of the hardware, grab the
		 * SCSI/floppy DMA interrupt. Skip if we haven't init'd
		 * the DMA channel yet...
		 */
		if((fcp->sfahp->sfah_last_dev != SF_LD_FD) &&
		   (fcp->flags & FCF_INITD)) {
			install_scanned_intr(I_SCSI_DMA, (PFI)dma_intr,
				    &fcp->dma_chan);
		}
	}
	XIODBG(("fc_wait_sf_access: returning %n\n", rtn, fd_return_values));
	return(rtn);

} /* fc_wait_sf_access() */

static fd_return_t fc_perpend(fd_controller_t fcp, 
	boolean_t wgate, 
	boolean_t gap)
{
	struct fd_ioreq ioreq;
	struct fd_perpendicular_cmd *fpcp;
	fd_return_t rtn;
	
	XIODBG(("fc_perpend: wgate %d gap %d\n", wgate, gap));
	bzero(&ioreq, sizeof(struct fd_ioreq));
	fpcp = (struct fd_perpendicular_cmd *)ioreq.cmd_blk;
	fpcp->opcode = FCCMD_PERPENDICULAR;
	fpcp->wgate = wgate;
	fpcp->gap = gap;
	ioreq.timeout = TO_SIMPLE;
	ioreq.command = FDCMD_CMD_XFR;
	ioreq.num_cmd_bytes = sizeof(struct fd_perpendicular_cmd);
	return fc_send_cmd(fcp, &ioreq);
}

static void fc_grant_sf_access(fd_controller_t fcp)
{
	/*
	 * called from sfa_relinquish() or sfa_arbitrate() when we have been
	 * granted access to the bus. We assume that we are at splbio() or
	 * above.
	 */
	XIODBG(("fc_grant_sf_access\n"));
	simple_lock(&fcp->flags_lock);
	fcp->flags |= FCF_FC_ACCESS;
	simple_unlock(&fcp->flags_lock);
	if(!fd_polling_mode)
		thread_wakeup((int *)&fcp->flags);
}

void fc_flags_bset(fd_controller_t fcp, u_int bits)
{
	/*
	 * safely set bit(s) in fcp->flags.
	 */
	int s;
	
	s = splbio();
	simple_lock(&fcp->flags_lock);
	fcp->flags |= bits;
	simple_unlock(&fcp->flags_lock);
	splx(s); 
}

void fc_flags_bclr(fd_controller_t fcp, u_int bits)
{
	/*
	 * safely clear bit(s) in fcp->flags.
	 */
	int s;
	
	s = splbio();
	simple_lock(&fcp->flags_lock);
	fcp->flags &= ~bits;
	simple_unlock(&fcp->flags_lock);
	splx(s); 
}


/* end of fd_io.c */
