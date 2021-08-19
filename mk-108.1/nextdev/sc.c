/*	@(#)sc.c	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 * sc.c -- implementation of NCR53C90 specific scsi routines
 * KERNEL VERSION
 *
 * FIXME:
 *	See if we can utilize the "dual-ranked" command register
 *	Someday this should handle extended messages and negotiate
 *		synchronous transfers
 *
 * HISTORY
 * 03-May-90	Doug Mitchell
 *	Changed XDBG to XNDB
 * 07-Mar-90	Doug Mitchell
 *	Avoided calling sfa_arbitrate() if we already own hardware
 * 20-Feb-90	Doug Mitchell
 *	Added sfa_arbitrate logic
 * 14-Nov-89	Doug Mitchell
 *	Avoided sending ID message as first byte in case PHASE_COMMAND 
 *	in scfsm()
 * 24-May-89	Doug Mitchell
 *	Enabled 500 ms delay in screset()
 * 24-Mar-89	Doug Mitchell
 *	Used sdp->sd_timeout in all calls to scsi_expectintr()
 * 10-Feb-89	Doug Mitchell at NeXT
 *	scstart now returns an int for command reject detection
 * 09-Feb-89	Doug Mitchell at NeXT
 *	Added s5cp->s5c_53c90a logic
 *	Purged some detailed revision history
 * 17-Jan-89	Doug Mitchell (dmitch) at NeXT
 *	Added some XDBG fields
 *	Fixed target extraction bug in state S5CSTATE_RESELECTABLE in scintr()
 *	Subtracted fifo flags value from bytes DMA's in data out in scintr()
 * 1-Mar-88    Mike DeMoney (mike) at NeXT
 *	Upgrade parity error support.
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 */ 

#import <sc.h>
#if NSC > 0

#import <sys/errno.h>
#import <sys/types.h>
#import <sys/buf.h>
#import <sys/time.h>
#import <sys/param.h>
#import <sys/kernel.h>
#import <sys/conf.h>
#import <sys/proc.h>
#import <kern/task.h>
#import <sys/callout.h>
#import <vm/vm_kern.h>
#import <sys/dk.h>
#import <next/autoconf.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/printf.h>
#import <nextdev/disk.h>
#import <nextdev/dma.h>
#import <nextdev/scsireg.h>
#import <nextdev/scsivar.h>
#import <nextdev/sf_access.h>
#import <nextdev/odreg.h>		/* only for DISR */
#import <nextdev/screg.h>
#import <nextdev/scvar.h>
#import <kern/xpr.h>
#import <machine/spl.h>
#import <next/kernel_pmon.h>		/* debug only */
#import <nextdev/fd_reg.h>

extern u_char disr_shadow;		/* for cubes with floppy protos only */
extern void *fd_controller;		/* actually an array of fd_controllers 
					 */

/* No 53C90A's just yet */
/* #define	SC_53C90A	1	/* 53C90 has bugs, 53C90A fixes 'em */
/* #define 	DO_IGNORE_ILLC	1	/* enables "ignore illegal command"  */
					/*   after reselection */
#define EARLYRESEL_ENABLE 	1	/* enable reselection before */
					/*    checking for other commands in 
					 *    pipe in scintr()  */
#define ALLOW_MSG_REJECT	1	/* handle message reject gracefully */

#if	DEBUG
#define EPRINTF(x)	{ XDBG(x); printf x; }
/* #define	SC_TRACE	1	/* things are really messed up */
#else	DEBUG
#define	EPRINTF(x)
#endif	DEBUG

#define	MAX_S5CERRS	5	/* max errors on one command */

/*
 * Normal SCSI mass storage gate array setup
 * WARNING: S5R_CLOCKFREQ MUST match setup in S5RDMAC_NORMALBITS!
 */
#define	S5RDMAC_NOINTRBITS	S5RDMAC_20MHZ
#define	S5RDMAC_NORMALBITS	(S5RDMAC_NOINTRBITS | S5RDMAC_INTENABLE)
#define	S5R_CLOCKFREQ		20	/* 20 MHz */
#define	S5R_TIMEOUT		250	/* 250 ms */

/*
 * procedure declarations
 *
 * All procedures are declared static since they should only
 * be accessed via scdriver or sc_scsw.
 */
/* called externally via bus_driver struct */
static int scprobe(/* volatile caddr_t reg, int ctrl */);
static int scslave(/* struct bus_device *bdp, volatile caddr_t reg */);
static void scattach(/* struct bus_device *bdp */);
static void scgo(/* struct bus_ctrl *bcp */);
static void scinit(/* volatile caddr_t reg */);

/* called externally via scsi_csw */
static scstart(/* struct scsi_device *sdp */);
static int scioctl(/* struct scsi_ctrl *scp, int cmd, caddr_t data, int flag */);
static void screset(/* struct scsi_ctrl *scp, int abortflag, char *why */);

/* called externally via dma_chan */
static void scdmaintr(/* struct scsi_5390_ctrl *s5cp */);

/* called externally via install_scanned_intr */
static void scintr(/* struct scsi_5390_ctrl *s5cp */);

/* only called internally */
static void scsetup(/* volatile struct scsi_5390_regs *s5rp, int target */);
static void scfsm(/* struct scsi_5390_ctrl *s5cp */);
static void scdmastart(/* struct scsi_5390_ctrl *s5cp, int direction */);
static void scmsgin(/* struct scsi_5390_ctrl *s5cp */);
static void scmsgout(/* struct scsi_5390_ctrl *s5cp, u_char msg,
    u_char postmsgstate */);
static void scshowstatus(/* struct scsi_5390_ctrl *s5cp, char *why */);
static void sc_dostart(/* struct scsi_device *sdp */);

/* autoconfiguration info */
static struct bus_ctrl *sc_bcp[NSC];

/* hack - used by sd.c */
unsigned scminphys(/* struct buf *bp */);

struct	bus_driver scdriver = {
	scprobe, scslave, (int (*)())scattach, (int (*)())scgo,
	NULL, NULL, (int (*)())scinit, sizeof (struct scsi_5390_regs),
	"SEE bus_device", NULL, "sc", sc_bcp, BUS_SCSI
};

/* controller switch */
struct scsi_csw sc_scsw = {
	scstart,
	scioctl,
	screset
};

/*
 * NCR53C90 register definitions
 */
#if	DEBUG || XPR_DEBUG
struct reg_values s5r_cmd_values[] = {
/* value		name */
{  S5RCMD_NOP,		"NOP"		},
{  S5RCMD_FLUSHFIFO,	"FlushFifo"	},
{  S5RCMD_RESETCHIP,	"ResetChip"	},
{  S5RCMD_RESETBUS,	"ResetBus"	},
{  S5RCMD_SELECT,	"Select"	},
{  S5RCMD_SELECTATN,	"SelectATN"	},
{  S5RCMD_SELECTATNSTOP,"SelectATNStop"	},
{  S5RCMD_ENABLESELECT,	"EnableSelect"	},
{  S5RCMD_DISABLESELECT,"DisableSelect"	},
{  S5RCMD_TRANSFERINFO,	"TransferInfo"	},
{  S5RCMD_INITCMDCMPLT,	"InitCmdCmplt"	},
{  S5RCMD_MSGACCEPTED,	"MsgAccepted"	},
{  S5RCMD_TRANSFERPAD,	"TransferPad"	},
{  S5RCMD_SETATN,	"SetATN"	},
{  S5RCMD_DISCONNECT,	"Disconnect"	},
{  0,			NULL		}
};

struct reg_desc s5r_cmd_desc[] = {
/* mask			shift	name		format	values */
{  S5RCMD_ENABLEDMA,	0,	"DMA",		NULL,	NULL	},
{  0x7f,		0,	"CMD",		"0x%x",	s5r_cmd_values },
{  0,			0,	NULL,		NULL,	NULL	}
};

struct reg_desc s5r_status_desc[] = {
/* mask			shift	name		format	values */
{  S5RSTS_GROSSERROR,	0,	"GROSS_ERROR",	NULL,	NULL	},
{  S5RSTS_PARITYERROR,	0,	"PARITY_ERROR",	NULL,	NULL	},
{  S5RSTS_COUNTZERO,	0,	"COUNT_ZERO",	NULL,	NULL	},
{  S5RSTS_TRANSFERCMPLT,0,	"XFER_CMPLT",	NULL,	NULL	},
{  S5RSTS_PHASEMASK,	0,	"SCSI_PHASE",	NULL,	scsi_phase_values },
{  0,			0,	NULL,		NULL,	NULL	}
};

struct reg_desc s5r_intrstatus_desc[] = {
/* mask			shift	name		format	values */
{  S5RINT_SCSIRESET,	0,	"SCSI_RESET",	NULL,	NULL,	},
{  S5RINT_ILLEGALCMD,	0,	"ILLEGAL_CMD",	NULL,	NULL,	},
{  S5RINT_DISCONNECT,	0,	"DISCONNECT",	NULL,	NULL,	},
{  S5RINT_BUSSERVICE,	0,	"BUS_SERVICE",	NULL,	NULL,	},
{  S5RINT_FUNCCMPLT,	0,	"FUNC_CMPLT",	NULL,	NULL,	},
{  S5RINT_RESELECTED,	0,	"RESELECTED",	NULL,	NULL,	},
{  S5RINT_SELECTEDATN,	0,	"SELECTED_ATN",	NULL,	NULL,	},
{  S5RINT_SELECTED,	0,	"SELECTED",	NULL,	NULL,	},
{  0,			0,	NULL,		NULL,	NULL	}
};

