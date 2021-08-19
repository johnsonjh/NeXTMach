/*	@(#)sg.c	1.0	01/26/89	(c) 1989 NeXT	*/

/* 
 * sg.c -- implementation of generic scsi routines
 * KERNEL VERSION
 *
 * Supports general purpose SCSI I/O. Interface consists mainly of cdb's
 * passed via ioctl.
 *
 * HISTORY
 * 12-Jun-89	Doug Mitchell
 *	Checked SRF_BUSY flag on wakeup in sgcmd()
 * 26-Jan-89	Doug Mitchell (dmitch) at NeXT
 *	Created.
 *
 */ 
 
/* #define SG_OLAY_OK	0	/* OK to set target/lun = to sd disk */

#import <sg.h>
#if NSG > 0

#import <sys/errno.h>
#import <sys/types.h>
#import <sys/buf.h>
#import <sys/time.h>
#import <sys/param.h>
#import <sys/kernel.h>
#import <sys/conf.h>
#import <sys/proc.h>
#import <kern/task.h>
#import <sys/user.h>
#import <kern/xpr.h>
#import <kern/queue.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <next/printf.h>
#import <nextdev/busvar.h>
#import <nextdev/disk.h>
#import <nextdev/dma.h>
#import <nextdev/screg.h>			/* debug only */
#import <nextdev/scvar.h>			/* debug only */
#import <nextdev/scsireg.h>
#import <nextdev/scsivar.h>
#import <nextdev/sgvar.h>
#import <machine/spl.h>
#import <vm/vm_kern.h>

extern struct reg_values s5c_state_values[];	/* in sc.c */
extern struct reg_values sriost_values[];	/* in sg.c */
extern struct reg_values scsi_cmd_values[];	/* in scsi.c */
extern int scsi_ndevices;

#if	DEBUG || XPR_DEBUG

struct reg_values sgioc_values[] = {
/* value		name			*/
{ SGIOCSTL,		"set target/lun" 	},
{ SGIOCREQ,		"cmd request" 		},
{ SGIOCENAS,		"enable autosense" 	},
{ SGIOCDAS,		"disable autosense" 	},
{ SGIOCRST,		"reset SCSI bus " 	},
{ 0,			NULL			}
};

struct reg_values sriost_values[] = {
/* value		name					*/
{ SR_IOST_GOOD, 	"successful" 				},
{ SR_IOST_SELTO, 	"selection timeout" 			},
{ SR_IOST_CHKSV, 	"check status, sr_esense valid" 	},
{ SR_IOST_CHKSNV, 	"check status, sr_esense not valid" 	},
{ SR_IOST_DMAOR, 	"sr_dma_max byte count exceeded" 	},
{ SR_IOST_IOTO, 	"sr_ioto exceeded" 			},
{ SR_IOST_BV, 		"SCSI Bus violation" 			},
{ SR_IOST_CMDREJ, 	"command reject (by driver)" 		},
{ SR_IOST_MEMALL, 	"memory allocation failure" 		},
{ SR_IOST_MEMF, 	"memory fault" 				},
{ SR_IOST_PERM, 	"not super user" 			},
{ SR_IOST_NOPEN, 	"device not open" 			},
{ SR_IOST_TABT, 	"target aborted command" 		},
{ ST_IOST_BADST, 	"bad SCSI status (other than check)" 	},
{ ST_IOST_INT, 		"internal driver error" 		},
{ SR_IOST_BCOUNT,	"unexpected byte count"			},
{ SR_IOST_VOLNA,	"desired volume not available"		},
{ SR_IOST_WP,		"Media Write Protected"			},
{ 0,			NULL					}
};

#endif 	DEBUG || XPR_DEBUG

/*
 * global function declarations accessed via [cb]devsw
 */
int sgopen(dev_t dev);
int sgclose(dev_t dev);
int sgioctl(dev_t dev, int cmd, caddr_t data, int flag);

/*
 * static function declarations accessed externally via sg_sdsw
 */
static struct scsi_device *sggetsdp(int device);
static int sgslave(struct scsi_device *sdp, struct bus_device *bdp);
static void sgattach(struct scsi_device *sdp);
static sgstart(struct scsi_device *sdp);
static void sgintr(struct scsi_device *sdp);

