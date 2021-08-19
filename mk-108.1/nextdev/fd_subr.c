/*	@(#)fd_subr.c	2.0	01/29/90	(c) 1990 NeXT	*/

/* 
 * fd_subr.c -- Support routines for Floppy Disk Driver
 * KERNEL VERSION
 *
 * HISTORY
 *  2-Aug-90	Gregg Kellogg (gk) at NeXT
 *	Changes for programatic disksort interface.
 * 10-Jul-90	Gregg Kellogg
 *	Added support for procedural disksort interface.
 * 29-Jan-90	Doug Mitchell at NeXT
 *	Created.
 */ 

#import "fd.h"
#import <sys/types.h>
#import <sys/buf.h>
#import <sys/callout.h>
#import <kern/queue.h>
#import <sys/param.h>
#import <sys/errno.h>
#import <sys/proc.h>
#import <sys/time_stamp.h>
#import <sys/dk.h>
#import <nextdev/disk.h>
#import <next/psl.h>
#import <next/machparam.h>
#import <vm/vm_map.h>
#import <vm/vm_kern.h>
#import <nextdev/dma.h>
#import	<nextdev/fd_extern.h>
#import	<nextdev/fd_vars.h> 
#import <nextdev/insertmsg.h>
#import <nextdev/voldev.h>
#import <next/pmon.h>
#import <next/pmon_targets.h>
#import <next/kernel_pmon.h>

/*
 * prototypes of private functions
 */
static void fd_log_2_phys(fd_volume_t fvp,
	u_int sector,
	struct fd_rw_cmd *cmdp);
static int fd_phys_to_log(fd_volume_t fvp, struct fd_rw_cmd *cmdp);
static boolean_t fd_setup_rw_req(fd_volume_t fvp, boolean_t bad_block_map);
static void fd_gen_rw_cmd(fd_volume_t fvp,
	fd_ioreq_t fdiop,
	u_int block,
	u_int byte_count,
	caddr_t dma_addrs,
	int read);
static void fd_gen_recal(fd_volume_t fvp, fd_ioreq_t fdiop);
static void fd_log_error(fd_volume_t fdp, char *err_string);
static void fd_copy_vol(fd_volume_t source_vol, fd_volume_t dest_vol);
static int vc_compare_vol(fd_volume_t fvp1, fd_volume_t fvp2);
static void vol_check_assign(fd_volume_t fvp,
	fd_drive_t fdp,	
	fd_volume_t vc_fvp);
static int dens_to_capacity(int density);
static void fd_alert_panel(fd_volume_t fvp, boolean_t wrong_disk);
static void fd_vol_panel_abort(void *param, int tag, int response_value);
static u_int fd_sectsize_max(void);
static void fd_disk_eject();

#ifdef	vm_allocate
#undef	vm_allocate	/* force use of vm/vm_user.c, with map argument */
#endif	vm_allocate
#ifdef	vm_deallocate
#undef	vm_deallocate
#endif	vm_deallocate


fd_return_t fd_start(fd_volume_t fvp)
{
	/* 
	 * Called when volume has new work to do. New command to be processed
	 * is represented by the first buf in fvp->io_queue. If this is
	 * a command which originated in fd_command (bp = fvp->local_buf),
	 * no additional processing is required here; the command parameters
	 * needed by the controller are in *fvp->local_ioreq_p. Otherwise, it's 
	 * a normal file system read or write started by fdstrategy(); build 
	 * command parameters (and place in fvp->dev_ioreq) based on the buf
	 * parameters.
	 *
	 * returns 0 (success) or -1 (error).
	 */
	struct buf *bp;
	int part_num;
	struct fd_format_info *fip = &fvp->format_info;
	
	XDBG(("fd_start: volume %d", fvp->volume_num));
	ASSERT(fvp->state == FVSTATE_IDLE);

	bp = disksort_first(&fvp->io_queue);
	if (bp)
		disksort_qbusy(&fvp->io_queue);

	if(bp == &fvp->local_buf) {
		XDBG((" local_buf request\n"));
		/* special command; easy case... */
		fvp->dev_ioreq = *fvp->local_ioreq_p;
		fvp->bytes_to_go = 0;
	}
	else {
		/* 
		 * normal I/O started by fdstrategy(). First map file system
		 * block to logical device block.
		 */
		struct partition *pp;
		struct disk_label *dlp = fvp->labelp;
		int num_blocks;
		
		part_num = FD_PART(bp->b_dev);
		XDBG((" normal request; part = %d\n", part_num));
		if(part_num >= NUM_FP) {
			printf("fd%d: Bad Floppy Partition\n",
				fvp->volume_num);
			goto err;
		}
		if(bp->b_flags&B_READ)
			fvp->io_flags |= FVIOF_READ;
		else
			fvp->io_flags &= ~FVIOF_READ;
		if(part_num != FD_LIVE_PART) {
			/*
			 * Normal partitions.
			 */
			if (!(fvp->format_info.flags & FFI_LABELVALID)) {
				printf("fd%d: invalid label\n",
					fvp->volume_num);
				goto err;
			}
			fvp->io_flags &= ~FVIOF_LPART;
			pp = &dlp->dl_part[part_num];
			fvp->start_sect = bp->b_blkno + pp->p_base + 
						dlp->dl_front; 	/* FS blocks */
			fvp->start_sect *= fvp->bratio;		/* sectors */
				
			num_blocks = howmany(bp->b_bcount, dlp->dl_secsize);
			/*
			 * only go to end of partition...
			 */
			if(bp->b_blkno + num_blocks > pp->p_size) {
				num_blocks = pp->p_size - bp->b_blkno;
				fvp->bytes_to_go = 
					num_blocks * dlp->dl_secsize;
			}
			else
				fvp->bytes_to_go = bp->b_bcount;
		}
		else {
			
			/*
			 * Special case - last partition:
			 * -- has no "partition" struct.
			 * -- does not need a label.
			 * -- has no front porch. 
			 * -- no bad block mapping.
			 * -- block size = physical sector size.
			 *
			 * Live partition I/O DOES perform track crossing 
			 * mapping and retries.
			 *
			 * Still need physical disk parameters,
			 * though...that was checked in fdstrategy. 
			 */
			fvp->io_flags |= FVIOF_LPART;
			fvp->start_sect = bp->b_blkno;
			num_blocks = howmany(bp->b_bcount,
					     fip->sectsize_info.sect_size);
			/*
			 * only go to end of partition...
			 */
			if(fvp->start_sect + num_blocks > fip->total_sects) {
				num_blocks = fip->total_sects -
				   	     fvp->start_sect;
				fvp->bytes_to_go = num_blocks *
						  fip->sectsize_info.sect_size;
			}
			else 
				fvp->bytes_to_go = bp->b_bcount;
		}
		
		/*
		 * Set up controller-level command block.
		 */
		fvp->start_addrs = bp->b_un.b_addr;
		if((bp->b_flags & (B_PHYS|B_KERNSPACE)) == B_PHYS) {
			fvp->dev_ioreq.map = bp->b_proc->task->map;
			fvp->dev_ioreq.pmap = vm_map_pmap(fvp->dev_ioreq.map);
		}
		else {
		    	fvp->dev_ioreq.pmap = pmap_kernel();
		}
		/*
		 * only do bad-block mapping on writes for the first pass. 
		 * Also, no mapping for last partition.
		 */
		fd_setup_rw_req(fvp, !(fvp->io_flags & FVIOF_READ));
		fvp->inner_retry_cnt = fd_inner_retry;
		fvp->outer_retry_cnt = fd_outer_retry;
		pmon_log_event(PMON_SOURCE_FD,
			fvp->io_flags & FVIOF_READ ? 
				KP_SCSI_READDQ : KP_SCSI_WRITEDQ,
  			bp->b_blkno,
  			bp->b_bcount,
			fvp->volume_num);

	} /* normal I/O request */
	fvp->state = FVSTATE_EXECUTING;
	return(fc_start(fvp));

err:
	/*
	 * bogus system request. Abort.
	 */
	if(FD_PART(bp->b_dev) == FD_LIVE_PART)
		bp->b_error = FDR_REJECT;
	else
		bp->b_error = EINVAL;
	bp->b_flags |= B_ERROR;
	fd_done(fvp);		/* FIXME: ??? */
	return(1);
} /* fd_start() */

