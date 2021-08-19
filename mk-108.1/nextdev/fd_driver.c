/*	@(#)fd_driver.c	2.0	01/24/90	(c) 1990 NeXT	*/

/* 
 * fd_driver.c -- Unix block/character interface to Floppy Disk driver
 * KERNEL VERSION
 *
 * HISTORY
 *  2-Aug-90	Gregg Kellogg (gk) at NeXT
 *	Changes for programatic disksort interface.
 * 10-Jul-90	Gregg Kellogg
 *	Added support for procedural disksort interface.
 * 24-Jan-90	Doug Mitchell at NeXT
 *	Created.
 */ 
 
#import "fd.h"
#import <sys/types.h>
#import <sys/uio.h>
#import <sys/buf.h>
#import <kern/queue.h>
#import <sys/errno.h> 
#import <sys/user.h>
#import <sys/dk.h>
#import <sys/fcntl.h>
#import <nextdev/disk.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <next/pmon.h>
#import <next/pmon_targets.h>
#import <next/kernel_pmon.h>
#import <sys/conf.h>
#import	<nextdev/fd_extern.h>
#import	<nextdev/fd_vars.h> 

#define FD_ALLOW_IOC	1	/* allow non-suser access to ioctl's */

extern int nchrdev;
extern int nblkdev;

/*
 * prototypes for internal functions
 */
static u_int fdminphys(struct buf *bp);
static void fd_init_notify();
/*
 * autoconfiguration
 */
int fd_slave(fd_controller_t fcp, struct bus_device *bdp)
{
	int drive_num = bdp->bd_unit;
	fd_drive_t fdp = &fd_drive[drive_num];
	fd_volume_t fvp;
	struct fd_drive_stat dstat;
	int rtn;
	struct bdevsw *bd;
	struct cdevsw *cd;
	
	XCDBG(("fd_slave: drive %d\n", drive_num));
	
	if(drive_num >= NFD)
		return(0);

	/*
	 * init fd_drive[unit].
	 */
	simple_lock_init(&fdp->lock);
	fdp->unit = bdp->bd_slave;	/* drive # relative to controller */
	fdp->fcp = fcp;
	fdp->fvp = NULL;
	fdp->intended_fvp = NULL;
	fdp->bdp = bdp;
	fdp->last_access.high_val = fdp->last_access.low_val = 0;
	fdp->flags = 0;
	/*
	 * No way to figure this out from hardware right now...
	 */
	fdp->drive_type = DRIVE_TYPE_FD288;

	/*
	 * try a get status command. We can only tell whether or not drive 1 is
	 * present. The hardware only returns an error for drive 1 if it's
	 * not there, but we'll try anyway for all drives.
	 * 
	 */
	fd_volume_p[drive_num] = fvp = fd_new_fv(drive_num);
	fdp->flags |= FDF_PRESENT;		/* for now... */
	fd_assign_dv(fvp, drive_num);
	fd_polling_mode = TRUE;
	if(fd_get_status(fvp, &dstat) || !dstat.drive_present) {
		XDBG(("fd_slave: DRIVE %d NOT PRESENT\n", drive_num));
		fdp->fvp = NULL;
		fdp->flags &= ~FDF_PRESENT;
		fd_free_fv(fvp);
		fd_volume_p[drive_num] = NULL;
		rtn = 0;
	}
	else {
		XDBG(("fd_slave: DRIVE %d PRESENT\n", drive_num));
		rtn = 1;
		/*
		 * prepend drive type to "fdN at fcN..." string.
		 */
		printf("%s as ", 
			fd_drive_info[fdp->drive_type].gen_drive_info.di_name);
	}
	fd_polling_mode = FALSE;
	/*
	 * initialize a volume struct for the controller device.
	 */
	if(fd_volume_p[FVP_CONTROLLER] == NULL)
		fd_volume_p[FVP_CONTROLLER] = fd_new_fv(FVP_CONTROLLER);
		
	/* 
	 * remember major device numbers.
	 */
	for (bd = bdevsw; bd < &bdevsw[nblkdev]; bd++)
		if (bd->d_open == (int (*)())fdopen)
			fd_blk_major = bd - bdevsw;
	for (cd = cdevsw; cd < &cdevsw[nchrdev]; cd++)
		if (cd->d_open == (int (*)())fdopen)
			fd_raw_major = cd - cdevsw;

	return(rtn);	
} /* fd_slave() */

