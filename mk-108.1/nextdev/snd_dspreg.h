/*	@(#)dspreg.h	1.0	12/11/87	(c) 1988 NeXT	*/

/* 
 * HISTORY
 * 22-Dec-87  Gregg Kellogg (gk) at NeXT
 *	Created
 */ 

#ifndef	_SND_DSPREG_
#define	_SND_DSPREG_

#import <next/cpu.h>

/* structure view of dsp registers */
struct dsp_regs {
	u_char	icr;
#define ICR_INIT	0x80
#define ICR_HM1		0x40
#define ICR_HM0		0x20
#define ICR_HF1		0x10
#define ICR_HF0		0x08
#define	ICR_TREQ	0x02
#define	ICR_RREQ	0x01
	u_char	cvr;
#define CVR_HC		0x80
#define	CVR_HV		0x1f
	u_char	isr;
#define ISR_HREQ	0x80
#define ISR_DMA		0x40
#define ISR_HF3		0x10
#define ISR_HF2		0x08
#define	ISR_TRDY	0x04
#define ISR_TXDE	0x02
#define ISR_RXDF	0x01
	u_char	ivr;
	union {
		u_int	receive_i;
		struct {
			u_char	pad;
			u_char	h;
			u_char	m;
			u_char	l;
		} receive_struct;
		struct {
			u_short	pad;
			u_short	s;
		} receive_s;
		u_int	transmit_i;
		struct {
			u_char	pad;
			u_char	h;
			u_char	m;
			u_char	l;
		} transmit_struct;
		struct {
			u_short pad;
			u_short s;
		} transmit_s;
	} data;
};

#if	KERNEL

/* Receive and transmit bytes */
#define rxh data.receive_struct.h
#define rxm data.receive_struct.m
#define rxl data.receive_struct.l
#define rxs data.receive_s.s

#define txh data.transmit_struct.h
#define txm data.transmit_struct.m
#define txl data.transmit_struct.l
#define txs data.transmit_s.s

/* Receive and transmit words */
#define transmit data.transmit_i
#define receive data.receive_i
#endif	KERNEL

/* DSP SCR2 bits */
#define DSP_RESET	0x80000000	/* Reset the DSP (active low) */
#define DSP_BLOCK_E_E	0x40000000	/* DSP Block End Enable */
#define DSP_UNPACK_E	0x20000000	/* DSP Unpacked Enable */
#define	DSP_MODE_B	0x10000000	/* DSP Mode B */
#define DSP_MODE_A	0x08000000	/* DSP Mode A */

#define DSP_SCR2_MASK	0xf8000000	/* DSP bits in SCR2 */

#define DSP_SCR2_INTREN	0x00000040	/* Enable dsp interrupts @ level 4 */
#define DSP_SCR2_MEMEN	0x00000020	/* !DSP memory enable */

#endif	_SND_DSPREG_