void fd_intr(fd_volume_t fvp)
{
	int device;
	fd_ioreq_t fdiop = &fvp->dev_ioreq;
	struct buf *bp;
	u_char opcode;
	
	/*
	 * controller level has done everything it could to process the 
	 * command in fvp->dev_ioreq. If success, biodone() the result and go
	 * on to the next request in fvp->io_queue. If error, do retries here.
	 *
	 * Note: during normal operation (not polling), this function runs
	 * as part of the controller thread.
	 */
	XDBG(("fd_intr: volume %d state %n status %n\n", fvp->volume_num,
		fvp->state, fv_state_values, fdiop->status, fd_return_values));
	XDBG(("   cmd = %n", fdiop->command, fd_command_values));
	opcode = fdiop->cmd_blk[0] & FCCMD_OPCODE_MASK;
#ifdef	DEBUG
	if(fdiop->command == FDCMD_CMD_XFR) {
		XDBG(("  opcode = %n  bytes_to_go = %x\n",
			opcode,	fc_opcode_values, fvp->bytes_to_go));
	}
	else {
		XDBG(("\n"));
	}
#endif	DEBUG
	if(VALID_DRIVE(fvp->drive_num)) {
		if ((device = fd_drive[fvp->drive_num].bdp->bd_dk) >= 0)
			dk_busy &=~ (1 << device);
	}
	/* else must have just executed eject command...*/
	
	/*
	 * no retries on special commands.
	 */
	bp = disksort_first(&fvp->io_queue);
	if (bp == &fvp->local_buf) {
		fd_done(fvp);
	}
	else {
	    /* 
	     * note any data transferred regardless of status.
	     */
	    if(fdiop->bytes_xfr) {
	    	    int sect_size = fvp->format_info.sectsize_info.sect_size;
		    
		    /*
		     * OK: if doing a read or write command, we can only move
		     * an integral number of sectors, even though the hardware
		     * might tell us differently (on writes with poorly aligned
		     * end address, the hardware will tell us we moved up to
		     * 0x1C extra bytes). 
		     */
		    if((opcode == FCCMD_READ) || (opcode == FCCMD_WRITE))
			fdiop->bytes_xfr &= ~(sect_size - 1);

		    /* 
		     * FIXME: I think the 82077 passes us a whole sector's
		     * worth of data before detecting this. To indicate which 
		     * sector was bad (and to avoid passing bad data), back off
		     * on the "bytes transferred" by one sector.
		     */
		    if((fdiop->status == FDR_DATACRC) && (fdiop->bytes_xfr))
		    	fdiop->bytes_xfr -= sect_size;

	    	    if(fvp->padded_byte_cnt) {
		    	/*
			 * we wrote a short sector, padded with zeroes.
			 */
			ASSERT(fvp->sect_buf && 
			     (fvp->current_byte_cnt == sect_size));
			kfree(fvp->sect_buf, sect_size);
			XDBG(("   Padded I/O: 0x%x bytes xfr'd\n",
				fvp->padded_byte_cnt));
			if(fdiop->bytes_xfr == fvp->current_byte_cnt)
				fvp->bytes_to_go -= fvp->padded_byte_cnt;
			/*
			 * else assume error...
			 */
			fvp->padded_byte_cnt = 0;
		    }
		    else {
		    	XDBG(("   0x%x bytes xfr'd\n", fdiop->bytes_xfr));
			fvp->bytes_to_go -= fdiop->bytes_xfr;
		    }
		    fvp->start_addrs += fdiop->bytes_xfr;
		    if(sect_size) {
		    	/* maybe a non-disk I/O...*/
			fvp->start_sect += (fdiop->bytes_xfr / sect_size);
		    }
		    /* 
		     * In case of error, fvp->start sect should be the sector
		     * in error (the sector to be retried).
		     */
	    }
	    /*
	     * on break from this switch, must either fd_done() to complete
	     * the current I/O or fc_start() to retry it.
	     */
	    switch (fdiop->status) {
		case FDR_SUCCESS:
		    switch(fvp->state) {
			case FVSTATE_RECALIBRATING:
			    /* 
			     * now we go back and try some more I/O. Go back
			     * to retry mode.
			     */
			    XDBG(("fd_intr: state := FVSTATE_RETRYING\n"));
			    fvp->state = FVSTATE_RETRYING;
			    goto do_some_more;
			    
			case FVSTATE_RETRYING:
			    /*
			     * hey, the retry worked. Exit this mode.
			     */
			    XDBG(("fd_intr: state := FVSTATE_EXECUTING\n"));
			    fvp->state = FVSTATE_EXECUTING;
			    /* drop thru... */
			    
			case FVSTATE_EXECUTING:
do_some_more:
			    /*
			     * if any more data is yet to be transferred, set
			     * up another I/O request and send it down to the 
			     * controller. No remapping on read here, since 
			     * we're reading this data for the first time.
			     */
			    if(fvp->bytes_to_go) {
				fd_setup_rw_req(fvp, 
					!(fvp->io_flags & FVIOF_READ));
				fc_start(fvp);
				break;
			    }
			    else {
				/*
				 * Normal I/O completion.
				 */
				fd_done(fvp);
				break;	 
			    }
			default:
			    panic("fd_intr: BOGUS fvp->state");
		    } /* switch state */
		    break; 			/* from case FDR_SUCCESS */
		    
		case FDR_TIMEOUT:
		case FDR_DATACRC:
		case FDR_HDRCRC:
		case FDR_MEDIA:
		case FDR_SEEK:
		case FDR_DRIVE_FAIL:
		case FDR_NOHDR:
		case FDR_NO_ADDRS_MK:
		case FDR_CNTRL_MK:
		case FDR_NO_DATA_MK:
		case FDR_DMAOURUN:
		    /* 
		     * these errors are retriable.
		     */
		    if(fvp->state == FVSTATE_RETRYING) {
			if (--fvp->inner_retry_cnt <= 0) {
			    if(fvp->outer_retry_cnt-- <= 0) {
				/*
				 * we shot our wad. Give up.
				 */
				fd_log_error(fvp, "FATAL");
				fd_done(fvp);
				break;
			    }
			    else {
				/*
				 * Well, let's recalibrate the drive
				 * and try some more.
				 */
				fd_log_error(fvp, "RECALIBRATE");
				fvp->inner_retry_cnt = fd_inner_retry;
				fd_gen_recal(fvp, &fvp->dev_ioreq);
				fvp->state = FVSTATE_RECALIBRATING;
				fc_start(fvp);
				break;
			    }
			} /* inner retries exhausted */
			else {
			    /* 
			     * OK, try again. No need to remap; we doing 
			     * the same I/O as last time (or at least part of 
			     * it).
			     */
			    fd_log_error(fvp, "RETRY");
			    fd_setup_rw_req(fvp, FALSE);
			    fc_start(fvp);
			    break;
			}
		    } /* already retrying */
		    else {
			/*
			 * First error for this I/O. If reading, see if
		  	 * we have to map around the block in error. Then
			 * restart the I/O.
			 */
			if(fvp->io_flags & FVIOF_READ) {
			    /* map around possible bad blocks. (If writing,
			     * we already mapped around bad blocks in 
			     * fd_start().)
			     */
			    if(fd_setup_rw_req(fvp, TRUE) == 0) {
			    	  fvp->state = FVSTATE_RETRYING;
			    }
			    /* else NOT retrying; remapping. */
			}
			else {
			    /* 
			     * Retrying after a write error. No need to remap;
			     * we already did that for the initial I/O request.
			     */
			    fd_setup_rw_req(fvp, FALSE);
			    fvp->state = FVSTATE_RETRYING;
			}
			if((fvp->state == FVSTATE_RETRYING) && 
			   (fvp->inner_retry_cnt == 0)) {
			    /* 
			     * retries disabled. give up. 
			     */
			    fd_log_error(fvp, "FATAL");
			    fd_done(fvp);
			    break;
			}
			fd_log_error(fvp, "RETRY");
			fc_start(fvp);
			break;
		    } /* starting retry sequence */
		    break;			/* from retryable error case */
		    
		default:			/* non-retriable errors */
		    fd_log_error(fvp, "FATAL");
		    fd_done(fvp);
		    break;
	    } /* switch fdiop->status */
	} /* normal I/O request */ 
	XDBG(("fd_intr: EXITING; fvp->stat = %n\n", fvp->state,
		fv_state_values));
	return;
		
} /* fd_intr() */

static void fd_log_error(fd_volume_t fvp, char *err_string)
{
	if(fd_polling_mode)
		return;		/* errors during boot not important */
	printf("fd%d: Sector %d(d) cmd = %s; %n: %s\n", 
		fvp->volume_num, fvp->start_sect, 
		fvp->io_flags & FVIOF_READ ? "Read" : "Write",
		fvp->dev_ioreq.status,	fd_return_values, err_string);
}
	
void fd_done(fd_volume_t fvp)	
{
	struct buf *bp;
	fd_ioreq_t fdiop = &fvp->dev_ioreq;
	
	
	XDBG(("fd_done: volume %d\n", fvp->volume_num));
	/*
	 * The current request disksort_first(&fvp->io_queue) has either
	 * succeeded or we've given up trying to make it work.
	 * Advance io_queue, do a biodone(), and restart I/O if io_queue
	 * non-empty.
	 */
	bp = disksort_first(&fvp->io_queue);
	if (bp == NULL)
		panic("sddone: no buf on sdd_queue");

	bp->b_resid = fvp->bytes_to_go;
	if(fdiop->status) {
		switch(fdiop->status) {
		    case FDR_VOLUNAVAIL:
		    	bp->b_error = ENXIO;
		    	break;
		    /* 
		     * FIXME: any other special cases?
		     */
		    default:
			bp->b_error = EIO;
			break;
		}
	}
	if(bp->b_error) 
		bp->b_flags |= B_ERROR;
	simple_lock(&fvp->lock);
	(void) disksort_remove(&fvp->io_queue, bp);
	simple_unlock(&fvp->lock);
	if(bp == &fvp->local_buf) {
		/*
		 * Special command; update status for caller
		 */
		*fvp->local_ioreq_p = fvp->dev_ioreq;
	}
#ifdef	PMON
	else {
		pmon_log_event(PMON_SOURCE_FD,
			bp->b_flags&B_READ ? 
				KP_SCSI_READ_CMP : KP_SCSI_WRITE_CMP,
			bp->b_blkno,
			bp->b_bcount,
			fvp->volume_num);
	}
#endif	PMON
	biodone(bp);
	fvp->state = FVSTATE_IDLE;
	if (disksort_first(&fvp->io_queue) != NULL)
		fd_start(fvp);

} /* fd_done() */

#define KVM_PUNT	1	
#define REQUIRE_SECT_ALIGN	1	/* */

