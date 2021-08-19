/*	@(#)st.c	1.0	02/22/89	(c) 1989 NeXT	*/

/* 
 * st.c -- implementation of scsi tape driver routines
 * KERNEL VERSION
 *
 * HISTORY
 * 27-Mar-90	Doug Mitchell
 *	Added mt_type support in MTIOCGET ioctl.
 * 16-Jun-89	Doug Mitchell
 *	Added MTIOCSRQ ioctl
 *	MTIOCFIXBLK now passes blocksize
 * 12-Jun-89	Doug Mitchell
 *	Checked SRF_BUSY flag on wakeup in stcmd()
 *	Do 2 kmem_alloc's instead of one in st_rw
 * 10-Jun-89	Doug Mitchell
 *	NULL'd stdp->std_sdp if stslave() fails
 * 10-Apr-89	Doug Mitchell
 *	Made open access single threaded via STF_OPEN flag
 * 06-Apr-89	Doug Mitchell
 *	Added auto rewind device, unload command
 * 22-Feb-89	Doug Mitchell (dmitch) at NeXT
 *	Created.
 *
 */ 
 
/*
 * Four different devices are implemented here:
 *
 *	rst - generic SCSI tape, rewind on close
 *	nrst - generic SCSI tape, no rewind on close 
 *	rxt - Exabyte SCSI tape, rewind on close
 *	nrxt - Exabyte SCSI tape, no rewind on close
 *
 *	All 4 devices have the same major number. Bit 0 of the minor number 
 *	selects "rewind on close" (0) or "no rewind" (1). Bit 1 of the 
 *	minor number select generic (0) or Exabyte (1).
 *
 *	The Exabyte drive currently requires these actions on open:
 *
 *		-- enable Buffered Write mode
 *		-- Inhibit Illegal Length errors
 *		-- Disable Disconnect During Data Transfer
 */	

#import <st.h>
#if NST > 0

#import <sys/errno.h>
#import <sys/types.h>
#import <sys/buf.h>
#import <sys/time.h>
#import <sys/param.h>
#import <sys/kernel.h>
#import <sys/conf.h>
#import <sys/proc.h>
#import <kern/task.h>
#import <sys/uio.h>
#import <sys/user.h>
#import <kern/xpr.h>
#import <kern/queue.h>
#import <sys/mtio.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <next/printf.h>
#import <nextdev/busvar.h>
#import <nextdev/disk.h>
#import <nextdev/dma.h>
#import <nextdev/screg.h>
#import <nextdev/scvar.h>
#import <nextdev/scsireg.h>
#import <nextdev/scsivar.h>
#import <nextdev/stvar.h>
#import <machine/spl.h>
#import <vm/vm_kern.h>

#define USE_EBD	1		/* use "even byte diconnect" rather than 
				 * "no disconnect during data xfer" for exabyte
				 */
				 
extern struct reg_values s5c_state_values[];	/* in sc.c */
extern struct reg_values sriost_values[];	/* in sg.c */
extern struct reg_values scsi_cmd_values[];	/* in scsi.c */

#if	DEBUG || XPR_DEBUG

struct reg_values stioc_values[] = {
/* value		name			*/
{ MTIOCTOP,		"mag tape op" 		},
{ MTIOCGET,		"get status" 		},
{ MTIOCIEOT,		"ignore EOT error" 	},
{ MTIOCIEOT,		"enable EOT error" 	},
{ MTIOCFIXBLK,		"set fixed block mode" 	},
{ MTIOCFIXBLK,		"set variable block mode"},
{ MTIOCMODSEL,		"Mode Select"		},
{ MTIOCMODSEN,		"Mode Sense"		},
{ MTIOCINILL,		"Inhibit ILI Errors"	},
{ MTIOCALILL,		"Allow ILI Errors"	},
{ 0,			NULL			}
};

struct reg_values mtop_values[] = {
/* value		name			*/
{ MTWEOF,		"write FM" 			},
{ MTFSF,		"forward space file" 		},
{ MTBSF,		"backward space file" 		},
{ MTFSR,		"forward space record" 		},
{ MTBSR,		"backward space record" 	},
{ MTREW,		"rewind" 			},
{ MTOFFL,		"rewind/offline" 		},
{ MTNOP,		"no operation" 			},
{ MTCACHE,		"enable controller cache" 	},
{ MTNOCACHE,		"disable controller cache" 	},
{ MTRETEN,		"retension" 			},
{ MTERASE,		"erase" 			},
{ 0,			NULL				}
};

#endif 	DEBUG || XPR_DEBUG

/*
 * global function declarations accessed via [cb]devsw
 */
int stopen(dev_t dev);
int stclose(dev_t dev);		/* ??? */
int stread(dev_t dev, struct uio *uiop);
int stwrite(dev_t dev, struct uio *uiop);
int stioctl(dev_t dev, int cmd, caddr_t data, int flag);

/*
 * static function declarations accessed externally via st_sdsw
 */
static struct scsi_device *stgetsdp(int device);
static int stslave(struct scsi_device *sdp, struct bus_device *bdp);
static void stattach(struct scsi_device *sdp);
static int ststart(struct scsi_device *sdp);
static void stintr(struct scsi_device *sdp);

/*
 *  scsi level calls device level via this transfer vector
 */
struct scsi_dsw st_sdsw = {
	"st",
	0,
	0,
	stgetsdp,
	stslave,
	stattach,
	ststart,
	stintr
};

/*
 * static function declarations accessed only internally
 */
static void stinit(struct scsi_device *sdp);
static void stattach2(struct scsi_device *sdp);
static void stwake(struct scsi_tape_device *stdp);
static int stinquiry(struct scsi_tape_device *stdp, int polling);
static int sttestrdy(struct scsi_tape_device *stdp, int polling);
static int stclosefile(struct scsi_tape_device *stdp);
static int strewind(struct scsi_tape_device *stdp);
static int streqsense(struct scsi_tape_device *stdp, int polling);
static int stmodesel(struct scsi_tape_device *stdp, 
	struct modesel_parms *mspp, 
	int polling);
