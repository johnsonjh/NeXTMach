/*	@(#)odvar.h	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */

/* error control */
struct od_err {
	u_char	e_action;		/* error action */
#define	EA_ADV		0		/* advisory only */
#define	EA_RETRY	1		/* just retry previous operation */
#define	EA_RTZ		2		/* must RTZ immediately */
#define	EA_EJECT	3		/* eject disk immediately */
#define	EA_RESPIN	4		/* spin down and up again */
	u_char	e_retry;		/* number of retries to attempt */
	u_char	e_rtz;			/* number of RTZs to attempt */
	char	*e_msg;
} od_err[] = {
#define	E_NO_ERROR	0
	EA_RETRY, 3, 1,	"no error?",
#define	E_TIMING	1
	EA_RETRY, 10, 1, "formatter/ECC over(under)run",
#define	E_CMP		2
	EA_RETRY, 3, 1, "compare mismatch",
#define	E_ECC		3
	EA_RETRY, 10, 1, "ECC",
#define	E_TIMEOUT	4
	EA_RETRY, 3, 0, "sector timeout",
#define	E_FAULT		5
	EA_RETRY, 3, 1, "read fault",
#define	E_D_PARITY	6
	EA_RETRY, 3, 1, "drive parity error",

#define	E_RESV_1	7
#define	E_STATUS_BASE	E_RESV_1
	EA_RETRY, 3, 1, "reserved 1",
#define	E_INSERT	8
	EA_RETRY, 0, 0, "load completed",
#define	E_RESET		9
	EA_ADV,	  3, 1, "power on reset",
#define	E_SEEK		10
	EA_RTZ,   3, 0, "address fault",
#define	E_CMD		11
	EA_RESPIN,3, 0, "invalid or unimplemented command",
#define	E_INTERFACE	12
	EA_RETRY, 3, 1, "interface fault",
#define	E_I_PARITY	13
	EA_RETRY, 3, 1, "interface parity error",
#define	E_RESV_2	14
	EA_RETRY, 3, 1, "reserved 2",
#define	E_STOPPED	15
	EA_ADV,	  3, 1, "not spinning",
#define	E_SIDE		16
	EA_EJECT, 0, 0, "media upside down",
#define	E_SERVO		17
	EA_ADV,	  3, 1, "servo not ready",
#define	E_POWER		18
	EA_RETRY, 3, 1, "laser power alarm",
#define	E_WP		19
	EA_RETRY, 0, 0, "disk write protected",
#define	E_EMPTY		20
	EA_RETRY, 0, 0, "no disk inserted",
#define	E_BUSY		21
	EA_RETRY, 3, 1, "execute busy",

#define	E_RF		22
#define	E_EXTENDED_BASE	E_RF
	EA_RETRY, 3, 1, "RF detect",
#define	E_RESV_3	23
	EA_RETRY, 3, 1, "reserved 3",
#define	E_WRITE_INH	24
	EA_RETRY, 3, 1, "write inhibit (high temperature)",
#define	E_WRITE		25
	EA_RETRY, 3, 1, "write mode failed",
#define	E_COARSE	26
	EA_RETRY, 3, 1, "coarse seek failed",
#define	E_TEST		27
	EA_RETRY, 3, 1, "test write failed",
#define	E_SLEEP		28
	EA_RETRY, 3, 1, "sleep/wakeup failed",
#define	E_LENS		29
	EA_RTZ,   3, 0, "lens out of range",
#define	E_TRACKING	30
	EA_RTZ,   3, 0, "tracking servo failed",
#define	E_PLL		31
	EA_RETRY, 10, 1, "PLL failed",
#define	E_FOCUS		32
	EA_RTZ,   3, 0, "focus failed",
#define	E_SPEED		33
	EA_RTZ,   3, 0, "not at speed",
#define	E_STUCK		34
	EA_EJECT, 0, 0, "disk cartridge stuck",
#define	E_ENCODER	35
	EA_RTZ,   3, 0, "linear encoder failed",
#define	E_LOST		36
	EA_RTZ,   3, 0, "tracing failure",

#define	E_RESV_5	37
#define	E_HARDWARE_BASE	E_RESV_5
	EA_RETRY, 3, 1, "reserved 5",
#define	E_RESV_6	38
	EA_RETRY, 3, 1, "reserved 6",
#define	E_RESV_7	39
	EA_RETRY, 3, 1, "reserved 7",
#define	E_RESV_8	40
	EA_RETRY, 3, 1, "reserved 8",
#define	E_RESV_9	41
	EA_RETRY, 3, 1, "reserved 9",
#define	E_RESV_10	42
	EA_RETRY, 3, 1, "reserved 10",
#define	E_LASER		43
	EA_RESPIN,3, 0, "laser power failed",
#define	E_INIT		44
	EA_RETRY, 3, 1, "drive init failed",
#define	E_TEMP		45
	EA_EJECT, 0, 0, "high drive temperature",
#define	E_CLAMP		46
	EA_EJECT, 0, 0, "spindle clamp misaligned",
#define	E_STOP		47
	EA_EJECT, 0, 0, "spindle stop timeout",
#define	E_TEMP_SENS	48
	EA_EJECT, 0, 0, "temperature sensor failed",
#define	E_LENS_POS	49
	EA_RESPIN,3, 0, "lens position failure",
#define	E_SERVO_CMD	50
	EA_RESPIN,3, 0, "servo command failure",
#define	E_SERVO_TIMEOUT	51
	EA_RESPIN,3, 0, "servo timeout failure",
#define	E_HEAD		52
	EA_RESPIN,3, 0, "head select failure",

#define	E_SOFT_TIMEOUT	53
	EA_RESPIN,3, 0, "soft timeout",
#define	E_BAD_BAD	54
	EA_RETRY, 0, 0, "bitmap bad but no alternate found!",
#define	E_BAD_REMAP	55
	EA_RETRY, 0, 0, "no more alternates for remap!",
#define	E_STARVE	56
	EA_RETRY, 3, 1, "DMA starvation",
#define	E_DMA_ERROR	57
	EA_RETRY, 3, 1, "DMA error",
#define	E_BUSY_TIMEOUT1	58
	EA_RETRY, 3, 1, "busy timeout #1",
#define	E_BUSY_TIMEOUT2	59
	EA_RETRY, 3, 1, "busy timeout #2",
#define	E_EXCESSIVE_ECC	60
	EA_RETRY, 3, 1, "excessive ECC errors",
};