/*
 * Generic scsi level calls device level via this transfer vector
 */
struct scsi_dsw sg_sdsw = {
	"sg",
	0,
	0,
	sggetsdp,
	sgslave,
	sgattach,
	sgstart,
	sgintr
};

/*
 * static function declarations accessed only internally
 */
static void sginit(struct scsi_device *sdp);
static void sgattach2(struct scsi_device *sdp);
static void sgwake(struct scsi_generic_device *sgdp);
static int sgcmd(struct scsi_generic_device *sgdp, struct scsi_req *sr);
static void sgdone(struct scsi_generic_device *sgdp, long resid, int io_status);
static int sgsettl(struct scsi_generic_device *sgdp, struct scsi_adr *sadp);

static struct scsi_device *
sggetsdp(int device)
{
	return(&sg_sd[device]);
}

static int
sgslave(struct scsi_device *sdp, struct bus_device *bdp)
{
	XDBG(("sgslave: unit = %d\n",sdp->sd_bdp->bd_unit));
	sginit(sdp);	/* initialize our data structures */
	printf("Generic SCSI Device as ");
	
	/* we do NOT reserve this lun as ours; target and lun may change
	 * via ioctl...
	 */
	sdp->sd_devtype = 'g';
	bdp->bd_slave = SCSI_SLAVE(7, 7);
	return(1);
	
} /* sgslave() */

static void
sgattach(struct scsi_device *sdp)
{
	sgattach2(sdp);
	scsi_ndevices--;		/* not really a device yet... we'll
					 * increment this in sgopen() */
}

static void
sgattach2(struct scsi_device *sdp)
{
	struct scsi_generic_device *sgdp = &sg_sgd[sdp->sd_bdp->bd_unit];
	extern hz;

	XDBG(("sgattach2: unit = %d\n",sdp->sd_bdp->bd_unit));
	sginit(sdp);
} /* sgattach2() */

static void
sgwake(struct scsi_generic_device *sgdp)
{
	wakeup(sgdp);
}


static void
sginit(struct scsi_device *sdp)
{
	struct scsi_generic_device *sgdp = &sg_sgd[sdp->sd_bdp->bd_unit];

	bzero(sgdp, sizeof(struct scsi_generic_device));
	sgdp->sgd_sdp = sdp;
	sgdp->sgd_erp = DMA_ALIGN(struct esense_reply *, &sgdp->sgd_erbuf);
	queue_init(&sgdp->sgd_io_q);
	sgdp->sgd_state = SGDSTATE_STARTINGCMD;
	sgdp->sgd_autosense = 0;
	sgdp->sgd_sdp->sd_target = -1;
	sgdp->sgd_sdp->sd_lun    = -1;
	return;
}

int
sgopen(dev_t dev)
{
	int unit = SG_UNIT(dev); 
	struct scsi_generic_device *sgdp = &sg_sgd[unit];

	XDBG(("sgopen: dev = %d unit = %d\n",dev,unit));
	if (unit >= NSG || 			/* illegal device */
	    sgdp->sgd_sdp == NULL ||		/* hasn't been init'd */
	    sgdp->sgd_open)			/* already open */
		return(ENXIO);
	sgattach2(sgdp->sgd_sdp);
	sgdp->sgd_open = 1;
	scsi_ndevices++;
	return(0);
}

int
sgclose(dev_t dev)
{
	int unit = SG_UNIT(dev); 
	struct scsi_generic_device *sgdp = &sg_sgd[unit];

	XDBG(("sgclose: dev = %d unit = %d\n",dev,unit));
	if (unit >= NSG || 			/* illegal device */
	    !sgdp->sgd_open)			/* not open */
		return(ENXIO);
	sgdp->sgd_open = 0;
	sgdp->sgd_sdp->sd_target = -1;
	sgdp->sgd_sdp->sd_lun = -1;
	scsi_ndevices--;			/* this device essentially
						 * disappears */
	return(0);
}


