/*	@(#)fd_global.c	  2.0	01/26/90	(c) 1990 NeXT	*/

/* 
 * fd_global.c -- Global variables for Floppy Disk driver
 * KERNEL VERSION
 *
 * HISTORY
 * 26-Jan-90	Doug Mitchell at NeXT
 *	Created.
 */ 

#import "fd.h"
#import <sys/types.h>
#import <next/printf.h>
#import <nextdev/disk.h>
#import	<nextdev/fd_vars.h>

/*
 * the vanilla, non-ifdef'd globals...
 */
int 			vol_check_event;	/* event volume_check thread 
						 * sleeps on */
char 			vol_check_alive;
int 			vol_check_delay=VC_DELAY_NORM;	
						/* us volume_check thread 
						 * sleeps each loop */
simple_lock_data_t 	vol_check_lock;		/* protects above two variables
						 */
queue_head_t		disk_eject_q;		/* disk_eject_thread's
						 * input queue */
boolean_t		fc_thread_init=FALSE;	/* threads have been started */
boolean_t		fc_thread_timer_started=FALSE;	
boolean_t 		fd_polling_mode=FALSE;
lock_data_t		fd_open_lock;		/* limits open()s to one at a
						 * time for clean 
						 * volume-to-drive mapping */
queue_head_t		vol_abort_q;		/* queue of vol_abort_entries 
						 */
simple_lock_data_t	vol_abort_lock;		/* protects access to 
						 * vol_abort_q */
int			fd_inner_retry;		/* retry counts - can be */
int			fd_outer_retry;		/* modified with ioctl's */
int			fd_blk_major;
int			fd_raw_major;

/*
 * XPR control variables
 */
#ifdef	DEBUG
boolean_t fd_xpr_cmd=FALSE;		/* read, write, format, etc. */
boolean_t fd_xpr_all=TRUE;		/* general trace */
boolean_t fd_xpr_pio=FALSE;		/* every byte of PIO */
boolean_t fd_xpr_addrs=TRUE;		/* all r/w addresses */
boolean_t fd_xpr_thrblock=FALSE;	/* all thread blocks and wakeups */
boolean_t fd_xpr_io=FALSE;		/* all controller-level I/O */
#endif	DEBUG
 
#if	NFC > 0
struct fd_controller fd_controller[NFC];
struct bus_ctrl *fc_bcp[NFC];
struct bus_driver fcdriver = {
	(PFI)fc_probe, 			/* br_probe */
	(PFI)fc_slave,			/* br_slave */
	(PFI)fd_attach,			/* br_attach */
	(PFI)fc_go,			/* br_go */
	0, 				/* br_done bus transfer complete */
	0, 				/* br_intr service interrupt */
	(PFI)fc_init, 			/* br_init */
	sizeof (struct fd_cntrl_regs),	/* br_size */
	"fd", 				/* br_dname */
	0, 				/* br_dinfo - backpointer to dinit
					 * struct */
	"fc", 				/* br_cname */
	fc_bcp, 			/* br_cinfo */
	/* BUS_SCSI?? */ 0		/* br_flags */
};
#endif	NFC
#if	NUM_FV > 0
fd_volume_t fd_volume_p[NUM_FVP];
#endif	NUM_FV
#if	NFD > 0
struct fd_drive fd_drive[NFD];
#endif	NFD

#define	DRIVE_TYPES	1		/* number of supported drive types */

/*
 * Drive timings for specify command
 */
#define SONY_SEEK_RATE		3	/* step pulse rate in ms */
#define SONY_HEAD_UNLOAD	32	/* head unload time in ms */
#define SONY_HEAD_LOAD		15	/* head load time in ms */

struct fd_drive_info fd_drive_info[DRIVE_TYPES] = {
	{				/* FD-288 drive */
	    {
		"Sony MPX-111N",	/* disk name */
		{ 0, 
		  0, 
		  0,
		  0 }, 			/* label locations - not used */
		1024,			/* device sector size. This is
					 * not fixed. */
		64 * 1024		/* max xfer bcount */
	    },
	    	SONY_SEEK_RATE,
		SONY_HEAD_LOAD,
		SONY_HEAD_UNLOAD,
		TRUE,			/* perpendicular recording */
		FALSE			/* no precompensation */
	},
};

/*
 * Constant tables for mapping media_id and density into physical disk 
 * parameters.
 */
struct fd_disk_info fd_disk_info[] = {
   /* media_id          tracks_per_cyl  num_cylinders   max_density */
    { FD_MID_1MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_1    },
    { FD_MID_2MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_2    },
    { FD_MID_4MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_4    },
    { FD_MID_NONE,	0, 		0, 		FD_DENS_NONE },
};

static struct fd_sectsize_info ssi_1mb[] = {
/* sect_size	N	sects_per_trk	rw_gap_length	fmt_gap_length */
{  512,		2,	0x9,		0x1b,		0x54		},
{  1024,	3,	0x5,		0x35,		0x74		},
{  0,		0,	0,		0,		0		}
};

static struct fd_sectsize_info ssi_2mb[] = {
/* sect_size	N	sects_per_trk	rw_gap_length	fmt_gap_length */
{  512,		2,	0x12,		0x1b,		0x65		},
{  0,		0,	0,		0,		0		}
};

