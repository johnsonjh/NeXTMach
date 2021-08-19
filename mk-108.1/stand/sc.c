/*	@(#)sc.c	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * sc.c -- implementation of NCR53C90 specific scsi routines
 * STANDALONE MONITOR VERSION
 *
 * HISTORY
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#import <sys/param.h>
#import <next/cpu.h>
#import <stand/scsireg.h>
#import <stand/scsivar.h>
#import <stand/screg.h>
#import <nextdev/dma.h>
#import <nextdev/fd_reg.h>
#import <mon/global.h>

extern struct mon_global *restore_mg();

/* #define	SC_53C90A	1	/* No 53C90A chip, yet */
/* #define	SC_DEBUG	1	/* */

#ifndef NULL
#define	NULL		0
#endif !NULL

#define	SCSI_CSR	P_SCSI

/*
 * private per-5390-chip info
 * this struct parallels scsi_ctrl and bus_ctrl structs
 */
struct scsi_5390_ctrl {			/* per controller info */
	struct	dma_chan s5c_dc;	/* DMA info */
	dma_list_t s5c_dl;		/* DMA segment info */
	u_char	s5c_state;		/* controller state */
	/* saved interrupt state */
	u_char	s5c_status;		/* last 5390 status */
	u_char	s5c_seqstep;		/* last 5390 seqstep */
	u_char	s5c_intrstatus;		/* last 5390 intrstatus */
	u_char	s5c_dmastatus;		/* mass-storage chip status */
	/* state of currently connected target */
	struct scsi_device *s5c_cursdp;
	caddr_t	s5c_curaddr;		/* current address of dma */
	long	s5c_curbcount;		/* current remaining bytes for dma */
	u_char	s5c_curmsg;		/* current command msg */
} sc_s5c;

#define	S5CSTATE_DISCONNECTED	0	/* disconnected & not reselectable */
#define	S5CSTATE_SELECTING	1	/* SELECT* command issued */
#define	S5CSTATE_INITIATOR	2	/* following target SCSI phase */
#define	S5CSTATE_COMPLETING	3	/* initiator cmd cmplt in progress */
#define	S5CSTATE_DMAING		4	/* dma is in progress */
#define	S5CSTATE_ACCEPTINGMSG	5	/* MSGACCEPTED cmd in progress */
#define	S5CSTATE_PADDING	6	/* transfer padding in progress */
#define	S5CSTATE_GETTINGMSG	7	/* transfer msg in progress */

/*
 * Normal SCSI mass storage gate array setup
 * WARNING: S5R_CLOCKFREQ MUST match setup in S5RDMAC_NORMALBITS!
 */
#define	S5RDMAC_NOINTRBITS	S5RDMAC_20MHZ
#define	S5RDMAC_NORMALBITS	(S5RDMAC_NOINTRBITS | S5RDMAC_INTENABLE)
#define	S5R_CLOCKFREQ		20	/* 20 MHz */
#define	S5R_TIMEOUT		250	/* 250 ms */

static void scstart();
static void scfsm();
static void scmsgin();
static void screset();
static void scshowstatus();
static void scintr();

scsetup()
{
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *)SCSI_CSR;
	struct scsi_5390_ctrl *s5cp = &sc_s5c;

	/*
	 * initialize scsi_5390_ctrl struct
	 */
	((struct fd_cntrl_regs*)P_FLOPPY)->flpctl &= ~FLC_82077_SEL;
	bzero (s5cp, sizeof(struct scsi_5390_ctrl));	/* init ctrl info */
	s5cp->s5c_state = S5CSTATE_DISCONNECTED;
	/*
	 * initialize dma_chan and interrupt linkage
	 */
	s5cp->s5c_dc.dc_handler = scintr;
	s5cp->s5c_dc.dc_hndlrarg = (int)s5cp;
	s5cp->s5c_dc.dc_flags = DMACHAN_AUTOSTART | DMACHAN_PAD;
	s5cp->s5c_dc.dc_ddp = (struct dma_dev *)P_SCSI_CSR;
	dma_init(&s5cp->s5c_dc, I_SCSI_DMA);
	/*
	 * Reset and configure the chip
	 * For now, assume our SCSI bus id is 7, scsi_probe
	 * may change that, that's why we rewrite the config
	 * register after scsi_probe.
	 */
	s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS | S5RDMAC_RESET;
	DELAY(10);
	s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS;
	DELAY(10);
	s5rp->s5r_config = S5RCNF_ENABLEPARITY | S5RCNF_DISABLERESETINTR
	    | (7 & S5RCNF_BUSIDMASK);
	/* 20 MHz clock, 250ms selection timeout */
	s5rp->s5r_clkconv = S5RCLKCONV_FACTOR(S5R_CLOCKFREQ);
	s5rp->s5r_seltimeout = S5RSELECT_TIMEOUT(S5R_CLOCKFREQ, S5R_TIMEOUT);
	s5rp->s5r_syncoffset = 0;	/* async */
	s5rp->s5r_syncperiod = 5;
	s5rp->s5r_cmd = S5RCMD_RESETBUS;
	DELAY(2000000);		/* let bus clean up - 2 sec */
}