void fd_attach(struct bus_device *bdp)
{
	int unit = bdp->bd_unit;
	fd_volume_t fvp;
	fd_drive_t fdp = &fd_drive[unit];
	struct fd_drive_stat dstat;
	
	/*
	 * unit in this case means "drive" (not volume). Let's assume volume
	 * 0 for drive 0, volume 1 for drive 1.
	 */
	 
	XCDBG(("fd_attach: unit %d\n", unit));
	if(!(fdp->flags & FDF_PRESENT))
		return;			/* forget it... */
		
	/* 
	 * init fd_volume[unit]. We'll deallocate this if no disk is present.
	 * Note special case in fd_slave() could leave a valid fvp for volume 
	 * 1.
	 */
	if(fd_volume_p[unit] == NULL) {
		fd_volume_p[unit] = fvp = fd_new_fv(unit);
		fd_assign_dv(fvp, unit);
	}
	else
		fvp = fd_volume_p[unit];

	/* 
	 * See if a disk is present. If not, invalidate the volume.
	 */
	fd_polling_mode = TRUE;
	if(fd_get_status(fvp, &dstat)) {
		XDBG(("fd_attach: I/O ERROR ON GET STATUS\n"));
		goto bad_volume;		/* I/O error */
	}
	if(!dstat.drive_present) {
		XDBG(("fd_attach: NO DRIVE\n"));
		goto bad_volume;	
	}
	if(dstat.media_id == FD_MID_NONE) {
		XDBG(("fd_attach: NO DISK\n"));
		goto bad_volume;	
	}
	/*
	 * try to read the label. We use depressed retry counts for this 
	 * operation.
	 */
	fd_inner_retry = INNER_RETRY_CONF;
	fd_outer_retry = OUTER_RETRY_CONF;
	if(fd_attach_com(fvp))
		goto bad_volume;	/* This means I/O error */
	fd_basic_cmd(fvp, FDCMD_MOTOR_OFF);
	/*
	 * Queue up notification of this volume for WSM.
	 */
	volume_notify(fvp);
	fd_inner_retry = INNER_RETRY_NORM;
	fd_outer_retry = OUTER_RETRY_NORM;
	fd_polling_mode = FALSE;
	return;
	
bad_volume:
	fd_polling_mode = FALSE;
	fdp->fvp = NULL;
	fd_free_fv(fvp);
	fd_inner_retry = INNER_RETRY_NORM;
	fd_outer_retry = OUTER_RETRY_NORM;
} /* fd_attach() */

/* #define ALLOW_ALL_DENS	1	/* attempt reading at higher than legal 
				 	 * density */
				 
