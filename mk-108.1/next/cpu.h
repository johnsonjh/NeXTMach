/*	@(#)cpu.h	1.0	11/09/86	(c) 1986 NeXT	*/

/* 
 * HISTORY
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Define SLOT_ID_BMAP to be at the proper offset when the ROM 
 *	includes this file.
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	The BMAP registers (P_BMAP) are offset by SLOT_ID, not SLOT_ID_BMAP.
 *
 * 09-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 */ 

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cpus.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)cpu.h	7.1 (Berkeley) 6/5/86
 */
#ifndef	_CPU_
#define	_CPU_
 
#ifndef	ASSEMBLER
/* 
 *  We use these types in the definitions below and hence ought to include them
 *  directly to aid new callers (even if our caller is likely to have already
 *  done so).
 */
#import <sys/types.h>
#endif	ASSEMBLER

#ifndef	ASSEMBLER
#ifdef KERNEL
int	master_cpu;
#endif	KERNEL
#endif	ASSEMBLER

#ifdef	LOCORE_
#undef	LOCORE
#if	LOCORE_
#define	LOCORE	1
#endif	LOCORE_
#undef	LOCORE_
#endif	LOCORE_

/*
 * Implementation constants
 */
#define	N_SIMM		4		/* number of SIMM memory modules */

/*
 * Hardware revisions
 */
#ifdef KERNEL
#ifndef	ASSEMBLER
u_short	dma_chip;
u_short	disk_chip;
u_char	machine_type;			/* see next/scr.h for values */
u_char	board_rev;
u_char	cpu_type;
#endif	ASSEMBLER

/* cpu_type values */
#define	MC68030		0
#define	MC68040		1
#endif	KERNEL

/*
 * Base physical addresses of resources (slot-relative unless otherwise noted)
 */
#ifdef KERNEL
#ifndef	ASSEMBLER
volatile int *intrmask;
int intr_mask;
#endif	ASSEMBLER
#endif	KERNEL

#if	KERNEL

/* kernel */
#if	ASSEMBLER
#define	SLOT_ID		0x0
#define	SLOT_ID_BMAP	0x0
#else	ASSEMBLER
int slot_id;
#define	SLOT_ID		slot_id
int slot_id_bmap;
#define	SLOT_ID_BMAP	slot_id_bmap
#endif	ASSEMBLER
#else	KERNEL
#if	STANDALONE

/* standalone */
int slot_id;
#define	SLOT_ID		slot_id
int slot_id_bmap;
#define	SLOT_ID_BMAP	slot_id_bmap
#else	STANDALONE

/* ROM */
#define	SLOT_ID		0x0
#ifdef	MC68030
#define	SLOT_ID_BMAP	0x0
#endif	MC68030
#ifdef	MC68040
#define	SLOT_ID_BMAP	0x00100000
#endif	MC68040
#endif	STANDALONE
#endif	KERNEL

/* ROM */
#define	P_EPROM		(SLOT_ID+0x00000000)
#define P_EPROM_BMAP	(SLOT_ID+0x01000000)
#define	P_EPROM_SIZE	(128 * 1024)

/* device space */
#define	P_DEV_SPACE	(SLOT_ID+0x02000000)
#define	P_DEV_BMAP	(SLOT_ID+0x02100000)
#define	DEV_SPACE_SIZE	0x0001c000

/* DMA control/status (writes MUST be 32-bit) */
#define	P_SCSI_CSR	(SLOT_ID+0x02000010)
#define	P_SOUNDOUT_CSR	(SLOT_ID+0x02000040)
#define	P_DISK_CSR	(SLOT_ID+0x02000050)
#define	P_SOUNDIN_CSR	(SLOT_ID+0x02000080)
#define	P_PRINTER_CSR	(SLOT_ID+0x02000090)
#define	P_SCC_CSR	(SLOT_ID+0x020000c0)
#define	P_DSP_CSR	(SLOT_ID+0x020000d0)
#define	P_ENETX_CSR	(SLOT_ID+0x02000110)
#define	P_ENETR_CSR	(SLOT_ID+0x02000150)
#define	P_VIDEO_CSR	(SLOT_ID+0x02000180)
#define	P_M2R_CSR	(SLOT_ID+0x020001d0)
#define	P_R2M_CSR	(SLOT_ID+0x020001c0)

