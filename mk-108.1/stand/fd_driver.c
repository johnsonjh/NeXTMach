/*	@(#)fd_driver.c	2.0	01/24/90	(c) 1990 NeXT	*/

/* 
 * fd_driver.c -- Front end for Floppy Disk driver
 * STANDALONE BOOT VERSION
 *
 * HISTORY
 * 03-Apr-90	Doug Mitchell at NeXT
 *	Created.
 */ 

#import <sys/types.h>
#import <sys/param.h>
#import <nextdev/disk.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <sys/conf.h>
#import <stand/saio.h>
#import	<stand/fd_extern.h>
#import	<stand/fd_vars.h> 

/*
 * global variables
 */
struct fd_global fd_global;

/*
 * Constants 
 */
#define	DRIVE_TYPES	1		/* number of supported drive types */

struct fd_drive_info fd_drive_info[DRIVE_TYPES] = {
	{				/* FD-288 drive */
	    {
		"Sony MPX-111N",	/* disk name */
		{ 0, 
		  0, 
		  0,
		  0 }, 			/* label locations - not used */
		1024,			/* device sector size. This is
					 * not fixed. */
		64 * 1024		/* max xfer bcount */
	    },
	    	SONY_SEEK_RATE,
		SONY_HEAD_LOAD,
		SONY_HEAD_UNLOAD
	},
};

/*
 * Constant tables for mapping media_id and density into physical disk 
 * parameters.
 */
struct fd_disk_info fd_disk_info[] = {
   /* media_id          tracks_per_cyl  num_cylinders   max_density */
    { FD_MID_1MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_1    },
    { FD_MID_2MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_2    },
    { FD_MID_4MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_4    },
    { FD_MID_NONE,	0, 		0, 		FD_DENS_NONE },
};

struct fd_density_info fd_density_info[] = {
    /* density     	capacity    	gap3	mfm         */
    { FD_DENS_1,	720  * 1024,	8,	TRUE	}, /* 720  KB */
    { FD_DENS_2,	1440 * 1024, 	16,	TRUE	}, /* 1.44 MB */
    { FD_DENS_4,	2880 * 1024,	32,	TRUE	}, /* 2.88 MB */
    { FD_DENS_NONE,	720  * 1024,	48,	TRUE	}, /* unformatted */
};

/* #define SHOW_AUTOSIZE	1	/* */

static int fd_attach(fd_volume_t fvp)
{
	struct disk_label *dlp = fvp->labelp;
	int retry;
	struct fd_rw_stat rw_stat;
	int sect_num;
	struct fd_ioreq ioreq;
	struct fd_disk_info *dip;
	struct fd_format_info *fip = &fvp->format_info;
	struct fd_drive_stat dstat;
	int sect_size;
	int density;
	int rtn;
	
#ifdef	FD_DEBUG
	printf("fd_attach: volume %d drive %d\n", fvp->volume_num,
		fvp->drive_num);
#endif	FD_DEBUG
	/* 
	 * Determine density and try to read disk label.
	 *
	 * First try a recalibrate.
	 */
	fip->density_info.density = FD_DENS_1;	/* necessary for seek rate
						 * programming */
	fip->flags &= ~FFI_FORMATTED;
	for(retry=FV_ATTACH_TRIES; retry; retry--) {
		if(fd_recal(fvp) == 0)
			break;
	} /* retrying */
	if(retry == 0) {
		/* 
		 * Can't even recalibrate; forget it.
		 */
		printf("fd: RECALIBRATE FAILED\n");
		goto attach_fail;
	}
			
	/*
	 * Determine media ID and write protect.
	 */
	fip->disk_info.media_id = FD_MID_NONE;
	if(fd_get_status(fvp, &dstat)) {
		printf("fd: CONTROLLER I/O ERROR\n");
		goto attach_fail;
	}
	if(dstat.media_id == FD_MID_NONE) {
		printf("fd: NO MEDIA DETECTED\n");
		return(EDIO);			/* (Should only happen during
						 * autoconf) */
	}
	/*
	 * We know the disk; set disk-specific parameters.
	 */
	fip->disk_info.media_id = dstat.media_id;
	for(dip = fd_disk_info; dip->media_id != FD_MID_NONE; dip++) {
		if(dip->media_id == dstat.media_id)
			break;
	}
	fip->disk_info = *dip;
		
	/*
	 * try reading one sector for each sector size and density
	 */
	fip->flags |= FFI_FORMATTED;		/* invalidate if this part 
						 * fails... */
	fip->density_info.mfm = 1;
	sect_num = 0;
	for(fip->density_info.density=FD_DENS_4; 
	    fip->density_info.density>FD_DENS_NONE; 
	    fip->density_info.density--) {
#ifdef	SHOW_AUTOSIZE
		printf("density = %d\n", fip->density_info.density);
#endif	SHOW_AUTOSIZE
		fd_set_density_info(fvp, fip->density_info.density);
		for(sect_size=FD_SECTSIZE_MIN;
		    sect_size<=FD_SECTSIZE_MAX;
		    sect_size <<= 1) {
			/*
			 * Validate physical disk parameters for this sector
			 * size so we can do I/O.
			 */
#ifdef	SHOW_AUTOSIZE
			printf("sect_size = %d\n", sect_size);
#endif	SHOW_AUTOSIZE
			fd_set_sector_size(fvp, sect_size);
			for(retry=FV_ATTACH_TRIES; retry; retry--) {	
				if(fd_raw_rw(fvp,
				    sect_num,
				    1,			/* sect_count */
				    (caddr_t)fvp->labelp,
				    TRUE) == 0)		/* read */
					goto disk_formatted;
				/*
				 * Try another sector.
				 */			
				sect_num += (fip->sects_per_trk + 1);
			}
		} /* for each sector size */
		if(fd_recal(fvp)) {
			printf("RECALIBRATE FAILED\n");
			goto attach_fail;
		}
	} /* for each density */
	if(retry == 0) {
		/*
		 * Could not read this disk. Leave it marked unformatted 
		 * (even though we know its density).
		 */
		printf("fd: DISK UNREADABLE\n");
		goto attach_fail;
	}
disk_formatted:
	/*
	 * At least we know everything about the disk's format.
	 * Try reading a label.
	 */
	if(fd_get_label(fvp) == 0) {
		fd_setbratio(fvp);
		return(0);
	} 
	printf("fd: UNABLE TO READ DISK LABEL\n");
	
attach_fail:
	fd_basic_cmd(fvp, FDCMD_MOTOR_OFF);
	return(EDIO);

} /* fd_attach() */

