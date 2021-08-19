/*	@(#)sd.c	1.0	09/10/87	(c) 1987 NeXT	*/

/* 
 * sd.c -- implementation of disk specific scsi routines
 * KERNEL VERSION
 *
 * Supports all scsi disks meeting mandatory requirements of
 * ANSI X3.131.
 *
 * FIXME:
 *	sure would be nice if success/failure returns were consistent
 *	Fix timeout stuff in sdcmd when polling/waiting
 *	First do INQUIRY, then START, then do TEST_READY
 *
 * HISTORY
 * 13-Aug-90	Doug Mitchell at NeXT
 *	Added sdd_med_retry_max and SDF_GETLABEL
 *		mechanisms for cleaner "unformatted disk" handling. 
 *  2-Aug-90	Gregg Kellogg (gk) at NeXT
 *	Changes for programatic disksort interface.
 * 10-Jul-90	Gregg Kellogg
 *	Added support for procedural disksort interface.
 * 03-May-90	Doug Mitchell
 * 	Extensive changes for removable media support.
 * 05-Jan-90	Doug Mitchell
 *	Added pmon support.
 * 25-Sep-89	Doug Mitchell
 *	Added OD_SIMULATE logic.
 * 17-Jan-89	Doug Mitchell (dmitch) at NeXT
 *	Enabled disconnects.
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 */ 

#import <sd.h>
#if NSD > 0

#import <sys/errno.h>
#import <sys/types.h>
#import <sys/buf.h>
#import <sys/time.h>
#import <sys/param.h>
#import <sys/kernel.h>
#import <sys/conf.h>
#import <sys/proc.h>
#import <sys/fcntl.h>
#import <kern/task.h>
#import <kern/queue.h>
#import <sys/uio.h>
#import <sys/user.h>
#import <vm/vm_kern.h>
#import <sys/dk.h>
#import <sys/conf.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <next/printf.h>
#import <next/eventc.h>
#import <next/event_meter.h>
#import <nextdev/disk.h>
#import <nextdev/busvar.h>
#import <nextdev/dma.h>
#import <nextdev/screg.h>		/* debug only */
#import <nextdev/scvar.h>		/* debug only */
#import <nextdev/scsireg.h>
#import <nextdev/scsivar.h>
#import <nextdev/sdvar.h>
#import <machine/spl.h>
#import <kern/xpr.h>
#import <next/kernel_pmon.h>		/* debug only */
#import <nextdev/insertmsg.h>
#import <nextdev/voldev.h>

#ifdef	OD_SIMULATE
#import <sys/callout.h>
#endif	OD_SIMULATE


#ifdef 	DEBUG
static int	SD_NODISCONNECT=0;	/* tweakable by debugger */
boolean_t sd_xpr_all=1;
boolean_t sd_xpr_cmd=1;
boolean_t sd_xpr_cntrl=0;
boolean_t sd_xpr_scsi=0;
#else	DEBUG
#define		SD_NODISCONNECT	0	/* normally false */
#endif	DEBUG
/* #define	SD_TRACE	1	/* */

extern struct reg_values s5c_state_values[];	/* in sc.c */
extern struct reg_values sriost_values[];	/* in sg.c */
extern struct reg_values scsi_cmd_values[];	/* in scsi.c */
extern int nchrdev;
extern int nblkdev;

/*
 * static variables
 */
static sd_volume_t sd_volume_p[NUM_SDVP];
static int sd_blk_major;
static int sd_raw_major;
static queue_head_t sd_abort_q;		/* queue of sd_abort_req's to be 
					 * processed by sd_vol_check */
static queue_head_t sd_eject_q;		/* queue of sd_eject_req's to be 
					 * processed by sd_vol_check */
static queue_head_t sd_eject_freeq;	/* queue of free sd_eject_req's */
static int sd_vc_event;
static int sd_vc_delay = SD_VC_DELAY_NORM;
static int sd_thread_init=0;
static int sd_num_drives=0;		/* # disk drives actually present */
static int sd_num_removables=0;		/* # if drives with removable media */
static lock_data_t sd_open_lock;	/* limits open()s to one at a time for
				  	 * time for clean volume-to-drive 
					 * mapping */
static struct bus_device sd_vc_bd;	/* used by sd_vol_check thread */

/*
 * global function declarations accessed via [cb]devsw
 */
int sdopen(dev_t dev, int flag);
int sdclose(dev_t dev);
int sdread(dev_t dev, struct uio *uiop);
int sdwrite(dev_t dev, struct uio *uiop);
int sdstrategy(struct buf *bp);
int sdioctl(dev_t dev, 
	int cmd, 
	caddr_t data, 
	int flag);

/*
 * static function declarations accessed externally via sd_sdsw
 */
static scsi_device_t sdgetsdp(int device);
static int sdslave(scsi_device_t sdp, struct bus_device *bdp);
static void sdattach(scsi_device_t sdp);
static int sdstart(scsi_device_t sdp);
static void sdintr(scsi_device_t sdp);

/*
 * Generic scsi level calls device level via this transfer vector
 */
struct scsi_dsw sd_sdsw = {
	"sd",
	0,
	0,
	sdgetsdp,
	sdslave,
	sdattach,
	sdstart,
	sdintr
};

/*
 * static function declarations accessed only internally
 */
static void sdattach2(sd_volume_t sdvp, u_int polling);
static void sdsetbratio(sd_volume_t sdvp);
static void sdwake(sd_drive_t sddp);
static int sdinquiry(sd_volume_t sdvp, u_int polling);
static int sdmodesen(sd_volume_t sdvp, u_int polling, void *dest, int 
	byte_count);
static void sdspinup(sd_volume_t sdvp, u_int polling);
static int sdtestrdy(sd_volume_t sdvp, u_int polling);
static void sdgetlabel(sd_volume_t sdvp, u_int polling);
static int sdwritelabel(sd_volume_t sdvp);
static int sdreadcapacity(sd_volume_t sdvp, 
	struct capacity_reply *crp, 
	int polling);
int sdchecklabel(struct disk_label *dlp, int blkno);
static int sdcmd_srq(sd_volume_t sdvp, 
	struct scsi_req *srp, 
	u_int polling);
static void sddone(sd_drive_t sddp, 
	long resid, 
	int flags, 
	int io_status);
static void sd_err_info(sd_volume_t sdvp);
#ifdef	OD_SIMULATE
static void delay_biodone(int delay_time, struct buf *bp);
#endif	OD_SIMULATE
static void sd_assign_sv(sd_volume_t sdvp, int drive_num);
static void sd_vol_check();
static void sd_free_sv(sd_volume_t sdvp);
static sd_volume_t sd_new_sv(int vol_num);
static void sd_alert_panel(sd_volume_t sdvp, boolean_t wrong_disk);
static int sd_eject(sd_volume_t sdvp);
static void sd_volume_notify(sd_volume_t sdvp);
static void sd_vol_panel_abort(void *param, int tag, int response_value);
static void sd_vc_assign(sd_volume_t sdvp, sd_drive_t sddp);
static void sd_copy_vol(sd_volume_t source_vol, sd_volume_t dest_vol);
static int sd_vc_timeout();
static int sd_compare_vol(sd_volume_t sdvp1, sd_volume_t sdvp2);
static void sd_thread_timer();
static int sd_vstart(sd_volume_t sdvp);
static void sd_volume_done(sd_volume_t sdvp);
static void sdd_init(int drive_num);

/*
 * prototypes for misc. kernel functions
 */
extern void biodone(struct buf *bp);
extern void *kalloc(int size);

#ifndef	sizeofA
#define	sizeofA(x)	(sizeof(x)/sizeof((x)[0]))
#endif	!def(sizeofA)

static scsi_device_t sdgetsdp(int device)
{
	return(&sd_sd[device]);
}

static int
sdslave(scsi_device_t sdp, struct bus_device *bdp)
{
	int drive_num = sdp - &sd_sd[0];
	sd_drive_t sddp = &sd_sdd[drive_num];
	sd_volume_t sdvp;
	struct inquiry_reply *irp;
	int error;
	char *cp;
	struct bdevsw *bd;
	struct cdevsw *cd;

	XCDBG(("sdslave: target = %d  lun = %d\n",
		sdp->sd_target,sdp->sd_lun));
	sdd_init(drive_num);
	/*
	 * initialize a scsi_disk_volume for this dev. At autoconf time, 
	 * volumes map 1-to-1 to drives.
	 */
	sd_volume_p[drive_num] = sdvp = sd_new_sv(drive_num);
	sd_assign_sv(sdvp, drive_num);
	
	/* 
	 * Do an INQUIRY cmd. Note this should succeed even for a drive
	 * with no volume mounted.
	 */
	error = sdinquiry(sdvp, SDF_POLLING);

	/*
	 * if command wasn't successful or it's not a disk out there,
	 * slave fails
	 */
	irp = sddp->sdd_irp;
	if (error || 
	    ((irp->ir_devicetype != DEVTYPE_DISK) &&
	     (irp->ir_devicetype != DEVTYPE_WORM) &&
	     (irp->ir_devicetype != DEVTYPE_READONLY))) {
#ifdef	SD_TRACE
		scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "INQUIRY");
		printf("\tdevtype=%d\n", irp->ir_devicetype);
#endif	SD_TRACE
		XDBG(("sdslave: DRIVE %d NOT PRESENT\n", drive_num));
		/*
		 * Free this volume struct; mark drive struct as "no
		 * volume mounted".
		 */
		sd_free_sv(sdvp);
		sd_volume_p[drive_num] = NULL;
		sddp->sdd_sdvp = NULL;
		return(0);
	}
	/* 
	 * OK; this is a SCSI disk. Tell a little about it. Note that we NEVER
	 * will free this volume's scsi_disk_volume struct; it's the primary 
	 * volume for this drive and it never goes away.
	 */
	sd_num_drives++;
	if(irp->ir_removable)
		sd_num_removables++;
	sdp->sd_devtype = 'd';
	for (cp=irp->ir_revision-1; cp >= irp->ir_vendorid && *cp == ' '; cp--)
		continue;
	*++cp = '\0';
	if (cp != irp->ir_vendorid)
		printf("%s as ", irp->ir_vendorid);	
	/* 
	 * reserve this lun as ours.
	 */
	sdp->sd_scp->sc_lunmask[sdp->sd_target] |= 1 << sdp->sd_lun;
	bdp->bd_slave = SCSI_SLAVE(sdp->sd_target, sdp->sd_lun);
	
	/*
	 * One-time only initialization
	 */
	if(drive_num == 0) {		
		/* 
		 * remember major device numbers.
		 */
		for (bd = bdevsw; bd < &bdevsw[nblkdev]; bd++)
			if (bd->d_open == (int (*)())sdopen)
				sd_blk_major = bd - bdevsw;
		for (cd = cdevsw; cd < &cdevsw[nchrdev]; cd++)
			if (cd->d_open == (int (*)())sdopen)
				sd_raw_major = cd - cdevsw;
		/*
		 * initialize queues used by sd_vol_check.
		 */
		queue_init(&sd_abort_q);
		queue_init(&sd_eject_q);
		queue_init(&sd_eject_freeq);
		lock_init(&sd_open_lock, TRUE);         /* can_sleep true */
		/*
		 * initialize scsi_{device, disk_drive} and bus_device structs
		 * for the sd_vol_check thread. A copy of the current sdp
		 * will do for now; sd_vol_check will fill in sd_target and 
		 * sd_lun as needed.
		 */
		sd_sd[SVC_DEVICE] = *sdp;
		sdd_init(SVC_DEVICE);
		sd_vc_bd = *sdp->sd_bdp;
		sd_vc_bd.bd_dk = 0;			/* no io_stats */
		sd_vc_bd.bd_alive = 1;
		sd_sd[SVC_DEVICE].sd_bdp = &sd_vc_bd;
		
		/*
	 	 * start up timer to get sd_vol_check thread started.
	 	 */
		timeout((int(*)())sd_thread_timer, 0, hz);
	}
	return(1);
	
} /* sdslave() */

static void
sdattach(scsi_device_t sdp)
{
	sd_drive_t sddp = &sd_sdd[sdp - &sd_sd[0]];
	
	sdattach2(sddp->sdd_sdvp, SDF_POLLING);
	sddp->sdd_flags &= ~SDF_POLLING;
}

static void
sdattach2(sd_volume_t sdvp, u_int polling)
{
	sd_drive_t sddp = sdvp->sdv_sddp;
	scsi_device_t sdp = sddp->sdd_sdp;
	struct capacity_reply *crp;
	struct disk_label *dlp;
	int tries, s;
	extern hz;
	int no_disk=0;
	
	/* 
	 * We can assume that there is a valid sd_volume_p and
	 * sd_sdd for this unit. Now we basically attempt to read the 
	 * label.
	 */
	XCDBG(("sdattach2: vol %d target %d  lun %d\n",
		sdvp->sdv_volume_num, sdp->sd_target,sdp->sd_lun));
	ASSERT(sddp != NULL);
	dlp = sdvp->sdv_dlp;
	crp = sdvp->sdv_crp;
	tries = 0;
	do {
		sdspinup(sdvp, polling);	/* note - error on this (e.g.,
						 * check status because command
						 * is not implemented, or unit
						 * attention) is OK */
		if (tries) {
			if (tries < 0) {
				printf("Waiting for drive to come ready\n");
				tries = 1;
			} else {
				printf(".");
				tries++;
			}
			if (polling == SDF_INTERRUPT) {
				/*
				 * This is really being paranoid,
				 * but theoretically we could be delayed
				 * between the timeout and the sleep by
				 * an interrupt, so lockout softclock
				 * so the timeout can't expire before
				 * we're safely asleep.
				 */
				s = splsoftclock();
				timeout(sdwake, sddp, 2 * hz);
				sleep(sddp, PSLEP);
				splx(s);
			} else
				DELAY(1000000);	/* 1 secs */
		} else
			tries = -1;
	} while (! sdtestrdy(sdvp, polling) && tries < SD_RDY_RETRIES);
	if (tries > 0)
		printf("\n");
	sdvp->sdv_flags &= ~SVF_LABELVALID;
	if (tries < SD_RDY_RETRIES) {
		struct mode_sel_data *msdp;
		
		/*
		 * OK, at least we know there's a disk there. try to read the
		 * label.
		 */
		sdvp->sdv_flags |= SVF_MOUNTED;
		sdgetlabel(sdvp, polling);
		sdsetbratio(sdvp);
		/*
		 * See if it's write protected.
		 */
		sdvp->sdv_flags &= ~SVF_WP;
		msdp = (struct mode_sel_data *)kalloc(sizeof(*msdp));
		if(sdmodesen(sdvp, polling, msdp, sizeof(*msdp)) == 0) {
		    	if(msdp->msd_header.msh_wp)
				sdvp->sdv_flags |= SVF_WP;
		}
		kfree(msdp, sizeof(*msdp));
		
		/*
		 * Report what we've learned.
		 */
		if(!(sdvp->sdv_flags & SVF_FORMATTED))
			printf("\tDISK UNFORMATTED\n");
		else if(sdvp->sdv_flags & SVF_LABELVALID) {
			printf("\tDisk Label: %s\n", dlp->dl_label);
			printf("\tDisk Capacity %dMB, Device Block %d bytes\n",
			    (crp->cr_lastlba * crp->cr_blklen) / (1024 * 1024),
			    crp->cr_blklen);
		} else
			printf("\tUNABLE TO READ DISK LABEL\n");
		if(sdvp->sdv_flags & SVF_WP)
			printf("\tDisk is Write Protected\n");
	}
	else 
		no_disk++;
	/*
	 * Some disks give incomplete INQUIRY replies if they aren't
	 * spun-up, so we do another INQUIRY here to get the real truth.
	 */
	(void) sdinquiry(sdvp, polling);
	if(no_disk) {
		/*
		 * mark this drive as having no volume to enable sd_vol_check.
		 */
		sddp->sdd_sdvp = NULL;
		sdvp->sdv_flags &= ~(SVF_MOUNTED | SVF_VALID);
	}
	else if(polling == SDF_POLLING) {
		/*
		 * Queue up automount request.
		 */
#ifdef	notdef
		if(sddp->sdd_irp->ir_removable)
#endif	notdef
			sd_volume_notify(sdvp);
	}
} /* sdattach2() */

