/*	@(#)scsi.c	1.0	09/11/87	(c) 1987 NeXT	*/

/* 
 * scsi.c -- generic scsi routines
 * KERNEL VERSION
 *
 * HISTORY
 *  2-Aug-90	Gregg Kellogg (gk) at NeXT
 *	Changes for programatic disksort interface.  Use queue in busvar
 *	for enqueuing active queues.
 *
 * 27-Mar-90	Doug Mitchell
 *	removed call to scsi_cintr() in scsi_timer() 
 * 18-Apr-89	Doug Mitchell
 *	Init'd sdp in scsi_timer() case SCACTIVE_INTR
 * 10-Feb-89	Doug Mitchell at NeXT
 *	scsi_cstart() and scsi_dstart() return ints to indicate error status.  
 * 07-Feb-89	Doug Mitchell at NeXT
 *	Returned 0 for DKIOCRESET in scsi_ioctl()
 *	Fixed bug in reporting info bytes in scsi_sensemsg()
 * 03-Feb-89	Doug Mitchell at NeXT
 *	Modified interface between scsi_slave() and *sdswp->sdsw_slave
 * 25-Jan-89	Doug Mitchell (dmitch) at NeXT
 *	EPRINTF'd the ID target which resulted in a reselection timeout in
 *		scsi_timer()
 * 18-Jan-89	Doug Mitchell (dmitch) at NeXT
 *	Set sc_active = SCACTIVE_IDLE at top of scsi_cintr()
 * 18-Jan-89	Doug Mitchell (dmitch) at NeXT
 *	Added SCSI_SINGDISCOK, 
 *	Added an XDBG call in scsi_reselect()
 * 4-Mar-88	Mike DeMoney (mike) at NeXT
 *	Cleaned up scsi_timer()
 * 11-Sept-87	Mike DeMoney (mike) at NeXT
 *	Created.
 *
 */ 

/* #define	SCSI_NOTIMER	1	/* disable lost interrupt timer */
/* #define	SCSI_TRACE	1	/* trace polled commands */
/* #define	SCSI_SINGDISCOK 1	/* allow disconnects for 1 target */

#import <machine/spl.h>
#import <sys/types.h>
#import <sys/time.h>
#import <sys/buf.h>
#import <sys/param.h>
#import <sys/kernel.h>
#import <kern/queue.h>
#import <vm/vm_kern.h>
#import <next/printf.h>
#import <next/psl.h>
#import <nextdev/busvar.h>
#import <nextdev/scsireg.h>
#import <nextdev/scsivar.h>
#import <nextdev/disk.h>

#import <machine/cpu.h>
#import <kern/xpr.h>
#import <nextdev/dma.h>		/* temp only */
#import <nextdev/screg.h>		/* temp only */
#import <nextdev/scvar.h>		/* temp only */

extern struct reg_values s5c_state_values[];

#define EPRINTF(x)	{ XDBG(x); printf x; }

/*
 * global declarations
 */	
int scsi_ndevices; 			/* for inhibiting disconnects */

/*
 * local declarations
 */
static int scsi_timer_running;
static int scsi_timer_interval;
static queue_head_t scsi_ctrl_list;
static int scsi_init_flag;

static void scsi_timer();
static int scsi_cstart(struct scsi_ctrl *scp);

/*
 * Generic scsi register and value definitions
 */

#if	DEBUG || XPR_DEBUG
struct reg_values scsi_phase_values[] = {
/* value		name */
{  PHASE_DATAOUT,	"data_out"	},
{  PHASE_DATAIN,	"data_in"	},
{  PHASE_COMMAND,	"command"	},
{  PHASE_STATUS,	"status"	},
{  PHASE_MSGOUT,	"message_out"	},
{  PHASE_MSGIN,		"message_in"	},
{  0,			NULL		}
};

