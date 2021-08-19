/*	@(#)zs.c	1.0	08/12/87	(c) 1987 NeXT	*/
/*
 * HISTORY
 * 14-May-90  Gregg Kellogg (gk) at NeXT
 *	Changed to use us_timeout instead of timeout, as these can be
 *	called at levels > spl3().
 *
 */
#import <zs.h>

#if NZS > 0
#import <gdb.h>

/*
 *  Zilog 8530 driver
 *  Author: Mike DeMoney, mike@next.com
 *  Copyright 1987, 1990.  NeXT Inc.  All rights reserved.
 */
#import <sys/bkmac.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/buf.h>
#import <sys/vm.h>
#import <sys/conf.h>
#import <sys/bkmac.h>
#import <sys/file.h>
#import <sys/uio.h>
#import <sys/kernel.h>
#import <sys/machine.h>
#import <sys/syslog.h>
#import <next/psl.h>
#import	<next/scr.h>
#import <next/cons.h>
#import <next/us_timer.h>
#import <nextdev/dma.h>
#import <nextdev/zsreg.h>
#import <nextdev/zscom.h>
#import <nextdev/busvar.h>
#import <machine/spl.h>
#import <kern/xpr.h>
#import <next/spl.h>
#import <next/cpu.h>
#import <sys/callout.h>
#import <sys/time.h>

/*
 * External procedures referenced
 */
extern int ttrstrt(struct tty *tp);

/*
 * Options and tunable parameters
 */
/* Optional features */
#define	NONE_PARITY	1	/* EVEN&ODDP==0 => xmit 0 parity not even */
/* #define SCC_DMA	1	/* hw is broken, don't turn this on */

/* Default line state */
#define	ISPEED	B9600
#define	IFLAGS	(EVENP|ODDP|ECHO|CRMOD)

/* receiver buffering and flow control */
#define	NZSCHAR		128		/* receive buffer size */
#define	ZSLIMIT		(NZSCHAR/2)	/* turn RTS off */
#define	ZSHIWAT		(NZSCHAR/4)	/* trigger zssoftrint */
#define	ZSLOWAT		(NZSCHAR/8)	/* turn RTS on */

/* timer parameters */
#define	ZS_TIMERPERIOD	(hz)		/* ticks between timer invocations */
/* these timers are in seconds */
#define	ZS_XINTTIME	3		/* lost xmit interrupt timer  */
#define	ZS_SXINTTIME	3		/* lost sw xmit interrupt timer */
#define	ZS_SRINTTIME	3		/* lost sw rcv interrupt timer */
#define	ZS_OVRTIME	10		/* time between overrun reports */
#define	ZS_RXOVERTIME	10
#define	ZS_DTRDOWNTIME	2		/* DTR down time */

/* Internal macros */
#define	XDBG(x)		XPR(XPR_SCC, x)
#ifdef notdef
#define	XXDBG(x)	XPR(XPR_SCC, x)
#else
#define	XXDBG(x)
#endif
#define	ZS_TIME(x)	((x)*(hz/ZS_TIMERPERIOD))

/*
 * zs_buf is the circular buffer where characters recv'd at splscc
 * are put until the software interrupt handler can ttyrint()
 * them
 */
struct zs_buf {
	struct zb_char {
		/*
		 * zc_type is
		 * 	rr1 | ZCTYPE_CHAR for chars
		 *	ZCTYPE_STATUS for status bytes
		 * zc_c is
		 *	data if ZCTYPE_CHAR
		 *	rr1 if ZCTYPE_STATUS
		 */
		char zc_type;
		char zc_c;	/* character received */
	} zb_char[NZSCHAR];
	/*
	 * zb_in == zb_out => empty buffer
	 */
	struct zb_char *zb_in;	/* put chars here */
	struct zb_char *zb_out;	/* get chars here */
};

#define	ZCTYPE_MASK	0x01	/* 1 because EMPTY isn't really a type */

#define	ZCTYPE_STATUS	0x00	/* entry represents status change */
#define	ZCTYPE_CHAR	0x01	/* entry represents an char */
#define	ZCTYPE_EMPTY	0x02	/* queue is empty */

/*
 * Driver info -- a place to hide all kinds of crufty info
 */
struct zs_softc {
	struct	zs_buf zs_buf;	/* circular buffer for recv'd chars */
	volatile struct	zsdevice *zs_addr; /* csr address */
	struct	dma_chan zs_dc;	/* dma channel information */
	struct	dma_hdr	zs_dh;	/* dma buffer header */
	/*
	 * zs_srintdelay is how long input chars languish in the circular
	 * buf before being processed by ttyinput(); the idea is to attempt
	 * to batch a few characters...
	 */
	struct	timeval zs_srintdelay;
	int	zs_charmask;	/* mask to clear parity/fill bits */
	int	zs_flags;	/* flags, never modified at interrupt time */
	int	zs_iflags;	/* flags, modified scc ipl */
	int	zs_xinttime;	/* xmit'er interrupt timer */
	int	zs_sxinttime;	/* xmit'er SW interrupt timer */
	int	zs_srinttime;	/* recv'r SW interrupt timer */
	int	zs_ovrtime;	/* ignoring buffer overrun timer */
	int	zs_rxovertime;	/* ignoring recv'r overrun timer */
	int	zs_tcval;	/* software copy of baud rate time constant */
	int	zs_modem;	/* modem status, modified at scc ipl */
	int	zs_speed;	/* current baud rate */
	int	zs_nwaiting;	/* count of waiting incoming opens */
	char	zs_wr3;		/* software copy of WR3 */
	char	zs_wr4;		/* software copy of WR4 */
	char	zs_wr5;		/* software copy of WR5 */
};

/*
 * Mode flags, never changed at interrupt level
 */
#define	ZSFLAG_DMA		0x0001	/* channel has DMA support */
#define	ZSFLAG_SOFTCAR		0x0002	/* Device open with sw force of CD */
#define	ZSFLAG_HARDCAR		0x0004	/* Device open with hw CD in effect */
#define	ZSFLAG_BUSYIN		0x0008	/* ttyin device in use */
#define	ZSFLAG_BUSYOUT		0x0010	/* ttyout device in use */
#define	ZSFLAG_WAITIN		0x0020	/* incoming connection block for CD */
#define	ZSFLAG_FLOWCTL		0x0040	/* Device open with hw flow control */
#define	ZSFLAG_DELAYSET		0x0080	/* user has specified srint delay */
#define	ZSFLAG_INITIALIZED	0x0100	/* SCC has been initialized */

/*
 * Status flags, modified at interrupt level.
 *
 * All modifications of these flags must be done at splscc.
 */
#define	ZSIFLAG_OVR		0x0001	/* circular buffer overrun occurred */
#define	ZSIFLAG_RXOVER		0x0002	/* recv'r overrun occurred */
#define	ZSIFLAG_SINTPEND	0x0004	/* softint pending for recvr */

/*
 * Since the modem controls and how you deal with them change from
 * rev to rev, the driver internally uses these definitions as a constant.
 * Values are gotten and set by calling one routine that figures out
 * how to map from the hw by looking at the zstype.
 *
 * All modifications of these flags must be done at splscc.
 */
#define	ZSMODEM_RTS		0x01	/* RTS modem bit -- output */
#define	ZSMODEM_BRK		0x02	/* Transmit a break -- output */
#define	ZSMODEM_DTR		0x04	/* DTR modem bit -- output */
#define	ZSMODEM_CTS		0x08	/* CTS modem bit -- input */
#define	ZSMODEM_DCD		0x10	/* DCD modem bit -- input */
#define	ZSMODEM_RCVBRK		0x20	/* Receiver in middle of break */ 

