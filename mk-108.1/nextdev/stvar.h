/*	@(#)stvar.h	1.0	10/23/89	(c) 1989 NeXT	*/
/*
 * stvar.h -- definitions for scsi tape driver
 * KERNEL VERSION
 *
 * HISTORY
 * 25-Sep-89	dmitch at NeXT
 *	Added Exabyte event byte disconnect constants.
 * 22-Feb-89  	dmitch at NeXT
 *	Created.
 */

#ifndef _STVAR_
#define	_STVAR_
   
/*
 * private per-device st info
 * this struct parallels scsi_device and bus_device structs
 */
struct scsi_tape_device {
	struct	scsi_device 	*std_sdp;	/* -> parallel scsi_device */
	struct	inquiry_reply 	*std_irp;	/* for use during autoconf */
	struct	capacity_reply 	*std_crp;	/* for READ CAPACITY cmd */
	struct	modesel_parms 	*std_mspp;	/* for mode select/sense cmd */
	struct	esense_reply 	*std_erp;	/* results of last request
						 * sense command. Any other 
						 * SCSI command invalidates
						 * this data via STF_ERPVALID 
						 */
	struct	scsi_req 	std_scsi_req;	/* special request used only
						 * during init time. No locks.
						 */
	u_short			std_flags;	/* see below for defines */
	int			std_waitlimit;	/* wait limit for polled 
						 * commands */
	int			std_waitcnt;	/* wait cnt for polled cmds */
	u_int			std_blocksize;	/* size if STF_FIXED true */
	u_int			std_tries;	/* attempts to execute cmd */
	long			std_savedresid;	/* holds real resid during REQ
						 * SENSE */
	u_char			std_savedopcode; /* hold old opcode during REQ
						 * SENSE */
	u_char			std_state;	/* driver state */
	queue_head_t		std_io_q;	/* for queueing st_req's */
	char	std_irbuf[sizeof(struct inquiry_reply)+DMA_BEGINALIGNMENT-1];
	char	std_erbuf[sizeof(struct esense_reply)+DMA_BEGINALIGNMENT-1];
	char	std_crbuf[sizeof(struct capacity_reply)+DMA_BEGINALIGNMENT-1];
	char	std_msbuf[sizeof(struct modesel_parms)+DMA_BEGINALIGNMENT-1];
};

/*
 * bit fields in std_flags 
 */
 
#define STF_POLL_IP	0x0001		/* polling command in progress. */
					/* Cleared at command complete. */
#define STF_ERPVALID	0x0002		/* *std_erp is valid; i.e., the last */
					/* command executed was a request sense
					 * invoked by stintr
					 */
#define STF_FIXED	0x0004		/* fixed block size mode */
#define STF_WRITE	0x0008		/* the last command was a write */
#define STF_OPEN	0x0010		/* device currently open */
#define STF_SIL		0x0020		/* supress illegal length errors */
					/*   on reads */

#define	STDSTATE_STARTINGCMD	0	/* initial processing of cmd */
#define	STDSTATE_DOINGCMD	1	/* doing original cmd request */
#define	STDSTATE_GETTINGSENSE	2	/* getting REQ SENSE on failed cmd */

/*
 * dev_t to tape unit, other minor device bits 
 */
#define	ST_UNIT(dev)		(minor(dev) >> 3)
#define ST_RETURN(dev)		(minor(dev) & 1) 	/* bit 0 true - no */
							/* rewind on close */
#define ST_EXABYTE(dev)		(minor(dev) & 2)	/* bit 1 true - */
							/* Exabyte drive */

#define	ST_MAXTRIES	3		/* max retry attempts */

/*
 * I/O timouts in seconds
 */
 
#define ST_IOTO_NORM	120		/* default */
#define ST_IOTO_RWD	(5 * 60)	/* rewind command */
#define ST_IOTO_SENSE	1		/* request sense */
#define ST_IOTO_SPR	60		/* space records */
#define ST_IOTO_SPFM	(10 * 60)	/* space file marks. 10 minutes */
					/* PER FILE MARK TO SPACE. */
/*
 *   str_status values
 */
 
#define STRST_GOOD 	0		/* OK */					
#define STRST_BADST	1		/* bad SCSI status */
#define STRST_IOTO	2		/* I/O timeout */
#define STRST_VIOL	3		/* SCSI bus violation */
#define STRST_SELTO	4		/* selection timeout */
#define STRST_CMDREJ	5		/* driver command reject */
#define STRST_OTHER	6		/* other error */

/*
 * Polling flags
 */
#define ST_INTERRUPT	0	/* interrupt driven */
#define	ST_WAIT		1	/* poll and wait if device busy */

/*
 * Number of times to poll for device ready
 */
#define	ST_WAITRDYTRIES	10

/*
 * defined in scsiconf.c
 */
extern struct scsi_device st_sd[];
extern struct scsi_tape_device st_std[];

/* 
 * Vendor Unique mode select data for Exabyte drive
 */
struct exabyte_vudata {
	u_int		ct:1,		/* cartridge type */
			rsvd1:1,
			nd:1,		/* no disconnect during data xfer */
			rsvd2:1,
			nbe:1,		/* No Busy Enable */
			ebd:1,		/* Even Byte Disconnect */
			pe:1,		/* Parity Enable */
			nal:1,		/* No Auto Load */
			rsvd3:7,
			p5:1,		/* P5 cartridge */
			motion_thresh:8,	/* motion threshold */
			recon_thresh:8;	/* reconnect threshold */
	u_char		gap_thresh;	/* gap threshold */
};

#define MSP_VU_EXABYTE	0x05		/* # vendor unique bytes for mode  */
					/*     select/sense */

#endif _STVAR_