static int
sgstart(struct scsi_device *sdp)
{
	struct scsi_generic_device *sgdp = &sg_sgd[sdp->sd_bdp->bd_unit];
	struct scsi_req *srp;
	int s;
	
	/*
	 * The job here is to be the first half of the device's
	 * request processing fsm (sgintr is the second half).
	 *
	 * This routine fills in the info required in *sdp, which is 
	 * obtained from the head of sgdp->sgd_io_q and then asks
	 * the controller to execute it. Note that the request stays
	 * at the head of sgd_io_q until it is passesd back to the user
	 * (in sgdone()).
	 *
	 * Before exiting, the device level start routine must
	 * either call scsi_docmd() to initiate device activity
	 * or call scsi_rejectcmd() to indicate that the buffer
	 * can no longer be processed.  Scsi_rejectcmd will mark
	 * the device inactive and call sgintr() to allow error
	 * processing.
	 *
	 * MUST BE CALLED AT SCSI IPL.
	 */
	 
	XDBG(("sgstart: top: s5c_state = %n\n",sc_s5c[0].s5c_state,
		s5c_state_values));
	ASSERT(curipl() >= sdp->sd_scp->sc_ipl);
	if(queue_empty(&sgdp->sgd_io_q))
		panic("sgstart() called with empty sgd_io_q");
	srp = (struct scsi_req *)queue_first(&sgdp->sgd_io_q);
	if (sgdp->sgd_state == SGDSTATE_STARTINGCMD) 
		sgdp->sgd_state = SGDSTATE_DOINGCMD;
	
	if(srp->sr_cdb.cdb_c6.c6_lun != sgdp->sgd_sdp->sd_lun)
		goto err;
		
	/* fill in *sdp fields required from us */
	
	if (sgdp->sgd_state != SGDSTATE_GETTINGSENSE) {
		sdp->sd_disconnectok = 1;	
		sdp->sd_read         = (srp->sr_dma_dir == SR_DMA_RD) ? 1 : 0;		
		sdp->sd_cdb 	     = srp->sr_cdb;
		sdp->sd_addr 	     = srp->sr_addr;
		sdp->sd_bcount	     = srp->sr_dma_max;
		sdp->sd_padcount     = 0xFFFF;		/* absorb excess */	
		sdp->sd_pmap         = pmap_kernel();
		sdp->sd_timeout      = srp->sr_ioto;	
	}
	
	/*
	 * have the controller send the command to the target
	 */
	return(scsi_docmd(sdp));
err:
	srp->sr_io_status = SR_IOST_CMDREJ;
	scsi_rejectcmd(sdp);
	return(-1);
} /* sgstart() */

