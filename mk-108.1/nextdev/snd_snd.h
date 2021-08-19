/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 *  2-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Moved ramp_buf out of ll_snd_dma struct into ll_snd_var struct.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef _SND_SND_
#define _SND_SND_

#import <nextdev/snd_var.h>
#import <nextdev/busvar.h>

/*
 * Low-level per-device data structure for DAC/ADC
 */
typedef struct ll_snd_var {
	struct ll_snd_dma {
		struct dma_chan	chan;
		struct dma_hdr	spare;		// for spliting up single dma's
		snd_queue_t	sndq;		// info for region management
		volatile int	ndma;		// number dmas on dev
		u_int		shutdown:1;	// don't accept dma reqs
	} dma[2];
#define D_WRITE SND_DIR_PLAY
#define D_READ SND_DIR_RECORD
	volatile u_char		 dmaflags;
#define SOUT_ENAB	1
#define SOUT_DOUB	2
#define SOUT_ZERO	4
#define SIN_ENAB	1
	volatile u_char		 gpflags;
#define SGP_VSCS	0x01
#define SGP_VSCD	0x02
#define	SGP_VSCCK	0x04
#define SGP_LOWPASS	0x08
#define SGP_SPKREN	0x10
	u_int			ramp;		// ramp flags
	short			*ramp_buf;	// used to ramp on underflow
} ll_snd_var_t;

#define VOLUME_MAX	0x2b
#define VOLUME_MIN	0
#define VOLUME_MASK	0x3f
#define VOLUME_RCHAN	0x80
#define VOLUME_LCHAN	0x40

#if	KERNEL
extern volatile int vol_r, vol_l, gpflags;
#endif	KERNEL

/*
 * Routine prototypes
 */
#if	KERNEL
void snd_device_init(int direction);
void snd_device_reset(int direction);
void snd_device_set_parms(u_int parms);
void snd_device_set_ramp(u_int parms);
u_int snd_device_get_parms(void);
void snd_device_set_volume(u_int volume);
u_int snd_device_get_volume(void);
boolean_t snd_device_start(snd_dma_desc_t *ddp, int direction, int rate);
void snd_device_vol_set(void);
void snd_device_vol_save(void);
int snd_device_probe(caddr_t addr, register int unit);
int snd_device_attach(struct bus_device *bd);
int snd_device_def_dma_size(int direction);
int snd_device_def_high_water(int direction);
int snd_device_def_low_water(int direction);
#endif	KERNEL
#endif	_SND_SND_