int
scpollcmd(sdp)
struct scsi_device *sdp;
{
	int s;
	int tries;
	struct mon_global *mg = restore_mg();
	volatile int *intrstatus = mg->mg_intrstat;

	/*
	 * some SCSI controller chips can't be polled,
	 * so what we do here is to run the command
	 * "the normal way" and then spin waiting for the controller
	 * to go idle
	 */
#ifdef SC_DEBUG
	printf("scpollcmd: intrstatus = 0x%x\n", intrstatus);
#endif SC_DEBUG
	mg->mg_scsi_intr = &scintr;
	scstart(sdp);
	tries = 0;
	while (sdp->sd_state == SDSTATE_START) {
		DELAY(10000);
		while (((s = *intrstatus) & I_BIT(I_SCSI)) == 0) {
			DELAY(10000);
			if (++tries > 1000) {
				screset("Didn't complete");
				return(0);
			}

			/* in case interrupts are enabled.. */
			if (mg->mg_flags & MGF_SCSI_INTR)
				goto cont;
		}
		scintr();
cont:
		mg->mg_flags &= ~MGF_SCSI_INTR;
	}
	return(sdp->sd_state==SDSTATE_COMPLETED && sdp->sd_status==STAT_GOOD);
}

static void
scstart(sdp)
struct scsi_device *sdp;
{
	struct scsi_5390_ctrl *s5cp = &sc_s5c;
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *)SCSI_CSR;
	union cdb *cdbp = &sdp->sd_cdb;
	int cmdlen, identify_msg, cdb_ctrl;
	char *cp;

	/*
	 * Figure out what kind of cdb we've been given
	 * and snag the ctrl byte
	 */
	switch (SCSI_OPGROUP(cdbp->cdb_opcode)) {
	case OPGROUP_0:
	case OPGROUP_6:
		cmdlen = sizeof(struct cdb_6);
		cdb_ctrl = cdbp->cdb_c6.c6_ctrl;
		break;
	case OPGROUP_1:
	case OPGROUP_2:
	case OPGROUP_7:
		cmdlen = sizeof(struct cdb_10);
		cdb_ctrl = cdbp->cdb_c10.c10_ctrl;
		break;
	case OPGROUP_5:
		cmdlen = sizeof(struct cdb_12);
		cdb_ctrl = cdbp->cdb_c12.c12_ctrl;
		break;
	default:
		cmdlen = -1;
		cdb_ctrl = 0;
		break;
	}

	/*
	 * initialize return parameters
	 */
	sdp->sd_state = SDSTATE_START;
	sdp->sd_resid = sdp->sd_bcount;
	sdp->sd_status = 0;

	/*
	 * Make sure nothing unreasonable has been asked of us
	 */
	if (cmdlen < 0 || ! DMA_BEGINALIGNED(sdp->sd_addr)
	    || cdb_ctrl != CTRL_NOLINK) {
		sdp->sd_state = SDSTATE_UNIMPLEMENTED;
		return;
	}
	if (s5cp->s5c_state != S5CSTATE_DISCONNECTED)
		screset("scstart: bad state");
	s5cp->s5c_cursdp = sdp;
	s5cp->s5c_curaddr = sdp->sd_addr;
	s5cp->s5c_curbcount = sdp->sd_bcount;
	s5cp->s5c_state = S5CSTATE_SELECTING;
	/*
	 * set target bus id
	 * punch cdb into fifo
	 * write command register
	 */
	s5rp->s5r_cmd = S5RCMD_FLUSHFIFO;
	DELAY(10);
	s5rp->s5r_select = sdp->sd_target;
	identify_msg = MSG_IDENTIFYMASK | (sdp->sd_lun & MSG_ID_LUNMASK);
	s5rp->s5r_fifo = identify_msg;
	for (cp = (char *)cdbp; cmdlen-- > 0;)
		s5rp->s5r_fifo = *cp++;
	s5rp->s5r_cmd = S5RCMD_SELECTATN;
}

