/*	@(#)scr.h	1.0	12/02/86	(c) 1986 NeXT	*/

/* 
 * HISTORY
 * 15-Mar-90  John Seamons (jks) at NeXT
 *	Added missing SCR2 bits for DSP.
 *
 *  7-Mar-90  John Seamons (jks) at NeXT
 *	Moved bmap chip definitions to a seperate file (next/bmap.h).
 *
 * 02-Dec-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 */ 

/*
 *	machine type:	board rev:	description:
 *	NeXT_CUBE		0	zs: DCD input incorrectly inverted.
 *	NeXT_CUBE		1	zs: DCD input polarity fixed.
 *	NeXT_CUBE		2	dsp: must disable ext DSP mem before DSP reset.
 *
 *	NeXT_WARP9		0
 *
 *	NeXT_X15		0
 *
 *	NeXT_WARP9C		0
 */
 
/*
 *	System control register definitions
 */

/* Machine types, used in both assembler and C sources. */
#define	NeXT_CUBE	0
#define	NeXT_WARP9	1
#define	NeXT_X15	2
#define	NeXT_WARP9C	3

#ifndef	ASSEMBLER
struct scr1 {				/* loaded at power-on, read-only */
	u_int	s_slot_id : 4,		/* slot id */
		: 4,
		s_dma_rev : 8,		/* DMA chip revision # */
		s_cpu_rev : 8,		/* CPU board revision # */
#define	MACHINE_TYPE(x) \
	((x) >> 4)			/* convention: upper 4 bits is machine type */
#define	BOARD_REV(x) \
	((x) & 0xf)			/* convention: lower 4 bits is board rev */
		s_vmem_speed : 2,	/* video mem speed select */
		s_mem_speed : 2,	/* main mem speed select */
#define	MEM_120ns	0
#define	MEM_100ns	1
#define	MEM_80ns	2
#define	MEM_60ns	3
		s_reserved : 2,
		s_cpu_clock : 2;	/* CPU and FPU clock */
#define	CPU_40MHz	0
#define	CPU_20MHz	1
#define	CPU_25MHz	2
#define	CPU_33MHz	3
};

/*
 * CAUTION: On Warp 9C product, s_vmem_speed, s_mem_speed, and s_cpu_clock
 * fields are not meaningful!
 */

struct scr2 {				/* zeroed at power-on, read/write */
	u_int	s_dsp_reset : 1,
		s_dsp_block_end : 1,
		s_dsp_unpacked : 1,
		s_dsp_mode_B : 1,
		s_dsp_mode_A : 1,
		s_remote_int : 1,
		s_local_int : 2,
		s_dram_256K : 4,
		s_dram_1M : 4,
		s_timer_on_ipl7 : 1,
		s_rom_wait_states : 3,
		s_rom_1M : 1,
		s_rtdata : 1,
		s_rtclk : 1,
		s_rtce : 1,
		s_rom_overlay : 1,
		s_dsp_int_en : 1,
		s_dsp_mem_en : 1,
		s_reserved : 4,
		s_led : 1;
};
#endif	ASSEMBLER

#define	SCR2_LOCALINT	0x01000000
#define	SCR2_OVERLAY	0x00000080
#define	SCR2_PAGE_MODE	0x00110000
#define	SCR2_DRAM_1M	0x00010000	/* first bit in field */
#define	SCR2_ROM_1M	0x00000800
#define	SCR2_EKG_LED	0x00000001
#define	SCR2_RTDATA	0x00000400
#define	SCR2_RTCLK	0x00000200
#define	SCR2_RTCE	0x00000100
#define SCR2_TIMERIPL7	0x00008000

/* brightness register */
#define	BRIGHT_ENABLE	0x40
#define	BRIGHT_MASK	0x3f
#define	BRIGHT_MAX	0x3d
#define	BRIGHT_MIN	0x00

/* timer register */
#define TIMER_ENABLE	0x80
#define TIMER_UPDATE	0x40
#define TIMER_MAX	0xffff		/* Maximum value of timer */

#ifndef	ASSEMBLER
volatile struct scr1 *scr1;
volatile int *scr2;
volatile u_char *brightness;
#endif	ASSEMBLER