#define ZSMODEM_OUTPUTS		(ZSMODEM_RTS|ZSMODEM_BRK|ZSMODEM_DTR)
#define	ZSMODEM_INPUTS		(ZSMODEM_CTS|ZSMODEM_DCD|ZSMODEM_RCVBRK)

#define	ZSMODEM_ON		(ZSMODEM_DTR|ZSMODEM_RTS)
#define	ZSMODEM_OFF		(0)

/*
 * Public data declarations
 */
int zschars[NZS];		/* statistics */

/*
 * Public procedure declarations
 */
int zsopen(dev_t dev, int flag);
int zsclose(dev_t dev, int flag);
int zsread(dev_t dev, struct uio *uio);
int zswrite(dev_t dev, struct uio *uio);
int zsselect(dev_t dev, int rw);
int zsioctl(dev_t dev, int cmd, caddr_t data, int flag);
int zsstop(struct tty *tp, int flag);

/*
 * Private procedure declarations
 */
static inline int zsbufget(struct zs_buf *zbp, struct zb_char *zcp);
static inline int zschartype(struct zs_buf *zbp);
static inline int zsbufput(struct zs_buf *zbp, struct zb_char *zcp);
static inline int zscharcnt(struct zs_buf *zbp);

static int zsgettype(void);
static void zsinit(int unit);
static void zsparam(int unit, int flush);
static int zssrintdelay(struct tty *tp);
static void zsrint(int unit);
static void zssoftrint(int unit);
static void zsstart(struct tty* tp);
static void zsxint(int unit);
static void zssoftxint(int unit);
static void zssint(int unit);
static int zsmaprr0(int hw_bits);
static int zsmapwr5(int zs_bits);
static int zsmctl(int unit, int bits, int how);
static void zswr5set(int unit);
static void zstimer(void);
static int dmtozs(int bits);
static int zstodm(int bits);
static void zschan_init(volatile struct zsdevice *zsaddr, int speed);

/*
 * Private data declarations
 */
static int zstype;			/* board rev */

#define	ZSTYPE_DCD_IS_NCTS	0	/* CTS is ! DCD, RTS is enable */
#define	ZSTYPE_DCD_IS_CTS	1	/* CTS is DCD, RTS is enable */
#define	ZSTYPE_FLOWCTL		2	/* DCD is DCD, RTS/CTS is flow control */

static struct tty zs_tty[NZS];
static int zs_init[NZS];
static struct zs_softc zs_softc[NZS];

static int zs_speeds[16] = {
/* B0    */	0,
/* B50   */	50,
/* B75   */	75,
/* B110  */	110,
/* B134  */	134,
/* B150  */	150,
/* B200  */	200,
/* B300  */	300,
/* B600  */	600,
/* B1200 */	1200,
/* B1800 */	1800,
/* B2400 */	2400,
/* B4800 */	4800,
/* B9600 */	9600,
/* EXTA  */	19200,
/* EXTB  */	38400
};
 
/*
 * switch to vector interrupts via zscom.c
 */
static struct zs_intrsw zi_tty = {
	(int (*)())zsxint, (int (*)())zsrint, (int (*)())zssint };

/*
 * Private inline procedures
 */

/*
 * zsbufget -- get entry from circular buffer
 */
static inline int
zsbufget(struct zs_buf *zbp, struct zb_char *zcp)
{
	struct zb_char *newzcp;

	if (zbp->zb_in == zbp->zb_out)
		return(0);	/* empty */

	*zcp = *zbp->zb_out;

	newzcp = zbp->zb_out + 1;
	if (newzcp >= &zbp->zb_char[NZSCHAR])
		newzcp = zbp->zb_char;
	zbp->zb_out = newzcp;
	return(1);
}

/*
 * zschartype -- get type of next character in circular buffer
 */
static inline int
zschartype(struct zs_buf *zbp)
{
	if (zbp->zb_in == zbp->zb_out)
		return(ZCTYPE_EMPTY);	/* empty */

	return(zbp->zb_out->zc_type & ZCTYPE_MASK);
}

static inline int
zsbufput(struct zs_buf *zbp, struct zb_char *zcp)
{
	struct zb_char *newzcp;

	newzcp = zbp->zb_in + 1;
	if (newzcp >= &zbp->zb_char[NZSCHAR])
		newzcp = zbp->zb_char;
	if (newzcp == zbp->zb_out)
		return(1);	/* overflow, drop the character */

	*zbp->zb_in = *zcp;
	zbp->zb_in = newzcp;
	return(0);
}

/*
 * zscharcnt -- get count of characters in circular buffer
 * MUST BE CALLED AT splSCC
 */
static inline int
zscharcnt(struct zs_buf *zbp)
{
	ASSERT(curipl() == IPLSCC);
	return (zbp->zb_in >= zbp->zb_out
		? zbp->zb_in - zbp->zb_out
		: NZSCHAR - (zbp->zb_out - zbp->zb_in));
}

int
zsopen(dev_t dev, int flag)
{
	struct zs_softc *zsp;
	struct tty *tp;
	int unit, s, error, m;
	static int zs_timer_running;

	if (!zs_timer_running) {
		zs_timer_running++;
		zstype = zsgettype();
		XDBG(("zsopen: type is %d\n", zstype));
		zstimer();
	}
 
	m = minor(dev);
	unit = ZSUNIT(m);
	XDBG(("zsopen: unit %d, flag 0x%x\n", unit, flag));
	if (unit >= NZS)
		return (ENXIO);

	tp = &zs_tty[unit];
	zsp = &zs_softc[unit];
	zsp->zs_addr = (unit == 0) ? ZSADDR_A : ZSADDR_B;

	if (((tp->t_state&TS_XCLUDE) && u.u_uid != 0))
		return(EBUSY);
	if (ZSOUTWARD(m) && (zsp->zs_flags & ZSFLAG_BUSYIN))
		return(EBUSY);
	if ((ZSHARDCAR(m) && (zsp->zs_flags & ZSFLAG_SOFTCAR))
	  || (!ZSHARDCAR(m) && (zsp->zs_flags & ZSFLAG_HARDCAR)))
		return(EBUSY);
	if (error = zsacquire(unit, ZCUSER_TTY, &zi_tty))
		return(error);

	/*
	 * if this is an incoming line (getty) and the line is
	 * inuse with an outgoing connection, sleep until it's free
	 */
	if (!ZSOUTWARD(m))
		zsp->zs_nwaiting++;
checkbusy:
	if (ZSOUTWARD(m))
		zsp->zs_flags |= ZSFLAG_BUSYOUT;
	else {
		zsp->zs_flags |= ZSFLAG_WAITIN;
		if (zsp->zs_flags & ZSFLAG_BUSYOUT) {
			XDBG(("zsopen: sleeping for busyout"));
			if (sleep((caddr_t)&tp->t_rawq, TTIPRI | PCATCH)) {
			    XDBG(("zsopen: busyout sleep interrupted"));
			    if (--zsp->zs_nwaiting == 0) {
			    	zsp->zs_flags &= ~ZSFLAG_WAITIN;
			    	if ((zsp->zs_flags
			    	 & (ZSFLAG_BUSYOUT|ZSFLAG_BUSYIN)) == 0)
				    zsrelease(unit);
			    }
			    return EINTR;
			}
			goto checkbusy;
		}
	}

	zsp->zs_flags |= ZSHARDCAR(m) ? ZSFLAG_HARDCAR : ZSFLAG_SOFTCAR;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_dev = dev;
		tp->t_oproc = (int (*)())zsstart;
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_ispeed = ISPEED;
			tp->t_ospeed = ISPEED;
			tp->t_flags = IFLAGS;
		}
		if (ZSOUTWARD(m))
			tp->t_flags |= NOHANG;
		else
			tp->t_flags &= ~NOHANG;
		if (ZSFLOWCTL(m) && zstype == ZSTYPE_FLOWCTL)
			zsp->zs_flags |= ZSFLAG_FLOWCTL;
		if ((zsp->zs_flags & ZSFLAG_INITIALIZED) == 0) {
			zsinit(unit);
			zsp->zs_flags |= ZSFLAG_INITIALIZED;
		}
		zsparam(unit, 0);
		XDBG(("zsopen: turning on dtr\n"));
		(void) zsmctl(unit, ZSMODEM_ON, DMSET);
		XDBG(("zsopen: zs_modem = 0x%x\n", zsp->zs_modem));
	}


	s = spltty();
	if (ZSOUTWARD(m) || !ZSHARDCAR(m) || (zsp->zs_modem & ZSMODEM_DCD))
		tp->t_state |= TS_CARR_ON;
	else if (((flag & FNDELAY) == 0 && (tp->t_state & TS_CARR_ON) == 0)) {
		XDBG(("zsopen: sleeping waiting for carrier\n"));
		tp->t_state |= TS_WOPEN;
		if (sleep((caddr_t)&tp->t_rawq, TTIPRI | PCATCH)) {
		    splx(s);
		    XDBG(("zsopen: carrier sleep interrupted"));
		    if (--zsp->zs_nwaiting == 0) {
			zsp->zs_flags &= ~ZSFLAG_WAITIN;
			if ((zsp->zs_flags
			 & (ZSFLAG_BUSYOUT|ZSFLAG_BUSYIN)) == 0)
			    zsclose(dev, 0);
		    }
		    return EINTR;
		}
		/*
		 * Only incoming connections sleep here.  A wake-up occurs here
		 * whenever carrier transitions or the line is closed.
		 *
		 * In both cases, an outgoing line could have gotten the
		 * line first; or gotten it, released it, and botched our
		 * mode set-up in the process.  So on return from the sleep
		 * must check for BUSYOUT and reset modes.
		 */
		splx(s);
		goto checkbusy;
	}
	splx(s);

	if (!ZSOUTWARD(m)) {
		zsp->zs_flags |= ZSFLAG_BUSYIN;
		zsp->zs_flags &= ~ZSFLAG_WAITIN;
		zsp->zs_nwaiting--;
	}

	XDBG(("zsopen: done\n"));
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}
 