struct reg_desc s5r_config_desc[] = {
/* mask			shift	name		format	values */
{  S5RCNF_SLOWCABLE,	0,	"SLOW_CABLE",	NULL,	NULL	},
{  S5RCNF_DISABLERESETINTR,0,	"DIS_RESET_INTR",NULL,	NULL	},
{  S5RCNF_PARITYTEST,	0,	"PARITY_TEST",	NULL,	NULL	},
{  S5RCNF_ENABLEPARITY,	0,	"ENABLE_PARITY",NULL,	NULL	},
{  S5RCNF_BUSIDMASK,	0,	"BUS_ID",	"0x%x",	NULL	},
{  0,			0,	NULL,		NULL,	NULL	}
};

/*
 * Mass storage gate array register descriptions
 */
struct reg_values s5r_clock_values[] = {
/* value		name */
{  S5RDMAC_10MHZ,	"10 MHz"	},
{  S5RDMAC_12MHZ,	"12.5 MHz"	},
{  S5RDMAC_16MHZ,	"16.6 MHz"	},
{  S5RDMAC_20MHZ,	"20 MHz"	},
{  0,			NULL		}
};

struct reg_desc s5r_dmactrl_desc[] = {
/* mask			shift	name		format	values */
{  S5RDMAC_CLKMASK,	0,	"SCSI_CLOCK",	NULL,	s5r_clock_values },
{  S5RDMAC_INTENABLE,	0,	"INT_ENABLE",	NULL,	NULL	},
{  S5RDMAC_DMAMODE,	0,	"DMA_MODE",	NULL,	NULL	},
{  S5RDMAC_DMAREAD,	0,	"DMA_READ",	NULL,	NULL	},
{  S5RDMAC_FLUSH,	0,	"FLUSH",	NULL,	NULL	},
{  S5RDMAC_RESET,	0,	"RESET",	NULL,	NULL	},
{  S5RDMAC_WD33C92,	0,	"WD33C92",	NULL,	NULL	},
{  0,			0,	NULL,		NULL,	NULL	}
};

struct reg_values s5r_dmastate_values[] = {
/* value		name */
{  S5RDMAS_D0S0,	"DMA 0, SCSI 0"		},
{  S5RDMAS_D0S1,	"DMA 0, SCSI 1"		},
{  S5RDMAS_D1S1,	"DMA 1, SCSI 1"		},
{  S5RDMAS_D1S0,	"DMA 1, SCSI 0"		},
{  0,			NULL			}
};

struct reg_desc s5r_dmastatus_desc[] = {
/* mask			shift	name		format	values */
{  S5RDMAS_STATE,	0,	"DMA_STATE",	NULL,	s5r_dmastate_values },
{  S5RDMAS_OUTFIFOMASK,	0,	"OUT_FIFO_BYTE","0x%x",	NULL	},
{  S5RDMAS_INFIFOMASK,	0,	"IN_FIFO_BYTE",	"0x%x",	NULL	},
{  0,			0,	NULL,		NULL,	NULL	}
};

/*
 * Driver state values
 */
struct reg_values s5c_state_values[] = {
/* value			name */
{  S5CSTATE_DISCONNECTED,	"DISCONNECTED"		},
{  S5CSTATE_RESELECTABLE,	"RESELECTABLE"		},
{  S5CSTATE_SELECTING,		"SELECTING"		},
{  S5CSTATE_INITIATOR,		"INITIATOR"		},
{  S5CSTATE_DISABLINGSEL,	"DISABLING_SEL"		},
{  S5CSTATE_COMPLETING,		"COMPLETING"		},
{  S5CSTATE_DMAING,		"DMAING"		},
{  S5CSTATE_ACCEPTINGMSG,	"ACCEPTING_MSG"		},
{  S5CSTATE_SENDINGMSG,		"SENDING_MSG"		},
{  S5CSTATE_PADDING,		"PADDING"		},
{  S5CSTATE_GETTINGMSG,		"GETTING_MSG"		},
{  S5CSTATE_GETTINGLUN,		"GETTING_LUN"		},
#ifdef	DO_IGNORE_ILLC
{  S5CSTATE_IGNOREILLCMD,	"INGNORING_ILLCMD"      },	/* dpm 01/20/89 */
#endif	DO_IGNORE_ILLC
{  0,				NULL			}
};
#endif	DEBUG || XPR_DEBUG

static void
scinit(reg)
volatile register caddr_t reg;
{
	volatile struct scsi_5390_regs *s5rp = (struct scsi_5390_regs *)reg;

	XDBG(("scinit\n"));
	/*
	 * Gain access to 53C90.
	 */
	switch(machine_type) {
    	    case NeXT_CUBE:
	    	*(u_char *)DISR_ADDRS = disr_shadow = disr_shadow & ~OMD_SDFDA;
		break;
	    default:
	    	XDBG(("scinit: calling fc_flpctl_bclr; fcp = 0x%x\n", 
			&fd_controller));
		fc_flpctl_bclr(&fd_controller, FLC_82077_SEL);
		break;
	}
	
	/*
  	 * Shut down scsi interrupts if we think a scsi chip is out
	 * there.
	 */
	XDBG(("scinit: PROBING\n"));
	if (probe_rb(&s5rp->s5r_dmactrl)) {
		s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS | S5RDMAC_RESET;
		DELAY(10);
		s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS;
		DELAY(10);
	}
	scsi_init();
	XDBG(("scinit: DONE\n"));
};

static int
scprobe(reg, ctrl)
volatile register caddr_t reg;
register int ctrl;
{
	volatile struct scsi_5390_regs *s5rp;
	struct scsi_5390_ctrl *s5cp = &sc_s5c[ctrl];
	struct scsi_ctrl *scp = &sc_sc[ctrl];
	struct bus_ctrl *bcp = sc_bcp[ctrl];
	sf_access_head_t sfahp = &sf_access_head[ctrl];

	reg += slot_id_bmap;
	s5rp = (struct scsi_5390_regs *)reg;
	bzero (s5cp, sizeof(struct scsi_5390_ctrl));
	/*
	 * setup for arbitration between SCSI and floppy.
	 */
	simple_lock_init(&sfahp->sfah_lock);
	queue_init(&sfahp->sfah_q);
	sfahp->sfah_wait_cnt = 0;
	sfahp->sfah_flags  = 0;
	sfahp->sfah_last_dev = SF_LD_NONE;
	sfahp->sfah_excl_q = 0;
	sfahp->sfah_busy = 0;
	s5cp->s5c_sfah = sfahp;
	s5cp->s5c_sfad.sfad_flags = 0;

	/*
	 * Gain access to 53C90.
	 */
	switch(machine_type) {
    	    case NeXT_CUBE:
		*(u_char *)DISR_ADDRS = disr_shadow = disr_shadow & ~OMD_SDFDA;
		break;
	    default:
		fc_flpctl_bclr(&fd_controller, FLC_82077_SEL);
		break;
	}

	/*
	 * make sure we can read a few of the regs and then make sure
	 * we see something that looks like the fifo
	 */
	if (!probe_rb(&s5rp->s5r_cntlsb) || !probe_rb(&s5rp->s5r_fifo)
	    || !probe_rb(s5rp->s5r_cmd) || !probe_rb(s5rp->s5r_fifoflags))
		return (0);
	s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS | S5RDMAC_RESET;
	DELAY(10);
	s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS;
	DELAY(10);
	if ((s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) != 0)
		return(0);
	s5rp->s5r_fifo = 0;
	s5rp->s5r_fifo = 0;
	if ((s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) != 2)
		return(0);
		
	/*
	 * initialize scsi_5390_ctrl struct
	 */
	s5cp->s5c_scp = scp;
	s5cp->s5c_state = S5CSTATE_DISCONNECTED;
	s5cp->s5c_cursdp = NULL;
	s5cp->s5c_msgoutstate = S5CMSGSTATE_NONE;

	/* 
	 * determine if this is a 53c90 or a 53c90a. The difference is the 
	 * config2 register.
	 */
	
	s5rp->s5r_config2 = S5RCNF2_TESTMASK;
	DELAY(10);
	if((s5rp->s5r_config2 & S5RCNF2_TESTMASK) == S5RCNF2_TESTMASK) {
		s5rp->s5r_config2 = 0;
		DELAY(10);
		if((s5rp->s5r_config2 & S5RCNF2_TESTMASK) == 0)
			s5cp->s5c_53c90a = 1;    /* success - new part */
	}
		
	/*
	 * initialize dma_chan and interrupt linkage
	 *
	 * FIXME: share this DMA channel with floppy!
	 */
	s5cp->s5c_dc.dc_handler = scdmaintr;
	s5cp->s5c_dc.dc_hndlrarg = (int)s5cp;
	s5cp->s5c_dc.dc_hndlrpri = CALLOUT_PRI_SOFTINT1;
	s5cp->s5c_dc.dc_flags = DMACHAN_AUTOSTART | DMACHAN_PAD;
#if XPR_DEBUG
	if (xprflags & XPR_SCSI)
		s5cp->s5c_dc.dc_flags |= DMACHAN_DBG;
#endif XPR_DEBUG
	s5cp->s5c_dc.dc_ddp = (struct dma_dev *)P_SCSI_CSR;
	dma_init(&s5cp->s5c_dc, I_SCSI_DMA);
	install_scanned_intr(I_SCSI, (func)scintr, s5cp);
	/*
	 * link up with the greater scsi world
	 * and get a target number assigned for this controller
	 */
	scsi_probe(bcp, scp, &sc_scsw, I_IPL(I_SCSI));
	/*
	 * Reset and configure the chip
	 */
	scsetup(s5rp, scp->sc_target);
	s5rp->s5r_cmd = S5RCMD_RESETBUS;
	DELAY(500000);		/* 500 ms to let targets cleanup */
	if(s5cp->s5c_53c90a)
		printf("SCSI 53C90A Controller, Target %d, as ",
			 scp->sc_target);
	else
		printf("SCSI 53C90 Controller, Target %d, as ",
			 scp->sc_target);
	return((int)reg);
} /* scprobe() */