static void
sgintr(struct scsi_device *sdp)
{
	struct scsi_generic_device *sgdp = &sg_sgd[sdp->sd_bdp->bd_unit];
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	int device;

	/*
	 * We're called when the controller can no longer process
	 * the command indicated in sdp.  Our job is to
	 * advance the driver state machine and deal with any special
	 * recovery and/or error processing.
	 *
	 * Before returning, this module should either call
	 * scsi_dstart() if there's more to do to accomplish or
	 * recover this transfer; or if we're done or giving up,
	 * call sgdone() possibly indicating error.  Sgdone() will
	 * biodone() the buffer and advance the queue.
	 */
	 
	XDBG(("sgintr: sd_state = %n\n",sdp->sd_state,scsi_sdstate_values));
	XDBG(("        target %d   lun %d\n",sdp->sd_target,sdp->sd_lun));

	switch (sgdp->sgd_state) {
	case SGDSTATE_DOINGCMD:

		switch (sdp->sd_state) {
		case SDSTATE_REJECTED:		/* we wouldn't do it */
		case SDSTATE_UNIMPLEMENTED:	/* ctrlr couldn't do it */
			scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "ERROR");
			sgdone(sgdp, sdp->sd_resid, SR_IOST_CMDREJ);
			break;
		case SDSTATE_SELTIMEOUT:	/* select timed out */
			sgdone(sgdp, sdp->sd_resid, SR_IOST_SELTO);
			break;
		case SDSTATE_TIMEOUT:		/* target never reselected */
			sgdone(sgdp, sdp->sd_resid, SR_IOST_IOTO);
			break;
		case SDSTATE_ABORTED:		/* ctrlr aborted cmd */
			sgdone(sgdp, sdp->sd_resid, SR_IOST_TABT);
			break;
		case SDSTATE_DROPPED:		/* target dropped connection */
			sgdone(sgdp, sdp->sd_resid, SR_IOST_BV);
			break;
		case SDSTATE_RETRY:		/* SCSI bus reset, retry */
			scsi_dstart(sdp);
			break;
		case SDSTATE_COMPLETED:
			switch (sdp->sd_status & STAT_MASK) {
			case STAT_GOOD:
				sgdone(sgdp, sdp->sd_resid, SR_IOST_GOOD);
				break;
			case STAT_CHECK:
				if(sgdp->sgd_autosense) {
				
				    /* do a REQUEST SENSE to find out why */
				    sgdp->sgd_state = SGDSTATE_GETTINGSENSE;
				    sgdp->sgd_savedresid = sdp->sd_resid;
				    sgdp->sgd_savedop = 
				    	sdp->sd_cdb.cdb_opcode;
				    c6p->c6_opcode = C6OP_REQSENSE;
				    c6p->c6_lun = sdp->sd_lun;
				    c6p->c6_lba = 0;
				    c6p->c6_len = sizeof(*sgdp->sgd_erp);
				    c6p->c6_ctrl = CTRL_NOLINK;
				    sdp->sd_disconnectok = 0;
				    sdp->sd_read = 1;
				    sdp->sd_addr = (caddr_t)sgdp->sgd_erp;
				    sdp->sd_bcount = sizeof(*sgdp->sgd_erp);
				    sdp->sd_padcount = 0;
				    sdp->sd_pmap = pmap_kernel();
				    sdp->sd_timeout = SG_IOTO_SENSE;
				    scsi_dstart(sdp);
				}
				else {
				    sgdone(sgdp, sdp->sd_resid, 
				    	   SR_IOST_CHKSNV);
				}
				break;
			default:
				sgdone(sgdp, sdp->sd_resid, ST_IOST_BADST);
				break;
			}
			break;
		default:
			panic("sgintr: bad sd_state");
		}
		break;
	case SGDSTATE_GETTINGSENSE:
		switch (sdp->sd_state) {
		case SDSTATE_SELTIMEOUT:	/* select timed out */
			sgdone(sgdp, sgdp->sgd_savedresid, SR_IOST_SELTO);
			break;
		case SDSTATE_RETRY:		/* SCSI bus reset */
		case SDSTATE_TIMEOUT:		/* target never reselected */
		case SDSTATE_ABORTED:		/* ctrlr aborted cmd */
		case SDSTATE_DROPPED:		/* target dropped connection */
		case SDSTATE_REJECTED:		/* we wouldn't do it */
		case SDSTATE_UNIMPLEMENTED:	/* ctrlr couldn't do it */
			scsi_msg(sdp, sgdp->sgd_savedop, "ERROR");
			sgdone(sgdp, sgdp->sgd_savedresid, SR_IOST_CHKSNV);
			break;
		case SDSTATE_COMPLETED:
			switch (sdp->sd_status & STAT_MASK) {
			case STAT_GOOD:
				sgdone(sgdp, sgdp->sgd_savedresid, 
					SR_IOST_CHKSV);
				break;
			default:
				scsi_msg(sdp, sgdp->sgd_savedop, "ERROR");
				sgdone(sgdp, sgdp->sgd_savedresid, 
					SR_IOST_CHKSNV);
				break;
			}
			break;
		default:
			scsi_msg(sdp, sgdp->sgd_savedop, "PANIC");
			panic("sdintr: bad sd_state");
		}
		break;
	default:
		scsi_msg(sdp,sdp->sd_cdb.cdb_opcode, "PANIC");
		panic("sdintr: bad sgd_state");
	}
} /* sgintr() */