static boolean_t fd_setup_rw_req(fd_volume_t fvp, 
	boolean_t bad_block_map)	/* TRUE = check for remapping */
{
	/* input:  fvp->start_sect
	 *	   fvp->bytes_to_go
	 *	   fvp->start_addrs
	 *	   fvp->io_flags
	 *	   fvp->dev_ioreq.pmap
	 *	   fvp->dev_ioreq.map
	 *	   fvp->format_info
	 * output: fvp->current_sect
	 *	   fvp->current_byte_cnt
	 *	   fvp->current_addrs
	 *	   fvp->dev_ioreq
	 * 
	 * This routine determines the largest "safe" I/O which can be
	 * performed starting at fvp->start_sect, up to a max of 
	 * fvp->bytes_to_go bytes. The bad block table will only be consulted
	 * if bad_block_map is also TRUE.
	 *
	 * No I/O is allowed to go past a track boundary.
	 *
	 * Short writes (less than one sector) result in user data being 
	 * copied to *fvp->sect_buf, memory for which is allocated here.
	 * DMA occurs out of this buffer. The buffer must be freed upon I/O
	 * complete. 
	 *
	 * Parameters describing the resulting "safe" I/O are placed in
	 * fvp->current_sect and fvp->current_byte_cnt. An fd_rw_cmd is then
	 * generated and placed in fvp->dev_ioreq.cmd_blk[].
	 *
	 * Returns TRUE if mapping occurred.
	 */
	struct fd_rw_cmd start_rw_cmd;		/* first sector, phys params */
	struct fd_format_info *fip = &fvp->format_info;
	u_int sect_size = fip->sectsize_info.sect_size;
	int num_sects = howmany(fvp->bytes_to_go, sect_size);
	int bytes_to_xfr;
	caddr_t start_addrs;
	
	XDBG(("fd_setup_rw_req: start_sect = %d\n", fvp->start_sect));
	/*
	 * See if we'll wrap past a track boundary. Max sector number on
	 * a track is sects_per_trk, NOT sects_per_trk-1...
	 */
	fd_log_2_phys(fvp, fvp->start_sect, &start_rw_cmd);
	if((start_rw_cmd.sector + num_sects) >
	  (fip->sectsize_info.sects_per_trk+1)) {
		num_sects = fip->sectsize_info.sects_per_trk -
		    start_rw_cmd.sector + 1;
		bytes_to_xfr = num_sects * sect_size;
		XDBG(("fd_setup_rw_req: track wrap: num_sects = %d\n", 
			num_sects));
	}
	else {
		/*
		 * The whole I/O fits on one track.
		 */
		bytes_to_xfr = fvp->bytes_to_go;
	}
	/*
	 * See if we have to deal with a partial sector.
	 */
	start_addrs = fvp->start_addrs;
	fvp->padded_byte_cnt = 0;	
#ifdef	REQUIRE_SECT_ALIGN
	if(bytes_to_xfr & (sect_size - 1)) {
		printf("fd%d: PARTIAL SECTOR I/O\n", fvp->volume_num);
	}
#else	REQUIRE_SECT_ALIGN
	if((bytes_to_xfr & (sect_size - 1)) && 
	   !(fvp->io_flags & FVIOF_READ)) {
		if(bytes_to_xfr < sect_size) {
			/* 
			 *
			 * FIXME: can't this come out??
			 *
			 * This sector is short. Copy into fvp->sect_buf.
			 *
			 * There are three ways of dealing with this, depending
			 * on who we are and where the data is.
			 * 
			 * -- if the data is NOT a phys I/O request, it is
			 *    mapped by the kernel's pmap, which we are also
			 *    using now. Just bcopy.
			 * -- if we are in a user task (called from 
			 *    fdstrategy), we can do a copyin.
			 * -- Else (called from fc_thread, via fd_intr()),
			 *    do vm_copy.
			 */
			start_addrs = fvp->sect_buf = kalloc(sect_size);
			bzero(fvp->sect_buf, sect_size);
			if(fvp->dev_ioreq.pmap == pmap_kernel()) {
				/*
				 * easy case; request came from kernel.
				 */
				bcopy(fvp->start_addrs,
					fvp->sect_buf,
					bytes_to_xfr);
			}
			else if(current_task() != kernel_task) {
				/*
				 * called from user task. copyin.
				 */
				copyin(fvp->start_addrs,
					fvp->sect_buf,
					bytes_to_xfr);
			}
			else {
				vm_offset_t vm_a, vm_a1;
				kern_return_t krtn;
				int i;
				char *cp;
#ifdef	KVM_PUNT	
				/*
				 * until we fix the code below...
				 */
				start_addrs = fvp->start_addrs;
				fvp->padded_byte_cnt = bytes_to_xfr;
				bytes_to_xfr = sect_size;
				goto short_write_out;
#endif	KVM_PUNT
				/* 
				 * The hard way. We're in the kernel task.
				 *
				 * Must do a virtual copy from the user's 
				 * address space. Allocate and wire some
				 * kernel vm.
				 */
				
				krtn = vm_allocate(kernel_map,
					(vm_offset_t *)&vm_a,
					round_page(sect_size), 
					TRUE);
				if(krtn) {
					panic("fd_setup_rw_req: vm_allocate"
						" failed");
				}
				krtn = vm_map_pageable(kernel_map, 
					(vm_offset_t)vm_a,
					(vm_offset_t)vm_a + sect_size,
					FALSE);
				if(krtn) {
					panic("fd_setup_rw_req: "
						"vm_map_pageable failed");
				}
				/*
				 * debug: read some of it.
				 */
				XDBG((" kernel virtual data:\n  "));
				for(i=0; i<0x10; i++) {
					XDBG(("0x%x  ", ((char *)vm_a)[i]));
				}
				/*
				 * more debug: see if we can copy to ourself.
				 */
				krtn = vm_allocate(kernel_map,
					(vm_offset_t *)&vm_a1,
					round_page(sect_size), 
					TRUE);
				if(krtn) {
					panic("fd_setup_rw_req: vm_allocate"
						" failed");
				}
				krtn = vm_map_pageable(kernel_map, 
					(vm_offset_t)vm_a1,
					(vm_offset_t)vm_a1 + sect_size,
					FALSE);
				if(krtn) {
					panic("fd_setup_rw_req: "
						"vm_map_pageable failed");
				}
				cp = (char *)vm_a1;
				for(i=0; i<sect_size; i++)
					*cp++ = i;
				/*
				 * this copy HANGS, looping in 
				 * pmap_remove_range()...
				 */
				krtn = vm_map_copy(kernel_map, 	/* dst_map */
					kernel_map,		/* src_map */
					vm_a,			/* dst_addr */
					(vm_size_t)sect_size,
					vm_a1,			/* src_addr */
					FALSE,			/* dst_alloc */
					FALSE);		/* src_destroy */
				if(krtn) {
					printf("fd_setup_rw_req: vm_map_copy "
						" returned %d\n", krtn);	
					panic("fd_setup_rw_req: vm_map_copy"
						" failed");
				}
				
				/*
				 * end debug
				 */
				krtn = vm_map_copy(kernel_map, 	/* dst_map */
					fvp->dev_ioreq.map,	/* src_map */
					vm_a,			/* dst_addr */
					(vm_size_t)bytes_to_xfr,
					fvp->start_addrs,	/* src_addr */
					FALSE,			/* dst_alloc */
					FALSE);		/* src_destroy */
				if(krtn) {
					printf("fd_setup_rw_req: vm_map_copy "
						" returned %d\n", krtn);	
					panic("fd_setup_rw_req: vm_map_copy"
						" failed");
				}
				/*
				 * Now do the physical copy so we can DMA
				 * later.
				 */
				bcopy((void *)vm_a,
					fvp->sect_buf,
					bytes_to_xfr);
				krtn = vm_map_delete(kernel_map, 
					(vm_offset_t)vm_a, 
	    				round_page(vm_a + sect_size));
				if(krtn) {
					panic("fd_setup_rw_req: vm_map_delete"
						" failed");
				}
			} /* in the kernel task */
			/*
			 * In any case, one full sector is DMA'd from the
			 * kernel's pmap.
			 */
			fvp->dev_ioreq.pmap = pmap_kernel();
			fvp->padded_byte_cnt = bytes_to_xfr;
			bytes_to_xfr = sect_size;
			
		}
		else {
			/*
			 * At least one full sector to go before the short
			 * write. Just truncate the current I/O.
			 */
			bytes_to_xfr &= ~(sect_size -1);
		}
	} /* short write */	
#endif	REQUIRE_SECT_ALIGN
	
short_write_out:

#ifdef	FD_BADBLOCK_MAPPING
	/*
	 * CURRENT IMPLEMENTATION PERFORMS NO BAD BLOCK MAPPING. Call 
	 * mapping routines here...
	 */
#else
	fvp->current_sect     = fvp->start_sect;
	fvp->current_byte_cnt = bytes_to_xfr;
	fvp->current_addrs    = start_addrs;
#endif	FD_BADBLOCK_MAPPING
	fd_gen_rw_cmd(fvp,
		&fvp->dev_ioreq,
		fvp->current_sect,
		fvp->current_byte_cnt,
		fvp->current_addrs,
		fvp->io_flags & FVIOF_READ);
	return(FALSE);
	
} /* fd_setup_rw_req() */

/*
 * generate a write or read command in *fdiop (No I/O).
 */
static void fd_gen_rw_cmd(fd_volume_t fvp,
	fd_ioreq_t fdiop,
	u_int sector,
	u_int byte_count,
	caddr_t dma_addrs,
	int read)		/* non-zero ==> read */
{
	struct fd_rw_cmd *cmdp = (struct fd_rw_cmd *)fdiop->cmd_blk;
	struct fd_format_info *fip = &fvp->format_info;
	
	XADDBG(("fd_gen_rw_cmd: sector 0x%x byte_count 0x%x read = %d\n",
		sector, byte_count, read));
	bzero(cmdp, SIZEOF_RW_CMD);
	fd_log_2_phys(fvp, sector, cmdp);	/* assign track, head, sect */
	cmdp->mt = 0;				/* multitrack - always false */
	cmdp->mfm = fip->density_info.mfm;
	cmdp->opcode = read ? FCCMD_READ : FCCMD_WRITE;
	cmdp->hds = cmdp->head;
	/*
	 * controller thread writes drive_sel.
	 */
	cmdp->sector_size = fip->sectsize_info.n;
	/*
	 * eot = the number of the LAST sector to be read/written.
	 */
	cmdp->eot = cmdp->sector + howmany(byte_count,
					   fip->sectsize_info.sect_size) - 1;
	cmdp->gap_length = fip->sectsize_info.rw_gap_length;
	cmdp->dtl = 0xff;
	
	fdiop->density = fip->density_info.density;
	fdiop->timeout = TO_RW;
	fdiop->command = FDCMD_CMD_XFR;
	fdiop->num_cmd_bytes = SIZEOF_RW_CMD;
	fdiop->addrs = dma_addrs;
	fdiop->byte_count = byte_count;
	fdiop->num_stat_bytes = SIZEOF_RW_STAT;
	if(read)
		fdiop->flags |= FD_IOF_DMA_RD;
	else
		fdiop->flags &= ~FD_IOF_DMA_RD;
	
} /* fd_gen_rw_cmd() */

