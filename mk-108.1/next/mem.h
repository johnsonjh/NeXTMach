/*	@(#)mem.h	1.0	9/2/87		(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 2-Sep-87  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 **********************************************************************
 */ 

#ifndef _MEM_
#define	_MEM_

/* Memory timing control registers */
#define	NMTREG	5			/* number of registers */

#define	MT0_CPU_VIDEO_2			0x80
#define	MT0_CPU_VIDEO_3			0x40
#define	MT0_CPU_VIDEO_4			0x00
#define	MT0_CPU_VIDEO_5			0xc0
#define	MT0_CPU_CLK_REPLACE		0x20
#define	MT0_CPU_MAIN_CAS_LAST		0x10
#define	MT0_CPU_MAIN_CAS_LAST_1		0x00
#define	MT0_CPU_MAIN_LEN_NORM_1		0x0c
#define	MT0_CPU_MAIN_LEN_NORM_2		0x08
#define	MT0_CPU_MAIN_LEN_NORM_3		0x04
#define	MT0_CPU_MAIN_LEN_NORM_4		0x00
#define	MT0_CPU_MAIN_LEN_REPL_3		0x04
#define	MT0_CPU_MAIN_LEN_REPL_4		0x00
#define	MT0_CPU_MAIN_LEN_REPL_5		0x0c
#define	MT0_CPU_MAIN_LEN_REPL_6		0x08
#define	MT0_CPU_START_LEN_3		0x01
#define	MT0_CPU_START_LEN_4		0x02
#define	MT0_CPU_START_LEN_5		0x03
#define	MT0_CPU_START_LEN_6		0x00

#define	MT1_CPU_MAIN_SM1_SHORT		0x80
#define	MT1_CPU_MAIN_SM1_NORM		0x00
#define	MT1_CPU_REPL_SM1_SHORT		0x40
#define	MT1_CPU_REPL_SM1_NORM		0x00
#define	MT1_DMA_VIDEO_LEN_4		0x20
#define	MT1_DMA_VIDEO_LEN_6		0x00
#define	MT1_DMA_MAIN_LEN_2		0x10
#define	MT1_DMA_MAIN_LEN_4		0x00
#define	MT1_DMA_START_LEN_2		0x08
#define	MT1_DMA_START_LEN_4		0x04
#define	MT1_DMA_START_LEN_6		0x00
#define	MT1_CPU_VIDEO_CAS_LAST		0x01
#define	MT1_CPU_VIDEO_CAS_LAST_1	0x00
#define	MT1_CPU_VIDEO_CAS_LAST_2	0x03
#define	MT1_CPU_VIDEO_CAS_LAST_3	0x02

#define	MT2_CPU_RAS_20			0x80
#define	MT2_CPU_RAS_30			0x40
#define	MT2_CPU_RAS_40			0x20
#define	MT2_CPU_RAS_50			0x10
#define	MT2_CPU_RAS_60			0x08
#define	MT2_DMA_RAS_20			0x04
#define	MT2_DMA_RAS_30			0x02
#define	MT2_DMA_RAS_40			0x00
#define	MT2_RAC_20_20			0x01
#define	MT2_RAC_30_30			0x00

#define	MT3_CPU_VIDEO_CAS_START_20	0xc0
#define	MT3_CPU_VIDEO_CAS_START_30	0x80
#define	MT3_CPU_VIDEO_CAS_START_40	0x00
#define	MT3_CPU_MAIN_CAS_START_NORM_20	0x18
#define	MT3_CPU_MAIN_CAS_START_NORM_30	0x10
#define	MT3_CPU_MAIN_CAS_START_NORM_40	0x00
#define	MT3_CPU_MAIN_CAS_START_REPL_20	0x10
#define	MT3_CPU_MAIN_CAS_START_REPL_30	0x08
#define	MT3_CPU_MAIN_CAS_START_REPL_40	0x04
#define	MT3_CPU_MAIN_CAS_START_REPL_50	0x02
#define	MT3_CPU_MAIN_CAS_START_REPL_60	0x01

#define	MT4_DMA_VIDEO_CAS_START_20	0x0c
#define	MT4_DMA_VIDEO_CAS_START_30	0x08
#define	MT4_DMA_VIDEO_CAS_START_40	0x00
#define	MT4_DMA_MAIN_CAS_START_20	0x03
#define	MT4_DMA_MAIN_CAS_START_30	0x02
#define	MT4_DMA_MAIN_CAS_START_40	0x00

#endif	_MEM_