static void
sdsetbratio(sd_volume_t sdvp)
{
	scsi_device_t sdp = sdvp->sdv_sddp->sdd_sdp;
	struct capacity_reply *crp;
	struct disk_label *dlp;
	int dk, rps;

	dlp = sdvp->sdv_dlp;
	crp = sdvp->sdv_crp;

	/*
	 * Make sure we've got reasonable block length
	 * parameters.
	 */
	if (crp->cr_blklen == 0) {
		sdvp->sdv_flags &= ~SVF_LABELVALID;
		printf("DEVICE RETURNS INVALID BLOCK LEN\n");
		crp->cr_blklen = 512;		/* has to be SOMETHING! */
		return;
	}
	if(sdvp->sdv_flags & SVF_LABELVALID) {
		if (dlp->dl_secsize < crp->cr_blklen
		    || dlp->dl_secsize % crp->cr_blklen) {
			sdvp->sdv_flags &= ~SVF_LABELVALID;
			printf("FS BLOCK NOT MULTIPLE OF DEVICE BLOCK\n");
			printf("FS block: %d, DEV block: %d\n",
			    dlp->dl_secsize, crp->cr_blklen);
			return;
		}
		/*
		 * FIXME:  Some day, we should tell the FS code
		 * this, rather than have it built in!
		 */
		if (dlp->dl_secsize != DEV_BSIZE) {
			sdvp->sdv_flags &= ~SVF_LABELVALID;
			printf("FS BLOCK NOT EQUAL TO DEV_BSIZE\n");
			printf("FS block: %d, DEV_BSIZE: %d\n",
				dlp->dl_secsize, DEV_BSIZE);
			return;
		}
		sdvp->sdv_bratio = dlp->dl_secsize / crp->cr_blklen;
		if ((dk = sdp->sd_bdp->bd_dk) >= 0) {
			rps = dlp->dl_rpm >= 60 ? dlp->dl_rpm / 60 : 60;
			dk_bps[dk] = dlp->dl_secsize * dlp->dl_nsect * rps;
		}
	}
	else {
		/* 
		 * enable use of live partition, including I/O stats. The
		 * dk_bps field will be meaningless, but has to be non-zero to 
		 * enable the other stats.
		 */
		sdvp->sdv_bratio = 1;
		if((dk = sdp->sd_bdp->bd_dk) >= 0)
			dk_bps[dk] = 500 * 1024;
	}
} /* sdsetbratio() */

static void
sdwake(sd_drive_t sddp)
{
	wakeup(sddp);
}

static int
sdinquiry(sd_volume_t sdvp, u_int polling)
{
	struct scsi_req sr;
	struct cdb_6 *c6p = &sr.sr_cdb.cdb_c6;
	struct inquiry_reply *irp;
	sd_drive_t sddp = sdvp->sdv_sddp;
	
	bzero(&sr,sizeof(sr));
	irp = sddp->sdd_irp;	/* destination of inquiry data */
	c6p->c6_opcode = C6OP_INQUIRY;
	c6p->c6_lun = sddp->sdd_sdp->sd_lun;
	c6p->c6_len = sizeof(*irp);
	sr.sr_addr = (caddr_t)irp;
	sr.sr_dma_max = sizeof(*irp);
	sr.sr_dma_dir = SR_DMA_RD;
	sr.sr_ioto = SD_IOTO_NORM;
	return(sdcmd_srq(sdvp, &sr, polling));
}

static int
sdmodesen(sd_volume_t sdvp, u_int polling, void *dest, int byte_count)
{
	struct scsi_req sr;
	struct cdb_6 *c6p = &sr.sr_cdb.cdb_c6;
	struct inquiry_reply *irp;
	sd_drive_t sddp = sdvp->sdv_sddp;
	
	bzero(&sr,sizeof(sr));
	c6p->c6_opcode = C6OP_MODESENSE;
	c6p->c6_lun = sddp->sdd_sdp->sd_lun;
	c6p->c6_len = byte_count;
	sr.sr_addr = (caddr_t)dest;
	sr.sr_dma_max = byte_count;
	sr.sr_dma_dir = SR_DMA_RD;
	sr.sr_ioto = SD_IOTO_NORM;
	return(sdcmd_srq(sdvp, &sr, polling));
}

static void
sdspinup(sd_volume_t sdvp, u_int polling)
{
	struct scsi_req sr;
	struct cdb_6 *c6p = &sr.sr_cdb.cdb_c6;

	bzero(&sr,sizeof(sr));
	c6p->c6_opcode = C6OP_STARTSTOP;
	c6p->c6_lun = sdvp->sdv_sddp->sdd_sdp->sd_lun;
	c6p->c6_len = C6S_SS_START; 	/* START bit */
	c6p->c6_lba = (1<<16);		/* IMMEDIATE bit */
	sr.sr_addr = (caddr_t)0;
	sr.sr_dma_max = 0;
	sr.sr_dma_dir = SR_DMA_RD;
	sr.sr_ioto = SD_IOTO_NORM;
	sdcmd_srq(sdvp, &sr, polling);
} /* sdspinup() */

static int
sdtestrdy(sd_volume_t sdvp, u_int polling)

	/* returns non-zero iff ready */
{
	struct scsi_req sr;
	struct cdb_6 *c6p = &sr.sr_cdb.cdb_c6;

	bzero(&sr,sizeof(sr));
	c6p->c6_opcode = C6OP_TESTRDY;
	c6p->c6_lun = sdvp->sdv_sddp->sdd_sdp->sd_lun;
	sr.sr_addr = (caddr_t)0;
	sr.sr_dma_max = 0;
	sr.sr_dma_dir = SR_DMA_RD;
	sr.sr_ioto = SD_IOTO_NORM;
	if(sdcmd_srq(sdvp, &sr, polling) ||
	   sr.sr_io_status)
	   	return(0);
	else
		return(1);
	
} /* sdtestrdy() */

static int sd_eject(sd_volume_t sdvp)
{
	struct scsi_req sr;
	struct cdb_6 *c6p = &sr.sr_cdb.cdb_c6;
	sd_drive_t sddp = sdvp->sdv_sddp;
	int rtn;
	
	XCDBG(("sd_eject: vol %d\n", sdvp->sdv_volume_num));
	if(!(sdvp->sdv_flags & SVF_MOUNTED))
		return(0);
	ASSERT(sdvp->sdv_sddp != NULL);
	bzero(&sr,sizeof(sr));
	c6p->c6_opcode = C6OP_STARTSTOP;
	c6p->c6_lun = sdvp->sdv_sddp->sdd_sdp->sd_lun;
	c6p->c6_lba = 0;
	c6p->c6_len = C6S_SS_EJECT;
	c6p->c6_ctrl = CTRL_NOLINK;
	sr.sr_addr = 0;
	sr.sr_dma_max = 0;
	sr.sr_dma_dir = SR_DMA_RD;
	sr.sr_ioto = SD_IOTO_EJECT;
	rtn = sdcmd_srq(sdvp, &sr, SDF_INTERRUPT);
	/*
	 * Avoid touching this drive for a while.
	 */
	event_set_ts(&sdvp->sdv_sddp->sdd_last_eject);
    	/*
	 * Mark this drive as empty.
	 */
	sdvp->sdv_sddp->sdd_sdvp = NULL;  
	sdvp->sdv_flags &= ~SVF_MOUNTED;
	sdvp->sdv_sddp->sdd_flags |= SDF_EJECTING;
	return(rtn);
}

int
sdopen(dev_t dev, int flag)
{
	int volume = SD_VOLUME(dev); 
	u_char partition_bit = 1 << SD_PART(dev);
	sd_volume_t sdvp = sd_volume_p[volume];
	int rtn;
	
	XCDBG(("sdopen dev 0x%x\n", dev));
	if(volume >= NUM_SDV) {
		if(volume == SCD_VOLUME)
			return(0);		/* controller device - nop */
		else
			return(ENXIO);
	}
	/*
	 * We this lock is prevent conflicts between potential mapping we'll
	 * do here and mapping done by the sd_vol_check thread.
	 */
	lock_write(&sd_open_lock);

	/*
	 * Cases to handle:
	 * 1. sd_volume_p valid, SVF_VALID true. Return successfully 
	 *    immediately. We already know everything there is to know about 
	 *    this disk.
	 * 2. No sd_volume_p. We're starting from scratch. Map to first 
	 *    available drive and "fault in" with a Test Unit Ready command.
	 * 3. sd_volume_p exists, but !SVF_VALID. New open of primary volume.
	 *    Trivial mapping case. "Fault in" with a Test Unit Ready command.
	 */
	if(sdvp && 
	  ((sdvp->sdv_flags & (SVF_VALID | SVF_NOTAVAIL)) == SVF_VALID)) {
		lock_done(&sd_open_lock);
     		goto done;			/* case 1 */
	}
	/*
	 * Further actions require volume insertion.
	 */
	if(flag & O_NDELAY) {
		lock_done(&sd_open_lock);
		return(EWOULDBLOCK);
	}
	if(sd_num_removables == 0) {
		lock_done(&sd_open_lock);
		return(ENXIO);
	}
	if(sdvp == NULL) {
		sd_drive_t sddp;
		
		/*
		 * New open for this volume. Need to find a drive.
		 * First look for a drive with no disk.
		 */
		XDBG(("sdopen: NO DRIVE ASSIGNED\n"));
		sdvp = sd_volume_p[volume] = sd_new_sv(volume);
		for(sddp=&sd_sdd[0]; sddp<&sd_sdd[sd_num_drives]; sddp++) {
			if(!sddp->sdd_irp->ir_removable)	/* fixed */
				continue;
			if(sddp->sdd_sdvp != NULL)		/* other vol
								 * mounted */
				continue;
			if(sddp->sdd_intended_sdvp != NULL)	/* expecting an
								 * insert */
				continue;	
			/*
			 * Whew. This drive's fair game. The alert panel will
			 * be generated when it's actually time to do the 
			 * I/O (in sdstart()). Note we do NOT assign 
			 * sddp->sdd_sdvp; that's done when we actually see
			 * the desired volume.
			 */
			sdvp->sdv_sddp = sddp;
			goto do_attach;
		}
		/*
		 * we'll have to kick a disk out. Take the first removable
		 * media drive.
		 */
		for(sddp=&sd_sdd[0]; sddp<&sd_sdd[sd_num_drives]; sddp++) {
			
			if(!sddp->sdd_irp->ir_removable)
				continue;
			XDBG(("sdopen: EJECTING vol %d from drive %d\n",
				sddp->sdd_sdvp->sdv_volume_num, 
				sddp - &sd_sdd[0]));
			/*
			 * All we have to do is assign the sddp to this volume;
			 * sync and eject of old volume will be handled by 
			 * sd_vstart and sd_vol_check.
			 */
			sdvp->sdv_sddp = sddp;
			goto do_attach;
		}
		panic("sdopen: no removable media drives");
	}
	else {
		XDBG((" sdopen: NEW OPEN FOR PRIMARY VOLUME\n"));
		ASSERT(sdvp->sdv_sddp != NULL);
	}
	/*
	 * execute a simple command; this will "fault in" the desired volume.
	 */
do_attach:
	sdvp->sdv_flags |= SVF_NEWVOLUME;
	lock_done(&sd_open_lock);
	sdtestrdy(sdvp, SDF_INTERRUPT);
	if(sdvp->sdv_flags & SVF_NOTAVAIL) 
		return(ENXIO);
	/*
	 * All other errors on the test unit ready command are OK...
	 */
done:
	sdvp->sdv_owner = u.u_ruid;
	if(major(dev) == sd_raw_major)
		sdvp->sdv_raw_open |= partition_bit;
	else
		sdvp->sdv_blk_open |= partition_bit;
	return(0);
}

int sdclose(dev_t dev) 
{
	int volume = SD_VOLUME(dev); 
	u_char partition_bit = 1 << SD_PART(dev);
	sd_volume_t sdvp = sd_volume_p[volume];
	int rtn;
	
	XCDBG(("sdclose dev 0x%x\n", dev));
	if(volume >= NUM_SDV) {
		return(0);					/* nop */
	}
	else if((sdvp->sdv_sddp->sdd_sdp == NULL) ||		/* slave never 
	   						 	 * executed */
	        (!sdvp->sdv_sddp->sdd_sdp->sd_bdp->bd_alive)) {
			return(ENXIO);	
	}	
	/*
	 * remember which device has been closed.
	 */
	if(major(dev) == sd_raw_major)
		sdvp->sdv_raw_open &= ~partition_bit;
	else
		sdvp->sdv_blk_open &= ~partition_bit;
	
	if(!(sdvp->sdv_raw_open | sdvp->sdv_blk_open)) {
		/*
		 * No references remaining. Forget this volume if 
		 * volume is ejected. 
		 */
	   	if(sdvp->sdv_sddp->sdd_irp->ir_removable &&
		   !(sdvp->sdv_flags & SVF_MOUNTED)) {
		   	sd_volume_done(sdvp);
		}
	}
	return(0);
} /* sdclose() */

