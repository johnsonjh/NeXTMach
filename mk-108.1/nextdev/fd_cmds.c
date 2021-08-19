/*	@(#)fd_cmds.c	2.0	02/23/90	(c) 1990 NeXT	*/

/* 
 * fd_cmds.c -- I/O commands and timer functions for Floppy Disk driver
 * KERNEL VERSION
 *
 * HISTORY
 * 14-May-90	Gregg Kellogg (gk) at NeXT
 *	Untimeout us_timed-out functions via us_untimeout, not untimeout.
 *
 * 23-Feb-90	Doug Mitchell at NeXT
 *	Created.
 */

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
#import <next/pmon.h>
#import <next/pmon_targets.h>
#import <next/kernel_pmon.h>

/*
 * prototypes of local functions:
 */
static void fc_enable_interrupt(fd_controller_t fcp, fd_ioreq_t fdiop);
static fd_return_t fc_wait_intr(fd_controller_t fcp, 
	int us_time, 
	int polling_loops);
static void fc_log_bad_phase(fd_controller_t fcp);
static int dma_bytes_moved(struct dma_chan *dcp, dma_list_t dhp);
static void fc_dma_init(fd_controller_t fcp);

/*
 * Standard controller command routines. Command to be executed is in
 * *fcp->fdiop.
 */

void fc_cmd_xfr(fd_controller_t fcp, fd_drive_t fdp)
{
	register fd_cntrl_regs_t fcrp = fcp->fcrp;
	fd_ioreq_t fdiop = fcp->fdiop;
	fd_return_t rtn;
	u_char motor_bit;
	u_char opcode = fdiop->cmd_blk[0] & FCCMD_OPCODE_MASK;
	
	XDBG(("fc_cmd_xfr: opcode = %n\n", opcode, fc_opcode_values));
	
	/*
	 * Set last access time for this drive.
	 */
	event_set_ts(&fdp->last_access);
		
	/* 
 	 * select drive (in the command block to be sent). Why does the
	 * controller make us do this here and in fcrp->dor?
	 */
	switch(opcode) {
	    case FCCMD_VERSION:
	    case FCCMD_INTSTAT:
	    case FCCMD_SPECIFY:
	    case FCCMD_CONFIGURE:
	    case FCCMD_DUMPREG:
	    case FCCMD_PERPENDICULAR:
	    	/* no drive select necessary */
	    	break;
	    default:
	    	fdiop->cmd_blk[1] |= fcp->fdiop->unit;
		break;
	}
	
	/*
	 * Select Density. Skip if density hasn't changed since the last time 
	 * we came this way.
	 */
	if(fdiop->density != fcp->last_dens) {		
	   	if(rtn = fc_configure(fcp, fdiop->density)) {
			fdiop->status = rtn;
			return;
		}
		if(rtn = fc_specify(fcp,
				fdiop->density,
				&fd_drive_info[fdp->drive_type])) {
			fdiop->status = rtn;
			return;
		}
	}
	/*
	 * start motor if necessary. Skip if motor is already on or we're
	 * executing a command which does not require the disk to be spinning.
	 */
	motor_bit = DOR_MOTEN0 << fdiop->unit;
	if(!(fcrp->dor & motor_bit)) {
		switch(opcode) {
		    case FCCMD_READ:
 		    case FCCMD_READ_DELETE:
		    case FCCMD_WRITE:
		    case FCCMD_WRITE_DELETE:
		    case FCCMD_READ_TRACK:
		    case FCCMD_VERIFY:
		    case FCCMD_FORMAT:
  		    case FCCMD_RECAL:
		    case FCCMD_READID:
		    case FCCMD_SEEK:
			fc_motor_on(fcp);
			break;
		    default:
		    	break;
		}
	}
	/*
	 * go for it.
	 */
	fdiop->status = fc_send_cmd(fcp, fdiop);
} /* fc_cmd_xfr() */

/*
 * The following three commands use the timer for delay.
 */
