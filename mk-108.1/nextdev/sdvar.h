/*	@(#)sdvar.h	1.0	10/23/89	(c) 1989 NeXT	*/
/*
 * sdvar.h -- definitions for generic scsi disk driver
 * KERNEL VERSION
 *
 * HISTORY
 * 13-Aug-90	Doug Mitchell at NeXT
 *	Added SD_MED_RETRIES_LABEL, SDF_GETLABEL
 *  2-Aug-90	Gregg Kellogg (gk) at NeXT
 *	Changes for programatic disksort interface.
 * 10-Jul-90	Gregg Kellogg (gk) at NeXT
 *	Added support for procedural access to disk queue.
 * 03-May-90	Doug Mitchell at NeXT
 *	Split scsi_disk_device into scsi_disk_drive and scsi_disk_volume for
 *	    multivolume support.
 * 22-Mar-89	Doug Mitchell at NeXT
 *	Added scsi_disk_device.sdd_retry, scsi_disk_device.sdd_srq; 
 *	    added SD_IOTO_xxx timeout constants.
 * 10-Sept-87  	Mike DeMoney (mike) at NeXT
 *	Created.
 */

#ifndef _SDVAR_
#define	_SDVAR_

#import <sys/types.h>
#import <sys/time_stamp.h>
#import <sys/disksort.h>

#ifdef	DEBUG
/* #define OD_SIMULATE	1	/* simulate OD disk */
#endif	DEBUG

/*
 * private per-device sd info. This information is split into a drive-specific
 * portion and a volume-specific portion. Each drive can have any number of 
 * volumes mapped to it; at most one volume is present (mounted) at a time.
 * Mapping from volume to drive occurs at attach() time (either autoconfig or 
 * open); henceforth, a volume is always assigned to the same drive as long as 
 * it's valid. We don't reassign volumes to other drives because the driver
 * lacks the information necessary to determine which drive can handle
 * which disk. The exception to this fixed mapping is that when a volume is
 * ejected, the USER can insert it back into any drive; if we can read it,
 * then we'll reassign the volume to that drive.
 *
 * The scsi_disk_volume structs are dynamically allocated; such a 
 * struct is created at attach time and freed when both the raw and block
 * devices associated with the volume are closed AND the volume is ejected.
 * 'Primary volumes' (volume 1 for drive 1, etc.) are persistent; these are
 * never freed but only marked invalid.
 *
 * The scsi_disk_drive struct parallels scsi_device and bus_device structs.
 * The scsi_disk_volume struct represents an item in /dev/.
 */
typedef	struct scsi_disk_drive *sd_drive_t;
typedef struct scsi_disk_volume *sd_volume_t;

struct scsi_disk_volume {
	sd_drive_t	sdv_sddp;	/* drive this is mapped to. */
	u_int		sdv_volume_num;
	u_int		sdv_flags;	/* see SVF_xxx, below */
	u_char		sdv_blk_open;	/* block device open - one per 
					 * partition */
	u_char		sdv_raw_open;	/* raw device open - one per 
					 * partition */
	u_int		sdv_bratio;	/* device blocks per fs block */
	int		sdv_vol_tag;	/* for alert panels */
	uid_t		sdv_owner;	/* owner at last open */
	struct	buf 	sdv_cbuf;	/* special command buf. Serializes 
					 * access to sdv_srp. */
	struct scsi_req *sdv_srp;	/* for I/Os using a scsi_req */
	struct ds_queue	sdv_queue;	/* queue of I/Os. Bufs are enqueued by
					 * sdstrategy(), processed by 
					 * sd_vstart(), sdstart(), and
					 * sdintr(), and dequeued by 
					 * sddone(). */
	
	/*
	 * various DMA buffers and their well-aligned pointers.
	 */
	char	sdv_erbuf[sizeof(struct esense_reply)+DMA_BEGINALIGNMENT-1];
	char	sdv_crbuf[sizeof(struct capacity_reply)+DMA_BEGINALIGNMENT-1];
	struct esense_reply 	*sdv_erp;	/* used after STAT_CHECK */
	struct capacity_reply 	*sdv_crp;	/* for READ CAPACITY cmd */
	struct disk_label 	*sdv_dlp;	/* disk label. kalloc'd. */

}; /* struct scsi_disk_volume */