static void
scsetup(s5rp, target)
volatile struct scsi_5390_regs *s5rp;
int target;
{
	register int junk;

	s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS | S5RDMAC_RESET;
	DELAY(10);
	s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS;
	DELAY(10);
	s5rp->s5r_config = S5RCNF_ENABLEPARITY | S5RCNF_DISABLERESETINTR
	    | (target & S5RCNF_BUSIDMASK);
	/* 20 MHz clock, 250ms selection timeout */
	s5rp->s5r_clkconv = S5RCLKCONV_FACTOR(S5R_CLOCKFREQ);
	s5rp->s5r_seltimeout = S5RSELECT_TIMEOUT(S5R_CLOCKFREQ, S5R_TIMEOUT);
	s5rp->s5r_syncoffset = 0;	/* async */
	s5rp->s5r_syncperiod = 5;
	junk = s5rp->s5r_intrstatus;	/* clear any pending interrupts */
	DELAY(10);
	s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS;
}

static int
scslave(bdp, reg)
struct bus_device *bdp;
volatile caddr_t reg;
{
	/*
	 * let scsi_slave do all the work since this
	 * is mostly a device level activity
	 */
	return(scsi_slave(&sc_sc[bdp->bd_bc->bc_ctrl], bdp));
}

static void
scattach(bdp)
struct bus_device *bdp;
{
	/*
	 * nothing to do really, but give generic level a
	 * shot at it
	 */
	scsi_attach((*scsi_sdswlist[bdp->bd_type]->sdsw_getsdp)(bdp->bd_unit));
	return;
}

static
scstart(sdp)
struct scsi_device *sdp;
{
	struct scsi_5390_ctrl *s5cp = &sc_s5c[sdp->sd_bdp->bd_bc->bc_ctrl];
	struct scsi_ctrl *scp = s5cp->s5c_scp;
	union cdb *cdbp = &sdp->sd_cdb;
	int cmdlen, identify_msg, cdb_ctrl;

	XDBG(("scstart: %n targ %d lun %d bcount 0x%x\n", 
	     cdbp->cdb_opcode, scsi_cmd_values, sdp->sd_target,
	     sdp->sd_lun, sdp->sd_bcount));
#ifdef SC_TRACE
	scsi_msg(sdp, cdbp->cdb_opcode, "STARTING");
#endif SC_TRACE
	/*
	 * the scsi_device is all setup to describe a command,
	 * blast the command to the target
	 *
	 * MUST BE CALLED AT SCSI IPL
	 */
	ASSERT(curipl() >= scp->sc_ipl);

	/*
	 * Figure out what kind of cdb we've been given
	 * and snag the ctrl byte
	 */
	switch (SCSI_OPGROUP(cdbp->cdb_opcode)) {
	case OPGROUP_0:
		cmdlen = sizeof(struct cdb_6);
		cdb_ctrl = cdbp->cdb_c6.c6_ctrl;
		break;
	case OPGROUP_1:
		cmdlen = sizeof(struct cdb_10);
		cdb_ctrl = cdbp->cdb_c10.c10_ctrl;
		break;
	case OPGROUP_5:
		cmdlen = sizeof(struct cdb_12);
		cdb_ctrl = cdbp->cdb_c12.c12_ctrl;
		break;
	default:
		/* 
		 * all others - assume a max of 12 bytes and a control byte 
		 * of 0. If target moves fewer bytes, no problem.
		 */
		cmdlen = sizeof(struct cdb_12);
		cdb_ctrl = 0;
		break;
	}

	/*
	 * initialize return parameters
	 */
	sdp->sd_state = SDSTATE_START;
	sdp->sd_resid = sdp->sd_bcount;
	sdp->sd_bytes_moved = 0;
	sdp->sd_status = 0;

	/*
	 * Make sure nothing unreasonable has been asked of us
	 */
	if (! DMA_BEGINALIGNED(sdp->sd_addr) || 
            ((cdb_ctrl & CTRL_LINKFLAG) != CTRL_NOLINK)) {
	    	XNDBG(("scstart: illegal request"));
		if (! DMA_BEGINALIGNED(sdp->sd_addr))
			EPRINTF(("sc%d: unaligned buffer address\n",
			    s5cp->s5c_scp->sc_bcp->bc_ctrl));
		sdp->sd_state = SDSTATE_UNIMPLEMENTED;
		scsi_cintr(scp);
		return(-1);
	}

	if (s5cp->s5c_state != S5CSTATE_DISCONNECTED
	    && s5cp->s5c_state != S5CSTATE_RESELECTABLE) {
#if	DEBUG
		scshowstatus(s5cp, "scstart: bad state");
#endif	DEBUG
		XNDBG(("scstart panic: s5c_state = %n\n",
			s5cp->s5c_state,s5c_state_values));
		panic("scstart: bad state");
	}
	/*
	 * Setting cursdp indicates that the controller has been
	 * given work from the sc_bcp->bc_tab.  s5c_cursdp non-null tells
	 * the interrupt routine that scsi_cintr should be called
	 * when the controller gets to a state where no more progress
	 * is possible.
	 */
	s5cp->s5c_cursdp = sdp;
	s5cp->s5c_curaddr = sdp->sd_addr;
	s5cp->s5c_curbcount = sdp->sd_bcount;
	/*
	 * allow one more pad byte, because we get transfer completes
	 * before target can switch phases so we still see DATA phase.
	 * A pad will allow us to respond to the data phase and
	 * get an interrupt when the target switches to status phase.
	 */
	s5cp->s5c_curpadcount = sdp->sd_padcount + 1;
	s5cp->s5c_msgoutstate = S5CMSGSTATE_NONE;
	s5cp->s5c_state = S5CSTATE_SELECTING;
	s5cp->s5c_target = -1;
	s5cp->s5c_errcnt = 0;
	s5cp->s5c_cmdlen = cmdlen;
	/*
	 * arbitrate with floppy driver for access to hardware.
	 */
	s5cp->s5c_sfad.sfad_start = sc_dostart;
	s5cp->s5c_sfad.sfad_arg   = sdp;
	if(sfa_arbitrate(s5cp->s5c_sfah, &s5cp->s5c_sfad))
		XNDBG(("scstart: sfa_arbitrate ENQUEUED\n"));
	return(0);
} /* scstart() */

static void
sc_dostart(sdp)
struct scsi_device *sdp;
{
	/*
	 * this is the hardware end of scstart(). Called either via 
	 * sfa_arbitrate() (if the bus is free) or sfa_relinquish().
	 */
	 
	struct scsi_5390_ctrl *s5cp = &sc_s5c[sdp->sd_bdp->bd_bc->bc_ctrl];
	struct scsi_ctrl *scp = s5cp->s5c_scp;
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *)scp->sc_bcp->bc_addr;
	union cdb *cdbp = &sdp->sd_cdb;
	int cmdlen = s5cp->s5c_cmdlen;
	int identify_msg;
	char *cp;
	int s;
	
	/*
	 * gain access to SCSI chip
	 */
	s = spldma();
	switch(machine_type) {
    	    case NeXT_CUBE:
	    	*(u_char *)DISR_ADDRS = disr_shadow = disr_shadow & ~OMD_SDFDA;
		break;
	    default:
		fc_flpctl_bclr(&fd_controller, FLC_82077_SEL);
		break;
	}
	splx(s);
	
	/*
	 * If we weren't the last owner of the hardware, grab the SCSI/floppy
	 * DMA interrupt.
	 */
	if(s5cp->s5c_sfah->sfah_last_dev != SF_LD_SCSI)
		install_scanned_intr(I_SCSI_DMA, (func)dma_intr,
			&s5cp->s5c_dc);

	/*
	 * set target bus id
	 * punch cdb into fifo
	 * write command register
	 */
	XNDBG(("sc_dostart: %n targ %d lun %d bcount 0x%x\n", 
	     cdbp->cdb_opcode,scsi_cmd_values, sdp->sd_target,
	     sdp->sd_lun,s5cp->s5c_curbcount));
	XNDBG(("         sc_active = %d\n",scp->sc_active));
	s5rp->s5r_cmd = S5RCMD_FLUSHFIFO;
	s5rp->s5r_select = sdp->sd_target;
	identify_msg = MSG_IDENTIFYMASK | (sdp->sd_lun & MSG_ID_LUNMASK);
	if ((sdp->sd_disconnectok) && (s5cp->s5c_53c90a))
		identify_msg |= MSG_ID_DISCONN;
	s5rp->s5r_fifo = identify_msg;
	for (cp = (char *)cdbp; cmdlen-- > 0;)
		s5rp->s5r_fifo = *cp++;
	s5rp->s5r_cmd = S5RCMD_SELECTATN;
	pmon_log_event(PMON_SOURCE_SCSI, KP_SCSI_CTLR_START,
		sdp->sd_target, cdbp->cdb_opcode, cdbp->cdb_c10.c10_lba);
	scsi_expectintr(scp, sdp->sd_timeout);
} /* sc_dostart() */