struct reg_values scsi_msg_values[] = {
/* value		name */
{  MSG_CMDCMPLT,	"cmd cmplt"		},
{  MSG_SAVEPTRS,	"save ptrs"		},
{  MSG_RESTOREPTRS,	"restore ptrs"		},
{  MSG_DISCONNECT,	"disconnect"		},
{  MSG_IDETERR,		"init detected err"	},
{  MSG_ABORT,		"abort"			},
{  MSG_MSGREJECT,	"msg reject"		},
{  MSG_NOP,		"nop"			},
{  MSG_MSGPARERR,	"msg parity err"	},
{  MSG_DEVICERESET,	"device reset"		},
{  0,			NULL			}
};

struct reg_values scsi_status_values[] = {
/* value		name */
{  STAT_GOOD,		"GOOD"			},
{  STAT_CHECK,		"CHECK"			},
{  STAT_CONDMET,	"CONDITION_MET"		},
{  STAT_BUSY,		"BUSY"			},
{  STAT_INTMGOOD,	"INTERMEDIATE_GOOD"	},
{  STAT_INTMCONDMET,	"INTERMEDIATE_COND_MET"	},
{  STAT_RESERVED,	"RESERVATION_CONFLICT"	},
{  0,			NULL			}
};

struct reg_values scsi_sensekey_values[] = {
/* value		name */
{  SENSE_NOSENSE,	"NO_ERROR"		},
{  SENSE_RECOVERED,	"RECOVERED_ERROR"	},
{  SENSE_NOTREADY,	"NOT_READY"		},
{  SENSE_MEDIA,		"MEDIA_FLAW"		},
{  SENSE_HARDWARE,	"HARDWARE_ERROR"	},
{  SENSE_ILLEGALREQUEST,"ILLEGAL_REQUEST"	},
{  SENSE_UNITATTENTION,	"UNIT_ATTENTION"	},
{  SENSE_DATAPROTECT,	"DATA_PROTECT"		},
{  SENSE_ABORTEDCOMMAND,"ABORTED_COMMAND"	},
{  SENSE_VOLUMEOVERFLOW,"VOLUME_OVERFLOW"	},
{  SENSE_MISCOMPARE,	"MISCOMPARE"		},
{  0,			NULL			}
};

struct reg_values scsi_sdstate_values[] = {
/* value		name */
{  SDSTATE_START,	"START"			},
{  SDSTATE_INPROGRESS,	"IN_PROGRESS"		},
{  SDSTATE_SELTIMEOUT,	"SELECTION_TIMEOUT"	},
{  SDSTATE_DISCONNECTED,"DISCONNECTED"		},
{  SDSTATE_COMPLETED,	"COMPLETED"		},
{  SDSTATE_RETRY,	"RETRY"			},
{  SDSTATE_TIMEOUT,	"RESELECTION_TIMEOUT"	},
{  SDSTATE_ABORTED,	"ABORTED"		},
{  SDSTATE_UNIMPLEMENTED,"UNIMPLEMENTED_REQ"	},
{  SDSTATE_REJECTED,	"DEVICE_DRIVER_REJ"	},
{  SDSTATE_DROPPED,	"DEVICE_DROPPED_CONN"	},
{  0,			NULL			}
};