/*******************************************
 *	Standard I/O command routines      *
 *******************************************/
 
/*
 * read disk label and place in *fvp->labelp. Reads are
 * sector_aligned.
 */
int fd_get_label(fd_volume_t fvp)
{
	int try;
	int rtn;
	struct fd_ioreq ioreq;
	int sect = 0;
	int label_sects;
	int resid_sects;
	
	XCDBG(("fd_get_label: volume %d\n", fvp->volume_num));
	if(!fvp->format_info.flags & FFI_FORMATTED)
		return(EIO);
	label_sects = howmany(sizeof(struct disk_label),
			fvp->format_info.sectsize_info.sect_size);
	for(try=0; try<NLABELS; try++) {
		rtn = fd_live_rw(fvp,
				sect,	
				label_sects,
				(caddr_t)fvp->labelp,	
				TRUE,
				&resid_sects);
		if (!rtn && sdchecklabel(fvp->labelp, sect)) {
			simple_lock(&fvp->lock);
			fvp->format_info.flags |= FFI_LABELVALID;
			simple_unlock(&fvp->lock);
			return(0);
		}
		sect += label_sects;
	}
	/*
	 * None of 'em were good.
	 */
	simple_lock(&fvp->lock);
	fvp->format_info.flags &= ~FFI_LABELVALID;
	simple_unlock(&fvp->lock);
	return(1);

} /* fd_get_label() */

/*
 * write *fvp->labelp to disk. Writes are sector-aligned.
 */
int fd_write_label(fd_volume_t fvp)
{
	int try;
	int error=0;
	struct fd_ioreq ioreq;
	int sect = 0;
	int label_sects;
	int resid_sects;
	
	XCDBG(("fd_get_label: volume %d\n", fvp->volume_num));
	if(!fvp->format_info.flags & FFI_FORMATTED)
		return(EIO);
	label_sects = howmany(sizeof(struct disk_label),
			fvp->format_info.sectsize_info.sect_size);
	
	/*
	 * Set label valid so we can use the disk even if we can't write the
	 * label.
	 */
	simple_lock(&fvp->lock);
	fvp->format_info.flags |= FFI_LABELVALID;
	simple_unlock(&fvp->lock);
	fd_setbratio(fvp);	
	for(try=0; try<NLABELS; try++) {
		fvp->labelp->dl_label_blkno = sect;
		if(fd_live_rw(fvp,
				sect,	
				label_sects,
				(caddr_t)fvp->labelp,	
				FALSE,
				&resid_sects))
			error = 1;
		sect += label_sects;
	}
	return(error);

} /* fd_write_label() */

fd_return_t fd_live_rw(fd_volume_t fvp,
	int sector,			/* raw sector # */
	int sect_count,
	caddr_t addrs,			/* src/dest of data */
	boolean_t read,
	int *resid_sects)		/* RETURNED */
{
	/*
	 * execute a read or write command. This kalloc's a buf, sets up
	 * an I/O request in the buf for partition FD_LIVE_PART (the 'live'
	 * device), enqueues the buf via fdstrategy, and does a biowait.
	 */
	struct buf *bp;
	int rtn;
	
	XCDBG(("fd_live_rw: sector %d  sect_count 0x%x  read %d\n",
		sector, sect_count, read));
	bp = kalloc(sizeof(struct buf));
	if(bp == NULL)
		return(ENOMEM);
	bzero(bp, sizeof(struct buf));
	bp->b_blkno = sector;
	bp->b_un.b_addr = addrs;	
	bp->b_bcount = sect_count * fvp->format_info.sectsize_info.sect_size;
	bp->b_flags = read ? B_READ : B_WRITE; 
	bp->b_dev = FD_DEVT(fd_raw_major, fvp->volume_num, FD_LIVE_PART);
	if((rtn = fdstrategy(bp)) == 0) {
	
		/* only biowait() if we could actually start command. In 
		 * polling mode, the command is complete on return from 
		 * fdstrategy. 
		 */
		if (!fd_polling_mode)
			biowait(bp);
	}
	if(fvp->format_info.sectsize_info.sect_size)
		*resid_sects = bp->b_resid /
				fvp->format_info.sectsize_info.sect_size;
	if(bp->b_flags & B_ERROR) {
	    	switch(bp->b_error) {
		    case EIO:
		    	rtn = FDR_MEDIA;
			break;
		    case EROFS:
		    	rtn = FDR_WRTPROT;
			break;
		    case EINVAL:
		    	rtn = FDR_REJECT;
			break;
		    case ENXIO:
		    	rtn = FDR_VOLUNAVAIL;
			break;
		    default:
		    	rtn = FDR_DRIVE_FAIL;
			break;
		}
	}
	kfree(bp, sizeof(struct buf));
	XCDBG(("fd_live_rw: returning %d\n", rtn));
	return(rtn);
} /* fd_live_rw() */

/*
 * Execute a read or write using local_buf. This avoids retries, remapping,
 * and error logging on console.
 */
 
int fd_raw_rw(fd_volume_t fvp,
	int sector,			/* raw sector # */
	int sect_count,
	caddr_t addrs,			/* src/dest of data */
	boolean_t read)
{
	struct fd_ioreq ioreq;

	XDBG(("fd_raw_rw: volume %d sect %d sect_count %d\n",
		fvp->volume_num, sector, sect_count));
	bzero(&ioreq, sizeof(struct fd_ioreq));
	fd_gen_rw_cmd(fvp,
		&ioreq,
		sector,
		sect_count * fvp->format_info.sectsize_info.sect_size,
		addrs,
		read);
	return(fd_command(fvp, &ioreq));	
} /* fd_raw_rw() */

/*
 * execute a Read ID command at specified head. Status (ID read) returned in
 * rw_stat_p.
 */
fd_return_t fd_readid(fd_volume_t fvp, int head, struct fd_rw_stat *statp)
{
	struct fd_ioreq ioreq;
	int rtn;
	struct fd_readid_cmd *cmdp = (struct fd_readid_cmd *)ioreq.cmd_blk;
	
	XDBG(("fd_readid: volume %d\n", fvp->volume_num));
	bzero(&ioreq, sizeof(struct fd_ioreq));
	cmdp->opcode = FCCMD_READID;
	cmdp->mfm = fvp->format_info.density_info.mfm;
	cmdp->hds = head;
	ioreq.density = fvp->format_info.density_info.density;
	ioreq.timeout = TO_RW;
	ioreq.command = FDCMD_CMD_XFR;
	ioreq.num_cmd_bytes = sizeof(struct fd_readid_cmd);
	ioreq.addrs = 0;
	ioreq.byte_count = 0;
	ioreq.num_stat_bytes = SIZEOF_RW_STAT;
	ioreq.flags = 0;
	rtn = fd_command(fvp, &ioreq);
	XDBG(("fd_readid: status = %n\n", ioreq.status, fd_return_values));
	*statp = *((struct fd_rw_stat *)ioreq.stat_blk);
	if(ioreq.status)
		return(ioreq.status);
	else if(rtn)
		return(FDR_REJECT);
	else
		return(FDR_SUCCESS);
} /* fd_readid() */

/*
 * execute a FDCMD_GET_STATUS command. Status returned in *dstatp.
 */
int fd_get_status(fd_volume_t fvp, struct fd_drive_stat *dstatp)
{
	struct fd_ioreq ioreq;
	int rtn;
	
	XDBG(("fd_get_status: volume %d\n", fvp->volume_num));
	bzero(&ioreq, sizeof(ioreq));
	ioreq.command = FDCMD_GET_STATUS;
	ioreq.timeout = TO_SIMPLE;
	rtn = fd_command(fvp, &ioreq);
	XDBG(("fd_get_status: status = %n\n", ioreq.status, fd_return_values));
	*dstatp = ioreq.drive_stat;
	return(rtn);
}

/*
 * execute recalibrate command.
 */
int fd_recal(fd_volume_t fvp)
{
	struct fd_ioreq ioreq;
	int rtn;
	
	XDBG(("fd_recal: volume %d\n", fvp->volume_num));
	fd_gen_recal(fvp, &ioreq);
	rtn = fd_command(fvp, &ioreq);
	XDBG(("fd_recal: status = %n\n", ioreq.status, fd_return_values));
	return(rtn);
}

/*
 * execute seek command.
 */
int fd_seek(fd_volume_t fvp, int cyl, int head)
{
	struct fd_ioreq ioreq;
	int rtn;
	
	XDBG(("fd_seek: volume %d\n", fvp->volume_num));
	bzero(&ioreq, sizeof(struct fd_ioreq));
	fd_gen_seek(fvp->format_info.density_info.density, &ioreq, cyl, head);
	rtn = fd_command(fvp, &ioreq);
	XDBG(("fd_seek: status = %n\n", ioreq.status, fd_return_values));
	return(rtn);
}

/*
 * send a simple command (like FDCMD_NEWVOLUME or FDCMD_EJECT) to controller
 * thread.
 *
 * fvp->drive_num must be valid to route this request to the proper 
 * controller thread; otherwise the request will be mapped by v2d_map.
 */
int fd_basic_cmd(fd_volume_t fvp, int command)
{
	struct fd_ioreq ioreq;
	int rtn;
	
	XDBG(("fd_basic_cmd: command %n\n", command, fd_command_values));
	bzero(&ioreq, sizeof(ioreq));
	ioreq.command = command;
	ioreq.timeout = TO_SIMPLE;
	rtn = fd_command(fvp, &ioreq);
	if(rtn == FDR_SUCCESS)
		rtn = ioreq.status;
	XDBG(("fd_basic_cmd: returning = %n\n", rtn, fd_return_values));
	return(rtn);

} /* fd_basic_cmd() */


/*
 * generate a recalibrate command in *fdiop.
 */