void fc_eject(fd_controller_t fcp)
{
	struct fd_ioreq ioreq;
	fd_return_t rtn;
	u_char motor_bit;
	int density;
	
	/*
	 * First turn the motor on if it's off.
	 */
	XIODBG(("fc_eject\n"));
	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	if(!(fcp->fcrp->dor & motor_bit))
		fc_motor_on(fcp);

	/*
	 * Seek to the parking track. Make sure that we have a reasonable 
	 * density selected (we may be ejecting an unformatted disk).
	 */
	if(fcp->last_dens == FD_DENS_NONE)
		density = FD_DENS_1;
	else
		density = fcp->last_dens;
	bzero(&ioreq, sizeof(ioreq));
	fd_gen_seek(density, &ioreq, FD_PARK_TRACK, 0);
	if(rtn = fc_send_cmd(fcp, &ioreq)) {
		XIODBG(("fc_eject: SEEK FAILED\n"));
		fcp->fdiop->status = rtn;
		return;
	}
	
	/*
	 * Eject the disk.
	 */
	fc_flpctl_bset(fcp, FLC_EJECT);
	DELAY(TO_EJECT_PULSE);
	fc_flpctl_bclr(fcp, FLC_EJECT);
	XIODBG(("fc_eject: EXPECTING TIMEOUT\n"));
	fc_flags_bclr(fcp, FCF_INT_PEND);
	fc_wait_intr(fcp, TO_EJECT*1000, EJECT_LOOP);	
					/* just delay; no interrupt */
#ifdef	notdef
	/*
	 * Shut the motor off.
	 *
	 * Removed 03/29 - have to leave motor on to detect subsequent disk
	 * insertion. The motor won't actually spin.
	 */
	fcp->fcrp->dor &= ~motor_bit;
#endif	notdef
	fcp->fdiop->status = FDR_SUCCESS;
} /* fc_eject() */

void fc_motor_on(fd_controller_t fcp) 
{
	u_char motor_bit;
	
	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	if(fcp->fcrp->dor & motor_bit) {
		XIODBG(("fc_motor_on: already on\n"));
		return;
	}
	fcp->fcrp->dor |= motor_bit;
	XIODBG(("fc_motor_on: EXPECTING TIMEOUT\n"));
	fc_flags_bclr(fcp, FCF_INT_PEND);
	fc_wait_intr(fcp, TO_MOTORON*1000, MOTOR_ON_LOOP);	
					/* just delay; no interrupt */
} /* fc_motor_on() */

void fc_motor_off(fd_controller_t fcp) 
{
	u_char motor_bit;
	
	XIODBG(("fc_motor_off\n"));
	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	fcp->fcrp->dor &= ~motor_bit;
} /* fc_motor_off() */

/*
 * 	Common I/O subroutines
 */
#ifdef	DEBUG
int	fd_pre_init=0;
int	fd_post_init=1;
#endif	DEBUG

