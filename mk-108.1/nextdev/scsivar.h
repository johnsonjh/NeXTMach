/*	@(#)scsivar.h	1.0	10/23/89	(c) 1989 NeXT	*/
/*
 * scsivar.h -- definitions of generic scsi driver data structures
 * KERNEL VERSION
 *
 * HISTORY
 *  2-Aug-90	Gregg Kellogg (gk) at NeXT
 *	Changes for programatic disksort interface.  Use queue in busvar
 *	for enqueuing active queues.
 *
 * 18-Jul-90	Gregg Kellogg (gk) at NeXT
 *	Use sc_bcp->bc_tab instead of sc_work_q.
 * 30-Oct-89	dmitch
 *	Added private #defines (from scsireg.h)
 * 10-Feb-89	dmitch at NeXT
 *	Disabled SCSI_MACROS
 * 10-Sept-87	Mike DeMoney (mike) at NeXT
 *	Created.
 */

#ifndef _SCSIVAR_
#define	_SCSIVAR_

#import <kern/lock.h>
#import <nextdev/busvar.h>

/* #define DISC_DBG 0 */

/*
 * scsi_csw -- scsi controller switch
 * pointers to scsi controller specific functions
 */
struct scsi_csw {
	int (*scsw_start)(/* sdp */);	/* run command sdp->sd_cdb */
	int (*scsw_ioctl)(/* scp, cmd, data, flag */);
	void (*scsw_reset)(/* scp, abortflag, whystr */);
};

/*
 * abortflag arg to scsw_reset routine
 */
#define	SCRESET_ABORT	0	/* abort any currently executing command */
#define	SCRESET_RETRY	1	/* retry any currently executing command */

/*
 * scsi_ctrl -- parallels bus_ctrl struct
 * one per scsi controller
 */
struct scsi_ctrl {
	queue_head_t sc_dis_q;		/* disconnected scsi_devices */
	queue_chain_t sc_ctrl_list;	/* link for list of known ctrlrs */
	struct	bus_ctrl *sc_bcp;	/* ptr to parallel bus_ctrl struct */
	struct	scsi_csw *sc_scswp;	/* ptr to controller specific funcs */
	u_char	sc_lunmask[SCSI_NTARGETS]; /* bitmask of attached luns */
	u_char	sc_target;		/* we're responding as this target */
	u_char	sc_ipl;			/* interrupt level */
	u_char	sc_active;		/* controller active */
	u_char	sc_expectintr;		/* expect interrupt in future */
	int	sc_timer;		/* timeout period in hz */
	u_char sc_disc_cnt;		/* number of devices currently 
					 * disconnected */
};

#define	SCACTIVE_IDLE		0	/* controller is idle */
#define	SCACTIVE_POLLING	1	/* scsi_pollcmd() in use */
#define	SCACTIVE_INTR		2	/* normal interrupt usage */
#define	SCACTIVE_HOLD		3	/* holding ctrlr for timeout stuff */

/*
 * scsi_dsw -- scsi device switch
 * pointers to scsi device specific functions
 *
 * NOTE:  Order of lines in config file will affect order in
 * which "device-type match attempts" are made.  => Most finicky device
 * sub-types should come first in config file before less finicky types
 * of the same SCSI device type.
 */
struct scsi_dsw {
	char *sdsw_name;		/* scsi device type described */
	u_char sdsw_nexttarget;		/* next target to check during config */
	u_char sdsw_nextlun;		/* next lun to check during config */
	struct scsi_device *(*sdsw_getsdp)();
	int (*sdsw_slave)();		/* is it one of us? */
	void (*sdsw_attach)();		/* link its device structs */
	int (*sdsw_start)();		/* start device fsm */
	void (*sdsw_intr)();		/* device interrupt handler */
};

/*
 * scsi_device -- parallels bus_device struct
 * one per scsi device
 */