static int stmodesen(struct scsi_tape_device *stdp, 
	struct modesel_parms *mspp, 
	int polling);
static int stcmd_int(struct scsi_tape_device *stdp, 
	struct scsi_req *srp, 
	int polling);
static int stcmd(struct scsi_tape_device *stdp, 
	struct scsi_req *srp, 
	int polling);
static void stdone(struct scsi_tape_device *stdp, 
	long resid, 
	int io_status);
static int st_mtop(dev_t dev, struct mtop *mtopp);
static int st_fixed(struct scsi_tape_device *stdp, 
	int blocksize, 
	int polling);

static struct scsi_device *
stgetsdp(int device)
{
	return(&st_sd[device]);
}

static int
stslave(struct scsi_device *sdp, struct bus_device *bdp)
{
	struct scsi_tape_device *stdp = &st_std[sdp->sd_bdp->bd_unit];
	struct inquiry_reply *irp;
	int error;
	char *cp;

	XDBG(("stslave: target = %d lun = %d\n",sdp->sd_target,sdp->sd_lun));
	stinit(sdp);	/* initialize so we can run a command */

	/*  kludge for dumb sun controller - the tur command is merely
	 * to clear unit attention. Success is irrelevent.
	 */
	 
	sttestrdy(stdp,ST_WAIT);
	/* Do an INQUIRY cmd */
	error = stinquiry(stdp, ST_WAIT);

	/*
	 * if command wasn't successful or it's not a tape out there,
	 * slave fails
	 */
	irp = stdp->std_irp;
	if (error || irp->ir_devicetype != DEVTYPE_TAPE) {
		stdp->std_sdp = NULL;		/* indicates no device */
		return(0);
	}
	/* Otherwise, we like it; so tell a little about it */
	sdp->sd_devtype = 't';
	for (cp=irp->ir_revision-1; cp >= irp->ir_vendorid && *cp == ' '; cp--)
		continue;
	*++cp = '\0';
	if (cp != irp->ir_vendorid)
		printf("%s as ", irp->ir_vendorid);
		
	/* for now, set default of variable block size */
	st_fixed(stdp, 0, ST_WAIT);
	
	/* reserve this lun as ours */
	sdp->sd_scp->sc_lunmask[sdp->sd_target] |= 1 << sdp->sd_lun;
	bdp->bd_slave = SCSI_SLAVE(sdp->sd_target, sdp->sd_lun);
	
	/* one more to clear attention - Exabyte */
	sttestrdy(stdp,ST_WAIT);
	return(1);
	
} /* stslave() */

static void
stattach(struct scsi_device *sdp)
{
	stattach2(sdp);
}

static void
stattach2(struct scsi_device *sdp)
{
	struct scsi_tape_device *stdp = &st_std[sdp->sd_bdp->bd_unit];

	/* TODO: get status - see if tape loaded */
	XDBG(("stattach2: target = %d  unit = %d\n",
		sdp->sd_target,sdp->sd_lun));
	/* stinit(sdp); */
} /* stattach2() */

static void
stwake(struct scsi_tape_device *stdp)
{
	wakeup(stdp);
}


static void
stinit(struct scsi_device *sdp)
{
	struct scsi_tape_device *stdp = &st_std[sdp->sd_bdp->bd_unit];

	bzero(stdp, sizeof(struct scsi_tape_device));
	stdp->std_sdp = sdp;
	stdp->std_erp = DMA_ALIGN(struct esense_reply *, &stdp->std_erbuf);
	stdp->std_irp = DMA_ALIGN(struct inquiry_reply *, &stdp->std_irbuf);
	stdp->std_crp = DMA_ALIGN(struct capacity_reply *, &stdp->std_crbuf);
	stdp->std_mspp = DMA_ALIGN(struct modesel_parms *, &stdp->std_msbuf);
	stdp->std_flags = 0;
	queue_init(&stdp->std_io_q);
	stdp->std_state = STDSTATE_STARTINGCMD;
	return;
}

int
stopen(dev_t dev)
{
	int unit = ST_UNIT(dev); 
	struct scsi_tape_device *stdp = &st_std[unit];
	struct modesel_parms *mspp=stdp->std_mspp;

	XDBG(("stopen: dev = %d unit = %d\n",dev,unit));
	if(stdp->std_flags & STF_OPEN)
		return(EBUSY);			/* already open */
	if ((unit >= NST) || 			/* illegal device */
	    (stdp->std_sdp == NULL )) 		/* hasn't been init'd */
		return(ENXIO);			/* FIXME - try to init here */
	stattach2(stdp->std_sdp);

	/* we allow this twice in case of unit attention */
	if(sttestrdy(stdp,ST_WAIT))
		if(sttestrdy(stdp,ST_WAIT))
			return(EIO);
	
	if(ST_EXABYTE(dev)) {
		struct exabyte_vudata *evudp = 
			(struct exabyte_vudata *)&mspp-> msp_data.msd_vudata;
		struct mode_sel_hdr *mshp;
	  
		/* Exabyte "custom" setup */
		if(st_fixed(stdp, 0, ST_INTERRUPT))
			return(EIO);
		stdp->std_flags |= STF_SIL;	/* inhibit illegal length */
		
		/* do a mode sense */
		mspp->msp_bcount = sizeof(struct mode_sel_hdr) + 
			           sizeof(struct mode_sel_bd) + 
			           MSP_VU_EXABYTE;
		if(stmodesen(stdp, stdp->std_mspp, ST_INTERRUPT))
			return(EIO);
			
		/* some fields we have to zero as a matter of course */	
		mshp = &mspp->msp_data.msd_header;	
		mshp->msh_sd_length_0 = 0;
		mshp->msh_med_type  = 0;
		mshp->msh_wp        = 0;
		mshp->msh_bd_length = sizeof(struct mode_sel_bd);
		mspp->msp_data.msd_blockdescript.msbd_blocklength = 0;
		mspp->msp_data.msd_blockdescript.msbd_numblocks = 0;
		
		/* set up buffered mode, #blocks = 0, even byte disconnect,
		 * enable parity; do mode selsect
		 */
		mspp->msp_data.msd_header.msh_bufmode = 1;
#ifdef	USE_EBD
		/* clear NDD and set EBD; enable parity  */
		evudp->nd = 0;		/* disconnects OK */
		evudp->ebd = 1;		/* but only on word boundaries */
		evudp->pe = 1;		/* parity enabled */
		evudp->nbe = 1;		/* Busy status disabled */
#else	USE_EBD
		evudp->nd = 1;
#endif	USE_EBD
		if(stmodesel(stdp, stdp->std_mspp, ST_INTERRUPT))
			return(EIO);
	}
	stdp->std_flags |= STF_OPEN;
	return(0);
}