int
sdread(dev_t dev, struct uio *uiop)
{
	int volume = SD_VOLUME(dev); 
	extern unsigned scminphys();
	int blocksize;
	sd_volume_t sdvp = sd_volume_p[volume];
	
	if (volume >= NUM_SDV)
		return(ENXIO);
	if(sdvp == NULL)
		return(ENXIO);
	if(SD_PART(dev) == SD_LIVE_PART)
	    	blocksize = sdvp->sdv_crp->cr_blklen;
	else
		blocksize = DEV_BSIZE;
	return(physio(sdstrategy, &sd_volume_p[volume]->sdv_sddp->sdd_rbuf, 
		dev, B_READ, scminphys, uiop, blocksize));
}

int
sdwrite(dev_t dev, struct uio *uiop)
{
	int volume = SD_VOLUME(dev); 
	extern unsigned scminphys();
	int blocksize;
	sd_volume_t sdvp = sd_volume_p[volume];
	
	if (volume >= NUM_SDV)
		return(ENXIO);
	if(sd_volume_p[volume] == NULL)
		return(ENXIO);
	if(SD_PART(dev) == SD_LIVE_PART)
	    	blocksize = sdvp->sdv_crp->cr_blklen;
	else
		blocksize = DEV_BSIZE;
	return(physio(sdstrategy, &sd_volume_p[volume]->sdv_sddp->sdd_rbuf, 
		dev, B_WRITE, scminphys, uiop, blocksize));
}

int
sdstrategy(register struct buf *bp)
{
	int volume = SD_VOLUME(bp->b_dev);
	int part = SD_PART(bp->b_dev);
	sd_volume_t sdvp = sd_volume_p[volume];
	sd_drive_t sddp;
	scsi_device_t sdp;
	struct disk_label *dlp;
	struct partition *pp;
	daddr_t bno;
	ds_queue_t *q;
	int nsectrk;
	int s;
	int rtn=0;
	
	/*
	 * before exiting, the device strategy routine must either
	 * biodone the buffer or enqueue it on sdv_queue.  If a buffer
	 * is enqueued and SVF_ACTIVE == 0, sd_vstart() must be called.
	 */
	XDBG(("sdstrategy...device 0x%x\n",bp->b_dev));
	if((volume == SCD_VOLUME) || 			/* controller device */
	   (sdvp == NULL) || 				/* no volume */
	   (volume >= NUM_SDVP)) {			/* invalid volume # */
		bp->b_error = ENXIO;
		goto bad;
	}
	sddp = sdvp->sdv_sddp;
	sdp  = sddp->sdd_sdp;
	dlp  = sdvp->sdv_dlp;
	
	/*
	 * Don't check special commands, rely on sdstart to
	 * reject anything unacceptable
	 */
	if (bp != &sdvp->sdv_cbuf) {
#ifdef	DEBUG
		if(bp->b_flags&B_READ) {
			XCDBG(("sdstrategy: READ target %d block %xH count "
				"%xH\n",
				sdp->sd_target, bp->b_blkno, bp->b_bcount));
		}
		else {
			XCDBG(("sdstrategy: WRITE target = %d block %xH count"
				"%xH\n",
				sdp->sd_target, bp->b_blkno, bp->b_bcount));
		}
#endif	DEBUG
		bno = bp->b_blkno;
		bp->b_sort_key = bno;
		if(SD_PART(bp->b_dev) == SD_LIVE_PART) {
			/*
			 * Live partition - no front porch, no label.
			 */
			int disk_size = sdvp->sdv_crp->cr_lastlba + 1;
			
			if(bno > disk_size) {
				bp->b_error = EINVAL;
				goto bad;
			}
			if (bno == disk_size) {		/* end-of-file */
				bp->b_resid = bp->b_bcount;
				goto done;
			}
		}
		else {
			if (!(sdvp->sdv_flags & SVF_LABELVALID)) {
				bp->b_error = ENXIO;
				goto bad;
			}
			if (part >= sizeofA(dlp->dl_part)) {
				bp->b_error = ENXIO;
				goto bad;
			}
			pp = &dlp->dl_part[part];
			if (pp->p_size == 0 || bno < 0 || bno > pp->p_size) {
				bp->b_error = EINVAL;
				goto bad;
			}
			if (bno == pp->p_size) {	/* end-of-file */
				bp->b_resid = bp->b_bcount;
				goto done;
			}
			bp->b_sort_key += pp->p_base;
			nsectrk = dlp->dl_nsect * dlp->dl_ntrack;
			if (nsectrk > 0)
				bp->b_sort_key /= nsectrk;
		}
		
		/* 
		 * log this event if enabled
		 */
		pmon_log_event(PMON_SOURCE_SCSI,
			bp->b_flags & B_READ ? KP_SCSI_READQ : KP_SCSI_WRITEQ,
  			bp->b_blkno,
  			bp->b_bcount,
  			volume);
			
	} 
	
	/*
	 * add request to per-device queue. Start the I/O if device is 
	 * currently idle.
	 */
	q = &sdvp->sdv_queue;
	s = spln(ipltospl(sdp->sd_scp->sc_ipl));
	if (bp == &sdvp->sdv_cbuf) {
		/*
		 * special commands go LAST to ensure proper ordering of 
		 * eject commands.
		 */
		disksort_enter_tail(q, bp);
	}
	else
		disksort_enter(q, bp);
	if(!(sdvp->sdv_flags & (SVF_ACTIVE | SVF_WAITING))) 
		rtn = sd_vstart(sdvp);
	splx(s);
	return(rtn);
bad:
	bp->b_flags |= B_ERROR;
	rtn = -1;
done:
	biodone(bp);
	return(rtn);
} /* sdstrategy() */

static int sd_vstart(sd_volume_t sdvp)
{
	/* 
 	 * First half of scsi_dstart/sdstart mechanism. 
	 * One of two actions:
	 * -- If vol is currently mounted, and device is currently idle, 
	 *    start up via scsi_dstart().
	 * -- else generate an eject request.
	 */
		 
	sd_drive_t sddp = sdvp->sdv_sddp;
	int rtn = 0;
	
	XDBG(("sd_vstart: \n"));
	ASSERT(sdvp != NULL);
	ASSERT(sdvp->sdv_sddp != NULL);
	ASSERT((sdvp->sdv_flags & (SVF_ACTIVE & SVF_WAITING)) == 0);
	if(sdvp != sddp->sdd_sdvp) {
		struct sd_eject_req *serp;
		
		/*
		 * Volume not mounted. Generate an eject request. Note that we 
		 * do this even if no other volume is mounted so we 
		 * vol_panel_disk_xxx() is called from a thread (not from here,
		 * which could be an interrupt handler).
		 */
		XDBG(("sd_vstart: VOL NOT MOUNTED\n"));
		sdvp->sdv_flags |= SVF_WAITING;
		if(queue_empty(&sd_eject_freeq))
			panic("sd_vstart: sd_eject_freeq empty");
		serp = (struct sd_eject_req *)sd_eject_freeq.next;
		queue_remove(&sd_eject_freeq, serp, struct sd_eject_req *,
			link);
		serp->sdvp = sddp->sdd_sdvp;	/* current volume - could be 
						 * null */
		serp->new_sdvp = sdvp;		/* intended volume */
		queue_enter(&sd_eject_q, serp, struct sd_eject_req *, link);
		thread_wakeup(&sd_vc_event);
	}
	else {
		/*
		 * Normal case...
		 */
		sdvp->sdv_flags |= SVF_ACTIVE;
		if(sddp->sdd_sdp->sd_active == 0)
			rtn = scsi_dstart(sddp->sdd_sdp);
	}
	return(rtn);
} /* sd_vstart() */

static
sdstart(scsi_device_t sdp)
{
	sd_drive_t sddp = &sd_sdd[sdp - &sd_sd[0]];
	sd_volume_t sdvp;
	struct disk_label *dlp;
	struct cdb_10 *c10p = &sdp->sd_cdb.cdb_c10;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	struct scsi_req *srp;
	union cdb *cdbp;
	struct partition *pp;
	struct buf *bp;
	int spb, num_blocks, start_block;
	int nbytes;
	int device;
	
	/*
	 * The job here is to be the first half of the device's
	 * request processing fsm (sdintr is the second half). Volume to 
	 * be serviced is sddp->sdd_sdvp). Buf to be serviced is at head of
	 * that volume's sdv_queue.
	 *
	 * Requests can be initated in one of two ways:
	 *
	 *   -- via cbuf - I/O started via ioctl(SDIOCSRQ). All parameters are 
	 *	explicitly specified by the user in *srp.
	 *
	 *   -- via sdstrategy(bp). Normal file system requests and raw I/O
	 *      requests are done this way. All I/O parameters are specified by
	 *      the system in *bp.
	 * 
	 * This interprets the local state and the request in
	 * the current buf at the head of sdd_queue, builds a cdb, and then
	 * asks the controller to execute it.  We must initialize
	 * bp->b_resid to the portion of the transfer that we're not
	 * even going to attempt; sddone() will add on the portion
	 * attempted that didn't get done.
	 *
	 * Before exiting, the device level start routine must
	 * either call scsi_docmd() to initiate device activity
	 * or call scsi_rejectcmd() to indicate that the buffer
	 * can no longer be processed.  Scsi_rejectcmd will mark
	 * the device inactive and call sdintr() to allow error
	 * processing.
	 */
	XDBG(("sdstart: top: s5c_state = %n\n",sc_s5c[0].s5c_state,
		s5c_state_values));
	sdvp = sddp->sdd_sdvp;
	ASSERT(sdvp != NULL);
	ASSERT(!(sdvp->sdv_flags & SVF_WAITING));

	bp = disksort_first(&sdvp->sdv_queue);
	if (bp)
		disksort_qbusy(&sdvp->sdv_queue);
	dlp = sdvp->sdv_dlp;
	if (sddp->sdd_state == SDDSTATE_STARTINGCMD) {
		/* 
		 * this stuff for only initial command execution, not for
		 * retries.
		 */
		sddp->sdd_gross_retry = 0;	/* gross errors */
		sddp->sdd_busy_retry = 0;	/* busy wait */
		sddp->sdd_med_retry = 0;	/* media / hw retry */
		sddp->sdd_rdy_retry = 0;	/* wait for ready retry */
		sddp->sdd_state = SDDSTATE_DOINGCMD;
	}
	if (sddp->sdd_state != SDDSTATE_GETTINGSENSE) {
		if (bp == &sdvp->sdv_cbuf) {
		    ASSERT(sdvp->sdv_srp != NULL);
		    /* 
		     * start commands initiated by sdcmd_srq().
		     */
		    srp = sdvp->sdv_srp;
		    sdp->sd_disconnectok = 1;	
		    sdp->sd_read         = 
			    (srp->sr_dma_dir == SR_DMA_RD) ? 1 : 0;		
		    sdp->sd_cdb 	     = srp->sr_cdb;
		    sdp->sd_addr 	     = srp->sr_addr;
		    sdp->sd_bcount	     = srp->sr_dma_max;
		    sdp->sd_padcount     = 0xFFFF;	    /* absorb excess */	
		    sdp->sd_pmap         = pmap_kernel();
		    sdp->sd_timeout      = srp->sr_ioto;	
#ifdef	OD_SIMULATE
		    sddp->sdd_opcounter = 1;
#endif	OD_SIMULATE
		} /* bp == cbuf */ 
		else {
		    /* 
		     * normal I/O started by sdstrategy().
		     */
		    bzero(c10p, sizeof(*c10p));
		    c10p->c10_opcode = (bp->b_flags&B_READ)
			? C10OP_READEXTENDED : C10OP_WRITEEXTENDED;
		    c10p->c10_lun = sdp->sd_lun;
		    if(SD_PART(bp->b_dev) != SD_LIVE_PART) {
			if (!(sdvp->sdv_flags & SVF_LABELVALID)) {
				printf("sd%d: invalid label\n",
					sdp - &sd_sd[0]);
				goto err;
			}

			/*
			 * Add in dl_front so partition space starts
			 * at zero offset.
			 */
		        pp = &dlp->dl_part[SD_PART(bp->b_dev)];
			start_block = bp->b_blkno + pp->p_base + dlp->dl_front;
			/*
			 * Convert starting address to SCSI blocks.
			 */
			start_block *= sdvp->sdv_bratio;
			num_blocks = howmany(bp->b_bcount, dlp->dl_secsize);
			num_blocks = MIN(num_blocks, pp->p_size - bp->b_blkno);
			/*
			 * Convert block count to SCSI blocks.
			 */
			num_blocks *= sdvp->sdv_bratio;
		    }
		    else {
		    	struct capacity_reply *crp = sdvp->sdv_crp;
			
		    	/*
			 * Live partition. No label, no front porch,
			 * blocksize is actual device block size.
			 */
			start_block = bp->b_blkno;
		        num_blocks = howmany(bp->b_bcount, crp->cr_blklen);
			num_blocks = MIN(num_blocks, 
				crp->cr_lastlba - start_block + 1);
		    }
		    /*
		     * I/O request is for num_blocks SCSI blocks starting at 
		     * start_block.
		     */
		    c10p->c10_lba = start_block;
		    if (c10p->c10_lba != start_block) {
			    printf("sd%d: start_block too big\n",
				sdp - &sd_sd[0]);
			    goto err;
		    }
		    if (num_blocks > 0xffff || num_blocks <= 0) {
			    printf("sd%d: num_blocks too big\n",
				sdp - &sd_sd[0]);
			    goto err;
		    }
		    c10p->c10_len = num_blocks;
		    c10p->c10_ctrl = CTRL_NOLINK;
		    nbytes = num_blocks * sdvp->sdv_crp->cr_blklen;
		    sdp->sd_bcount = MIN(bp->b_bcount, nbytes);
		    bp->b_resid = bp->b_bcount - nbytes;
		    if (bp->b_resid >= 0)
			    sdp->sd_padcount = 0;
		    else {
			    sdp->sd_padcount = -bp->b_resid;
			    bp->b_resid = 0;
		    }
		    sdp->sd_disconnectok = 1;
		    sdp->sd_timeout = SD_IOTO_NORM;
		    sdp->sd_addr = bp->b_un.b_addr;
		    sdp->sd_read = bp->b_flags & B_READ;
		    if((bp->b_flags & (B_PHYS|B_KERNSPACE)) == B_PHYS) 
			sdp->sd_pmap = vm_map_pmap(bp->b_proc->task->map);
		    else
			sdp->sd_pmap = pmap_kernel();
#if	EVENTMETER
		    if(sdvp->sdv_flags & SVF_LABELVALID) {
			    event_meter (EM_DISK);
			    event_disk (EM_SD, EM_Y_SD, c10p->c10_lba /
				(sdvp->sdv_bratio * dlp->dl_nsect * 
				    dlp->dl_ntrack),
				dlp->dl_ncyl,
				bp->b_flags & B_READ? EM_READ : EM_WRITE);
		    }
#endif	EVENTMETER
#ifdef	OD_SIMULATE
		    /* init the op counter only for first pass thru...
		     * subsequent passes will have sdd_state = 
		     * SDDSTATE_RETRYING.
		     */
		    if(sddp->sdd_state != SDDSTATE_RETRYING) {
			    if(sddp->sdd_simulate && !sdp->sd_read)
				    sddp->sdd_opcounter = SD_ODSIM_WPASSES;
			    else
				    sddp->sdd_opcounter = 1;
		    }
#endif	OD_SIMULATE
		    /* 
			* log this event if enabled
			*/
		    pmon_log_event(PMON_SOURCE_SCSI,
			    bp->b_flags&B_READ ? 
				    KP_SCSI_READ : KP_SCSI_WRITE,
			    bp->b_blkno,
			    bp->b_bcount,
			    sdp - &sd_sd[0]);

		} /* normal I/O */
	} /* not GETTINGSENSE */
do_io:
	if ((device = sdp->sd_bdp->bd_dk) >= 0) {
		dk_busy |= 1 << device;
		dk_xfer[device]++;
		dk_seek[device]++;
		dk_wds[device] += bp->b_bcount >> 6;
	}
	/*
	 * have the controller send the command to the target
	 */
	if(SD_NODISCONNECT)
		sdp->sd_disconnectok = 0;
	return(scsi_docmd(sdp));
err:
	scsi_rejectcmd(sdp);
	return(-1);
} /* sdstart() */