static void
scgo(bcp)
register struct bus_ctrl *bcp;
{
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *)bcp->bc_addr;

#ifndef SC_53C90A
	s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
	s5rp->s5r_cmd = S5RCMD_TRANSFERINFO | S5RCMD_ENABLEDMA;
	s5rp->s5r_dmactrl = bcp->bc_cmd;
	/* FIXME: this timeout is questionable. We really should get this from
	 * sdp->sd_timeout...
	 */
	scsi_expectintr(&sc_sc[bcp->bc_ctrl], 10);
}

static void
scdmaintr(s5cp)
struct scsi_5390_ctrl *s5cp;
{
	struct scsi_ctrl *scp = s5cp->s5c_scp;
	int s;

	/*
	 * Only called if bus error occurred during dma.
	 * We're in a world of shit at this time...
	 * Check that the scsi interrupt didn't beat us;
	 * if not, just blow the command off completely.
	 *
	 * FIXME: floppy used this channel, too!
	 */
#ifdef SC_TRACE
	printf("sc%d: dma interrupt\n", s5cp->s5c_scp->sc_bcp->bc_ctrl);
#endif SC_TRACE
	XNDBG(("scdmaintr: interrupt\n"));
	s = splx(ipltospl(s5cp->s5c_scp->sc_ipl));
	/*
	 * Because scdmaintr is called by a software interrupt,
	 * a race exists with scintr to handle this error situation.
	 * DMACHAN_ERROR clear implies that scintr won the race.
	 */
	if (s5cp->s5c_dc.dc_flags & DMACHAN_ERROR)
		scintr(s5cp);
	splx(s);
}


static void
scintr(s5cp)
struct scsi_5390_ctrl *s5cp;
{
	struct scsi_ctrl *scp = s5cp->s5c_scp;
	volatile struct scsi_5390_regs *s5rp = (struct scsi_5390_regs *)
		scp->sc_bcp->bc_addr;
	struct scsi_device *sdp;
	int i, lun, target;
	extern volatile int *intrstat;
	static int old_s5c_state;

again:
	scsi_gotintr(scp);
	sdp = s5cp->s5c_cursdp;
	
	/* 01/18/89 dpm - coming here from the end of scintr as a result of
	 * reselect interrupt - sc_active may well be IDLE. We are no longer
	 * idle and we have to rememeber this ourself.
	 */
	scp->sc_active = SCACTIVE_INTR;
	if (s5cp->s5c_state == S5CSTATE_DMAING) {
		/* capture ms chip state and re-enter pio mode */
		DELAY(20);
		s5cp->s5c_dmastatus = s5rp->s5r_dmastatus;
		if (sdp->sd_read) {
			for (i = 0;
			     i < DMA_ENDALIGNMENT / S5RDMA_FIFOALIGNMENT;
			     i++) {
				s5rp->s5r_dmactrl = S5RDMAC_FLUSH
				    | S5RDMAC_NORMALBITS | S5RDMAC_DMAMODE
				    | S5RDMAC_DMAREAD;
				DELAY(5);
				s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS
				    | S5RDMAC_DMAMODE | S5RDMAC_DMAREAD;
				DELAY(5);
			}
			DELAY(20);
		}
		s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS;
	}

	/* capture status */
	s5cp->s5c_status = s5rp->s5r_status;
	s5cp->s5c_seqstep = s5rp->s5r_seqstep;
	s5cp->s5c_intrstatus = s5rp->s5r_intrstatus;
	XNDBG(("scintr: s5c_state %n\n",s5cp->s5c_state,s5c_state_values));
	XNDBG(("        s5c_status %r\n", s5cp->s5c_status, s5r_status_desc));
	XNDBG(("        intrstatus %r\n", s5cp->s5c_intrstatus,
	    s5r_intrstatus_desc));
	XNDBG(("        sc_active = %d\n",scp->sc_active));
	
	/*
	 * First handle interrupt status bits that indicate
	 * errors which keep the state machine from advancing
	 * normally.
	 */
	 
	/* 01/20/89 dpm - try to ignore illegal command once if state = 
	 * ACCEPTING_MSG. This allows us to handle the error condition 
	 * resulting from a select w/atn following a reselect (which we didn't 
	 * see because interrupts were disabled). This case is handle below in 
	 * case S5CSTATE_IGNOREILLCMD. (This kludge is enabled/disabled via 
	 * DO_IGNORE_ILLC.)
	 */

#ifdef	DO_IGNORE_ILLC	 
	if ((s5cp->s5c_status & S5RSTS_GROSSERROR)
	    || ((s5cp->s5c_intrstatus & S5RINT_ILLEGALCMD) && 
	        (s5cp->s5c_state != S5CSTATE_ACCEPTINGMSG)))
#else
	if ((s5cp->s5c_status & S5RSTS_GROSSERROR) || 
	    (s5cp->s5c_intrstatus & S5RINT_ILLEGALCMD))
#endif	DO_IGNORE_ILLC
	{
		/*
		 * We have to deal with an unusual race condition here:
		 * The problem is:
		 *   The 53C90 generates an interrupt because its
		 *    byte count goes to zero.
		 *   The target hasn't changed the SCSI phase lines to
		 *    "status" yet, so scfsm sees data out.
		 *   The target gets around to changing to status phase
		 *   Scfsm writes a TRANSFER PAD
		 *   The chip pukes because we're not in a "data" phase.
		 *
		 * The only way I currently know to handle this is to
		 * watch for it, and ignore the error.
		 * If it happens again, we'll screset("transfer len error")
		 * because the padcount should be zero.
		 *
		 * NOTE: I haven't seen this in a while -- it may have
		 * gone away with the "ifndef SC53C90A" errata fix.
		 */
		if (old_s5c_state != S5CSTATE_DMAING
		    || s5cp->s5c_state != S5CSTATE_PADDING) {
			/* OOPS, we screwed up bad! */
#if	DEBUG
			printf("old s5c state = %N\n", old_s5c_state,
			       s5c_state_values);
			scshowstatus(s5cp, "software error");
#endif	DEBUG
			scsetup(s5rp, scp->sc_target);
			screset(scp, SCRESET_ABORT, "software error");
			goto out;
		} else
			EPRINTF(("scsi race\n"));
	}
	old_s5c_state = s5cp->s5c_state;