int
stclose(dev_t dev)
{
	int unit = ST_UNIT(dev); 
	struct scsi_tape_device *stdp = &st_std[unit];
	int rtn = 0;
	
	XDBG(("stclose: dev = %d unit = %d\n",dev,unit));
	if(stdp->std_flags & STF_WRITE) 
	
		/* we must write a file mark to close the file */
		
		rtn = stclosefile(stdp);
	if(ST_RETURN(dev) == 0)		/* returning device? */
		rtn = strewind(stdp);
	stdp->std_flags &= ~STF_OPEN;
	return(rtn);
}

static int
stinquiry(struct scsi_tape_device *stdp, int polling)
{
	struct scsi_req sr;
	struct cdb_6 *cdbp;
	struct inquiry_reply *irp;

	XDBG(("stinquiry: target = %d  unit = %d\n",
		stdp->std_sdp->sd_target,stdp->std_sdp->sd_bdp->bd_unit));
	bzero(&sr,sizeof(sr));
	irp             = stdp->std_irp;    /* destination of inquiry data */
	cdbp            = (struct cdb_6 *)&sr.sr_cdb;
	cdbp->c6_opcode = C6OP_INQUIRY;
	cdbp->c6_lun    = stdp->std_sdp->sd_lun;	
	cdbp->c6_len    = sizeof(*irp);
	sr.sr_addr      = (caddr_t)irp;
	sr.sr_dma_max   = sizeof(*irp);
	sr.sr_dma_dir   = SR_DMA_RD;
	sr.sr_ioto      = ST_IOTO_NORM;
	return(stcmd_int(stdp, &sr, polling));
} /* stinquiry() */

static int
sttestrdy(struct scsi_tape_device *stdp, int polling)
{
	struct scsi_req sr;
	struct cdb_6 *cdbp;

	XDBG(("sttestrdy: target = %d  unit = %d\n",
		stdp->std_sdp->sd_target,stdp->std_sdp->sd_bdp->bd_unit));
	bzero(&sr,sizeof(sr));
	cdbp             = (struct cdb_6 *)&sr.sr_cdb;
	cdbp->c6_opcode  = C6OP_TESTRDY;
	cdbp->c6_lun     = stdp->std_sdp->sd_lun;	
	sr.sr_ioto       = ST_IOTO_NORM;
	sr.sr_addr       = 0;
	sr.sr_dma_xfr	 = 0;
	return(stcmd_int(stdp, &sr, polling));
} /* sttestrdy() */

static int
stclosefile(struct scsi_tape_device *stdp)
{
	struct scsi_req sr;
	struct cdb_6s *cdbp;
	
	XDBG(("stclosefile: target = %d  unit = %d\n",
		stdp->std_sdp->sd_target,stdp->std_sdp->sd_bdp->bd_unit));
	
	bzero(&sr,sizeof(sr));
	cdbp             = (struct cdb_6s *)&sr.sr_cdb;
	cdbp->c6s_opcode = C6OP_WRTFM;
	cdbp->c6s_lun    = stdp->std_sdp->sd_lun;	
	cdbp->c6s_len    = 1;				/* one file mark */
	sr.sr_ioto       = ST_IOTO_NORM;
	return(stcmd_int(stdp, &sr, ST_INTERRUPT));
} /* stclosefile() */

static int
strewind(struct scsi_tape_device *stdp)
{
	struct scsi_req sr;
	struct cdb_6s *cdbp;

	XDBG(("strewind: target = %d  unit = %d\n",
		stdp->std_sdp->sd_target,stdp->std_sdp->sd_bdp->bd_unit));
	bzero(&sr,sizeof(sr));
	cdbp             = (struct cdb_6s *)&sr.sr_cdb;
	cdbp->c6s_opcode = C6OP_REWIND;
	cdbp->c6s_lun    = stdp->std_sdp->sd_lun;	
	sr.sr_ioto       = ST_IOTO_RWD;
	sr.sr_addr       = 0;
	sr.sr_dma_xfr	 = 0;
	return(stcmd_int(stdp, &sr, ST_INTERRUPT));
} /* strewind() */


static int
streqsense(struct scsi_tape_device *stdp, int polling)
{
	/* request sense - destination of sense data is *stdp->erp */
	struct scsi_req sr;
	struct cdb_6 *cdbp;
	struct esense_reply *erp;

	XDBG(("streqsense: target = %d  unit = %d\n",
		stdp->std_sdp->sd_target,stdp->std_sdp->sd_bdp->bd_unit));
	bzero(&sr,sizeof(sr));
	erp = stdp->std_erp;		/* destination of sense data */
	cdbp = (struct cdb_6 *)&sr.sr_cdb;
	cdbp->c6_opcode = C6OP_REQSENSE;
	cdbp->c6_lun = stdp->std_sdp->sd_lun;	
	cdbp->c6_len = sizeof(*erp);
	sr.sr_addr = (caddr_t)erp;
	sr.sr_dma_max = sizeof(*erp);
	sr.sr_dma_dir = SR_DMA_RD;
	sr.sr_ioto = ST_IOTO_NORM;
	return(stcmd_int(stdp, &sr, polling));
} /* streqsense() */