static void
sgdone(struct scsi_generic_device *sgdp, 
       long resid, 
       int io_status)
{
	struct scsi_req *srp;

	/*
	 * The current request (at the head of sgdp->sgd_io_q) has either
	 * succeeded or we've given up trying to make it work.
	 * Advance sgd_io_q and do a biodone().
	 */
	XDBG(("sgdone: resid = %d   io_status = %n\n",resid,
	       io_status,sriost_values));
	if (queue_empty(&sgdp->sgd_io_q))
		panic("sgdone: no buf on sgd_io_q");
	sgdp->sgd_state = SGDSTATE_STARTINGCMD;
	srp = (struct scsi_req *)queue_next(&sgdp->sgd_io_q);
	queue_remove(&sgdp->sgd_io_q, srp, struct scsi_req *, sr_io_q);
	srp->sr_dma_xfr = srp->sr_dma_max - resid;
	srp->sr_io_status = io_status;
	if(io_status == SR_IOST_CHKSV)
		srp->sr_scsi_status = STAT_CHECK;
	else
		srp->sr_scsi_status = sgdp->sgd_sdp->sd_status;	
	if (!queue_empty(&sgdp->sgd_io_q)) {
		XDBG(("sgdone - restarting via scsi_dstart()\n"));
		scsi_dstart(sgdp->sgd_sdp);
	}
	else {
		srp->sr_flags |= SRF_USERDONE;
		wakeup(srp);	
	}
	XDBG(("        return from wakeup(); srp = 0x%x\n",srp));
} /* sgdone() */

/* #define SG_DMA_DIS 1 */

#ifdef 	SG_DMA_DIS
int sg_dma_block=60000;		/* TEMPORARY for printer test */
#endif	SD_DMA_DIS

int
sgioctl(dev_t dev, 
	int cmd, 		/* SGIOCREQ, etc */
	caddr_t data, 		/* actually a ptr to scsi_req, if used */
	int flag)		/* for historical reasons. Not used. */
{
	struct scsi_generic_device *sgdp = &sg_sgd[SG_UNIT(dev)];
	struct scsi_req *srp = (struct scsi_req *)data;
	int error = 0;
	int s;

	XDBG(("sgioctl: dev = %d   cmd = %n\n",dev,
	       cmd,sgioc_values));
	if(!sgdp->sgd_open) {
		srp->sr_io_status = SR_IOST_NOPEN;
		return(EACCES);
	}
	switch (cmd) {
	case SGIOCSTL:				/* set target/lun */
		error = sgsettl(sgdp,(struct scsi_adr *)data);
		break;
	case SGIOCREQ:
		error = sgcmd(sgdp,srp);
		break;
	case SGIOCENAS:				/* enable auto sense */
		sgdp->sgd_autosense = 1;
		break;
	case SGIOCDAS: 				/* disable auto sense */
		sgdp->sgd_autosense = 0;
#ifdef	SG_DMA_DIS
		/* TEMPORARY: force sg_dma_block us delay for printer test */
		s = spldma();
		DELAY(sg_dma_block);
		splx(s);
#endif	SG_DMA_DIS
		break;
	case SGIOCRST:
		if (!suser()) {			/* this is too dangerous for
						 * the hoi polloi */
			error = u.u_error;
			break;
		}
		s = spln(ipltospl(sgdp->sgd_sdp->sd_scp->sc_ipl));
		error = scsi_ioctl(sgdp->sgd_sdp, DKIOCRESET, (void *)0, 0);
		splx(s);
		break;
	default:
		return(EINVAL);			/* invalid argument */
	}
	return(error);
} /* sgioctl() */

static int 
sgsettl(struct scsi_generic_device *sgdp,
	struct scsi_adr *sadp)	
{
	XDBG(("sgsettl: target %d  lun %d\n",sadp->sa_target,sadp->sa_lun));
#ifndef	SG_OLAY_OK
	/* verify target/lun not already in use. Super user can do this. */
	if (sgdp->sgd_sdp->sd_scp->sc_lunmask[sadp->sa_target] & 
	   (1 << sadp->sa_lun)) {
	   	if(!suser())
			return(EACCES);
	}
#endif	SG_OLAY_OK
	sgdp->sgd_sdp->sd_target = sadp->sa_target;
	sgdp->sgd_sdp->sd_lun    = sadp->sa_lun;
	return(0);
}