int fd_attach_com(fd_volume_t fvp)
{
	struct disk_label *dlp = fvp->labelp;
	struct fd_drive_stat dstat;
	int retry;
	struct fd_rw_stat rw_stat;
	int sect_num;
	struct fd_ioreq ioreq;
	struct fd_disk_info *dip;
	struct fd_format_info *fip = &fvp->format_info;
	struct fd_sectsize_info *ssip;
	int sect_size;
	int density;
	int resid_sects;
	int rtn;
	int read_id_cyl=1;
	
	/* 
	 * Determine density and try to read disk label. If fvp->drive_num
	 * is invalid, controller level code will assign a drive; otherwise
	 * I/O will be done on the specified drive.
	 *
	 * Returns an errno.
	 */
	XCDBG(("fd_attach_com: volume %d drive %d\n", 
		fvp->volume_num, fvp->drive_num));
	ASSERT(fvp != NULL);
	if(fip->flags & FFI_LABELVALID) {
		XDBG(("fd_attach_com: label already valid\n"));
		return(0);
	}
		
	/*
	 * try a recalibrate.
	 */
	fip->density_info.density = FD_DENS_4;	/* necessary for seek rate
						 * programming */
	fip->flags &= ~FFI_FORMATTED;
	for(retry=FD_ATTACH_TRIES; retry; retry--) {
		if(fd_recal(fvp) == 0)
			break;
	} /* retrying */
	if(retry == 0) {
		/* 
		 * Can't even recalibrate; forget it.
		 */
		fip->density_info.density = FD_DENS_NONE;
		printf("fd: RECALIBRATE FAILED\n");
		return(EIO);
	}
			
	/*
	 * Determine media ID and write protect.
	 */
	fip->disk_info.media_id = FD_MID_NONE;
	if(fd_get_status(fvp, &dstat)) {
		printf("fd: CONTROLLER I/O ERROR\n");
		/* FIXME: handle 'user said NO to disk insert alert' */
		return(EIO);		
	}
	if(!dstat.drive_present) {
		printf("fd: NO DRIVE DETECTED\n");
		return(EIO);			/* (Should only happen during
						 * autoconf) */
	}
	if(dstat.media_id == FD_MID_NONE) {
		printf("fd: NO MEDIA DETECTED\n");
		return(EIO);			/* (Should only happen during
						 * autoconf) */
	}
	if(dstat.write_prot)
		fip->flags |= FFI_WRITEPROTECT;
	else
		fip->flags &= ~FFI_WRITEPROTECT;
	/*
	 * We know the disk; set disk-specific parameters.
	 */
	fip->disk_info.media_id = dstat.media_id;
	for(dip = fd_disk_info; dip->media_id != FD_MID_NONE; dip++) {
		if(dip->media_id == dstat.media_id)
			break;
	}
	ASSERT(dip->media_id != FD_MID_NONE);
	fip->disk_info = *dip;
	XDBG(("fd_attach_com: media_id = %d\n", dstat.media_id));
	
	/*
	 * try reading some IDs, starting at the highest legal density and 
	 * working down. Use a different cylinder for each attempt.
	 */
	fip->density_info.mfm = 1;
#ifdef	ALLOW_ALL_DENS
	for(fip->density_info.density=FD_DENS_4; 
#else	ALLOW_ALL_DENS
	for(fip->density_info.density=fip->disk_info.max_density; 
#endif	ALLOW_ALL_DENS
	    fip->density_info.density>FD_DENS_NONE; 
	    fip->density_info.density--) {
		for(retry=FD_ATTACH_TRIES; retry; retry--) {	
			XDBG(("fd_attach_com: density = %d cyl = %d\n", 
				fip->density_info.density, read_id_cyl));
			if(fd_seek(fvp, read_id_cyl, 0)) {
				printf("fd: SEEK FAILED\n");
				return(EIO);
			}
			switch(fd_readid(fvp, 0, &rw_stat)) {
			    case FDR_SUCCESS:
				goto found_dens;
#ifdef	notdef
			    /* 
			     * This was only for first prototype drive...
			     */
			    case FDR_TIMEOUT:
			    	/*
				 * Special case. A timeout on a read ID command
				 * means No Disk!
				 */
				return(EIO);
#endif	notdef
			    default:
			    	break;		/* try again */
			}
		} /* retrying */
	}
found_dens:
	if(fip->density_info.density == FD_DENS_NONE) {
		/*
		 * disk is unformatted. No sense in proceeding. (this isn't
		 * an error.)  Set density and sect_size info to give
		 * formatter a set of defaults to use.
		 */
		density = fip->disk_info.max_density;
		sect_size = fd_get_sectsize_info(density)->sect_size;
		fd_set_density_info(fvp, density);
		fd_set_sector_size(fvp, sect_size);
		fip->flags &= ~FFI_FORMATTED;
		printf("fd: DISK UNFORMATTED\n");
		return(0);
	}
	XDBG(("  Header found: cyl 0x%x  hd 0x%x  sector 0x%x\n",
		rw_stat.cylinder, rw_stat.head, rw_stat.sector));
	fd_set_density_info(fvp, fip->density_info.density);
	
	/*
	 * try reading one sector for each sector size. The idea is to 
	 * determine the sector size independently of seeing a valid label.
	 */
	fip->flags |= FFI_FORMATTED;		/* invalidate if this part 
						 * fails... */
	/*
	 * Find appropriate sector info for density
	 */
	for (ssip=fd_get_sectsize_info(fip->density_info.density);
	    ssip->sect_size;
	    ssip++) {
		sect_size = ssip->sect_size;
		XDBG(("fd_attach_com: sect_size = %d\n", sect_size));
		/*
		 * Validate physical disk parameters for this sector size so we
		 * can do I/O.
		 */
		fd_set_sector_size(fvp, sect_size);
		sect_num = 0;
		for(retry=FD_ATTACH_TRIES; retry; retry--) {	
			XDBG(("fd_attach_com: sect_num = %d\n", sect_num));
			if(fd_raw_rw(fvp,
			    sect_num,
			    1,				/* sect_count */
			    (caddr_t)fvp->labelp,
			    TRUE) == 0)			/* read */
				goto disk_formatted;
			/*
			 * Try another sector.
			 */			
			sect_num += (fip->sectsize_info.sects_per_trk + 1);
		}
	} /* for each sector size */
	
	if(retry == 0) {
		/*
		 * Could not read this disk. Leave it marked unformatted 
		 * (even though we know its density).
		 */
		printf("fd: DISK UNREADABLE\n");
		fip->flags &= ~FFI_FORMATTED;
		return(0);
	}
disk_formatted:
	/*
	 * At least we know everything about the disk's format.
	 * Try reading a label.
	 */
	if(fd_get_label(fvp) == 0) {
		fd_setbratio(fvp);
		printf("\tDisk Label: %s\n", dlp->dl_label);
		printf("\tDisk Capacity %d KB, Device Block %d "
			"bytes\n",
			fip->sectsize_info.sect_size *
			fip->sectsize_info.sects_per_trk *
			fip->disk_info.tracks_per_cyl * 
			fip->disk_info.num_cylinders / 1024,
			fip->sectsize_info.sect_size);
	} 
	else
		printf("fd: UNABLE TO READ DISK LABEL\n");
	if(fip->flags & FFI_WRITEPROTECT)
		printf("\tDisk is Write Protected\n");
	return(0);
} /* fd_attach_com() */

int fdopen(dev_t dev, int flag)
{
	int volume = FD_VOLUME(dev);
	u_char partition_bits = 1 << FD_PART(dev);
	int rtn;
	fd_volume_t fvp = fd_volume_p[volume];
	int drive_num;
	
	XCDBG(("fdopen: volume %d part %d\n", volume, FD_PART(dev)));
	if(volume == FVP_CONTROLLER)
		return(0);		/* controller device */
	if(volume > FVP_CONTROLLER)
		return(ENXIO);
	rtn = ENXIO;
	/*
	 * Make sure there's at least one drive in the system.
	 */
	for(drive_num=0; drive_num<NFD; drive_num++) {
		if(fd_drive[drive_num].flags & FDF_PRESENT) {
			rtn = 0;
			break;
		}
	}
	if(rtn)
		return(rtn);
	/*
	 * we allow only one open() in progress at a time. This avoids nasty
	 * race conditions involving:
	 * -- creating new fd_volumes (done here)
	 * -- mapping volumes to drives (done by the volume_check() thread,
	 *    dependent on new fd_volume's we create here and on FVF_NEWVOLUME 
	 *    flags)
	 * -- multiple attempts to read labels for new volumes (done here,
	 *    via fd_attach_com(); another open() could happen while we're
	 *    blocked trying to read a label, forcing yet another attempt
	 *    to read the same label).
	 */
	lock_write(&fd_open_lock);
	if(flag & O_NDELAY) {
	 	if((fvp == NULL) || (fvp->drive_num == DRIVE_UNMOUNTED)) {
			rtn = EWOULDBLOCK;
			goto out;
		}
	}
	if(fvp == NULL) {
		struct fd_drive_stat stat;
		
		/*
		 * new volume. Do a get status command - this will "fault in"
		 * the volume. If the get_status fails, so does the open (this
		 * means a user-level abort via 'n' to the alert panel). 
		 *
		 * If the get_status succeeds, fd_attach_com has already been
		 * called (by the vol_check thread). vol_check has validated
		 * the fd_format_info in out fvp.
		 */
		fd_volume_p[volume] = fvp = fd_new_fv(volume);
		fvp->flags |= FVF_NEWVOLUME;
		fvp->drive_num = DRIVE_UNMOUNTED;
		if(fd_get_status(fvp, &stat)) {
			/*
			 * Failed. Free the volume struct; this was a nop.
			 */
			fd_free_fv(fvp);
			rtn = ENXIO;
			goto out;
		}
	}
	/* rtn = fd_attach_com(fvp); */
		 
	/*
	 * remember which device has been opened.
	 */
	if(rtn == 0) {
		simple_lock(&fvp->lock);
		if(major(dev) == fd_raw_major)
			fvp->raw_open |= partition_bits;
		else
			fvp->blk_open |= partition_bits;
		simple_unlock(&fvp->lock);
	}
	fvp->owner = u.u_ruid;
out:
	lock_done(&fd_open_lock);
	return(rtn);
	
} /* fdopen() */

int fdclose(dev_t dev)
{
	int volume = FD_VOLUME(dev);
	u_char partition_bits = 1 << FD_PART(dev);
	fd_volume_t fvp = fd_volume_p[volume];
	
	XCDBG(("fdclose: volume %d part %d\n", volume, FD_PART(dev)));
	if(volume == FVP_CONTROLLER)
		return(0);		/* controller device */
	if(volume > FVP_CONTROLLER)
		return(ENXIO);
		
	/*
	 * remember which device has been closed.
	 */
	simple_lock(&fvp->lock);
	if(major(dev) == fd_raw_major)
		fvp->raw_open &= ~partition_bits;
	else
		fvp->blk_open &= ~partition_bits;
	simple_unlock(&fvp->lock);
	
	lock_write(&fd_open_lock);
	if(!(fvp->raw_open | fvp->blk_open)) {
		/*
		 * No references remaining.
		 */
	   	if(fvp->drive_num == DRIVE_UNMOUNTED) {
	   		XDBG(("fdclose: freeing fvp\n"));
			fd_free_fv(fvp);
		}
		else {
			fd_basic_cmd(fvp, FDCMD_MOTOR_OFF);
		}
	}
	lock_done(&fd_open_lock);
	return(0);
	
} /* fdclose() */
				 
int fdread(dev_t dev, struct uio *uiop)
{
	int volume = FD_VOLUME(dev); 
	fd_volume_t fvp = fd_volume_p[volume];
	int rtn;
	int blocksize;
	
	/*
	 * returns an errno.
	 */
	XCDBG(("fdread: volume %d part %d\n", volume, FD_PART(dev)));
	
	if(volume >= FVP_CONTROLLER)	/* invalid for controller device */
		return(ENXIO);
	if(fvp == NULL)
		return(ENXIO);		/* not open */
	if(FD_PART(dev) == FD_LIVE_PART)
	    	blocksize = fvp->format_info.sectsize_info.sect_size;
	else
		blocksize = DEV_BSIZE;
	rtn = physio(fdstrategy, &fd_volume_p[volume]->rio_buf, dev, B_READ,
	    	fdminphys, uiop, blocksize);
	return(rtn);
		
} /* fdread() */

int fdwrite(dev_t dev, struct uio *uiop)
{
	int volume = FD_VOLUME(dev); 
	fd_volume_t fvp = fd_volume_p[volume];
	int rtn;
	int blocksize;
	
	/*
	 * returns an errno.
	 */
	XCDBG(("fdwrite: volume %d part %d\n", volume, FD_PART(dev)));
	
	if (volume >= FVP_CONTROLLER)	/* invalid for controller device */
		return(ENXIO);
	if(fvp == NULL)
		return(ENXIO);		/* not open */
	if(FD_PART(dev) == FD_LIVE_PART)
	    	blocksize = fvp->format_info.sectsize_info.sect_size;
	else
		blocksize = DEV_BSIZE;
	rtn = physio(fdstrategy, &fd_volume_p[volume]->rio_buf, dev, B_WRITE,
	    	fdminphys, uiop, blocksize);
	return(rtn);
} /* fdwrite() */

static u_int fdminphys(bp)
	struct buf *bp;
{
	/* The SCSI driver enforces a maximum DMA size of 8K. If we 
	 * do the same, I/O to the live partition breaks, since physio()
	 * will break up large I/O into smaller pieces, using the
	 * wrong (hard-coded) device block size. Thus we allow very large 
	 * I/Os...
	 */
	if (bp->b_bcount > FD_MAX_DMA)
		bp->b_bcount = FD_MAX_DMA;
}

int fdstrategy(register struct buf *bp)
{
	int volume = FD_VOLUME(bp->b_dev);
	int partition = FD_PART(bp->b_dev);
	fd_volume_t fvp = fd_volume_p[volume];
	int rtn = 0;
	ds_queue_t *q;
	int s;
	daddr_t bno;
	struct partition *pp;
	int sect_per_cyl;
	struct disk_label *dlp = fvp->labelp;
	
	/*
	 * returns 0/-1.
	 */
	XCDBG(("fdstrategy: volume %d part %d\n", volume, partition));
	if (volume > NUM_FVP) {
		XDBG(("fdstrategy: bad volume\n"));
		bp->b_error = ENXIO;
		goto bad;
	}
	ASSERT(fvp != NULL);
	
	/*
	 * Don't check special commands, rely on fd_start to
	 * reject anything unacceptable
	 */
	if (bp != &fvp->local_buf) {
#ifdef	DEBUG
		if(bp->b_flags&B_READ) {
			XCDBG(("fdstrategy: READ block 0x%x count 0x%x\n",
				bp->b_blkno, bp->b_bcount));
		}
		else {
			XCDBG(("fdstrategy: WRITE block 0x%x count 0x%x\n",
				bp->b_blkno, bp->b_bcount));
		}
#endif	DEBUG
		if(!(fvp->format_info.flags & FFI_FORMATTED)) {
			/*
			 * Unformatted. Can't do this.
			 */
			bp->b_error = ENXIO;
			goto bad;
		}
		if(bp->b_bcount &
		    (fvp->format_info.sectsize_info.sect_size - 1)) {
			/*
			 * Not multiple of sector size. Forget it.
			 */
			XDBG(("fdstrategy: partial sector\n"));
			bp->b_error = EINVAL;
			goto bad;
		}
		if(((bp->b_flags & B_READ) == 0) && 
		   (fvp->format_info.flags & FFI_WRITEPROTECT)) {
			XDBG(("fdstrategy: write to write protected disk\n"));
		 	bp->b_error = EROFS;
			goto bad;
		}
		if(partition != FD_LIVE_PART) {
			/* 
			 * normal partitions
			 */
			if (!(fvp->format_info.flags & FFI_LABELVALID)) {
				bp->b_error = ENXIO;
				goto bad;
			}
			bno = bp->b_blkno;
			bp->b_sort_key = bno;
			if (partition >= NUM_FP) {
				bp->b_error = ENXIO;
				goto bad;
			}
			pp = &dlp->dl_part[partition];
			if (pp->p_size == 0 || bno < 0 || bno > pp->p_size) {
				bp->b_error = EINVAL;
				goto bad;
			}
			if (bno == pp->p_size) {	/* end-of-file */
				bp->b_resid = bp->b_bcount;
				goto done;
			}
			bp->b_sort_key += pp->p_base;
			sect_per_cyl = dlp->dl_nsect * dlp->dl_ntrack;
			if (sect_per_cyl > 0)
				bp->b_sort_key /= sect_per_cyl;
		}
		else {			
			/*
			 * Special case - last partition. We don't need
			 * any "partition" or label info; this partition is
			 * the whole disk. We only need to know the 
			 * physical disk parameters.
			 */
				
			bno = bp->b_blkno;
			bp->b_sort_key = bno;
			if((bno < 0) || (bno > fvp->format_info.total_sects)) {
				bp->b_error = EINVAL;
				goto bad;
			}
			if (bno == fvp->format_info.total_sects) {  /* EOF */
				bp->b_resid = bp->b_bcount;
				goto done;
			}
		}
		/* 
		 * log this event if enabled
		 */
		pmon_log_event(PMON_SOURCE_FD,
			bp->b_flags & B_READ ? KP_SCSI_READQ : KP_SCSI_WRITEQ,
  			bp->b_blkno,
  			bp->b_bcount,
  			volume);
	} 

	/*
	 *
	 */
	s = splbio();
	simple_lock(&fvp->lock);
	q = &fvp->io_queue;
	/* 
	 * Add request to per-device queue. 
	 */
	if(bp == &fvp->local_buf) {
		/* 
		 * Special commands go first, except for eject commands, which 
		 * go last.
		 */
		if(fvp->local_ioreq_p->command == FDCMD_EJECT) {
			disksort_enter_tail(q, bp);
		}
		else {
			disksort_enter_head(q, bp);
		}		   
	}
	else
		disksort_enter(q, bp);		/* normal r/w */
	simple_unlock(&fvp->lock);
	splx(s);
	if(fvp->state == FVSTATE_IDLE)
		rtn = fd_start(fvp);
	return(rtn);
bad:
	bp->b_flags |= B_ERROR;
	rtn = -1;
done:
	biodone(bp);
	return(rtn);

} /* fdstrategy() */

int fdioctl(dev_t dev, 
	int cmd, 
	caddr_t data, 
	int flag)
{
	int volume = FD_VOLUME(dev);
	fd_volume_t fvp = fd_volume_p[volume];
	struct drive_info *dip;
	struct fd_ioreq *fdiop;
	caddr_t cp = *(caddr_t *)data;
	caddr_t ap;
	char *user_addr;
	int i, nblk, error = 0;
	u_short *dl_cksum, size;
	struct tsval ts;
	int drive_num;
	
	/*
	 * returns an errno.
	 */
	XCDBG(("fdioctl: volume %d cmd = %d\n", FD_VOLUME(dev), cmd));

	/*
	 * do ioctls for fdc (volume == FVP_CONTROLLER)....
	 */
	switch (cmd) {
	    case DKIOCGFREEVOL:
	    	/* 
		 * find first free volume. ENODEV if no drives present.
		 * FIXME: mark as reserved???
		 */
		if(!(fd_drive[0].flags & FDF_PRESENT))
			return(ENODEV);
		for(i=0; i<NUM_FV; i++) {
			if(fd_volume_p[i] == NULL) {
				*(int*) data = i;
				return(0);
			}
		}
		*(int*) data = -1;		/* no free volumes */
		return (0);

		/*
		 * Retry count operations.
		 */
	   case FDIOCSIRETRY:
		fd_inner_retry = *(int *)data;
		XDBG(("   set fd_inner_retry = %d\n", fd_inner_retry));
		return(0);
	   case FDIOCSORETRY:
		fd_outer_retry = *(int *)data;
		XDBG(("   set fd_outer_retry = %d\n", fd_outer_retry));
		return(0);
	   case FDIOCGIRETRY:
		*(int *)data = fd_inner_retry;
		return(0);
	   case FDIOCGORETRY:
		*(int *)data = fd_outer_retry;
		return(0);

	    default:
	    	break;
	}
	
	if(volume >= FVP_CONTROLLER)
		return(ENXIO);
	switch (cmd) {
	    case DKIOCGLABEL:
		if (! (fvp->format_info.flags & FFI_LABELVALID))
			return(ENXIO);
		error = copyout(fvp->labelp, cp, sizeof(*fvp->labelp));
		break;
		
	    case DKIOCSLABEL:
		if (!suser())
			return(u.u_error);
		simple_lock(&fvp->lock);
		if (error = copyin(cp, fvp->labelp, sizeof(*fvp->labelp))) {
			simple_unlock(&fvp->lock);
			break;
		}
		if ((fvp->labelp->dl_version == DL_V1) ||
		    (fvp->labelp->dl_version == DL_V2)) {
			size = sizeof (struct disk_label);
			dl_cksum = &fvp->labelp->dl_checksum;
		} else {
			size = sizeof (struct disk_label) -
				sizeof (fvp->labelp->dl_un);
			dl_cksum = &fvp->labelp->dl_v3_checksum;
		}
		/*
		 * tag label with time. Assume this label is for block 0.
		 */
		event_set_ts(&ts);
		fvp->labelp->dl_tag = ts.low_val;
		fvp->labelp->dl_label_blkno = 0;
		*dl_cksum = 0;
		*dl_cksum = checksum_16 ((u_short *)fvp->labelp, size >> 1);
		
		simple_unlock(&fvp->lock);
		if (! sdchecklabel(fvp->labelp, 0))
			return(EINVAL);
		if (fd_write_label(fvp))
			return(EIO);
		break;
		
	    case DKIOCINFO:
	    	{
		char namebuf[10];
		
	    	drive_num = fvp->drive_num;
		if(drive_num == DRIVE_UNMOUNTED)
			drive_num = 0;		/* hack to get a valid
						 * index into fd_drive[] */
		dip = (struct drive_info *)data;
		*dip = fd_drive_info[fd_drive[drive_num].drive_type].
		       gen_drive_info;
		dip->di_devblklen = fvp->format_info.sectsize_info.sect_size;
		/*
		 * We need to append the disk capacity specifier to the
		 * disk name.
		 */
		sprintf(namebuf, " %d", fvp->format_info.total_sects);
		strcat(dip->di_name, namebuf);
		break;
		}
				
	   case FDIOCSDENS:
	        {
		int density = *(int *)data;
		
		XDBG(("   set density = %d\n", density));
		if((density < FD_DENS_NONE) || (density > FD_DENS_4))
			return(EINVAL);
		fd_set_density_info(fvp, density);
		break;
		}
		
	   case FDIOCSSIZE:
	        {
		int sect_size = *(int *)data;
		
		XDBG(("   set sect_size = %d\n", sect_size));
		if(fd_set_sector_size(fvp, sect_size))
			return(EINVAL);
		else
			break;
		}
		
	   case FDIOCSGAPL:
	   	{
	   	/*
		 * set gap 3 length.
		 */
		int gap_length = *(int *)data;
		
		XDBG(("   set gap_length = %d\n", gap_length));
		fvp->format_info.sectsize_info.rw_gap_length = gap_length;
		break;
		}
		
	   case FDIOCGFORM:
	   	/* 
		 * get Disk Format
		 */
		*(struct fd_format_info *)data = fvp->format_info;
		break;
		
	   case DKIOCEJECT:
		/* 
		 * only allow suser and owner of volume to eject.
		 */
		if (!suser() && (u.u_ruid != fvp->owner))
			error = u.u_error;
		else {
			/*
		 	 * Sync the file system on fvp before the eject.
		 	 */
		  	update(FD_DEVT(fd_blk_major, fvp->volume_num, 0),
			       ~(NPART - 1));
			error = fd_basic_cmd(fvp, FDCMD_EJECT);
		}
		break;
		
	   case FDIOCREQ:
	   	/*
		 * perform I/O specified as a fd_ioreq
		 */
#ifndef	FD_ALLOW_IOC
		if (!suser())
			return(u.u_error);
#endif	FD_ALLOW_IOC
		fdiop = (fd_ioreq_t)data;
		
		/* if user expects to do some DMA, get some kernel memory. Copy
		 * in the user's data if a DMA write is expected. Well-aligned
		 * transfers are required.
		 */
		if (fdiop->byte_count != 0) {
			if(!DMA_ENDALIGNED(fdiop->byte_count))
				return(EINVAL);
			ap = 
			   (caddr_t)kalloc(fdiop->byte_count);
			if(ap == 0) {
				XDBG(("        ...kalloc() failed\n"));
				fdiop->status = FDR_MEMALLOC;
				return(ENOMEM);
			}
			if((fdiop->flags & FD_IOF_DMA_DIR) == FD_IOF_DMA_WR) {
				if (error = copyin(fdiop->addrs, ap,
						   fdiop->byte_count)) {
				    XDBG(("        ...copyin() returned %d\n",
					    error));
				    fdiop->status = FDR_MEMFAIL;
				    goto err_exit;
				}
			}
		}
			
		user_addr = fdiop->addrs;
		fdiop->addrs = ap;
		error = fd_command(fvp, fdiop);
		/*
		 * Setting errno nonzero prevents caller from seeing 
		 * fdiop->status. We force caller to get error from
		 * fdiop->status.
		 */
		if(fdiop->status)
			error = 0;
		fdiop->addrs = user_addr;
		if ((fdiop->flags & FD_IOF_DMA_DIR) == FD_IOF_DMA_RD && 
		    (fdiop->bytes_xfr != 0))
			error = copyout(ap, user_addr, fdiop->bytes_xfr);
err_exit:
		/* 
		 * if we kalloc'd any memory, free it.
		 */
		if (fdiop->byte_count != 0) 
			kfree(ap, fdiop->byte_count); 
		break;
	    
	    case FDIOCRRW:
	    	{
		struct fd_rawio *rawiop = (struct fd_rawio *)data;
		int byte_count = rawiop->sector_count * 
			         fvp->format_info.sectsize_info.sect_size;
		int rtn;
		int resid_sects;
		
	    	/* 
		 * Read/write from/to the 'live' partition.
		 */
		if(!fvp->format_info.flags & FFI_FORMATTED)
			return(EINVAL);
#ifndef	FD_ALLOW_IOC
		if (!suser())
			return(u.u_error);
#endif	FD_ALLOW_IOC
		
		/* Get some kernel memory; copy in the user's data if writing.
		 */
		if ((ap = (caddr_t)kalloc(byte_count)) == 0) {
			XDBG(("        ...kalloc() failed\n"));
			rawiop->status = FDR_MEMALLOC;
			return(ENOMEM);
		}
		if(!rawiop->read) {
		    if (error = copyin(rawiop->dma_addrs, ap, byte_count)) {
			XDBG(("        ...copyin() returned %d\n", error));
			rawiop->status = FDR_MEMFAIL;
			goto err_exit1;
		    }
		}
		rawiop->status = fd_live_rw(fvp,
			rawiop->sector,
			rawiop->sector_count,
			ap,
			rawiop->read,
			&resid_sects);
		error = 0;		/* let caller see rawiop->status */
		rawiop->sects_xfr = rawiop->sector_count - resid_sects;
		if (rawiop->read && rawiop->sects_xfr)
			error = copyout(ap, rawiop->dma_addrs, byte_count);
err_exit1:
		kfree(ap, byte_count); 
		break;
		}
		
	    default:
		u.u_error = EINVAL;
		return(EINVAL);
	} /* switch cmd */
	
	return(error);
} /* fdioctl() */

int fd_command(fd_volume_t fvp,	fd_ioreq_t fdiop)
{
	/* 
	 * returns an errno.
	 * Parameters are passed from the user (and to fd_start()) in a 
	 * fd_ioreq struct. We use fvp->local_buf to serialize access to 
	 * fvp->local_ioreq_p.
	 *
	 * Polling should ONLY be used during auto-configuration
	 * when we can't sleep because the system isn't up far enough.
	 */
	 
	struct buf *bp = &fvp->local_buf;
	int rtn = 0;
	int s;
	
	XDBG(("fd_command: volume %d\n", fvp->volume_num));
	s = splbio();
	while ((bp->b_flags & B_BUSY) && !fd_polling_mode) {
		bp->b_flags |= B_WANTED;
		sleep(bp, PRIBIO);
	}
	bp->b_flags = B_BUSY | B_READ;		/* be sure to free this... */
	splx(s);
	
	/* we now own fvp->local_buf and fvp->local_ioreq_p. */
	fvp->local_ioreq_p = fdiop;
	fvp->local_ioreq_p->pmap = pmap_kernel();
	
	/* nobody looks at partition number if we're using local_ioreq_p. */
	bp->b_dev = FD_DEVT(fd_blk_major, fvp->volume_num, 0);
	
	if((rtn = fdstrategy(bp)) == 0) {
	
		/* only biowait() if we could actually start command. In 
		 * polling mode, the command is complete on return from 
		 * fdstrategy. 
		 */
		if (!fd_polling_mode)
			biowait(bp);
	}
	if(rtn)
		rtn = EIO;		/* don't set errno of -1! */
	if((rtn == 0) && (fdiop->status))
		rtn = EIO;
	XCDBG(("fd_command: returning %d status = %n\n", rtn, fdiop->status,
		fd_return_values));
	fvp->local_ioreq_p = NULL;	/* this field is now meaningless */
	bp->b_flags &= ~B_BUSY;
	if ((bp->b_flags & B_WANTED) && !fd_polling_mode)
		wakeup(bp);
	return(rtn);
	
} /* fd_command() */

/* end of fd_driver.c */