int 
zsclose(dev_t dev, int flag)
{
	volatile struct zsdevice *zsaddr;
	struct zs_softc *zsp;
	struct tty *tp;
	int unit, m, s;

	XDBG(("zsclose state=0x%x\n", tp->t_state));
	m = minor(dev);
	unit = ZSUNIT(m);

	zsp = &zs_softc[unit];
	zsaddr = zsp->zs_addr;
	tp = &zs_tty[unit];

	(*linesw[tp->t_line].l_close)(tp);
	(void) zsmctl(unit, ZSMODEM_BRK, DMBIC);
	if ((tp->t_state&(TS_HUPCLS|TS_WOPEN))
	    || (tp->t_state&TS_ISOPEN) == 0) {
		XDBG(("zsclose DTR off\n"));
		(void) zsmctl(unit, ZSMODEM_OFF, DMSET);
	}
	ttyclose(tp);

	s = splscc();
	ZSWRITE(zsaddr, 1, 0);	/* clear interrupt enables */
	zsp->zs_xinttime = 0;
	zsp->zs_sxinttime = 0;
	zsp->zs_srinttime = 0;
	zsp->zs_ovrtime = 0;
	zsp->zs_rxovertime = 0;
	zsp->zs_iflags = 0;
	splx (s);

	/* sleep for 2 seconds to make sure the modem see DTR down */
	us_delay(ZS_DTRDOWNTIME * 1000000);

	/* Don't release zs if incoming side is waiting for carrier */
	zsp->zs_flags &= ZSOUTWARD(m) ? ZSFLAG_WAITIN : 0;
	if ((zsp->zs_flags & ZSFLAG_WAITIN) == 0) {
		XDBG(("zsclose releasing zscom\n"));
		zsrelease(unit);
	}
	
	/* wakeup any sleeping incoming opens */
	wakeup((caddr_t)&tp->t_rawq);
}
 
int
zsread(dev_t dev, struct uio *uio)
{
	struct tty *tp;
	struct zs_softc *zsp;
	int unit, error, s;
 
 	unit = ZSUNIT(minor(dev));
	tp = &zs_tty[unit];
	zsp = &zs_softc[unit];

	error = (*linesw[tp->t_line].l_read)(tp, uio);

	/*
	 * Read consumed some input; if we had hw flow control turned off,
	 * call sw rcv interrupt routine to check if flow should be resumed.
	 */
	s = spltty();
	if ((zsp->zs_modem & ZSMODEM_RTS) == 0
	 && (tp->t_state & TS_INPUTFULL) == 0) {
	 	XDBG(("zsread: call zssoftrint to turn RTS on\n"));
		zssoftrint(unit);
	}
	splx(s);
	return error;
}

int
zswrite(dev_t dev, struct uio *uio)
{
	struct tty *tp;

	tp = &zs_tty[ZSUNIT(minor(dev))];
	return ((*linesw[tp->t_line].l_write)(tp, uio));
}

int
zsioctl(dev_t dev, int cmd, caddr_t data, int flag)
{
	struct tty *tp;
	struct zs_softc *zsp;
	int unit, error, s, features;

	unit = ZSUNIT(minor(dev));
	tp = &zs_tty[unit];
	zsp = &zs_softc[unit];

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag);
	if (error >= 0)
		goto checkflowctl;
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0) {
		if (cmd == TIOCSETP || cmd == TIOCSETN || cmd == TIOCLBIS ||
		    cmd == TIOCLBIC || cmd == TIOCLSET)
			zsparam(unit, cmd == TIOCSETP);
		goto checkflowctl;
	}
	error = 0;
	switch (cmd) {

	case TIOCSBRK:
		(void) zsmctl(unit, ZSMODEM_BRK, DMBIS);
		break;

	case TIOCCBRK:
		(void) zsmctl(unit, ZSMODEM_BRK, DMBIC);
		break;

	case TIOCSDTR:
		(void) zsmctl(unit, ZSMODEM_DTR, DMBIS);
		break;

	case TIOCCDTR:
		(void) zsmctl(unit, ZSMODEM_DTR, DMBIC);
		break;

	case TIOCMSET:
		(void) zsmctl(unit, dmtozs(*(int *)data), DMSET);
		break;

	case TIOCMBIS:
		(void) zsmctl(unit, dmtozs(*(int *)data), DMBIS);
		break;

	case TIOCMBIC:
		(void) zsmctl(unit, dmtozs(*(int *)data), DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = zstodm(zsmctl(unit, 0, DMGET));
		break;

	case ZIOCTSET:
		zsp->zs_srintdelay.tv_usec = *(int *)data;
		zsp->zs_flags |= ZSFLAG_DELAYSET;
		break;

	case ZIOCTGET:
		*(int *)data = zsp->zs_srintdelay.tv_usec;
		break;

	case ZIOCFGET:
		features = 0;
		switch (zstype) {
		case ZSTYPE_FLOWCTL:
			features |= ZSFEATURE_HWFLOWCTRL;
			break;
		}
		*(int *)data = features;
		break;

	default:
		error = ENOTTY;
		break;
	}
checkflowctl:
	s = spltty();
	if ((zsp->zs_modem & ZSMODEM_RTS) == 0
	 && (tp->t_state & TS_INPUTFULL) == 0) {
	 	XDBG(("zsioctl: calling zssoftrint to turn RTS on\n"));
		zssoftrint(unit);
	}
	splx(s);
	return (0);
}