static void fd_gen_recal(fd_volume_t fvp, fd_ioreq_t fdiop)
{
	struct fd_recal_cmd *cmdp = (struct fd_recal_cmd *)fdiop->cmd_blk;
	
	cmdp->opcode = FCCMD_RECAL;
	cmdp->rsvd1 = 0;
	cmdp->drive_sel = 0;			/* to be changed by controller
						 * code */
	fdiop->density = fvp->format_info.density_info.density;
	fdiop->timeout = TO_RECAL;
	fdiop->command = FDCMD_CMD_XFR;
	fdiop->num_cmd_bytes = sizeof(struct fd_recal_cmd);
	fdiop->addrs = 0;
	fdiop->byte_count = 0;
	fdiop->num_stat_bytes = sizeof(struct fd_int_stat);
} /* fd_gen_recal() */

/*
 * generate a seek command to specified track in *fdiop.
 */
void fd_gen_seek(int density, fd_ioreq_t fdiop, int cyl, int head)
{
	struct fd_seek_cmd *cmdp = 
		(struct fd_seek_cmd *)fdiop->cmd_blk;
	
	cmdp->opcode = FCCMD_SEEK;
	cmdp->relative = 0;
	cmdp->dir = 0;
	cmdp->rsvd1 = 0;		/* cheaper than a bzero */
	cmdp->hds = head;
	cmdp->cyl = cyl;
	fdiop->density = density;
	fdiop->timeout = TO_SEEK;
	fdiop->command = FDCMD_CMD_XFR;
	fdiop->num_cmd_bytes = SIZEOF_SEEK_CMD;
	fdiop->addrs = 0;
	fdiop->byte_count = 0;
	fdiop->num_stat_bytes = sizeof(struct fd_int_stat);

} /* fd_gen_seek() */

/*****************************************
 * 	miscellaneous subroutines        *
 *****************************************/
/*
 * convert logical sector # into cylinder, head, sector. No range checking.
 * Physical parameters are obtained from *fvp->labelp and fvp->sects_per_track.
 *
 * First sector on a track is sector 1.
 */
static void fd_log_2_phys(fd_volume_t fvp,
	u_int sector,
	struct fd_rw_cmd *cmdp)
{
	u_int track;
	struct fd_format_info *fip = &fvp->format_info;
	
	ASSERT((fip->sectsize_info.sects_per_trk != 0) && 
	       (fip->disk_info.tracks_per_cyl != 0));	
	track = sector / fip->sectsize_info.sects_per_trk;
	cmdp->cylinder = track / fip->disk_info.tracks_per_cyl;
	cmdp->head     = track % fip->disk_info.tracks_per_cyl;
	cmdp->sector   = sector % fip->sectsize_info.sects_per_trk + 1;
	XADDBG(("fd_log_2_phys: lsect 0x%x; cyl 0x%x  head 0x%x  sector "
		"0x%x\n", sector, cmdp->cylinder, cmdp->head, cmdp->sector-1));
}

/*
 * create a new volume struct for the specified volume #.
 */
fd_volume_t fd_new_fv(int vol_num)
{
	fd_volume_t fvp;
	
	fvp = kalloc(sizeof(struct fd_volume));
	if(fvp == NULL)
		panic("fd_new_fv: Couldn't kalloc fd_volume");
	bzero(fvp, sizeof(struct fd_volume));
	simple_lock_init(&fvp->lock);
	fvp->drive_num = DRIVE_UNMOUNTED;	/* i.e., not inserted */
	fvp->volume_num = vol_num;
	/*
	 * Reserve space for a label using largest legal sector size.
	 */
	fvp->labelp = kalloc(FD_SECTALIGN(sizeof(struct disk_label),
			     fd_sectsize_max()));
	if(fvp->labelp == NULL)
		panic("fd_new_fv: Couldn't kalloc disk_label");
	fvp->format_info.density_info.density = FD_DENS_NONE;
	fvp->format_info.flags = 0;
	ASSERT(DMA_ENDALIGNED(fvp->labelp));
	disksort_init(&fvp->io_queue);
	return(fvp);
} /* fd_new_fv() */

/*
 * free a volume struct and everything in it.
 */
void fd_free_fv(fd_volume_t fvp)
{
	XDBG(("fd_free_fv: freeing volume %d\n", fvp->volume_num));
	ASSERT(fvp->volume_num <= NUM_FVP);
	fd_volume_p[fvp->volume_num] = NULL;
	kfree(fvp->labelp, FD_SECTALIGN(sizeof(struct disk_label),
			     fd_sectsize_max()));
	disksort_free(&fvp->io_queue);
	kfree(fvp, sizeof(struct fd_volume));
} /* fd_free_fv() */

/*
 * Assign specified drive to specified volume number. During normal operation,
 * this should only be called by the volume_check thread.
 */
void fd_assign_dv(fd_volume_t fvp, int drive_num)
{
	fd_drive_t fdp = &fd_drive[drive_num];
	
	XDBG(("fd_assign: volume %d drive %d\n", fvp->volume_num, drive_num));	 
	ASSERT(drive_num < NFD);
	ASSERT(fdp->flags & FDF_PRESENT);
	fvp->drive_num = drive_num;
	fvp->bdp = fdp->bdp;
	fdp->fvp = fvp;
	/*
	 * Nullify possible "requested disk not available" condition.
	 */
	simple_lock(&fvp->lock);
	fvp->flags &= ~FVF_NOTAVAIL;
	simple_unlock(&fvp->lock);
}

void v2d_map(fd_volume_t fvp)
{
	/*
	 * a routine responsible for volume-to-drive mapping. Called by 
	 * controller thread when fvp->drive_num == DRIVE_UNMOUNTED. This
	 * routine figures out where to do the I/O, then either
	 *
	 * -- has the disk_eject thread sync, clean, and eject the disk
	 *    currently in the drive, or
	 * -- puts up an alert panel asking for the desired disk.
	 *
	 * The I/O request which was passed is placed on fcp->holding_q for the
	 * controller to which the intended drive is connected.
	 *
	 * Not used during polling - drive level figures that out on its own.
	 *
	 * NOTE: the final act of mapping a volume to a drive - calling
	 * fd_assign_dv() - is NOT done here. That's done when the desired
	 * disk is finally inserted, by the volume_check thread. The most this
	 * routine can do is set the fvp->intended_drive field.
	 */
	queue_head_t *qhp;
	fd_controller_t fcp;
	fd_volume_t held_vol;
	fd_drive_t fdp;
	int i;
	struct tsval oldest_tsval;
	int lru_drive;
	struct disk_eject_req *derp;
	
	/*
	 * first see if any other I/O requests for this volume are already in
	 * any holding_q. If so, just enqueue this one on the holding_q as
	 * well (we've already done the grunt work in this case).
	 */
	XDBG(("v2d_map: vol %d\n", fvp->volume_num));
	fcp = &fd_controller[0];
	for(i=0; i<NFC; i++) {
		simple_lock(&fcp->holding_lock);
		qhp = &fcp->holding_q;
		held_vol = (fd_volume_t)queue_first(qhp);
		while(!queue_end(qhp, (queue_t)held_vol)) {
			if(held_vol->volume_num == fvp->volume_num) {
				XDBG(("v2d_map: existing entry in"
				     " holding_q; intended_drive = %d\n",
				     held_vol->intended_drive));
				queue_enter(qhp,
					fvp,
					fd_volume_t,
					io_link);
				simple_unlock(&fcp->holding_lock);
				return;
			}
			else
				held_vol = (fd_volume_t)held_vol->io_link.next;
		}
		simple_unlock(&fcp->holding_lock);
		fcp++;
	} /* for each controller's holding_q */

	/*
	 * FIXME: look for invalid volumes (volumes in drives, but not open)
	 */
	/*
	 * Try to find a drive which has no disk inserted and into which we're 
	 * not currently expecting a disk to be inserted.
	 */
	fdp = &fd_drive[0];
	for(i=0; (i<NFD) && (fdp->flags & FDF_PRESENT); i++, fdp++) {
		if((fdp->fvp == NULL) && (fdp->intended_fvp == NULL)) {
			/*
			 * found one. Assign it; put current request in
			 * appropriate holding_q, and put up an alert panel.
			 */
			XDBG(("v2d_map: empty drive (#%d)\n", i));
			/*
			 * Keep all volume assignment synchronous - let the
			 * vol_check thread put up the panel (01-Nov-90 dpm).
			 */
			goto make_eject_request;
		}
	} /* for each drive */
	
	/*
	 * Well, we have to kick a disk out. Find the one that's been least
	 * recently used.
	 */
	oldest_tsval = fd_drive[0].last_access;
	lru_drive = 0;
	fdp = &fd_drive[0];
	for(i=0; (i<NFD) && (fdp->flags & FDF_PRESENT); i++, fdp++) {
		if(ts_greater(&oldest_tsval, &fdp->last_access)) {
			oldest_tsval = fdp->last_access;
			lru_drive = i;
		}
	}
	/*
	 * have fd_disk_eject sync, clean, and eject the disk in
	 * lru_drive. fd_disk_eject will put up an alert panel asking
	 * for a new disk when the eject is complete. 
	 *
	 * Note a special case - the drive has no currentvolume, but has an
	 * intended_fvp - in that case, pass a NULL fvp to disk_eject; it will
	 * put up an alert panel. (This happens when requests are outstanding
	 * for two or more different unmounted volumes.)
	 */
	fdp = &fd_drive[lru_drive];
	XDBG(("v2d_map: ejecting vol %d from drive %d\n",
		fdp->fvp->volume_num, lru_drive));
make_eject_request:
	derp = kalloc(sizeof(struct disk_eject_req));
	ASSERT(derp != NULL);
	derp->fvp = fdp->fvp;		/* current volume - may be NULL! */
	derp->new_fvp = fvp;		/* new volume */
	queue_enter(&disk_eject_q,
		derp,
		struct disk_eject_req *,
		q_link);
	/*
	 * Finally, enqueue this request on the appropriate controller's
	 * holding_q and flag intended_drive.
	 */
	simple_lock(&fdp->fcp->holding_lock);
	queue_enter(&fdp->fcp->holding_q,
		fvp,
		fd_volume_t,
		io_link);
	simple_unlock(&fdp->fcp->holding_lock);
	simple_lock(&fvp->lock);
	fvp->flags |= FVF_ALERT;
	simple_unlock(&fvp->lock);
	fvp->intended_drive = fdp - fd_drive;
	return;
} /* v2d_map */

