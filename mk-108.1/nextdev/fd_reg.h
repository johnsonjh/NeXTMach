/*	@(#)fd_reg.h	2.0	01/19/89	(c) 1990 NeXT	         
 *
 * fd_reg.h -- register definitions for Intel 82077 Floppy Disk Controller
 * KERNEL VERSION 
 *
 * HISTORY
 * 19-Jan-90	Doug Mitchell at NeXT
 *	Created.
 *************************************************************************/

#ifndef _FDREG_
#define	_FDREG_

#import <sys/types.h>

/*
 *	hardware registers on the 82077
 */
struct fd_cntrl_regs {
	u_char	sra;			/* (R/O) status register A */		
	u_char	srb;			/* (R/O) status register B */
	u_char	dor;			/* (R/W) digital output register */
	u_char	rsvd3;			/* reserved */
	u_char	msr;			/* (R/O) main status register */
#define dsr	msr			/* (W/O) data rate select register */
	u_char	fifo;			/* (R/W) data FIFO */
	u_char	rsvd6;			/* reserved */
	u_char	dir;			/* (R/O) digital input register */
#define ccr	dir			/* (W/O) configuration control reg */
	u_char	flpctl;			/* (W/O) control (not in 82077) */
};

typedef	volatile struct fd_cntrl_regs *fd_cntrl_regs_t;

/*
 *	hardware register bits
 */
 
/*
 * sra (status register A). Read Only. 
 */
#define SRA_INTPENDING		0x80	/* interrupt pending */
#define SRA_DRV1_NOT		0x40	/* 2nd drive present. True LOW. */
#define SRA_STEP		0x20
#define SRA_TRK0_NOT		0x10	/* Head at Track 0. True LOW. */
#define SRA_HDSEL		0x08	/* Head Select */
#define SRA_INDX_NOT		0x04	/* Index Pulse. True LOW. */
#define SRA_WP_NOT		0x02	/* Write Protect. True LOW. */
#define SRA_DIR			0x01	/* Step Direction. */

/*
 * srb (status register B). Read Only.
 */
#define SRB_NOTUSED		0xC0	/* not used (read as 1's) */
#define SRB_DRVSEL0_NOT		0x20	/* Drive 0 Selected. True Low. */
#define SRB_WRDATA		0x10	/* WRDATA toggle */
#define SRB_RDDATA		0x08	/* RDDATA toggle */
#define SRB_WE			0x04	/* Write Enable */
#define SRB_MOTEN1		0x02	/* Motor enable 1 */
#define SRB_MOTEN0		0x01	/* Motor enable 0 */

/*
 * dor (digital output register). Read/Write.
 */
#define DOR_MOTEN3		0x80	/* motor enable 3 */
#define DOR_MOTEN2		0x40	/* motor enable 2 */
#define DOR_MOTEN1		0x20	/* motor enable 1 */
#define DOR_MOTEN0		0x10	/* motor enable 0 */
#define DOR_DMAGATE_NOT		0x08	/* DMA gate. True LOW. */
#define DOR_RESET_NOT		0x04	/* Reset. True LOW. */
#define DOR_DRVSEL1		0x02	/* Encoded drive select */
#define DOR_DRVSEL0		0x01
#define DOR_DRVSEL_MASK		0x03

/*
 * msr (main status register). Read Only.
 */
#define MSR_RQM			0x80	/* Host can transfer data if true. */
#define MSR_DIO			0x40	/* direction of data transfer */
#define DIO_READ		MSR_DIO
#define DIO_WRITE		0
#define MSR_NONDMA		0x20	
#define MSR_CMDBSY		0x10	/* command in progress */
#define MSR_DRVBSY3		0x08	/* drive 3 busy */
#define MSR_DRVBSY2		0x04
#define MSR_DRVBSY1		0x02
#define MSR_DRVBSY0		0x01
#define MSR_POLL_BITS		(MSR_RQM|MSR_DIO)

/*
 * dsr (data rate select register). Write Only.
 */
#define DSR_SWRESET		0x80	/* software reset */
#define DSR_PWR_DOWN		0x40	/* power down */
#define DSR_NOTUSED		0x20	/* not used. must be 0. */
#define DSR_PRECOMP2		0x10	/* Precomp. See PRECOMP_xxx values, */
#define DSR_PRECOMP1		0x08	/*    below. */
#define DSR_PRECOMP0		0x04
#define DSR_DRATE1		0x02	/* data rate select 1. See DRATE_xxx */
#define DSR_DRATE0		0x01	/*    values, below. */