static void
scintr()
{
	struct scsi_5390_ctrl *s5cp = &sc_s5c;
	struct scsi_device *sdp = s5cp->s5c_cursdp;
	volatile struct scsi_5390_regs *s5rp
	     = (struct scsi_5390_regs *) SCSI_CSR;
	char *errmsg;
	int i, lun, target;

#ifdef	SC_DEBUG
	printf("scintr()\n");
#endif	SC_DEBUG
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
#ifdef	SC_DEBUG
	printf("\ts5r_status = 0x%x  s5c_seqstep = 0x%x  s5r_intrstatus = 0x%x\n",
		s5cp->s5c_status, s5cp->s5c_seqstep, s5cp->s5c_intrstatus);
	printf("\ts5c_state = %d\n", s5cp->s5c_state);
#endif	SC_DEBUG

	/*
	 * First handle interrupt status bits that indicate
	 * errors which keep the state machine from advancing
	 * normally.
	 */
	if ((s5cp->s5c_status & S5RSTS_GROSSERROR)
	    || (s5cp->s5c_intrstatus & S5RINT_ILLEGALCMD)) {
		/* OOPS, we screwed up bad! */
		screset("software error");
		return;
	}
	if (s5cp->s5c_status & S5RSTS_PARITYERROR) {
		/* we're simple.... */
		screset("parity error");
		return;
	}

	switch (s5cp->s5c_state) {
	case S5CSTATE_SELECTING:
		/*
		 * Determine if selection was successful;
		 * if so, go to initiator state and call fsm;
		 * if not, go to DISCONNECTED state.
		 *
		 * Maybe someday handle selection sequence for
		 * SASI peripherals that don't MSG OUT for identify
		 * msg?  (Naaaa.....)
		 */
		if (s5cp->s5c_intrstatus & S5RINT_DISCONNECT) {
			/*
			 * selection timed-out
			 */
			sdp->sd_state = SDSTATE_SELTIMEOUT;
			s5cp->s5c_state = S5CSTATE_DISCONNECTED;
			s5cp->s5c_cursdp = NULL;
			return;
		} else if (((s5cp->s5c_seqstep & S5RSEQ_SEQMASK) == 4
		       || (s5cp->s5c_seqstep & S5RSEQ_SEQMASK) == 2)
		   && s5cp->s5c_intrstatus==(S5RINT_FUNCCMPLT|S5RINT_BUSSERVICE)) {
			/* it worked! */
			s5cp->s5c_state = S5CSTATE_INITIATOR;
		} else {
			screset("selection failed");
			return;
		}
		break;
	case S5CSTATE_DMAING:
		i = s5cp->s5c_curbcount;
		s5cp->s5c_curbcount = ((s5rp->s5r_cntmsb<<8)|s5rp->s5r_cntlsb);
		i -= s5cp->s5c_curbcount;	/* actual bytes transfered */
		s5cp->s5c_curaddr += i;
		s5cp->s5c_state = S5CSTATE_INITIATOR;
		dma_cleanup(&s5cp->s5c_dc, s5cp->s5c_curbcount);
		if (s5cp->s5c_dc.dc_flags & DMACHAN_ERROR) {
			screset("bus error");
			return;
		}
		break;
	case S5CSTATE_PADDING:
		s5cp->s5c_state = S5CSTATE_INITIATOR;
		break;
	case S5CSTATE_COMPLETING:
		if (s5cp->s5c_intrstatus & S5RINT_DISCONNECT) {
			screset("target aborted");
			return;
		}
		if ((s5cp->s5c_intrstatus & S5RINT_FUNCCMPLT) == 0
		    || (s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) != 2) {
			screset("fifo level");
			return;
		}
		s5cp->s5c_state = S5CSTATE_INITIATOR;
		sdp->sd_status = s5rp->s5r_fifo;
		s5cp->s5c_curmsg = s5rp->s5r_fifo;
		scmsgin(s5cp);
		break;
	case S5CSTATE_GETTINGMSG:
		if (s5cp->s5c_intrstatus & S5RINT_DISCONNECT) {
			screset("target aborted2");
			return;
		}
		if ((s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK) == 1) {
			s5cp->s5c_curmsg = s5rp->s5r_fifo;
			scmsgin(s5cp);
		} else {
			screset("msgin fifo level");
			return;
		}
		break;
	case S5CSTATE_ACCEPTINGMSG:
		s5cp->s5c_state = S5CSTATE_DISCONNECTED;
		s5cp->s5c_cursdp = NULL;
		sdp->sd_resid = s5cp->s5c_curbcount;
		sdp->sd_state = SDSTATE_COMPLETED;
		break;
	default:
	case S5CSTATE_DISCONNECTED:
		screset("scintr program error");
		break;
	}
	if (s5cp->s5c_state == S5CSTATE_INITIATOR)
		scfsm(s5cp);
}