/*
 * tsval functions (should be in the kernel somewhere...)
 */

void ts_diff(struct tsval *ts1, struct tsval *ts2, struct tsval *ts_diff)
{
	struct tsval  *ts_temp;
	
	/* returns abs(ts1 - ts2) in ts_diff */
	
	if(ts_greater(ts2, ts1)) {
		ts_temp = ts1;
		ts1 = ts2;
		ts2 = ts_temp;
	}
	/* ts1 >= ts2 */
	if(ts2->low_val > ts1->low_val)
		ts1->high_val--;		/* borrow */
	ts_diff->low_val  = ts1->low_val  - ts2->low_val;
	ts_diff->high_val = ts1->high_val - ts2->high_val;	 
}

boolean_t ts_greater(struct tsval *ts1, struct tsval *ts2) 
{
	if(ts1->high_val == (ts2)->high_val)	
		return(ts1->low_val > ts2->low_val);
	else
		return(ts1->high_val > ts2->high_val);
}

void ts_add(struct tsval *ts1, u_int micros)
{
	/* 
	 * add specified # of microseconds to ts1.
	 */
	u_int old_low_val = ts1->low_val;
	
	ts1->low_val += micros;
	if(ts1->low_val < old_low_val)
		ts1->high_val++;		/* carry */
}

/*
 * This function keeps getting called as a timeout until the kernel is
 * running; at that point, we start up threads.
 */
void fc_thread_timer() 
{
	int i;
	
	if(kernel_task && !fc_thread_init) {
		kernel_thread_noblock(kernel_task, volume_check);
		for(i=0; i<NFC; i++) {
			if(!(fd_controller[i].flags & FCF_THR_ALIVE))
				kernel_thread_noblock(kernel_task, fc_thread);
		}
		fc_thread_init = TRUE;
	}
	else
		timeout((int (*)())fc_thread_timer, 0, hz);	/* try again */
} /* fc_thread_timer() */

/*
 * Common way for all driver threads to block. Waits for any event_bits in
 * *eventp to be set, using *lockp to protect accesses to *eventp.
 */
void fd_thread_block(int *eventp, int event_bits, simple_lock_t lockp)
{	
	int s;
	
	XTBDBG(("fd_thread_block: event = 0x%x  event_bits = 0x%x\n",
		*eventp, event_bits));
	s = splbio();
	simple_lock(lockp);
	while((*eventp & event_bits) == 0) {
		assert_wait((int)eventp, FALSE);
		simple_unlock(lockp);
		splx(s);
		thread_block();
		s = splbio();
		simple_lock(lockp);
	}
	simple_unlock(lockp);
	XTBDBG(("fd_thread_block returning: event = 0x%x  event_bits = 0x%x\n",
		*eventp, event_bits));
	splx(s);
}

void fd_set_density_info(fd_volume_t fvp, int density)
{
	/*
	 * Set up fvp->format_info.density_info per density. Density = 
	 * FD_DENS_NONE marks the disk as unformatted.
	 */
	struct fd_density_info *deip;
	int sect_size;
	
	/*
	 * Must remember current sector size and reset sectsize_info
	 * after changing density, because sectsize_info is dependent
	 * on density.  If sector size hasn't been set yet, all is ok
	 * since fd_set_sector_size() will ignore bogus requests.
	 */
	sect_size = fvp->format_info.sectsize_info.sect_size;
	for(deip = fd_density_info; 
	    deip->density != FD_DENS_NONE;
	    deip++) {
	    	if(density == deip->density) 
			break;
	}
	fvp->format_info.density_info = *deip;
	fd_set_sector_size(fvp, sect_size);
	if(density == FD_DENS_NONE) {
		simple_lock(&fvp->lock);
		fvp->format_info.flags &= ~FFI_FORMATTED;
		simple_unlock(&fvp->lock);
	}
}
			
int fd_set_sector_size(fd_volume_t fvp, int sect_size)
{
	struct fd_format_info *fip = &fvp->format_info;
	struct fd_sectsize_info *ssip;
	
	/*
	 * Validate legal sector size. 
	 */
	for (ssip=fd_get_sectsize_info(fvp->format_info.density_info.density);
	    ssip->sect_size;
	    ssip++) {
	    	if(sect_size == ssip->sect_size) {
			break;
		}
	}
	if(! ssip->sect_size)
		return(1);		/* illegal sector size */
	fip->sectsize_info = *ssip;
	fip->total_sects = fip->sectsize_info.sects_per_trk *
			    fip->disk_info.tracks_per_cyl * 
			    fip->disk_info.num_cylinders;
	simple_lock(&fvp->lock);
	fip->flags |= FFI_FORMATTED;
	fip->flags &= ~FFI_LABELVALID;
	simple_unlock(&fvp->lock);
	return(0);
}

struct fd_sectsize_info *fd_get_sectsize_info(int density)
{
	struct fd_density_sectsize *dsp;

	/*
	 * Find appropriate sector info for density
	 */
	for (dsp = fd_density_sectsize; dsp->ssip; dsp++)
		if (dsp->density == density)
			return dsp->ssip;
	panic("fd_sectsize_info: bad density");
}

void fd_setbratio(fd_volume_t fvp)
{
	/*
	 * set fvp->bratio based on sect_size.
	 */
	struct disk_label *dlp;
	int dk, rps;
	struct fd_format_info *fip = &fvp->format_info;
	
	dlp = fvp->labelp;
	ASSERT(fvp->format_info.flags & FFI_LABELVALID);
	if (dlp->dl_secsize < fip->sectsize_info.sect_size
	    || dlp->dl_secsize % fip->sectsize_info.sect_size) {
	    	simple_lock(&fvp->lock);
		fvp->format_info.flags &= ~FFI_LABELVALID;
	    	simple_unlock(&fvp->lock);
		printf("FS BLOCK NOT MULTIPLE OF DEVICE BLOCK\n");
		printf("FS block: %d, DEV block: %d\n",
		    dlp->dl_secsize, fip->sectsize_info.sect_size);
		return;
	}
	/*
	 * FIXME:  Some day, we should tell the FS code
	 * this, rather than have it built in!
	 */
	if (dlp->dl_secsize != DEV_BSIZE) {
	    	simple_lock(&fvp->lock);
		fvp->format_info.flags &= ~FFI_LABELVALID;
	    	simple_unlock(&fvp->lock);
		printf("FS BLOCK NOT EQUAL TO DEV_BSIZE\n");
		printf("FS block: %d, DEV_BSIZE: %d\n", dlp->dl_secsize,
		    DEV_BSIZE);
		return;
	}
	fvp->bratio = dlp->dl_secsize / fip->sectsize_info.sect_size;
	if ((dk = fvp->bdp->bd_dk) >= 0) {
		rps = dlp->dl_rpm >= 60 ? dlp->dl_rpm / 60 : 60;
		dk_bps[dk] = dlp->dl_secsize * dlp->dl_nsect * rps;
	}

} /* fd_setbratio() */

/**************************************
 *		threads               *
 **************************************/
 
/* volume_check thread
 *
 * This thread is responsible for two tasks:
 *
 * -- check for newly inserted disks (normally once per second), assigning
 *    them to the appropriate volume, and notifying the controller thread and
 *    other interested parties (like the Automounter daemon) of the change.
 * -- Turn off motor for drive which has been idle more than 2 seconds.
 *
 * One volume_check thread per system. This thread is the only thread which can
 * alter volume-to-drive mappings (other than at autoconfig time).
 */