int
zsstop(struct tty *tp, int flag)
{
	struct zs_softc *zsp;
	int s;

	zsp = &zs_softc[ZSUNIT(minor(tp->t_dev))];
	/*
	 * This assumes that splscc is higher than
	 * spltty
	 */
	s = splscc();
	XDBG(("zsstop enter\n"));
	if (tp->t_state & TS_BUSY) {
		if (zsp->zs_flags & ZSFLAG_DMA)
			dma_abort(&zsp->zs_dc);
		else {
			/*
			 * zsxint detects that next == stop and posts
			 * a software interrupt for zssoftxint
			 */
			zsp->zs_dc.dc_next = zsp->zs_dh.dh_stop;
		}
		if ((tp->t_state&TS_TTSTOP)==0)
			tp->t_state |= TS_FLUSH;
	}
	XDBG(("zsstop exit\n"));
	splx(s);
}

int
zsselect(dev_t dev, int rw)
{
	struct tty *tp;

	tp = &zs_tty[ZSUNIT(minor(dev))];
	return (*linesw[tp->t_line].l_select)(tp, rw);
}

/*
 * Initialization routines
 */
static int
zsgettype(void)
{
	int zstype;
	
	XDBG(("machine type 0x%x board rev 0x%x\n", machine_type, board_rev));
	switch (machine_type) {
	case NeXT_CUBE:
		switch (board_rev) {
		case 0:
			zstype = ZSTYPE_DCD_IS_NCTS;
			break;
		case 1:
		case 2:
			zstype = ZSTYPE_DCD_IS_CTS;
			break;
		case 3:
		default:
			zstype = ZSTYPE_FLOWCTL;
			break;
		}
		break;
	default:
		switch (board_rev) {
		case 0:
		default:
			zstype = ZSTYPE_FLOWCTL;
			break;
		}
		break;
	}
	return zstype;
}

static void
zsinit(int unit)
{
	struct tty *tp;
	volatile struct zsdevice *zsaddr;
	struct zs_softc *zsp;
	int reset, wr1, rw15, s, ctrl;
	int zsdmaxint();
 
	XDBG(("zsinit\n"));

	tp = &zs_tty[unit];
	zsp = &zs_softc[unit];
	zsaddr = zsp->zs_addr;

	/*
	 * set tcval to nonsense, so zsparam is forced to set SCC
	 */
	zsp->zs_tcval = -1;
	zsp->zs_speed = zs_speeds[ISPEED];

	/*
	 * Initialize dma_chan struct
	 */
#if	SCC_DMA
	if (unit == 0) {
		zsp->zs_dc.dc_queue.dq_head = NULL;
		zsp->zs_dc.dc_current = NULL;
		zsp->zs_dc.dc_handler = zssoftxint;
		zsp->zs_dc.dc_hndlrarg = unit;
		zsp->zs_dc.dc_hndlrpri = CALLOUT_PRI_SOFTINT0;
		zsp->zs_dc.dc_dev = (struct dma_dev *) P_SCC_CSR;
		zsp->zs_dc.dc_flags = DMACHAN_INTR;
		zsp->zs_dh.dh_link = NULL;
		zsp->zs_flags |= ZSFLAG_DMA;
		dma_init(&zsp->zs_dc, I_SCC_DMA);
		/*
		 * FIXME: Is there any problem with turning this stuff
		 * on here?
		 */
		wr1 = WR1_EXTIE|WR1_RXALLIE|WR1_REQFUNC|WR1_REQENABLE;
	} else {
#endif	SCC_DMA
		wr1 = WR1_EXTIE|WR1_RXALLIE|WR1_TXIE;
		zsp->zs_flags &= ~ZSFLAG_DMA;
#if	SCC_DMA
	}
#endif	SCC_DMA

	/*
	 * reset channel, errors, and status
	 * setup wr1 (interrupt enables & dma), wr9 (no vector),
	 * wr11 (clock source), wr14 (br gen enable & dma),
	 * and wr15 (interrupt enables), wr0 (error and status reset),
	 * wr9 (Master Interrupt Enable)
	 */
	XDBG(("zsinit: RESETTING CHANNEL!\n"));
	*(u_char*) P_SCC_CLK = PCLK_3684_MHZ | SCLKB_4_MHZ | SCLKA_4_MHZ;
	reset = (unit == 0) ? WR9_RESETA : WR9_RESETB;
	s = splscc();
	ZSWRITE(zsaddr, 9, reset | WR9_NV);
	DELAY(10);	/* FIXME, is this necessary?  see 4.1.10 in SCC man */
	ZSWRITE(zsaddr, 1, wr1);
	ZSWRITE(zsaddr, 9, WR9_NV | WR9_MIE);
	ZSWRITE(zsaddr, 10, WR10_NRZ);
	ZSWRITE(zsaddr, 11, WR11_TXCLKBRGEN|WR11_RXCLKBRGEN);
	rw15 = RW15_BREAKIE;
	switch (zstype) {
	case ZSTYPE_DCD_IS_NCTS:
	case ZSTYPE_DCD_IS_CTS:
		if (zsp->zs_flags & ZSFLAG_HARDCAR)
			rw15 |= RW15_CTSIE;
		break;
	default:
	case ZSTYPE_FLOWCTL:
		if (zsp->zs_flags & ZSFLAG_FLOWCTL)
			rw15 |= RW15_CTSIE;
		if (zsp->zs_flags & ZSFLAG_HARDCAR)
			rw15 |= RW15_DCDIE;
		break;
	}
	ZSWRITE(zsaddr, 15, rw15);
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESET;
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESETTXPEND;
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESET_STAT;
	DELAY(1);
	ctrl = zsaddr->zs_ctrl;
	zsp->zs_modem = zsmaprr0(ctrl) | (zsp->zs_modem & ZSMODEM_OUTPUTS);
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESET_STAT;
	zsp->zs_buf.zb_in = zsp->zs_buf.zb_out = zsp->zs_buf.zb_char;
	XDBG(("zsinit done\n"));
	splx (s);
}
 