struct reg_values scsi_cmd_values[] = {
/* value		name */
{  C6OP_TESTRDY,	"TEST_READY"		},
{  C6OP_REWIND,		"REWIND"		},
{  C6OP_REQSENSE,	"REQUEST_SENSE"		},
{  C6OP_FORMAT,		"FORMAT"		},
{  C6OP_REASSIGNBLK,	"REASSIGN_BLK"		},
{  C6OP_READ,		"READ"			},
{  C6OP_WRITE,		"WRITE"			},
{  C6OP_SEEK,		"SEEK"			},
{  C6OP_READREV,	"READ REVERSE"		},
{  C6OP_WRTFM,		"WRITE FILE MARK"	},
{  C6OP_SPACE,		"SPACE"			},
{  C6OP_INQUIRY,	"INQUIRY"		},
{  C6OP_VERIFY,		"VERIFY"		},
{  C6OP_MODESELECT,	"MODE_SELECT"		},
{  C6OP_MODESENSE,	"MODE_SENSE"		},
{  C6OP_STARTSTOP,	"START_STOP"		},
{  C6OP_SENDDIAG,	"SEND_DIAG"		},
{  C10OP_READCAPACITY,	"READ_CAPACITY"		},
{  C10OP_READEXTENDED,	"READ_EXTENDED"		},
{  C10OP_WRITEEXTENDED,	"WRITE_EXTENDED"	},
{  C10OP_READDEFECTDATA,"READ_DEFECT_DATA"	},
{  0,			NULL			}
};
#endif	DEBUG || XPR_DEBUG

#define	streq(a,b)	(!strcmp(a,b))

void
scsi_init()
{
	if (!scsi_init_flag) {
		queue_init(&scsi_ctrl_list);
		scsi_init_flag = 1;
	}
}

void
scsi_probe(struct bus_ctrl *bcp, 
	struct scsi_ctrl *scp, 
	struct scsi_csw *scswp, 
	int ipl)
{
	int s;

	if (sizeof(struct cdb_6) != 6 || sizeof(struct cdb_10) != 10
	    || sizeof(struct cdb_12) != 12)
		panic("scsi: compiler alignment problems");
	/*
	 * initialize generic scsi structs for controller
	 * number "ctrl".
	 */
	scp->sc_bcp = bcp;
	scp->sc_scswp = scswp;	/* controller specific routines */
	for (s = 0; s < SCSI_NLUNS; s++)
		scp->sc_lunmask[s] = 0;
	scp->sc_target = 7;
	scp->sc_lunmask[scp->sc_target] = -1;
	scp->sc_active = SCACTIVE_IDLE;
	scp->sc_expectintr = 0;
	scp->sc_ipl = ipl;
	scp->sc_disc_cnt = 0;
	queue_init(&scp->sc_dis_q);
	s = splsoftclock();		/* timer might be running */
	queue_enter(&scsi_ctrl_list, scp, struct scsi_ctrl *, sc_ctrl_list);
	splx(s);
} /* scsi_probe() */

int
scsi_slave(struct scsi_ctrl *scp, struct bus_device *bdp)
{
	struct scsi_dsw *sdswp;
	struct scsi_device *sdp;
	int i;

	/* find entry in device switch for this device type */
	for (i = 0; sdswp = scsi_sdswlist[i]; i++) {
		if (streq(sdswp->sdsw_name, bdp->bd_name))
			break;
	}
	if (sdswp == 0) {
		printf("No driver configured for SCSI device type %s\n",
		    bdp->bd_name);
		return(0);
	}
	/*
	 * Unfortunately, we have to initialize everything here rather
	 * than in the attach routine.  It's just too hard to do slave
	 * detection without using the controller and driver level code.
	 */
	bdp->bd_type = i;	/* for controller attach routine's benefit */
	sdp = (*sdswp->sdsw_getsdp)(bdp->bd_unit);
	sdp->sd_scp = scp;
	sdp->sd_bdp = bdp;
	sdp->sd_expectresel = 0;
	sdp->sd_reseltimeout = 0;
	sdp->sd_active = 0;
	sdp->sd_sdswp = sdswp;
	if (bdp->bd_slave != '?') {
		sdp->sd_target = SCSI_TARGET(bdp->bd_slave);
		sdp->sd_lun = SCSI_LUN(bdp->bd_slave);
		/* has it already been snagged */
		if (scp->sc_lunmask[sdp->sd_target] & (1 << sdp->sd_lun))
			return(0);
		if ((*sdswp->sdsw_slave)(sdp,bdp)) 
			return(1);
		return(0);
	}
	/*
	 * Config line has a wildcard for scsi slave....
	 * starting where we last left off, try to find a device
	 * out there of the appropriate device type
	 */
	while (sdswp->sdsw_nexttarget < SCSI_NTARGETS) {
		sdp->sd_target = sdswp->sdsw_nexttarget;
		while (sdswp->sdsw_nextlun < SCSI_NLUNS) {
			sdp->sd_lun = sdswp->sdsw_nextlun++;
			if(scp->sc_lunmask[sdp->sd_target]&(1<<sdp->sd_lun)) {
				/* it's already been snagged. Forget this 
				 * target.
				 */
				sdswp->sdsw_nextlun = SCSI_NLUNS;
				continue;
			}
			if ((*sdswp->sdsw_slave)(sdp,bdp))
				return(1);
				
			/*
			 * A little optimization here....
			 */
			if (sdp->sd_state == SDSTATE_SELTIMEOUT)
				sdswp->sdsw_nextlun = SCSI_NLUNS;
			if (sdp->sd_state == SDSTATE_ABORTED) {
				/*
				 * no interrupts are coming from
				 * the scsi chip, let's just give
				 * up completely so we don't take
				 * for ever to config non-existant
				 * scsi devices
				 */
				sdswp->sdsw_nexttarget = SCSI_NTARGETS;
				sdswp->sdsw_nextlun = SCSI_NLUNS;
			}
		}
		sdswp->sdsw_nexttarget++;
		sdswp->sdsw_nextlun = 0;
	}
	return(0);
} /* scsi_slave() */