void volume_check()
{
	fd_volume_t vc_fvp;
	fd_drive_t fdp;
	int drive;
	int volume;
	fd_volume_t volp;
	struct fd_drive_stat drive_status;
	int rtn;
	struct tsval curtime, oldtime;
	struct timeval tv;
 	queue_head_t *qhp;
	struct vol_abort_req *varp, *next_varp;
	
	/*
	 * Create a volume struct for all I/O used by this thread.
	 */
	vc_fvp = fd_volume_p[FVP_VOLCHECK] = fd_new_fv(FVP_VOLCHECK);
	vol_check_alive = 1;
	
	while(1) {
	    /* 
	     * Handle any pending abort requests.
	     */
	    qhp = &vol_abort_q;
	    simple_lock(&vol_abort_lock);
	    varp = (struct vol_abort_req *)queue_first(qhp);
	    while(!queue_end(qhp, (queue_t)varp)) {
	    	    fd_drive_t intended_fdp;
		    fd_volume_t intended_fvp;
		    
		    XDBG(("volume_check: ABORT REQUEST vol %d\n", 
			    varp->fvp->volume_num));
		    next_varp = (struct vol_abort_req *)(varp->link.next);
		    queue_remove(&vol_abort_q, varp, struct vol_abort_req *, 
		    	link);
		    intended_fvp = varp->fvp;
		    
		    /*
		     * Skip this whole thing if the volume has been inserted 
		     * (this is a race condition in which the "volume present"
		     * situation should prevail).
		     */
		    if(!(intended_fvp->flags & FVF_ALERT))
		    	goto abort_end;
		    intended_fdp = &fd_drive[intended_fvp->intended_drive];
		    simple_lock(&intended_fvp->lock);
		    intended_fvp->flags |= FVF_NOTAVAIL;
		    intended_fvp->flags &= ~FVF_ALERT;
		    /*
		     * decouple the intended drive and fvp.
		     */
		    vc_fvp->drive_num = intended_fvp->intended_drive;
		    intended_fdp->intended_fvp = NULL;
		    intended_fvp->intended_drive = DRIVE_UNMOUNTED;
		    simple_unlock(&intended_fvp->lock);
		    /*
		     * Send an abort command to the controller thread
		     * associated with this volume, then free the abort 
		     * request.
		     */
		    fd_basic_cmd(vc_fvp, FDCMD_ABORT);
abort_end:
		    kfree(varp, sizeof(*varp));
		    varp = next_varp;
	    }
	    simple_unlock(&vol_abort_lock);
	    
	    /* 
	     * Handle new eject requests.
	     */
	    fd_disk_eject();

	    /*
	     * check each drive that currently has no volume for a newly
	     * inserted disk.
	     */
	    fdp = &fd_drive[0];
	    for(drive=0; 
	       (drive<NFD) && (fdp->flags & FDF_PRESENT); 
	        drive++, fdp++) {
		if(fdp->fvp == NULL) {
		    vc_fvp->drive_num = drive;
		    /*
		     * FIXME: an optimization is possible here. We could just 
		     * go out and look at the hardware without waking up
		     * the controller thread...do we want to do that???
		     * (Possible race conditions??? let's leave it alone
		     * for now...)
		     */
		    rtn = fd_get_status(vc_fvp, &drive_status);
		    if(rtn)
			    goto next_drive;	/* I/O error - skip */
		    if(drive_status.media_id == FD_MID_NONE) 
			    goto next_drive;	/* still no disk - skip */
			
		    /*
		     * new disk inserted. Try to attach (no mapping will occur
		     * by v2d_map() since we have assigned this drive to
		     * our fd_volume).
		     */
		    XDBG(("volume_check: New disk\n"));
		    vc_fvp->format_info.flags = 0;	
		    /* not formatted; no label */
		    if(fd_attach_com(vc_fvp)) {
			    XDBG(("volume_check: fd_attach_com FAILED\n"));
			    /* 
			     * The only ways this can fail are gross
			     * controller failure or timeout on Read ID, which
			     * means No Disk.
			     */
			    goto next_drive;
		    }
		    /*
		     * The first thing to check is whether we are 
		     * expecting a new disk in this drive. 
		     */
		    if(fdp->intended_fvp != NULL) {
		    	fd_volume_t intended_fvp = fdp->intended_fvp;
			int vol_num;
			fd_volume_t foo_fvp;
			
			XDBG(("vol_check: DISK INSERT EXPECTED\n"));
		    	if(intended_fvp->flags & FVF_NEWVOLUME) {
			    /*
			     * We are expecting a new volume. Make sure that
			     * this isn't a volume we already know about.
			     */
			    XDBG(("vol_check: NEW VOLUME REQUEST\n"));
			    for(vol_num=0; vol_num<NUM_FV; vol_num++) {
				foo_fvp = fd_volume_p[vol_num];
				if(foo_fvp == NULL) 
				    continue;
				if(vc_compare_vol(vc_fvp, foo_fvp) == 0) {
				    XDBG(("vol_check: KNOWN DISK;"
					"EXPECTED NEW\n"));
				    fd_basic_cmd(vc_fvp, FDCMD_EJECT);
				    /*
				     * Remove existing alert panel and put 
				     * up a more insistent one.
				     */
				    if(intended_fvp->flags & FVF_ALERT) {
					vol_panel_remove(intended_fvp->
							    vol_tag);
					fd_alert_panel(intended_fvp, TRUE);
				    }
				    goto next_drive;
				}
			    } /* scanning fvp's */
			    
			    /*
			     * Well, it must be OK. We are looking for a new 
			     * volume and this disk doesn't look like anything
			     * we know about. Copy over everything we learned
			     * about this volume to the intended fvp, then
			     * assign it to this drive.
			     */
			    XDBG(("vol_check: NEW VOLUME ASSIGN vol %d\n",
			    	intended_fvp->volume_num));
			    fd_copy_vol(vc_fvp, intended_fvp);
			    vol_check_assign(intended_fvp, fdp, vc_fvp);
			    goto next_drive;
			} /* expecting new volume */
		        else {
			    /*
			     * Not a new volume, so we require a perfect match
			     * between the new disk and the known (intended) 
			     * volume.
			     */
			    XDBG(("vol_check: EXPECTING KNOWN VOLUME\n"));
			    if(vc_compare_vol(vc_fvp, intended_fvp) == 0) {
				XDBG(("vol_check: MATCH - expected vol %d\n",
				    intended_fvp->volume_num));
				/*  
				 * Write protect may have changed while disk
				 * was out; update.
				 */
				fdp->intended_fvp->format_info.flags = 
				           vc_fvp->format_info.flags;
				vol_check_assign(fdp->intended_fvp, fdp,
						vc_fvp);
			    }
			    else {
				XDBG(("vol_check: WRONG VOLUME\n"));
				fd_basic_cmd(vc_fvp, FDCMD_EJECT);
				/*
				 * Remove existing alert panel and put 
				 * up a more insistent one.
				 */
				if(intended_fvp->flags & FVF_ALERT) {
				    vol_panel_remove(intended_fvp->vol_tag);
				    fd_alert_panel(intended_fvp, TRUE);
				}
			    }
			    goto next_drive;			
			}
		    } /* expected a disk insertion */
		    
		    /*
		     * The user inserted a disk without being asked to. See
		     * if we recognize the disk.
		     */
		    XDBG(("vol_check: UNPROMPTED DISK INSERTION\n"));
		    for(volume=0; volume<NUM_FV; volume++) {
			volp = fd_volume_p[volume];
			if(volp == NULL)
			    continue;
			if(vc_compare_vol(vc_fvp, volp) == 0) {
			    XDBG(("volume_check: KNOWN VOLUME vol "
				    "%d drive %d\n", volume,
				    drive));
			    ASSERT(volp->drive_num == DRIVE_UNMOUNTED);
			    /*
			     * We have already seen this volume; re-assign it
			     * to this drive. Write protect may have changed 
			     * while disk was out; update.
			     */
			    volp->format_info.flags = 
			  	vc_fvp->format_info.flags;
			    vol_check_assign(volp, fdp, vc_fvp);
			    goto next_drive;
			} /* match  */
		    } /* looking for a matching volume */
		    
		    /*
		     * New disk. We're not expecting it, and we've never seen
		     * it.
		     *
		     * Find first unopened volume; allocate a new volume 
		     * struct; copy over format info we learned during attach.
		     */
		    for(volume=0; volume<NUM_FV; volume++) {
			if(fd_volume_p[volume] == NULL) {
			    XDBG(("volume_check: NEW VOLUME = %d\n", volume));
			    volp = fd_volume_p[volume] = fd_new_fv(volume);
			    fd_assign_dv(volp, drive);
			    fd_copy_vol(vc_fvp, volp);
			    /*
			     * notify automounter/boot program.
			     */
			    volume_notify(volp);
			    goto next_drive;
			} /* found a null volume */
		    } /* looking for null volumes */
		    /*
		     * FIXME??
		     */
		    printf("fd_volume_check: NO FREE VOLUMES\n");
		    fd_basic_cmd(vc_fvp, FDCMD_EJECT);
		} /* drive currently has no volume */
next_drive:
		continue;
	    } /* for each drive */

	    /*
	     * Now check each drive to see if it has been idling, with the
	     * motor on, for greater than TO_IDLE ms. If so, turn its
	     * motor off. 
	     *
	     * NOTE: a "feature" of the Sony drive is that we can't read Media
	     * ID unless the motor is on. Therefore we have to leave the
	     * motor on for drives with no disks, so we can detect disk 
	     * insertion above. The motor doesn't actually spin in this case,
	     * but it's bullshit anyway.
	     */
	    fdp = &fd_drive[0];
	    for(drive=0; drive<NFD; drive++, fdp++) {
		    if((fdp->flags&(FDF_PRESENT|FDF_MOTOR_ON|FDF_EJECTING)) !=
		    	 	   (FDF_PRESENT|FDF_MOTOR_ON))
			continue;			/* no drive, already
							 * off, or ejecting */
	            if(fdp->fvp == NULL)
		    	continue;			/* no volume */
		    event_set_ts(&curtime);
		    oldtime = fdp->last_access;
		    ts_add(&oldtime, TO_IDLE * 1000);
		    if(ts_greater(&curtime, &oldtime)) {
			    vc_fvp->drive_num = drive;
			    XDBG(("volume_check: MOTOR OFF drive %d\n", 
				    drive));
			    fd_basic_cmd(vc_fvp, FDCMD_MOTOR_OFF);
		    }
	    } /* for each drive */
		
	    /*
	     * sleep. Normally we sleep for one second, but it could be 
	     * less if controller or disk_eject thread has put up a "please 
	     * insert disk" panel.
	     */
	    if(vol_check_delay >= 1000000) {
		    tv.tv_sec  = vol_check_delay / 1000000;
		    tv.tv_usec = vol_check_delay % 1000000;
	    }
	    else {
		    tv.tv_sec  = 0;
		    tv.tv_usec = vol_check_delay;
	    }
	    vol_check_event = 0;
	    us_timeout((int (*)())vol_check_timeout, 
		    NULL, 
		    &tv, 
		    CALLOUT_PRI_SOFTINT0);	
	    XTBDBG(("volume_check: calling fd_thread_block\n"));
	    fd_thread_block(&vol_check_event, 0xff, &vol_check_lock);
	    XTBDBG(("volume_check: returning from fd_thread_block\n"));
	} /* main loop */
	/* NOT REACHED */
} /* volume_check thread */

/*
 * Assign given drive to given fvp; notify controller of new disk;
 * remove alert panel if present.
 */