/* DMA scratch pad (writes MUST be 32-bit) */
#define	P_VIDEO_SPAD	(SLOT_ID+0x02004180)
#define	P_EVENT_SPAD	(SLOT_ID+0x0200418c)
#define	P_M2M_SPAD	(SLOT_ID+0x020041e0)

/* device registers */
#define	P_ENET		(SLOT_ID_BMAP+0x02006000)
#define	P_DSP		(SLOT_ID_BMAP+0x02008000)
#define	P_MON		(SLOT_ID+0x0200e000)
#define	P_PRINTER	(SLOT_ID+0x0200f000)
#define	P_DISK		(SLOT_ID_BMAP+0x02012000)
#define	P_SCSI		(SLOT_ID_BMAP+0x02014000)
#define	P_FLOPPY	(SLOT_ID_BMAP+0x02014100)
#define	P_TIMER		(SLOT_ID_BMAP+0x02016000)
#define P_TIMER_CSR	(SLOT_ID_BMAP+0x02016004)
#define	P_SCC		(SLOT_ID_BMAP+0x02018000)
#define	P_SCC_CLK	(SLOT_ID_BMAP+0x02018004)
#define	P_EVENTC	(SLOT_ID_BMAP+0x0201a000)
#define	P_BMAP		(SLOT_ID+0x020c0000)
/* All COLOR_FB registers are 1 byte wide */
#define P_C16_DAC_0	(SLOT_ID_BMAP+0x02018100)	/* COLOR_FB - RAMDAC */
#define P_C16_DAC_1	(SLOT_ID_BMAP+0x02018101)
#define P_C16_DAC_2	(SLOT_ID_BMAP+0x02018102)
#define P_C16_DAC_3	(SLOT_ID_BMAP+0x02018103)
#define P_C16_CMD_REG	(SLOT_ID_BMAP+0x02018180)	/* COLOR_FB - CSR */

/* system control registers */
#define	P_MEMTIMING	(SLOT_ID_BMAP+0x02006010)
#define	P_INTRSTAT	(SLOT_ID+0x02007000)
#define	P_INTRSTAT_CON	0x02007000
#define	P_INTRMASK	(SLOT_ID+0x02007800)
#define	P_INTRMASK_CON	0x02007800
#define	P_SCR1		(SLOT_ID+0x0200c000)
#define	P_SCR1_CON	0x0200c000
#define	P_SID		0x0200c800		/* NOT slot-relative */
#define	P_SCR2		(SLOT_ID+0x0200d000)
#define	P_SCR2_CON	0x0200d000
#define	P_RMTINT	(SLOT_ID+0x0200d800)
#define	P_BRIGHTNESS	(SLOT_ID_BMAP+0x02010000)
#define P_DRAM_TIMING	(SLOT_ID_BMAP+0x02018190) /* Warp 9C memory ctlr */
#define P_VRAM_TIMING	(SLOT_ID_BMAP+0x02018198) /* Warp 9C memory ctlr */

/* memory */
#define	P_MAINMEM	(SLOT_ID+0x04000000)
#define	P_MEMSIZE	0x04000000
#define	P_VIDEOMEM	(SLOT_ID+0x0b000000)
#define	P_VIDEOSIZE	0x0003a800
#define P_C16_VIDEOMEM	(SLOT_ID+0x06000000)	/* COLOR_FB */
#define P_C16_VIDEOSIZE	0x001D4000		/* COLOR_FB */
#define	P_WF4VIDEO	(SLOT_ID+0x0c000000)	/* w A+B-AB function */
#define	P_WF3VIDEO	(SLOT_ID+0x0d000000)	/* w (1-A)B function */
#define	P_WF2VIDEO	(SLOT_ID+0x0e000000)	/* w ceil(A+B) function */
#define	P_WF1VIDEO	(SLOT_ID+0x0f000000)	/* w AB function */
#define	P_WF4MEM	(SLOT_ID+0x10000000)	/* w A+B-AB function */
#define	P_WF3MEM	(SLOT_ID+0x14000000)	/* w (1-A)B function */
#define	P_WF2MEM	(SLOT_ID+0x18000000)	/* w ceil(A+B) function */
#define	P_WF1MEM	(SLOT_ID+0x1c000000)	/* w AB function */
#define	NMWF		4			/* # of memory write funcs */

