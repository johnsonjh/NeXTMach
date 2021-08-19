/*
 * npreg.h	- Register definitions for NeXT Laser Printer
 *		  Interface.
 * HISTORY
 *  8-Mar-88  Peter King (king) at NeXT
 *	Created.
 */

#ifndef	__NPREG__
#define __NPREG__

struct np_regs {
	union {
		volatile u_int	csrU_int;
/* Bitmask so that we can do fast bit tests */
#define	DMAOUT_DMAEN	0x80000000
#define DMAOUT_DAV	0x40000000
#define DMAOUT_OVR	0x20000000
#define DMAIN_DMAEN	0x08000000
#define DMAIN_DAV	0x04000000
#define DMAIN_OVR	0x02000000
#define CTRL_LPON	0x00800000
#define CTRL_NDIBIT	0x00400000
#define CTRL_INT	0x00080000
#define	CTRL_DAV	0x00040000
#define	CTRL_OVR	0x00020000
#define	DTX_PEND	0x00008000
#define	DTX		0x00004000
#define	CTX_PEND	0x00002000
#define	CTX		0x00001000
#define	RTX_PEND	0x00000800
#define	RTX		0x00000400
#define	RESET		0x00000200
#define	TXLOOP		0x00000100
		struct {
		  volatile u_int dmaout_dmaen : 1, /* Enable DMA for dataout */
			dmaout_dav : 1,	  /* Data requested from engine */
			dmaout_ovr : 1,	  /* Dataout underrun packet rcvd */
			: 1,		  /* Reserved */
			dmain_dmaen : 1,  /* Enable DMA for data in */
			dmain_dav : 1,	  /* Input available to engine */
			dmain_ovr : 1,	  /* 1 = input channel overrun */
			: 1,		  /* Reserved */
			control_lpon : 1, /* 1 = turn on, 0 = turn off */
			control_ndibit : 1, /* 0 = normal, 1 = behave like
					       monitor serial interface */
			: 2,		  /* Reserved */
			control_int : 1,  /* Interrupt caused by underrun
					     or unrecognized packet */
			control_dav: 1,	  /* underrun or unrecognized
					     packet received */
			control_ovr : 1,  /* control data register overrun */
			: 1,		  /* Reserved */
			dtx_pend : 1,	  /* DMA transmit is pending */
			dtx : 1,	  /* DMA transmit in progress */
			ctx_pend : 1,	  /* CPU transmit is pending */
			ctx : 1,	  /* CPU transmit in progress */
			rtx_pend : 1,	  /* Receive operation in progress */
			rtx : 1,	  /* Receive operation has occured */
			reset : 1,	  /* 1 = normal operation,
					     0 = reset printer interface */
			txloop : 1,	  /* 0 = normal operation,
					     1 = loopback tx->rx */
			cmd : 8;
#define	NP_RESET		0xff
#define	NP_DATA_OUT		0xc7
#define	NP_GPI_MASK		0xc5
#define	NP_GP_OUT		0xc4
#define	NP_GP_IN		0x04
#define	NP_MARGINS		0xc2
#define	NP_COPYRIGHT		0xe7
#define	NP_DATAOUT_CTRL(opt)	(0x07 | (opt << 3))
		} csrU_struct;
	} np_csrU;
#define	np_csr		np_csrU.csrU_struct
#define np_csrint	np_csrU.csrU_int

	int	np_data;
/* Data used in the reset packet */
#define	NP_RESET_DATA		0xffffffff
};

/*
 * General purpose output and input signals to the printer.
 */
#define	GP_BITS_IN(x)		(~(x>>24) & 0x3f)
#define	GP_BITS_OUT(x)		(((~x) & 0xff) << 24)
#define	GP_BITS_MASK(x)		((x & 0x3f) << 24)

#define	GPOUT_300DPI		0x40 /* This is the inverse of 400DPI */
#define GPOUT_VSYNC		0x20
#define	GPOUT_PRINT		0x10
#define	GPOUT_CPRDY		0x08
#define	GPOUT_CCLK		0x04
#define	GPOUT_CMD		0x02
#define	GPOUT_CBSY		0x01

#define	GPIN_PPRDY		0x10
#define	GPIN_RDY		0x08
#define	GPIN_VSREQ		0x04
#define	GPIN_SBSY		0x02
#define	GPIN_STS		0x01

/*
 * Margin word.
 */
#define	MARGIN_REQC_MASK	0x7f
#define	MARGIN_REQC_SHIFT	16
#define	MARGIN_BITC_MASK	0x1ff

/*
 * Dataout commands.
 */
#define	DATAOUT_OFF		0x00
#define	DATAOUT_300DPI		0x07
#define	DATAOUT_400DPI		0x05

/*
 * Printer Commands and Status
 */
#define	CMD_ERROR		0x80
#define	CMD_PARITYERROR		0x40
#define	CMD_PARITY		0x01
/*
 * Status request 0 - basic status
 */
#define	SR0_CMD			0x01
	#define	SR0_PRINTREQ		0x40
	#define	SR0_PAPERDLVR		0x20
	#define	SR0_DATARETRANS		0x10
	#define	SR0_WAIT		0x08
	#define	SR0_PAUSE		0x04
	#define	SR0_CALL		0x02
/*
 * Status request 1 - operator call status
 */
#define	SR1_CMD			0x02
	#define	SR1_NOCARTRIDGE		0x40
	#define	SR1_NOPAPER		0x10
	#define	SR1_JAM			0x08
	#define	SR1_DOOROPEN		0x04
	#define	SR1_TESTPRINT		0x02
/*
 * Status request 2 - service call status
 */
#define	SR2_CMD			0x04
	#define	SR2_FIXINGASMBLY	0x40
	#define	SR2_POORBDSIG		0x20
	#define	SR2_SCANNINGMOTOR	0x10
/*
 * Status request 4 - number of sheets for retransmission
 */
#define	SR4_CMD			0x08
/*
 * Status request 5 - paper size status
 */
#define	SR5_CMD			0x0b
	#define	SR5_NOCASSETTE	0x01	/* 0x00<<1 + odd parity */
	#define	SR5_A4		0x02	/* 0x01<<1 + odd parity */
	#define	SR5_LETTER	0x08	/* 0x04<<1 + odd parity */
	#define	SR5_B5		0x13	/* 0x09<<1 + odd parity */
	#define	SR5_LEGAL	0x19	/* 0x0c<<1 + odd parity */
/*
 * Status request 15 - S status (toner cartridge)
 */
#define SR15_CMD	0x1f
	#define	SR15_NOTONER	0x04
/*
 * Executable commands
 */
#define	EXTCLK_CMD		0x40	/* EC0 */
#define	PRINTERCLK_CMD		0x43	/* EC1 */
#define	PAUSE_CMD		0x45	/* EC2 */
#define	UNPAUSE_CMD		0x46	/* EC3 */
#define	DRUMON_CMD		0x49	/* EC4 */
#define	DRUMOFF_CMD		0x4a	/* EC5 */
#define	CASSFEED_CMD		0x4c	/* EC6 */
#define	HANDFEED_CMD		0x4f	/* EC7 */
#define	RETRANSCANCEL_CMD	0x5d	/* EC14 */

#endif	__NPREG__