static struct fd_sectsize_info ssi_4mb[] = {
/* sect_size	N	sects_per_trk	rw_gap_length	fmt_gap_length */
{  512,		2,	0x24,		0x1b,		0x53		},
{  0,		0,	0,		0,		0		}
};

struct fd_density_sectsize fd_density_sectsize[] = {
    { FD_DENS_1,	ssi_1mb },
    { FD_DENS_2,	ssi_2mb },
    { FD_DENS_4,	ssi_4mb },
    { FD_DENS_NONE,	ssi_1mb },
    { 0,		NULL }
};

struct fd_density_info fd_density_info[] = {
    /* density     	capacity    	mfm    */
    { FD_DENS_1,	720  * 1024,	TRUE,	}, /* 720  KB */
    { FD_DENS_2,	1440 * 1024, 	TRUE,	}, /* 1.44 MB */
    { FD_DENS_4,	2880 * 1024,	TRUE,	}, /* 2.88 MB */
    { FD_DENS_NONE,	720  * 1024,	TRUE,	}, /* unformatted */
};

struct reg_values fd_return_values[] = {
/* value			name */
{  FDR_SUCCESS,			"SUCCESS"			}, 
{  FDR_TIMEOUT,			"TIMEOUT"			},
{  FDR_MEMALLOC,		"Memory Allocation Error"	},
{  FDR_MEMFAIL,			"Memory Failure"		},
{  FDR_REJECT,			"Command Reject"		},
{  FDR_BADDRV,			"Bad Drive #"			},
{  FDR_DATACRC,			"Data CRC Error"		},
{  FDR_HDRCRC,			"Header CRC Error"		},
{  FDR_MEDIA,			"Media Error"			},
{  FDR_SEEK,			"Seek Error"			},
{  FDR_BADPHASE,		"Bad Controller Phase"		},
{  FDR_DRIVE_FAIL,		"Basic Drive Failure"		},
{  FDR_NOHDR,			"Header Not Found"		},
{  FDR_WRTPROT,			"Disk Write Protected"		},
{  FDR_NO_ADDRS_MK,		"Missing Address Mark" 		},
{  FDR_CNTRL_MK,		"Missing Control Mark" 		},
{  FDR_NO_DATA_MK,		"Missing Data Mark" 		},
{  FDR_CNTRL_REJECT,		"Controller rejected command"	},
{  FDR_CNTRLR,			"Controller Handshake Error"	},
{  FDR_DMAOURUN,		"DMA Over/underrun"		},
{  FDR_VOLUNAVAIL,		"FDR_VOLUNAVAIL"		},
{  0,				NULL				}
};

#ifdef	DEBUG

struct reg_values fd_command_values[] = {
/* value			name */
{  FDCMD_BAD,			"FDCMD_BAD"		}, 
{  FDCMD_CMD_XFR,		"FDCMD_CMD_XFR"		}, 
{  FDCMD_EJECT,			"FDCMD_EJECT"		}, 
{  FDCMD_MOTOR_ON,		"FDCMD_MOTOR_ON"	}, 
{  FDCMD_MOTOR_OFF,		"FDCMD_MOTOR_OFF"	}, 
{  FDCMD_GET_STATUS,		"FDCMD_GET_STATUS"	}, 
{  FDCMD_NEWVOLUME,		"FDCMD_NEWVOLUME"	}, 
{  FDCMD_ABORT,			"FDCMD_ABORT"		}, 
{  0,				NULL			}
};

struct reg_values fv_state_values[] = {
/* value			name */
{  FVSTATE_IDLE,		"FVSTATE_IDLE"		}, 
{  FVSTATE_EXECUTING,		"FVSTATE_EXECUTING"	}, 
{  FVSTATE_RETRYING,		"FVSTATE_RETRYING"	}, 
{  FVSTATE_RECALIBRATING,	"FVSTATE_RECALIBRATING"	},
{  0,				NULL			}
};

struct reg_values fc_opcode_values[] = {
/* value			name */
{  FCCMD_READ,			"FCCMD_READ"		},
{  FCCMD_READ_DELETE,		"FCCMD_READ_DELETE"	},
{  FCCMD_WRITE,			"CMD_WRITE"		},
{  FCCMD_WRITE_DELETE,		"FCCMD_WRITE_DELETE"	},
{  FCCMD_READ_TRACK,		"FCCMD_READ_TRACK"	},
{  FCCMD_VERIFY,		"FCCMD_VERIFY"		},
{  FCCMD_VERSION,		"FCCMD_VERSION"		},
{  FCCMD_FORMAT,		"FCCMD_FORMAT"		},
{  FCCMD_RECAL,			"FCCMD_RECAL"		},
{  FCCMD_INTSTAT,		"FCCMD_INTSTAT"		},
{  FCCMD_SPECIFY,		"FCCMD_SPECIFY"		},
{  FCCMD_DRIVE_STATUS,		"FCCMD_DRIVE_STATUS"	},
{  FCCMD_SEEK,			"FCCMD_SEEK"		},
{  FCCMD_CONFIGURE,		"FCCMD_CONFIGURE"	},
{  FCCMD_DUMPREG,		"FCCMD_DUMPREG"		},
{  FCCMD_READID,		"FCCMD_READID"		},
{  FCCMD_PERPENDICULAR,		"FCCMD_PERPENDICULAR"	},
{  0,				NULL			}
};

#endif	DEBUG
/* end of fd_global.c */



