fd_return_t fc_send_cmd(fd_controller_t fcp, fd_ioreq_t fdiop) 
{
	/*
	 * send command in fdiop->cmd_blk; DMA to/from fdiop->addrs;
	 * place status (from result phase) in fdiop->stat_blk[]. Good
	 * status is verified (for all the commands we know about at least).
	 */
	int i;
	fd_return_t rtn = FDR_SUCCESS;
	u_char *csp;
	u_char opcode;
	char *dma_start_addrs;
	boolean_t no_interrupt=FALSE;
	boolean_t dma_enable = FALSE;
	int requested_byte_count = fdiop->byte_count;
	int direction;
	int do_pad = 0;
	int dma_after=0;
	struct fd_rw_stat *rw_statp = (struct fd_rw_stat *)fdiop->stat_blk;
	int block;
	struct fd_rw_cmd *pcmdp = (struct fd_rw_cmd *)fdiop->cmd_blk;
	
	opcode = fdiop->cmd_blk[0] & FCCMD_OPCODE_MASK;
	XDBG(("fc_send_cmd: opcode = %n\n", opcode, fc_opcode_values));
	fdiop->status = 0xffffffff;		/* Invalid */
	fdiop->cmd_bytes_xfr = 0;
	fdiop->bytes_xfr = 0;
	fdiop->stat_bytes_xfr = 0;
	
#ifdef	PMON
	block = (((int)pcmdp->cylinder) << 16) |
		(((int)pcmdp->head) << 8) |
		pcmdp->sector;
	pmon_log_event(PMON_SOURCE_FD,
		opcode == FCCMD_READ ? KP_SCSI_READ : KP_SCSI_WRITE,
		block,
		0,
		0);
#endif	PMON
	/*
	 * Set up DMA if enabled.
	 */	
	if(requested_byte_count > 0) {
		
		/*
		 * We require well-aligned transfers since we have no way 
		 * of detecting the transfer count at the device level - we
		 * have to rely on the DMA engine.
		 *
		 * Since some commands (particularly format) can't live with
		 * this restriction, we'll only enforce it for reads and 
		 * writes.
		 */
		if((opcode == FCCMD_READ) || (opcode == FCCMD_WRITE)) {
			if((! DMA_BEGINALIGNED(fdiop->addrs)) ||
			   (! DMA_ENDALIGNED(requested_byte_count))) {
				rtn = FDR_REJECT;
				printf("fd: Unaligned DMA\n");
				goto out;
			}
		}
		if(requested_byte_count > 0x100000) {
			rtn = FDR_REJECT;
			printf("fd: DMA byte count > 64K\n");
			goto out;
		}
#ifdef	DEBUG
		/* 
		 * Doesn't seem to be necessary before transfer...
		 */
		if(fd_pre_init)
			fc_dma_init(fcp);
#endif	DEBUG
		direction = (fdiop->flags & FD_IOF_DMA_DIR) == FD_IOF_DMA_RD ? 
				DMACSR_READ : DMACSR_WRITE;
		/*
		 * Kludge: writing at 250 KHz (i.e., FD_DENS_1) requires the
		 * use of the FIFO to avoid DMA hangs resulting from defects
		 * in the 82077. If we use the FIFO, we have to allow for
		 * more data to be transferred to the 82077 than we really 
		 * want - enough to fill the FIFO after the actual data
		 * has moved. For now, just add on I82077_FIFO_SIZE bytes
		 * to the DMA if we're at the low data rate. For writes and 
		 * reads, we'll truncate to a sector size.
		 */
		if((fdiop->flags & FD_IOF_DMA_DIR) == FD_IOF_DMA_WR) {
		   	do_pad = 1;
			/* 
			 * pad using 'after' argument 
			 */
			dma_after = I82077_FIFO_SIZE;
			requested_byte_count += I82077_FIFO_SIZE;
		}
		dma_list(&fcp->dma_chan, 
			fcp->dma_list, 
			fdiop->addrs,
		    	requested_byte_count,
			fdiop->pmap, 
			direction,
			NDMAHDR, 
		        0, 		/* before ?? */
			dma_after,	/* after  ?? */
			0,		/* secsize */
			0);		/* rathole_va */
		dma_start(&fcp->dma_chan, 
			fcp->dma_list, 
			direction);
		dma_enable = TRUE;
	}
	
	/*
	 * enable interrupt detection.
	 */
	fc_enable_interrupt(fcp, fdiop);
		
	/*
	 * Start timer for command bytes.
	 */
	fc_start_timer(fcp, TO_FIFO_RW);

	/*
	 * send command bytes.
	 */
	csp = fdiop->cmd_blk;
	for(i=0; i<fdiop->num_cmd_bytes; i++) {
		rtn = fc_send_byte(fcp, *csp++);
		switch(rtn) {
		    case FDR_SUCCESS:
			fdiop->cmd_bytes_xfr++;
			break;				/* go for another */
		    case FDR_BADPHASE:
		    	/*
			 * We're in status phase...abort. Controller needs
			 * a reset. (FIXME: detect illegal command, or just
			 * blow it off?)
			 *
			 * We'd kind of like to do a reset right now, but 
			 * fc_82077_reset does a configure command, which
			 * calls us...if things are really screwed up, 
			 * we'll call fc_82077_reset forever, recursively.
			 * Just flag the bad hardware state.
			 */
			fc_stop_timer(fcp);
			XIODBG(("fc_send_cmd: UNEXPECTED STATUS PHASE\n"));
#ifdef	DEBUG		
			/* dump these bogus bytes...*/
			fc_log_bad_phase(fcp);
#endif	DEBUG
			fc_flags_bset(fcp, FCF_NEEDSINIT);
			goto out;
		    default:
			goto out;			/* no hope */
		}
	}
	fc_stop_timer(fcp);

	/*
	 * Switch hardware to DMA mode if appropriate.
	 */
	if(requested_byte_count > 0) {
		fc_flags_bset(fcp, FCF_DMAING);
		if((fdiop->flags & FD_IOF_DMA_DIR) == FD_IOF_DMA_RD) {
			*fcp->sczctl |= SDC_DMAREAD;
			fc_flags_bset(fcp, FCF_DMAREAD);
		}
		else {
			*fcp->sczctl &= ~SDC_DMAREAD;
			fc_flags_bclr(fcp, FCF_DMAREAD);
		}
		*fcp->sczctl |= SDC_DMAMODE;
		XIODBG(("fc_send_cmd: sczctl now = 0x%x\n", *fcp->sczctl));
	}
	else
		fc_flags_bclr(fcp, FCF_DMAING);
	
	/*
	 * For most commands, we wait for result phase.
	 *  
	 * Note that for commands which require a Sense
	 * Interrupt Status command at interrupt, the command is sent by 
	 * the interrupt handler. For all other commands, the interrupt handler
	 * moved the first status byte to fdiop->stat_blk[] upon interrupt.
	 */
	switch(opcode) {
	    case FCCMD_INTSTAT:		/* sense interrupt status */
	    case FCCMD_SPECIFY:
	    case FCCMD_DRIVE_STATUS:
	    case FCCMD_CONFIGURE:
	    case FCCMD_VERSION:
	    case FCCMD_DUMPREG:
	    case FCCMD_PERPENDICULAR:
	    	break;			/* no interrupt for these */
	    default:
		pmon_log_event(PMON_SOURCE_FD,
			KP_SCSI_WAITIOC,
			block,
			0,
			0);
		XIODBG(("fc_send_cmd: waiting for interrupt\n"));
		rtn = fc_wait_intr(fcp, fdiop->timeout * 1000, 
			fdiop->timeout * LOOPS_PER_MS);
		break;
	}
	if(fcp->flags & FCF_TIMER_RUN)
		fc_stop_timer(fcp);
	/*
	 * IF DMAing, flush DMA fifo and get back to PIO mode
	 */
	if(fcp->flags & FCF_DMAING) {
		
		int s;
		
		/*
		 * Flush the DMA fifo (if fc_intr hasn't already done so).
		 */
		s=splbio();
		if(*fcp->sczctl & SDC_DMAMODE) {
			for(i=0; i<8; i++) {
				*fcp->sczctl |= SDC_FLUSH;
				DELAY(5);
				*fcp->sczctl &= ~SDC_FLUSH;
				DELAY(5);
			}
			*fcp->sczctl &= ~SDC_DMAMODE;
		}
		splx(s);
	}

	switch(rtn) {
	    case FDR_SUCCESS:
	    	break;
	    case FDR_TIMEOUT: 
	    	no_interrupt = TRUE;	/* proceed to get status */
		break;
	    default:
		goto out;
	}
	if(fdiop->stat_bytes_xfr < fdiop->num_stat_bytes) {
		/*
		 * Start timer for status bytes.
		 */
		fc_start_timer(fcp, TO_FIFO_RW);
	}
	
	/*
	 * Attempt to move specified number of status bytes. (We may be 
	 * starting at stat_blk[1]...). Stop if the controller goes into
	 * "command" phase as well...
	 */
	csp = &fdiop->stat_blk[fdiop->stat_bytes_xfr];
	for(i=fdiop->stat_bytes_xfr; i<fdiop->num_stat_bytes; i++) {
		rtn = fc_get_byte(fcp, csp++);
		switch(rtn) {
		    case FDR_SUCCESS:
			fdiop->stat_bytes_xfr++;	
			break;				/* go for another */
		    case FDR_BADPHASE:
			XIODBG(("fd_send_cmd: PREMATURE CMD PHASE; "
			      "stat_bytes_xfr = 0x%x\n", 
			      fdiop->stat_bytes_xfr));
		    	if(fdiop->stat_bytes_xfr)
				goto parse_status;	/* make some sense
							 * out of what we 
							 * have */
			else 
				goto out;		/* no hope */
				
		    default:
			goto out;			/* no hope */
		}
	}
parse_status:
	if(fcp->flags & FCF_TIMER_RUN)
		fc_stop_timer(fcp);
	/*
	 * Handle DMA stuff:
	 *	-- check for DMA errors
	 *	-- get bytes transferred from DMA engine
	 */
	if(fcp->flags & FCF_DMAING) {
		
		fc_flags_bclr(fcp, FCF_DMAING);
		/*
		 * If this was a successful write command, dma_bytes_moved
		 * doesn't work if the write succeeded because of the 'after' 
		 * mechanism used to fill the FIFO. 
		 */
		if(((opcode == FCCMD_WRITE) || 
		    (opcode == FCCMD_WRITE_DELETE)) &&
		   (rw_statp->stat1 == SR1_EOCYL)) {
		   	fdiop->bytes_xfr = fdiop->byte_count;
		}
		else {
			fdiop->bytes_xfr = dma_bytes_moved(&fcp->dma_chan,
			 		                   fcp->dma_list);
		}
		if(fdiop->bytes_xfr > fdiop->byte_count) {
			XIODBG(("fc_send_cmd: bytes requested = 0x%x\n",
				fdiop->byte_count));
			fdiop->bytes_xfr = fdiop->byte_count;
		}
		XIODBG(("fc_send_cmd: 0x%x bytes DMA'd\n", fdiop->bytes_xfr));
		XIODBG(("   next = 0x%x  dma_start_addrs = 0x%x\n",
		    fcp->dma_chan.dc_ddp->dd_next, fcp->dma_list[0].dh_start));
		XIODBG(("   dma csr = 0x%x\n", fcp->dma_chan.dc_ddp->dd_csr));
		XIODBG(("   sczctl now = 0x%x\n", *fcp->sczctl));
		XIODBG(("   msr = 0x%x   sra = 0x%x\n", 
			fcp->fcrp->msr, fcp->fcrp->sra));
		if(fcp->flags & FCF_DMAERROR) {
			XIODBG(("fc_send_cmd: DMA ERROR\n"));
			rtn = FDR_MEMFAIL;
		}
		/*
	 	 * For reads with imperfectly aligned endpoints, move data from
		 * tail buffer to user's buffer. 
		 */
		if((fdiop->flags & FD_IOF_DMA_DIR) == FD_IOF_DMA_RD) {
			caddr_t end_addrs = fdiop->addrs + fdiop->bytes_xfr;
			
			if(!DMA_ENDALIGNED(end_addrs)) {
				XIODBG(("fc_send_cmd: calling dma_cleanup; "
					"dc_taillen = 0x%x\n",
					fcp->dma_chan.dc_taillen));
				dma_cleanup(&fcp->dma_chan, 
					fdiop->byte_count - fdiop->bytes_xfr);
			}
			else
				dma_abort(&fcp->dma_chan);
		}
		else
			dma_abort(&fcp->dma_chan);
		dma_enable = FALSE;
#ifdef	DEBUG
		if(fd_post_init)
#endif	DEBUG
			fc_dma_init(fcp);	/* clean up so SCSI doesn't 
						 * have to do this bullshit. */
	}

	/*
	 * OK, parse returned status. 
	 */
	if(rtn == FDR_SUCCESS) {
		switch(opcode) {
		    /* 
		     * handle all of the standard read/write commands.
		     */
		    case FCCMD_READ:
		    case FCCMD_READ_DELETE:
		    case FCCMD_WRITE:
		    case FCCMD_WRITE_DELETE:
		    case FCCMD_READ_TRACK:
		    case FCCMD_VERIFY: 
		    case FCCMD_FORMAT:
		    case FCCMD_READID:
		    {
			u_char sbyte;
			
			if(fdiop->stat_bytes_xfr == 0) {
				/*
				 * Insufficient status info. Punt.
				 */
				rtn = FDR_CNTRLR;
				goto report;
			}
			sbyte = rw_statp->stat0 & SR0_INTCODE;
			if(sbyte != INTCODE_COMPLETE) {
				/* end of cylinder is normal I/O complete
				 * for read and write.
				 */
				if(rw_statp->stat1 == SR1_EOCYL)
					break;	
			    	/*
			     	 * Special case for read and write - if we
				 * were trying to move a partial sector, 
				 * INTCODE_ABNORMAL is expected. We ignore
				 * this status if we moved all of the data
				 * we tried to and the controller is reporting
				 * over/underrun. Kludge. 
				 */
				if((fdiop->byte_count != 0) &&
				   (sbyte == INTCODE_ABNORMAL) &&
				   (fdiop->byte_count == fdiop->bytes_xfr) &&
				   (rw_statp->stat1 & SR1_OURUN))
				   	break;
					
				if(rw_statp->stat0 & SR0_EQ_CHECK) {
					rtn = FDR_DRIVE_FAIL;
					goto report;
				}
				/*
				 * Can only go on if we got the rest of the
				 * status bytes.
				 */
				if(fdiop->stat_bytes_xfr < SIZEOF_RW_STAT) {
					rtn = FDR_MEDIA;
					goto report;
				}
				sbyte = rw_statp->stat1;
				if(sbyte & SR1_CRCERR) {
				    	if(rw_statp->stat2 & SR2_DATACRC) 
						rtn = FDR_DATACRC;
				   	 else
						rtn = FDR_HDRCRC;
				    	goto report;
				}
				if(sbyte & SR1_OURUN) {
					rtn = FDR_DMAOURUN;
					goto report;
				}
				if(sbyte & SR1_NOHDR) {
					rtn = FDR_NOHDR;
					goto report;
				}
				if(sbyte & SR1_NOT_WRT) {
					rtn = FDR_WRTPROT;
					goto report;
				}
				if(sbyte & SR1_MISS_AM) {
					rtn = FDR_NO_ADDRS_MK;
					goto report;
				}
				sbyte = rw_statp->stat2;
				if(sbyte & SR2_CNTRL_MK) {
					rtn = FDR_CNTRL_MK;
					goto report;
				}
				if(sbyte & (SR2_WRONG_CYL | SR2_BAD_CYL)) {
					rtn = FDR_SEEK;
					goto report;
				}
				if(sbyte & SR2_MISS_AM) {
					rtn = FDR_NO_DATA_MK;
					goto report;
				}
report:
				XIODBG(("fd_send_cmd: cmd = %n ERROR = %n\n",
					opcode, fc_opcode_values,
					rtn, fd_return_values));
				XIODBG(("   st0 = 0x%x st1 = 0x%x  st2 = "
					"0x%x\n",
					rw_statp->stat0, rw_statp->stat1,
					rw_statp->stat2));
			}
			break;
		    }
			
		    case FCCMD_SEEK:
		    {
		     	struct fd_seek_cmd *cmdp = 
				(struct fd_seek_cmd *)fdiop->cmd_blk;
		    	struct fd_int_stat *statp =
				(struct fd_int_stat *)fdiop->stat_blk;

			/*
			 * verify Seek End.
			 */
			if((statp->stat0 & SR0_SEEKEND) == 0) {

				/* No Seek Complete */
				XIODBG(("fd_send_cmd: cmd = SEEK; SEEK"
					" ERROR\n"));
				XIODBG(("   st0 = 0x%x cmdp->cyl  pcn = %d\n",
					statp->stat0, cmdp->cyl,
					statp->pcn));
				rtn = FDR_SEEK;
				break;
			}
		    	/*
			 * verify we're on the right track. We got the current
			 * cylinder # from the Sense Interrupt Status command.
			 */
			cmdp = (struct fd_seek_cmd *)fdiop->cmd_blk;
			statp = (struct fd_int_stat *)fdiop->stat_blk; 
			if(cmdp->relative)
				break;		/* can't verify relative seek's
						 * destination */
			else if(statp->pcn != cmdp->cyl) {
				rtn = FDR_SEEK;
				XIODBG(("fc_send_cmd: cmd = FCCMD_SEEK "
					"cmdp->cyl = %d  statp->pcn = %d\n",
					cmdp->cyl, statp->pcn));
			}
			break;
			
		    } /* case FCCMD_SEEK */
		    
		    case FCCMD_RECAL:
		    {
		     	struct fd_seek_cmd *cmdp = 
				(struct fd_seek_cmd *)fdiop->cmd_blk;
		    	struct fd_int_stat *statp =
				(struct fd_int_stat *)fdiop->stat_blk;

		    	/*
			 * verify we're on track 0. We got the current
			 * cylinder # from the Sense Interrupt Status command.
			 */
			statp = (struct fd_int_stat *)fdiop->stat_blk; 
			/*
			 * verify Seek End, and controller and drive both 
			 * saying we're on track 0.
			 */
			if((statp->stat0 & SR0_SEEKEND) == 0) {

				/* No Seek Complete */
				XIODBG(("fd_send_cmd: cmd = RECAL; SEEK"
					" ERROR\n"));
				XIODBG(("   st0 = 0x%x pcn = %d\n",
					statp->stat0, statp->pcn));
				rtn = FDR_SEEK;
				break;
			}
			if(statp->pcn != 0) {
				rtn = FDR_SEEK;
				XIODBG(("fc_send_cmd: cmd = FCCMD_RECAL "
					"statp->pcn = %d\n", statp->pcn));
				break;
			}
			if(fcp->fcrp->sra & SRA_TRK0_NOT) {
			 	rtn = FDR_SEEK;
				XIODBG(("fc_send_cmd: cmd = FCCMD_RECAL "
				      " SRA_TRK0_NOT TRUE\n"));
			}
			break;
			
		    } /* case FCCMD_RECAL */
		    
		    case FCCMD_INTSTAT:
		    	/* 
			 * Now the others with no interesting status:
			 */
		    case FCCMD_VERSION:
		    case FCCMD_SPECIFY:
		    case FCCMD_DRIVE_STATUS:
		    case FCCMD_CONFIGURE:
		    case FCCMD_DUMPREG:
		    case FCCMD_PERPENDICULAR:
			 break;
		
		} /* switch command */
	}
out:
	/*
	 * Cleanup controller state.
	 */
	if(fcp->flags & FCF_TIMER_RUN)
		fc_stop_timer(fcp);
	if(dma_enable)
		dma_abort(&fcp->dma_chan);
	if(no_interrupt)
		rtn = FDR_TIMEOUT;
	XIODBG(("fc_send_cmd: returning %n\n", rtn , fd_return_values));
	return(rtn);
} /* fc_send_cmd() */