void
scsi_attach(struct scsi_device *sdp)
{
	/* special case for sg device - no valid device at time of attach;
	 * it will decrement scsi_ndevices at attach time and the increment/
	 * decrement in open() and close().
	 */
	scsi_ndevices++;
	(*sdp->sd_sdswp->sdsw_attach)(sdp);
	if (! scsi_timer_running) {
		scsi_timer_interval = hz/2;
		scsi_timer_running = 1;
		scsi_timer();
	}
	return;
}

int
scsi_pollcmd(struct scsi_device *sdp)
{
	struct scsi_ctrl *scp = sdp->sd_scp;
	int tries = 0;
	int waittries;
	int s;

	/*
	 * some SCSI controller chips can't be polled,
	 * so what we do here is to run the command
	 * "the normal way" and then spin waiting for the controller
	 * to go idle
	 */
	ASSERT(curipl() == 0);
	do {
		s = spln(ipltospl(scp->sc_ipl));
		ASSERT(sdp->sd_active == 0 && scp->sc_active == SCACTIVE_IDLE);
		sdp->sd_disconnectok = 0;	/* can't disconnect! */
		scp->sc_active = SCACTIVE_POLLING;
		(*scp->sc_scswp->scsw_start)(sdp);
		waittries = 0;
		while (scp->sc_active != SCACTIVE_IDLE && ++waittries < 1000) {
			splx(s);
			DELAY(1000);
			s = spln(ipltospl(scp->sc_ipl));
		}
		if (waittries >= 1000) {
#ifdef SCSI_TRACE
			printf("scsi_pollcmd: timeout\n");
#endif SCSI_TRACE
			(*scp->sc_scswp->scsw_reset)(scp, SCRESET_ABORT, NULL);
			scp->sc_active = SCACTIVE_IDLE;
		}
		splx(s);
	} while (++tries < 3 && sdp->sd_state == SDSTATE_SELTIMEOUT);
	return(sdp->sd_state==SDSTATE_COMPLETED && sdp->sd_status==STAT_GOOD);
} /* scsi_pollcmd() */

/*
 * scsi device start routine -- queues a unit for service by scsi
 * controller
 */