static void
sdintr(scsi_device_t sdp)
{
	sd_drive_t sddp = &sd_sdd[sdp - &sd_sd[0]];
	sd_volume_t sdvp = sddp->sdd_sdvp;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	int device;
	int io_stat;

	/*
	 * We're called when the controller can no longer process
	 * the command indicated in sdp.  Our job is to
	 * advance the driver state machine and deal with any special
	 * recovery and/or error processing.
	 *
	 * Before returning, this module should either call
	 * scsi_dstart() if there's more to do to accomplish or
	 * recover this transfer; or if we're done or giving up,
	 * call sddone() possibly indicating B_ERROR.  Sddone() will
	 * biodone() the buffer and advance the queue.
	 *
	 */
	if ((device = sdp->sd_bdp->bd_dk) >= 0)
		dk_busy &=~ (1 << device);
	XDBG(("sdintr: sd_state = %n\n",sdp->sd_state,scsi_sdstate_values));
	XDBG(("        target %d   lun %d\n",sdp->sd_target,sdp->sd_lun));

	switch (sddp->sdd_state) {
	case SDDSTATE_DOINGCMD:
	case SDDSTATE_RETRYING:
	
	    switch (sdp->sd_state) {
		case SDSTATE_SELTIMEOUT:	/* select timed out */
		    /* no notification at boot...*/
		    if(!(sddp->sdd_flags & SDF_POLLING))	
		    	scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "ERROR");
		    sddone(sddp, sdp->sd_resid, B_ERROR,SR_IOST_SELTO);
		    break;
		case SDSTATE_REJECTED:		/* we wouldn't do it */
		case SDSTATE_UNIMPLEMENTED:	/* ctrlr couldn't do it */
		    scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "ERROR");
		    sddone(sddp, sdp->sd_resid, B_ERROR,SR_IOST_CMDREJ);
		    break;
		case SDSTATE_TIMEOUT:		/* target never reselected */
		    io_stat = SR_IOST_IOTO;
		    goto sd_retry;
		case SDSTATE_ABORTED:		/* ctrlr aborted cmd */
		    io_stat = SR_IOST_TABT;
		    goto sd_retry;
		case SDSTATE_DROPPED:		/* target dropped connection */
		    io_stat = SR_IOST_BV;
sd_retry:
		    if (++sddp->sdd_gross_retry >= SD_GROSS_RETRIES) {
			    if (sdp->sd_bdp->bd_alive)
				    scsi_msg(sdp,sdp->sd_cdb.cdb_opcode,
					"ERROR");
			    sddone(sddp, sdp->sd_resid, B_ERROR,io_stat);
		    } else {
		    	    sddp->sdd_state = SDDSTATE_RETRYING;
			    scsi_dstart(sdp);
		    }
		    break;
		case SDSTATE_RETRY:		/* SCSI bus reset, retry */
		    scsi_dstart(sdp);
		    break;
		case SDSTATE_COMPLETED:
		    switch (sdp->sd_status & STAT_MASK) {
		    case STAT_GOOD:
			/* 
			 * Make sure we moved expected number of bytes for read 
			 * and write commands.
			 */
good_status:
			if((sdp->sd_cdb.cdb_opcode == C10OP_READEXTENDED) ||
			   (sdp->sd_cdb.cdb_opcode == C10OP_WRITEEXTENDED)) {
			       	if(sdp->sd_resid != 0) {
				    if(++sddp->sdd_med_retry < 
				    	 sddp->sdd_med_retry_max) {
				    
					/* retry the operation */
					printf("sd%d: Incomplete disk "
					  "transfer; bytes moved = 0x%x,"
					  " resid = 0x%x, retry %d\n", 
					   sdp - sd_sd, 
					   sdp->sd_bytes_moved, 
					   sdp->sd_resid,
					   sddp->sdd_med_retry);
					sddp->sdd_state = SDDSTATE_RETRYING;
					scsi_dstart(sdp);
					break;
				    }
				    else {
					/* retries exhausted */
					printf("sd%d: Incomplete disk transfer"
						" - FATAL\n", sdp - sd_sd);
					sddone(sddp, sdp->sd_resid,
					    B_ERROR,SR_IOST_BCOUNT);
					break;				
				    }
				} /* resid != 0 */
			} /* read or write command */
#ifdef	OD_SIMULATE
			if(sddp->sdd_simulate) {
			    if((--(sddp->sdd_opcounter)) > 0) {
				/* execute next write pass */
				XCDBG(("OD simulation : sdd_opcounter = %d\n",
					sddp->sdd_opcounter));
				sddp->sdd_state = SDDSTATE_RETRYING;
				scsi_dstart(sdp);
				break;
			    }
			}
			/* else done */
#endif	OD_SIMULATE
			sddone(sddp, sdp->sd_resid, 0,SR_IOST_GOOD);
			break;
		    case STAT_CHECK:
		    	/*
			 * Special case - check status on Test Unit Ready for
			 * removable media drives - this usually means 
			 * No Disk. avoid error reporting, request sense.
			 */
			if(sddp->sdd_irp->ir_removable && 
			   (sdp->sd_cdb.cdb_opcode == C6OP_TESTRDY)) {
			   	XDBG(("sdintr: Test Unit Ready/Check Stat for"
				      " Removable Media\n"));
			    	sddone(sddp, sddp->sdd_savedresid, B_ERROR,
				    SR_IOST_CHKSNV);
				break;
			}
			/* 
			 * Else do a REQUEST SENSE to find out why.
			 */
			sddp->sdd_state = SDDSTATE_GETTINGSENSE;
			sddp->sdd_savedresid = sdp->sd_resid;
			sddp->sdd_savedopcode = sdp->sd_cdb.cdb_opcode;
			c6p->c6_opcode = C6OP_REQSENSE;
			c6p->c6_lun = sdp->sd_lun;
			c6p->c6_lba = 0;
			c6p->c6_len = sizeof(*sdvp->sdv_erp);
			c6p->c6_ctrl = CTRL_NOLINK;
			sdp->sd_disconnectok = 0;
			sdp->sd_read = 1;
			sdp->sd_addr = (caddr_t)sdvp->sdv_erp;
			sdp->sd_bcount = sizeof(*sdvp->sdv_erp);
			sdp->sd_padcount = 0;
			sdp->sd_pmap = pmap_kernel();
			sdp->sd_timeout = SD_IOTO_SENSE;
			scsi_dstart(sdp);
			break;
		    case STAT_BUSY:
			if (++sddp->sdd_busy_retry < SD_BUSY_RETRIES) {
			    /* busy - wait a bit and retry */ 
			    if(!(sddp->sdd_flags & SDF_POLLING)) 
				printf("Target %d: BUSY; retry %d\n",
				    sdp->sd_target, sddp->sdd_busy_retry);
			    DELAY(100000);	/* 100 ms */
			    sddp->sdd_state = SDDSTATE_RETRYING;
			    scsi_dstart(sdp);
			    break;
			}
			/* else fall through ... */
		    default:
			scsi_msg(sdp,sdp->sd_cdb.cdb_opcode, "ERROR");
			sddone(sddp, sdp->sd_resid, B_ERROR,
				ST_IOST_BADST);
			break;
		    }
		    break;
		default:
		    panic("sdintr: bad sd_state");
	    }
	    break;
	case SDDSTATE_GETTINGSENSE:
	    switch (sdp->sd_state) {
		case SDSTATE_SELTIMEOUT:	/* select timed out */
		    if (++sddp->sdd_gross_retry >= SD_GROSS_RETRIES) {
			    scsi_msg(sdp, sddp->sdd_savedopcode, "ERROR");
			    sddone(sddp, sddp->sdd_savedresid, B_ERROR,
				    SR_IOST_SELTO);
		    } else
			    scsi_dstart(sdp);	/* retry the Request Sense */
		    break;
		case SDSTATE_RETRY:		/* SCSI bus reset */
		case SDSTATE_TIMEOUT:		/* target never reselected */
		case SDSTATE_ABORTED:		/* ctrlr aborted cmd */
		case SDSTATE_DROPPED:		/* target dropped connection */
		case SDSTATE_REJECTED:		/* we wouldn't do it */
		case SDSTATE_UNIMPLEMENTED:	/* ctrlr couldn't do it */
		    scsi_msg(sdp, sddp->sdd_savedopcode, "ERROR");
		    sddone(sddp, sddp->sdd_savedresid, B_ERROR,
			    SR_IOST_CHKSNV);
		    break;
		case SDSTATE_COMPLETED:
		    switch (sdp->sd_status & STAT_MASK) {
			default:
			case STAT_BUSY:
			case STAT_CHECK:
			    if(!(sddp->sdd_flags & SDF_POLLING))
				    scsi_msg(sdp, sddp->sdd_savedopcode,
				    	 "ERROR");
			    sddone(sddp, sddp->sdd_savedresid, B_ERROR,
				    SR_IOST_CHKSNV);
			    break;
			case STAT_GOOD:
			    switch (sdvp->sdv_erp->er_sensekey) {
			    
				case SENSE_NOTREADY:
				    /* only retry this one here if NOT in 
				     * polling mode; sdattach2() has its own
				     * mechanism...
				     */
				    if(sddp->sdd_flags & SDF_POLLING) {
				        sddone(sddp, sddp->sdd_savedresid, 0,
					    SR_IOST_CHKSV);
				        break;
				    }
				    else {
					if(++sddp->sdd_rdy_retry < 
					    SD_RDY_RETRIES) {
					    /* retry the operation */
					    printf("Target %d: NOT READY; "
					        "retry %d\n", sdp->sd_target,
					 	sddp->sdd_rdy_retry);
					    sddp->sdd_state = 
					    	SDDSTATE_RETRYING;
					    scsi_dstart(sdp);
					}
					else {
					    /* retries exhausted */
					    sd_err_info(sdvp);
					    sddone(sddp, sddp->sdd_savedresid,
						B_ERROR,SR_IOST_CHKSV);
					}
				    }
				    break;
				    
				case SENSE_NOSENSE:
				case SENSE_RECOVERED:
				    /*
				     * Handle as "good status"
				     */
				    sdp->sd_resid = sddp->sdd_savedresid;
				    goto good_status;

				case SENSE_MEDIA:
				    {
					int block_in_err;
					
					if(++sddp->sdd_med_retry < 
					     sddp->sdd_med_retry_max) {
					    /* retry the operation */
					    block_in_err = 
					    (sdvp->sdv_erp->er_infomsb << 24) | 
					        	sdvp->sdv_erp->er_info;
					    if(!(sddp->sdd_flags & 
					    	(SDF_POLLING | SDF_GETLABEL)))
						printf("Target %d: MEDIA ERROR"
						   "; block %XH retry %d\n",						    
						   sdp->sd_target,block_in_err,
						   sddp->sdd_med_retry);
					    sddp->sdd_state = 
					    	SDDSTATE_RETRYING;
					    scsi_dstart(sdp);
					}
					else {
					    /* retries exhausted */
					    if(!(sddp->sdd_flags & 
					    	(SDF_POLLING | SDF_GETLABEL)))
					    	    sd_err_info(sdvp);
					    sddone(sddp, sddp->sdd_savedresid,
						B_ERROR,SR_IOST_CHKSV);
					}
				    }
				    break;
				    
				case SENSE_DATAPROTECT:
				    /*
				     * Write protect violation. 
				     */
				    sddone(sddp, sddp->sdd_savedresid, B_ERROR,
				    	SR_IOST_WP);
				    break;
				    	    
				case SENSE_UNITATTENTION:
				    /* 
				     * Disk Has Changed! This OK for 
				     * non-removable disks; it's catastrophic
				     * for removable media.
				     */
				     
				    /*
				     * If the label isn't valid and we get a 
				     * unit attention, retry the command; since 
				     * the label isn't valid we can't be
				     * trying to do anything too dangerous. On
				     * the otherhand, if the label is valid, 
				     * then the unit attention indicates
				     * that the world as we know it has 
				     * changed, so we better indicate that the 
				     * label is no longer valid and blow off
				     * this command.
				     *
				     * FIXME: This should be a lot more 
				     * sophisticated.
				     */
#ifdef	SD_TRACE
				    printf("unit attention\n");
#endif	SD_TRACE
				    if (++sddp->sdd_rdy_retry < SD_RDY_RETRIES
					&& !sddp->sdd_irp->ir_removable) {
					sddp->sdd_state = SDDSTATE_RETRYING;
					scsi_dstart(sdp);
					break;
				    }
				    printf("sd%d: UNIT ATTENTION\n",
				    	sdvp->sdv_volume_num);
				    sdvp->sdv_flags &= ~SVF_LABELVALID;
				    /* fall through .... */
				default:
				    if ((sddp->sdd_savedopcode != 
				             C6OP_STARTSTOP) &&
				        !(sddp->sdd_flags & SDF_POLLING)) {
					    /* avoid error message at boot */
					    scsi_msg(sdp,
						sddp->sdd_savedopcode,
						"ERROR");
					    sd_err_info(sdvp);
				    }
				    sddone(sddp, sddp->sdd_savedresid,
					B_ERROR,SR_IOST_CHKSV);
				    break;
			    }
			    break;
		    }
		    break;
		default:
		    scsi_msg(sdp, sddp->sdd_savedopcode, "PANIC");
		    panic("sdintr: bad sd_state");
	    }
	    break;
	default:
	    scsi_msg(sdp,sdp->sd_cdb.cdb_opcode, "PANIC");
	    panic("sdintr: bad sdd_state");
	}
} /* sdintr() */