static void fc_dma_init(fd_controller_t fcp) 
{
	/*
	 * King Kludge: inititialize DMA channel. Per K. Grundy's
	 * suggestion, we have to enable a short DMA transfer from
	 * device to memory, then disable it. To be clean about
	 * the dma_chan struct and DMa interrupts, for now we'll
	 * actually do dma_list, etc...FIXME. optimize so this 
	 * isn't so much of a performance hit!
	 */
	char dummy_array[0x20];
	char *dummy_p;
	
	dummy_p = DMA_ENDALIGN(char *, dummy_array);
	dma_list(&fcp->dma_chan, 
		fcp->dma_list, 
		(caddr_t)dummy_p,
		0x10,
		pmap_kernel(), 
		DMACSR_READ,
		NDMAHDR, 
		0, 		/* before ?? */
		0,		/* after  ?? */
		0,		/* secsize */
		0);		/* rathole_va */
	dma_start(&fcp->dma_chan, 
		fcp->dma_list, 
		DMACSR_READ);
	DELAY(10);
	dma_abort(&fcp->dma_chan);
}

static int dma_bytes_moved(struct dma_chan *dcp, dma_list_t dhp)
{
	/*
	 * Determine how many bytes actually transferred during a DMA using
	 * *dcp and dma_list. It is assumed that the current value of next
	 * in hardware points to (last address transferred + 1) and that
	 * dma_list has not been disturbed since the DMA.
	 *
	 * FIXME: this is not bulletproof. Here's a case where it will fail:
	 *	first dma_hdr.dh_start = 1000
	 *	             .dh_stop  = 2000
	 *      ...
	 *      nth   dma_hdr.dh_start = 2000
	 *	             .dh_stop  = 3000
	 *	...
	 *	At end of DMA, dd_next = 2000. 
	 *
	 *	Did the DMA end at the end of the first buffer or the 
	 *	start of the nth buffer? The code below will claim that the 
	 *	DMA ended at the end of the first buffer...
	 */
	int hdr_num;
	int bytes_xfr=0;
	char *end_addrs = dcp->dc_ddp->dd_next;
	
	XIODBG(("dma_bytes_moved: end_addrs = 0x%x\n", end_addrs));
	for(hdr_num=0; hdr_num<NDMAHDR; hdr_num++) {
		XIODBG(("   dh_start 0x%x   dh_stop 0x%x\n",
			dhp->dh_start, dhp->dh_stop));
		if((end_addrs < dhp->dh_start) ||
		   (end_addrs > dhp->dh_stop)) {
		   	/* 
			 * end not in this buf; must have moved the whole
			 * thing.
			 */
			XIODBG(("      end not in this buf\n"));
			goto next_buf;
		}
		else if((dcp->dc_current == dhp) ||
		        ((end_addrs > dhp->dh_start) && 
		        (end_addrs < dhp->dh_stop))) {
			/*
			 * case 1: dcp->dc_current == dhp. 
			 * This is where we stopped in the case of 
			 * an aborted DMA - dc_current non-NULL always implies
			 * "no DMA complete".
			 *
			 * Case 2 - unamibiguous termination in this buf.
			 *
			 * In either case, this is where we stopped.
			 */
			XIODBG(("   end of xfer(1) dc_current 0x%x\n",
				 dcp->dc_current));
			bytes_xfr += end_addrs - dhp->dh_start;
			return(bytes_xfr);
		}
		else if(end_addrs == dhp->dh_stop) {
			/*
			 * The h/w next pointer points to the end of this 
			 * buf. We moved the entire buf. We're done if 
			 * no other bufs were enabled.
			 */
			if((dhp->dh_state != DMASTATE_1STDONE) ||
			   (dhp->dh_link == NULL)) {
				/*
				 * this is where we stopped.
				 */
				XIODBG(("   end of xfer(2) dh_state 0x%x  "
					"dh_link 0x%x\n",
					dhp->dh_state, dhp->dh_link));
				bytes_xfr += end_addrs - dhp->dh_start;
				return(bytes_xfr);
			}
		}
		XIODBG(("   (else) end not in this buf\n"));
next_buf:
		/*
		 * This buf is not the end.
		 */ 
		ASSERT(dhp->dh_link != NULL);
		bytes_xfr += dhp->dh_stop - dhp->dh_start;
		dhp++;
	}
	panic("dma_bytes_moved: DMA buf overflow");
} /* dma_bytes_moved() */

