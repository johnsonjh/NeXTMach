/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 *  6-Mar-90  John Seamons (jks) at NeXT
 *	Added check to keep event_disk() from dividing by zero.
 *
 * 28-Sep-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Made conditional.
 *
 * 12-Aug-88  John Seamons (jks) at NeXT
 *	Created.
 */

#import <eventmeter.h>

#if	EVENTMETER

#import <sys/types.h>
#import <sys/time.h>
#import <sys/param.h>
#import <sys/systm.h>
#import <sys/kernel.h>
#import <vm/vm_page.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/event_meter.h>
#import <nextdev/monreg.h>
#import <nextdev/kmreg.h>
#import <nextdev/dma.h>

#define	SFT		8
#define	FIX_MUL(a,b)	(((a) * (b)) >> (SFT * 2))
#define	FIX_250		0xfa00

int	em_y_event[EM_NEVENT] = {
	VIDEO_H + 4, VIDEO_H + 6, VIDEO_H + 8, VIDEO_H + 10,
	VIDEO_H + 16, VIDEO_H + 18, VIDEO_H + 20,
};

int	em_y_vm[EM_NVM] = {
	VIDEO_H + 6, VIDEO_H + 30, VIDEO_H + 54,
};

struct	em_title {
	int	x, y;
	char	title[3];
} em_title[] = {
#define	EM_T0_MISC	0
	8, VIDEO_H + 3, "vm",
	8, EM_Y_EVTOP, "ev",
	8, EM_Y_OD, "od",
	8, EM_Y_SD, "sd",
#define	EM_T1_MISC	3
#define	EM_T0_VM	4
	8, VIDEO_H + 9, "p0",
	8, VIDEO_H + 33, "p1",
	8, VIDEO_H + 57, "p2",
#define	EM_T1_VM	6
};

event_run (key, shift)
{
	register int i, j, t, x;
	register struct dma_dev *vd = (struct dma_dev*) P_VIDEO_CSR;
	int event_timeout(), s_bg, s_fg, s_x, s_y, s_see_msgs;
	struct em_title *tp;

	switch (machine_type) {
	case NeXT_CUBE:
	case NeXT_WARP9:
	case NeXT_X15:
		break;
 	
	case NeXT_WARP9C:
		return;	/* FB is very different from the 2 bit stuff. */
	default:
		return;	/* Paranoia... */
	}

	untimeout (event_timeout, 0);
	em.state += key;
	em.state = MIN (em.state, EM_NSTATE);
	em.state = MAX (em.state, 0);
	if (em.state == EM_OFF) {
		if (em.flags & EMF_LEDS) {
			km_send (MON_KM_USRREQ, KM_SET_LEDS(0, 0));
			em.flags &= ~EMF_LEDS;
		}
		vd->dd_start = EM_VID_NORM;
		return;
	}
	bzero (P_VIDEOMEM + VIDEO_H * VIDEO_NBPL,
		(VIDEO_MH - VIDEO_H) * VIDEO_NBPL);
	vd->dd_start = (char*) EM_VID_UP;
	s_see_msgs = km.flags & KMF_SEE_MSGS;
	km.flags |= KMF_SEE_MSGS;
	s_bg = km.bg;
	s_fg = km.fg;
	s_x = km.x;
	s_y = km.y;
	km.bg = WHITE;
	km.fg = BLACK;

	switch (em.state) {

	case EM_MISC:
		if (key == EM_KEY_UP && shift)
			em.flags |= EMF_LEDS;
		em.event_x = EM_L;
		for (i = 0; i < EM_NEVENT; i++)
			em.last[i] = EM_L;
		for (j = 1, t = 1; j <= 10000; j += t) {
			x = FIX_MUL (event_fixlog10 (j), FIX_250) + EM_L;
			for (i = 0; i <= EM_NEVENT; i++) {
				event_pt (x, em_y_event[i] - 1, BLACK);
				event_pt (x, em_y_event[i] + 1, BLACK);
			}
			if (j / t == 10)
				t *= 10;
		}
		for (i = EM_T0_MISC; i <= EM_T1_MISC; i++) {
			tp = &em_title[i];
			km.x = -tp->x;
			km.y = -tp->y;
			kmpaint (tp->title[0]);
			kmpaint (tp->title[1]);
		}
		event_timeout();
		break;

	case EM_VM:
		for (i = 0; i < EM_NVM; i++) {
			for (j = -2; j < 18; j++)
				event_hline (em_y_vm[i] + j, EM_L - 2,
					EM_R + 2, LT_GRAY);
		}
		for (i = EM_T0_VM; i <= EM_T1_VM; i++) {
			tp = &em_title[i];
			km.x = -tp->x;
			km.y = -tp->y;
			kmpaint (tp->title[0]);
			kmpaint (tp->title[1]);
		}
		break;
	}
	km.bg = s_bg;
	km.fg = s_fg;
	km.x = s_x;
	km.y = s_y;
	km.flags &= ~KMF_SEE_MSGS;
	km.flags |= s_see_msgs;
}

event_disk (which, y, new, max, rw)
{
	int last = em.disk_last[which];

	if (em.state != EM_MISC || max == 0)
		return;
	event_vline (EM_L + 1 + em.disk_rw[which], y + 12, y + 16,
		(BLACK & rw));
	event_vline (EM_L + 2 + em.disk_rw[which], y + 12, y + 16,
		WHITE);
	if (++em.disk_rw[which] >= EM_W)
		em.disk_rw[which] = 0;
	new = new * EM_W / max + 1;
	if (last == new)
		return;
	if (last != 0)
		event_vline (EM_L + last, y + 1, y + 8, DK_GRAY & 0x0ff00000);
	event_vline (EM_L + new, y + 1, y + 8, BLACK);
	em.disk_last[which] = new;
}