static int
stmodesel(struct scsi_tape_device *stdp, struct modesel_parms *mspp, int polling)
{
	struct scsi_req sr;
	struct cdb_6 *cdbp;

	XDBG(("stmodesel: target = %d  unit = %d\n",
		stdp->std_sdp->sd_target,stdp->std_sdp->sd_bdp->bd_unit));
	bzero(&sr,sizeof(sr));
	cdbp = (struct cdb_6 *)&sr.sr_cdb;
	cdbp->c6_opcode = C6OP_MODESELECT;
	cdbp->c6_lun = stdp->std_sdp->sd_lun;	
	cdbp->c6_len = sr.sr_dma_max = mspp->msp_bcount;
	sr.sr_addr = (caddr_t)&mspp->msp_data;
	sr.sr_dma_dir = SR_DMA_WR;
	sr.sr_ioto = ST_IOTO_NORM;
	return(stcmd_int(stdp, &sr, polling));
} /* stmodesel() */

static int
stmodesen(struct scsi_tape_device *stdp, struct modesel_parms *mspp, int polling)
{
	/* transfer count bytes of mode sense data to *msen_data */
	struct scsi_req sr;
	struct cdb_6 *cdbp;

	XDBG(("stmodesen: target = %d  unit = %d\n",
		stdp->std_sdp->sd_target,stdp->std_sdp->sd_bdp->bd_unit));
	bzero(&sr,sizeof(sr));
	cdbp = (struct cdb_6 *)&sr.sr_cdb;
	cdbp->c6_opcode = C6OP_MODESENSE;
	cdbp->c6_lun = stdp->std_sdp->sd_lun;	
	cdbp->c6_len = sr.sr_dma_max = mspp->msp_bcount;
	sr.sr_addr = (caddr_t)&mspp->msp_data;
	sr.sr_dma_dir = SR_DMA_RD;
	sr.sr_ioto = ST_IOTO_NORM;
	return(stcmd_int(stdp, &sr, polling));
} /* stmodesen() */

int
stread(dev_t dev, struct uio *uiop)
{
	return(st_rw(dev,uiop,SR_DMA_RD));
}

int
stwrite(dev_t dev, struct uio *uiop)
{
	return(st_rw(dev,uiop,SR_DMA_WR));
}


st_rw(dev_t dev, struct uio *uiop, int rw_flag) {

	int 			unit = ST_UNIT(dev); 
	struct scsi_tape_device *stdp = &st_std[unit];
	struct scsi_req 	*srp;
	caddr_t			dmap;
	struct cdb_6s		*cdbp;
	int			rtn = 0;
	
	if (unit >= NST) 
		return(ENXIO);
	if(uiop->uio_iovcnt != 1)		/* single requests only */ 
		return(EINVAL);
	if(uiop->uio_iov->iov_len == 0) 
		return(0);			/* nothing to do */
#ifdef	DEBUG
	if(rw_flag == SR_DMA_RD) {
		XCDBG(("st: READ; count = %xH\n", uiop->uio_iov->iov_len));
	}
	else {
		XCDBG(("st: WRITE; count = %xH\n", uiop->uio_iov->iov_len));
	}
#endif	DEBUG

	/* get memory for DMA and for an scsi_req. Do two allocs to ensure
	* page-aligned DMA.
	 * FIXME: should wire user's memory and DMA from there, avoiding
	 * a copyin() or copyout().
	 */
	srp = (struct scsi_req *) kmem_alloc (kernel_map,sizeof(*srp));
	dmap = (caddr_t)kmem_alloc(kernel_map, uiop->uio_iov->iov_len);
	if((srp == 0) || (dmap == 0))
		return(ENOMEM);
	
	/* build a CDB */
	cdbp = &(srp->sr_cdb.cdb_c6s);
	bzero(srp,sizeof(*srp));
	cdbp->c6s_lun = stdp->std_sdp->sd_lun;
	if(stdp->std_flags & STF_FIXED) {
		/* c6s_len is BLOCK COUNT */
		cdbp->c6s_len = howmany(uiop->uio_iov->iov_len,
			                stdp->std_blocksize);
		cdbp->c6s_opt = C6OPT_FIXED;
		srp->sr_dma_max = uiop->uio_iov->iov_len;
	}
	else {
		cdbp->c6s_len = srp->sr_dma_max = uiop->uio_iov->iov_len;
		if(rw_flag == SR_DMA_RD)
			if(stdp->std_flags & STF_SIL)
			    cdbp->c6s_opt |= C6OPT_SIL;
	}
		
	if(cdbp->c6s_len > C6S_MAXLEN) {
		rtn = EINVAL;
		goto out;
	}
	
	/* fill in the rest of the scsi_req */
	srp->sr_addr 		= dmap;
	srp->sr_ioto 		= ST_IOTO_NORM;
	srp->sr_dma_xfr	= 0;
	if(rw_flag == SR_DMA_RD) {
		cdbp->c6s_opcode  = C6OP_READ;
		srp->sr_dma_dir = SR_DMA_RD;
	}
	else {
		cdbp->c6s_opcode  = C6OP_WRITE;
		srp->sr_dma_dir = SR_DMA_WR;
	
	}
	/* Copy user data to kernel space if write. */
	if(rw_flag == SR_DMA_WR)
		if(rtn = copyin(uiop->uio_iov->iov_base, dmap,
		    uiop->uio_iov->iov_len))
			goto out;
		 	
	if(stcmd_int(stdp,srp,ST_INTERRUPT)) {	/* do it */
		rtn = EIO;
		goto out;
	}
	
	/* it worked. Copy data to user space if read. */
	if(srp->sr_dma_xfr && (rw_flag == SR_DMA_RD))
		rtn = copyout(dmap,uiop->uio_iov->iov_base, srp->sr_dma_xfr);  	
	if(srp->sr_io_status)
		rtn = EIO;
out:
	uiop->uio_resid = uiop->uio_iov->iov_len - srp->sr_dma_xfr;
	kmem_free(kernel_map, srp , sizeof(*srp));
	kmem_free(kernel_map, dmap, uiop->uio_iov->iov_len);
	u.u_error = rtn;
	return(rtn);

} /* st_rw() */