	switch (s5cp->s5c_state) {
	case S5CSTATE_SELECTING:
		/*
		 * Determine if selection was successful;
		 * if so, go to initiator state and call fsm;
		 * if not, go to DISCONNECTED state.
		 */
		ASSERT(sdp != NULL);
		if (s5cp->s5c_intrstatus & S5RINT_DISCONNECT) {
			/*
			 * selection timed-out
			 */
			s5rp->s5r_cmd = S5RCMD_FLUSHFIFO;
			s5cp->s5c_state = S5CSTATE_DISCONNECTED;
			sdp->sd_state = SDSTATE_SELTIMEOUT;
			break;
		}
		if (s5cp->s5c_intrstatus==(S5RINT_FUNCCMPLT|S5RINT_BUSSERVICE))
		{
			XNDBG(("scintr: selection seqstep=%d\n",
			    s5cp->s5c_seqstep & S5RSEQ_SEQMASK));
			switch (s5cp->s5c_seqstep & S5RSEQ_SEQMASK) {
			case 0:	/* device doesn't handle ATN */
			case 2:	/* no cmd phase, probably a unit attention */
			case 3: /* didn't complete cmd phase, parity? */
			case 4: /* everything worked */
				s5cp->s5c_state = S5CSTATE_INITIATOR;
				sdp->sd_state = SDSTATE_INPROGRESS;
				break;
			default:
#if	DEBUG
				scshowstatus(s5cp, "select seq");
#endif	DEBUG
				screset(scp, SCRESET_ABORT, "select seq");
				break;
			}
			break;
		}
		sdp = NULL;
		goto check_reselect;
		break;
	case S5CSTATE_DISABLINGSEL:
	case S5CSTATE_RESELECTABLE:
check_reselect:
		/*
		 * DISABLINGSEL:
		 * Determine if DISABLE was successful or if we got
		 * reselected before we could disable.
		 * If we got selected, rather than reselected, disconnect
		 * the selection and disconnect.
		 *
		 * RESELECTABLE:
		 * Make sure that we've really been reselected;
		 * if so, get identify message from fifo, check if
		 * we have a disconnected thread, reestablish the thread,
		 * go to initiator state and call fsm.
		 */
		ASSERT(sdp == NULL);
		if (s5cp->s5c_intrstatus & (S5RINT_SELECTEDATN|S5RINT_SELECTED))
		{
			/*
			 * WE WERE SELECTED -- Nobody should do that!
			 * We're in target mode now, so refuse the selection
			 * by doing a disconnect.
			 */
			EPRINTF(("sc: externally selected\n"));
			s5rp->s5r_cmd = S5RCMD_DISCONNECT;
			s5cp->s5c_state = S5CSTATE_DISCONNECTED;
		} else if (s5cp->s5c_intrstatus & S5RINT_RESELECTED) {
			/*
			 * We've been reselected, set state to
			 * S5CSTATE_INITIATOR, look in the fifo
			 * to find out who reselected us.
			 */
			/* make sure there's an identify message in the fifo */
			
			/* 01/19/89 dpm - per NCR documentation, both of these 
			 * tests could fail in the case where we were 
			 * reselected while trying to select.
			 */
			 
			/*....
			if ((s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) != 2
			   || (s5cp->s5c_intrstatus & S5RINT_FUNCCMPLT) == 0) {
				EPRINTF(("sc: reselection failed - no ID msg\n"));
				goto bad_reselect;
			}
			...*/
			
			/* make sure target set his bit */
			if ((i = s5rp->s5r_fifo &~ (1 << scp->sc_target)) == 0) {
				EPRINTF(("sc: reselection failed - no target bit\n"));
				goto bad_reselect;
			}
			/* figure out target from bit that's on */
			/* 01/17/89 dpm...*/
			for (target = 0; (i & 1) == 0; target++, i>>=1)
				continue;
getting_lun:
			/* make sure target sent an identify msg */
			i = s5rp->s5r_fifo;
			s5cp->s5c_curmsg = i;
			if (s5cp->s5c_status & S5RSTS_PARITYERROR) {
				EPRINTF(("sc: reselected parity error\n"));
				s5cp->s5c_target = target;
				if (++s5cp->s5c_errcnt > MAX_S5CERRS) {
					screset(scp, SCRESET_ABORT,
					    "scsi bus parity on reselect");
					break;
				}
				/* cancel message */
				s5cp->s5c_curmsg = MSG_NOP;
				scmsgout(s5cp, MSG_MSGPARERR,
				    S5CSTATE_INITIATOR);
				s5cp->s5c_state = S5CSTATE_ACCEPTINGMSG;
				s5rp->s5r_cmd = S5RCMD_MSGACCEPTED;
				scsi_expectintr(scp, sdp->sd_timeout);
				break;
			}
			if ((i & MSG_IDENTIFYMASK) == 0) {
				EPRINTF(("sc: reselection failed - bad msg byte\n"));
				goto bad_reselect;
			}
			lun = i & MSG_ID_LUNMASK;
			sdp = scsi_reselect(scp, target, lun);
			if (sdp != NULL) {
				/*
				 * Found a disconnected scsi_device to
				 * reconnect.
				 *
				 * IDENTIFY msg implies restore ptrs
				 */
				scsi_gotreselect(sdp);
				s5cp->s5c_cursdp = sdp;
				s5cp->s5c_curaddr = sdp->sd_addr;
				s5cp->s5c_curbcount = sdp->sd_bcount;
				s5cp->s5c_curpadcount = sdp->sd_padcount + 1;
				s5cp->s5c_state = S5CSTATE_ACCEPTINGMSG;
				s5rp->s5r_cmd = S5RCMD_MSGACCEPTED;
				pmon_log_event(PMON_SOURCE_SCSI,
					KP_SCSI_CTLR_RSLCT, sdp->sd_target,
					sdp->sd_cdb.cdb_opcode,
					sdp->sd_cdb.cdb_c10.c10_lba);
				scsi_expectintr(scp, sdp->sd_timeout);
			} else {
				EPRINTF(("sc: reselection failed - device not disconnected\n"));
bad_reselect:
				/* cancel message */
				s5cp->s5c_curmsg = MSG_NOP;
				if (s5cp->s5c_intrstatus & S5RINT_FUNCCMPLT) {
					scmsgout(s5cp, MSG_ABORT,
					    S5CSTATE_DISCONNECTED);
					s5cp->s5c_state = S5CSTATE_ACCEPTINGMSG;
					s5rp->s5r_cmd = S5RCMD_MSGACCEPTED;
				} else
					screset(scp, SCRESET_ABORT,
					    "bad reselect");
			}
		} else if (s5cp->s5c_state == S5CSTATE_DISABLINGSEL) {
			/* disable succeeded */
			s5cp->s5c_state = S5CSTATE_DISCONNECTED;
		} else {/* I'm confused.... */
#if	DEBUG
			scshowstatus(s5cp, "bad reselection");
#endif	DEBUG
			screset(scp, SCRESET_ABORT, "bad reselection");
		}
		break;
	case S5CSTATE_INITIATOR:
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		EPRINTF(("interrupt as initiator\n"));
#ifdef notdef
		/*
		 * I don't think this should ever happen
		 */
		scshowstatus(s5cp, "interrupt as initiator");
		screset(scp, SCRESET_ABORT, "interrupt as initiator");
#endif notdef
		break;
	case S5CSTATE_DMAING:
		ASSERT(sdp != NULL);
		i = s5cp->s5c_curbcount;
		s5cp->s5c_curbcount = ((s5rp->s5r_cntmsb<<8)|s5rp->s5r_cntlsb);
		
		/* 01/17/89 dpm - if this was a data out, we must subtract the 
		 * number of bytes in the fifo from the total transfer. The
		 * data in the fifo is not going to go to the target, so we 
		 * don't count it here...
		 */
		 
		if(!sdp->sd_read) 
			s5cp->s5c_curbcount += (s5rp->s5r_fifoflags & 
						S5RFLG_FIFOLEVELMASK);
		i -= s5cp->s5c_curbcount;	/* actual bytes transfered */
		sdp->sd_bytes_moved += i;
		s5cp->s5c_curaddr += i;
		XNDBG(("scintr: remaining bcount=0x%x i=0x%x "
			"sd_bytes_moved=0x%x\n", s5cp->s5c_curbcount, i,
			sdp->sd_bytes_moved));
		s5cp->s5c_state = S5CSTATE_INITIATOR;
		dma_cleanup(&s5cp->s5c_dc, s5cp->s5c_curbcount);
		busdone(scp->sc_bcp);
		if ((s5cp->s5c_dc.dc_flags & DMACHAN_ERROR)
		    || (s5cp->s5c_status & S5RSTS_PARITYERROR)) {
			/*
			 * FIXME: we should clear any pending softint's
			 * to scdmaintr here.  Otherwise, they come
			 * in at a later date and confuse us.
			 */
			EPRINTF(("sc: data transfer error\n"));
			EPRINTF(("sc: status: %R dc_flags: 0x%x\n",
			    s5cp->s5c_status, s5r_status_desc,
			    s5cp->s5c_dc.dc_flags));
			/*
			 * DMA channel errors are tough to recover
			 * from... we don't
			 */
			if (s5cp->s5c_dc.dc_flags & DMACHAN_ERROR)
				s5cp->s5c_errcnt = MAX_S5CERRS+1;
			s5cp->s5c_dc.dc_flags &= ~DMACHAN_ERROR;
			if (++s5cp->s5c_errcnt > MAX_S5CERRS) {
				screset(scp, SCRESET_ABORT,
				    (s5cp->s5c_dc.dc_flags & DMACHAN_ERROR)
				      ? "dma error"
				      : "scsi bus parity error during dma");
				break;
			}
			scmsgout(s5cp, MSG_IDETERR, S5CSTATE_INITIATOR);
		}
		break;
	case S5CSTATE_PADDING:
		ASSERT(sdp != NULL);
		i = s5cp->s5c_curpadcount;
		s5cp->s5c_curpadcount =
		    ((s5rp->s5r_cntmsb << 8) | s5rp->s5r_cntlsb);
		/* note the kludge: pad counts are always 15 bytes larger than
		 * they should be for writes; only do this if SOME pad bytes
		 * actually moved...
		 */
		i -= s5cp->s5c_curpadcount;
		sdp->sd_bytes_moved += i;
		if((!sdp->sd_read) && i)
			sdp->sd_bytes_moved -= 0xF; 
		XNDBG(("scintr: remaining padcount=%d\n", 
			s5cp->s5c_curpadcount));
		s5cp->s5c_state = S5CSTATE_INITIATOR;
		break;
	case S5CSTATE_COMPLETING:
		ASSERT(sdp != NULL);
		if (s5cp->s5c_intrstatus & S5RINT_DISCONNECT) {
			EPRINTF(("sc: unexpected completing disconnect\n"));
			break;
		}
		if (s5cp->s5c_intrstatus & S5RINT_FUNCCMPLT) {
			/*
			 * Got both status and msg
			 */
			if ((s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) != 2)
			    {
				/*
				 * We expect a status and msg in the
				 * fifo.  We must have a misunderstanding
				 * about the 53C90!
				 */
#if	DEBUG
				scshowstatus(s5cp, "fifo level");
#endif	DEBUG
				screset(scp, SCRESET_ABORT, "fifo level");
				break;
			}
			sdp->sd_status = s5rp->s5r_fifo;
			s5cp->s5c_curmsg = s5rp->s5r_fifo;
			XNDBG(("scintr: COMPLETING: status %n   msg %n\n",
				sdp->sd_status,scsi_status_values,
				s5cp->s5c_curmsg,scsi_msg_values));
			if (s5cp->s5c_status & S5RSTS_PARITYERROR) {
				EPRINTF(("sc: parity error on msg\n"));
				if (++s5cp->s5c_errcnt > MAX_S5CERRS) {
					screset(scp, SCRESET_ABORT,
					    "scsi bus parity on msg");
					break;
				}
				/* cancel message */
				s5cp->s5c_curmsg = MSG_NOP;
				scmsgout(s5cp, MSG_MSGPARERR,
				    S5CSTATE_INITIATOR);
				s5cp->s5c_state = S5CSTATE_ACCEPTINGMSG;
				s5rp->s5r_cmd = S5RCMD_MSGACCEPTED;
				scsi_expectintr(scp, sdp->sd_timeout);
				break;
			}
			s5cp->s5c_state = S5CSTATE_INITIATOR;
			scmsgin(s5cp);
		} else {
			/*
			 * Must have just got a status byte only
			 */
			EPRINTF(("sc: status only on complete\n"));
			if ((s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) != 1) {
#if	DEBUG
				scshowstatus(s5cp, "fifo level2");
#endif	DEBUG
				screset(scp, SCRESET_ABORT, "fifo level2");
				break;
			}
			sdp->sd_status = s5rp->s5r_fifo;
			XNDBG(("scintr: status %n\n", sdp->sd_status,
			    scsi_status_values));
			if (s5cp->s5c_status & S5RSTS_PARITYERROR) {
				EPRINTF(("sc: parity error on status\n"));
				if (++s5cp->s5c_errcnt > MAX_S5CERRS) {
					screset(scp, SCRESET_ABORT,
					    "scsi bus parity on status");
					break;
				}
				scmsgout(s5cp, MSG_IDETERR, S5CSTATE_INITIATOR);
			}
			s5cp->s5c_state = S5CSTATE_INITIATOR;
		}
		break;
	case S5CSTATE_GETTINGLUN:
		ASSERT(sdp == NULL);
		if (s5cp->s5c_intrstatus & S5RINT_DISCONNECT) {
			EPRINTF(("sc: getting lun disconnect\n"));
			break;
		}
		if ((s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) == 1) {
			target = s5cp->s5c_target;
			goto getting_lun;
		}
		screset(scp, SCRESET_ABORT, "msgin fifo error");
		break;
	case S5CSTATE_GETTINGMSG:
		ASSERT(sdp != NULL);
		if (s5cp->s5c_intrstatus & S5RINT_DISCONNECT) {
			EPRINTF(("sc: getting msg disconnect\n"));
			break;
		}
		if ((s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) != 1) {
			screset(scp, SCRESET_ABORT, "msgin fifo error");
			break;
		}
		/*
		 * FIXME: Extended messages
		 */
		s5cp->s5c_curmsg = s5rp->s5r_fifo;
		if (s5cp->s5c_status & S5RSTS_PARITYERROR) {
			EPRINTF(("sc: parity error on msg\n"));
			if (++s5cp->s5c_errcnt > MAX_S5CERRS) {
				screset(scp, SCRESET_ABORT,
				    "scsi bus parity on msg2");
				break;
			}
			/* cancel message */
			s5cp->s5c_curmsg = MSG_NOP;
			scmsgout(s5cp, MSG_MSGPARERR, S5CSTATE_INITIATOR);
			s5cp->s5c_state = S5CSTATE_ACCEPTINGMSG;
			s5rp->s5r_cmd = S5RCMD_MSGACCEPTED;
			scsi_expectintr(scp, sdp->sd_timeout);
			break;
		}
		scmsgin(s5cp);
		break;
	case S5CSTATE_ACCEPTINGMSG:
#ifdef	DO_IGNORE_ILLC	 
	case S5CSTATE_IGNOREILLCMD:
#endif	DO_IGNORE_ILLC
		/*
		 * We've told target that we've accepted his message,
		 * now figure out what to do next.
		 *
		 * 01/20/89 dpm - illegal command at this point may mean that 
		 * we have been reselected...check it out. Note that we can we 
		 * can not be at S5CSTATE_IGNOREILLCMD if S5RINT_ILLEGALCMD is
		 * true; there  was a trap for that at the top of the routine.
		 */
		 
#ifdef	DO_IGNORE_ILLC	 
		if(s5cp->s5c_intrstatus & S5RINT_ILLEGALCMD)  {
		
			/* ah hah. Ignore and try again. */
			
			s5cp->s5c_state = S5CSTATE_IGNOREILLCMD;
			s5rp->s5r_cmd = S5RCMD_MSGACCEPTED;
			scsi_expectintr(scp, 3);
			break;
		}
#endif 	DO_IGNORE_ILLC
		switch (s5cp->s5c_curmsg) {
		case MSG_CMDCMPLT:
			ASSERT(sdp != NULL);
			s5cp->s5c_state = S5CSTATE_DISCONNECTED;
			sdp->sd_resid = s5cp->s5c_curbcount;
			sdp->sd_state = SDSTATE_COMPLETED;
			pmon_log_event(PMON_SOURCE_SCSI, KP_SCSI_CTLR_CMPL,
				sdp->sd_target, sdp->sd_cdb.cdb_opcode,
				sdp->sd_cdb.cdb_c10.c10_lba);
			break;
		case MSG_DISCONNECT:
			/*
			 * FIXME: this is pretty gross....
			 * We could MODESENSE for geometry and
			 * keep track of cylinders....
			 */
			ASSERT(sdp != NULL);
#ifdef notdef
			if (sdp->sd_bdp->bd_dk >= 0)
				dk_seek[sdp->sd_bdp->bd_dk]++;
#endif notdef
			s5cp->s5c_state = S5CSTATE_DISCONNECTED;
			pmon_log_event(PMON_SOURCE_SCSI, KP_SCSI_CTLR_DISC,
				sdp->sd_target, sdp->sd_cdb.cdb_opcode,
				sdp->sd_cdb.cdb_c10.c10_lba);
			scsi_expectreselect(sdp);
			sdp->sd_state = SDSTATE_DISCONNECTED;
			break;
		default:
			s5cp->s5c_state = S5CSTATE_INITIATOR;
			s5rp->s5r_cmd = S5RCMD_NOP;			
			s5rp->s5r_cmd = S5RCMD_FLUSHFIFO; /* dpm 01/20/89 */		
			break;
		}
		break;
		
	case S5CSTATE_SENDINGMSG:
		ASSERT(sdp != NULL);
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		s5cp->s5c_state = s5cp->s5c_postmsgstate;
		break;
	case S5CSTATE_DISCONNECTED:
		ASSERT(sdp == NULL);
		scsetup(s5rp, scp->sc_target);
		screset(scp, SCRESET_ABORT, "stray interrupt");
		return;
	default:
#if	DEBUG
		scshowstatus(s5cp, "scintr: bad state");
#endif	DEBUG
		panic("scintr: bad state");
		
	} /* switch s5c_state */
	