static void
scfsm(s5cp)
struct scsi_5390_ctrl *s5cp;
{
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *) SCSI_CSR;
	struct scsi_device *sdp = s5cp->s5c_cursdp;

	/* make sure we start off with a clean slate */
	s5rp->s5r_cmd = S5RCMD_FLUSHFIFO;
	switch (s5cp->s5c_status & S5RSTS_PHASEMASK) {

	default:
	case PHASE_COMMAND:
		/* we shouldn't see command phase */
		screset("SCSI command phase");
		return;

	case PHASE_DATAOUT:	/* To Target from Initiator (write) */
		if (sdp->sd_read) {
			screset("SCSI bad i/o direction");
			break;
		}
		if (s5cp->s5c_curbcount == 0) {
			s5cp->s5c_state = S5CSTATE_PADDING;
			s5rp->s5r_cntlsb = 0;
			s5rp->s5r_cntmsb = 1;
			s5rp->s5r_fifo = 0;
#ifndef SC_53C90A
			s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
			s5rp->s5r_cmd = S5RCMD_TRANSFERPAD | S5RCMD_ENABLEDMA;
			break;
		}
		s5cp->s5c_state = S5CSTATE_DMAING;
		if (! DMA_BEGINALIGNED(s5cp->s5c_curaddr)) {
			screset("SCSI unaligned DMA segment");
			break;
		}
		dma_list(&s5cp->s5c_dc, s5cp->s5c_dl, s5cp->s5c_curaddr,
		    s5cp->s5c_curbcount, DMACSR_WRITE);
		dma_start(&s5cp->s5c_dc, s5cp->s5c_dl, DMACSR_WRITE);
		s5rp->s5r_cntlsb = s5cp->s5c_curbcount;
		s5rp->s5r_cntmsb = s5cp->s5c_curbcount >> 8;
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		s5rp->s5r_cmd = S5RCMD_TRANSFERINFO | S5RCMD_ENABLEDMA;
		s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS | S5RDMAC_DMAMODE;
		break;
	case PHASE_DATAIN:	/* From Target to Initiator (read) */
		if (!sdp->sd_read) {
			screset("SCSI bad i/o direction");
			break;
		}
		if (s5cp->s5c_curbcount == 0) {
			s5cp->s5c_state = S5CSTATE_PADDING;
			s5rp->s5r_cntlsb = 0;
			s5rp->s5r_cntmsb = 1;
			/* this doesn't really use dma, so no busgo() here */
#ifndef SC_53C90A
			s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
			s5rp->s5r_cmd = S5RCMD_TRANSFERPAD | S5RCMD_ENABLEDMA;
			break;
		}
		s5cp->s5c_state = S5CSTATE_DMAING;
		if (! DMA_BEGINALIGNED(s5cp->s5c_curaddr)) {
			screset("SCSI unaligned DMA segment");
			break;
		}
		dma_list(&s5cp->s5c_dc, s5cp->s5c_dl, s5cp->s5c_curaddr,
		    s5cp->s5c_curbcount, DMACSR_READ);
		dma_start(&s5cp->s5c_dc, s5cp->s5c_dl, DMACSR_READ);
		s5rp->s5r_cntlsb = s5cp->s5c_curbcount;
		s5rp->s5r_cntmsb = s5cp->s5c_curbcount >> 8;
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		s5rp->s5r_cmd = S5RCMD_TRANSFERINFO | S5RCMD_ENABLEDMA;
		s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS | S5RDMAC_DMAMODE
		    | S5RDMAC_DMAREAD;
		break;

	case PHASE_STATUS:	/* Status from Target to Initiator */
		/*
		 * scintr() will collect the STATUS byte
		 * (and hopefully a MSG) from the fifo when this
		 * completes.
		 */
		s5cp->s5c_state = S5CSTATE_COMPLETING;
		s5rp->s5r_cmd = S5RCMD_INITCMDCMPLT;
		break;
	case PHASE_MSGIN:	/* Message from Target to Initiator */
		s5cp->s5c_state = S5CSTATE_GETTINGMSG;
#ifndef SC_53C90A
		s5rp->s5r_cmd = S5RCMD_NOP;
#endif SC_53C90A
		s5rp->s5r_cmd = S5RCMD_TRANSFERINFO;
		break;
	case PHASE_MSGOUT:	/* Message from Initiator to Target */
		screset("SCSI msgout phase");
		break;
	}
}