static int
ststart(struct scsi_device *sdp)
{
	struct scsi_tape_device *stdp = &st_std[sdp->sd_bdp->bd_unit];
	struct scsi_req *srp;
	int s;
	
	/*
	 * The job here is to be the first half of the device's
	 * request processing fsm (stintr is the second half).
	 *
	 * This routine fills in the info required in *sdp, which is 
	 * obtained from the head of stdp->std_io_q and then asks
	 * the controller to execute it. Note that the request stays
	 * at the head of std_io_q until it is passed back to the user
	 * (in stdone()).
	 *
	 * Before exiting, the device level start routine must
	 * either call scsi_docmd() to initiate device activity
	 * or call scsi_rejectcmd() to indicate that the buffer
	 * can no longer be processed.  Scsi_rejectcmd will mark
	 * the device inactive and call stintr() to allow error
	 * processing.
	 *
	 * MUST BE CALLED AT SCSI IPL.
	 */
	 
	XDBG(("ststart: top: s5c_state = %n\n",sc_s5c[0].s5c_state,
		s5c_state_values));
	ASSERT(curipl() >= sdp->sd_scp->sc_ipl);
	if(queue_empty(&stdp->std_io_q))
		panic("ststart() called with empty std_io_q");
	srp = (struct scsi_req *)queue_first(&stdp->std_io_q);
	if (stdp->std_state == STDSTATE_STARTINGCMD) {
		/* only implement busy wait for polled commands */
		stdp->std_waitcnt = 
			(stdp->std_flags & STF_POLL_IP) ? 
				stdp->std_waitlimit : 0;
		stdp->std_state = STDSTATE_DOINGCMD;
	}
	if(srp->sr_cdb.cdb_c6s.c6s_lun != stdp->std_sdp->sd_lun)
		goto err;
		
	/* fill in *sdp fields required from us */
	
	if (stdp->std_state != STDSTATE_GETTINGSENSE) {
		sdp->sd_disconnectok = (stdp->std_flags & STF_POLL_IP) ? 0 : 1;	
		sdp->sd_read         = (srp->sr_dma_dir == SR_DMA_RD) ?
				       1 : 0;		
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
	srp->sr_io_status = STRST_CMDREJ;
	scsi_rejectcmd(sdp);
	return(-1);
} /* ststart() */

static void
stintr(struct scsi_device *sdp)
{
	struct scsi_tape_device *stdp = &st_std[sdp->sd_bdp->bd_unit];
	struct cdb_6 *cdbp = &sdp->sd_cdb.cdb_c6;
	int device;

	/*
	 * We're called when the controller can no longer process
	 * the command indicated in sdp.  Our job is to
	 * advance the driver state machine and deal with any special
	 * recovery and/or error processing. On entry, the command
	 * being processed is associated with the scsi_req at the head
	 * of stdp->std_io_q.
	 *
	 * Before returning, this module should either call
	 * scsi_dstart() if there's more to do to accomplish or
	 * recover this transfer; or if we're done or giving up,
	 * call stdone() possibly indicating error.  Stdone() will
	 * wakeup() the scsi_req and advance the queue.
	 */
	 
	XDBG(("stintr: sd_state = %n\n",sdp->sd_state,scsi_sdstate_values));
	XDBG(("        target %d   lun %d\n",sdp->sd_target,sdp->sd_lun));

	switch (stdp->std_state) {
	    case STDSTATE_DOINGCMD:

		switch (sdp->sd_state) {
		    case SDSTATE_REJECTED:	/* we wouldn't do it */
		    case SDSTATE_UNIMPLEMENTED:	/* ctrlr couldn't do it */
			scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "ERROR");
			stdone(stdp, sdp->sd_resid, SR_IOST_CMDREJ);
			break;
		    case SDSTATE_SELTIMEOUT:	/* select timed out */
			if(!stdp->std_flags & STF_POLL_IP)
			    scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "ERROR");
			stdone(stdp, sdp->sd_resid, SR_IOST_SELTO);
			break;
		    case SDSTATE_TIMEOUT:	/* target never reselected */
			stdone(stdp, sdp->sd_resid, SR_IOST_IOTO);
			break;
		    case SDSTATE_ABORTED:	/* ctrlr aborted cmd */
			stdone(stdp, sdp->sd_resid, SR_IOST_TABT);
			break;
		    case SDSTATE_DROPPED:	/* target dropped connection */
			scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "ERROR");		    
			stdone(stdp, sdp->sd_resid, SR_IOST_BV);
			break;
		    case SDSTATE_RETRY:		/* SCSI bus reset, retry */
			scsi_dstart(sdp);
			break;
		    case SDSTATE_COMPLETED:
			switch (sdp->sd_status & STAT_MASK) {
			    case STAT_GOOD:	/* ta da! */
				stdone(stdp, sdp->sd_resid, SR_IOST_GOOD);
				break;
			    case STAT_CHECK:
				
				/* do a REQUEST SENSE to find out why */
				stdp->std_state = STDSTATE_GETTINGSENSE;
				stdp->std_savedresid = sdp->sd_resid;
				stdp->std_savedopcode = 
				    sdp->sd_cdb.cdb_opcode;
				cdbp->c6_opcode = C6OP_REQSENSE;
				cdbp->c6_lun = sdp->sd_lun;
				cdbp->c6_lba = 0;
				cdbp->c6_len = sizeof(*stdp->std_erp);
				cdbp->c6_ctrl = CTRL_NOLINK;
				sdp->sd_disconnectok = 0;
				sdp->sd_read = 1;
				sdp->sd_addr = (caddr_t)stdp->std_erp;
				sdp->sd_bcount = sizeof(*stdp->std_erp);
				sdp->sd_padcount = 0;
				sdp->sd_pmap = pmap_kernel();
				sdp->sd_timeout = ST_IOTO_SENSE;
				scsi_dstart(sdp);
				break;
				
			    case STAT_BUSY:
				/* FIXME...untested. */
				if (stdp->std_waitcnt-- > 0) {
					DELAY(100000);	/* 100 ms */
					scsi_dstart(sdp);
					break;
				}
				/* else fall through ... */
			    default:
				stdone(stdp, sdp->sd_resid, ST_IOST_BADST);
				break;
			}
			break;
		default:
			panic("stintr: bad sd_state");
		}
		break;
	    case STDSTATE_GETTINGSENSE:
		switch (sdp->sd_state) {
		    case SDSTATE_SELTIMEOUT:	/* select timed out */
			stdone(stdp, stdp->std_savedresid, SR_IOST_SELTO);
			break;
		    case SDSTATE_RETRY:		/* SCSI bus reset */
		    case SDSTATE_TIMEOUT:	/* target never reselected */
		    case SDSTATE_ABORTED:	/* ctrlr aborted cmd */
		    case SDSTATE_DROPPED:	/* target dropped connection */
		    case SDSTATE_REJECTED:	/* we wouldn't do it */
		    case SDSTATE_UNIMPLEMENTED:	/* ctrlr couldn't do it */
			scsi_msg(sdp, stdp->std_savedopcode, "ERROR");
			stdone(stdp, stdp->std_savedresid, SR_IOST_CHKSNV);
			break;
		    case SDSTATE_COMPLETED:
			switch (sdp->sd_status & STAT_MASK) {
			    case STAT_GOOD:
			        stdp->std_flags |= STF_ERPVALID;
				
				/* sense data valid - error handling...
				 *
				 * if error is file mark and current command
				 *    is a READ, no error. User finds out
				 *    about this via uiop->uio_resid.
				 * All others - fatal. Log error and abort.
				 */

				if(stdp->std_erp->er_filemark &&
				  (stdp->std_savedopcode == C6OP_READ)) {
					stdone(stdp, stdp->std_savedresid,
						 SR_IOST_GOOD);
				}
				else {
					/* don't bother with this at boot
					 * time...
					 */
#ifdef	notdef				/* handled by stcmd_int() */
					if(!(stdp->std_flags & STF_POLL_IP))
					    scsi_sensemsg(stdp->std_sdp,
					    	stdp->std_erp);
#endif	notdef
					stdone(stdp, stdp->std_savedresid, 
						SR_IOST_CHKSV);
				}
				break;
			    default:
				scsi_msg(sdp, stdp->std_savedopcode, "ERROR");
				stdone(stdp, stdp->std_savedresid, 
					SR_IOST_CHKSNV);
				break;
			}
			break;
		    default:
			scsi_msg(sdp, stdp->std_savedopcode, "PANIC");
			panic("stintr: bad sd_state");
		}
		break;
	    default:
		scsi_msg(sdp,sdp->sd_cdb.cdb_opcode, "PANIC");
		printf("std_state = 0x%X\n",stdp->std_state);
		panic("stintr: bad std_state");
	}
} /* stintr() */