event_intr()
{
	event_meter (EM_INTR);
}

event_timeout()
{
	event_graph (EM_VMPF, vm_page_free_count);
	event_graph (EM_VMPA, vm_page_active_count);
	event_graph (EM_VMPI, vm_page_inactive_count);
	event_graph (EM_VMPW, vm_page_wire_count);
	event_graph (EM_FAULT, (vm_stat.faults - em.vm_stat.faults) *
		EM_RATE);
	event_graph (EM_PAGEIN, (vm_stat.pageins - em.vm_stat.pageins) *
		EM_RATE);
	event_graph (EM_PAGEOUT, (vm_stat.pageouts - em.vm_stat.pageouts) *
		EM_RATE);
	em.vm_stat = vm_stat;
	if (em.state == EM_MISC)
		timeout (event_timeout, 0, EM_UPDATE);
}

event_graph (event, val)
{
	register int v, *el, y;

	el = &em.last[event];
	y = em_y_event[event];
	v = FIX_MUL (event_fixlog10 (val), FIX_250) + EM_L;
	if (v > *el) {
		event_hline (y, *el, v, BLACK);
		*el = v;
	} else
	if (v < *el) {
		event_hline (y, v, *el, WHITE);
		*el = v;
	}
}

event_pt (x, y, color)
	register int x, y;
{
	register u_int *i, p, mask;

	i = (u_int*) (P_VIDEOMEM + ((x >> 4) << 2) + y * VIDEO_NBPL);
	mask = 3 << (30 - ((x % 16) << 1));
	p = *i & ~mask;
	*i = p | (color & mask);
}

event_hline (line, start, stop, color)
{
	register u_int *p, b, mask;
	register int np, n;

	p = (u_int*) (P_VIDEOMEM + line * VIDEO_NBPL + ((start >> 4) << 2));
	np = stop - start + 1;
	if (n = start % 16) {
		mask = (1 << (2 * (16 - n))) - 1;
		b = *p & ~mask;
		*p++ = b | (color & mask);
		np -= 16 - n;
	}
	for (; np >= 16; np -= 16)
		*p++ = color;
	if (np > 0) {
		mask = (1 << (2 * (16 - np))) - 1;
		b = *p & mask;
		*p = b | (color & ~mask);
	}
}

event_vline (x, y0, y1, color)
{
	register int i, y, sft;
	register u_int *p;

	p = (u_int*) (P_VIDEOMEM + y0 * VIDEO_NBPL + ((x >> 4) << 2));
	sft = (15 - (x % 16)) * 2;
	for (i = 30; i > 30 - (y1 - y0 + 1) * 2; i -= 2) {
		*p = (*p & ~(3 << sft)) | (((color >> i) & 3) << sft);
		p += (VIDEO_NBPL >> 2);
	}
}

event_fixlog10 (val)
{
	register int v, prod, r, b, p;
	static t[] = {
		0x10000,	/* 2^8 */
		0x1000,		/* 2^4 */
		0x400,		/* 2^2 */
		0x200,		/* 2^1 */
		0x16a,		/* 2^(1/2) */
		0x130,		/* 2^(1/4) */
		0x117,		/* 2^(1/8) */
		0x10b,		/* 2^(1/16) */
		0x105,		/* 2^(1/32) */
		0x102,		/* 2^(1/64) */
		0x101,		/* 2^(1/128) */
	};

#define	LOG2_10	0x352

	v = val << SFT;
	prod = 1 << SFT;
	r = 0;
	for (b = 11; b > 0; b--) {
		p = (prod * t[11-b]) >> SFT;
		if (p < v) {
			prod = p;
			r |= 1 << b;
		}
	}
	return (((r << (2*SFT)) / LOG2_10) >> SFT);
}

#ifdef	notdef
#define	EM_CHIRP_LEN	4096
char em_chirpbuf[EM_CHIRP_LEN + 0x10], *em_cbuf;
int em_chirp_init;

event_chirp()
{
	struct dma_dev *sound = (struct dma_dev*) P_SOUNDOUT_CSR;
	struct monitor *mon = (struct monitor*) P_MON;

	if (!em_chirp_init) {
		em_cbuf = (char*) ((int)em_chirpbuf & ~0xf) + 0x10;
		em_chirp_build (em_cbuf);
		mon_send (MON_GP_OUT, 0x00000000);	/* enable speaker */
		em_chirp_init = 1;
	} else
	if ((sound->dd_csr & DMACSR_COMPLETE) == 0)
		return;
	intr_mask &= ~I_BIT(I_SOUND_OUT_DMA);
	*intrmask &= ~I_BIT(I_SOUND_OUT_DMA);
	intr_mask &= ~I_BIT(I_SOUND_OVRUN);
	*intrmask &= ~I_BIT(I_SOUND_OVRUN);
	mon->mon_csr.dmaout_dmaen = 0;
	mon_send (MON_SNDOUT_CTRL(0x0), 0);	/* disable sound */
	mon->mon_csr.dmaout_ovr = 1;		/* reset overrun error bit */
	sound->dd_csr = DMACMD_RESET | DMACSR_INITBUF;
	sound->dd_next = em_cbuf;
	sound->dd_limit = em_cbuf + EM_CHIRP_LEN;
	sound->dd_csr = DMACMD_START;
	mon->mon_csr.dmaout_dmaen = 1;
	while (mon->mon_csr.dmaout_dmaen == 0)
		;
	mon_send (MON_SNDOUT_CTRL(0x1), 0);	/* enable sound */
}
#endif
#endif	EVENTMETER