	if (s5cp->s5c_state != S5CSTATE_DISCONNECTED
	    && (s5cp->s5c_intrstatus & S5RINT_DISCONNECT)) {
		/*
		 * the target just up and went away...
		 */
		EPRINTF(("sc: target disconnected\n"));
		s5cp->s5c_state = S5CSTATE_DISCONNECTED;
		/* This should always be true, but... */
		if ((sdp = s5cp->s5c_cursdp) != NULL)
			sdp->sd_state = SDSTATE_DROPPED;
	}
	if (s5cp->s5c_state == S5CSTATE_INITIATOR)
		scfsm(s5cp);
out:
	pmon_log_event(PMON_SOURCE_SCSI, KP_SCSI_CTLR_INTR,
		s5cp->s5c_cursdp != NULL ? s5cp->s5c_cursdp->sd_target : -1,
		old_s5c_state, s5cp->s5c_state);
	/*
	 * if we're off the bus, enable reselection at h/w level (dpm)
	 */
#ifdef	EARLYRESEL_ENABLE
	if (s5cp->s5c_state == S5CSTATE_DISCONNECTED) {
		XNDBG(("scintr: enabling reselection\n"));
		s5rp->s5r_cmd = S5RCMD_ENABLESELECT;
	}
#endif	EARLYRESEL_ENABLE
	
	/*
	 * If controller idle and not a spurious interrupt,
	 * restart controller
	 */
	if (s5cp->s5c_state == S5CSTATE_DISCONNECTED
	    && s5cp->s5c_cursdp != NULL) {
		s5cp->s5c_cursdp = NULL;
		XNDBG(("scintr: restart via scsi_cintr()\n"));
		XNDBG(("        s5c_state = S5CSTATE_DISCONNECTED\n"));
		scsi_cintr(scp);
	}
	/*
	 * if we're still DISCONNECTED,
	 * ENABLE_RESELECTION
	 */
	if (s5cp->s5c_state == S5CSTATE_DISCONNECTED) {
		s5cp->s5c_state = S5CSTATE_RESELECTABLE;
		XNDBG(("scintr: s5c_state = S5CSTATE_RESELECTABLE\n"));
#ifndef	EARLYRESEL_ENABLE
		s5rp->s5r_cmd = S5RCMD_ENABLESELECT;
#endif 	EARLYRESEL_ENABLE
	}
	XNDBG(("scintr: exit state %n\n", s5cp->s5c_state, s5c_state_values));
	XNDBG(("        sc_active = %d\n",scp->sc_active));