static void
sd_err_info(sd_volume_t sdvp)
{
	int 			block_in_err;	/* in scsi blocks */
	int 			sect_in_err;	/* in parition-relative 
						 *     SECTORS */
	int 			part;		/* partition */
	struct partition 	*pp;
	
	scsi_sensemsg(sdvp->sdv_sddp->sdd_sdp, sdvp->sdv_erp);
	block_in_err = (sdvp->sdv_erp->er_infomsb << 24) | 
		        sdvp->sdv_erp->er_info;
	if(sdvp->sdv_flags & SVF_LABELVALID) {
	
		struct disk_label *dlp = sdvp->sdv_dlp;
		
		sect_in_err = block_in_err/sdvp->sdv_bratio;
	
		/*  which partition? */
		if(sect_in_err >= dlp->dl_front) {
			pp = &dlp->dl_part[0];
			part = 0;
			sect_in_err -= dlp->dl_front;
			while(pp->p_base < sect_in_err) {
				/* break out one partition past sect_in_err */
				pp++;
				part++;
				if((pp->p_base == -1) || (part == NPART))
					break;		/* out of partitions */
			}
			pp--;
			sect_in_err -= pp->p_base;	/* now relative to 
							 * p_base */
			part = 'a' + part - 1;
			printf("    SCSI Block in error = %d; Partition %c "
				"F.S. sector %d\n",
				block_in_err,part,sect_in_err);
		}
		else 
			printf("    SCSI Block in error = %d (front porch)\n",
				block_in_err);
	}
	else
		printf("    SCSI Block in error = %d (no valid label)\n",
			block_in_err);
} /* sd_err_info() */

static void
sddone(sd_drive_t sddp, 
	long resid, 
	int flags,			/* for bp->b_flags */
	int io_status)			/* for scsi_req.sr_io_status */
{
	struct buf *bp;
	struct scsi_req *srp;
	int special_cmd=0;
#ifdef	OD_SIMULATE
	struct cdb_10 *c10p = &sddp->sdd_sdp->sd_cdb.cdb_c10;
	int seektime;		/* in ms */
#endif	OD_SIMULATE
	struct cdb_6 *c6p = &sddp->sdd_sdp->sd_cdb.cdb_c6;
	sd_volume_t sdvp;
	
	XDBG(("sddone: drive %d resid %d flags 0x%x io_status %n\n", 
		sddp-&sd_sdd[0], resid, flags, io_status, sriost_values));
	/*
	 * The current request disksort_first(&sdvp->sdv_queue) has either
	 * succeeded or we've given up trying to make it work.
	 * Advance sdd_queue and do a biodone().
	 */
	sdvp = sddp->sdd_sdvp;
	if(sdvp == NULL) {
		/*
		 * This has to be a "volume not available" I/O complete
		 * performed by sd_vol_check.
		 */
		sdvp = sddp->sdd_intended_sdvp;
		ASSERT(sdvp != NULL);
		sddp->sdd_intended_sdvp = NULL;
	}
	bp = disksort_first(&sdvp->sdv_queue);
	if (bp == NULL)
		panic("sddone: no buf on sdv_queue");

	bp->b_resid += resid;
	bp->b_flags |= flags;
	if (flags & B_ERROR) {
		/*
		 * Some special cases of error detected after sd_vstart:
		 */
		switch(io_status) {
		    case SR_IOST_WP:
			bp->b_error = EROFS;
			break;
		    case SR_IOST_VOLNA:
		    	bp->b_error = ENXIO;
			break;
		    default:
			bp->b_error = EIO;
			break;
		}
	}
	(void) disksort_remove(&sdvp->sdv_queue, bp);
	if(bp == &sdvp->sdv_cbuf) {
	
		/* command started by sdcmd_srq(). Log bytes transferred and
		 * I/O status.
		 */
		srp = sdvp->sdv_srp;
		special_cmd++;
		srp->sr_dma_xfr = srp->sr_dma_max - resid;
		srp->sr_io_status = io_status;
		if(io_status == SR_IOST_CHKSV)
			srp->sr_scsi_status = STAT_CHECK;
		else
			srp->sr_scsi_status = sddp->sdd_sdp->sd_status;	
		/*
		 * Copy sense data out to *srp if appropriate.
		 */
		if(io_status == SR_IOST_CHKSV)
			srp->sr_esense = *sdvp->sdv_erp;
	}
#ifdef	OD_SIMULATE
	else {
		/* normal I/O started via sdstrategy. Calculate delayed 
		 * biodone() time before we relinquish device.
		 * Seek time =
		 * (cur seek distance / max seek distance) * 
		 * 	sddp->sdd_maxseektime.
		 */
		if(c10p->c10_lba > sddp->sdd_lastblock)
			seektime = c10p->c10_lba - sddp->sdd_lastblock;
		else
			seektime = sddp->sdd_lastblock - c10p->c10_lba;
		seektime *= sddp->sdd_maxseektime;
		seektime /= sddp->sdd_sdvp->sdv_crp->cr_lastlba;
		/* offset by avg hard disk seek time */
		if(seektime >= SD_ODSIM_WIN_SEEK)
			seektime -= SD_ODSIM_WIN_SEEK;
		sddp->sdd_lastblock = c10p->c10_lba;
	}
#endif	OD_SIMULATE
out:
	sddp->sdd_state = SDDSTATE_STARTINGCMD;
	/*
	 * If this volume has no more work to do, deactivate it and remove it
	 * from this drive's work queue. Then if there are any more volumes on
	 * our work queue, start the first one up. 
	 */
	if (disksort_first(&sdvp->sdv_queue) == NULL) {
		/*
		 * This volume is done.
		 */
		sdvp->sdv_flags &= ~SVF_ACTIVE;
	}
	else 
		sd_vstart(sdvp);

	/* 
	 * log this event if enabled
	 */
	pmon_log_event(PMON_SOURCE_SCSI,
			bp->b_flags&B_READ ? KP_SCSI_READ_CMP : KP_SCSI_WRITE_CMP,
			bp->b_blkno,
			bp->b_bcount,
			sddp - sd_sdd);
			
#ifdef	OD_SIMULATE
	if(sddp->sdd_simulate && !special_cmd) 
		delay_biodone(seektime, bp);
	else
		biodone(bp);
#else	OD_SIMULATE
	biodone(bp);
#endif	OD_SIMULATE
}

#ifdef	OD_SIMULATE

static void delay_biodone(int seektime, 	/* in ms */
			  struct buf *bp) 	/* buf to biodone() */
{

	/* 
	 * do a delayed call to biodone() to simulate a long OD seek. 
	 */
	
	struct timeval tv;	
	
	XCDBG(("delay_biodone...seektime = %d\n",seektime));
	tv.tv_sec = 0;
	tv.tv_usec = seektime * 1000;
	us_timeout(biodone, bp, &tv, CALLOUT_PRI_SOFTINT1);
	return;
}
#endif	OD_SIMULATE

int
sdioctl(dev_t dev, 
	int cmd, 
	caddr_t data, 
	int flag)
{
	int volume = SD_VOLUME(dev);
	sd_volume_t sdvp = sd_volume_p[volume];
	sd_drive_t sddp;
	struct drive_info *dip;
	struct disk_req *drp;
	struct scsi_req *srp;
	caddr_t cp = *(caddr_t *)data;
	caddr_t ap;
	char *user_addr;
	int i, nblk, error = 0;
	u_short *dl_cksum, size;
	struct tsval ts;
	
	if(volume > SCD_VOLUME)
		return(ENXIO);
	switch(cmd) {
	/*
	 * First do the ioctls which don't need a volume
	 */
	case DKIOCGFREEVOL:
		if(sd_num_removables == 0)
			return(ENODEV);
	    	/* 
		 * find first free volume.
		 */
		for(i=0; i<NUM_SDV; i++) {
		    	if((sd_volume_p[i] == NULL) ||
			   (!(sd_volume_p[i]->sdv_flags & SVF_VALID))) {
				*(int*) data = i;
				return(0);
			}
		}
		*(int*) data = -1;		/* no free volumes */
		return (0);
		
#ifdef	OD_SIMULATE
	case SDIOCODSIM:
		sddp->sdd_simulate = *((int *)data);
		break;
	case SDIOCSMAXSEEK:
		sddp->sdd_maxseektime = *((int *)data);
		break;
	case SDIOCGMAXSEEK:
		*((int *)data) = sddp->sdd_maxseektime;
		break;
#endif	OD_SIMULATE
	default: 
		break;
	}
	
	/*
 	 * Remainder need a valid volume.
	 */
	if(sdvp == NULL)
		return(ENXIO);
	sddp = sdvp->sdv_sddp;
	
	switch (cmd) {
	case DKIOCSFORMAT:
		if(*(int *)data)
			sdvp->sdv_flags |= SVF_FORMATTED;
		else
			sdvp->sdv_flags &= ~SVF_FORMATTED;
		/*
		 * In either case, we no longer have a valid label.
		 */
		sdvp->sdv_flags &= ~SVF_LABELVALID;
		break;
	case DKIOCGFORMAT:
		*(int *)data = sdvp->sdv_flags & SVF_FORMATTED ? 1 : 0;
		break;
	case DKIOCGLABEL:
		if (!(sdvp->sdv_flags & SVF_LABELVALID))
			return(ENXIO);
		error = copyout(sdvp->sdv_dlp, cp, sizeof(*sdvp->sdv_dlp));
		break;
		
	case DKIOCSLABEL:
		if (!suser())
			return(u.u_error);
		if (error = copyin(cp, sdvp->sdv_dlp, sizeof(*sdvp->sdv_dlp)))
			break;
		if (sdvp->sdv_dlp->dl_version == DL_V1 ||
		    sdvp->sdv_dlp->dl_version == DL_V2) {
			size = sizeof (struct disk_label);
			dl_cksum = &sdvp->sdv_dlp->dl_checksum;
		} else {
			size = sizeof (struct disk_label) -
				sizeof (sdvp->sdv_dlp->dl_un);
			dl_cksum = &sdvp->sdv_dlp->dl_v3_checksum;
		}
		/*
		 * tag label with time. Assume this label is for block 0.
		 */
		event_set_ts(&ts);
		sdvp->sdv_dlp->dl_tag = ts.low_val;
		sdvp->sdv_dlp->dl_label_blkno = 0;
		*dl_cksum = 0;
		*dl_cksum = checksum_16 (sdvp->sdv_dlp, size >> 1);
		if (! sdchecklabel(sdvp->sdv_dlp, 0))
			return(EINVAL);
		if (sdwritelabel(sdvp))
			return(EIO);
		break;
		
	case DKIOCINFO:
		dip = (struct drive_info *)data;
		bzero(dip->di_name, MAXDNMLEN);
		/*
		 * The idea here is to compress multiple blanks out of
		 * the vendor id and product ID. Note that Inquiry data is 
		 * always specific to the drive, not the volume...
		 */
		ap = dip->di_name;
		cp = sddp->sdd_irp->ir_vendorid;
		while (cp < sddp->sdd_irp->ir_revision
		    && ap < &dip->di_name[MAXDNMLEN]) {
			*ap++ = *cp;
			if (*cp++ == ' ') {
				while (*cp == ' ')
					cp++;
			}
		}
		while (*--ap == ' ')
			*ap = '\0';
		dip->di_devblklen = sdvp->sdv_crp->cr_blklen;
		nblk = howmany(sizeof(*sdvp->sdv_dlp), 
		               sdvp->sdv_crp->cr_blklen);
		for (i = 0; i < NLABELS; i++)
			dip->di_label_blkno[i] = nblk * i;
		dip->di_maxbcount = 256 * sdvp->sdv_crp->cr_blklen;
		break;
				
	case SDIOCSRQ:				/* I/O request via scsi_req */
		if (!suser())
			return(u.u_error);
		srp = (struct scsi_req *)data;
		
		/* if user expects to do some DMA, get some kernel memory. Copy
		 * in the user's data if a DMA write is expected.
		 */
		if (srp->sr_dma_max != 0) {
			if ((ap = (caddr_t)kmem_alloc(kernel_map,
			    srp->sr_dma_max)) == 0) {
				XDBG(("        ...kmem_alloc() failed\n"));
				srp->sr_io_status = SR_IOST_MEMALL;
				return(ENOMEM);
			}
			if(srp->sr_dma_dir == SR_DMA_WR) {
				if (error = copyin(srp->sr_addr, ap,
						   srp->sr_dma_max)) {
				    XDBG(("        ...copyin() returned %d\n",
					    error));
				    srp->sr_io_status = SR_IOST_MEMF;
				    goto err_exit;
				}
			}
		}
		else
			ap = 0;		/* for alignment checks */
			
		user_addr = srp->sr_addr;
		srp->sr_addr = ap;
		error = sdcmd_srq(sdvp, srp, SDF_INTERRUPT);
		srp->sr_addr = user_addr;
		if ((srp->sr_dma_dir == SR_DMA_RD) && (srp->sr_dma_xfr != 0))
			error = copyout(ap, user_addr, srp->sr_dma_xfr);
err_exit:
		if (srp->sr_dma_max != 0)
			kmem_free(kernel_map, ap, srp->sr_dma_max);
		break;
		
	case DKIOCEJECT:
		if(!(sdvp->sdv_flags & SVF_MOUNTED)) {
			/*
			 * Nothing to do.
			 */
			error = 0;
			break;
		}
		/* 
		 * only allow suser and owner of volume to eject.
		 */
		if (!suser() && (u.u_ruid != sdvp->sdv_owner))
			error = u.u_error;
		else {
			/*
		 	 * This is a (legal) nop for non-removable drives.
			 */
			if(!(sdvp->sdv_sddp->sdd_irp->ir_removable)) {
				error = 0;
				break;
			}
			/*
		 	 * Sync the file system on sdvp before the eject.
		 	 */
			update(makedev(sd_blk_major,
				       SD_DEVT(sdvp->sdv_volume_num, 0)),
			       ~(NPART - 1));
			error = sd_eject(sdvp);
		}
		break;

	case SDIOCGETCAP:
		error = sdreadcapacity(sdvp, sdvp->sdv_crp, SDF_INTERRUPT);
		*(struct capacity_reply *)data = *sdvp->sdv_crp;
		break;
		
	default:
		return(EINVAL);
		/* return (scsi_ioctl(sddp->sdd_sdp, cmd, data, flag)); */
	}
	return(error);
} /* sdioctl() */