static void
stdone(struct scsi_tape_device *stdp, 
       long resid, 
       int io_status)			/* for sr_io_status */
{
	struct scsi_req *srp;

	/*
	 * The current request (at the head of stdp->std_io_q) has either
	 * succeeded or we've given up trying to make it work.
	 * Advance std_io_q and do a wakeup().
	 */
	XDBG(("stdone: resid = %d   io_status = %n\n",resid,
	       io_status,sriost_values));
	ASSERT(curipl() >= stdp->std_sdp->sd_scp->sc_ipl);
	if (queue_empty(&stdp->std_io_q))
		panic("stdone: no buf on std_io_q");
	stdp->std_state = STDSTATE_STARTINGCMD;
	srp = (struct scsi_req *)queue_next(&stdp->std_io_q);
	queue_remove(&stdp->std_io_q, srp, struct scsi_req *, sr_io_q);
	srp->sr_dma_xfr = srp->sr_dma_max - resid;
	srp->sr_io_status = io_status;
	if(io_status == SR_IOST_CHKSV)
		srp->sr_scsi_status = STAT_CHECK;
	else
		srp->sr_scsi_status = stdp->std_sdp->sd_status;	
	if (!queue_empty(&stdp->std_io_q)) {
		XDBG(("stdone - restarting via scsi_dstart()\n"));
		scsi_dstart(stdp->std_sdp);
	}
	if(stdp->std_flags & STF_POLL_IP)
		stdp->std_flags &= ~STF_POLL_IP; 	/* polling mode */
	else {
		srp->sr_flags |= SRF_USERDONE;
		XDBG(("stdone: wakeup(%X)\n",srp));
		wakeup(srp);				/* interrupt mode */	
	}
} /* stdone() */

