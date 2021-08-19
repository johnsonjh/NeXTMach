/*	@(#)scsireg.h	1.0	10/23/89	(c) 1989 NeXT	*/
/*
 * scsireg.h -- generic SCSI definitions
 * KERNEL VERSION
 *
 * HISTORY
 * 17-May-90	dmitch
 *	Added SDIOCGETCAP
 * 30-Apr-90	dmitch
 *	Added C6S_SS_START, C6S_SS_STOP, C6S_SS_EJECT, SR_IOST_VOLNA
 * 30-Oct-89	dmitch
 *	Deleted private #defines (--> scsivar.h)
 * 25-Sep-89	dmitch at NeXT
 *	Added scsi_req, support for sg and st drivers
 * 10-Sept-87  	Mike DeMoney (mike) at NeXT
 *	Created.
 */

#import <sys/ioctl.h>
#import <sys/time.h>
#import <kern/queue.h>

#ifndef _SCSIREG_
#define	_SCSIREG_

/*
 * status byte definitions
 */
#define	STAT_GOOD		0x00	/* cmd successfully completed */
#define	STAT_CHECK		0x02	/* abnormal condition occurred */
#define	STAT_CONDMET		0x04	/* condition met / good */
#define	STAT_BUSY		0x08	/* target busy */
#define	STAT_INTMGOOD		0x10	/* intermediate / good */
#define	STAT_INTMCONDMET	0x14	/* intermediate / cond met / good */
#define	STAT_RESERVED		0x18	/* reservation conflict */

#define	STAT_MASK		0x1e	/* clears vendor unique bits */


/*
 * SCSI command descriptor blocks
 * (Any device level driver doing fancy things will probably define
 * these locally and cast a pointer on top of the sd_cdb.  We define
 * them here to reserve the appropriate space, driver level routines
 * can use them if they want.)
 *
 * 6 byte command descriptor block, random device
 */
struct cdb_6 {
	u_int	c6_opcode:8,		/* device command */
		c6_lun:3,		/* logical unit */
		c6_lba:21;		/* logical block number */
	u_char	c6_len;			/* transfer length */
	u_char	c6_ctrl;		/* control byte */
};

/*
 * 6 byte command descriptor block, sequential device
 */
struct cdb_6s {
	u_char	c6s_opcode:8;		/* device command */
	u_char  c6s_lun:3,		/* logical unit */
		c6s_spare:3,		/* reserved */
		c6s_opt:2;		/* bits 1..0 - space type, fixed, 
					 *    etc. */ 
	u_int	c6s_len:24,		/* transfer length */
		c6s_ctrl:8;		/* control byte */
};


/*
 * 10 byte command descriptor block
 * BEWARE: this definition is compiler sensitive due to int on
 * short boundry!
 */
struct cdb_10 {
	u_char	c10_opcode;		/* device command */
	u_char	c10_lun:3,		/* logical unit */
		c10_dp0:1,		/* disable page out (cache control) */
		c10_fua:1,		/* force unit access (cache control) */
		c10_mbz1:2,		/* reserved: must be zero */
		c10_reladr:1;		/* addr relative to prev linked cmd */
	u_int	c10_lba;		/* logical block number */
	u_int	c10_mbz2:8,		/* reserved: must be zero */
		c10_len:16,		/* transfer length */
		c10_ctrl:8;		/* control byte */
};

/*
 * 12 byte command descriptor block
 * BEWARE: this definition is compiler sensitive due to int on
 * short boundry!
 */
struct cdb_12 {
	u_char	c12_opcode;		/* device command */
	u_char	c12_lun:3,		/* logical unit */
		c12_dp0:1,		/* disable page out (cache control) */
		c12_fua:1,		/* force unit access (cache control) */
		c12_mbz1:2,		/* reserved: must be zero */
		c12_reladr:1;		/* addr relative to prev linked cmd */
	u_int	c12_lba;		/* logical block number */
	u_char	c12_mbz2;		/* reserved: must be zero */
	u_char	c12_mbz3;		/* reserved: must be zero */
	u_int	c12_mbz4:8,		/* reserved: must be zero */
		c12_len:16,		/* transfer length */
		c12_ctrl:8;		/* control byte */
};
	
union cdb {
	struct	cdb_6	cdb_c6;
	struct  cdb_6s  cdb_c6s;
	struct	cdb_10	cdb_c10;
	struct	cdb_12	cdb_c12;
};

#define	cdb_opcode	cdb_c6.c6_opcode	/* all opcodes in same place */

/*
 * control byte values
 */
#define	CTRL_LINKFLAG		0x03	/* link and flag bits */
#define	CTRL_LINK		0x01	/* link only */
#define	CTRL_NOLINK		0x00	/* no command linking */

