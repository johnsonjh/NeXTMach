/*
 * sgvar.h -- definitions for generic scsi driver
 * KERNEL VERSION
 *
 * HISTORY
 * 26-Jan-89	Doug Mitchell (dmitch) at NeXT
 *	Created.
 */

#ifndef _SGVAR_
#define	_SGVAR_

/*
 * private per-device sg info
 * this struct parallels scsi_device and bus_device structs
 */
struct scsi_generic_device {
	struct	scsi_device 	*sgd_sdp;	/* to parallel scsi_device */
	struct	esense_reply 	*sgd_erp;	/* used after STAT_CHECK */
	queue_head_t 		sgd_io_q;	/* queue of scsi_req's */
	long			sgd_savedresid;	/* holds real resid during REQ
						 *    SENSE */
	u_char			sgd_savedop;	/* hold old opcode during REQ
						 *    SENSE */
	u_char			sgd_state;	/* driver state */
	char			sgd_open;	/* boolean - this device is 
						 *    open */
	char			sgd_autosense;	/* autosense after error 
						 *    enabled */
	char	sgd_erbuf[sizeof(struct esense_reply)+DMA_BEGINALIGNMENT-1];
};

	/*** sgd_state values ***/
	
#define	SGDSTATE_STARTINGCMD	0	/* initial processing of cmd */
#define	SGDSTATE_DOINGCMD	1	/* doing original cmd request */
#define	SGDSTATE_GETTINGSENSE	2	/* getting REQ SENSE on failed cmd */

/*
 * conversions from dev_t to disk unit and partition
 */
#define	SG_UNIT(dev)		(minor(dev) >> 3)
#define	SG_PART(dev)		(minor(dev) & 0x7)	
/*
 * conversion from disk unit and partition to dev_t (minor only)
 */
#define	SG_DEVT(unit,part)	(((unit) << 3) | ((part) & 0x7))

/*
 * I/O timouts in seconds
 */
 
#define SG_IOTO_NORM	20		/* default */
#define SG_IOTO_SENSE	3		/* request sense */

/*
 * defined in scsiconf.c
 */
extern struct scsi_device sg_sd[];
extern struct scsi_generic_device sg_sgd[];

#endif _SGVAR_