static void
scmsgin(s5cp)
struct scsi_5390_ctrl *s5cp;
{
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *) SCSI_CSR;
	struct scsi_device *sdp = s5cp->s5c_cursdp;

	if (sdp == NULL) {
		screset("scmsgin: no current sd");
		return;
	}
	switch (s5cp->s5c_curmsg) {
	case MSG_CMDCMPLT:
		/*
		 * this is handled after we tell the
		 * target that the message has been accepted
		 * (since we need to maintain cursdp until that's
		 * accomplished)
		 */
		break;
	default:
		alert_msg("SCSI unexpected msg:%d\n", s5cp->s5c_curmsg);
		screset("SCSI unexpected msg");
		return;
	}
	/* We assume that we always have to do a S5RCMD_MSGACCEPTED */
	if ((s5cp->s5c_intrstatus & S5RINT_FUNCCMPLT) == 0) {
		screset("scmsgin: no FUNCCMPLT");
		return;
	}
	s5cp->s5c_state = S5CSTATE_ACCEPTINGMSG;
	s5rp->s5r_cmd = S5RCMD_MSGACCEPTED;
}

static void
screset(why)
char *why;
{
	struct scsi_5390_ctrl *s5cp = &sc_s5c;
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *) SCSI_CSR;
	struct scsi_device *sdp;

	/*
	 * If any dma was in progress, abort it and release the bus.
	 * Reset the SCSI bus.  If there's a current scsi_device
	 * connected mark it disconnected.  Finally, restart all
	 * disconnected I/O.
	 */
	scshowstatus(s5cp, why);
	if (s5cp->s5c_state == S5CSTATE_DMAING) {
		s5rp->s5r_dmactrl = S5RDMAC_NORMALBITS;
		dma_abort(&s5cp->s5c_dc);
	}
	s5rp->s5r_cmd = S5RCMD_RESETBUS;
	s5cp->s5c_state = S5CSTATE_DISCONNECTED;
	if ((sdp = s5cp->s5c_cursdp) != NULL) {
		if (sdp->sd_state == SDSTATE_START)
			sdp->sd_state = SDSTATE_ABORTED;
		s5cp->s5c_cursdp = NULL;
	}
	scsetup();
	return;
}

static void
scshowstatus(s5cp, why)
struct scsi_5390_ctrl *s5cp;
char *why;
{
	volatile struct scsi_5390_regs *s5rp
	    = (struct scsi_5390_regs *)SCSI_CSR;
	struct scsi_device *sdp;

	alert_msg("sc: %s\n", why);
	if ((sdp = s5cp->s5c_cursdp) != NULL) {
		alert_msg("sd: target %d lun %d opcode %d\n",
		    sdp->sd_target, sdp->sd_lun, sdp->sd_cdb.cdb_opcode);
		alert_msg("sd: addr 0x%x bcount %d rdflag %d\n",
		    sdp->sd_addr, sdp->sd_bcount, sdp->sd_read);
		alert_msg("sd: sdstatus %d sdstate %d resid %d\n",
		    sdp->sd_status, sdp->sd_state, sdp->sd_resid);

	}
	alert_msg("sc: s5c state %d status 0x%x\n", s5cp->s5c_state,
	    s5cp->s5c_status);
	alert_msg("sc: intrstatus 0x%x seqstep 0x%x\n", s5cp->s5c_intrstatus,
	    s5cp->s5c_seqstep & S5RSEQ_SEQMASK);
	alert_msg("sc: fifo level = %d transfer count %d\n",
	       s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK,
	       s5rp->s5r_cntlsb | (s5rp->s5r_cntmsb << 8));
	alert_msg("sc: command 0x%x config 0x%x\n", s5rp->s5r_cmd,
	    s5rp->s5r_config);
	alert_msg("sc: dma ctrl  0x%x dma status 0x%x\n", s5rp->s5r_dmactrl,
	    s5rp->s5r_dmastatus);
}