static void
zsparam(int unit, int flush)
{
	struct tty *tp;
	volatile struct zsdevice *zsaddr;
	struct zs_softc *zsp;
	int wr3, wr4, wr5, rr1;
	int tmp, charmask, s;
 
	XDBG(("zsparam\n"));
	tp = &zs_tty[unit];
	zsp = &zs_softc[unit];
 	zsaddr = zsp->zs_addr;

	if (tp->t_ispeed == 0) {
		XDBG(("zsparam dtr off\n"));
		(void) zsmctl(unit, ZSMODEM_OFF, DMSET); /* hang up line */
		return;
	}

	if (tp->t_ispeed >= sizeof(zs_speeds)/sizeof(zs_speeds[0]))
		tp->t_ispeed = ISPEED;	/* sleazy, but need to do something */
	tmp = zs_tc(zs_speeds[tp->t_ispeed], 16);

	/*
	 * Setup SCC as for parity and character len
	 * Note: zs_charmask is anded with both transmitted and received
	 * characters.
	 */
	wr3 = 0;
	wr4 = WR4_X16CLOCK;
	wr5 = 0;
	if (tp->t_flags & (RAW|LITOUT|PASS8|PASS8OUT)) {
		/*
		 * Transmit 8 bit data, NO parity
		 *	Frame is START | 8 Data | STOP
		 * Receive 8 bit data, NO parity
		 */
		wr3 |= WR3_RX8;
		wr5 |= WR5_TX8;
		charmask = 0xff;
#ifdef NONE_PARITY
	} else if ((tp->t_flags & (EVENP|ODDP)) == 0) {
		/*
		 * If neither EVENP or ODDP was specified, then transmit
		 * 7 bit chars (the mask is 7f!), with 0 parity; and receive
		 * 7 bit characters (the mask is 7f) and ignore the parity bit.
		 * 	Frame is START | 7 Data | 0 Parity | STOP
		 */
		wr3 |= WR3_RX8;
		wr5 |= WR5_TX8;
		charmask = 0x7f;
#endif NONE_PARITY
	} else {
		/*
		 * Either EVENP, ODDP, or both specified.
		 * Transmit 7 bit data.  If EVENP is specified,
		 * xmit even parity (even if ODDP is also specified),
		 * if only ODDP specified, xmit odd parity.
		 * 	Frame is START | 7 Data | Even or Odd Parity | STOP
		 */
		wr3 |= WR3_RX7;
		wr5 |= WR5_TX7;
		wr4 |= WR4_PARENABLE;
		charmask = 0x7f;
	}

	if (tp->t_flags & EVENP)
		wr4 |= WR4_PAREVEN;

	if (tp->t_ispeed == B110)
		wr4 |= WR4_STOP2;
	else
		wr4 |= WR4_STOP1;

	if (zsp->zs_wr3== wr3 && zsp->zs_wr4 == wr4 && zsp->zs_wr5 == wr5
	  && tmp == zsp->zs_tcval) {
	 	zsp->zs_charmask = charmask;
		return;
	}
	s = splscc();
	ZSREAD(zsaddr, rr1, 1);
	if (flush && (rr1 & RR1_ALLSENT) == 0) {
		splx(s);
		/* delay for 12 bit times to let last char get out of shift reg */
		us_delay(12000000 / zsp->zs_speed);
		s = splscc();
	}
	ZSWRITE(zsaddr, 4, wr4);	/* transmit/receive control */
	ZSWRITE(zsaddr, 3, wr3);	/* receiver parameters */
	ZSWRITE(zsaddr, 5, wr5 | zsmapwr5(zsp->zs_modem));
	zsp->zs_wr5 = wr5;
	if (tmp != zsp->zs_tcval) {
		/*
		 * baud rate generator has to be disabled while switching
		 * time constants and clock sources
		 */
		if ((zsp->zs_flags & ZSFLAG_DELAYSET) == 0) {
			zsp->zs_srintdelay.tv_sec = 0;
			zsp->zs_srintdelay.tv_usec = zssrintdelay(tp);
		}
		ZSWRITE(zsaddr, 14, 0);		/* Baudrate gen disabled */
		ZSWRITE(zsaddr, 12, tmp);	/* low bits of time constant */
		ZSWRITE(zsaddr, 13, tmp >> 8);	/* high bits of time constant */
		ZSWRITE(zsaddr, 14, tmp >> 16);	/* clock source */
		DELAY(10);
		ZSWRITE(zsaddr, 14, WR14_BRENABLE|(tmp >> 16));
	}
	wr3 |= WR3_RXENABLE;
	ZSWRITE(zsaddr, 3, wr3);
	/* TXENABLE is set by zswr5set when ZSMODEM_CTS is set */
	zswr5set(unit);
	zsp->zs_charmask = charmask;
	splx (s);
	zsp->zs_wr3 = wr3;
	zsp->zs_wr4 = wr4;
	zsp->zs_tcval = tmp;
	zsp->zs_speed = zs_speeds[tp->t_ispeed];
#ifdef notdef
	/*
	 * This is really only useful if you have the console
	 * on one of the SCC ports, then it keeps a console
	 * printf from zapping the baudrate established by
	 * getty.
	 */
	zs_init[unit] = 1;
#endif notdef
	XDBG(("zsparam done\n"));
}

static int
zssrintdelay(struct tty *tp)
{
	int d;

	/*
	 * In cooked or cbreak mode, delay a max of 10 character
	 * times (else Xoff's get delayed too much).  In raw mode,
	 * delay a max of 50 character times since Xoff's aren't
	 * interpreted.  No matter what, don't delay longer than
	 * 20ms.
	 */
	
	d = 10000000 / zs_speeds[tp->t_ispeed];	/* usecs/char */
	d *= ((tp->t_flags & RAW) ? 50 : 10);

	if (d > 20000)
		d = 20000;
	return d;
}

/*
 * NOTE: FIXSCALE * max_clk_hz must be < 2^31
 */
#define	FIXSCALE	256

int
zs_tc(int baudrate, int clkx)
{
	int i, br, f, freq;
	int rem[2], tc[2];
 
	/*
	 * This routine takes the requested baudrate and calculates
	 * the proper clock input and time constant which will program
	 * the baudrate generator with the minimum error
	 */
	/* Special case 134.5 baud */
	if (baudrate == 134 && clkx == 16)
		return(854 | (WR14_BRPCLK<<16));
	for (i = 0; i < 2; i++) {
		freq = (i == 0) ? PCLK_HZ : RTXC_HZ;
		tc[i] = ((freq+(baudrate*clkx))/(2*baudrate*clkx)) - 2;
		/* kinda fixpt calculation */
		br = (FIXSCALE*freq) / (2*(tc[i]+2)*clkx);
		rem[i] = abs(br - (FIXSCALE*baudrate));
	}
	if (rem[0] < rem[1])	/* PCLK is better clock choice */
		return((tc[0] & 0xffff) | (WR14_BRPCLK<<16));
	return(tc[1] & 0xffff);	/* RTxC is better clock choice */
}

/*
 * Receive routines
 */

/*
 * zsrint -- receiver interrupt handler
 * MUST BE CALLED AT splscc().
 * Queues appropriate state and posts software interrupt
 */
static void
zsrint(int unit)
{
	struct zs_softc *zsp;
	volatile struct zsdevice *zsaddr;
	struct zb_char zc;
	int cnt;

	/*
	 * FIXME: I think that we do NOT need to issue an error reset
	 * for framing and parity errors because we are in "interrupt
	 * per received character mode"  (I could be wrong!)
	 */
	ASSERT(curipl() == IPLSCC);
	zsp = &zs_softc[unit];
 	zsaddr = zsp->zs_addr;

	XXDBG(("zsrint enter\n"));
	DELAY(1);
	while (zsaddr->zs_ctrl & RR0_RXAVAIL) {
		ZSREAD(zsaddr, zc.zc_type, 1);
		zc.zc_type |= ZCTYPE_CHAR;
		if (zc.zc_type & RR1_RXOVER)
			zsp->zs_iflags |= ZSIFLAG_RXOVER;
		DELAY(1);
		zc.zc_c = zsaddr->zs_data;
		XXDBG(("zsrint received %c %d\n", zc.zc_c&0x7f, zc.zc_c));
		if (zsbufput(&zsp->zs_buf, &zc))
			zsp->zs_iflags |= ZSIFLAG_OVR;
		DELAY(1);
	}
	cnt = zscharcnt(&zsp->zs_buf);
	if (cnt >= ZSHIWAT) {
		if (cnt >= ZSLIMIT && (zsp->zs_flags & ZSFLAG_FLOWCTL)
		    && (zsp->zs_modem & ZSMODEM_RTS)) {
			XDBG(("zsrint: clearing RTS\n"));
			zsp->zs_modem &= ~ZSMODEM_RTS;
			zswr5set(unit);
		}
		if (zsp->zs_srinttime == 0) {
			XXDBG(("zsrint sched softint\n"));
			zsp->zs_srinttime = ZS_TIME(ZS_SRINTTIME);
			softint_sched (CALLOUT_PRI_SOFTINT0, zssoftrint, unit);
		}
	} else if ((zsp->zs_iflags & ZSIFLAG_SINTPEND) == 0) {
		XXDBG(("zsrint start srint timer\n"));
		zsp->zs_iflags |= ZSIFLAG_SINTPEND;
		us_timeout((func)zssoftrint, (vm_address_t)unit,
			&zsp->zs_srintdelay, CALLOUT_PRI_SOFTINT0);
	}
	XXDBG(("zsrint exit\n"));
}