scsi_dstart(struct scsi_device *sdp)
{
	struct scsi_ctrl *scp = sdp->sd_scp;

	/*
	 * The referenced scsi_device has new work to do.
	 * Add the device to the controller work queue and start
	 * the controller if it isn't active.
	 *
	 * MUST BE CALLED AT SCSI IPL
	 */
	XSDBG(("scsi_dstart: sc_active = %d\n",scp->sc_active));
	ASSERT(curipl() >= scp->sc_ipl);
	ASSERT(sdp->sd_active == 0);
	queue_enter(&scp->sc_bcp->bc_tab, sdp, struct scsi_device *,
		sd_work_q);
	sdp->sd_active = 1;
	if (scp->sc_active == SCACTIVE_IDLE)
		return(scsi_cstart(scp));
	else
		return(0);
}

static int
scsi_cstart(struct scsi_ctrl *scp)
{
	struct scsi_device *sdp;

	/*
	 * job here is to select a scsi_device to be serviced by
	 * the scsi controller, mark the controller active and call
	 * the device specific start routine.
	 *
	 * MUST BE CALLED AT SCSI IPL
	 */
	XSDBG(("scsi_cstart: sc_active = %d\n",scp->sc_active));
	ASSERT(curipl() >= scp->sc_ipl);
	ASSERT(scp->sc_active == SCACTIVE_IDLE
	    && !queue_empty(&scp->sc_bcp->bc_tab));
	XSDBG(("scsi_cstart: s5c_state = %n, sc_active = %d\n",
		sc_s5c[0].s5c_state,s5c_state_values,scp->sc_active));
	sdp = (struct scsi_device *)queue_first(&scp->sc_bcp->bc_tab);
	scp->sc_active = SCACTIVE_INTR;
	return((*sdp->sd_sdswp->sdsw_start)(sdp));
}

scsi_docmd(struct scsi_device *sdp)
{
	struct scsi_ctrl *scp = sdp->sd_scp;

	XSDBG(("scsi_docmd: sc_active = %d\n",scp->sc_active));
	ASSERT(curipl() >= scp->sc_ipl);
	ASSERT(sdp->sd_active
	    && (struct scsi_device *)queue_first(&scp->sc_bcp->bc_tab) == sdp);
#ifndef SCSI_SINGDISCOK
	if (scsi_ndevices == 1)
		sdp->sd_disconnectok = 0;
#endif  SCSI_SINGDISCOK
	XSDBG(("scsi_docmd: s5c_state = %n  sc_active = %d\n",
		sc_s5c[0].s5c_state,s5c_state_values,scp->sc_active));
	return((*scp->sc_scswp->scsw_start)(sdp));
}