static int
sgcmd(struct scsi_generic_device *sgdp, 
      struct scsi_req *srp)
{
	caddr_t ap;
	char *user_addr;
	int rtn;
	struct timeval start, stop;
	int s;
	struct scsi_device *sdp=sgdp->sgd_sdp;
	
	XDBG(("sgcmd\n"));
	XCDBG(("sgcmd: op = %n target = %d  lun = %d\n",
		srp->sr_cdb.cdb_opcode,scsi_cmd_values,
		sdp->sd_target,sdp->sd_lun));
	if(sgdp->sgd_sdp->sd_target == 0xff) {
		srp->sr_io_status = SR_IOST_NOPEN;
		return(EACCES);
	}	
	microtime(&start);
	
	/* get some memory in which to do user's DMA. If we are 
	 * going to write to device, copy user's data here.
	 */
	
	if(srp->sr_dma_max != 0) {
		/* be sure to kmem_free() if error exit is taken...*/
		if ((ap = (caddr_t)kmem_alloc(kernel_map,
					      srp->sr_dma_max)) == 0) {
			XDBG(("        ...kmem_alloc() failed\n"));
			srp->sr_io_status = SR_IOST_MEMALL;
			return(ENOMEM);
		}
		if(srp->sr_dma_dir == SR_DMA_WR) {
			if (rtn = copyin(srp->sr_addr, ap,
					   srp->sr_dma_max)) {
				XDBG(("        ...copyin() returned %d\n",
					rtn));
				kmem_free(kernel_map, ap, srp->sr_dma_max);
				srp->sr_io_status = SR_IOST_MEMF;
				return(rtn);
			}
		}
	}
	else
		ap = 0;		/* for alignment checks */
	user_addr = srp->sr_addr;	/* switch address spaces... */
	srp->sr_addr = ap;
		
	/*
	 * add request to per-device queue
	 */
	 
	s = spln(ipltospl(sdp->sd_scp->sc_ipl));
	queue_enter(&sgdp->sgd_io_q, srp, struct scsi_req *, sr_io_q);
	rtn = 0;
	srp->sr_flags &= ~SRF_USERDONE;
	if (sdp->sd_active == 0) {
		XDBG(("sgcmd: startup via scsi_dstart()\n"));
		rtn = scsi_dstart(sdp);
	}
	if(rtn == 0) {
		/* don't bother sleeping if scsi_dstart() failed */
		XDBG(("      calling sleep(); srp = 0x%x\n",srp));
		while(!(srp->sr_flags & SRF_USERDONE)) {
			sleep(srp,PRIBIO);
#ifdef	DEBUG
			if(!(srp->sr_flags & SRF_USERDONE)) {
			    XDBG(("sgcmd - wakeup, sr still busy\n"));
			}
#endif	DEBUG
		}
	}
	splx(s);
	srp->sr_addr = user_addr;
	
	/* 
	 * I/O complete. Copy DMA buffer back to user space if appropriate 
	 */
	 
	if ((srp->sr_dma_xfr != 0) &&
	    (srp->sr_dma_dir == SR_DMA_RD)) {
		if(rtn = copyout(ap, user_addr, srp->sr_dma_xfr)) {
			XDBG(("        ...copyout() returned %d\n",rtn));
			kmem_free(kernel_map, ap, srp->sr_dma_max);
			srp->sr_io_status = SR_IOST_MEMF;
			return(rtn);
		}
	}
	if (srp->sr_dma_max != 0)
		kmem_free(kernel_map, ap, srp->sr_dma_max);
		
	/* 
	 * copy autosense data to *srp if valid 
	 */
	 
	if(srp->sr_io_status == SR_IOST_CHKSV);
		srp->sr_esense = *sgdp->sgd_erp;
	
	microtime(&stop);
	timevalsub(&stop, &start);
	srp->sr_exec_time = stop;
	XCDBG(("sgcmd: io_status = %n\n",srp->sr_io_status,sriost_values));
	return(0);
	
} /* sgcmd() */

#endif	NSG

/* end of sg.c */