/*
 * six byte cdb opcodes
 * (Optional commands should only be used by formatters)
 */
#define	C6OP_TESTRDY		0x00	/* test unit ready */
#define C6OP_REWIND		0x01	/* rewind */
#define	C6OP_REQSENSE		0x03	/* request sense */
#define	C6OP_FORMAT		0x04	/* format unit */
#define	C6OP_REASSIGNBLK	0x07	/* OPT: reassign block */
#define	C6OP_READ		0x08	/* read data */
#define	C6OP_WRITE		0x0a	/* write data */
#define	C6OP_SEEK		0x0b	/* seek */
#define C6OP_READREV		0x0F	/* read reverse */
#define C6OP_WRTFM		0x10	/* write filemarks */
#define C6OP_SPACE		0x11	/* space records/filemarks */
#define	C6OP_INQUIRY		0x12	/* get device specific info */
#define C6OP_VERIFY		0x13	/* sequential verify */
#define	C6OP_MODESELECT		0x15	/* OPT: set device parameters */
#define	C6OP_MODESENSE		0x1a	/* OPT: get device parameters */
#define	C6OP_STARTSTOP		0x1b	/* OPT: start or stop device */
#define	C6OP_SENDDIAG		0x1d	/* send diagnostic */

/*
 * ten byte cdb opcodes
 */
#define	C10OP_READCAPACITY	0x25	/* read capacity */
#define	C10OP_READEXTENDED	0x28	/* read extended */
#define	C10OP_WRITEEXTENDED	0x2a	/* write extended */
#define	C10OP_READDEFECTDATA	0x37	/* OPT: read media defect info */


/*
 *	c6s_opt - options for 6-byte sequential device commands 
 */
 
#define C6OPT_FIXED		0x01	/* fixed block transfer */
#define C6OPT_LONG		0x01	/* 1 = erase to EOT */
#define C6OPT_IMMED		0x01	/* immediate (for rewind, retension) */
#define C6OPT_BYTECMP		0x02	/* byte compare for C6OP_VERIFY */
#define C6OPT_SIL		0x02	/* suppress illegal length (Exabyte) */
#define C6OPT_SPACE_LB		0x00	/* space logical blocks */
#define C6OPT_SPACE_FM		0x01	/* space filemarks */
#define C6OPT_SPACE_SFM		0x02	/* space sequential filemarks */
#define C6OPT_SPACE_PEOD	0x03	/* space to physical end of data */	

/*	
 *	other 6-byte sequential command constants
 */
 
#define C6S_MAXLEN		0xFFFFFF	
#define C6S_RETEN		0x02	/* byte 4 of load/unload - retension */
#define C6S_LOAD		0x01	/* byte 4 of load/unload - load */

/*
 * these go in the c6_len fields of start/stop command
 */
#define C6S_SS_START		0x01	/* start unit */
#define C6S_SS_STOP		0x00	/* stop unit */
#define C6S_SS_EJECT		0x02	/* eject disk */

/*
 * extended sense data
 * returned by C6OP_REQSENSE
 */
struct esense_reply {
	u_char	er_ibvalid:1,		/* information bytes valid */
		er_class:3,		/* error class */
		er_code:4;		/* error code */
	u_char	er_segment;		/* segment number for copy cmd */
	u_char	er_filemark:1,		/* file mark */
		er_endofmedium:1,	/* end-of-medium */
		er_badlen:1,		/* incorrect length */
		er_rsvd2:1,		/* reserved */
		er_sensekey:4;		/* sense key */
	u_char	er_infomsb;		/* MSB of information byte */
	u_int	er_info:24,		/* bits 23 - 0 of info "byte" */
		er_addsenselen:8;	/* additional sense length */
	u_int	er_rsvd8;		/* copy status (unused) */
	u_char	er_addsensecode;	/* additional sense code */
	
	/* the following are used for tape only as of 27-Feb-89 */
	
	u_char	er_qualifier;		/* sense code qualifier */
	u_char  er_rsvd_e;
	u_char  er_rsvd_f;
	u_int   er_err_count:24,	/* three bytes of data error counter */
		er_stat_13:8;		/* byte 0x13 - discrete status bits */
	u_char  er_stat_14;		/* byte 0x14 - discrete status bits */
	u_char  er_stat_15;		/* byte 0x15 - discrete status bits */
	u_int   er_rsvd_16:8,
		er_remaining:24;	/* bytes 0x17..0x19 - remaining tape */
		
	/* technically, there can be additional bytes of sense info
	 * here, but we don't check them, so we don't define them
	 */
};

/*
 * sense keys
 */