/*
 * scsi_disk_volume.sdv_flags 
 */
#define SVF_NOTAVAIL		0x00000001	/* user responded to disk
						 * insert alert with "not 
						 * available" */
#define SVF_MOUNTED		0x00000002	/* this volume currently is
						 * in drive sdvp->sdv_sddp */
#define SVF_LABELVALID		0x00000004	/* *sdvp->sdv_dlp contains
						 * valid label */
#define SVF_ALERT		0x00000008	/* alert panel up for this
						 * volume */
#define SVF_NEWVOLUME		0x00000040	/* newly opened volume. true
						 * from call to sdopen() until
						 * a new disk is detected in
						 * the desired drive. */ 
#define SVF_VALID		0x00000080	/* true if we know something 
						 * about this volume. Only used
						 * for first NSD volume 
						 * structs; others are kfree'd
						 * when SVF_VALID is false.
						 */
#define SVF_ACTIVE		0x00000100	/* currently executing command
						 */
#define SVF_WAITING		0x00000200	/* waiting for this volume to 
						 * be inserted */
#define SVF_FORMATTED		0x00000400	/* disk formatted. Set when
						 * at least one I/O succeeds
						 * in while reading label or
						 * via DKIOCSFORMAT */
#define SVF_WP			0x00000800	/* disk is write protected */
						 
struct scsi_disk_drive {

	sd_volume_t	sdd_sdvp;	/* volume currently inserted. NULL 
					 * indicates no volume present. */
	sd_volume_t	sdd_intended_sdvp;	
					/* non-null if expecting a volume 
					 * insert */
	scsi_device_t	sdd_sdp;	/* ptr to parallel scsi_device */
	u_int		sdd_flags;	/* see SDF_xxx, below */
	u_char		sdd_state;	/* driver state */

	/* 
	 * all four retry counters increment.
	 */
	u_char		sdd_busy_retry;	/* busy retries */
	u_char		sdd_gross_retry;/* gross error retries */
	u_char		sdd_med_retry;	/* media/hardware retries */
	u_char		sdd_rdy_retry;	/* "wait for ready" retries */
	u_char 		sdd_med_retry_max;
					/* max allowable media error retries */
#ifdef	OD_SIMULATE
	char		sdd_opcounter;	/* counts writes to simulate erase/
					 * write/verify */
	u_char		sdd_simulate;	/* enable the simulation */
	u_int		sdd_lastblock;	/* last block read/written - for 
					 * calculating seek time */
	u_int		sdd_maxseektime;/* max seek time, in ms, of emulated
					 *   drive */
#endif	OD_SIMULATE
	long		sdd_savedresid;	/* holds real resid during REQ SENSE on 
					 * error detect */
	u_char		sdd_savedopcode;/* hold old opcode during REQ SENSE */
	struct	buf 	sdd_rbuf;	/* raw i/o buf */
	char	sdd_irbuf[sizeof(struct inquiry_reply)+DMA_BEGINALIGNMENT-1];
	struct inquiry_reply *sdd_irp;	/* for use during autoconf */
	struct tsval 	sdd_last_eject;	/* time of last eject command */
	int 		sdd_panel_tag;	/* tag of "Please Eject Disk" panel. 
					 * -1 indicates no panel present. */

}; /* scsi_disk_drive */

/*
 * scsi_disk_drive.sdd_flags
 */
#define SDF_POLLING	0x00000001	/* no interrupts - used during 
					 * autoconf */
#define SDF_INTERRUPT	0x00000000	/* logical inverse of SDF_POLLING */
#define SDF_EJECTING	0x00000002	/* ejecting disk */
#define SDF_DISCONNECT	0x00000004	/* disconnects OK for this command */
#define SDF_GETLABEL	0x00000008	/* reading label */
	 
/*
 * scsi_disk_drive.sdd_state values
 */
