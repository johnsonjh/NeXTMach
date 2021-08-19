/*	@(#)scvar.h	1.0	09/10/87	(c) 1987 NeXT	*/

/* 
 * scvar.h -- variable definitions for NCR53C90 specific scsi routines
 * KERNEL VERSION
 *
 * HISTORY
 * 19-Jun-90	Doug Mitchell
 *	Added FLPCTL_ADDRS
 * 20-Feb-90	Doug Mitchell
 *	Added s5c_sfah, s5c_sfad
 * 09-Feb-89	Doug Mitchell at NeXT
 *	Added scsi_5390_ctrl.s5c_53c90a
 * 01/20/89	Doug Mitchell (dmitch) at NeXT
 *	Added S5CSTATE_IGNOREILLCMD
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 */ 

#ifndef _SCVAR_
#define	_SCVAR_

#import <nextdev/sf_access.h>

/*
 * private per-5390-chip info
 * this struct parallels scsi_ctrl and bus_ctrl structs
 */
struct scsi_5390_ctrl {			/* per controller info */
	struct	scsi_ctrl *s5c_scp;	/* ptr to scsi_ctrl struct */
	struct	dma_chan s5c_dc;	/* DMA info */
	u_char	s5c_53c90a;		/* boolean. True indicates that this
					 * is a '90A part, i.e., we can do
					 * disconnects. */
	dma_list_t s5c_dl;		/* DMA segment info */
	int	s5c_errcnt;		/* errors during cmd execution */
	int	s5c_cmdlen;		/* cdb length in bytes */
	u_char	s5c_state;		/* controller state */
	u_char	s5c_msgoutstate;	/* MSG OUT states */
	u_char	s5c_postmsgstate;	/* new state after msg out is done */
	u_char	s5c_msgout;		/* msg to send out */
	u_char	s5c_target;		/* target if parity err on reselect */
	/* saved interrupt state */
	u_char	s5c_status;		/* last 5390 status */
	u_char	s5c_seqstep;		/* last 5390 seqstep */
	u_char	s5c_intrstatus;		/* last 5390 intrstatus */
	u_char	s5c_dmastatus;		/* mass-storage chip status */
	/* state of currently connected target */
	struct	scsi_device *s5c_cursdp;/* currently connected scsi device */
	caddr_t	s5c_curaddr;		/* current address of dma */
	long	s5c_curbcount;		/* current remaining bytes for dma */
	long	s5c_curpadcount;	/* current remaining bytes for pad */
	u_char	s5c_curmsg;		/* current command msg */
	sf_access_head_t s5c_sfah;	
					/* scsi/floppy access struct */
	struct sf_access_device s5c_sfad;
};

/*
 * Cube prototypes - bit 1 of od's disr controls 53c90A/82077 access
 * Warp9, X15      - bit 6 of fd's flpctl
 */
#define DISR_ADDRS	(P_DISK + 4)
#define S5C_DISR_SFA	0x00		// write this to disr for SCSI access 
#define FLPCTL_ADDRS	(P_FLOPPY + 8)	
			 
#define	S5CSTATE_DISCONNECTED	0	/* disconnected & not reselectable */
#define	S5CSTATE_RESELECTABLE	1	/* disconnected & reselectable */
#define	S5CSTATE_SELECTING	2	/* SELECT* command issued */
#define	S5CSTATE_INITIATOR	3	/* following target SCSI phase */
#define	S5CSTATE_DISABLINGSEL	4	/* disabling reselectable state */
#define	S5CSTATE_COMPLETING	5	/* initiator cmd cmplt in progress */
#define	S5CSTATE_DMAING		6	/* dma is in progress */
#define	S5CSTATE_ACCEPTINGMSG	7	/* MSGACCEPTED cmd in progress */
#define	S5CSTATE_SENDINGMSG	8	/* MSG_OUT phase in progress */
#define	S5CSTATE_PADDING	9	/* transfer padding in progress */
#define	S5CSTATE_GETTINGMSG	10	/* transfer msg in progress */
#define	S5CSTATE_GETTINGLUN	11	/* getting lun after parity error */
#define	S5CSTATE_IGNOREILLCMD	12	/* ignore illegal command after 
					 *   reselect/select race (dpm) */

#define	S5CMSGSTATE_NONE	0	/* no message to send */
#define	S5CMSGSTATE_WAITING	1	/* have msg, awaiting MSG OUT phase */
#define	S5CMSGSTATE_SAWMSGOUT	2	/* sent msg, check for retry */

/*
 * declared in scsiconf.c
 */
extern struct scsi_ctrl sc_sc[];
extern struct scsi_5390_ctrl sc_s5c[];
extern struct sf_access_head sf_access_head[];

#endif _SCVAR_