int
stioctl(dev_t dev, 
	int cmd, 		/* MTIOCTOP, etc */
	caddr_t data, 		/* actually a ptr to mt_op or mtget, if used */
	int flag)		/* for historical reasons. Not used. */
{
	int error = 0;
	int unit = ST_UNIT(dev); 
	struct scsi_tape_device *stdp = &st_std[ST_UNIT(dev)];
	struct mtget *mgp = (struct mtget *)data;
	struct esense_reply *erp;
	struct scsi_req *srp;
	caddr_t ap;
	char *user_addr;
	
	XDBG(("stioctl: dev = %d   cmd = %n\n",dev,
	       cmd,stioc_values));
	if (unit >= NST) 
		return(ENXIO);
	switch (cmd) {
	    case MTIOCTOP:			/* do tape op */
		XDBG(("         mt_op = %n   mt_count = %d\n",
			((struct mtop *)data)->mt_op, mtop_values,
			((struct mtop *)data)->mt_count));
		error = st_mtop(dev, (struct mtop *)data);
		break;
		
	    case MTIOCGET:			/* get status */
		/* if we just did a request sense command as part of 
		 * error recovery, avoid doing another one and
		 * thus blowing away possible volatile status info.
		 */
	    	if(!(stdp->std_flags & STF_ERPVALID))
			if(error = streqsense(stdp,ST_INTERRUPT))
				break;			/* forget it */
				
		/* one way or another, stdp->erp contains valid sense data */
		
		erp = stdp->std_erp;
		if(ST_EXABYTE(dev)) 
			mgp->mt_type = MT_ISEXB;
		else
			mgp->mt_type = MT_ISGS;
		mgp->mt_dsreg    = ((u_char *)erp)[2];
		mgp->mt_erreg    = erp->er_addsensecode;
		mgp->mt_ext_err0 = (((u_short)erp->er_stat_13) << 8) |
				    ((u_short)erp->er_stat_14);
		mgp->mt_ext_err1 = (((u_short)erp->er_stat_15) << 8) |
				    ((u_short)erp->er_rsvd_16);
		mgp->mt_resid    = (((u_int)erp->er_infomsb) << 24) |
				    ((u_int)erp->er_info);
				    
		/* force actual request sense next time */
		stdp->std_flags &= ~STF_ERPVALID;
		break;
		
	    case MTIOCFIXBLK:			/* set fixed block mode */
	    	error = st_fixed(stdp, *(int *)data, ST_INTERRUPT);
		break;
		
	    case MTIOCVARBLK:			/* set variable block mode */
	    	error = st_fixed(stdp, 0, ST_INTERRUPT);
		break;

	    case MTIOCINILL:			/* inhibit illegal length
	    					 *    errors on Read */
	    	stdp->std_flags |= STF_SIL;
		break;
		
	    case MTIOCALILL:			/* allow illegal length
	    					 *    errors on Read */
	    	stdp->std_flags &= ~STF_SIL;
		break;
		
	    case MTIOCMODSEL:			/* mode select */
		error = stmodesel(stdp,(struct modesel_parms *)data,
			ST_INTERRUPT); 
		break;	    	
	    	
	    case MTIOCMODSEN:			/* mode sense */
		error = stmodesen(stdp,(struct modesel_parms *)data,
			ST_INTERRUPT);
		break;	    	
	    	
	    case MTIOCSRQ:			/* I/O via scsi_req */
	    
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
		/* this kind of crufty but is actually a public service for the 
		 * caller - there is no way for the caller to know the lun of
		 * this device, so we put it in the CDB for€them.
		 */
		srp->sr_cdb.cdb_c6s.c6s_lun = stdp->std_sdp->sd_lun;	
		error = stcmd(stdp, srp, ST_INTERRUPT);
		srp->sr_addr = user_addr;
		if ((srp->sr_dma_dir == SR_DMA_RD) && (srp->sr_dma_xfr != 0))
			error = copyout(ap, user_addr, srp->sr_dma_xfr);
err_exit:
		if (srp->sr_dma_max != 0)
			kmem_free(kernel_map, ap, srp->sr_dma_max);
		break;

	    default:
		error = EINVAL;			/* invalid argument */
		break;
	}
	u.u_error = error;
	return(error);
} /* stioctl() */

static int 
st_mtop(dev_t dev, struct mtop *mtopp)
{
	struct scsi_tape_device *stdp = &st_std[ST_UNIT(dev)];
	struct scsi_req 	*srp;
	struct cdb_6s		*cdbp;
	int			rtn = 0;
	int			count;
	
	/* 
	 * none of these operations perform DMA. For each, just allocate a
	 * scsi_req, build a cdb, and pass it to stcmd().
	 */
	 
	srp = (struct scsi_req *) kmem_alloc (kernel_map, 
		sizeof(struct scsi_req));
	if(srp == 0) 
		return(ENOMEM);
		
	/* build a CDB */
	cdbp = &(srp->sr_cdb.cdb_c6s);
	bzero(srp,sizeof(*srp));
	cdbp->c6s_lun = stdp->std_sdp->sd_lun;
	srp->sr_ioto = ST_IOTO_NORM;	/* unless overridden below */
	
	switch(mtopp->mt_op) {
	    case MTWEOF:		/* write file marks */
	    	cdbp->c6s_opcode = C6OP_WRTFM;
		goto setcount_f;
		
	    case MTFSF:			/* space file marks forward */
	    	cdbp->c6s_opcode = C6OP_SPACE;
		cdbp->c6s_opt    = C6OPT_SPACE_FM;
		srp->sr_ioto     = mtopp->mt_count * ST_IOTO_SPFM;
		goto setcount_f;
		
	    case MTBSF:			/* space file marks backward */
	    	cdbp->c6s_opcode = C6OP_SPACE;
		cdbp->c6s_opt    = C6OPT_SPACE_FM;
		srp->sr_ioto     = mtopp->mt_count * ST_IOTO_SPFM;
		goto setcount_b;
		
	    case MTFSR:			/* space records forward */
	    	cdbp->c6s_opcode = C6OP_SPACE;
		cdbp->c6s_opt    = C6OPT_SPACE_LB;
		srp->sr_ioto     = ST_IOTO_SPR;
setcount_f:
		cdbp->c6s_len    = mtopp->mt_count;
		break;

	    case MTBSR:			/* space records backward */
	    	cdbp->c6s_opcode = C6OP_SPACE;
		cdbp->c6s_opt    = C6OPT_SPACE_LB;
		srp->sr_ioto     = ST_IOTO_SPR;
setcount_b:
		count 		 = 0 - mtopp->mt_count;
		cdbp->c6s_len    = count;
		break;
			
	    case MTREW:			/* rewind */
	    	cdbp->c6s_opcode = C6OP_REWIND;
		srp->sr_ioto     = ST_IOTO_RWD;
		break;
		
	    case MTOFFL:		/* set offline */
	    	cdbp->c6s_opcode = C6OP_STARTSTOP;
					/* note load bit is 0 */
		srp->sr_ioto     = ST_IOTO_RWD;
		break;
		
	    case MTNOP:			/* nop / get status */
	    case MTCACHE:		/* enable cache */
	    case MTNOCACHE:		/* disable cache */
	    case MTRETEN:
	    case MTERASE:
	    default:
	    	rtn = EINVAL;		/* FIXME: unsupported? */
		goto out;
		
	}
	srp->sr_addr    = 0;
	srp->sr_dma_xfr	= 0;

	rtn = stcmd_int(stdp,srp,ST_INTERRUPT); 	/* do it */
	XDBG(("st_mtop: sr_io_status = %n\n",
	       srp->sr_io_status,sriost_values));
out:
	kmem_free(kernel_map,srp,sizeof(struct scsi_req));
	return(rtn);
	
} /* st_mtop() */