static void fc_enable_interrupt(fd_controller_t fcp, fd_ioreq_t fdiop)
{
	int s;
	
	s = splbio();
	simple_lock(&fcp->req_lock);
	fcp->flags &= ~FCF_INT_PEND;
	fcp->flags |= FCF_INT_EXPECT;
	fcp->fdiop_i = fdiop;
	simple_unlock(&fcp->req_lock);
	splx(s);
}

static fd_return_t fc_wait_intr(fd_controller_t fcp, 
			int us_time, 
			int polling_loops)
{
	/*
	 * Sets up timeout of us_time microseconds; then waits for
	 * interrupt or timeout. Returns FDR_SUCCESS or FDR_TIMEOUT.
	 *
	 * fc_enable_intr() must be called before the earliest possible time 
	 * an interrupt could occur (like before sending command bytes).
	 */
	int s;
	fd_return_t rtn = FDR_SUCCESS;
	
	XIODBG(("fc_wait_intr\n"));
	ASSERT(us_time != 0);
	if(fd_polling_mode) {
		int loop;
		/*
		 * no timeouts or threads - count loops.
		 */
		fcp->flags &= ~FCF_TIMEOUT;
		for(loop=0; loop<polling_loops; loop++) {
			if(fcp->flags & FCF_INT_PEND)
				break;
		}
		if(loop == polling_loops) {
			XIODBG(("fc_wait_intr: polling timeout\n"));
			fcp->flags |= FCF_TIMEOUT;
		}
		else {
			XIODBG(("fc_wait_intr: loop = %d\n", loop));
		}
	}
	else {
		/*
		 * normal case.
		 */
		fc_flags_bclr(fcp, FCF_TIMEOUT);
		fc_start_timer(fcp, us_time);
		XIODBG(("fc_wait_intr: calling fd_thread_block\n"));
		fd_thread_block((int *)&fcp->flags, 
			FCF_INT_PEND | FCF_TIMEOUT,
			&fcp->req_lock);
		if(fcp->flags & FCF_TIMER_RUN)	/* shouldn't be, but just in 
						 * case... */
			fc_stop_timer(fcp);
		XIODBG(("fc_wait_intr: unblock; flags = 0x%x\n",
			fcp->flags));
	}
	/*
	 * disable further interrupts
	 */
	fc_flags_bclr(fcp, FCF_INT_EXPECT);
	if(fcp->flags & FCF_TIMEOUT) {
		XIODBG(("fc_wait_intr: TIMEOUT\n"));
		rtn = FDR_TIMEOUT;
	}
out:
	XIODBG(("fc_wait_intr: returning %n\n", rtn, fd_return_values));
	if((rtn == FDR_TIMEOUT) && !(fcp->flags & FCF_DMAING)) {
		XIODBG(("  sra=0x%x msr=0x%x\n", fcp->fcrp->sra, 
			fcp->fcrp->msr));
	}
	return(rtn);
	
} /* fc_wait_intr() */