#ifndef SCSI_MACROS
void
scsi_rejectcmd(struct scsi_device *sdp)
{
	struct scsi_ctrl *scp = sdp->sd_scp;

	XSDBG(("scsi_rejectcmd\n"));
	ASSERT(curipl() >= scp->sc_ipl);
	ASSERT(sdp->sd_active \
	    && (struct scsi_device *)queue_first(&scp->sc_bcp->bc_tab) == sdp);
	sdp->sd_state = SDSTATE_REJECTED;
	scsi_cintr(scp);
}
#endif !SCSI_MACROS
void
scsi_cintr(struct scsi_ctrl *scp)
{
	struct scsi_device *sdp = 
		(struct scsi_device *)queue_first(&scp->sc_bcp->bc_tab);

	/*
	 * We're only called if the controller chip is idle.
	 * The job here is to properly advance the controller
	 * work queue.
	 *
	 * Call scsi_cstart(scp) if there's some work to do,
	 * otherwise just return.
	 */
	ASSERT(curipl() >= scp->sc_ipl);
	XSDBG(("scsi_cintr: sc_active = %d\n",sdp->sd_scp->sc_active));
	if (scp->sc_active == SCACTIVE_POLLING) {
		scp->sc_active = SCACTIVE_IDLE;
		return;
	}
	ASSERT(!queue_empty(&scp->sc_bcp->bc_tab));
	
	/* careful - unethical use of index into sc_s5c[] */
	XSDBG(("scsi_cintr: s5c_state = %n  sd_state = %n\n",
		sc_s5c[0].s5c_state,s5c_state_values,
		sdp->sd_state,scsi_sdstate_values));
	XSDBG(("            sc_active WAS %d, is SCACTIVE_IDLE\n",
		scp->sc_active));	
	scp->sc_active = SCACTIVE_IDLE;	/* as of 01/18/89 */
	switch (sdp->sd_state) {
	case SDSTATE_COMPLETED:		/* done, possibly with error */
	case SDSTATE_ABORTED:		/* ctrlr aborted cmd */
	case SDSTATE_DROPPED:		/* device dropped connection */
	case SDSTATE_RETRY:		/* innocently killed by scsi reset */
	case SDSTATE_SELTIMEOUT:	/* selection timeout */
	case SDSTATE_REJECTED:		/* device level won't do it */
	case SDSTATE_UNIMPLEMENTED:	/* cntrlr can't do it */
		/*
		 * nothing more we can do with this at the controller
		 * level, see if the device interrupt handler wants
		 * to do anything with it
		 */
		XSDBG(("            done: target %d   lun %d\n",
			sdp->sd_target,sdp->sd_lun));
		queue_remove(&scp->sc_bcp->bc_tab, sdp, struct scsi_device *,
		    sd_work_q);
		sdp->sd_active = 0;
		(*sdp->sd_sdswp->sdsw_intr)(sdp);
		break;
	case SDSTATE_DISCONNECTED:
		/*
		 * target has temporarily disconnected, put scsi_device
		 * on disconnect queue until it reselects
		 */
		XSDBG(("            disconnect: target %d   lun %d\n",
			sdp->sd_target,sdp->sd_lun));
		queue_remove(&scp->sc_bcp->bc_tab, sdp, struct scsi_device *,
		    sd_work_q);
		queue_enter(&scp->sc_dis_q, sdp, struct scsi_device *,
		    sd_dis_q);
		scp->sc_disc_cnt++;
		break;
	case SDSTATE_START:
		/*
		 * controller couldn't start this before (e.g. it was
		 * in "reselection mode".  try again.
		 */
		break;
	default:
#if	DEBUG
		scsi_msg(sdp, sdp->sd_cdb.cdb_opcode, "PANIC bad sd_state");
#endif	DEBUG
		panic("scsi_cintr: bad sd_state");
	}
	/* was... 
	scp->sc_active = SCACTIVE_IDLE;
	...This resulted in two calls to scsi_cstart if there was more work to
	   do when this routine was called (one below, and one via sdintr,
	   from the switch above). 
	   
	Moved above 01/18/89. (dpm) */
	XSDBG(("scsi_cintr: end: s5c_state = %n  sc_active = %d\n",
		sc_s5c[0].s5c_state,s5c_state_values,scp->sc_active));
	/* was...
	if (!queue_empty(&scp->sc_bcp->bc_tab))
	... is: */
	if ((!queue_empty(&scp->sc_bcp->bc_tab)) && 
	    (scp->sc_active == SCACTIVE_IDLE))
		scsi_cstart(scp);
		
} /* scsi_cintr() */

struct scsi_device *
scsi_reselect(struct scsi_ctrl *scp, 
	u_char target, 
	u_char lun)
{
	struct scsi_device *sdp;

	ASSERT(curipl() >= scp->sc_ipl);
	XSDBG(("scsi_reselect: target %d   lun %d\n",target,lun));
	for (sdp = (struct scsi_device *)queue_first(&scp->sc_dis_q);
	    !queue_end(&scp->sc_dis_q, (queue_entry_t)sdp);
	    sdp = (struct scsi_device *)queue_next(&sdp->sd_dis_q)) {
		if (target == sdp->sd_target && lun == sdp->sd_lun) {
			queue_remove(&scp->sc_dis_q, sdp, struct scsi_device *,
			    sd_dis_q);
			queue_enter_first(&scp->sc_bcp->bc_tab, sdp,
			    struct scsi_device *, sd_work_q);
			scp->sc_disc_cnt--;
			return(sdp);
		}
	}
	return((struct scsi_device *)NULL);
}