#define	SENSE_NOSENSE		0x0	/* no error to report */
#define	SENSE_RECOVERED		0x1	/* recovered error */
#define	SENSE_NOTREADY		0x2	/* target not ready */
#define	SENSE_MEDIA		0x3	/* media flaw */
#define	SENSE_HARDWARE		0x4	/* hardware failure */
#define	SENSE_ILLEGALREQUEST	0x5	/* illegal request */
#define	SENSE_UNITATTENTION	0x6	/* drive attention */
#define	SENSE_DATAPROTECT	0x7	/* drive access protected */
#define	SENSE_ABORTEDCOMMAND	0xb	/* target aborted command */
#define	SENSE_VOLUMEOVERFLOW	0xd	/* eom, some data not transfered */
#define	SENSE_MISCOMPARE	0xe	/* source/media data mismatch */

/*
 * inquiry data
 */
struct inquiry_reply {
	u_char	ir_devicetype;		/* device type, see below */
	u_char	ir_removable:1,		/* removable media */
		ir_typequalifier:7;	/* device type qualifier */
	u_char	ir_zero1:2,		/* reserved */
		ir_ecmaversion:3,	/* ECMA version number */
		ir_ansiversion:3;	/* ANSI version number */
	u_char	ir_zero2:4,		/* reserved */
		ir_rspdatafmt:4;	/* response data format */
	u_char	ir_addlistlen;		/* additional list length */
	u_char	ir_zero3[3];		/* vendor unique field */
	char	ir_vendorid[8];		/* vendor name in ascii */
	char	ir_productid[16];	/* product name in ascii */
	char	ir_revision[32];	/* revision level info in ascii */
	char	ir_endofid[1];		/* just a handle for end of id info */
};

#define	DEVTYPE_DISK		0x00	/* read/write disks */
#define	DEVTYPE_TAPE		0x01	/* tapes and other sequential devices */
#define	DEVTYPE_PRINTER		0x02	/* printers */
#define	DEVTYPE_PROCESSOR	0x03	/* cpu's */
#define	DEVTYPE_WORM		0x04	/* write-once optical disks */
#define	DEVTYPE_READONLY	0x05	/* cd rom's, etc */
#define	DEVTYPE_NOTPRESENT	0x7f	/* logical unit not present */

/*
 * read capacity reply
 */
struct capacity_reply {
	u_int	cr_lastlba;		/* last logical block address */
	u_int	cr_blklen;		/* block length */
};

/*
 * Standard Mode Select/Mode Sense data structures
 */
 
struct mode_sel_hdr {

	u_char 		msh_sd_length_0;	/* byte 0 - length (mode sense
						 *    only)  */
	u_char		msh_med_type;		/* medium type - random access
						 *   devices only */
	u_char		msh_wp:1,		/* byte 2 bit 7 - write protect
						 *   mode sense only) */
			msh_bufmode:3,		/* buffered mode - sequential
						 *   access devices only */
			msh_speed:4;		/* speed - sequential access
						 *   devices only */
	u_char		msh_bd_length;		/* block descriptor length */
};

struct mode_sel_bd {				/* block descriptor */

	u_int		msbd_density:8,
			msbd_numblocks:24;
	u_int		msbd_rsvd_0:8,		/* byte 4 - reserved */
			msbd_blocklength:24;	
};

#define MODSEL_DATA_LEN	0x30

struct mode_sel_data {

	/* transferred to/from target during mode select/mode sense */
	struct mode_sel_hdr msd_header;
	struct mode_sel_bd  msd_blockdescript;
	u_char msd_vudata[MODSEL_DATA_LEN];	/* for vendor unique data */
};

/* 
 * struct for MTIOCMODSEL/ MTIOCMODSEN
 */
struct modesel_parms {
	struct mode_sel_data    msp_data;
	int			msp_bcount;	/* # of bytes to DMA */
};

/*
 * Day-to-day constants in the SCSI world
 */
#define	SCSI_NTARGETS	8		/* 0 - 7 for target numbers */
#define	SCSI_NLUNS	8		/* 0 - 7 luns for each target */

/*
 * Defect list header
 * Used by FORMAT and REASSIGN BLOCK commands
 */
struct defect_header {
	u_char	dh_mbz1;
	u_char	dh_fov:1,		/* format options valid */
		dh_dpry:1,		/* disable primary */
		dh_dcrt:1,		/* disable certification */
		dh_stpf:1,		/* stop format */
		dh_mbz2:4;
	u_short	dh_len;			/* items in defect list */
};

/*
 * SCSI Request used by sg driver via SGIOCREQ and internally in st driver
 */
 
struct scsi_req {

	/*** inputs ***/
	