static int
st_fixed(struct scsi_tape_device *stdp, int blocksize, int polling )
{
	/* blocksize == 0 --> variable
	 * blocksize != 0 --> fixed @ blocksize
	 */
	int rtn;
	struct mode_sel_hdr *mshp;
	/* execute mode sense, then a mode select with block length set to 
	 * 0 (variable) or blocksize (fixed)
	 */

	stdp->std_mspp->msp_bcount = sizeof(struct mode_sel_hdr) + 
				     sizeof(struct mode_sel_bd);
	if(rtn = stmodesen(stdp, stdp->std_mspp, polling))
		return(rtn);
		
	/* some fields in the header have to be zeroed...But we maintain
	 * current values of buffered mode and speed.
	 */

	mshp = &stdp->std_mspp->msp_data.msd_header;	
	mshp->msh_sd_length_0 = 0;
	mshp-> msh_med_type  = 0;
	mshp-> msh_wp        = 0;
	mshp-> msh_bd_length = sizeof(struct mode_sel_bd);
	stdp->std_mspp->msp_data.msd_blockdescript.msbd_blocklength =
			blocksize;
	stdp->std_mspp->msp_data.msd_blockdescript.msbd_numblocks = 0;
	if(rtn = stmodesel(stdp, stdp->std_mspp, polling))
		return(rtn);
	if(blocksize) {
		stdp->std_flags |= STF_FIXED;
		stdp->std_blocksize = blocksize;
	}
	else
		stdp->std_flags &= ~STF_FIXED;
	return(0);
}

static int stcmd_int(struct scsi_tape_device *stdp, 
      struct scsi_req *srp,
      int polling)
{
	/* this is used to detect "sr_io_status != 0" errors, which are
	 * detected by the user directly (when doing MTIOCSRQ) for additional
	 * error info. For normal (our own) use, any error returns nonzero.
	 */
	 
	int rtn;
	
	rtn = stcmd(stdp, srp, polling);
	if(!rtn) {
		if(srp->sr_io_status) {
			XDBG(("stcmd_int: cmd = 0x%x sr_io_status = 0x%x\n",
				 srp->sr_cdb.cdb_c6.c6_opcode, 
				 srp->sr_io_status));
			if(polling == ST_INTERRUPT) {
			    printf("st: cmd = 0x%x sr_io_status = %XH\n",
				    srp->sr_cdb.cdb_c6.c6_opcode,
				    srp->sr_io_status);
			    if(srp->sr_io_status == SR_IOST_CHKSV) {
				printf("    Sense key = 0x%x  "
					"Sense Code = 0x%x\n",
					srp->sr_esense.er_sensekey,
					srp->sr_esense.er_addsensecode);
			    }
			}
			return(EIO);
		}
		else
			return(0);
	}
	else
		return(0);
} /* stcmd_int() */

static int
stcmd(struct scsi_tape_device *stdp, 
      struct scsi_req *srp,
      int polling)
{
	int rtn = 0;
	int s;
	struct scsi_device *sdp=stdp->std_sdp;
	
	XDBG(("stcmd\n"));
	XCDBG(("stcmd: op = %n target = %d  lun = %d\n",
		srp->sr_cdb.cdb_opcode,scsi_cmd_values,
		sdp->sd_target,sdp->sd_lun));
	stdp->std_waitlimit = (polling == ST_WAIT) ? 50 : 0;/* 50 retries */
	
	/*
	 * add request to per-device queue
	 */
	 
	s = spln(ipltospl(sdp->sd_scp->sc_ipl));
	queue_enter(&stdp->std_io_q, srp, struct scsi_req *, sr_io_q);
	rtn = 0;
	if(polling == ST_INTERRUPT)
		srp->sr_flags &= ~SRF_USERDONE;
	else
		stdp->std_flags |= STF_POLL_IP;
	if (sdp->sd_active == 0) {
		XDBG(("stcmd: startup via scsi_dstart()\n"));
		rtn = scsi_dstart(sdp);
	}
	if(rtn == 0) {
	
		/* only sleep() if we could actually start command... */
					
		if (polling == ST_INTERRUPT) {
		
			int wait_result;
			
			XDBG(("stcmd: sleep(%X, %X)\n", srp, PRIBIO));
			while(!(srp->sr_flags & SRF_USERDONE)) {
			        sleep(srp,PRIBIO);
#ifdef	DEBUG
				if(!(srp->sr_flags & SRF_USERDONE)) {	
				    wait_result = 
				    	current_thread()->wait_result;
				    XDBG(("stcmd - wakeup, sr not done\n"));
				    XDBG(("      wait_result = %X\n",
				    	wait_result));
				}
#endif	DEBUG
			}
			splx(s);
		}
		else {
			int tries = 0;
	
			splx(s);
			ASSERT(curipl() == 0);
			stdp->std_flags |= STF_POLL_IP;
			while (stdp->std_flags & STF_POLL_IP) {
				/* Wait up to 1s for command to complete */
				if (++tries > 1000) {
					scsi_timeout(stdp->std_sdp->sd_scp);
					tries = 0;
				} else
					DELAY(1000);
			}
		}
		if(srp->sr_io_status && polling)
			rtn = EIO;
		
	} else {
		splx(s);
		stdp->std_flags &= ~STF_POLL_IP;
	}
	
	/* copy autosense data to *srp if valid */
	if(srp->sr_io_status == SR_IOST_CHKSV);
		srp->sr_esense = *stdp->std_erp;
	
	/* if this was a write which succeeded, remember it for stclose() */ 
	if((rtn == 0) &&
	   (srp->sr_io_status == SR_IOST_GOOD) &&
	   (srp->sr_cdb.cdb_opcode == C6OP_WRITE))
	   	stdp->std_flags |= STF_WRITE;
	else
		stdp->std_flags &= ~STF_WRITE;
	XCDBG(("stcmd: io_status = %n\n",srp->sr_io_status,sriost_values));
	return(rtn);
} /* stcmd() */

#endif	NST

/* end of st.c */