void
scsi_restart(struct scsi_ctrl *scp)
{
	struct scsi_device *sdp;

	ASSERT(curipl() >= scp->sc_ipl);
	for (sdp = (struct scsi_device *)queue_first(&scp->sc_dis_q);
	    !queue_end(&scp->sc_dis_q, (queue_entry_t)sdp);
	    sdp = (struct scsi_device *)queue_next(&sdp->sd_dis_q)) {
		queue_remove(&scp->sc_dis_q, sdp, struct scsi_device *,
		    sd_dis_q);
		/*
		 * let device level have a shot at restarting command
		 */
		sdp->sd_state = SDSTATE_RETRY;
		sdp->sd_expectresel = 0;
		sdp->sd_active = 0;
		(*sdp->sd_sdswp->sdsw_intr)(sdp);
	}
}

int
scsi_ioctl(struct scsi_device *sdp, 
	int cmd, 
	caddr_t data, 
	int flag)
{
	struct scsi_ctrl *scp = sdp->sd_scp;

	switch (cmd) {
	case DKIOCRESET:	/* SCSI bus reset */
		(*scp->sc_scswp->scsw_reset)(scp, SCRESET_RETRY, "bus reset");
		return(0);

	default:
		return((*scp->sc_scswp->scsw_ioctl)(scp, cmd, data, flag));
	}
}

#ifndef SCSI_MACROS
void
scsi_expectintr(struct scsi_ctrl *scp, int timeout)
{
	scp->sc_timer = timeout * hz;
	scp->sc_expectintr = 1;
}

void
scsi_gotintr(struct scsi_ctrl *scp)
{
	scp->sc_expectintr = 0;
}

void
scsi_expectreselect(struct scsi_device *sdp)
{
	sdp->sd_timer = sdp->sd_timeout * hz;
	sdp->sd_expectresel = 1;
}

void
scsi_gotreselect(struct scsi_device *sdp)
{
	sdp->sd_expectresel = 0;
}
#endif !SCSI_MACROS

#define	SDP_T	struct scsi_device *