/*
 * Interrupt structure.
 * BASE and BITS define the origin and length of the bit field in the
 * interrupt status/mask register for the particular interrupt level.
 * The first component of the interrupt device name indicates the bit
 * position in the interrupt status and mask registers; the second is the
 * interrupt level; the third is the bit index relative to the start of the
 * bit field.
 */
#define	I(l,i,b)	(((b) << 8) | ((l) << 4) | (i))
#define	I_INDEX(i)	((i) & 0xf)
#define	I_IPL(i)	(((i) >> 4) & 7)
#define	I_BIT(i)	( 1 << (((i) >> 8) & 0x1f))

#define	I_IPL7_BASE	0
#define	I_IPL7_BITS	2
#define	I_NMI		I(7,0,31)
#define	I_PFAIL		I(7,1,30)

#define	I_IPL6_BASE	2
#define	I_IPL6_BITS	12
#define	I_TIMER		I(6,0,29)
#define	I_ENETX_DMA	I(6,1,28)
#define	I_ENETR_DMA	I(6,2,27)
#define	I_SCSI_DMA	I(6,3,26)
#define	I_DISK_DMA	I(6,4,25)
#define	I_PRINTER_DMA	I(6,5,24)
#define	I_SOUND_OUT_DMA	I(6,6,23)
#define	I_SOUND_IN_DMA	I(6,7,22)
#define	I_SCC_DMA	I(6,8,21)
#define	I_DSP_DMA	I(6,9,20)
#define	I_M2R_DMA	I(6,10,19)
#define	I_R2M_DMA	I(6,11,18)

#define	I_IPL5_BASE	14
#define	I_IPL5_BITS	3
#define	I_SCC		I(5,0,17)
#define	I_REMOTE	I(5,1,16)
#define	I_BUS		I(5,2,15)

#define	I_IPL4_BASE	17
#define	I_IPL4_BITS	1
#define	I_DSP_4		I(4,0,14)

#define	I_IPL3_BASE	18
#define	I_IPL3_BITS	12
#define	I_DISK		I(3,0,13)
#define	I_C16_VIDEO	I(3,0,13)	/* COLOR_FB - Steals old ESDI interrupt */
#define	I_SCSI		I(3,1,12)
#define	I_PRINTER	I(3,2,11)
#define	I_ENETX		I(3,3,10)
#define	I_ENETR		I(3,4,9)
#define	I_SOUND_OVRUN	I(3,5,8)
#define	I_PHONE		I(3,6,7)
#define	I_DSP_3		I(3,7,6)
#define	I_VIDEO		I(3,8,5)
#define	I_MONITOR	I(3,9,4)
#define	I_KYBD_MOUSE	I(3,10,3)
#define	I_POWER		I(3,11,2)

#define	I_IPL2_BASE	30
#define	I_IPL2_BITS	1
#define	I_SOFTINT1	I(2,0,1)

#define	I_IPL1_BASE	31
#define	I_IPL1_BITS	1
#define	I_SOFTINT0	I(1,0,0)

#if	NCPUS == 1
#define CPU_NUMBER_d0		clrl	d0;
#define	cpu_number()		(0)
#else	NCPUS == 1
#define	CPU_NUMBER_d0		movc	msp,d0; \
				andl	\#0xff,d0;
#endif	NCPUS == 1

#endif	_CPU_