static void
sdgetlabel(sd_volume_t sdvp, u_int polling)
{
	struct disk_req dr;
	struct scsi_req sr;
	struct cdb_10 *c10p = &sr.sr_cdb.cdb_c10;
	int tries, error, nblk;

	if(sdreadcapacity(sdvp, sdvp->sdv_crp, polling)) {
		/* It MUST to be set to something */
		sdvp->sdv_crp->cr_blklen = 512;
		sdvp->sdv_flags &= ~SVF_LABELVALID;
		return;
	}
	if (sdvp->sdv_crp->cr_blklen == 0) {
		printf("ERROR: Invalid device block length\n");
		sdvp->sdv_crp->cr_blklen = 512;
	}

	nblk = howmany(sizeof(*sdvp->sdv_dlp),sdvp->sdv_crp->cr_blklen);
	sdvp->sdv_flags &= ~SVF_FORMATTED;
	/*
	 * For this operation, we inhibit the reporting
	 * of media errors and limit the number of retries -
	 * this error always happens when attaching an
	 * unformatted disk.
	 */
	sdvp->sdv_sddp->sdd_flags |= SDF_GETLABEL;
	sdvp->sdv_sddp->sdd_med_retry_max = SD_MED_RETRIES_LABEL;
	for (tries = 0; tries < NLABELS; tries++) {
		bzero(sdvp->sdv_dlp, sizeof(*sdvp->sdv_dlp));
		bzero(&sr, sizeof(sr));
		c10p->c10_opcode = C10OP_READEXTENDED;
		c10p->c10_lba = nblk * tries;
		c10p->c10_len = nblk;
		sr.sr_addr = (caddr_t)sdvp->sdv_dlp;
		sr.sr_dma_max = sizeof(*sdvp->sdv_dlp);
		sr.sr_dma_dir = SR_DMA_RD;
		sr.sr_ioto = SD_IOTO_NORM;
		error = sdcmd_srq(sdvp, &sr, polling);
		/*
		 * Subsequent to autoconfig, sdcmd_srq does not report error
		 * on media flaw...
		 */
		error |= sr.sr_io_status;
		if (!error) {
			/*
			 * At least we know we can read the disk.
			 */
			sdvp->sdv_flags |= SVF_FORMATTED;
			if(sdchecklabel(sdvp->sdv_dlp, c10p->c10_lba)) {
				sdvp->sdv_flags |= SVF_LABELVALID;
				break;
			}
		}
	}
	/*
	 * Go back to normal retry mode.
	 */
	sdvp->sdv_sddp->sdd_flags &= ~SDF_GETLABEL;
	sdvp->sdv_sddp->sdd_med_retry_max = SD_MED_RETRIES;
} /* sdgetlabel() */

static int
sdwritelabel(sd_volume_t sdvp)
{
	struct disk_req dr;
	struct scsi_req sr;
	struct cdb_10 *c10p = &sr.sr_cdb.cdb_c10;
	int nblk, tries, failed;

	if(sdreadcapacity(sdvp, sdvp->sdv_crp, SDF_INTERRUPT))
		return(EIO);		
	/*
	 * mark label valid now, that way disk is usable even
	 * if label can't be written
	 */
	sdvp->sdv_flags |= SVF_LABELVALID;
	sdsetbratio(sdvp);

	failed = EIO;
	nblk = howmany(sizeof(*sdvp->sdv_dlp), sdvp->sdv_crp->cr_blklen);
	for (tries = 0; tries < NLABELS; tries++) {

		bzero(&sr, sizeof(sr));
		c10p->c10_opcode = C10OP_WRITEEXTENDED;
		c10p->c10_lba = nblk * tries;
		c10p->c10_len = nblk;
		sr.sr_addr = (caddr_t)sdvp->sdv_dlp;
		sr.sr_dma_max = sizeof(*sdvp->sdv_dlp);
		sr.sr_dma_dir = SR_DMA_WR;
		sr.sr_ioto = SD_IOTO_NORM;
		sdvp->sdv_dlp->dl_label_blkno = c10p->c10_lba;
		if (sdcmd_srq(sdvp, &sr, SDF_INTERRUPT) == 0)
			failed = 0;
	}
	return(failed);
} /* sdwritelabel() */

static int
sdreadcapacity(sd_volume_t sdvp, struct capacity_reply *crp, int polling)
{
	struct scsi_req sr;
	struct cdb_10 *c10p;
	
	bzero(&sr, sizeof(sr));
	c10p = &sr.sr_cdb.cdb_c10;
	c10p->c10_opcode = C10OP_READCAPACITY;
	sr.sr_addr = (caddr_t)crp;
	sr.sr_dma_max = sizeof(struct capacity_reply);
	sr.sr_dma_dir = SR_DMA_RD;
	sr.sr_ioto = SD_IOTO_NORM;
	if (sdcmd_srq(sdvp, &sr, polling)) {
		printf("ERROR: Can't read device capacity\n");
		return(1);
	}
	return(0);
}

/*
 * FIXME:
 * this should be a generic function for all disk devices
 */
int
sdchecklabel(struct disk_label *dlp, int blkno)
{
	u_short sum, cksum, checksum_16(), *dl_cksum, size;

	if (dlp->dl_version == DL_V1 || dlp->dl_version == DL_V2) {
		size = sizeof (struct disk_label);
		dl_cksum = &dlp->dl_checksum;
	} else
	if (dlp->dl_version == DL_V3) {
		size = sizeof (struct disk_label) - sizeof (dlp->dl_un);
		dl_cksum = &dlp->dl_v3_checksum;
	} else {
		printf("Bad disk label magic number\n");
		return (0);
	}
	if (dlp->dl_label_blkno != blkno) {
		printf("Label in wrong location\n");
		return (0);	/* label not where it's supposed to be */
	}
	dlp->dl_label_blkno = 0;
	cksum = *dl_cksum;
	*dl_cksum = 0;
	if ((sum = checksum_16 (dlp, size >> 1)) != cksum) {
		printf("Label checksum error: was 0x%x sb 0x%x\n",
		    sum, cksum);
		return (0);
	}
	if (dlp->dl_secsize != DEV_BSIZE) {
		printf("Blk size in label (%d) != file system blk size (%d)\n",
		    dlp->dl_secsize, DEV_BSIZE);
		return(0);
	}
	*dl_cksum = cksum;
	return (1);
}


static int
sdcmd_srq(sd_volume_t sdvp,
	struct scsi_req *srp,
	u_int polling)
{
	/* Parameters are passed from the user (and to sdstart()) in a 
	 * scsi_req struct. We use sdvp->sdv_cbuf to serialize 
	 * access to sdvp->sdv_srp.
	 *
	 * Polling should ONLY be used during auto-configuration
	 * when we can't sleep because the system isn't up far enough
	 */
	 
	sd_drive_t sddp = sdvp->sdv_sddp;
	struct buf *bp = &sdvp->sdv_cbuf;
	int rtn = 0;
	struct timeval start, stop;
	int s;
	scsi_device_t sdp;
	
	ASSERT(sddp != NULL);
	ASSERT((polling == SDF_POLLING) || (polling == SDF_INTERRUPT));
	sdp = sddp->sdd_sdp;
	XCDBG(("sdcmd_srq: op = %n target = %d  lun = %d\n",
		srp->sr_cdb.cdb_opcode,scsi_cmd_values,
		sdp->sd_target,sdp->sd_lun));
	/*
	 * First wait for access to our volume's command buf and sdv_srp.
	 */
	s = spln(ipltospl(sdp->sd_scp->sc_ipl));
	while ((bp->b_flags & B_BUSY) && (polling == SDF_INTERRUPT)) {
		bp->b_flags |= B_WANTED;
		sleep(bp, PRIBIO);
	}
	bp->b_flags = B_BUSY | B_READ;		/* be sure to free this... */
	splx(s);
	
	/* 
	 * we now own sdvp->sdv_srp. 
	 */
	sdvp->sdv_srp = srp;
	microtime(&start);
	
	/* only allow disconnects if interrupt driven */
	if(polling == SDF_POLLING) {
		sddp->sdd_flags &= ~SDF_DISCONNECT;
		sddp->sdd_flags |= SDF_POLLING;
	}
	else {
		sddp->sdd_flags |= SDF_DISCONNECT;
		sddp->sdd_flags &= ~SDF_POLLING;
	}
	
	/* nobody looks at partition number if we're using the *srp. */
	bp->b_dev = SD_DEVT(sdvp->sdv_volume_num, 0);
	
	if(sdstrategy(bp) == 0) {
	
		/* only biowait() if we could actually start command... */
					
		if (polling == SDF_INTERRUPT)
			biowait(bp);
		else {
			int tries = 0;
	
			ASSERT(curipl() == 0);
			while ((bp->b_flags & B_DONE) == 0) {
				/* Wait up to 4s for command to complete */
				if (++tries > 4000) {
					scsi_timeout(sdp->sd_scp);
					tries = 0;
				} else
					DELAY(1000);
			}
		}
	}

	/* 
	 * copy autosense data to *srp if valid. If not polling, return good
	 * u_error status unless gross error ocurred (which was detected in
	 * sdstrategy)... 
	 */
	 
	if(srp->sr_io_status == SR_IOST_CHKSV)
		srp->sr_esense = *sdvp->sdv_erp;
	
	if((rtn == 0) && (srp->sr_io_status) && polling)
		rtn = EIO;
	microtime(&stop);
	timevalsub(&stop, &start);
	srp->sr_exec_time = stop;
	XCDBG(("sdcmd_srq: io_status = %n\n", srp->sr_io_status,
		sriost_values));
	/*
	 * Relinquish ownership of command buf and sdv_srp.
	 */
	sdvp->sdv_srp = NULL;
	bp->b_flags &= ~B_BUSY;
	if ((bp->b_flags & B_WANTED) && (polling == SDF_INTERRUPT))
		wakeup(bp);
	return(rtn);
	
} /* sdcmd_srq() */

/*
 * create a new volume struct for the specified volume #.
 */
sd_volume_t sd_new_sv(int vol_num)
{
	sd_volume_t sdvp;
	
	XDBG(("sd_new_sv: vol_num %d\n", vol_num));
	sdvp = kalloc(sizeof(struct scsi_disk_volume));
	if(sdvp == NULL)
		panic("sd_new_sv: Couldn't kalloc sd_volume");
	bzero(sdvp, sizeof(struct scsi_disk_volume));
	sdvp->sdv_volume_num = vol_num;
	sdvp->sdv_flags = SVF_VALID;			/* note NOT MOUNTED */
	sdvp->sdv_cbuf.b_flags = 0;
	sdvp->sdv_srp = NULL;
	sdvp->sdv_blk_open = 0;
	sdvp->sdv_raw_open = 0;

	sdvp->sdv_dlp = kalloc(sizeof(struct disk_label));
	if(sdvp->sdv_dlp == NULL)
		panic("sd_new_sv: Couldn't kalloc disk_label");
	ASSERT(DMA_ENDALIGNED(sdvp->sdv_dlp));
	/*
	 * Get well-aligned pointers to request sense and Read Capacity
	 * buffers.
	 */
	sdvp->sdv_erp = DMA_ALIGN(struct esense_reply *, &sdvp->sdv_erbuf);
	sdvp->sdv_crp = DMA_ALIGN(struct capacity_reply *, &sdvp->sdv_crbuf);
	/*
	 * Initialize disksort info.
	 */
	disksort_init(&sdvp->sdv_queue);

	return(sdvp);
} /* sd_new_sv() */

/*
 * free a volume struct and everything in it.
 */
static void sd_free_sv(sd_volume_t sdvp)
{
	XDBG(("sd_free_sv: freeing volume %d\n", sdvp->sdv_volume_num));
	ASSERT(sdvp->sdv_volume_num <= NUM_SDVP);
	disksort_free(&sdvp->sdv_queue);
	sd_volume_p[sdvp->sdv_volume_num] = NULL;
	kfree(sdvp->sdv_dlp, sizeof(struct disk_label));
	kfree(sdvp, sizeof(struct scsi_disk_volume));
} /* sd_free_sv() */

/*
 * Assign specified drive to specified volume number. During normal operation,
 * this should only be called by the sd_vol_check thread.
 */
static void sd_assign_sv(sd_volume_t sdvp, int drive_num)
{
	sd_drive_t sddp = &sd_sdd[drive_num];
	
	ASSERT(drive_num <= NSD);
	sdvp->sdv_sddp = sddp;
	sddp->sdd_sdvp = sdvp;
	/*
	 * Nullify possible "requested disk not available" condition.
	 */
	sdvp->sdv_flags &= ~SVF_NOTAVAIL;
}