/* drive commands */
struct od_dcmd {
	u_short	dc_cmd;
	u_short	dc_mask;
	char	*dc_msg;
} od_dcmd[] = {
#define	OMD1_SEK	0x0000
	OMD1_SEK,	0xf000,	"seek",
#define	OMD1_HOS	0xa000
	OMD1_HOS,	0xfff0, "set high-order seek addr",
#define	OMD1_REC	0x1000
	OMD1_REC,	0xffff, "recalibrate",
#define	OMD1_RDS	0x2000
	OMD1_RDS,	0xffff, "request drive status",
#define	OMD1_RCA	0x2200
	OMD1_RCA,	0xffff, "request current address",
#define	OMD1_RES	0x2800
	OMD1_RES,	0xffff, "request extended status",
#define	OMD1_RHS	0x2a00
	OMD1_RHS,	0xffff, "request hardware status",
#define	OMD1_RGC	0x3000
	OMD1_RGC,	0xffff, "request general config",
#define	OMD1_RVI	0x3f00
	OMD1_RVI,	0xffff, "request vendor ident",
#define	OMD1_SRH	0x4100
	OMD1_SRH,	0xffff, "select read head",
#define	OMD1_SVH	0x4200
	OMD1_SVH,	0xffff, "select verify head",
#define	OMD1_SWH	0x4300
	OMD1_SWH,	0xffff, "select write head",
#define	OMD1_SEH	0x4400
	OMD1_SEH,	0xffff, "select erase head",
#define	OMD1_SFH	0x4500
	OMD1_SFH,	0xffff, "select RF head",
#define	OMD1_RID	0x5000
	OMD1_RID,	0xffff, "reset attn & status",
#define	OMD1_SPM	0x5200
	OMD1_SPM,	0xffff, "stop spindle motor",
#define	OMD1_STM	0x5300
	OMD1_STM,	0xffff, "start spindle motor",
#define	OMD1_LC		0x5400
	OMD1_LC,	0xffff, "lock cartridge",
#define	OMD1_ULC	0x5500
	OMD1_ULC,	0xffff, "unlock cartridge",
#define	OMD1_EC		0x5600
	OMD1_EC,	0xffff, "eject cartridge",
#define	OMD1_SOO	0x5900
	OMD1_SOO,	0xffff, "spiral operation on",
#define	OMD1_SOF	0x5a00
	OMD1_SOF,	0xffff, "spiral operation off",
#define	OMD1_RSD	0x8000	
	OMD1_RSD,	0xffff, "request self-diagnostic",
#define	OMD1_SD		0xb000
	OMD1_SD,	0xf000, "send data",
#define	OMD1_RJ		0x5100
	OMD1_RJ,	0xff00,	"relative jump",
	0,		0,
};

/* formatter commands */
struct od_fcmd {
	short	fc_cmd;
	char	*fc_msg;
} od_fcmd[] = {
#define	OMD_ECC_READ	0x80
	OMD_ECC_READ,	"ECC read",
#define	OMD_ECC_WRITE	0x40
	OMD_ECC_WRITE,	"ECC write",
#define	OMD_RD_STAT	0x20
	OMD_RD_STAT,	"read status",
#define	OMD_ID_READ	0x10		/* must reset bit manually */
	OMD_ID_READ,	"ID read",
#define	OMD_VERIFY	0x08
	OMD_VERIFY,	"verify",
#define	OMD_ERASE	0x04
	OMD_ERASE,	"erase",
#define	OMD_READ	0x02
	OMD_READ,	"read",
#define	OMD_WRITE	0x01
	OMD_WRITE,	"write",
/* pseudo-commands FIXME: find a better way to issue arbitrary cmds? */
#define	OMD_SPINUP	0xf0
	OMD_SPINUP,	"spinup",
#define	OMD_EJECT	0xf1
	OMD_EJECT,	"eject",
#define	OMD_SEEK	0xf2
	OMD_SEEK,	"seek",
#define	OMD_SPIRAL_OFF	0xf3
	OMD_SPIRAL_OFF,	"spiral off",
#define	OMD_RESPIN	0xf4
	OMD_RESPIN,	"respin",
#define	OMD_TEST	0xf5
	OMD_TEST,	"test",
#define	OMD_EJECT_NOW	0xf6
	OMD_EJECT_NOW,	"eject_now",
	0,		0,
};

/*
 * DKIOCREQ mappings
 */
struct dr_cmdmap {
	u_char	dc_cmd;
	daddr_t	dc_blkno;
	u_short	dc_flags;
#define	DRF_SPEC_RETRY	0x0001		/* user specified retry/rtz values */
#define	DRF_CERTIFY	0x0002		/* media certification processing */
	char	dc_retry;		/* # of retries before aborting */
	char	dc_rtz;			/* # of restores before aborting */
	u_short	dc_wait;		/* wait time after cmd */
};

struct dr_errmap {
	char	de_err;
	u_int	de_ecc_count;		/* # of ECCs required */
};

struct od_stats {
	int	s_vfy_retry;
	int	s_vfy_failed;
};