int fdopen(struct iob *iobp)
{
	fd_volume_t fvp;
	int rtn;
	
#ifdef	FD_DEBUG
	printf("fd_open: c %d u %d p %d\n", 
		iobp->i_ctrl, iobp->i_unit, iobp->i_part);
#endif	FD_DEBUG
	if (iobp->i_unit < 0)
		iobp->i_unit = 0;
	if (iobp->i_part < 0)
		iobp->i_part = 0;
	if (iobp->i_part >= NPART)
		_stop("bad partition number");
	if (iobp->i_ctrl < 0) {
		iobp->i_ctrl = 0;
		iobp->i_unit = 0;
	} else {
		if (iobp->i_ctrl >= NFD)
			_stop("illegal fd drive number");
	}
	fvp = &fd_global.fd_volume;
	fd_new_fv(fvp, iobp->i_unit);
	fd_assign_dv(fvp, iobp->i_unit);
	/*
	 * init drive info.
	 */
	fvp->unit = iobp->i_unit;
	fvp->fcp = &fd_global.fd_controller;
	fvp->drive_flags = FDF_PRESENT;	
	/*
	 * Init controller hardware.
	 */
	if(fc_init((caddr_t)P_FLOPPY))
		return(-1);
	rtn = fd_attach(fvp);
	if(rtn)
		_stop("Couldn't initialize floppy device");
	if(!(fvp->format_info.flags & FFI_LABELVALID))
		_stop("Couldn't read disk label");
	iobp->i_secsize = fvp->labelp->dl_secsize;
	return(0);	
} /* fdopen() */

void fdclose(struct iob *iobp)
{
	fd_volume_t fvp = &fd_global.fd_volume;
	
	fd_basic_cmd(fvp, FDCMD_MOTOR_OFF);
	fd_free_fv(&fd_global.fd_volume);
}

int fdstrategy(struct iob *iobp, int func)
{
	int rtn=0;
	fd_volume_t fvp;

	int partition;
	int s;
	daddr_t bno;
	int sect_per_cyl;
	struct disk_label *dlp = fvp->labelp;
	
#ifdef	FD_DEBUG
	printf("fd_strategy: unit %d block %d cc %d\n", iobp->i_unit,
		iobp->i_bn, iobp->i_cc);
#endif	FD_DEBUG
	if (iobp->i_unit > NFV) {
		iobp->i_error = EDEV;
		goto bad;
	}
	if (iobp->i_part > NFP) {
		iobp->i_error = EDEV;
		goto bad;
	}
	fvp = &fd_global.fd_volume;
	if(!(fvp->format_info.flags & FFI_FORMATTED)) {
		/*
		 * Unformatted. Can't do this.
		 */
		iobp->i_error = ENIO;
		goto bad;
	}
	if(iobp->i_cc & (fvp->format_info.sect_size - 1)) {
		/*
		 * Not multiple of sector size. Forget it.
		 */
#ifdef	FD_DEBUG
		printf("fdstrategy: partial sector\n");
#endif	FD_DEBUG
		iobp->i_error = ECMD;
		goto bad;
	}
	if (!(fvp->format_info.flags & FFI_LABELVALID)) {
		iobp->i_error = ENIO;
		goto bad;
	}
	switch(func) {
	    case READ:
	    	break;
	    default:
	    	iobp->i_error = ECMD;
		goto bad;
	}
	fvp->iob = *iobp;
	fvp->io_flags &= ~FVIOF_SPECIAL;
	rtn = fd_start(fvp);
	if(fvp->dev_ioreq.status)
		iobp->i_error = EDIO;
	return(iobp->i_cc - fvp->bytes_to_go);
bad:
	iobp->i_error = ECMD;
	return(-1);

} /* fdstrategy() */

int fd_command(fd_volume_t fvp,	fd_ioreq_t fdiop)
{
	/* 
	 * returns an errno.
	 * Parameters are passed from the user (and to fd_start()) in a 
	 * fd_ioreq struct.
	 */
	 
	int rtn = 0;
	
#ifdef	FD_DEBUG
	printf("fd_command: volume %d cmd %d\n", fvp->volume_num,
		fvp->dev_ioreq.command);
#endif	FD_DEBUG
	fvp->dev_ioreq = *fdiop;
	fvp->io_flags |= FVIOF_SPECIAL;
	rtn = fd_start(fvp);	
	if(rtn)
		rtn = EDIO;		/* don't set errno of -1! */
	if((rtn == 0) && (fvp->dev_ioreq.status))
		rtn = EDIO;
	/*
	 * replace caller's ioreq with our local ioreq
	 */
	*fdiop = fvp->dev_ioreq;
#ifdef	FD_DEBUG
	printf("fd_command: returning %d\n", rtn);
#endif	FD_DEBUG
	return(rtn);
	
} /* fd_command() */

/* end of fd_driver.c */