	union cdb		sr_cdb;		/* command descriptor block - 
						 * one of three formats */
	int 			sr_dma_dir;	/* SR_DMA_RD / SR_DMA_WR */
	caddr_t			sr_addr;	/* memory addr for data 
						 * transfers */
	int			sr_dma_max;	/* maximum number of bytes to
						 * transfer */
	int			sr_ioto;	/* I/O timeout in seconds */
						 
	/*** outputs ***/
	
	int			sr_io_status;	/* driver status */
	u_char			sr_scsi_status;	/* SCSI status byte */
	struct esense_reply 	sr_esense;	/* extended sense in case of
						 * check status */
	int			sr_dma_xfr;	/* actual number of bytes 
						 * transferred by DMA */
	struct	timeval		sr_exec_time;	/* execution time in 
						 * microseconds */
	/*** for driver's internal use ***/
	
	u_char			sr_flags;
	queue_chain_t		sr_io_q;	/* for linking onto sgdp->
						 *    sdg_io_q */
}; /* scsi_req */

struct scsi_adr {

	u_char			sa_target;
	u_char			sa_lun;
	
}; /* scsi_adr */


/*
 *	Generic SCSI ioctl requests
 */
 
#define SGIOCSTL	_IOW ('s', 0, struct scsi_adr)	/* set target/lun */
#define	SGIOCREQ	_IOWR('s', 1, struct scsi_req) 	/* cmd request */
#define SGIOCENAS	_IO(  's', 2)		 	/* enable autosense */
#define SGIOCDAS	_IO(  's', 3)			/* disable autosense */
#define SGIOCRST	_IO(  's', 4)			/* reset SCSI bus */

/*
 *	ioctl requests specific to SCSI disks
 */
#define	SDIOCSRQ	_IOWR('s', 1, struct scsi_req) 	/* cmd request using */
							/* struct scsi_req */
#define SDIOCODSIM	_IOW ('s', 2, int)		/* enable/disable 
							 * OD simulation */
#define SDIOCSMAXSEEK	_IOW ('s', 3, int)		/* set max seek time
							 *   in ms */
#define SDIOCGMAXSEEK	_IOR ('s', 4, int)		/* get max seek time
							 *   in ms */
#define SDIOCGETCAP	_IOR  ('s', 5, struct capacity_reply)
							/* Get Read 
							 * Capacity info */
							
/* 
 *	ioctl requests specific to SCSI tapes 
 */
 
#define	MTIOCFIXBLK	_IOW('m', 5, int )	/* set fixed block mode */
#define MTIOCVARBLK 	_IO('m',  6)		/* set variable block mode */
#define MTIOCMODSEL	_IOW('m', 7, struct modesel_parms)	
						/* mode select */
#define MTIOCMODSEN	_IOWR('m',8, struct modesel_parms)	
						/* mode sense */
#define MTIOCINILL	_IO('m',  9)		/* inhibit illegal length */
						/*    errors */
#define MTIOCALILL	_IO('m',  10)		/* allow illegal length */
						/*    errors */
#define	MTIOCSRQ	_IOWR('m', 11, struct scsi_req) 	
						/* cmd request using 
						 * struct scsi_req */


/*
 *	constants used in scsi_req
 */
 
 	/*** sr_dma_dir ***/
	
#define 	SR_DMA_RD	0		/* DMA from device to host */
#define 	SR_DMA_WR	1		/* DMA from host to device */

	/*** sr_io_status ***/
	
#define 	SR_IOST_GOOD	0		/* successful */
#define		SR_IOST_SELTO	1		/* selection timeout */
#define		SR_IOST_CHKSV	2		/* check status, sr_esense */
						/*    valid */
#define		SR_IOST_CHKSNV	3		/* check status, sr_esense */
						/*    not valid */
#define		SR_IOST_DMAOR	4		/* target attempted to move */
						/*    more than sr_dma_max */
						/*    bytes */
#define		SR_IOST_IOTO	5		/* sr_ioto exceeded */
#define		SR_IOST_BV	6		/* SCSI Bus violation */
#define		SR_IOST_CMDREJ	7		/* command reject (by 
						 *    driver) */
#define 	SR_IOST_MEMALL	8		/* memory allocation failure */
#define		SR_IOST_MEMF	9		/* memory fault */
#define		SR_IOST_PERM	10		/* not super user */
#define 	SR_IOST_NOPEN	11		/* device not open */
#define		SR_IOST_TABT	12		/* target aborted command */
#define		ST_IOST_BADST	13		/* bad SCSI status byte  */
						/*  (other than check status)*/
#define		ST_IOST_INT	14		/* internal driver error */
#define 	SR_IOST_BCOUNT	15		/* unexpected byte count */
						/* seen on SCSI bus */ 
#define 	SR_IOST_VOLNA	16		/* desired volume not available
						 */
#define 	SR_IOST_WP	17		/* Media Write Protected */

#endif _SCSIREG_