static void
scsi_timer()
{
	struct scsi_ctrl *scp;
	struct scsi_device *sdp;
	int s, timed_out;
	int to_target;		/* target which timed out - dpm */

	for (scp = (struct scsi_ctrl *)queue_first(&scsi_ctrl_list);
	     !queue_end(&scsi_ctrl_list, (queue_entry_t)scp);
	     scp = (struct scsi_ctrl *)queue_next(&scp->sc_ctrl_list)) {
		s = spln(ipltospl(scp->sc_ipl));
		switch (scp->sc_active) {
		    case SCACTIVE_POLLING:
			break;
		    case SCACTIVE_INTR:
			if (scp->sc_expectintr) {
			    ASSERT(scp->sc_active != SCACTIVE_IDLE);
			    scp->sc_timer -= scsi_timer_interval;
			    if (scp->sc_timer < 0) {
				scp->sc_expectintr = 0;
				/*
				 * Controller reset should
				 * scsi_restart() all disconnected
				 * requests.
				 */
				sdp = (struct scsi_device *)
				    queue_first(&scp->sc_bcp->bc_tab);
				(*scp->sc_scswp->scsw_reset)(scp,
				    SCRESET_ABORT, "lost interrupt");
				scsi_msg(sdp, sdp->sd_cdb.cdb_opcode,
					"scsi_timer: timeout");
				/* scsi_cintr(scp);...screset does this for 
				 * us... */
			    }
			}
			break;
		case SCACTIVE_IDLE:
			/*
			 * Only trick here is to mark the one request
			 * that timed out with SDSTATE_TIMEOUT, otherwise
			 * we would just call the controller reset routine
			 */
			timed_out = 0;
			for (sdp = (SDP_T)queue_first(&scp->sc_dis_q);
			    !queue_end(&scp->sc_dis_q, (queue_entry_t)sdp);
			    sdp = (SDP_T)queue_next(&sdp->sd_dis_q)) {
				if (sdp->sd_expectresel) {
					ASSERT(sdp->sd_active);
					sdp->sd_timer -= scsi_timer_interval;
					if (sdp->sd_timer < 0) {
						queue_remove(&scp->sc_dis_q,
						    sdp, SDP_T, sd_dis_q);
						sdp->sd_state = SDSTATE_TIMEOUT;
						sdp->sd_expectresel = 0;
						sdp->sd_active = 0;
						/*
						 * Can't restart controller
						 * level until we're done
						 * here.  Otherwise, reset
						 * that follows would trash
						 * everything.
						 */
						scp->sc_active = SCACTIVE_HOLD;
						(*sdp->sd_sdswp->sdsw_intr)(sdp);
						timed_out = 1;
						to_target = sdp->sd_target;
					}
				}
			}
			if (timed_out) {
				EPRINTF(("reselect timeout - target %d\n",
					to_target));
				(*scp->sc_scswp->scsw_reset)(scp,
					SCRESET_RETRY,"reselect timeout");
				scp->sc_active = SCACTIVE_IDLE;
				if (!queue_empty(&scp->sc_bcp->bc_tab))
					scsi_cstart(scp);
			}
			break;
		default:
			panic("scsi_timer: sc_active");
		}
		splx(s);
	}
#ifndef SCSI_NOTIMER
	timeout(scsi_timer, 0, scsi_timer_interval);
#endif SCSI_NOTIMER
} /* scsi_timer() */

void
scsi_timeout(struct scsi_ctrl *scp)
{
	int s;

	XSDBG(("scsi_timeout: sc_active = %d\n",scp->sc_active));
	s = spln(ipltospl(scp->sc_ipl));
	(*scp->sc_scswp->scsw_reset)(scp, SCRESET_ABORT, NULL);
	/* scsi_cintr(scp); screset does this for us...*/
	splx(s);
}

void
scsi_msg(struct scsi_device *sdp, 
	u_char opcode, 
	char *msg)
{
#if DEBUG || XPR_DEBUG
	printf("s%c%d (%d,%d): %s op:%n sd_state:%n scsi status:%n\n",
	    sdp->sd_devtype, sdp->sd_bdp->bd_unit, sdp->sd_target, 
	    sdp->sd_lun, msg, opcode, scsi_cmd_values, sdp->sd_state,
	    scsi_sdstate_values, sdp->sd_status & STAT_MASK, 
	    scsi_status_values);
#else
	printf("s%c%d (%d,%d): %s op:0x%x sd_state:%d scsi status:0x%x\n",
	    sdp->sd_devtype,sdp->sd_bdp->bd_unit, sdp->sd_target, 
	    sdp->sd_lun, msg, opcode, sdp->sd_state, 
	    sdp->sd_status & STAT_MASK);
#endif DEBUG || XPR_DEBUG
}

void
scsi_sensemsg(struct scsi_device *sdp, struct esense_reply *erp)
{
#if DEBUG || XPR_DEBUG
	printf("s%c%d (%d,%d): sense key:%n    additional sense code:0x%x\n",
	    sdp->sd_devtype, sdp->sd_bdp->bd_unit, sdp->sd_target, sdp->sd_lun,
	    erp->er_sensekey, scsi_sensekey_values, erp->er_addsensecode);
		
#else
	printf("s%c%d (%d,%d): sense key:0x%x  additional sense code:0x%x\n",
	    sdp->sd_devtype, sdp->sd_bdp->bd_unit, sdp->sd_target, sdp->sd_lun,
	    erp->er_sensekey, erp->er_addsensecode);
#endif DEBUG || XPR_DEBUG
}