static void vol_check_assign(fd_volume_t fvp, 	/* vol to be assigned */
	fd_drive_t fdp,				/* drive be assigned */
	fd_volume_t vc_fvp)			/* vol_check's fvp for I/O */
{
	int drive_num = fdp - fd_drive;
	
#ifdef	DEBUG
	printf("fd: vol %d assigned to drive %d\n", 
		fvp->volume_num, drive_num);
#endif	DEBUG
	fd_assign_dv(fvp, drive_num);
	fvp->intended_drive = DRIVE_UNMOUNTED;
	simple_lock(&fvp->lock);
	fvp->flags &= ~FVF_NEWVOLUME;
	if(fvp->flags & FVF_ALERT) {
	    	fvp->flags &= ~FVF_ALERT;
		simple_unlock(&fvp->lock);
#ifdef	DEBUG
	    	printf("Alert Panel removed for volume %d\n", fvp->volume_num);
#endif	DEBUG
		vol_panel_remove(fvp->vol_tag);
	}
	else
		simple_unlock(&fvp->lock);
	fd_basic_cmd(vc_fvp, FDCMD_NEWVOLUME);
	fdp->intended_fvp = NULL;
}

static int vc_compare_vol(fd_volume_t fvp1, fd_volume_t fvp2)
{
	/*
	 * Determine if these fvps refer to the same media (to the best of
	 * our ability). Returns 0 if same, else 1.
	 */
	XDBG(("vol_compare_vol\n"));
	if(fvp1->format_info.disk_info.media_id != 
	   fvp2->format_info.disk_info.media_id)
		return(1);
	if((fvp1->format_info.flags & (FFI_LABELVALID | FFI_FORMATTED)) !=
	   (fvp2->format_info.flags & (FFI_LABELVALID | FFI_FORMATTED)))
	   	return(1);
	if(fvp1->format_info.flags & FFI_LABELVALID) {
		if(fvp1->labelp->dl_tag != fvp2->labelp->dl_tag)
			return(1);
		if(strncmp(fvp1->labelp->dl_label,
			   fvp2->labelp->dl_label,
			   MAXLBLLEN))
			return(1);
	}
	if(fvp1->format_info.flags & FFI_FORMATTED) {
	    	if(fvp1->format_info.sectsize_info.sect_size !=
		     fvp2->format_info.sectsize_info.sect_size)
		    	return(1);
	    	if(fvp1->format_info.density_info.density != 
		   fvp2->format_info.density_info.density)
		    	return(1);
	}
	XDBG(("vol_compare_vol: MATCH\n"));
	return(0);
} /* vc_compare_vol() */

void volume_notify(fd_volume_t fvp)
{
	int vol_state;
	char form_type[FORM_TYPE_LENGTH];
	int density;
	int kbytes;
	char one_dens[20];
	char dev_str[OID_DEVSTR_LEN];
	int flags;
	
	form_type[0] = '\0';
	if(fvp->format_info.flags & FFI_FORMATTED) {
		if(fvp->format_info.flags & FFI_LABELVALID) 
			vol_state = IND_VS_LABEL;
		else 
			vol_state = IND_VS_FORMATTED;
	}
	else {
		vol_state = IND_VS_UNFORMATTED;
	}
	/*
	 * Encode all legal densities at which this disk can be 
	 * formatted.
	 */
	for(density=FD_DENS_1; 
	    density <= fvp->format_info.disk_info.max_density;
	    density++) {
		kbytes = dens_to_capacity(density) / 1024;	 	   
		sprintf(one_dens, "%d ", kbytes);
		strcat(form_type, one_dens);
	}
	/*
	 * Generate the entry in /dev/.
	 */
	sprintf(dev_str, "fd%d", fvp->volume_num);
	flags = IND_FLAGS_REMOVABLE;
	if(fvp->format_info.flags & FFI_WRITEPROTECT)
		flags |= IND_FLAGS_WP;
	vol_notify_dev(FD_DEVT(fd_blk_major, fvp->volume_num, 0), 
		FD_DEVT(fd_raw_major, fvp->volume_num, 0),
		form_type,
		vol_state,
		dev_str,
		flags);
}

static int dens_to_capacity(int density)
{
	struct fd_density_info *deip;
	
	for(deip = fd_density_info; 
	    deip->density != FD_DENS_NONE;
	    deip++) {
	    	if(density == deip->density) 
			return(deip->capacity);
	}
	return(0);
}

void vol_check_timeout()
{
	int s;
	
	s = splbio();
	if(vol_check_alive) {
		simple_lock(&vol_check_lock);
		vol_check_event = 1;
		thread_wakeup(&vol_check_event);
		simple_unlock(&vol_check_lock);
	}
	splx(s);
}

static void fd_copy_vol(fd_volume_t source_vol, fd_volume_t dest_vol)
{
	/*
	 * Copy format/label info from one volume to another.
	 */
    	dest_vol->format_info = source_vol->format_info;
    	dest_vol->bratio = source_vol->bratio;
	*dest_vol->labelp = *source_vol->labelp;
}

/*
 * fd_disk_eject()
 *
 * This routine takes as its input fd_volume_t's, enqueued on disk_eject_q.
 * The routine does the following for requested volumes:
 * -- for the requested drive's intended_drive (if any):
 *    -- do a file system sync for the volume currently intended_drive
 *    -- eject the volume
 * -- put up alert panel asking for another disk (this thread is only invoked
 *    when the controller thread is kicking out a still-mounted disk in order
 *    to make room for another disk.)
 */
void fd_disk_eject() {

	struct disk_eject_req *derp;
	queue_head_t *qhp = &disk_eject_q;
	fd_volume_t target_fvp;
	fd_drive_t intended_fdp;
	
		if(queue_empty(qhp))
			return;
		derp = (struct disk_eject_req *)queue_first(qhp);
		
		/*
		 * Blow off this eject/panel request if the drive in question 
		 * is already waiting for a volume.
		 */
		intended_fdp = &fd_drive[derp->new_fvp->intended_drive];
		if(intended_fdp->intended_fvp)
		{
			XDBG(("disk_eject: drive already waiting for vol"
			 "%d\n", intended_fdp->intended_fvp->volume_num));
			return;
		}

		queue_remove(qhp,
			derp,
			struct disk_eject_req *,
			q_link);
		/*
		 * intended_fdp->fvp is the volume to eject. If NULL, just put up 
		 * an alert panel. Eject the volume if it's in a drive. (Should 
		 * we ever see a case where it's not??)
		 */
		target_fvp = intended_fdp->fvp;
		if((target_fvp != NULL) && 
		   VALID_DRIVE(target_fvp->drive_num)) {
		    	/*
		 	 * Sync the file system on target_fvp if the device is
			 * open.
		 	 */
			XDBG(("disk_eject: ejecting vol %d\n",
				target_fvp->volume_num));
			if(target_fvp->raw_open || target_fvp->blk_open) {
			  	update(FD_DEVT(fd_blk_major, 
					target_fvp->volume_num,
					0),
				       ~(NPART - 1));
			}
		 
		 	/*
			 * eject the disk.
			 */
			fd_basic_cmd(target_fvp, FDCMD_EJECT);	
			if(!target_fvp->raw_open &&
			   !target_fvp->blk_open && 
			   !(target_fvp->flags & FVF_NEWVOLUME)) {
				/*
				 * Volume no longer in use. This happens when
				 * all references to the volume closed it, but
				 * did not eject it. Now that we've ejected
				 * it, we can free the volume struct.
				 */
				XDBG(("fd_disk_eject: freeing fvp\n"));
				fd_free_fv(target_fvp);
			}
		}
#ifdef	DEBUG
		else 
			printf("fd_disk_eject: no disk present\n");
#endif	DEBUG
		/*
		 * Remember that this drive is now waiting for a disk.
		 */
		intended_fdp->intended_fvp = derp->new_fvp;
		
		/*
		 * put up alert panel.
		 */
#ifdef	DEBUG
		printf("fd_disk_eject: Insert volume %d in Floppy drive"
			" %d\n", derp->new_fvp->volume_num, 
			derp->new_fvp->intended_drive);
#endif	DEBUG
		fd_alert_panel(derp->new_fvp, FALSE);
		kfree(derp, sizeof(struct disk_eject_req));

} /* fd_disk_eject() */

static void fd_alert_panel(fd_volume_t fvp, boolean_t wrong_disk) {

	kern_return_t krtn;
	
	if(fvp->format_info.flags & FFI_LABELVALID) {
		krtn = vol_panel_disk_label(fd_vol_panel_abort,
			fvp->labelp->dl_label,
			PR_DRIVE_FLOPPY,
			fvp->intended_drive,
			fvp,
			wrong_disk,
			&fvp->vol_tag);
	}
	else {
		krtn = vol_panel_disk_num(fd_vol_panel_abort,
			fvp->volume_num,
			PR_DRIVE_FLOPPY,
			fvp->intended_drive,
			fvp,
			wrong_disk,
			&fvp->vol_tag);
	}
	if(krtn) {
		printf("fd_alert_panel: vol_panel_disk() returned "
			"%d\n", krtn);
	}
	simple_lock(&fvp->lock);
	fvp->flags |= FVF_ALERT;
	simple_unlock(&fvp->lock);
}

static void fd_vol_panel_abort(void *param, int tag, int response_value) {
	
	/*
	 * Called by the vol driver upon receipt of a "disk insert panel 
	 * abort" message. Queue up an event for the volume_thread to deal
	 * with, since we can't do I/O here (we don't have an fvp).
	 */
	struct vol_abort_req *varp;
	
	varp = kalloc(sizeof(*varp));
	varp->fvp = param;
	varp->tag = tag;
	varp->response = response_value;
	XDBG(("vol_panel_abort: fvp 0x%x volume_num %d tag %d\n",
		param, varp->fvp->volume_num, tag));
	simple_lock(&vol_abort_lock);
	queue_enter(&vol_abort_q, varp, struct vol_abort_req *, link);
	simple_unlock(&vol_abort_lock);
}

static u_int fd_sectsize_max(void)
{
	struct fd_density_sectsize *dsp;
	struct fd_sectsize_info *ssip;
	static u_int sectsize_max;

	if (sectsize_max == 0) {
		for (dsp = fd_density_sectsize; dsp->ssip; dsp++)
			for (ssip = dsp->ssip; ssip->sect_size; ssip++)
				if (ssip->sect_size > sectsize_max)
					sectsize_max = ssip->sect_size;
		if (sectsize_max == 0)
			panic("fd_sectsize_max");
	}
	return sectsize_max;
}


/* end of fd_subr.c */
