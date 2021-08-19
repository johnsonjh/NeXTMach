/*
 *	Copyright (C) 1989, NeXT, Inc.
 *
 * HISTORY
 *
 * 01-Mar-90  Ted Cohn (tcohn) at NeXT
 *	Added DKIOCREGISTER ioctl to register the MegaPixel with ev driver.
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Added STOP_ANIM flag.
 *
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Created.
 */

#ifndef	_VIDEO_
#define	_VIDEO_

/* 2 bit display parameters */
#define PIXPERXFER	16		/* pixel alignment of a transfer */
#define PPXMASK		((unsigned int)0x0000000f)
#define LONGSIZE	32		/* pixel width of saved rect */

#define	NPPB		4		/* Pixels per byte */
#define	VIDEO_W		1120		/* Visible pixels per scanline */
#define	VIDEO_MW	1152		/* Actual pixels per scanline */
#define	VIDEO_H		832		/* Visible scanlines */
#define	VIDEO_MH	910
#define	VIDEO_NBPL	(VIDEO_MW >> 2)
#define	WHITE		0x00000000
#define	LT_GRAY		0x55555555
#define	DK_GRAY		0xaaaaaaaa
#define	BLACK		0xffffffff
#define	RETRACE_START	0x00
#define	RETRACE_LIMIT	0xea		/* limit reg value to int @ retrace */
#define	RETRACE_MAX	0xfc
#define	MWF_SIZE	USRSTACK
#define	CONTINUE_ANIM	0x12345678	/* continue animation (if any) */
#define	STOP_ANIM	0x87654321
#define VID_SIZE	0x08000000	/* size of video memory area to be mapped */

/* 16 bit color display parameters */
#define C16_VIDEO_W		1120	/* 1120 x 832 mode */
#define C16_VIDEO_MW		1152
#define C16_VIDEO_H		832
#define C16_VIDEO_MH		910
#define C16_VIDEO_NBPL		(C16_VIDEO_MW * 2)	/* 2 bytes/pixel */
#define C16_VID_SIZE		0x00200000	 /* 2 Mbytes of VRAM */

#define C16_NPPW		2	/* 2 pixels per word. */
#define	C16_WHITE		0xFFF0FFF0
#define	C16_LT_GRAY		0xAAA0AAA0
#define	C16_DK_GRAY		0x55505550
#define	C16_BLACK		0

/* Bits for the write-only P_C16_CMD_REG register. */
#define C16_CMD_CLRINTR		1
#define C16_CMD_INTRENA		2
#define C16_CMD_UNBLANK		4

#ifndef	ASSEMBLER
struct mwf {
    int min;		/* range of va's that need.. */
    int max;		/* ..to also map mem write funcs */
};

struct alarm {
	int	alarm_enabled;
	int	alarm_time;
};
#endif	ASSEMBLER

#define	DKIOCGADDR	_IOWR('v', 0, int)	/* get address of video mem */
#define	DKIOCSMWF	_IOWR('v', 1, struct mwf) /* set mwf bounds */
#define	DKIOCBRIGHT	_IOWR('v', 2, int)	/* get/set brightness */
#define	DKIOCDISABLE	_IO('v', 3)		/* disable translation */
#define	DKIOCGNVRAM	_IOR('v', 4, struct nvram_info) /* get NVRAM */
#define	DKIOCSNVRAM	_IOW('v', 4, struct nvram_info) /* set NVRAM */
#define	DKIOCREGISTER	_IOW('v', 5, evioScreen) /* register screen */
#define	DKIOCGFBINFO	_IO('v', 6)		/* Get info on built-in frame buffer */
#define	DKIOCGALARM	_IOR('v', 7, struct alarm)	/* get RTC alarm */
#define	DKIOCSALARM	_IOW('v', 7, struct alarm)	/* set RTC alarm */

#endif	_VIDEO_