struct scsi_device {
	queue_chain_t sd_work_q;	/* link for ctlr work queue */
	queue_chain_t sd_dis_q;		/* link for ctlr disconnect queue */
	struct	bus_device *sd_bdp;	/* ptr to parallel bus_device struct */
	struct	scsi_dsw *sd_sdswp;	/* ptr to device specific funcs */
	struct	scsi_ctrl *sd_scp;	/* ptr to scsi_ctrl for this device */
	u_char	sd_target;		/* scsi target number */
	u_char	sd_lun;			/* lun on target */
	u_char	sd_devtype;		/* ASCII - 'd', 't', 'g', etc. */
	int	sd_timer;		/* reselect timer in hz */
	u_int	sd_expectresel:1,	/* expecting reselect in future */
		sd_reseltimeout:1,	/* timed-out waiting for interrupt */
		sd_active:1,		/* device is on controller queue */
	/*
	 * Info filled in by device level and passed to controller level
	 * to start cmd
	 *
	 * Sd_timeout should be set to a reasonable interrupt timeout
	 * for the device operation.  Sd_padcount may be used when device
	 * will transfer more bytes than the dma length given in sd_bcount.
	 * (E.g. reading less than a full disk sector.)  For transfers
	 * to the target (writes), sd_padcount bytes of zeros will be sourced
	 * before an error is indicated.  For transfers from the target (reads)
	 * sd_padcount bytes will be received and ignored before and error
	 * is indicated.
	 *
	 * NOTE: We don't guarantee that these values will be unchanged
	 * after the command is executed.
	 */
		sd_disconnectok:1,	/* allow target to disconnect */
		sd_read:1;		/* i/o from target to initiator */
	union	cdb sd_cdb;		/* current cdb */
	caddr_t	sd_addr;		/* buffer address */
	long	sd_bcount;		/* dma byte count */
	long	sd_padcount;		/* pad/toss count if not full sector */
	pmap_t	sd_pmap;		/* addr is mapped by this pmap */
	int	sd_timeout;		/* reselect timeout period in secs */
	/*
	 * this info is filled in by controller for return to device
	 * level interrupt routine
	 */
	long	sd_resid;		/* # of requested bytes not xfered */
	long	sd_bytes_moved;		/* actual number of bytes transferred,
					 *   including pads */
	u_char	sd_status;		/* SCSI status if SDSTATE_COMPLETED */
	u_char	sd_state;		/* device level fsm state */
};

typedef	struct scsi_device *scsi_device_t;

/*
 * controller level calls device level routine when sd_state is any
 * of SELTIMEOUT, DISCONNECTED, or COMPLETED.  Sd_status and sense
 * information in sd_er is only valid when state is COMPLETED
 */
#define	SDSTATE_START		0	/* device yet to be selected */
#define	SDSTATE_INPROGRESS	1	/* selected, xfer in progress */
#define	SDSTATE_SELTIMEOUT	2	/* timeout on target selection */
#define	SDSTATE_DISCONNECTED	3	/* target will reselect later */
#define	SDSTATE_COMPLETED	4	/* target sent COMMAND COMPLETE msg */
#define	SDSTATE_RETRY		5	/* SCSI bus reset, attempt retry */
#define	SDSTATE_TIMEOUT		6	/* reselection timed out */
#define	SDSTATE_ABORTED		7	/* target disconnected unexpectedly */
#define	SDSTATE_UNIMPLEMENTED	8	/* ctrlr can't do what device asked */
#define	SDSTATE_REJECTED	9	/* device level rejected request */
#define	SDSTATE_DROPPED		10	/* device dropped connection */

/*
 * conversions from bus_device bd_slave field to scsi target and lun
 * and vice versa
 */
#define	SCSI_TARGET(slave)	(((slave) >> 4) & 0x7)
#define	SCSI_LUN(slave)		((slave) & 0x7)
#define	SCSI_SLAVE(target, lun)	(((target) << 4) | (lun))

/*
 * global function declarations
 */
extern void scsi_init();
extern void scsi_probe(struct bus_ctrl *bcp, 
	struct scsi_ctrl *scp, 
	struct scsi_csw *scswp, 
	int ipl);
extern int scsi_slave(struct scsi_ctrl *scp, struct bus_device *bdp);
extern void scsi_attach(struct scsi_device *sdp);
extern int scsi_pollcmd(struct scsi_device *sdp);
extern scsi_dstart(struct scsi_device *sdp);
extern void scsi_cintr(struct scsi_ctrl *scp);
extern struct scsi_device *scsi_reselect(struct scsi_ctrl *scp, 
	u_char target, 
	u_char lun);
extern void scsi_restart(struct scsi_ctrl *scp);
extern int scsi_ioctl(struct scsi_device *sdp, 
	int cmd, 
	caddr_t data, 
	int flag);
extern void scsi_timeout(struct scsi_ctrl *scp);
extern void scsi_msg(struct scsi_device *sdp, 
	u_char opcode, 
	char *msg);
extern void scsi_sensemsg(struct scsi_device *sdp, struct esense_reply *erp);
extern int scsi_docmd(struct scsi_device *sdp);

/*
 * SCSI macros
 */
#define	SCSI_MACROS	1 

#ifdef SCSI_MACROS
#define	scsi_expectintr(scp, timeout)					\
	{								\
		extern hz;						\
		(scp)->sc_timer = (timeout) * hz;			\
		(scp)->sc_expectintr = 1;				\
	}

#define	scsi_gotintr(scp)						\
	{								\
		(scp)->sc_expectintr = 0;				\
	}

#define	scsi_expectreselect(sdp)					\
	{								\
		extern hz;						\
		(sdp)->sd_timer = (sdp)->sd_timeout * hz;		\
		(sdp)->sd_expectresel = 1;				\
	}

#define	scsi_gotreselect(sdp)						\
	{								\
		(sdp)->sd_expectresel = 0;				\
	}