	/*
	 * check for another pending interrupt here, if there
	 * is one go back to the top and start over
	 */
	if (*intrstat & I_BIT(I_SCSI)) {
		XNDBG(("scintr: exit intr present\n"));
		goto again;
	}
	
	/*
	 * last thing: if no I/O is active and no devices are disconnected,
	 * relinquish hardware to give floppy a chance.
	 */
	if((s5cp->s5c_state == S5CSTATE_RESELECTABLE) &&
	   (s5cp->s5c_scp->sc_disc_cnt == 0)) {
	   	XNDBG(("scintr: RELINQUISHING hardware\n"));
		sfa_relinquish(s5cp->s5c_sfah, &s5cp->s5c_sfad, SF_LD_SCSI);
	}
		
} /* scintr() */

static void
scfsm(s5cp)
struct scsi_5390_ctrl *s5cp;
{
	volatile struct scsi_5390_regs *s5rp = (struct scsi_5390_regs *)
		s5cp->s5c_scp->sc_bcp->bc_addr;
	struct scsi_device *sdp = s5cp->s5c_cursdp;
	struct scsi_ctrl *scp = s5cp->s5c_scp;
	union cdb *cdbp = &sdp->sd_cdb;
	int identify_msg, cmdlen, phase;
	char *cp;

	/*
	 * Advance msg out state machine -- SCSI spec says if
	 * we do a msg out phase and then see another phase
	 * we can assume msg was transmitted without error
	 */
	if ((s5cp->s5c_status & S5RSTS_PHASEMASK) != PHASE_MSGOUT
	    && s5cp->s5c_msgoutstate == S5CMSGSTATE_SAWMSGOUT)
		s5cp->s5c_msgoutstate = S5CMSGSTATE_NONE;

	/* make sure we start off with a clean slate */
	s5rp->s5r_cmd = S5RCMD_FLUSHFIFO;

	/*
	 * sdp can be NULL if we got a parity error during
	 * the identify message of a reselection -- we deal with
	 * that situation specially and only accept a MSGIN phase
	 */
	phase = (s5cp->s5c_status & S5RSTS_PHASEMASK);
	XNDBG(("scfsm:  phase = %n\n", phase, scsi_phase_values));
	if (sdp == NULL && phase != PHASE_MSGIN) {
		screset(scp, SCRESET_ABORT, "no connection");
		return;
	}

	switch (phase) {

	default:
	case PHASE_COMMAND:
		EPRINTF(("sc: command phase\n"));
		cmdlen = s5cp->s5c_cmdlen;
		for (cp = (char *)cdbp; cmdlen-- > 0;)
			s5rp->s5r_fifo = *cp++;
		/*
		 * fill fifo to avoid spurious command phase for target
		 * chips that try to get max command length
		 */
		for (cmdlen = 14 - s5cp->s5c_cmdlen; cmdlen > 0; cmdlen--)
			s5rp->s5r_fifo = 0;
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		s5rp->s5r_cmd = S5RCMD_TRANSFERINFO;
		s5cp->s5c_state = S5CSTATE_INITIATOR;
		scsi_expectintr(scp, sdp->sd_timeout);
		break;

	case PHASE_DATAOUT:	/* To Target from Initiator (write) */
		if (sdp->sd_read) {
			screset(scp, SCRESET_ABORT, "bad i/o direction");
			break;
		}
		scdmastart(s5cp, DMACSR_WRITE);
		break;
	case PHASE_DATAIN:	/* From Target to Initiator (read) */
		if (!sdp->sd_read) {
			screset(scp, SCRESET_ABORT, "bad i/o direction");
			break;
		}
		scdmastart(s5cp, DMACSR_READ);
		break;
	case PHASE_STATUS:	/* Status from Target to Initiator */
		/*
		 * scintr() will collect the STATUS byte
		 * (and hopefully a MSG) from the fifo when this
		 * completes.
		 */
		s5cp->s5c_state = S5CSTATE_COMPLETING;
		s5rp->s5r_cmd = S5RCMD_INITCMDCMPLT;
		scsi_expectintr(scp, sdp->sd_timeout);
		break;
	case PHASE_MSGIN:	/* Message from Target to Initiator */
		/*
		 * If we don't have an sdp setup, we must have got
		 * a parity error during reselection, so try getting
		 * the message.  If it's not an identify, we'll
		 * abort.
		 */
		if (sdp == NULL)
			s5cp->s5c_state = S5CSTATE_GETTINGLUN;
		else
			s5cp->s5c_state = S5CSTATE_GETTINGMSG;
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		s5rp->s5r_cmd = S5RCMD_TRANSFERINFO;
		scsi_expectintr(scp, sdp->sd_timeout);
		break;
	case PHASE_MSGOUT:	/* Message from Initiator to Target */
		EPRINTF(("sc: msg out phase\n"));
		if (s5cp->s5c_msgoutstate != S5CMSGSTATE_NONE) {
			s5rp->s5r_fifo = s5cp->s5c_msgout;
			s5cp->s5c_msgoutstate = S5CMSGSTATE_SAWMSGOUT;
		} else {
			/*
			 * Target went to msg out and we don't have
			 * anything to send!  Just give it a nop.
			 */
			s5rp->s5r_fifo = MSG_NOP;
			s5cp->s5c_postmsgstate = S5CSTATE_INITIATOR;
		}

		s5cp->s5c_state = S5CSTATE_SENDINGMSG;
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		/* ATN is automatically cleared when transfer info completes */
		s5rp->s5r_cmd = S5RCMD_TRANSFERINFO;
		scsi_expectintr(scp, sdp->sd_timeout);
		break;
	}
} /* scfsm() */

static void
scdmastart(s5cp, direction)
struct scsi_5390_ctrl *s5cp;
int direction;
{
	volatile struct scsi_5390_regs *s5rp = (struct scsi_5390_regs *)
		s5cp->s5c_scp->sc_bcp->bc_addr;
	struct scsi_device *sdp = s5cp->s5c_cursdp;
	struct scsi_ctrl *scp = s5cp->s5c_scp;

	XNDBG(("scdmastart: bcount 0x%x padcount 0x%x\n", s5cp->s5c_curbcount,
	    s5cp->s5c_curpadcount));
	if (s5cp->s5c_curbcount == 0) {
		/* nothing else to send, just transfer pad */
		if (s5cp->s5c_curpadcount == 0) {
			/* pad exhausted, too */
			screset(scp, SCRESET_ABORT, "transfer len exceeded");
			return;
		}
		s5cp->s5c_state = S5CSTATE_PADDING;
		/* fifo is sent on write pads */
		if (direction == DMACSR_WRITE)
			s5rp->s5r_fifo = 0;
		s5rp->s5r_cntlsb = s5cp->s5c_curpadcount;
		s5rp->s5r_cntmsb = s5cp->s5c_curpadcount >> 8;
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		/* this doesn't really use dma, so no busgo() here */
		s5rp->s5r_cmd = S5RCMD_TRANSFERPAD | S5RCMD_ENABLEDMA;
		return;
	}
	s5cp->s5c_state = S5CSTATE_DMAING;
	if (! DMA_BEGINALIGNED(s5cp->s5c_curaddr)) {
		screset(scp, SCRESET_ABORT, "unaligned DMA segment");
		return;
	}
	if (s5cp->s5c_curbcount > 0x10000) {
		screset(scp, SCRESET_ABORT, "DMA len > 64K");
		return;
	}
	dma_list(&s5cp->s5c_dc, s5cp->s5c_dl, s5cp->s5c_curaddr,
	    s5cp->s5c_curbcount, sdp->sd_pmap, direction, NDMAHDR, 0, 0);
	dma_start(&s5cp->s5c_dc, s5cp->s5c_dl, direction);
	s5rp->s5r_cntlsb = s5cp->s5c_curbcount;
	s5rp->s5r_cntmsb = s5cp->s5c_curbcount >> 8;
	scp->sc_bcp->bc_cmd = (direction == DMACSR_READ)
	    ? (S5RDMAC_NORMALBITS | S5RDMAC_DMAMODE | S5RDMAC_DMAREAD)
	    : (S5RDMAC_NORMALBITS | S5RDMAC_DMAMODE);
	busgo(sdp->sd_bdp);
} /* scdmastart() */

unsigned
scminphys(bp)
	struct buf *bp;
{
	extern int maxphys;
	
	if (bp->b_bcount > maxphys)
		bp->b_bcount = maxphys;
}

static void
scmsgin(s5cp)
struct scsi_5390_ctrl *s5cp;
{
	struct scsi_ctrl *scp = s5cp->s5c_scp;
	volatile struct scsi_5390_regs *s5rp = (struct scsi_5390_regs *)
		scp->sc_bcp->bc_addr;
	struct scsi_device *sdp = s5cp->s5c_cursdp;

