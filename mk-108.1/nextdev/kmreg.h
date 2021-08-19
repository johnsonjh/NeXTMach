/*	@(#)kmreg.h	1.0	10/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 12-Oct-87  John Seamons (jks) at NeXT
 *	Created.
 * 29 Dec-87  Leo Hourvitz (leo) at NeXT
 *	KMREG_H
 *
 **********************************************************************
 */

#ifndef KMREG_H
#define KMREG_H

#import "sys/types.h"
#import "sys/ioctl.h"
#import "sys/tty.h"
#import "mon/nvram.h"


#define	KMIOCEXISTING	_IO('k', 0)		/* use existing window */
#define	KMIOCPOPUP	_IO('k', 1)		/* popup new window */
#define	KMIOCRESTORE	_IO('k', 2)		/* restore background */
#define	KMIOCDUMPLOG	_IO('k', 3)		/* dump message log */
#define	KMIOCGFLAGS	_IOR('k', 4, int)	/* return km.flags value */
#define KMIOCDRAWRECT	_IOW('k', 5, struct km_drawrect)  /* Draw rect from bits */
#define KMIOCERASERECT	_IOW('k', 6, struct km_drawrect)  /* Erase a rect. */
#define KMIOSLEDSTATES	_IOW('k', 7, int)	/* Turn LED's on and off. */

/* Colors for fg, bg in struct km_drawrect */
#define KM_COLOR_WHITE		0
#define KM_COLOR_LTGRAY		1
#define KM_COLOR_DKGRAY		2
#define KM_COLOR_BLACK		3

/*
 * The data to be rendered is treated as a pixmap of 2 bit pixels.
 * The most significant bits of each byte is
 * the leftmost pixel in that byte.  Pixel values are
 * assigned as described above.
 *
 * Each scanline should start on a 4 pixel boundry within the bitmap,
 * and should be a multiple of 4 pixels in length.
 *
 * For the KMIOCERASERECT call, 'data' should be an integer set to the 
 * color to be used for the clear operation (data.fill).
 * A rect at (x,y) measuring 'width' by 'height' will be cleared to 
 * the specified value.
 */
struct km_drawrect {
	unsigned short x;	/* Upper left corner of rect to be imaged. */
	unsigned short y;
	unsigned short width;	/* Width and height of rect to be imaged, in pixels */
	unsigned short height;
	union {
		void *bits;	/* Pointer to 2 bit per pixel raster data. */
		int   fill;	/* Const color for erase operation. */
	} data;
		
};

#define	POPUP_Y		223			/* where popup window goes */

/* popup window styles */
#define	POPUP_NOM	0			/* nominal style */
#define	POPUP_ALERT	1			/* alert style */
#define	POPUP_FULL	2			/* nom, but draw full screen */

/*
 * Keyboard bus commands and responses.
 * 0 <= adrs <= 15; 15 is broadcast; mouse addresses are odd.
 */
#define	KM_RESET		0x0f000000
#define	KM_SET_ADRS(adrs)	(0xef000000 | ((adrs) << 16))
#define	KM_READ_VERS(adrs)	(0xf0000000 | ((adrs) << 24))
#define	KM_POLL(adrs)		(0x10000000 | ((adrs) << 24))
#define	KM_SET_LEDS(adrs,leds)	(0x00000000 | ((adrs) << 24) | ((leds) << 16))
#define	KM_LED_LEFT		1
#define	KM_LED_RIGHT		2

/* format of data returned by keyboard & mouse */
struct keyboard {
	u_int	: 16,
		valid : 1,
		alt_right : 1,
		alt_left : 1,
		command_right : 1,
		command_left : 1,
		shift_right : 1,
		shift_left : 1,
		control : 1,
		up_down : 1,
#define	KM_DOWN	0
#define	KM_UP	1
		key_code : 7;
};

struct mouse {
	u_int	: 16,
		delta_y : 7,
		button_right : 1,
		delta_x : 7,
		button_left : 1;
};

union kybd_event {
	int	data;
	struct	keyboard k;
};

union mouse_event {
	int	data;
	struct	mouse m;
};

struct km {
	union	kybd_event kybd_event;
	union	kybd_event autorepeat_event;
	short	x, y;
#if defined(MONITOR)
	/* No backing store support in ROM monitor. */
	short	nc_tm, nc_lm, nc_w, nc_h;
	int	store;
#else	/* KERNEL */
	short	nc_tm, nc_lm, nc_w, nc_w2, nc_h, nc_h2;
	int	store, store2;
#endif
	int	fg, bg;
	short	ansi;
	short	*cp;
#define	KM_NP		3
	short	p[KM_NP];
	volatile short flags;
#define	KMF_INIT	0x0001
#define	KMF_AUTOREPEAT	0x0002
#define	KMF_STOP	0x0004
#define	KMF_SEE_MSGS	0x0008
#define	KMF_PERMANENT	0x0010
#define	KMF_ALERT	0x0020
#define	KMF_HW_INIT	0x0040
#define	KMF_MON_INIT	0x0080

#if defined(MONITOR)
#define	KMF_CURSOR	0x0100
#define	KMF_NMI		0x0200
#else	/* KERNEL */
/* Assorted stuff for kernel console support */
#define	KMF_ALERT_KEY	0x0100
#define	KMF_CURSOR	0x0200
#define	KMF_ANIM_RUN	0x0400
	short	save;
#define	KM_NSAVE	8
	short	save_flags[KM_NSAVE];
	struct	tty *save_tty[KM_NSAVE];
	short	showcount;
#endif	/* KERNEL */
};