/* ...removed 10-Feb-89; this must return an int for command reject detect.. 
#define	scsi_docmd(sdp)							\
	{								\
		struct scsi_ctrl *scp = (sdp)->sd_scp;			\
		extern int scsi_ndevices;				\
		ASSERT(curipl() >= (scp)->sc_ipl);			\
		ASSERT((sdp)->sd_active					\
		    && (struct scsi_device *)				\
		        queue_first(&(scp)->sc_bcp->bc_tab)		\
		    == sdp);						\
		if (scsi_ndevices == 1)					\
			(sdp)->sd_disconnectok = 0;			\
		(*(scp)->sc_scswp->scsw_start)(sdp);			\
	}
.......*/

#define	scsi_rejectcmd(sdp)						\
	{								\
		struct scsi_ctrl *scp = (sdp)->sd_scp;			\
		ASSERT(curipl() >= scp->sc_ipl);			\
		ASSERT((sdp)->sd_active					\
		    && (struct scsi_device *)				\
		       queue_first(&(scp)->sc_bcp->bc_tab)		\
		    == sdp);						\
		(sdp)->sd_state = SDSTATE_REJECTED;			\
		scsi_cintr(scp);					\
	}

#else !SCSI_MACROS
extern void scsi_expectintr(/* scp, timeout */);
extern void scsi_gotintr(/* scp */);
extern void scsi_expectreselect(/* sdp */);
extern void scsi_gotreselect(/* sdp */);
extern void scsi_rejectcmd(/* sdp */);
#endif !SCSI_MACROS

/*
 * register descriptions
 */
extern struct reg_values scsi_phase_values[];
extern struct reg_values scsi_sdstate_values[];
extern struct reg_values scsi_status_values[];
extern struct reg_values scsi_sensekey_values[];
extern struct reg_values scsi_cmd_values[];
extern struct reg_values scsi_msg_values[];
extern struct reg_values sr_iostat_values[];

/*
 * scsiconf information
 */
extern struct scsi_dsw *scsi_sdswlist[];


/* 
 * SCSI bus constants used only by driver (not exported) 
 */
 
/*
 * message codes
 */
#define	MSG_CMDCMPLT		0x00	/* to host: command complete */
#define	MSG_SAVEPTRS		0x02	/* to host: save data pointers */
#define	MSG_RESTOREPTRS		0x03	/* to host: restore pointers */
#define	MSG_DISCONNECT		0x04	/* to host: disconnect */
#define	MSG_IDETERR		0x05	/* to disk: initiator detected error */
#define	MSG_ABORT		0x06	/* to disk: abort op, go to bus free */
#define	MSG_MSGREJECT		0x07	/* both ways: last msg unimplemented */
#define	MSG_NOP			0x08	/* to disk: no-op message */
#define	MSG_MSGPARERR		0x09	/* to disk: parity error last message */
#define	MSG_LNKCMDCMPLT		0x0a	/* to host: linked command complete */
#define	MSG_LNKCMDCMPLTFLAG	0x0b	/* to host: flagged linked cmd cmplt */
#define	MSG_DEVICERESET		0x0c	/* to disk: reset and go to bus free */

#define	MSG_IDENTIFYMASK	0x80	/* both ways: thread identification */
#define	MSG_ID_DISCONN		0x40	/*	can disconnect/reconnect */
#define	MSG_ID_LUNMASK		0x07	/*	target LUN */

/*
 * opcode groups
 */
#define	SCSI_OPGROUP(opcode)	((opcode) & 0xe0)

#define	OPGROUP_0		0x00	/* six byte commands */
#define	OPGROUP_1		0x20	/* ten byte commands */
#define	OPGROUP_2		0x40	/* ten byte commands */
#define	OPGROUP_5		0xa0	/* twelve byte commands */
#define	OPGROUP_6		0xc0	/* six byte, vendor unique commands */
#define	OPGROUP_7		0xe0	/* ten byte, vendor unique commands */

/*
 * scsi bus phases
 */
#define	PHASE_DATAOUT		0x0
#define	PHASE_DATAIN		0x1
#define	PHASE_COMMAND		0x2
#define	PHASE_STATUS		0x3
#define	PHASE_MSGOUT		0x6
#define	PHASE_MSGIN		0x7

/*
 * 	scsi_req.sr_flags
 */
 
#define 	SRF_USERDONE	0x01

#ifdef	DEBUG
extern boolean_t sd_xpr_all;		/* misc. device level stuff */
extern boolean_t sd_xpr_cmd;		/* top-level device commands */
extern boolean_t sd_xpr_cntrl;		/* controller level */
extern boolean_t sd_xpr_scsi;		/* SCSI (middle) layer) */

#define	XPR_scsi(x)	XPR(XPR_SCSI, x)
#define XDBG(x)		if(sd_xpr_all) { \
				XPR_scsi(x); \
			}
#define XCDBG(x)	if(sd_xpr_all | sd_xpr_cmd) { \
				XPR_scsi(x); \
			}
#define XNDBG(x)	if(sd_xpr_cntrl) { \
				XPR_scsi(x); \
			}
#define XSDBG(x)	if(sd_xpr_scsi) { \
				XPR_scsi(x); \
			}
#else	DEBUG
#define XDBG(x)
#define XCDBG(x)
#define XNDBG(x)
#define XSDBG(x)
#endif	DEBUG

#endif _SCSIVAR_