	/* make sure we're connected to someone */
	if (sdp == NULL) {
#if	DEBUG
		scshowstatus(s5cp, "scmsgin: no current sd");
#endif	DEBUG
		panic("scmsgin: no current sd");
	}
	/*
	 * if msgout is pending, ignore incoming messages
	 * they might have a parity error
	 */
	XNDBG(("scmsgin: msg = %N msgoutstate = %d\n", s5cp->s5c_curmsg,
	    scsi_msg_values, s5cp->s5c_msgoutstate));
	if (s5cp->s5c_msgoutstate == S5CMSGSTATE_WAITING)
		s5cp->s5c_curmsg = MSG_NOP;
	else {
		switch (s5cp->s5c_curmsg) {
		case MSG_CMDCMPLT:
			/*
			 * this is handled after we tell the
			 * target that the message has been accepted
			 * (since we need to maintain cursdp until that's
			 * accomplished)
			 */
			break;
		case MSG_DISCONNECT:
			if ((! sdp->sd_disconnectok) || 
			    (! s5cp->s5c_53c90a)) {
				/* cancel message */
				s5cp->s5c_curmsg = MSG_NOP;
				scmsgout(s5cp, MSG_MSGREJECT,
					 S5CSTATE_INITIATOR);
			}
			break;
		case MSG_SAVEPTRS:
			sdp->sd_addr = s5cp->s5c_curaddr;
			sdp->sd_bcount = sdp->sd_resid = s5cp->s5c_curbcount;
			sdp->sd_padcount = s5cp->s5c_curpadcount - 1;
			break;
		case MSG_RESTOREPTRS:
			s5cp->s5c_curaddr = sdp->sd_addr;
			s5cp->s5c_curbcount = sdp->sd_bcount;
			s5cp->s5c_curpadcount = sdp->sd_padcount + 1;
			break;
		case MSG_MSGREJECT:
			/*
			 * we wouldn't ask a target to do anything
			 * unreasonable; so if it refuses, we give up
			 */
			printf("sc: MESSAGE REJECT RECEIVED\n");
#ifndef	ALLOW_MSG_REJECT
			/* 05/23/90 - setting postmsgstate to 
			 * S5CSTATE_DISCONNECTED causes us to miss the 
			 * "target dropped connection" interrupt/state
			 * transition if it happens after the
			 * Abort message. Let's just stay in the initiator
			 * state and let the target do its thing...
			 */
			scmsgout(s5cp, MSG_ABORT, S5CSTATE_DISCONNECTED);
#endif	ALLOW_MSG_REJECT
			break;
		case MSG_LNKCMDCMPLT:
		case MSG_LNKCMDCMPLTFLAG:
			/*
			 * Someday maybe we should handle linked commands....
			 *
			 * This should never happen, because scstart trashes
			 * commands with the LINK bit on.
			 */
			screset(scp, SCRESET_ABORT, "Linked command");
			break;
		default:
			if (s5cp->s5c_curmsg & MSG_IDENTIFYMASK) {
				/* this shouldn't happen */
				EPRINTF(("scmsgin: got IDENTIFY msg\n"));
				break;
			}
			scmsgout(s5cp, MSG_MSGREJECT, S5CSTATE_INITIATOR);
			break;
		}
	}
	/* We assume that we always have to do a S5RCMD_MSGACCEPTED */
	if ((s5cp->s5c_intrstatus & S5RINT_FUNCCMPLT) == 0) {
#if	DEBUG
		scshowstatus(s5cp, "scmsgin: no FUNCCMPLT");
#endif	DEBUG
		screset(scp, SCRESET_ABORT, "scmsgin: no FUNCCMPLT");
		return;
	}
	s5cp->s5c_state = S5CSTATE_ACCEPTINGMSG;
	s5rp->s5r_cmd = S5RCMD_MSGACCEPTED;
	scsi_expectintr(scp, sdp->sd_timeout);
} /* scmsgin() */

static void
scmsgout(s5cp, msg, postmsgstate)
struct scsi_5390_ctrl *s5cp;
u_char msg;
u_char postmsgstate;
{
	volatile struct scsi_5390_regs *s5rp = (struct scsi_5390_regs *)
		s5cp->s5c_scp->sc_bcp->bc_addr;

	EPRINTF(("sc: doing msg out\n"));
	XNDBG(("scmsgout: msg out %N\n", msg, scsi_msg_values));
	s5cp->s5c_msgout = msg;
	s5cp->s5c_postmsgstate = postmsgstate;
	s5cp->s5c_msgoutstate = S5CMSGSTATE_WAITING;
	s5rp->s5r_cmd = S5RCMD_SETATN;
}

static void
screset(scp, abortflag, why)
struct scsi_ctrl *scp;
int abortflag;
char *why;
{
	struct scsi_5390_ctrl *s5cp = &sc_s5c[scp->sc_bcp->bc_ctrl];
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *)s5cp->s5c_scp->sc_bcp->bc_addr;
	struct scsi_device *sdp;
	int notify_flag=0;
	
	XNDBG(("screset\n"));
	/*
	 * If any dma was in progress, abort it and release the bus.
	 * Reset the SCSI bus.  If there's a current scsi_device
	 * connected mark it disconnected.  Finally, restart all
	 * disconnected I/O.
	 */
	s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS;
	dma_abort(&s5cp->s5c_dc);
	s5cp->s5c_dc.dc_flags &= ~DMACHAN_ERROR;
	if (s5cp->s5c_state == S5CSTATE_DMAING)
		busdone(s5cp->s5c_scp->sc_bcp);
	if (s5cp->s5c_state != S5CSTATE_DISCONNECTED) {
		s5rp->s5r_cmd = S5RCMD_RESETBUS;
		DELAY(500000);	/* 500 ms to let targets cleanup */
		s5cp->s5c_state = S5CSTATE_DISCONNECTED;
		notify_flag++;
	}
	/*
	 * Release hardware.
	 */
   	XNDBG(("screset: RELINQUISHING hardware\n"));
	sfa_abort(s5cp->s5c_sfah, &s5cp->s5c_sfad, SF_LD_SCSI);
	if ((sdp = s5cp->s5c_cursdp) != NULL) {
		sdp->sd_state = abortflag == SCRESET_ABORT
		    ? SDSTATE_ABORTED : SDSTATE_RETRY;
		s5cp->s5c_cursdp = NULL;
		if(notify_flag) {
			/* 
			 * one more thing - notify device level that we are
			 *  through with this command. (07/21/89 DPM)
			 */
			scsi_cintr(scp);
		}
	}
	if (why != NULL)
		EPRINTF(("sc%d: SCSI bus reset: %s\n",
		    s5cp->s5c_scp->sc_bcp->bc_ctrl, why));
	/*
	 * No matter what, no outstanding reselects exist now.
	 */
	s5cp->s5c_scp->sc_disc_cnt = 0;
	scsi_restart(s5cp->s5c_scp);
} /* screset() */

static int
scioctl(scp, cmd, data, flag)
struct scsi_ctrl *scp;
int cmd;
caddr_t data;
int flag;
{
	switch (cmd) {
	/*
	 * No controller level ioctl's currently, maybe
	 * someday will add one to set slow cable mode.
	 */
	default:
		return(ENOTTY);
	}
}

#if	DEBUG || XPR_DEBUG
void
scshowstatus(s5cp, why)
struct scsi_5390_ctrl *s5cp;
char *why;
{
	struct scsi_ctrl *scp = s5cp->s5c_scp;
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *)scp->sc_bcp->bc_addr;
	int ctrl = scp->sc_bcp->bc_ctrl;
	struct scsi_device *sdp;

	printf("sc%d: %s\n", ctrl, why);
	printf("sc%d: s5c state = %N\n", ctrl, s5cp->s5c_state,
	       s5c_state_values);
	printf("sc%d: status = %R\n", ctrl, s5cp->s5c_status, s5r_status_desc);
	printf("sc%d: intrstatus = %R\n", ctrl, s5cp->s5c_intrstatus, 
	    s5r_intrstatus_desc);
	printf("sc%d: seqstep = 0x%x\n", ctrl,
	    s5cp->s5c_seqstep & S5RSEQ_SEQMASK);
	printf("sc%d: fifo level = %d\n", ctrl,
	    s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK);
	printf("sc%d: transfer count = %d\n", ctrl,
	    s5rp->s5r_cntlsb | (s5rp->s5r_cntmsb << 8));
	printf("sc%d: command = %R\n", ctrl, s5rp->s5r_cmd, s5r_cmd_desc);
	printf("sc%d: config = %R\n", ctrl, s5rp->s5r_config, s5r_config_desc);
	printf("sc%d: dma ctrl = %R\n", ctrl, s5rp->s5r_dmactrl,
	    s5r_dmactrl_desc);
	printf("sc%d: dma status = %R\n", ctrl, s5rp->s5r_dmastatus,
	    s5r_dmastatus_desc);
	if ((sdp = s5cp->s5c_cursdp) != NULL)
		scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "CURSDP");
}
#endif	DEBUG || XPR_DEBUG

#endif	NSC

/* end of sc.c */