/*
 * zssoftrint -- software interrupt routine for SCC receive
 */
static void
zssoftrint(int unit)
{
	struct tty *tp;
	struct zs_softc *zsp;
	struct zb_char zc;
	char buf[32], *cp;
	int n, stat_chk, changed, s;

	XXDBG(("zssoftrint\n"));
	tp = &zs_tty[unit];
	zsp = &zs_softc[unit];
	cp = buf;
	
	stat_chk = RR1_FRAME;
	if ((tp->t_flags & (EVENP|ODDP)) == EVENP
	    || (tp->t_flags & (EVENP|ODDP)) == ODDP)
		stat_chk |= RR1_PARITY;

	s = splscc();
	zsp->zs_srinttime = 0;
	zsp->zs_iflags &= ~ZSIFLAG_SINTPEND;
	splx(s);

	s = spltty();
	while((tp->t_state & TS_INPUTFULL) == 0 && zsbufget(&zsp->zs_buf, &zc)) {
		if (zc.zc_type == ZCTYPE_STATUS) {
			if (cp != buf) {
				n = cp - buf;
				zschars[unit] += n;
				if (tp->t_state & TS_ISOPEN)
				    (*linesw[tp->t_line].l_rend)(buf, n, tp);
				cp = buf;
			}
			splscc();
			changed = (zsp->zs_modem ^ zc.zc_c) & ZSMODEM_INPUTS;
			zsp->zs_modem ^= changed;
			spltty();
			XDBG(("zssoftrint: status changed 0x%x new 0x%x\n",
			  changed, zsp->zs_modem));
			if ((changed & ZSMODEM_DCD)
			 && (zsp->zs_flags & (ZSFLAG_HARDCAR|ZSFLAG_BUSYOUT))
			  == ZSFLAG_HARDCAR) {
				XDBG(("zssoftrint: DCD changed to %d\n",
				    zsp->zs_modem & ZSMODEM_DCD));
				if ((*linesw[tp->t_line].l_modem)(tp,
				 zsp->zs_modem & ZSMODEM_DCD) == 0) {
			    		(void) zsmctl(unit, ZSMODEM_OFF, DMSET);
					XDBG(("zssoftrint: DTR off\n"));
				}
			}
			/*
			 * did BREAK change state?
			 */
			if ((changed & ZSMODEM_RCVBRK)
			 && (zsp->zs_modem & ZSMODEM_RCVBRK) == 0) {
				/*
				 * The 8530 also silo's a NULL when
				 * a break completes.  Brute force
				 * here, we just flush everything
				 * after the break status change.
				 * Note that zsint checks for
				 */ 
				while(zschartype(&zsp->zs_buf) == ZCTYPE_CHAR)
					zsbufget(&zsp->zs_buf, &zc);
				zc.zc_c = (tp->t_flags & RAW) ? 0 : tp->t_intrc;
				zc.zc_type = ZCTYPE_CHAR;
				goto rintchar;
			}
			continue;
		}
rintchar:
		/*
		 * Framing errors do NOT represent BREAK's on the
		 * 8530 (just garbage characters!)
		 */
		if (zc.zc_type & stat_chk) {
			XDBG(("zssoftrint: parity or framing error\n"));
			continue;
		}
		zc.zc_c &= zsp->zs_charmask;
		XXDBG(("zssoftrint got char %c %d\n", zc.zc_c, zc.zc_c));
#if NBK >  0
		if (tp->t_line == NETLDISC) {
			zc.zc_c &= 0177;
			BKINPUT(zc.zc_c, tp);
		} else
#endif NBK > 0
		{
			if (cp >= &buf[sizeof(buf)]) {
				n = cp - buf;
				zschars[unit] += n;
				if (tp->t_state & TS_ISOPEN)
				    (*linesw[tp->t_line].l_rend)(buf, n, tp);
				cp = buf;
			}
			*cp++ = zc.zc_c;
		}
	}
	if (cp != buf) {
		n = cp - buf;
		zschars[unit] += n;
		if (tp->t_state & TS_ISOPEN)
			(*linesw[tp->t_line].l_rend)(buf, n, tp);
	}
	XXDBG(("zssoftrint exit\n"));
	splscc();
	if ((zsp->zs_flags & ZSFLAG_FLOWCTL)
	 && (zsp->zs_modem & (ZSMODEM_RTS|ZSMODEM_DTR)) == ZSMODEM_DTR
	 && zscharcnt(&zsp->zs_buf) <= ZSLOWAT) {
	 	XDBG(("zssoftrint: turning RTS on\n"));
	 	zsp->zs_modem |= ZSMODEM_RTS;
		zswr5set(unit);
	}
	splx(s);
}

/*
 * Transmit routines
 */

static void
zsstart(struct tty* tp)
{
	struct zs_softc *zsp;
	int cc, s, unit;
 
	unit = ZSUNIT(minor(tp->t_dev));
	zsp = &zs_softc[unit];

	s = spltty();
	XXDBG(("zsstart enter\n"));
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP)) {
		XXDBG(("zsstart busy t_state 0x%x\n", tp->t_state));
		goto out;
	}
	if (tp->t_outq.c_cc <= TTLOWAT(tp)) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			XXDBG(("zsstart: wakeup outq\n"));
			wakeup((caddr_t)&tp->t_outq);
		}
		if (tp->t_wsel) {
			XXDBG(("zsstart: selwakeup outq\n"));
			selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
			tp->t_wsel = 0;
			tp->t_state &= ~TS_WCOLL;
		}
	}
	if (tp->t_outq.c_cc == 0) {
		XXDBG(("zsstart outq empty\n"));
		goto out;
	}
	if (tp->t_flags & (RAW|LITOUT|PASS8OUT))
		cc = ndqb(&tp->t_outq, 0);
	else {
		cc = ndqb(&tp->t_outq, 0200);
		if (cc == 0) {
			struct timeval tv;
			cc = getc(&tp->t_outq);
			ticks_to_timeval((cc&0x7f)+6, &tv);
			us_timeout((func)ttrstrt, (vm_address_t)tp, &tv,
				CALLOUT_PRI_SOFTINT0);
			tp->t_state |= TS_TIMEOUT;
			goto out;
		}
	}
	tp->t_state |= TS_BUSY;
	XXDBG(("zsstart queuing %d chars\n", cc));
	zsp->zs_dh.dh_start = zsp->zs_dc.dc_next
	    = zsp->zs_dh.dh_stop = tp->t_outq.c_cf;
	zsp->zs_dh.dh_stop += cc;
	if (zsp->zs_flags & ZSFLAG_DMA) {
		ASSERT(zsp->zs_dh.dh_link == NULL);
		dma_start(&zsp->zs_dc, &zsp->zs_dh, DMACSR_WRITE);
	} else {
		splscc();
		zsxint(unit);
	}
out:
	XXDBG(("zsstart exit\n"));
	splx(s);
}
 
/*
 * zsxint -- SCC transmitter interrupt
 * Called for channel B to fake missing DMA hardware
 */