#ifdef	DEBUG
static void fc_log_bad_phase(fd_controller_t fcp)
{
	fd_cntrl_regs_t fcrp = fcp->fcrp;
	u_char ch;
	int num_bytes=10;
	/*
	 * Dump up to 10 status bytes.
	 */
	while(((fcrp->msr & MSR_POLL_BITS) == (MSR_RQM | DIO_READ)) &&
		num_bytes) {
		ch = fcrp->fifo;
		XIODBG(("fc_log_bad_phase: status = 0x%x\n", ch));
		DELAY(10);
		num_bytes--;
	}		
}
#endif	DEBUG

/*
 * Timer functions.
 */
 
void fc_start_timer(fd_controller_t fcp, int microseconds)
{
	struct timeval tv;
	int s;
	
	if(fd_polling_mode)
		return;				/* timer not used */
	s = splbio();
	simple_lock(&fcp->req_lock);
	XIODBG(("fc_start_timer: us = %d\n", microseconds));
	ASSERT(!(fcp->flags & FCF_TIMER_RUN));
	fcp->flags &= ~FCF_TIMEOUT;
	fcp->flags |=  FCF_TIMER_RUN;
	simple_unlock(&fcp->req_lock);
	splx(s);
	if(microseconds >= 1000000) {
		tv.tv_sec  = microseconds / 1000000;
		tv.tv_usec = microseconds % 1000000;
	}
	else {
		tv.tv_sec  = 0;
		tv.tv_usec = microseconds;
	}
	us_timeout((func)fc_timeout, (vm_address_t)fcp, &tv,
		CALLOUT_PRI_SOFTINT0);	
	XIODBG(("fc_start_timer: done\n"));
}

void fc_timeout(fd_controller_t fcp)
{
	XIODBG(("fc%d: TIMEOUT DETECTED\n", fcp-fd_controller));
	fc_flags_bset(fcp, FCF_TIMEOUT);
	fc_flags_bclr(fcp, FCF_TIMER_RUN);
	thread_wakeup((int *)&fcp->flags);
}

void fc_stop_timer(fd_controller_t fcp)
{
	int s;

	/*
	 * There's a race condition here, where a timeout could happen just 
	 * as we're being called. In that case, we deem the timeout invalid;
	 * we clear the timer running as well as the timeout detected bits
	 * after cancelling the timer callout.
	 */	
	if(fd_polling_mode)
		return;			
	XIODBG(("fc_stop_timer\n"));
	us_untimeout((func)fc_timeout, (vm_address_t)fcp);
	fc_flags_bclr(fcp, FCF_TIMER_RUN | FCF_TIMEOUT);
} 

/* end of fd_cmds.c */