/*
 * dir (digital input register). Read Only.
 */
#define DIR_DSKCHG		0x80	/* disk changed (disk out?) */
#define DIR_HIDENS_NOT		0x01	/* high density (true LOW) */

/* 
 * ccr (configuration control). Write Only. These appear to be the
 * same bits as in dsr<1..0>...
 */
#define CCR_DRATE1		0x02	/* data rate select 1. See DRATE_xxx */
#define CCR_DRATE0		0x01	/*    values, below. */

/*
 * flpctl (floppy control register). Write Only.
 */
#define FLC_EJECT		0x80	/* eject disk */
#define FLC_DS0			0x40	/* density select bit 0 - inverted. Old
					 * Cube prototypes only. */
#define FLC_82077_SEL		0x40	/* set = 82077, clear = 53C90A */
#define FLC_DRIVEID		0x04	/* Drive present. True LOW. */
#define FLC_MID1		0x02	/* Media ID bit 1 */
#define FLC_MID0		0x01	/* Media ID bit 0 */
					/* values for MID bits in fd_extern.h
					 * (FD_MID_xxx) */
#define FLC_MID_MASK		(FLC_MID1|FLC_MID0)
				
/* 
 * DSR_PRECOMPx values
 */
#define PRECOMP_0		0x1c	/* precomp disabled */
#define PRECOMP_41_67		0x04	/* 41.67 ns */
#define PRECOMP_83_34		0x08	/* 83.34 ns */
#define PRECOMP_125_00		0x0C	/* 125 ns */
#define PRECOMP_166_67		0x10	/* 166.67 ns */
#define PRECOMP_208_33		0x14	/* 208.33 ns */
#define PRECOMP_250_00		0x18	/* 250 ns */
#define PRECOMP_DEFAULT		0	/* default */

/*
 * DSR_DRATEx, CCR_DRATEx values
 */
#define DRATE_MFM_1000		0x03	/* 1 Mbit MFM */
#define DRATE_MFM_500		0x00	/* 500 Kbit MFM */
#define DRATE_FM_250		0x00	/* 250 Kbit FM */
#define DRATE_MFM_300		0x01	/* 300 Kbit MFM */
#define DRATE_FM_150		0x01	/* 150 Kbit FM */
#define DRATE_MFM_250		0x02	/* 250 Kbit MFM */
#define DRATE_FM_125		0x02	/* 125 Kbit FM */

/*
 * OD controller DISR register 
 */
#define DISR_ADDRS	(P_DISK + 4)

/*
 * SCSI/floppy DMA control register (fd_controller.sczctl)
 */
#define SDC_ADDRS		0x2014020
#define	SDC_CLKMASK		0xc0	/* clock selection bits */
#define	SDC_10MHZ		0x00	/* 10 MHz clock */
#define	SDC_12MHZ		0x40	/* 12.5 MHz clock */
#define	SDC_16MHZ		0xc0	/* 16.6 MHz clock */
#define	SDC_20MHZ		0x80	/* 20 MHz clock */
#define	SDC_INTENABLE		0x20	/* interrupt enable */
#define	SDC_DMAMODE		0x10	/* 1 => dma, 0 => pio */
#define	SDC_DMAREAD		0x08	/* 1 => dma from scsi to mem */
#define	SDC_FLUSH		0x04	/* flush fifo */
#define	SDC_RESET		0x02	/* reset scsi chip */
#define	SDC_WD33C92		0x01	/* 1 => WD33C92, 0 => NCR 53[89]0 */

/*
 * dma fifo status register (fd_controller.sczfst)
 */
#define SFS_ADDRS		0x2014021
#define	SFS_STATE		0xc0	/* DMA/SCSI bank state */
#define	SFS_D0S0		0x00	/* DMA rdy for bank 0, SCSI in 
					 * bank 0 */
#define	SFS_D0S1		0x40	/* DMA req for bank 0, SCSI in 
					 * bank 1 */
#define	S5RDMAS_D1S1		0x80	/* DMA rdy for bank 1, SCSI in 
					 * bank 1 */
#define	S5RDMAS_D1S0		0xc0	/* DMA req for bank 1, SCSI in 
					 * bank 0 */
#define	S5RDMAS_OUTFIFOMASK	0x38	/* output fifo byte (INVERTED) */
#define	S5RDMAS_INFIFOMASK	0x07	/* input fifo byte (INVERTED) */


/*
 * misc. controller-specific timing
 */
#define FC_RESET_HOLD	50		/* us to hold reset true */
#endif	_FDREG_

