static void
zsxint(int unit)
{
	volatile struct zsdevice *zsaddr;
	struct zs_softc *zsp;
	struct dma_chan *dcp;
	struct dma_hdr *dhp;
	int rr0;

	zsp = &zs_softc[unit];

	ASSERT(curipl() == IPLSCC);
	ASSERT((zsp->zs_flags & ZSFLAG_DMA) == 0);

 	zsaddr = zsp->zs_addr;
	dhp = &zsp->zs_dh;
	dcp = &zsp->zs_dc;

	DELAY(1);
	while (zsaddr->zs_ctrl & RR0_TXEMPTY) {
		if (dcp->dc_next < dhp->dh_stop) {
			XXDBG(("zsxint sending %c %d\n", *dcp->dc_next&0x7f,
			    *dcp->dc_next));
			DELAY(1);
			zsaddr->zs_data = *dcp->dc_next++ & zsp->zs_charmask;
			DELAY(1);
			zsp->zs_xinttime = ZS_TIME(ZS_XINTTIME);
		} else {
			zsp->zs_xinttime = 0;
			DELAY(1);
			zsaddr->zs_ctrl = WR0_RESETTXPEND;
			XXDBG(("zsxint empty\n"));
			zsp->zs_sxinttime = ZS_TIME(ZS_SXINTTIME);
			softint_sched (CALLOUT_PRI_SOFTINT0, zssoftxint, unit);
			break;
		}
	}
	XXDBG(("zsxint exit\n"));
}

/*
 * zssoftxint -- handle software interrupts from DMA or pseudo-dma
 * completion
 */
static void
zssoftxint(int unit)
{
	struct tty *tp;
	struct dma_chan *dcp;
	struct zs_softc *zsp;
	int s;

	XXDBG(("zssoftxint enter, ipl %d\n", curipl()));
	ASSERT(curipl() == IPLSOFTCLOCK);

	s = spltty();
	tp = &zs_tty[unit];
	dcp = &zs_softc[unit].zs_dc;
	zsp = &zs_softc[unit];

	zsp->zs_sxinttime = 0;
	tp->t_state &= ~TS_BUSY;
	if (tp->t_state & TS_FLUSH)
		tp->t_state &= ~TS_FLUSH;
	else
		ndflush(&tp->t_outq, dcp->dc_next - tp->t_outq.c_cf);

	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		zsstart(tp);
	XXDBG(("zssoftxint exit\n"));
	splx(s);
}

/*
 * Status change routines
 */

static void
zssint(int unit)
{
	struct zs_softc *zsp;
	volatile struct zsdevice *zsaddr;
	struct tty *tp;
	struct zb_char zc;
	int ctrl;

	XDBG(("zssint enter\n"));
	ASSERT(curipl() == IPLSCC);
	zsp = &zs_softc[unit];
 	zsaddr = zsp->zs_addr;

	DELAY(1);
	ctrl = zsaddr->zs_ctrl;
	zc.zc_c = zsmaprr0(ctrl);
	zc.zc_type = ZCTYPE_STATUS;
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESET_STAT;
	XDBG(("zssint got status 0x%x\n", zc.zc_c));
	/*
	 * CTS transitions are handled here so flow control is
	 * not delayed.  On the otherhand, DCD and BREAK transitions
	 * are handled in zssoftrint so they appear in the proper
	 * place in the input stream.
	 */
	if ((zc.zc_c ^ zsp->zs_modem) & ZSMODEM_CTS) {
		tp = &zs_tty[unit];
		zsp->zs_modem ^= ZSMODEM_CTS;
		XDBG(("zssint: CTS turned %s\n",
		    zsp->zs_modem & ZSMODEM_CTS ? "on" : "off"));
		if (zsp->zs_flags & ZSFLAG_FLOWCTL)
			zswr5set(unit);
		if (((zsp->zs_modem ^ zc.zc_c) & ZSMODEM_INPUTS) == 0)
			return;
	}

	if (zsbufput(&zsp->zs_buf, &zc))
		zsp->zs_iflags |= ZSIFLAG_OVR;
	softint_sched (CALLOUT_PRI_SOFTINT0, zssoftrint, unit);
	XDBG(("zssint exit\n"));
}

/*
 * zsmaprr0 -- Maps rr0 hw bits to zs modem bits taking board rev into account
 */
static int
zsmaprr0(int hw_bits)
{
	int zs_bits;
	
	zs_bits = 0;
	if (hw_bits & RR0_BREAK) zs_bits |= ZSMODEM_RCVBRK;
	switch (zstype) {
	case ZSTYPE_DCD_IS_NCTS:
		if ((hw_bits & RR0_CTS) == 0) zs_bits |= ZSMODEM_DCD;
		zs_bits |= ZSMODEM_CTS;
		break;
	case ZSTYPE_DCD_IS_CTS:
		if (hw_bits & RR0_CTS) zs_bits |= ZSMODEM_DCD;
		zs_bits |= ZSMODEM_CTS;
		break;
	case ZSTYPE_FLOWCTL:
		if (hw_bits & RR0_DCD) zs_bits |= ZSMODEM_DCD;
		if (hw_bits & RR0_CTS) zs_bits |= ZSMODEM_CTS;
		break;
	}
	return zs_bits;
}

static int
zsmapwr5(int zs_bits)
{
	int hw_bits;

	hw_bits = 0;
	if (zs_bits & ZSMODEM_DTR) hw_bits |= WR5_DTR;
	if (zs_bits & ZSMODEM_BRK) hw_bits |= WR5_BREAK;
	switch (zstype) {
	case ZSTYPE_DCD_IS_NCTS:
	case ZSTYPE_DCD_IS_CTS:
		hw_bits |= WR5_RTS;
		break;
	case ZSTYPE_FLOWCTL:
		if (zs_bits & ZSMODEM_RTS) hw_bits |= WR5_RTS;
		break;
	}
	return hw_bits;
}

static int
zsmctl(int unit, int bits, int how)
{
	struct zs_softc *zsp;
	volatile struct zsdevice *zsaddr;
	int mbits, s;

	zsp = &zs_softc[unit];
	zsaddr = zsp->zs_addr;
	bits &= ZSMODEM_OUTPUTS;

	s = splscc();
	DELAY(1);
	mbits = zsp->zs_modem;
	XDBG(("zsmctl: mbits 0x%x\n", mbits));

	switch (how) {
	case DMSET:
		mbits = bits | (mbits & ZSMODEM_INPUTS);
		break;

	case DMBIS:
		mbits |= bits;
		break;

	case DMBIC:
		mbits &= ~bits;
		break;

	case DMGET:
		splx(s);
		return(mbits);
	}
	zsp->zs_modem = mbits;
	zswr5set(unit);
	(void) splx(s);

	return(mbits);
}

static void
zswr5set(int unit)
{
	volatile struct zsdevice *zsaddr;
	struct zs_softc *zsp;
	int wr5, s;

	zsp = &zs_softc[unit];
	zsaddr = zsp->zs_addr;
	s = splscc();
	wr5 = zsp->zs_wr5 | zsmapwr5(zsp->zs_modem);
	XDBG(("zswr5set: MODEM BITS 0x%x\n", zsp->zs_modem));
	if ((zsp->zs_modem & ZSMODEM_CTS)
	  || !(zsp->zs_flags & ZSFLAG_FLOWCTL))
		wr5 |= WR5_TXENABLE;
	XDBG(("zswr5set: SETTING 0x%x, transmitter %s\n", wr5,
	    wr5 & WR5_TXENABLE ? "on" : "off"));
	ZSWRITE(zsaddr, 5, wr5);
	splx(s);
}

/*
 * Timer routines
 */