static void sd_vol_check() 
{
	/*
	 * one thread per system. Responsible for:
	 * 
	 * -- Processing Eject Requests, These are made by sd_vstart when 
	 *    an I/O request is made for an umounted volume. An eject request
	 *    consists of an sd_eject_req placed on sd_eject_q.
	 * -- Processing Abort Requests. These result from callbacks from the
	 *    when a panel request is acked. An abort request consists of an
	 *    sd_abort_req's placed on sd_abort_q.
	 * -- Polling drives with no mounted volumes to detect disk insertions.
	 *
	 * This thread does I/O via sd_volume_p[SVC_VOLUME] and 
	 * sd_sdd[SVC_DEVICE]. This is necessary to avoid disturbing the 
	 * state of other volume and drive structs while we do our I/O.
	 */
	 
	/* we use these 3 structs for all of our I/O */
    	sd_volume_t vc_sdvp;		
	sd_drive_t vc_sddp   = &sd_sdd[SVC_DEVICE];
	scsi_device_t vc_sdp = &sd_sd[SVC_DEVICE];
	sd_volume_t work_sdvp;		/* scratch */
	sd_drive_t work_sddp;		/* scratch */
	sd_volume_t intended_sdvp;
	sd_drive_t current_sddp;	/* This and current_drive represent the
					 * drive we're currently looking at */
	int i;
	int current_drive;
  	queue_head_t *qhp;
 	struct sd_abort_req *sarp, *next_sarp;
	struct buf *bp;
	int vol_num;
	struct timeval tv;
	int s;
	struct bus_device *current_bdp;
	struct tsval curtime, oldtime;
	int resid;
	struct sd_eject_req *serp, *next_serp;
	
    	sd_volume_p[SVC_VOLUME] = vc_sdvp = sd_new_sv(SVC_VOLUME); 
	sd_assign_sv(vc_sdvp, SVC_DEVICE);
	vc_sdvp->sdv_flags |= SVF_MOUNTED;
	
	/*
	 * Initialize a queue of free sd_eject_req's.
	 */
	serp = kalloc(NUM_FREE_EJECT_REQ * sizeof(struct sd_eject_req));
	for(i=0; i<NUM_FREE_EJECT_REQ; i++) {
		queue_enter(&sd_eject_freeq, serp, struct sd_eject_req *,
			    link);
		serp++;
	}
	
	while(1) {
	    /*
	     * Loop forever. First handle any pending abort requests.
	     */
	    qhp = &sd_abort_q;
	    sarp = (struct sd_abort_req *)queue_first(qhp);
	    while(!queue_end(qhp, (queue_t)sarp)) {
		    work_sdvp = sarp->sdvp;
		    XDBG(("sd_vol_check: ABORT REQUEST vol %d\n", 
			    work_sdvp->sdv_volume_num));
		    next_sarp = (struct sd_abort_req *)(sarp->link.next);
		    queue_remove(qhp, sarp, struct sd_abort_req *, link);
		    
		    /*
		     * Skip this whole thing if the volume has been inserted 
		     * (this is a race condition in which the "volume present"
		     * situation should prevail).
		     */
		    if(!(work_sdvp->sdv_flags & SVF_WAITING))
		    	goto abort_end;
			
		    current_sddp = work_sdvp->sdv_sddp;
		    
		    /*
		     * Get the bp which generated the alert panel which 
		     * aborted - it's at the head of this volume's sdv_queue.
		     * Mark it unavailable and abort via sddone().
		     *
		     * This seems kind of radical, but remember that the 
		     * sd_drive for this volume is sitting in state
		     * SDDSTATE_VOLWAIT and no I/O is happening.
		     */
		    bp = disksort_first(&work_sdvp->sdv_queue);
		    work_sdvp->sdv_flags |= SVF_NOTAVAIL;
		    work_sdvp->sdv_flags &= ~(SVF_ALERT | SVF_WAITING);
		    bp->b_error = ENXIO;
		    /*
		     * Careful; we haven't called sdstart yet, so sdp->sd_resid
		     * isn't valid...
		     */
		    if(bp == &work_sdvp->sdv_cbuf) 
		    	resid = work_sdvp->sdv_srp->sr_dma_max;
		    else
		    	resid = bp->b_bcount;
		    ASSERT(current_sddp->sdd_state == SDDSTATE_VOLWAIT);
		    ASSERT(current_sddp->sdd_intended_sdvp == work_sdvp);
		    s = spln(ipltospl(current_sddp->sdd_sdp->sd_scp->sc_ipl));
		    current_sddp->sdd_state = SDDSTATE_STARTINGCMD;
		    sddone(current_sddp, 
			resid, 
			B_ERROR,
			SR_IOST_VOLNA);
		    splx(s);
abort_end:
		    kfree(sarp, sizeof(*sarp));
		    sarp = next_sarp;
	    } /* going thru sd_abort_q */

	    /*
	     * Handle pending eject requests.
	     */
	    qhp = &sd_eject_q;
	    serp = (struct sd_eject_req *)queue_first(qhp);
	    while(!queue_end(qhp, (queue_t)serp)) {
		    work_sdvp = serp->new_sdvp->sdv_sddp->sdd_sdvp;
		    XDBG(("sd_vol_check: EJECT REQUEST vol %d\n", 
			    work_sdvp->sdv_volume_num));
		    next_serp = (struct sd_eject_req *)(serp->link.next);
		    /*
		     * If the drive to which the intended volume is to be 
		     * mapped is currently waiting for a volume, skip this
		     * request. We only want to have one alert panel up at
		     * at time.
		     */
		    if(serp->new_sdvp->sdv_sddp->sdd_intended_sdvp != NULL)
		    	goto eject_q_end;
			
		    queue_remove(qhp, serp, struct sd_eject_req *, link);
		    
		    /*
		     * work_sdvp is the volume to eject. If NULL, just put up 
		     * an alert panel. Eject the volume if it's in a drive. 
		     */
		    if((work_sdvp != NULL) && 
		       (work_sdvp->sdv_flags & SVF_VALID) &&
		       (work_sdvp->sdv_sddp != NULL)) {
			    /*
			     * Sync the file system on work_sdvp if the device
			     * is open.
			     */
			    if(work_sdvp->sdv_raw_open || 
			       work_sdvp->sdv_blk_open) {
				    update(makedev(sd_blk_major,
				       	SD_DEVT(work_sdvp->sdv_volume_num, 0)),
			       		~(NPART - 1));
			    }
			
			    /*
			     * eject the disk.
			     */
			    sd_eject(work_sdvp);
			    if(!(work_sdvp->sdv_raw_open) &&
			       !(work_sdvp->sdv_blk_open) &&
			       !(work_sdvp->sdv_flags & SVF_NEWVOLUME)) {
				    /*
				     * Volume no longer in use. This happens 
				     * when all references to the volume closed
				     * it, but did not eject it. Now that we've 
				     * ejected it, we can free the volume 
				     * struct.
				     */
				    XDBG(("sd_vol_check: freeing sdvp\n"));
				    sd_volume_done(work_sdvp);
			    }
		    }
#ifdef	DEBUG
		    else 
			    printf("sd eject request: no disk present\n");
#endif	DEBUG
		    /* 
		     * Now deal with the new (desired) volume...
		     */
		    work_sdvp = serp->new_sdvp;
		    work_sddp = work_sdvp->sdv_sddp;
		    work_sddp->sdd_state = SDDSTATE_VOLWAIT;
		    work_sddp->sdd_intended_sdvp = work_sdvp;
		    sd_alert_panel(work_sdvp, FALSE);
		    queue_enter(&sd_eject_freeq, serp, struct sd_eject_req *,
		    	link);
eject_q_end:
		    serp = next_serp;
	    } /* going thru sd_eject_q */

	    /*
	     * Now look for newly inserted disks. Disable any open()s of any
	     * SCSI disks while we're doing this to ensure clean mapping.
	     */
	    lock_write(&sd_open_lock);
	    
	    for(current_drive=0; 
	        current_drive<sd_num_drives; 
		current_drive++) {
		
	        current_sddp = &sd_sdd[current_drive];
		ASSERT(current_sddp->sdd_sdp->sd_bdp->bd_alive != NULL);
	    	if(current_sddp->sdd_sdvp != NULL)
		    continue;				/* has a volume */
		/*
		 * Workaround for PLI bug - accessing too soon after an eject
		 * can result in bogus successful Test Unit Ready and then
		 * a Selection Timeout on a subsequent Read command!
		 */
		event_set_ts(&curtime);
		oldtime = current_sddp->sdd_last_eject;
		ts_add(&oldtime, SD_DELAY_EJECT * 1000000);
		if(ts_greater(&oldtime, &curtime)) 
		    continue;

		/*
		 * Bind our sdp to this drive's target and lun and
		 * do a test unit ready. Also, copy this drive's bus_device
		 * info to our bogus bus_device.
		 */
		vc_sdp->sd_target = current_sddp->sdd_sdp->sd_target;
		vc_sdp->sd_lun    = current_sddp->sdd_sdp->sd_lun;
		/*
		 * Make our drive just like the one we're looking at..
		 */
		*vc_sddp->sdd_irp = *current_sddp->sdd_irp;
		current_bdp       = current_sddp->sdd_sdp->sd_bdp;
		sd_vc_bd.bd_unit  = current_bdp->bd_unit;
		sd_vc_bd.bd_ctrl  = current_bdp->bd_ctrl;
		if(sdtestrdy(vc_sdvp, SDF_INTERRUPT) == 0) {
		    /* 
		     * If ejecting, we've been waiting for this. Now we start
		     * looking for unit ready.
		     */
		    if(current_sddp->sdd_flags & SDF_EJECTING) {
		    	current_sddp->sdd_flags &= ~SDF_EJECTING;
			if(current_sddp->sdd_panel_tag >= 0) {
			    vol_panel_remove(current_sddp->sdd_panel_tag);
			    current_sddp->sdd_panel_tag = -1;
			}
			continue;
		    }
		    else {
			/* 
			 * No disk and still not ready. 
			 */
			continue;
		    }
		}
		if(current_sddp->sdd_flags & SDF_EJECTING) {
		   if(current_sddp->sdd_panel_tag < 0) {
			/*
			 * If we're waiting for eject, and the drive is ready, 
			 * it needs a manual eject. Tell the user the first 
			 * time we see this situation.
		         *
		         * Warning: this doesn't work well when we have ejected 
		         * a disk and put up an alert asking for a new one. The
		         * user will see both alerts ("Please eject" and 
			 * "Please Insert") at the same time. The only fix
			 * is do all of the ejects in a separate thread. The
			 * eject thread would only mark a disk ejected when
			 * it saw the drive go not ready; sd_eject() wouldn't
			 * return until that happened. We can't do all of that 
			 * here since THIS thread  makes several calls to 
			 * sd_eject().
			 *
			 * Since WS isn't supporting multiple volumes, let's
			 * let this slide. (10-Sep-90 dmitch)
			 */
#ifdef	DEBUG
			printf("sd_vol_check: Please Eject SCSI disk in "
				"drive %d\n", current_sddp - &sd_sdd[0]);
#endif	DEBUG
			vol_panel_request((vpt_func)NULL,  /* no callback */
			    PR_RT_EJECT_REQ,
			    PR_RT_CANCEL,		/* leave up 'til we 
							* take it down */
			    0,				/* p1, not used */
			    PR_DRIVE_SCSI,
			    current_sddp - &sd_sdd[0],
			    0,				/* p4, not used */
			    "",
			    "",
			    (void *)NULL,		/* no callback */
			    &current_sddp->sdd_panel_tag);
		    }
		    /*
		     * Skip the attach in any case.
		     */
		    continue;
		}
		
		/*
		 * New disk inserted. Try to attach in order to get the 
		 * label.
		 */
		XDBG(("sd_vol_check: NEW DISK INSERTED drive %d\n", 
			current_drive));
		sdattach2(vc_sdvp, SDF_INTERRUPT);
		intended_sdvp = current_sddp->sdd_intended_sdvp;
		if(intended_sdvp != NULL) {
		    if(intended_sdvp->sdv_flags & SVF_NEWVOLUME) {
			/*
			 * This drive is expecting a new volume. Make sure 
			 * we don't already know about this volume.
			 */
			XDBG(("sd_vol_check: NEW VOLUME REQUEST\n"));
			for(vol_num=0; vol_num<NUM_SDV; vol_num++) {
			    work_sdvp = sd_volume_p[vol_num];
			    if((work_sdvp == NULL) ||
			       (vol_num == intended_sdvp->sdv_volume_num))
				continue;
			    if(sd_compare_vol(vc_sdvp, work_sdvp) == 0) {
				XDBG(("sd_vol_check: KNOWN DISK;"
				    "EXPECTED NEW\n"));
				sd_eject(vc_sdvp);
				/*
				 * reassign because of eject...
				 */
				sd_assign_sv(vc_sdvp, SVC_DEVICE);
				vc_sdvp->sdv_flags |= SVF_MOUNTED;

				/*
				 * Remove existing alert panel and put 
				 * up a more insistent one.
				 */
				if(intended_sdvp->sdv_flags & SVF_ALERT) {
				    vol_panel_remove(intended_sdvp->
							sdv_vol_tag);
				    sd_alert_panel(intended_sdvp, TRUE);
				}
				goto next_drive;
			    }
			} /* scanning sdvp's */
			/*
			 * Well, it must be OK. We are looking for a new 
			 * volume and this disk doesn't look like anything
			 * we know about. Copy over everything we learned
			 * about this volume to the intended sdvp, mark it
			 * mounted, and remove existing alert panel.
			 */
			XDBG(("sd_vol_check: NEW VOLUME ASSIGN vol %d\n",
			    intended_sdvp->sdv_volume_num));
			sd_copy_vol(vc_sdvp, intended_sdvp);
			sd_vc_assign(intended_sdvp, current_sddp);
			goto next_drive;
		    } /* new volume */
		    else {
		        /*
			 * We are expecting a specific disk. Make sure this is 
			 * it.
			 */
			XDBG(("sd_vol_check: EXPECTING KNOWN VOLUME\n"));
			if(sd_compare_vol(vc_sdvp, intended_sdvp) == 0) {
			    XDBG(("vol_check: MATCH - expected vol %d\n",
				intended_sdvp->sdv_volume_num));
			    sd_vc_assign(intended_sdvp, current_sddp);
			}
			else {
			    XDBG(("sd_vol_check: WRONG VOLUME\n"));
			    sd_eject(vc_sdvp);
			    /*
			     * reassign because of eject...
			     */
			    sd_assign_sv(vc_sdvp, SVC_DEVICE);
			    vc_sdvp->sdv_flags |= SVF_MOUNTED;
			    /*
			     * Remove existing alert panel and put 
			     * up a more insistent one.
			     */
			    if(intended_sdvp->sdv_flags & SVF_ALERT) {
				vol_panel_remove(intended_sdvp->sdv_vol_tag);
				sd_alert_panel(intended_sdvp, TRUE);
			    }
			}
			goto next_drive;			
		    } /* expecting specific disk */
		} /* expecting disk */
		else {
		    /*
		     * User inserted a disk with no prompt. Do we recognize
		     * it?
		     */
		    XDBG(("sd_vol_check: UNPROMPTED DISK INSERTION\n"));
		    for(vol_num=0; vol_num<NUM_SDV; vol_num++) {
			work_sdvp = sd_volume_p[vol_num];
			/*
			 * First reject the cases in which no valid volume 
			 * exists.
			 */
			if(work_sdvp == NULL)
			    continue;
			if(!(work_sdvp->sdv_flags & SVF_VALID))
			    continue;
			if((sd_compare_vol(vc_sdvp, work_sdvp) == 0) &&
			   (!(work_sdvp->sdv_flags & SVF_MOUNTED))) {
			    XDBG(("sd_vol_check: KNOWN VOLUME vol "
				    "%d drive %d\n", vol_num, current_drive));
			    /*
			     * We have already seen this volume; re-assign it
			     * to this drive. 
			     *
			     * Note this is the only place where we allow 
			     * a volume to switch drives!
			     */
#ifdef	DEBUG
			    if(work_sdvp->sdv_sddp != current_sddp) {
			        XDBG(("sd_vol_check: VOL CHANGED DRIVES - vol "
				   "%d  old drive %d new drive %d\n",
				   work_sdvp->sdv_volume_num,
				   work_sdvp->sdv_sddp - &sd_sdd[0],
				   current_drive));
			    }
#endif	DEBUG
			    sd_vc_assign(work_sdvp, current_sddp);
			    goto next_drive;
			} /* match  */
		    } /* looking for a matching volume */
		    
		    /*
		     * New disk. We're not expecting it, and we've never seen
		     * it.
		     *
		     * Find first unopened volume; allocate a new volume 
		     * struct; copy over format info we learned during attach.
		     *
		     * Note the the first sd_num_drives volume structs are 
		     * reserved as "primary volumes"; they never go away and 
		     * are bound to their respective drives. We can use one of
		     * these if it's invalid and it's for this volume/drive 
		     * combo.
		     */
		    work_sdvp = sd_volume_p[current_drive];
		    ASSERT(work_sdvp != NULL);
		    if(!(work_sdvp->sdv_flags & SVF_VALID)) {
		        XDBG(("sd_vol_check: NEW PRIMARY VOLUME vol %d\n",
				work_sdvp->sdv_volume_num));
		    	work_sdvp->sdv_flags |= SVF_VALID;
			sd_copy_vol(vc_sdvp, work_sdvp);
			sd_vc_assign(work_sdvp, current_sddp);
			/*
			 * notify automounter.
			 */
			sd_volume_notify(work_sdvp);
			goto next_drive;
		    }
		    /*
		     * It can't be a primary volume; look for another.
		     */
		    for(vol_num=sd_num_drives; vol_num<NUM_SDV; vol_num++) {
			if(sd_volume_p[vol_num] == NULL) {
			    XDBG(("sd_vol_check: NEW VOLUME = %d\n", vol_num));
			    work_sdvp = sd_volume_p[vol_num] = 
			    	sd_new_sv(vol_num);
			    sd_copy_vol(vc_sdvp, work_sdvp);
			    sd_vc_assign(work_sdvp, current_sddp);
			    /*
			     * notify automounter/boot program.
			     */
			    sd_volume_notify(work_sdvp);
			    goto next_drive;
			} /* found a null volume */
		    } /* looking for null volumes */
		    printf("sd_vol_check: NO FREE VOLUMES\n");
		    sd_eject(vc_sdvp);
		    sd_assign_sv(vc_sdvp, SVC_DEVICE);
		    vc_sdvp->sdv_flags |= SVF_MOUNTED;
		} /* user inserted disk without prompt */
next_drive:
		continue;
	    } /* for each drive */

	    lock_done(&sd_open_lock);
	    
	    /*
	     * sleep. Normally we sleep for one second, but it could be 
	     * less if we've put up a "please insert disk" panel.
	     */
	    if(sd_vc_delay >= 1000000) {
		    tv.tv_sec  = sd_vc_delay / 1000000;
		    tv.tv_usec = sd_vc_delay % 1000000;
	    }
	    else {
		    tv.tv_sec  = 0;
		    tv.tv_usec = sd_vc_delay;
	    }
	    sd_vc_event = 0;
	    us_timeout(sd_vc_timeout, 
		    NULL, 
		    &tv, 
		    CALLOUT_PRI_SOFTINT0);
	    s = splsched();
	    while(!sd_vc_event) {
	    	    assert_wait(&sd_vc_event, FALSE);
	    	    splx(s);
	    	    thread_block();
		    s = splsched();
	    }
	    splx(s);
    	} /* main loop */
	/* NOT REACHED */
}