/*
 * Data layout for console device.
 */
struct km_console_info
{
	int	pixels_per_word;	/* Pixels per 32 bit word: 16, 4, 2, or 1 */
	int	bytes_per_scanline;
	int	dspy_w;			/* Visible display width in pixels */
	int	dspy_max_w;		/* Display width in pixels */
	int	dspy_h;			/* Visible display height in pixels */
#define KM_CON_ON_NEXTBUS	1	/* flag_bits: Console is NextBus device */
	int	flag_bits;		/* Vendor and NeXT flags */
	int	color[4];		/* Bit pattern for white thru black */
#define KM_HIGH_SLOT	6		/* highest possible console slot. */
	char	slot_num;		/* Slot of console device */
	char	fb_num;			/* Logical frame buffer in slot for console */
	char	byte_lane_id;		/* A value of 1, 4, or 8 */
	int	start_access_pfunc;	/* P-code run before each FB access */
	int	end_access_pfunc;	/* P-code run after each FB access */
	struct	{		/* Frame buffer related addresses to be mapped */
			int	phys_addr;
			int	virt_addr;
			int	size;
#define KM_CON_MAP_ENTRIES	6
#define KM_CON_PCODE		0
#define KM_CON_FRAMEBUFFER	1
#define KM_CON_BACKINGSTORE	2
		} map_addr[KM_CON_MAP_ENTRIES];
	int	access_stack;
};

#if	KERNEL
struct km km;
extern char *mach_title;
int alert_key;
/*
 * The following struct describes the memory layout of the console device, for I/O 
 * purposes.  This structure is by default initialized to support the lowest common 
 * denominator, the 2 bit display.
 */
struct km_console_info km_coni;

/* Convenient defs into this structure. */
#define KM_NPPW		km_coni.pixels_per_word
#define KM_WHITE	km_coni.color[0]
#define KM_LT_GRAY	km_coni.color[1]
#define KM_DK_GRAY	km_coni.color[2]
#define KM_BLACK	km_coni.color[3]
#define KM_VIDEO_W	km_coni.dspy_w
#define KM_VIDEO_MW	km_coni.dspy_max_w
#define KM_VIDEO_H	km_coni.dspy_h
#define KM_P_VIDEOMEM	km_coni.map_addr[KM_CON_FRAMEBUFFER].virt_addr
#define KM_VIDEO_NBPL	km_coni.bytes_per_scanline
#define KM_BACKING_STORE	km_coni.map_addr[KM_CON_BACKINGSTORE].virt_addr
#define KM_BACKING_SIZE	km_coni.map_addr[KM_CON_BACKINGSTORE].size
#define KM_PCODE_TABLE	km_coni.map_addr[KM_CON_PCODE].virt_addr

#endif	KERNEL
#if defined(MONITOR)
/* Convenient defs into this structure, for the ROM monitor. */
#define KM_NPPW		mg->km_coni.pixels_per_word
#define KM_WHITE	mg->km_coni.color[0]
#define KM_LT_GRAY	mg->km_coni.color[1]
#define KM_DK_GRAY	mg->km_coni.color[2]
#define KM_BLACK	mg->km_coni.color[3]
#define KM_VIDEO_W	mg->km_coni.dspy_w
#define KM_VIDEO_MW	mg->km_coni.dspy_max_w
#define KM_VIDEO_H	mg->km_coni.dspy_h
#define KM_VIDEO_NBPL	mg->km_coni.bytes_per_scanline
#define KM_P_VIDEOMEM	mg->km_coni.map_addr[KM_CON_FRAMEBUFFER].virt_addr
#define KM_BACKING_STORE	mg->km_coni.map_addr[KM_CON_BACKINGSTORE].virt_addr
#define KM_BACKING_SIZE	mg->km_coni.map_addr[KM_CON_BACKINGSTORE].size
#define KM_PCODE_TABLE	mg->km_coni.map_addr[KM_CON_PCODE].virt_addr

#define KM_DEFAULT_STORE	(P_MAINMEM + 0x100000)
#define KM_DEFAULT_STORE_SIZE	0x100000

#endif	/* MONITOR */

#endif KMREG_H