static void
zstimer(void)
{
	int unit, s;
	struct zs_softc *zsp;
	struct timeval tv;

	for (unit = 0; unit < NZS; unit++) {
		zsp = &zs_softc[unit];
		s = spln(ipltospl(IPLSOFTCLOCK));
		if (zsp->zs_srinttime) {
			if (--zsp->zs_srinttime == 0) {
				printf("zs%d: lost recv SW interrupt\n", unit);
				XDBG(("LOST RECV SW INTERRUPT\n"));
				zssoftrint(unit);
			}
		}
		splx(s);
		s = spln(ipltospl(IPLSOFTCLOCK));
		if (zsp->zs_sxinttime) {
			if (--zsp->zs_sxinttime == 0) {
				printf("zs%d: lost xmit SW interrupt\n", unit);
				XDBG(("LOST XMIT SW INTERRUPT\n"));
				zssoftxint(unit);
			}
		}
		splx(s);
		s = spln(ipltospl(IPLSCC));
		if (zsp->zs_xinttime && (zsp->zs_modem & ZSMODEM_CTS)) {
			if (--zsp->zs_xinttime == 0) {
				printf("zs%d: lost xmit HW interrupt\n", unit);
				XDBG(("LOST XMIT HW INTERRUPT\n"));
				zsxint(unit);
			}
		}
		splx(s);
		if (zsp->zs_ovrtime) {
			zsp->zs_ovrtime -= 1;
			s = splscc();
			zsp->zs_iflags &= ~ZSIFLAG_OVR;
			splx(s);
		} else if (zsp->zs_iflags & ZSIFLAG_OVR) {
			log(LOG_WARNING, "zs%d: recv buffer overrun\n", unit);
			zsp->zs_ovrtime = ZS_TIME(ZS_OVRTIME);
		}
		if (zsp->zs_rxovertime) {
			zsp->zs_rxovertime -= 1;
			s = splscc();
			zsp->zs_iflags &= ~ZSIFLAG_RXOVER;
			splx(s);
		} else if (zsp->zs_iflags & ZSIFLAG_RXOVER) {
			log(LOG_WARNING, "zs%d: recv uart overrun\n", unit);
			zsp->zs_rxovertime = ZS_TIME(ZS_RXOVERTIME);
		}
	}
	ticks_to_timeval(ZS_TIMERPERIOD, &tv);
	us_timeout((func)zstimer, 0, &tv, CALLOUT_PRI_SOFTINT0);
}

/*
 * Support routines
 */

static int
dmtozs(int bits)
{
	int b;

	b = 0;
	if (bits & DML_CAR) b |= ZSMODEM_DCD;
	if (bits & DML_DTR) b |= ZSMODEM_DTR;
	if (bits & DML_RTS) b |= ZSMODEM_RTS;
	if (bits & DML_CTS) b |= ZSMODEM_CTS;
	return(b);
}

static int
zstodm(int bits)
{
	int b;

	b = 0;
	if (bits & ZSMODEM_DCD) b |= DML_CAR;
	if (bits & ZSMODEM_DTR) b |= DML_DTR;
	if (bits & ZSMODEM_RTS) b |= DML_RTS;
	if (bits & ZSMODEM_CTS) b |= DML_CTS;
	return(b);
}

/*
 * Console support routines
 */
static void
zschan_init(volatile struct zsdevice *zsaddr, int speed)
{
	int tmp, s;

	s = splhigh();
	tmp = (zsaddr == ZSADDR_A) ? WR9_RESETA : WR9_RESETB;
	ZSWRITE(zsaddr, 9, tmp | WR9_NV);
	DELAY(10);
	ZSWRITE(zsaddr, 1, 0);
	ZSWRITE(zsaddr, 9, WR9_NV);
	ZSWRITE(zsaddr, 10, WR10_NRZ);
	ZSWRITE(zsaddr, 11, WR11_TXCLKBRGEN|WR11_RXCLKBRGEN);
	ZSWRITE(zsaddr, 15, 0);
	ZSWRITE(zsaddr, 4, WR4_X16CLOCK|WR4_STOP1);
	ZSWRITE(zsaddr, 3, WR3_RX8);
	ZSWRITE(zsaddr, 5, WR5_TX8);
	/*
	 * baud rate generator has to be disabled while switching
	 * time constants and clock sources
	 */
	ZSWRITE(zsaddr, 14, 0);		/* Baud rate generator disabled */
	tmp = zs_tc(speed, 16);
	/* setup scc clock select register */
	*(u_char*) P_SCC_CLK = PCLK_3684_MHZ | SCLKB_4_MHZ | SCLKA_4_MHZ;
	ZSWRITE(zsaddr, 12, tmp);
	ZSWRITE(zsaddr, 13, tmp>>8);
	ZSWRITE(zsaddr, 14, tmp>>16);
	DELAY(10);
	ZSWRITE(zsaddr, 14, WR14_BRENABLE|(tmp>>16));
	ZSWRITE(zsaddr, 3, WR3_RX8|WR3_RXENABLE);
	ZSWRITE(zsaddr, 5, WR5_TX8|WR5_RTS|WR5_DTR|WR5_TXENABLE);
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESET;
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESET_STAT;
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESETTXPEND;
	splx(s);
}

/*
 * Print a character on console.
 * Attempts to save and restore device
 * status.
 */
void
zsputc (dev_t dev, int c)
{
	int unit = ZSUNIT(minor(dev));
	volatile struct zsdevice *zsaddr = (unit == 0) ? ZSADDR_A : ZSADDR_B;
	int s, timo;

	if (unit > NZS)
		return;
	if (zs_init[unit] == 0) {
		zs_init[unit] = 1;
		zschan_init(zsaddr, 9600);
	}

	timo = 30000;

	/*
	 * Try waiting for the console tty to come ready,
	 * otherwise give up after a reasonable time.
	 */
	s = splhigh();
	DELAY(1);
	while ((zsaddr->zs_ctrl & RR0_TXEMPTY) == 0) {
		if(--timo == 0)
			break;
		DELAY(1);
	}
	if (c == 0) {
		splx(s);
		return;
	}
	DELAY(1);
	zsaddr->zs_data = c & 0xff;	/* FIXME: does chip add parity? */

	/* wait for, and clear, transmit interrupt */
	DELAY(1);
	while ((zsaddr->zs_ctrl & RR0_TXEMPTY) == 0)
		;
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESETTXPEND;
	DELAY(1);
	(void)splx(s);
	if (c == '\n')
		zsputc (dev, '\r');
}

/*
 *  Get character from console.
 */
int
zsgetc (dev_t dev)
{
	int unit = ZSUNIT(minor(dev));
	volatile struct zsdevice *zsaddr = (unit == 0) ? ZSADDR_A : ZSADDR_B;
	int s, c;

	if (unit > NZS)
		return;
	if (zs_init[unit] == 0) {
		zs_init[unit] = 1;
		zschan_init(zsaddr, 9600);
	}

	/*
	 *  Make sure we loop at or above the console interrupt priority
	 *  otherwise we can get into an infinite loop processing console
	 *  receive interrupts which will never be handled because the
	 *  interrupt routine is short-circuited while we are doing direct
	 *  input.
	 */
	s = splhigh();
	DELAY(1);
	while ((zsaddr->zs_ctrl&RR0_RXAVAIL)==0)
		DELAY(1);
	DELAY(1);
	c = zsaddr->zs_data;
	(void)splx(s);
#if	GDB
	if (dev != dbug_tp->t_dev) {
#endif	GDB
		if (c == '\r')
			c = '\n';
		zsputc (dev, c);
#if	GDB
	} else
#endif	GDB
		c &= 0x7f;
	return (c);
}
#endif