#define	SDDSTATE_STARTINGCMD	0	/* initial processing of cmd */
#define	SDDSTATE_DOINGCMD	1	/* doing original cmd request */
#define	SDDSTATE_GETTINGSENSE	2	/* getting REQ SENSE on failed cmd */
#define SDDSTATE_RETRYING	3	/* retrying */
#define SDDSTATE_VOLWAIT	4	/* waiting for new volume */

/*
 * retry counts
 */
#define	SD_GROSS_RETRIES	3	/* max gross error attempts */
#define SD_MED_RETRIES		10	/* max media/hw error retries */
#define SD_MED_RETRIES_LABEL	2	/* max media/hw error retries while 
					 * reading label */
#define SD_BUSY_RETRIES 	50	/* max Busy status retries */
#define	SD_RDY_RETRIES		20	/* max "wait for ready" retries */
	 
/*
 * conversions from dev_t to disk volume and partition
 */
#define	SD_VOLUME(dev)		(minor(dev) >> 3)
#define	SD_PART(dev)		(minor(dev) & 0x7)	

/*
 * conversion from disk unit and partition to dev_t (minor only)
 */
#define	SD_DEVT(unit,part)	(((unit) << 3) | ((part) & 0x7))
#define SD_LIVE_PART	7		/* partition # of "live" partition */

#define NUM_SDV		16		/* total number of scsi disk volumes */
#define NUM_SDVP	(NUM_SDV+2)	/* number of volume pointers. 
					 * One per volume.
					 * One for the controller device.
					 * One for the sd_vol_check thread. */
#define SCD_VOLUME	NUM_SDV		/* volume used by controller device */ 
#define SVC_VOLUME	(NUM_SDV+1)	/* volume used by sd_vol_check thread 
					 */
#define SVC_DEVICE	NSD		/* scsi_{disk_drive, device} used by
					 * sd_vol_check thread */
/*
 * I/O timouts in seconds
 */ 
#define SD_IOTO_NORM	60		/* default */
#define SD_IOTO_FRMT	(30 * 60)	/* format command */
#define SD_IOTO_STST	60		/* start/stop */
#define SD_IOTO_SENSE	10		/* request sense */
#define SD_IOTO_EJECT	20		/* eject disk */
#define SD_DELAY_EJECT	3		/* time to wait after eject command
					 * before any other I/O (PLI bug) */
/*
 * misc. constants
 */
#define SD_VC_DELAY_NORM	1000000	/* us to sleep between sd_vol_check
					 * acesses */
					 
/*
 * OD simulation constants
 */
#define	SD_ODSIM_WPASSES	3	/* # of write passes */
#define SD_ODSIM_MAXSEEK	90	/* default value of sdd_maxseektime
					 * (in ms)
					 */
#define SD_ODSIM_WIN_SEEK	16	/* normal winchester seek time */
/*
 * defined in scsiconf.c
 */
extern struct scsi_device sd_sd[];
extern struct scsi_disk_drive sd_sdd[];

/*
 * struct used for enqueueing "aborted disk insertion" requests. 
 * sd_panel_abort(), which is actually called by the vol driver thread, 
 * generates these and enqueues them on the sd_abort_q. The sd_vol_check 
 * thread dequeues these and marks the associated volumes in the various
 * sd_drive_t's sdd_queues with SVF_NOTAVAIL and does an sddone() on each one,
 * aborting the I/Os.
 */
struct sd_abort_req {
	queue_chain_t	link;
	sd_volume_t	sdvp;
	int 		tag;		/* not used (yet) */
	int 		response;	/* not used (yet). Same as ps_value in
					 * the vol_panel_resp message. */
};

/*
 * struct used to make "Sync and Eject" requests of the sd_vol_check()
 * thread. 
 */ 
struct sd_eject_req {
	queue_chain_t	link;		/* for linking on fd_eject_req_q */
	sd_volume_t	sdvp;		/* volume to eject */
	sd_volume_t	new_sdvp;	/* intended volume */
};

#define NUM_FREE_EJECT_REQ	4

#endif _SDVAR_