static int sd_vc_timeout() {
	sd_vc_event = 1;
	thread_wakeup(&sd_vc_event);
}

static int sd_compare_vol(sd_volume_t sdvp1, sd_volume_t sdvp2)
{
	/*
	 * Determine if these sdvps refer to the same media (to the best of
	 * our ability). Returns 0 if same, else 1.
	 */
	XDBG(("sd_compare_vol\n"));
	if(sdvp1->sdv_crp->cr_lastlba != 
	   sdvp2->sdv_crp->cr_lastlba)		/* num blocks */
		return(1);
	if(sdvp1->sdv_crp->cr_blklen != 
	   sdvp2->sdv_crp->cr_blklen)		/* block size */
		return(1);
	if((sdvp1->sdv_flags & SVF_LABELVALID) !=
	   (sdvp2->sdv_flags & SVF_LABELVALID))
	   	return(1);
	if(sdvp1->sdv_flags & SVF_LABELVALID) {
		if(sdvp1->sdv_dlp->dl_tag != sdvp2->sdv_dlp->dl_tag)
			return(1);
		if(strncmp(sdvp1->sdv_dlp->dl_label,
			   sdvp2->sdv_dlp->dl_label,
			   MAXLBLLEN))
			return(1);
	}
	XDBG(("sd_compare_vol: MATCH\n"));
	return(0);
} /* sd_compare_vol() */

#define FLAG_COPY_MASK	(SVF_LABELVALID|SVF_FORMATTED|SVF_WP)

static void sd_copy_vol(sd_volume_t source_vol, sd_volume_t dest_vol)
{
	/*
	 * Copy capacity/label info from one volume to another. Called when
	 * we've done an attach using sd_vol_check's sdvp and we want
	 * to transfer everything we've learned about a volume to 
	 * a usable sdvp.
	 */
	*dest_vol->sdv_dlp = *source_vol->sdv_dlp;
	*dest_vol->sdv_crp = *source_vol->sdv_crp;
	dest_vol->sdv_flags &= ~FLAG_COPY_MASK;
	dest_vol->sdv_flags |= (source_vol->sdv_flags & FLAG_COPY_MASK);
	dest_vol->sdv_flags |= SVF_VALID;
	dest_vol->sdv_bratio = source_vol->sdv_bratio;
}

static void sd_vc_assign(sd_volume_t sdvp, sd_drive_t sddp)
{
	int s;
	
	/*
	 * Bind newly inserted disk to its drive. If any I/O pending for this 
	 * volume, transfer it from its drive's sd_holdq to its sd_vol_q and
	 * start up the I/O.
	 */
#ifdef	DEBUG
	printf("sd_vc_assign: Assigning vol %d to drive %d\n",
	 	sdvp->sdv_volume_num, sddp - &sd_sdd[0]);
#endif	DEBUG
	sddp->sdd_sdvp = sdvp;
	sdvp->sdv_sddp = sddp;
	sddp->sdd_intended_sdvp = NULL;
	sdvp->sdv_flags |= SVF_MOUNTED;
	sdvp->sdv_flags &= ~(SVF_NOTAVAIL | SVF_NEWVOLUME);
	if(sdvp->sdv_flags & SVF_ALERT) {
	    	sdvp->sdv_flags &= ~SVF_ALERT;
		vol_panel_remove(sdvp->sdv_vol_tag);
	}
	s = spln(ipltospl(sddp->sdd_sdp->sd_scp->sc_ipl));
	if (disksort_first(&sdvp->sdv_queue) != NULL) {
		/*
		 * This means that we've been waiting for this volume...
		 */
		ASSERT((sdvp->sdv_flags & (SVF_WAITING | SVF_ACTIVE)) == 
			SVF_WAITING);
		sdvp->sdv_flags &= ~SVF_WAITING;
		/*
		 * Could be at SDDSTATE_STARTINGCMD if we did an abort
		 * sequence since this I/O was enqueued...
		 */
		ASSERT((sddp->sdd_state == SDDSTATE_VOLWAIT) ||
		       (sddp->sdd_state == SDDSTATE_STARTINGCMD));
		sddp->sdd_state = SDDSTATE_STARTINGCMD;
		if(sddp->sdd_sdp->sd_active == 0) 
			scsi_dstart(sddp->sdd_sdp);
	}
	splx(s);
}

static void sd_alert_panel(sd_volume_t sdvp, boolean_t wrong_disk) {

	kern_return_t krtn;
	
	if((sdvp->sdv_flags & (SVF_LABELVALID | SVF_VALID)) == 
		 	      (SVF_LABELVALID | SVF_VALID)) {
		krtn = vol_panel_disk_label(sd_vol_panel_abort,
			sdvp->sdv_dlp->dl_label,
			PR_DRIVE_SCSI,
			sdvp->sdv_sddp - sd_sdd,	/* intended drive */
			sdvp,
			wrong_disk,
			&sdvp->sdv_vol_tag);
	}
	else {
		krtn = vol_panel_disk_num(sd_vol_panel_abort,
			sdvp->sdv_volume_num,
			PR_DRIVE_SCSI,
			sdvp->sdv_sddp - sd_sdd,
			sdvp,
			wrong_disk,
			&sdvp->sdv_vol_tag);
	}
	if(krtn) {
		printf("sd_alert_panel: vol_panel_disk() returned "
			"%d\n", krtn);
	}
	sdvp->sdv_flags |= SVF_ALERT;
}

static void sd_vol_panel_abort(void *param, int tag, int response_value)
{
	/*
	 * called from vol driver when it receives a vol_panel_cancel message
	 * from the WSM.
	 */
	struct sd_abort_req *sarp;
	
	sarp = kalloc(sizeof(*sarp));
	sarp->sdvp = param;
	sarp->tag = tag;
	sarp->response = response_value;
	XDBG(("sd_vol_panel_abort: sdvp 0x%x volume_num %d tag %d\n",
		param, sarp->sdvp->sdv_volume_num, tag));
	queue_enter(&sd_abort_q, sarp, struct sd_abort_req *, link);
	thread_wakeup(&sd_vc_event);	/* make sd_vol_check sees this NOW */
}

static void sd_volume_notify(sd_volume_t sdvp)
{
	char dev_str[OID_DEVSTR_LEN];
	int vol_state;
	int flags=0;
	
	if(sdvp->sdv_flags & SVF_FORMATTED) {
		vol_state = sdvp->sdv_flags & SVF_LABELVALID ?
			IND_VS_LABEL : IND_VS_FORMATTED;
	}
	else
		vol_state = IND_VS_UNFORMATTED;
	sprintf(dev_str, "sd%d", sdvp->sdv_volume_num);
	flags = sdvp->sdv_sddp->sdd_irp->ir_removable ? 
			IND_FLAGS_REMOVABLE : IND_FLAGS_FIXED;
	if(sdvp->sdv_flags & SVF_WP)
		flags |= IND_FLAGS_WP;
	vol_notify_dev(makedev(sd_blk_major, SD_DEVT(sdvp->sdv_volume_num, 0)), 
		makedev(sd_raw_major, SD_DEVT(sdvp->sdv_volume_num, 0)),
		"",
		vol_state,
		dev_str,
		flags);
}

/*
 * This function keeps getting called as a timeout until the kernel is
 * running; at that point, we start up the sd_vol_check thread. We skip
 * starting up this thread if there are no removable media drives in the
 * system.
 */
void sd_thread_timer() 
{
	int i;
	
	if(kernel_task && !sd_thread_init) {
		if(sd_num_removables)
			kernel_thread_noblock(kernel_task, sd_vol_check);
		sd_thread_init = TRUE;
	}
	else
		timeout((int (*)())sd_thread_timer, 0, hz);	/* try again */
} /* sd_thread_timer() */

static void sd_volume_done(sd_volume_t sdvp)
{
	if(sdvp->sdv_volume_num < sd_num_drives) {
		/*
		 * Primary volume; just mark it invalid.
		 */
		XDBG(("sd_volume_done: INVALIDATING vol %d\n",
			sdvp->sdv_volume_num));
		sdvp->sdv_flags &= ~SVF_VALID;
	}
	else {
		XDBG(("sd_volume_done: FREEING vol %d\n",
			sdvp->sdv_volume_num));
		sd_free_sv(sdvp);
	}
}

static void sdd_init(int drive_num) 
{
	sd_drive_t sddp = &sd_sdd[drive_num];
	struct scsi_device *sdp = &sd_sd[drive_num];
	
	bzero(sddp, sizeof(struct scsi_disk_drive));
	sddp->sdd_sdp = sdp;
	sddp->sdd_irp = DMA_ALIGN(struct inquiry_reply *, &sddp->sdd_irbuf);
	sddp->sdd_flags = 0;
	sddp->sdd_rbuf.b_flags = 0;
	sddp->sdd_state = SDDSTATE_STARTINGCMD;
	sddp->sdd_med_retry_max = SD_MED_RETRIES;
#ifdef	OD_SIMULATE
	sddp->sdd_maxseektime = SD_ODSIM_MAXSEEK;
#endif	OD_SIMULATE
	sddp->sdd_panel_tag = -1;
}

#endif	NSD
/* end of sd.c */
