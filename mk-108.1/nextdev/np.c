/*
 * Copyright (c) 1988 by NeXT, Inc.
 *
 * HISTORY
 * 15-Aug-90	Doug Mitchell
 *	Added np_ioctl_common_top, retries on "No SBSY False" in np_serial_cmd.
 *
 * 10-Aug-90	Doug Mitchell
 *	Added timeout detection to np_serial_cmd.
 *
 * 29-Mar-90 	Peter King
 *	Put status device back in to ease release transition.
 *
 * 19-Mar-90	Peter King
 *	Totally gutted and cleaned up.  All recovery logic (except for
 *	dma_underrun is now done outside the kernel).  np_buddy--our
 *	old nemesis--is gone.  Status device is gone, too many race
 *	conditions and not enough need to justify the maintenance
 *	overhead.  np_sleep is gone (good thing, it was of
 *	questionable worth).  Printer initialization is now done by a
 *	softint thread, so we don't need to worry about an
 *	initialization lock.  np_serial_cmd has been made reliable and
 *	now watches for the printer door opening.  Other neat stuff.
 *
 * 04-Dec-89	Doug Mitchell
 *	Added lock_write(np_seriallock) in np_init_printer()
 *	Added lock_write(np_initlock) in np_power_off()
 * 27-Nov-89	Doug Mitchell
 *	Added sc_initl_th, sc_seriall_th mechanisms
 *	np_sleep() and np_nap() now take sc as argument
 * 15-Jul-89	Doug Mitchell
 *	Added PF_JAM logic
 * 04-Jun-89	Doug Mitchell
 *	All calls to np_gpinwait() now handle timeouts via np_serial_timeout()
 * 02-Jun-89	Doug Mitchell
 *	Added interruptible flag to np_gpinwait(), np_serial_cmd(), 
 *		np_init_printer()
 * 01-Jun-89	Doug Mitchell
 *	Added retries to clear SBSY in np_serial_cmd()
 * 31-May-89	Doug Mitchell
 *	Avoided panic() on stuck SBSY in np_serial_cmd()
 * 22-Mar-89	Doug Mitchell
 *	ifdef'd out non-blocking Set Manual Feed logic
 * 13-Mar-89	Doug Mitchell at NeXT
 *	Added PS_UNDERRUN logic in np_dev_intr()
 *  8-Mar-88  Peter King (king) at NeXT, Inc.
 *	Created.
 */

#import "np.h"
#if	NNP > 0

/*
 * NeXT Laser Printer device driver
 */

#import <machine/spl.h>
#import <sys/errno.h>
#import <sys/types.h>
#import <sys/buf.h>
#import <sys/boolean.h>
#import <sys/callout.h>
#import <sys/param.h>
#import <sys/systm.h>
#import <sys/kernel.h>
#import <sys/ioctl.h>
#import <nextdev/npio.h>
#import <kern/xpr.h>
#import <sys/file.h>
#import <kern/sched_prim.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <sys/proc.h>
#import <sys/uio.h>
#import <sys/user.h>
#import <kern/mach_types.h>
#import <sys/message.h>
#import <sys/notify.h>
#import <kern/lock.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <next/autoconf.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <nextdev/busvar.h>
#import <nextdev/dma.h>
#import <nextdev/npreg.h>
#import <next/printf.h>


/*
 * Margins structure
 */
typedef struct {
	int		left; 		/* # of bits to indent on left */
	int		top; 		/* # of lines in top margin */
	int		width; 		/* width of image in #'s of longwords 
					 */
	int		height; 	/* height of image in lines */
} np_margin_t;


/* Local Storage */
/* one per device */
struct np_softc {
	volatile struct np_regs	*sc_regs;
	struct dma_chan	sc_dc;		/* DMA info */
	struct dma_hdr	*sc_dl;		/* Pointer to DMA Header list */
	int		sc_dlsize; 	/* Number of entries in the list */
	u_char		sc_cflags; 	/* Control flags changed at intr level
					 */
#define	PCF_CMDTIMEOUT	0x01
	u_int		sc_flags;
#define	PF_MARGINSET	0x0001		/* Margins have been set for this page
					 */
#define	PF_OPEN		0x0002		/* Device is open for write */
#define	PF_MANUALFEED	0x0010		/* Manual paper feed selected */
#define PF_NEWMANFEED	0x0020		/* PF_MANFEED has changed */
#define	PF_WSELCOLL	0x0040		/* Collision on Write select happened
					 */
#define	PF_ESELCOLL	0x0080		/* Collision on Error select happened 
					 */
#define	PF_NBIO		0x0100		/* Non-blocking I/O for this printer */
	lock_data_t	sc_seriallock;	/* Lock for singular serial cmd */
	lock_data_t	sc_powerlock;	/* Lock for powering on the printer */
	u_char		sc_gpout; 	/* General Purpose output latch */
	u_char		sc_gpin; 	/* General Purpose input latch */
	u_char		sc_gpimask; 	/* GPIN Mask */
	thread_t	sc_wsel; 	/* Thread doing a write select */
	thread_t	sc_esel; 	/* Thread doing an exception select */
	u_int		sc_state; 	/* State the printer is in */
#define PS_OFF		0		/* power off */
#define	PS_NEEDSINIT	1		/* printer needs initialization */
#define	PS_READY	2		/* ready for normal I/O */
#define	PS_STARTING	3		/* print starting - waiting for 
					 *	VSREQ */
#define	PS_PRINTING	4		/* print in process */
#define PS_WINDINGDOWN	5		/* print completing */
#define	PS_ERROR	6		/* error in print job */
#define	PS_SHUTTINGDOWN	7		/* powering down */
#define PS_UNDERRUN	8		/* DMA underrun detected; retry */
#define PS_TIMEOUT	9		/* printing timed out */
	char 		*sc_last_dh_start;	/* last DMA start ptr for 
						 * current page */
	int		sc_retry;		/* reprint counter */
#define NUM_PRINT_RETRIES 3
	np_margin_t	sc_newmargin;
	u_char		sc_resolution; 		/* 300/400 DPI */
	u_char		sc_newresolution;
	u_int		sc_offset;
	u_int		sc_len;
};
typedef struct np_softc *np_softcp_t;

#define	splprinter()	splbio()

/*
 * Prototypes for static functions
 */
void np_send(volatile struct np_regs *np, int cmd, int data);
boolean_t np_recv(volatile struct np_regs *np, int *cmd, int *data);
void np_gpiwait_timeout(volatile struct np_softc *sc);
void np_nap(int nhz, volatile struct np_softc *sc);
void np_gpinwait(volatile struct np_softc *sc, int time);
void np_setmask(volatile struct np_softc *sc, u_char bit);
void np_clearmask(volatile struct np_softc *sc, u_char bit);
void np_setgpout(volatile struct np_softc *sc, u_char bit);
void np_cleargpout(volatile struct np_softc *sc, u_char bit);
boolean_t np_getgpi(volatile struct np_softc *sc, u_char *gpi);
void np_serial_timeout(volatile struct np_softc *sc);
void np_state_timeout(volatile struct np_softc *sc);
int np_serial_cmd(volatile struct np_softc *sc, u_char cmd, u_char *status);
void np_setstate(volatile struct np_softc *sc, u_int newstate);
int np_power_on(volatile struct np_softc *sc);
void np_power_off(volatile struct np_softc *sc);
void np_init_printer(volatile struct np_softc *sc);
void np_printing_timeout(volatile struct np_softc *sc);
void np_printing_shutdown(volatile struct np_softc *sc);
void np_startdata(volatile struct np_softc *sc);
int np_wait_printer_ready(volatile struct np_softc *sc);
int np_dev_intr(volatile struct np_softc *sc);
void np_dma_intr(volatile struct np_softc *sc);

/* 
 * prototypes for driver functions
 */
int np_open(dev_t dev, int flags);
int nps_open(dev_t dev, int flags);
int np_open_common(dev_t dev, int flags, int statusdev);
int np_close(dev_t dev, int flags);
int np_ioctl(dev_t dev, int cmd, caddr_t data, int flag);
int nps_ioctl(dev_t dev, int cmd, caddr_t data, int flag);
static int np_ioctl_common_top(dev_t dev, int cmd, caddr_t data, int flag, int statusdev);
int np_ioctl_common(dev_t dev, int cmd, caddr_t data, int flag, int statusdev);
int np_select(dev_t dev, int flags);
int nps_select(dev_t dev, int flags);
int np_select_common(dev_t dev, int flags, int statusdev);
int np_write(dev_t dev, struct uio *uio);
int np_probe(caddr_t regs, int unit);
int np_attach(struct bus_device *bd);

int np_copyright[] = {
	0x00434f50,		/* <null>COP */
	0x522e204e,		/* Y.<sp>N */
	0x65585420,		/* eXT<sp> */
	0x31393837,		/* 1987 */
};

/* Local Storage */
volatile struct np_softc np_softc[NNP];

struct bus_device	*np_dinfo[NNP];
struct bus_driver	npdriver = {
	np_probe, 0, np_attach, 0, 0, np_dev_intr, 0,
	sizeof (struct np_regs), "np", np_dinfo,
};

#if	DEBUG
static int npdbgflag = 0;
static int NP_DATA_TO = 15;		/* data transfer timeout in seconds */
static int VS_TIMEOUT=30;
static int NP_PRINT_DELAY=1;		/* delay between setting margins and
					 *   asserting PRINT in seconds */
#define NP_SBSY_TO	(hz / 2)	/* wait for SBSY after command 
					 *   transfer */
#define NP_SBSYDROP_TO	(hz / 20 + 1)	/* wait for SBSY to drop */
#define NP_PPRDY_TO	(hz * 2)	/* wait for PPRDY true after power 
					 *   up */
#define	npdebug(str) {if (npdbgflag)printf str; else XPR(XPR_PRINTER, str);}
#define XDBG(str) XPR(XPR_PRINTER, str)

struct reg_values npop_values[] = {
/* value		name				*/
{ NPSETPOWER,		"printer on/off"		},
{ NPSETMARGINS,		"Set margins"			},
{ NPSETRESOLUTION,	"Set resolution"		},
{ NPGETSTATUS,		"Get status"			},
{ NPCLEARRETRANS,	"Clear retransmit counter"	},
{ NPGETPAPERSIZE,	"Get paper size"		},
{ NPSETMANUALFEED,	"Set manual feed"		},
{ 0,			NULL				}
};

struct reg_values scstate_values[] = {
/* value		name				*/
{ PS_OFF,		"PS_OFF"			},
{ PS_NEEDSINIT,		"PS_NEEDSINIT"			},
{ PS_READY,		"PS_READY"			},
{ PS_STARTING,		"PS_STARTING"			},
{ PS_PRINTING,		"PS_PRINTING"			},
{ PS_WINDINGDOWN,	"PS_WINDINGDOWN"		},
{ PS_ERROR,		"PS_ERROR"			},
{ PS_SHUTTINGDOWN,	"PS_SHUTTINGDOWN"		},
{ PS_UNDERRUN,		"PS_UNDERRUN"			},
{ PS_TIMEOUT,		"PS_TIMEOUT"			},
{ 0,			NULL				}
};

struct reg_values npioc_values[] = {
/* value		name				*/
{ NPIOCPOP,		"printer op"			},
{ FIONBIO,		"set/clear non-blocking I/O"	},
{ FIOASYNC,		"set/clear async I/O"		},
{ 0,			NULL				}
};

#else	DEBUG
#define	npdebug(str)
#define XDBG(str)
#define NP_DATA_TO	15
#define	XPR_STATUS	0	/* true = show status device ioctls */
#define VS_TIMEOUT	30
#define NP_PRINT_DELAY	1
#define NP_SBSY_TO	(hz / 2)	/* wait for SBSY after command 
					 *   transfer */
#define NP_SBSYDROP_TO	(hz / 20 + 1)	/* wait for SBSY to drop */
#define NP_PPRDY_TO	(hz * 2)	/* wait for PPRDY true after power on*/
#endif	DEBUG

/* #define XPR_ON_SENDRCV	1 */
#define XPR_ON_GETGPI	1
/* #define NP_LOG_INTS	1	 */
#define RPT_COVR	1
/* #define STATDEV_EXCL 1	/ * nps is exclusive (one open at a time) */

/* Miscellaneous routines */
void
np_send(register volatile struct np_regs *np,
	int	cmd,
	int	data)
{
	extern	void	lpr_send();

#ifdef 	XPR_ON_SENDRCV		
	XDBG(("np_send called: cmd=%x, data=%x\n", cmd, data));
#endif	XPR_ON_SENDRCV
	lpr_send(cmd, data);
}

/*
 * Routine: np_recv
 * Function:
 *	Get a received command and its data out of the registers if it
 *	is there.
 * Implementation:
 *	Check to see if something has come in, if it has then get it
 *	and return TRUE, otherwise return FALSE.
 */
boolean_t
np_recv(register volatile struct np_regs	*np,
	int	*cmd,		/* OUTPUT */
	int	*data)		/* OUTPUT */
{
	int	s;

	s = splprinter();
	/*
	 * Let's go ahead and spin if a receive operation
	 * is in progress.
	 */
	while (np->np_csrint & RTX_PEND)
		;
	/*
	 * Return data if it's there.
	 */
	if ((np->np_csrint & CTRL_DAV) || (np->np_csrint & CTRL_OVR)) {

		/*
		 * Was there a previous data overrun?
		 */
		if (np->np_csrint & CTRL_OVR) {
			/*
			 * Clear it.
			 */
			 
			/* FIXME: why do we get so many of these? */
#ifdef	RPT_COVR
			XDBG(("np_recv: control overrun\n"));
#endif	RPT_COVR
			np->np_csr.control_ovr = 1;
		}

		/*
		 * We still might as well return one of the packets that
		 * came in and deal with it higher up.
		 */
		*cmd = np->np_csr.cmd;
		*data = np->np_data;
#ifdef	XPR_ON_SENDRCV
		XDBG(("np_recv: returning cmd=%x, data=%x\n", *cmd, *data));
#endif	XPR_ON_SENDRCV
		(void) splx(s);
		return(TRUE);
	}
	(void) splx(s);
	return(FALSE);
}

/*
 * Routine: np_nap
 * Function:
 *	Sleep the current task for the specified amount of HZ.  I
 *	didn't do this with msg_rcv because not all of this driver
 *	is compiled to call the right internal routines for Mach
 *	system calls.
 */
void
np_nap(int nhz, volatile struct np_softc *sc)
{
	boolean_t	flag = FALSE;

	timeout(thread_wakeup, (caddr_t)&flag, nhz);
	assert_wait((int)&flag, FALSE);
	thread_block();
}

/*
 * Routine: np_gpiwait_timeout
 * Function:
 *	Force a gpin packet to arrive by sending a request for it.
 */
void
np_gpiwait_timeout(register volatile struct np_softc *sc)
{
	XDBG(("np%d: np_gpiwait_timeout: called\n", sc - np_softc));
	np_send(sc->sc_regs, NP_GP_IN, 0);
}

/*
 * Routine: np_gpinwait
 * Function:
 *	Allows a thread to wait for the next incoming GPIN packet.  It
 *	will force one to come by a certain time by sending a GPIN
 *	request after the timeout. 
 */
void
np_gpinwait(register volatile struct np_softc *sc,
	    int	time)
{
	if (time) {
		timeout(np_gpiwait_timeout, (caddr_t)sc, time);
	}
	assert_wait((int)&(sc->sc_gpin), FALSE);
	thread_block();
	untimeout(np_gpiwait_timeout, (caddr_t)sc);
}

/*
 * Routine: np_setmask
 * Function:
 *	Used to set a bit in the GPI mask.  np_clearmask() is used to
 *	clear it.
 */
void
np_setmask(register volatile struct np_softc *sc,
	   u_char bit)
{
	int s = splprinter();

	sc->sc_gpimask |= bit;
	np_send(sc->sc_regs, NP_GPI_MASK, GP_BITS_MASK(sc->sc_gpimask));
	(void) splx(s);
}

void
np_clearmask(register volatile struct np_softc *sc,
	     u_char bit)
{
	int s = splprinter();

	sc->sc_gpimask &= ~(bit);
	np_send(sc->sc_regs, NP_GPI_MASK, GP_BITS_MASK(sc->sc_gpimask));
	(void) splx(s);
}

/*
 * Routines: np_setgpout and np_cleargpout
 * Function:
 *	Set and clear GPOUT signals.
 */
void
np_setgpout(register volatile struct np_softc *sc,
	   u_char bits)
{
	int s = splprinter();

	sc->sc_gpout |= bits;
	np_send(sc->sc_regs, NP_GP_OUT, GP_BITS_OUT(sc->sc_gpout));
	(void) splx(s);
}

void
np_cleargpout(register volatile struct np_softc *sc,
	      u_char bits)
{
	int s = splprinter();

	sc->sc_gpout &= ~(bits);
	np_send(sc->sc_regs, NP_GP_OUT, GP_BITS_OUT(sc->sc_gpout));
	(void) splx(s);
}

/*
 * Routine: np_getgpi
 * Function:
 *	Get the general input signal bits from the printer.  
 * Implementation:
 *	Disables the printer interrupts because it does not want
 *	the interrupt handler intercepting the incoming GPI packets.
 *	Return false if the printer does not respond in time.
 *	
 */
#define	MAXGPILOOP	5	/*
				 * FIXME: Reasonable value? Most seem to
				 * return on the first try.
				 */
#define MAXGPIRECV	40
boolean_t
np_getgpi(register volatile struct np_softc *sc,
	  u_char *gpi)
{
	register volatile struct np_regs *np = sc->sc_regs;
	register int	trys;
	register int	recv_trys;
	register int	pprdy_trys;
	int		cmd;		/* Returned command */
	int		data;		/* Returned data */
	int		s;
	boolean_t	gotit = FALSE;

#ifdef	XPR_ON_GETGPI
	XDBG(("np_getgpi: called\n"));
#endif	/* XPR_ON_GETGPI */

	s = splprinter();

	ASSERT(sc->sc_state != PS_OFF);

	/*
	 * Now send the request.
	 */
	for (pprdy_trys = 2; pprdy_trys > 0; pprdy_trys--) {
		for (trys = MAXGPILOOP; trys >= 0; trys--) {
			/* Send the GPIN request */
			np_send(np, NP_GP_IN, 0);

			/* Wait for a response */
			for (recv_trys = 0; recv_trys < MAXGPIRECV;
			     recv_trys++) {
				if (np_recv(np, &cmd, &data)) {
					if (cmd == NP_GP_OUT) {
						gotit = TRUE;
						break;
					}
				npdebug(("np_getgpi: stray packet, cmd = %x\n",
					 cmd));
				}
			}
			if (gotit) {
				break;
			}
		}
		if (!gotit) {
			(void) splx(s);
			npdebug(("np_getgpi: returning FALSE\n"));
			return(FALSE);
		}

		*gpi = GP_BITS_IN(data);

		/*
		 * Sometime PPRDY is reported as down when it isn't.  Do
		 * a retry just to make sure.
		 */
		if (!(*gpi & GPIN_PPRDY) && (sc->sc_state != PS_NEEDSINIT)) {
			XDBG(("np_getgpi: PPRDY down, retrying\n"));
			continue;
		}
		break;
	}

	/*
	 * Make sure that we don't lose notification of PPRDY
	 * going down.
	 */
	if (!(*gpi & GPIN_PPRDY) && (sc->sc_state != PS_NEEDSINIT)) {
		XDBG(("np_getgpi: confirmed PPRDY down\n"));
		if (sc->sc_state == PS_PRINTING) {
			np_printing_shutdown(sc);
		}
		np_setstate(sc, PS_NEEDSINIT);
	}

	(void) splx(s);

#ifdef	XPR_ON_GETGPI
	XDBG(("np_getgpi:returning TRUE\n"));
#endif	XPR_ON_GETGPI

	return(TRUE);
}

/*
 * Routine: np_serial_timeout
 * Function:
 *	Mark that the np_serial_cmd routine has timed out and
 *	wake up routines waiting for incoming GPI packets (i.e. the
 *	serial_cmd routine.
 */
void
np_serial_timeout(register volatile struct np_softc *sc)
{
	XDBG(("np%d: np_serial_timeout called\n", sc - np_softc));
	sc->sc_cflags |= PCF_CMDTIMEOUT;
	thread_wakeup(&sc->sc_gpin);
}

/*
 * Routine: np_serial_cmd
 * Function:
 *	Send a serial command to the printer on the SC and SCLK output
 *	lines.  Return the status returned.
 * Implementation:
 *	Twiddle the data and clock bits in the gp output latch
 *	to create a serial signal.  Then read in the response packet
 *	in a similar way and return it in "status."
 * Returns:
 *	0: success
 *	EIO: I/O error, the printer is really hosed.
 *	EDEVERR: Someone opened the door while we're reading the command
 */

#define	SERIALRETRY	3		/* # of times to re-send command */
#define FLUSH_RETRY	3		/* # of times to flush incoming status */
int
np_serial_cmd(register volatile struct np_softc *sc,
	      u_char 	cmd,
	      u_char	*status)
{
	u_char			gpin;
	register int 		i;	/* Generic counter */
	u_char			result;
	int			s;
	int			parity;
	int 			retry = SERIALRETRY;
	int			flush_retry;
	int			error;
	
	/*
	 * Get the serial command lock.
	 */
	lock_write((lock_t)&sc->sc_seriallock);

retry:
	/*
	 * Make sure that the printer is not trying to send us something.
	 */
	if (!np_getgpi(sc, &gpin)) {
		XDBG(("np_getgpi() failed in np_serial_cmd()\n"));
		error = EIO;
		goto np_serial_out;
	}

	while(gpin&GPIN_SBSY) {
		/*
		 * Yes, the printer is trying to send us something.
		 * Flush it.
		 */
		for(flush_retry=FLUSH_RETRY; flush_retry; flush_retry--) {
		

			npdebug(("np%d: np_serial_cmd - SBSY true\n", sc - np_softc));

			for (i = 0; i < 8; i++) {
				/*
				 * Cycle the clock signal.
				 */
				DELAY(50);
				np_setgpout(sc, GPOUT_CCLK);
				DELAY(50);
				np_cleargpout(sc, GPOUT_CCLK);
			}
			if (!np_getgpi(sc, &gpin)) {
			XDBG(("np_getgpi() # 2 failed in np_serial_cmd()\n"));
				error = EIO;
				goto np_serial_out;
			}
			if (!(gpin & GPIN_SBSY) || !(gpin & GPIN_PPRDY)) 
				break;			/* success */
		} /* for flush_retry */
		if(!flush_retry) {
			/* aaggh. Could not flush out incoming status. */
			XDBG(("np_serial_cmd: can't flush printer status\n"));
			error = EIO;
			goto np_serial_out;
		}
	} /* while SBSY */

 	/*
 	 * Tell the printer a command is coming.
  	 */
	np_setgpout(sc, GPOUT_CBSY);
	DELAY(200);

	/*
	 * Now send the command to the printer.
	 */
	for (i = 7; i >= 0; i--) {
		u_char	bit;

		bit = (cmd>>i) & 0x01;
		DELAY(50);
		s = splprinter();
		if (bit) {
			sc->sc_gpout |= GPOUT_CMD;
		} else {
			sc->sc_gpout &= ~GPOUT_CMD;
		}
		/*
		 * Cycle the clock.
		 */
		(void) splx(s);
		np_setgpout(sc, GPOUT_CCLK);
		DELAY(50);
		np_cleargpout(sc, GPOUT_CCLK);
	}

	/*
	 * Enable automatic sending of a GPI packet for change in state of
	 * SBSY.
	 *
	 * Note: We will sleep waiting for the state change because it
	 *	 is only guaranteed to take no longer than 200 ms. We'll
	 *	 give it 500 ms.
	 */
	np_setmask(sc, GPIN_SBSY);

	/*
	 * Now reset CMD and CBSY signals,
	 */
	DELAY(1000);
	np_cleargpout(sc, (GPOUT_CMD|GPOUT_CBSY));

	/*
	 * and wait to be notified about the change in SBSY.
	 */
	if (!np_getgpi(sc, (u_char *)&sc->sc_gpin)) {
		np_clearmask(sc, GPIN_SBSY);
		XDBG(("np_getgpi() #3 failed in np_serial_cmd()\n"));
		error = EIO;
		goto np_serial_out;
	}

	sc->sc_cflags &= ~(PCF_CMDTIMEOUT);
	timeout(np_serial_timeout, sc, (int) NP_SBSY_TO);	/* 1/2 sec */
	s = splprinter();
	while (!(sc->sc_cflags & PCF_CMDTIMEOUT) &&
	       !(sc->sc_gpin & GPIN_SBSY) &&
	       (sc->sc_gpin & GPIN_PPRDY)) {
		(void) np_gpinwait(sc, (int)(NP_SBSY_TO / 2));
	}
	(void) splx(s);
	untimeout(np_serial_timeout, sc);
	np_clearmask(sc, GPIN_SBSY);

	/*
	 * there are only three ways to get here - timeout, SBSY, or ~PPRDY.
	 */
	s = splprinter();
	if(sc->sc_cflags & PCF_CMDTIMEOUT) {
		/* 
		 * no response. The printer is dead. 
		 */
		(void) splx(s);
		XDBG(("timeout in np_serial_cmd() waiting for SBSY\n"));
		error = EIO;
		goto np_serial_out;
	}
	if (!(sc->sc_gpin & GPIN_PPRDY)) {
		/*
		 * Yikes!  The door has been opened.
		 */
		(void) splx(s);
		XDBG(("PPRDY false detected in np_serial_cmd()\n"));
		error = EDEVERR;
		goto np_serial_out;
	}
	if(!(sc->sc_gpin & GPIN_SBSY)) {
		/*
		 * Huh? 
		 */
		(void) splx(s);
		XDBG(("np_serial_cmd - no SBSY\n"));
		error = EIO;
		goto np_serial_out;
	}
	(void) splx(s);

	/*
	 * Now we can get the status message.
	 */
	result = 0;
	parity = 0;
	for (i = 0; i < 8; i++) {
		/*
		 * Cycle the clock.  The SC signal is valid after the
		 * falling edge.
		 */
		DELAY(50);
		np_setgpout(sc, GPOUT_CCLK);
		DELAY(50);
		np_cleargpout(sc, GPOUT_CCLK);
		if (!np_getgpi(sc, &gpin)) {
			XDBG(("np_getgpi() #4 failed in np_serial_cmd()\n"));
			error = EIO;
			goto np_serial_out;
		}
		result <<= 1;
		if (gpin & GPIN_STS) {
			result |= 0x01;
			parity++;
		}
	}
	result &= 0xff;

	/*
	 * Watch for SBSY dropping.  It is guaranteed to be no more than
	 * 10 ms, but we'll use 1/50th of a second to be gracious.
	 */
	np_setmask(sc, GPIN_SBSY);
	if (!np_getgpi(sc, (u_char *)&sc->sc_gpin)) {
		np_clearmask(sc, GPIN_SBSY);
		XDBG(("np_getgpi() #5 failed in np_serial_cmd()\n"));
		error = EIO;
		goto np_serial_out;
	}

	timeout(np_serial_timeout, sc, (int) NP_SBSYDROP_TO);
	sc->sc_cflags &= ~(PCF_CMDTIMEOUT);
	s = splprinter();
	while(!(sc->sc_cflags & PCF_CMDTIMEOUT) &&
	      (sc->sc_gpin & GPIN_SBSY) &&
	      (sc->sc_gpin & GPIN_PPRDY)) {
		(void) np_gpinwait(sc, (int)(NP_SBSYDROP_TO / 2 + 1));
	}
	(void) splx(s);
	untimeout(np_serial_timeout, sc);
	np_clearmask(sc, GPIN_SBSY);

	s = splprinter();
	if(sc->sc_cflags & PCF_CMDTIMEOUT) {
		/* 
		 * no response. The printer is dead. 
		 */
		(void) splx(s);
		if (retry--) {
			XDBG(("np_serial_command timeout (SBSY FALSE) "
				"RETRY\n"));
			goto retry;
		}
		XDBG(("timeout in np_serial_cmd() waiting for SBSY FALSE\n"));
		error = EIO;
		goto np_serial_out;
	}
	if (!(sc->sc_gpin & GPIN_PPRDY)) {
		/*
		 * The door has been opened.
		 */
		(void) splx(s);
		XDBG(("PPRDY false detected in np_serial_cmd()\n"));
		error = EDEVERR;
		goto np_serial_out;
	}

	if (sc->sc_gpin & GPIN_SBSY) {
		/*
		 * Timeout, printer is dead.
		 */
		(void) splx(s);
		XDBG(("timeout in np_serial_cmd() waiting for SBSY to drop\n"));
		error = EIO;
		goto np_serial_out;
	}
	(void) splx(s);

	/*
	 * Check the parity of the result.  It should be odd.
	 */
	if (!(parity % 2)) {
		if (retry--) {
			XDBG(("np_serial_command return val parity error,"
			      "retrying\n"));
			goto retry;
		}

		printf("np%d: serial return value parity error\n",
		       sc - np_softc);
		error = EIO;
		goto np_serial_out;
	}
	
	/*
	 * Check that this isn't an error from the printer controller.
	 */
	if (result & CMD_ERROR) {
		if (retry--) {
			goto retry;
		}
		if (result & CMD_PARITYERROR) {
			printf("np%d: serial command parity error\n",
			       sc - np_softc);
		} else {
			printf("np%d: serial command error\n",
			       sc - np_softc);
		}
		XDBG(("printer detected error in np_serial_cmd()\n"));
		error = EIO;
		goto np_serial_out;
	}

	*status = result;
	error = 0;
np_serial_out:
	if(error == EIO) {
		/*
		 * These error calls for a printer reset. EDEVERR is handled
		 * elsewhere.
		 */
		np_setstate(sc, PS_NEEDSINIT);
	}
	lock_done((lock_t)&sc->sc_seriallock);
	return(error);
}

/*
 * Routine: np_setstate
 * Function:
 *	Set the software state of the printer and wakeup anything that
 *	might appropriately be waiting for it.
 * Note:
 *	Should be called at splprinter().
 */
void
np_setstate(register volatile struct np_softc *sc,
 	    u_int newstate)
{
	register int s = splprinter();

	XDBG(("np_setstate called\n"));
	XDBG(("\t(oldstate = %n, newstate = %n)\n", 
		sc->sc_state, scstate_values,
	        newstate, scstate_values));

	sc->sc_state = newstate;
	switch(newstate) {
	      case PS_READY:
		/*
		 * Wakeup anyone doing a select for write.
		 */
		if (sc->sc_wsel) {
			selwakeup(sc->sc_wsel,
				  sc->sc_flags&PF_WSELCOLL);
			sc->sc_flags &= ~PF_WSELCOLL;
			sc->sc_wsel = THREAD_NULL;
		}
		break;

	      case PS_ERROR:
	      case PS_OFF:
		/*
		 * Wakeup anyone doing a select for exceptions.
		 */
		if (sc->sc_esel) {
			selwakeup(sc->sc_esel,
				  sc->sc_flags&PF_ESELCOLL);
			sc->sc_flags &= ~PF_ESELCOLL;
			sc->sc_esel = THREAD_NULL;
		}
		break;

	    case PS_NEEDSINIT:
		/*
		 * Schedule the printer initialization routine.
		 */
		softint_sched(CALLOUT_PRI_THREAD, np_init_printer,
			      (caddr_t) sc);
		break;
	}

	/*
	 * Now wakeup anyone waiting for a state change whether it
	 * has changed or not.
	 */
	wakeup(&sc->sc_state);
	(void) splx(s);
}

/*
 * Routine: np_setstate_rdyerr
 * Function:
 *	Set the state of the printer to PS_READY or PS_ERROR depending
 *	on the state of the GPIN_RDY signal.
 */
int
np_setstate_rdyerr(register volatile struct np_softc *sc)
{
	int	s;

	if (!np_getgpi(sc, (u_char *)&sc->sc_gpin)) {
		np_power_off(sc);
		return (EIO);
	} else {
		s = splprinter();
		if (sc->sc_gpin & GPIN_RDY) {
			np_setstate(sc, PS_READY);
		} else {
			np_setstate(sc, PS_ERROR);
		}
		(void) splx(s);
	}
	return(0);
}

/*
 * Routine: np_power_on
 * Function:
 *	Turn on the printer, reset its gate array, and make sure it is
 *	NeXT Printer.
 * Caveat:
 *	This should only be called when sc->sc_stat == PS_OFF or
 *	PS_SHUTTINGDOWN.
 */
int
np_power_on(register volatile struct np_softc *sc)
{
	register volatile struct np_regs *np = sc->sc_regs;
#if	FIXME
	/* This is used when ready copyright */
	register int i;		/* generic counter */
#endif	FIXME
	int trys;
	int cmd;
	int data;
	int s;

	XDBG(("np%d: np_power_on() called\n", sc - np_softc));

	/*
	 * Wait if we are in the middle of shutting down.
	 */
	s = splprinter();
	while (sc->sc_state == PS_SHUTTINGDOWN) {
		sleep((int)&sc->sc_state, PRIBIO);
	}
	ASSERT(sc->sc_state == PS_OFF);

	/*
	 * Reset the local interface.
	 */
	np->np_csr.reset = 0;
	np->np_csr.reset = 1;
	np->np_csr.control_lpon = 1;
	(void) splx(s);

	/*
	 * Give the printer a chance to deal with life.
	 */
	np_nap(3 * hz, sc);

	/*
	 * Now that things have settled down, we send a reset packet
	 * and look for the Copyright notice from the printer.
	 *
	 * Note: We do this at splprinter because we don't want the
	 *	 interrupt system to intercept the incoming packets.
	 */
	s = splprinter();
	np_send(np, NP_RESET, NP_RESET_DATA);

	/*
	 * First rev of hardware doesn't catch copyright, so we ask
	 * for the GP bits instead of trying to read the copyright. 
	 */
	DELAY(10);

	/* Flush left over responses to the NP_RESET command */
	while (np_recv(np, &cmd, &data))
		;

	np_send(np, NP_GP_IN, 0);

	for (trys = 40; trys > 0; trys--) {
		if (np_recv(np, &cmd, &data)) {
			/*
			 * Check that the packet is the right one.
			 */
			if (cmd != NP_GP_OUT) {

				npdebug(("np%d power_on: stray packet, cmd = %x\n", (sc - np_softc), cmd));

				continue;
			}
			break;
		}
	}
	if (trys <= 0) {
		/*
		 * This can only happen if there was no response.
		 */
		np->np_csr.control_lpon = 0;
		np->np_csr.reset = 0;
		(void) splx(s);
		return(ENODEV);
	}

	/* Initialize pointer to DMA header list */
	sc->sc_dl = NULL;
	(void) splx(s);

	/* Install interrupt routine. */
	install_scanned_intr(I_PRINTER, (func)np_dev_intr, (void *)sc);

	/* Initialize driver state */
	np_setstate(sc, PS_NEEDSINIT);

	XDBG(("\tnp_power_on - normal exit\n"));

	return(0);
}

/*
 * Routine: np_power_off
 * Function:
 *	Turn off the printer and reset the software state.
 */
void
np_power_off(register volatile struct np_softc *sc)
{
	int s;

	XDBG(("np%d: np_power_off()\n", sc - np_softc));
	s = splprinter();
	if (sc->sc_state == PS_SHUTTINGDOWN) {
		/* Uninstall interrupt handler. */
		uninstall_scanned_intr(I_PRINTER);

		/* Power off the printer */
		sc->sc_regs->np_csr.control_lpon = 0;
		sc->sc_regs->np_csr.reset = 0;
		np_setstate(sc, PS_OFF);
		sc->sc_flags &= ~(PF_MARGINSET);
	} else if (sc->sc_state != PS_OFF) {
		np_setstate(sc, PS_SHUTTINGDOWN);
		timeout(np_power_off, sc, hz * 5);
	}
	(void) splx(s);
}

/*
 * Routine: np_init_printer
 * Function:
 *	Initialize the printer and software state.
 * Implementation:
 *	Initialize the software copy of the GPIN signals.  Set up the
 *	GPOUT signals.
 * Note:
 *	If this initialization fails, it will turn the printer off to
 *	get it to a known state.
 */
void
np_init_printer(register volatile struct np_softc *sc)
	
{
	register int 	s;
	register int	error = 0;
	u_char		status;

	XDBG(("np%d: np_init_printer(%d) called\n", 
		sc - np_softc));

	s = splprinter();
	if (sc->sc_state != PS_NEEDSINIT) {
	    (void) splx(s);
	    XDBG(("\tnp_init_printer returning (OK, easy case)\n"));
	    return;
	}
	(void) splx(s);

	if (!np_getgpi(sc, (u_char *)&sc->sc_gpin)) {
		XDBG(("\tnp_getgpi() failed in  np_init_printer\n"));
		goto errout;
	}

restart_init:
	/* 
	 * reset outputs and masks to known state 
	 */
	sc->sc_gpout = 0;
	np_setgpout(sc, 0);
	sc->sc_gpimask = 0;
	np_setmask(sc, 0);

	/*
	 * Hold CPRDY signal false to reset the printer.
	 */
	np_nap(hz / 2, sc);

	/*
	 * Watch for PPRDY signal. Give it 2 seconds.
	 */
	np_setmask(sc, GPIN_PPRDY);
	timeout(np_serial_timeout, sc, (int) NP_PPRDY_TO);
	sc->sc_cflags &= ~(PCF_CMDTIMEOUT);
	s = splprinter();
	while(!(sc->sc_gpin & GPIN_PPRDY) &&
	      !(sc->sc_cflags & PCF_CMDTIMEOUT)) {
		np_gpinwait(sc, hz);
	}
	(void) splx(s);
	untimeout(np_serial_timeout, sc);
	np_clearmask(sc, GPIN_PPRDY);

	if(sc->sc_cflags & PCF_CMDTIMEOUT) {
		/* 
		 * no response. The printer is dead. 
		 */
	      XDBG(("\tnp_init_printer returning (EIO - printer dead)\n"));
	      goto errout;
	}

	/*
	 * We need to make sure that it is up for 2.5 seconds to make
	 * sure the printer is sincere.
	 */
	np_nap(hz * 25 / 10, sc);
	if (!np_getgpi(sc, (u_char *)&sc->sc_gpin)) {
		XDBG(("\tnp_getgpi # 2 failed in np_init_printer\n"));
		goto errout;
	}
	s = splprinter();
	if ((sc->sc_gpin & GPIN_PPRDY) == 0) {
		(void) splx(s);
		XDBG(("\tPPRDY false detected in np_init_printer\n"));
		goto errout;
	}
	(void) splx(s);
	
	/*
	 * Let printer know we are alive and wait for 2.5 seconds so
	 * that the printer knows we are sincere.
	 */
	np_setgpout(sc, GPOUT_CPRDY);
	np_nap(hz * 25 / 10, sc);

	/*
	 * Now send the EC0 command so that the printer knows we will
	 * always provide the clock for serial commands.
	 */
	if (error = np_serial_cmd(sc, EXTCLK_CMD, &status)) {
		XDBG(("\tnp_init_printer returning (EC0 error)\n"));
		if (error == EDEVERR) {
			/*
			 * Printer door opened, try to init again.
			 */
			goto restart_init;
		}
		goto errout;
	}

	/*
	 * Now that things are set up, we can initialize the state and
	 * watch for RDY changes.
	 */
	np_setmask(sc, GPIN_RDY|GPIN_PPRDY);

	if (sc->sc_flags & PF_MANUALFEED) {
		if (error = np_serial_cmd(sc, HANDFEED_CMD, &status)) {
			XDBG(("\tnp_init_printer returning (serial cmd error)\n"));
			if (error == EDEVERR) {
				/*
				 * Printer door opened, try to init again.
				 */
				goto restart_init;
			}
			goto errout;
		}
	}
	sc->sc_flags &= ~(PF_NEWMANFEED);
	sc->sc_resolution = DPI400;	/* to match hardware (gpout) */

	if (np_setstate_rdyerr(sc)) {
		goto errout;
	}

	XDBG(("\tnp_init_printer returning\n"));

	return;

errout:

	XDBG(("\tnp_init_printer returning after error\n"));
	lock_write((lock_t)&sc->sc_powerlock);
	np_power_off(sc);
	lock_done((lock_t)&sc->sc_powerlock);
	return;
}
	
/*
 * Routine: np_printing_timeout.
 * Function: shut down the DMA because it hasn't finished in a reasonable
 *	amount of time.
 */
void
np_printing_timeout(register volatile struct np_softc *sc)
{
	int	s;

	npdebug(("np%d: np_printing_timeout called.\n", sc - np_softc));

	s = splprinter();
	if(sc->sc_state == PS_PRINTING) {
		/*
		 * This should only happen if there is a paper jam or
		 * something.  Thus GPIN_RDY should be false.
		 */

		/*
		 * Shut down the printing.
		 */
		np_printing_shutdown(sc);
		np_setstate(sc,PS_TIMEOUT);
		(void) splx(s);

	} else {
		(void) splx(s);
		printf("np%d: spurious printer timeout\n", sc - np_softc);
	}
}
	
/*
 * Routine: np_printing_shutdown
 * Function:
 *	This routine is called by a variety of interrupt routines to
 *	shutdown printing at the end of a page.
 */
void
np_printing_shutdown(register volatile struct np_softc *sc)
{
	register volatile struct np_regs *np = sc->sc_regs;
	int	s;

	XDBG(("np%d: np_printing_shutdown called\n", sc - np_softc));

	s = splprinter();
	
	/* some race conditions could cause us to be called when sc_state 
	 * is NOT PS_PRINTING...such as I/O complete when we get a (bogus) DMA
	 * overrun before we get DMA complete.
	 */
	 
	if(sc->sc_state != PS_PRINTING) {
		(void) splx(s);
		return;
	}
	(void) splx(s);

	untimeout((int (*)())np_printing_timeout, (int)sc);

	s = spldma();
	np_send(np, NP_DATAOUT_CTRL(DATAOUT_OFF), 0);
	dma_abort(&sc->sc_dc);
	np->np_csr.dmaout_dmaen = 0;
	np_setstate(sc, PS_WINDINGDOWN);
	(void) splx(s);
}

/*
 * Routine: np_startdata
 * Function:
 *	Start up the video data flowing to the printer now that the
 *	page is positioned correctly.
 */
void
np_startdata(register volatile struct np_softc *sc)
{
	register volatile struct np_regs *np = sc->sc_regs;
	register struct dma_hdr *dhp;
	register u_int		firstlword;
	int	s;

	XDBG(("np%d: np_startdata called\n", sc - np_softc));
	s = splprinter();
	if (sc->sc_state != PS_STARTING) {
		(void) splx(s);
		return;
	}
	(void) splx(s);

	/*
	 * Reset VSYNC and PRINT.
	 */
	np_cleargpout(sc, (GPOUT_VSYNC|GPOUT_PRINT));

	/*
	 * Preload the first lword of data.
	 */
	dhp = sc->sc_dl;
	firstlword = (u_int) *dhp->dh_start;
	np_send(np, NP_DATA_OUT, firstlword);
	dhp->dh_start += sizeof(u_int);
	/*
	 * Set up the DMA.
	 */
	dma_start(&sc->sc_dc, sc->sc_dl, DMACSR_WRITE);
	lpr_csr_or(DMAOUT_DMAEN>>24);
	lpr_csr_or(DMAOUT_OVR>>24);

	/*
	 * Enable data out.
	 */
	np_setstate(sc, PS_PRINTING);
	if (sc->sc_resolution == DPI300) {
		np_send(np, NP_DATAOUT_CTRL(DATAOUT_300DPI), 0);
	} else {
		np_send(np, NP_DATAOUT_CTRL(DATAOUT_400DPI), 0);
	}

	/*
	 * Set up timeout in case printer jams.
	 */
	timeout((int (*)())np_printing_timeout, (int)sc, NP_DATA_TO * hz);
}

/*
 * Routine: np_wait_printer_ready
 * Function:
 *	Generalized wait procedure to wait for the printer to become
 *	ready.  If the printer is powered down, has an error, or
 *	if we are non-blocking, return the appropriate error.
 */
int
np_wait_printer_ready(register volatile struct np_softc	*sc)
{
	int s;
	int error = 0;

	/*
	 * Refresh the software gpin state
	 */
	if (!np_getgpi(sc, (u_char *)&sc->sc_gpin)) {
		XDBG(("\tnp_getgpi failed in np_wait_printer_ready\n"));
		return(EIO);
	}

	s = splprinter();
	while (sc->sc_state != PS_READY) {
		if ((sc->sc_state == PS_ERROR) && (sc->sc_flags & PF_NBIO)) {
			error = EDEVERR;
			goto out;
		}
		if (sc->sc_state == PS_OFF || sc->sc_state == PS_SHUTTINGDOWN){
			error = EPWROFF;
			goto out;
		}
		if (sc->sc_flags & PF_NBIO) {
			error = EWOULDBLOCK;
			goto out;
		}
		sleep((int)&sc->sc_state, PSLEP);
	}
out:
	(void) splx(s);
	return(error);
}
			

/* Interrupt routines. */

/*
 * Routine: np_dev_intr
 * Function:
 *	Handle printer interrupts.
 */
int
np_dev_intr(register volatile struct np_softc *sc)
{
	register volatile struct np_regs *np = sc->sc_regs;
	int s;
	u_char	cmd;
	u_int	data;
	boolean_t	newgpin = FALSE;

	/*
	 * This should only be called when we have an overrun,
	 * underrun, or when we have received a GPIN packet.
	 */
#ifdef	NP_LOG_INTS
	XDBG(("np%d: np_dev_intr called\n", sc - np_softc));
#endif	NP_LOG_INTS

	/*
	 * Check the DMA out underrun.  If we are printing, and we
	 * are on the last or second to last buffer in the chain, ignore it.
	 * Some hardware gives the device interrupt before the DMA interrupt
	 * even when no underrun has occurred, so check the current DMA 
	 * pointer as well as sc_state (which tells us whether or not a 
	 * DMA complete interrupt has occurred). 
	 */
	if (np->np_csrint & DMAOUT_OVR) {
		s = spldma();
		if ((sc->sc_state == PS_PRINTING) &&
		    (sc->sc_dc.dc_ddp->dd_next != sc->sc_last_dh_start) &&
		    (sc->sc_dc.dc_ddp->dd_start != sc->sc_last_dh_start)) {
		    
			/* no DMA interrupt and not on last DMA buffer; this a 
			 * genuine underrun. The page should be reprinted.
			 */
			XDBG(("np%d: np_dev_intr dmaout underrun\n", 
				sc - np_softc));
			np_printing_shutdown(sc);
			np_setstate(sc,PS_UNDERRUN);
		} else {
			/* normal I/O termination */
			XDBG(("np%d: Normal Printer I/O Complete\n", 
				sc - np_softc));
			XDBG(("\t\t(sc_state = %n)\n", sc->sc_state,
				scstate_values));
			if(sc->sc_state == PS_PRINTING) {
			
				/* careful of race condition.....we'll be 
				 * getting a DMA interrupt any moment...but
				 * we're at spldma.
				 */
				np_printing_shutdown(sc);
			}
		}
		lpr_csr_or(DMAOUT_OVR>>24);
		(void) splx(s);
	}

	if (np->np_csrint & DMAIN_OVR) {
		/*
		 * This should not happen since we are not selling
		 * scanners yet.
		 */
		printf("np%d: spurious dmain overrun\n", sc - np_softc);
		lpr_csr_or(DMAIN_OVR>>24);
	}

	if ((np->np_csrint & CTRL_OVR) || (np->np_csrint & CTRL_DAV)) {
		if (np->np_csrint & CTRL_OVR) {

			npdebug(("np%d: np_dev_intr control overrun\n", sc - np_softc));

			np->np_csr.control_ovr = 1; /* Reset the interrupt */
		}
		/*
		 * Overrun or not, we have received a packet.  It
		 * should only be a GPOUT.  Check the RDY and the
		 * PPRDY bit to see if we should flag a state change.
		 */
		cmd = np->np_csr.cmd;
		data = np->np_data;

#ifdef	NP_LOG_INTS
		XDBG(("np%d: np_dev_intr data available: cmd=%x, data=%x\n",
		      sc - np_softc, cmd, data));
#endif	NP_LOG_INTS

		if (cmd == NP_GP_OUT) {
			sc->sc_gpin = GP_BITS_IN(data);
			newgpin = TRUE;
		} else if (cmd != 0xc6) {
			/*
			 * Ignore bogus 0xc6 characters.  This is
			 * a hardware anomaly.
			 */
			printf("np%d: spurious packet received, cmd = %x\n",
			       sc - np_softc, cmd);
		}

		/*
		 * If we have new GPIN state, check to see if any action
		 * is neccesary.
		 */
		if (newgpin) {
			if (!(sc->sc_gpin & GPIN_PPRDY) &&
			    (sc->sc_state != PS_NEEDSINIT)) {
				/*
				 * Yikes!  The printer door has been opened.
				 */

				npdebug(("np%d: PPRDY false\n",
					 sc - np_softc));

				XDBG(("np%d: Door Opened\n", sc - np_softc));
				if (sc->sc_state == PS_PRINTING) {
					np_printing_shutdown(sc);
				}
				np_setstate(sc, PS_NEEDSINIT);
			} else {
				switch (sc->sc_state) {
				      case PS_READY:
					if (!(sc->sc_gpin & GPIN_RDY)) {
						XDBG((" np_dev_intr - printer dropped RDY\n"));
						np_setstate(sc, PS_ERROR);
					}
					break;

				      case PS_ERROR:
					if (sc->sc_gpin & GPIN_RDY) {
						np_setstate(sc, PS_READY);
					}
					break;
				}
			}
			thread_wakeup(&(sc->sc_gpin));
		}
	}
#ifdef	NP_LOG_INTS
	XDBG(("np%d: np_dev_intr exiting\n", sc - np_softc));
#endif	NP_LOG_INTS
	return (0);
}

/*
 * Routine: np_dma_intr
 * Function:
 *	Handle interrupts from the DMA routines.  These should only
 *	happen when a chain completes, or when there's an error of some
 *	sort.
 */ 
void
np_dma_intr(register volatile struct np_softc *sc)
{
	int s;

	s = splprinter();
#ifdef	NP_LOG_INTS
	XDBG(("np%d: np_dma_intr called; sc_state = %n\n", 
		sc - np_softc, sc->sc_state, scstate_values));
#endif	NP_LOG_INTS

	/*
	 * FIXME:  It would be nice to get an interrupt after we have
	 *	printed every ten pages or thereabouts so that we could
	 *	free them, but we need to wait for the improved DMA
	 *	support.  I don't want to trigger an interrupt on every
	 *	page because they are done with about every ms.  So...
	 *	for now, we unwire up all pages at the end of the transfer.
	 */

	/*
	 * Make sure np_dev_intr didn't beat us to it.
	 */
	if (sc->sc_state == PS_PRINTING) {
		/*
		 * Since we are only called here for errors and
		 * completion, shut down the dma.
		 */
		np_printing_shutdown(sc);
	} else {
		XDBG(("\t\t(not printing)\n"));
	}
	/*
	 * Check for errors.
	 */
	(void) spldma();
	if (sc->sc_dc.dc_flags & DMACHAN_ERROR) {
		printf("np%d: DMA error, flushing page\n", sc - np_softc);
		sc->sc_dc.dc_flags &= ~(DMACHAN_ERROR);
	}
	(void) splx(s);
}

/* Driver routines */

/*
 * Routine: np_open
 */
int
np_open(dev_t dev, int flags)
{
    return(np_open_common(dev, flags, FALSE));
}

/*
 * Routine: nps_open
 */
int
nps_open(dev_t dev, int flags)
{
    return(np_open_common(dev, flags, TRUE));
}

/*
 * Routine: np_open_common
 * Function:
 *	Check that it's OK to open the printer device.  Flag opens for
 *	writing since they are exclusive.
 */
#define NP_OPEN_EIO_RETRY	3

int
np_open_common(dev_t dev, int flags, int statusdev)
{
	register volatile struct np_softc *sc = &np_softc[minor(dev)];
	register struct bus_device *bd = np_dinfo[minor(dev)];
	int 		s;
	int		error = 0;
	u_char		status;
	int 		eio_retry=0;
	
	if ((minor(dev) >= NNP) || !(bd->bd_alive)) {
		error = ENXIO;
		goto open_out;
	}

	/*
	 * only allow one open of each device at a time 
	 */
	if (!statusdev) {
	    if (sc->sc_flags & PF_OPEN) {
		error = EBUSY;
		goto open_out;
	    }
	    sc->sc_flags |= (PF_OPEN);
	    sc->sc_flags &= ~(PF_NBIO|PF_MANUALFEED|PF_NEWMANFEED);
	}

	/*
	 * If the printer is powered off, we need to request a power
	 * on.
	 */
	lock_write((lock_t)&sc->sc_powerlock);
	s = splprinter();
	if (sc->sc_state == PS_OFF || sc->sc_state == PS_SHUTTINGDOWN) {
		(void) splx(s);
		if (error = np_power_on(sc)) {
			lock_done((lock_t)&sc->sc_powerlock);
			goto err_out;
		}
		s = splprinter();
	}
	lock_done((lock_t)&sc->sc_powerlock);

	if (!statusdev && sc->sc_state != PS_NEEDSINIT) {
		/*
		 * At least make sure the default parameters are set.
		 */
		(void) splx(s);
retry:
		if(error = np_serial_cmd(sc, CASSFEED_CMD, &status)) {
			switch(error) {
			    case EDEVERR:
				/*
				 * The printer door has been opened.
				 * np_init_printer will take care of things.
				 */
				error = 0;
				break;
			    case EIO:
			    	/*
				 * Give it another shot - these usually go 
				 * away.
				 */
				if(++eio_retry <= NP_OPEN_EIO_RETRY) {
#ifdef	DEBUG
					printf("np0: open EIO; RETRY\n");
#endif	DEBUG
					goto retry;
				}
				else {
					break;
				}
			    default:
			    	/* 
				 * No recovery from other errors.
				 */
				break;
			}
		} else if (sc->sc_resolution != DPI400) {
			np_cleargpout(sc, GPOUT_300DPI);
			/* Let the controller deal with the change */
			np_nap(2 * hz, sc);
			sc->sc_resolution = DPI400;
		}
	} else {
		(void) splx(s);
	}

err_out:
	if (error && !statusdev) {
		sc->sc_flags &= ~(PF_OPEN);
	}
open_out:
	return(error);
}

/*
 * Routine: np_close
 * Function:
 *	Close the printer device.
 */
int
np_close(dev_t dev, int flags)
{
	register volatile struct np_softc *sc = &np_softc[minor(dev)];

	sc->sc_flags &= ~PF_OPEN;
	XDBG(("np%d: close\n", sc - np_softc));
	return(0);
}	

/*
 * Routine: np_ioctl
 */
int
np_ioctl(dev_t dev, int cmd, caddr_t data, int flag)
{
	return (np_ioctl_common_top(dev, cmd, data, flag, FALSE));
}

/*
 * Routine: nps_ioctl
 */
int
nps_ioctl(dev_t dev, int cmd, caddr_t data, int flag)
{
	return(np_ioctl_common_top(dev, cmd, data, flag, TRUE));
}

#define NP_IOCTL_EIO_RETRY	3
#define NP_IOCTL_PPRDY_RETRY	2

static int
np_ioctl_common_top(dev_t dev, int cmd, caddr_t data, int flag, int statusdev)
{
	/*
	 * this is a catch-all retry loop for errors which lower-level retries
	 * did not recover from and for errors requiring printer 
	 * initialization. If state is PS_NEEDSINIT after an error, common 
	 * ioctl code will wait for re-init before trying again.
	 */
	int eio_retry=0;
	int pprdy_retry = 0;
	int rtn;
	struct npop *npop;
	
retry_top:
	rtn = np_ioctl_common(dev, cmd, data, flag, statusdev);
	switch(rtn) {
	    case 0:
	        npop = (struct npop *)data;
	    	if((cmd == NPIOCPOP) && 
		   (npop->np_op == NPGETSTATUS) &&
		   (npop->np_status.flags & NPDOOROPEN)) {
		    	/* 
		    	 * Door open - bogus status reported by hardware - try
			 * once more to make sure.
		     	 */
			if(++pprdy_retry <= NP_IOCTL_PPRDY_RETRY) {
#ifdef	DEBUG
				printf("np0: ioctl PPRDY false; RETRY\n");
#endif	DEBUG
				npdebug(("np_ioctl_common_top: PPRDY; "
					" RETRY\n"));
				goto retry_top;
			}
			else {
				npdebug(("np_ioctl_common_top: PPRDY; "
					" FATAL\n"));
				return(rtn);
			}
		}
	    	return(0);
	    case EIO:
		if(++eio_retry <= NP_IOCTL_EIO_RETRY) {
#ifdef	DEBUG
			printf("np0: ioctl EIO; RETRY\n");
#endif	DEBUG
			npdebug(("np_ioctl_common_top: EIO; RETRY\n"));
			goto retry_top;
		}
		else {
			printf("np0: ioctl error; fatal\n");
			npdebug(("np_ioctl_common_top: EIO; FATAL\n"));
			return(rtn);
		}
	    default:
		npdebug(("np_ioctl_common_top: rtn=%d; FATAL\n"));
		return(rtn);
	}
}

/*
 * Routine: np_iotcl_common
 * Function:
 *	Handle ioctl system call.
 */
int
np_ioctl_common(dev_t dev, int cmd, caddr_t data, int flag, int statusdev)
{
	register volatile struct np_softc	*sc = &np_softc[minor(dev)];
	struct npop 			*npop;
	int 				error = 0;
	int 				s;
	u_char				status;
	
	XDBG(("np_ioctl: cmd = %n\n", cmd, npioc_values));

	switch (cmd) {
	      case NPIOCPOP:
		npop = (struct npop *)data;

		XDBG(("\t\top  = %n\n", npop->np_op, npop_values));

		/*
		 * If we are the status device, limit the available cmds.
		 */
		if (statusdev) {
			switch(npop->np_op) {
			    case NPSETPOWER:
			    case NPSETMARGINS:
			    case NPSETRESOLUTION:
			    case NPSETMANUALFEED:
			    case NPCLEARRETRANS:
				error = ENXIO;
				goto ioctl_out;

			    default:
				break;
			}
		}

		if (npop->np_op == NPSETPOWER) {
			if (npop->np_power) {
				lock_write((lock_t)&sc->sc_powerlock);
				s = splprinter();
				if (sc->sc_state == PS_OFF ||
				    sc->sc_state == PS_SHUTTINGDOWN) {
					(void) splx(s);
					error = np_power_on(sc);
				} else {
					(void) splx(s);
				}
				lock_done((lock_t)&sc->sc_powerlock);

			} else {
				lock_write((lock_t)&sc->sc_powerlock);
				s = splprinter();
				/*
				 * Make sure the printer is not printing.
				 */
				while ((sc->sc_state != PS_READY) &&
				       (sc->sc_state != PS_ERROR) &&
				       (sc->sc_state != PS_OFF)) {
					sleep((int)&sc->sc_state, PSLEP);
				}
				(void) splx(s);
				np_power_off(sc);
				lock_done((lock_t)&sc->sc_powerlock);
			}
			goto ioctl_out;
		}

		/*
		 * The rest of these need to make sure the power is on.
		 */
		s = splprinter();
		if (sc->sc_state == PS_OFF || 
		    sc->sc_state == PS_SHUTTINGDOWN) {
			(void) splx(s);
			error = EPWROFF;
			goto ioctl_out;
		}
		(void) splx(s);

		switch(npop->np_op) {
		    case NPSETMARGINS:
			/*
			 * Check parameters.
			 */
			if ((npop->np_margins.left > 0x01ff) ||
			    (npop->np_margins.left < 0)) {
				/* Can only be 9 bits */
			    	error = EINVAL;
				goto ioctl_out;
			}

			if ((npop->np_margins.width > 0x7f) ||
			    (npop->np_margins.width <= 0)) {
				/* Can only be 7 bits. */
			    	error = EINVAL;
				goto ioctl_out;
			}
			if ((npop->np_margins.top <= 0) ||
			    (npop->np_margins.height <= 0)) {
			    	error = EINVAL;
				goto ioctl_out;
			}

			/*
			 * Store the values away.
			 */
			sc->sc_newmargin.left = npop->np_margins.left;
			sc->sc_newmargin.width = npop->np_margins.width;
			sc->sc_newmargin.height = npop->np_margins.height;
			sc->sc_newmargin.top = npop->np_margins.top;

			sc->sc_flags |= PF_MARGINSET;

			break;

		      case NPSETRESOLUTION:
			/*
			 * Check the parameters
			 */
			if ((npop->np_resolution != DPI300) &&
			    (npop->np_resolution != DPI400)) {
			    	error = EINVAL;
				goto ioctl_out;
			}

			/*
			 * Store the value.
			 */
			sc->sc_newresolution = npop->np_resolution;

			break;

		      case NPGETSTATUS:
			npop->np_status.flags = 0;
			npop->np_status.retrans = 0;

			/*
			 * Make sure the printer is initialized.
			 */
			s = splprinter();
			while (sc->sc_state == PS_NEEDSINIT) {
				sleep((int)&sc->sc_state, PSLEP);
			}

			if (sc->sc_state == PS_STARTING ||
			    sc->sc_state == PS_PRINTING) {
				splx(s);
				npop->np_status.flags = NPPAPERDELIVERY;
				break;
			}
			(void) splx(s);

			if(error = np_serial_cmd(sc, SR0_CMD, &status)) {
				/*
				 * there is one legitimate failure here.
				 * EDEVERR means that the printer cover
				 * has been opened. Log that one.
				 */
				if (error == EDEVERR) {
					npop->np_status.flags |= NPDOOROPEN;
					error = 0;
				}
				break;
			}
			if (status & SR0_PAPERDLVR) {
				npop->np_status.flags |= NPPAPERDELIVERY;
			}
			if (status & SR0_DATARETRANS) {
				u_char	otherstat;

				if (error = np_serial_cmd(sc, SR15_CMD,
							  &otherstat)){
					if (error == EDEVERR) {
						npop->np_status.flags |=
							NPDOOROPEN;
						error = 0;
						break;
					}
					goto ioctl_out;
				}
				npop->np_status.flags |= NPDATARETRANS;
				npop->np_status.retrans = (otherstat&0x7e)>>1;
			}
			if (status & SR0_WAIT) {
				npop->np_status.flags |= NPCOLD;
			}
			if (status & SR0_CALL) {
				/*
				 * Check the various call status registers.
				 */

				/* Operator call */
				if (error = np_serial_cmd(sc, SR1_CMD,
							  &status)) {
					if (error == EDEVERR) {
						npop->np_status.flags |=
							NPDOOROPEN;
						error = 0;
						break;
					}
					goto ioctl_out;
				}
				if (status & SR1_NOCARTRIDGE) {
					npop->np_status.flags |= NPNOCARTRIDGE;
				}
				if (status & SR1_NOPAPER) {
					npop->np_status.flags |= NPNOPAPER;
				}
				if (status & SR1_JAM) {
					npop->np_status.flags |= NPPAPERJAM;
				}
				if (status & SR1_DOOROPEN) {
					npop->np_status.flags |= NPDOOROPEN;
				}

				/* Service call */
				if (error = np_serial_cmd(sc, SR2_CMD,
							  &status)) {
					if (error == EDEVERR) {
						npop->np_status.flags |=
							NPDOOROPEN;
						error = 0;
						break;
					}
					goto ioctl_out;
				}
				if (status & (SR2_FIXINGASMBLY|SR2_POORBDSIG|
					      SR2_SCANNINGMOTOR)) {
					npop->np_status.flags |= NPHARDWAREBAD;
				}
				if (status & SR2_FIXINGASMBLY) {
					npop->np_status.flags |=
						NPHARDWAREBAD|NPFUSERBAD;
				}
				if (status & SR2_POORBDSIG) {
					npop->np_status.flags |=
						NPHARDWAREBAD|NPLASERBAD;
				}
				if (status & SR2_SCANNINGMOTOR) {
					npop->np_status.flags |=
						NPHARDWAREBAD|NPMOTORBAD;
				}
			}

			/*
			 * Check the toner status.
			 */
			if(error = np_serial_cmd(sc, SR15_CMD, &status)) {
				if (error == EDEVERR) {
					npop->np_status.flags |=
						NPDOOROPEN;
					error = 0;
					break;
				}
				goto ioctl_out;
			}
			if (status & SR15_NOTONER) {
				npop->np_status.flags |= NPNOTONER;
			}

			/*
			 * Check Manual feed status.
			 */
			if (sc->sc_flags & PF_MANUALFEED) {
				npop->np_status.flags |= NPMANUALFEED;
			}

			XDBG(("np%d: get status: flags = %XH\n",
				   sc - np_softc,npop->np_status.flags));

			break;

		      case NPCLEARRETRANS:
			/*
			 * Since we are sending a serial command, we
			 * should wait for the printer to be initialized.
			 */
			s = splprinter();
			if (sc->sc_state == PS_NEEDSINIT) {
				/*
				 * We can return here because the
				 * initialization process clears the
				 * retry count.
				 */
				(void) splx(s);
				goto ioctl_out;
			}
			(void) splx(s);
					
			if (error = np_serial_cmd(sc, RETRANSCANCEL_CMD,
						  &status)) {
				if (error == EDEVERR) {
					/*
					 * This only happens if the printer
					 * door is open.  When closed, the
					 * retrans will be reset.
					 */
					error = 0;
					break;
				}
				goto ioctl_out;
			}
			break;

		      case NPGETPAPERSIZE:
			/*
			 * Since we are sending a serial command, we
			 * should make sure the printer it initialized.
			 */
			s = splprinter();
retry_papersize:
			while (sc->sc_state == PS_NEEDSINIT) {
				sleep((int)&sc->sc_state, PSLEP);
			}
			(void) splx(s);

			if (error = np_serial_cmd(sc, SR5_CMD, &status)) {
				if (error == EDEVERR) {
					goto retry_papersize;
				}
				goto ioctl_out;
			}
			switch (status) {
			      case SR5_NOCASSETTE:
				npop->np_size = NOCASSETTE;
				break;

			      case SR5_A4:
				npop->np_size = A4;
				break;

			      case SR5_LETTER:
				npop->np_size = LETTER;
				break;

			      case SR5_B5:
				npop->np_size = B5;
				break;

			      case SR5_LEGAL:
				npop->np_size = LEGAL;
				break;

			      default:
				XDBG(("bogus SR5 status (%x)\n",status));
			    	error = EIO;
				goto ioctl_out;
			}
			break;

		      case NPSETMANUALFEED:
		      	/* just record whether or not it is changing; we'll
			 * actually tell the printer about this the next time
			 * we print a page.
			 */
			if (!(sc->sc_flags & PF_MANUALFEED)&&npop->np_bool) 
				 sc->sc_flags |= PF_NEWMANFEED | PF_MANUALFEED;
			else if((sc->sc_flags & PF_MANUALFEED) &&
				 !(npop->np_bool)) {
				sc->sc_flags |= PF_NEWMANFEED;
				sc->sc_flags &= ~PF_MANUALFEED;
			}
			break;
   		      default:
			    error = ENXIO;
			    goto ioctl_out;
		}
		break;

	      case FIONBIO:
		if (*(int*)data) {
			sc->sc_flags |= PF_NBIO;
		} else {
			sc->sc_flags &= ~PF_NBIO;
		}
		break;

	      case FIOASYNC:
		break;

	      default:
		error = ENXIO;
		goto ioctl_out;
	}
ioctl_out:
	XDBG(("\tioctl - returning %d\n", error));
	return(error);
}

/*
 * Routine: np_select()
 */
int
np_select(dev_t dev, int flag)
{
	return(np_select_common(dev, flag, FALSE));
}

/*
 * Routine: nps_select();
 */
int
nps_select(dev_t dev, int flag)
{
	return(np_select_common(dev, flag, TRUE));
}

/*
 * Routine: np_select_common
 *
 * Function:
 *	Handle the select system call.
 */
int
np_select_common(dev_t dev, int flag, int statusdev)
{
	register volatile struct np_softc *sc = &np_softc[minor(dev)];
	int s;

	XDBG(("np%d: np_select()\n", sc - np_softc));

	if (minor(dev) > NNP) {
		return(ENXIO);
	}

	s = splprinter();
	switch (flag) {	
	      case FWRITE:
		if (statusdev) {
			break;
		}

		if (sc->sc_state == PS_READY) {
			goto win;
		}

		if (sc->sc_wsel && sc->sc_wsel->wait_event == (int)&selwait) {
			sc->sc_flags |= PF_WSELCOLL;
		} else {
			sc->sc_wsel = current_thread();
		}
		break;

	      case 0:		/* Exception */
		if (sc->sc_state == PS_ERROR || sc->sc_state == PS_OFF) {
			goto win;
		}

		if (sc->sc_esel && sc->sc_esel->wait_event == (int)&selwait) {
			sc->sc_flags |= PF_ESELCOLL;
		} else {
			sc->sc_esel = current_thread();
		}
		break;
	}
	(void) splx(s);
	XDBG(("np_select LOSE: flag = %d, current_thread = %x\n",
	      flag, current_thread()));
	return(0);
win:
	(void) splx(s);
	XDBG(("np_select WIN: flag = %d, current_thread = %x\n",
	      flag, current_thread()));
	return(1);		
}

int
np_write(dev_t dev,
	 struct uio *uio)
{
	register volatile struct np_softc *sc = &np_softc[minor(dev)];
	register volatile struct np_regs *np = sc->sc_regs;
	register struct iovec *iov;
	vm_map_t	procmap;
	vm_map_t	kerneltask_map;
	int		size;
	register int s;
	int		error;
	u_char		status;
	u_int		margindata;
	struct timeval 	tv;
	register struct dma_hdr *dhp;

	XDBG(("np_write\n"));

	if (minor(dev) > NNP) {
		error = ENXIO;
		goto np_write_out;
	}

	/*
	 * For now we only support one iovec.
	 */
	if (uio->uio_iovcnt != 1) {
		error = EINVAL;
		goto np_write_out;
	}
	iov = uio->uio_iov;

	/*
	 *  Page layout should already be set.
	 */
	if ((sc->sc_flags & PF_MARGINSET) == 0) {
		error = ENOINIT;
		goto np_write_out;
	}

	/*
	 * Check that the write parameters make sense.
	 */
	size = sc->sc_newmargin.width * NBPW *
		sc->sc_newmargin.height; /* image bytes */
	if (iov->iov_len < size) {
		/*
		 * Doesn't match the page paramters
		 */
		error = EINVAL;
		goto np_write_out;
	}

	if ((round_page(iov->iov_base) != (vm_offset_t) iov->iov_base) &&
	    !DMA_ENDALIGNED(iov->iov_base + iov->iov_len)) {
		/*
		 * Not long/quad word aligned.
		 */
		error = EINVAL;
		goto np_write_out;
	}

	/* Make sure the printer can accept the manual feed command. */
	s = splprinter();
	while (1) {
		if (sc->sc_state == PS_OFF ||
		    sc->sc_state == PS_SHUTTINGDOWN) {
			(void) splx(s);
			error = EPWROFF;
			goto np_write_out;
		}
		if (sc->sc_state != PS_NEEDSINIT) {
			break;
		}
		sleep((int)&sc->sc_state, PSLEP);
	}
	(void) splx(s);

	/*
	 * Change manual feed mode if changed since last page.
	 */
	if(sc->sc_flags & PF_NEWMANFEED) {
		if (sc->sc_flags & PF_MANUALFEED) {
			if (error = np_serial_cmd(sc, HANDFEED_CMD, &status)) {
				if (error != EDEVERR) {

				npdebug(("np%d: NPSETMANUALFEED HANDFEED_CMD "
					  "failed, error = %d\n",
					  sc - np_softc, error));

					goto np_write_out;
				}
			}
		} else {
			if (error = np_serial_cmd(sc, CASSFEED_CMD, &status)) {
				if (error != EDEVERR) {

			  npdebug(("np%d: NPSETMANUALFEED CASSFEED_CMD failed,"
					       " error = %d\n",
				  sc - np_softc, error));

					goto np_write_out;
				}
			}
		}
		sc->sc_flags &= ~PF_NEWMANFEED;
	} 
	
	/*
	 * Copy the image into kernel memory
	 *
	 */	
	procmap = current_task()->map;
	kerneltask_map = kernel_task->map;

	sc->sc_offset = 0;
	sc->sc_len = round_page(iov->iov_len);
	if (vm_map_find(kerneltask_map, VM_OBJECT_NULL, (vm_offset_t) 0,
			&sc->sc_offset, sc->sc_len, TRUE) != KERN_SUCCESS) {
		error = ENOMEM;
		goto np_write_out;
	}

	vm_map_reference(procmap);
	if (vm_map_copy(kerneltask_map, procmap, sc->sc_offset,
			sc->sc_len, iov->iov_base,
			FALSE, FALSE) != KERN_SUCCESS) {
		if (vm_map_remove(kerneltask_map, sc->sc_offset,
			      (sc->sc_offset + sc->sc_len)) != KERN_SUCCESS) {
			panic("np_write: vm_map_copy recovery");
		}
		vm_map_deallocate(procmap);
		error = EFAULT;
		goto np_write_out;
	}
	vm_map_deallocate(procmap);

	if (vm_map_protect(kerneltask_map, sc->sc_offset,
			   sc->sc_offset + sc->sc_len,
			   VM_PROT_READ, FALSE) != KERN_SUCCESS) {
		if (vm_map_remove(kerneltask_map, sc->sc_offset,
				  sc->sc_offset + sc->sc_len)
		    != KERN_SUCCESS) {
			panic("np_write: vm_map_protect recovery");
		}
		error = EFAULT;
		goto np_write_out;
	}

	/*
	 * Wire down the memory in the kernel.       
	 */
	if (vm_map_pageable(kerneltask_map, trunc_page(sc->sc_offset),
			    round_page(sc->sc_offset + sc->sc_len), FALSE) !=
	    KERN_SUCCESS) {
		if (vm_map_remove(kerneltask_map, sc->sc_offset,
				  sc->sc_offset + sc->sc_len)
		    != KERN_SUCCESS) {
			panic("np_write: vm_map_pageable recovery");
		}
		error = EFAULT;
		goto np_write_out;
	}

	/*
	 * Set up the DMA list.
	 */
	sc->sc_dlsize = howmany(sc->sc_len, PAGE_SIZE) + 1;
	sc->sc_dl = (struct dma_hdr *) kmem_alloc(kernel_map,
						  (sc->sc_dlsize *
						   sizeof(struct dma_hdr)));

	

	/* Init the retry count */
	sc->sc_retry = NUM_PRINT_RETRIES;

np_write_retry:
		
	/*
	 * Wait for the printer to be ready.  We do this again because
	 * setting the manual feed may drop RDY.
	 */
	if (error = np_wait_printer_ready(sc)) {
		goto np_write_abort;
	}
	
	dma_list(&sc->sc_dc, sc->sc_dl, sc->sc_offset, sc->sc_len,
		 kerneltask_map->pmap, DMACSR_WRITE, sc->sc_dlsize, 0, 0);

	/*
	 * Make sure that the last byte is 0 so that nothing will be
	 * printed in case we do not shut off the data requests fast
	 * enough at the end of a page.
	 */
	for (dhp = sc->sc_dl; dhp->dh_link; dhp++)
		;

	ASSERT(dhp < sc->sc_dl + sc->sc_dlsize - 1);

	if (*(dhp->dh_stop - 1) != 0) {
		dhp->dh_link = dhp + 1;
		dhp->dh_state = 0;
		dhp++;
		dhp->dh_start = (caddr_t)sc->sc_dc.dc_ptail;
		dhp->dh_stop = dhp->dh_start + DMA_ENDALIGNMENT;
		dhp->dh_link = NULL;
		dhp->dh_state = 0;
	}

	/* remember start of last page for underrun detect */
	sc->sc_last_dh_start = dhp->dh_start;
	

	/*
	 * Load print margins & set resolution
	 */
	margindata = (sc->sc_newmargin.width & MARGIN_REQC_MASK) <<
		MARGIN_REQC_SHIFT;
	margindata |= (sc->sc_newmargin.left & MARGIN_BITC_MASK);
	np_send(np, NP_MARGINS, margindata);
	if (sc->sc_newresolution != sc->sc_resolution) {
		if (sc->sc_newresolution == DPI300) {
			np_setgpout(sc, GPOUT_300DPI);
		} else {
			np_cleargpout(sc, GPOUT_300DPI);
		}

		/*
		 * Give the slow controller on the printer a chance
		 * to figure things out.
		 */
		np_nap(NP_PRINT_DELAY * hz, sc);

		sc->sc_resolution = sc->sc_newresolution;
	}

	/*
	 * Set the print signal and get things rolling.
	 */
	np_setgpout(sc, GPOUT_PRINT);

	/*
	 * Wait for VSREQ signal. Canon specs a 10 second timeout.
	 */	
	np_setmask(sc, GPIN_VSREQ);
	timeout(np_serial_timeout, sc, (int) (hz * VS_TIMEOUT));
	sc->sc_cflags &= ~(PCF_CMDTIMEOUT);
	s = splprinter();
	while (!(sc->sc_gpin & GPIN_VSREQ) && 
	       !(sc->sc_state == PS_NEEDSINIT) &&
	       !(sc->sc_state == PS_ERROR) &&
	       !(sc->sc_cflags & PCF_CMDTIMEOUT)) {
		np_gpinwait(sc, (5 * hz));
	}
	(void) splx(s);
	untimeout(np_serial_timeout, sc);
	np_clearmask(sc, GPIN_VSREQ);

	if(sc->sc_cflags & PCF_CMDTIMEOUT) {
		/* 
		 * no response. The printer is dead.  Shut it down
		 * and exit.
		 */
		XDBG(("np_write - VSREQ timeout, aborting.\n"));
		printf("np%d: VSREQ Timeout\n",sc-np_softc);
		lock_write((lock_t)&sc->sc_powerlock);
		np_power_off(sc);
		lock_done((lock_t)&sc->sc_powerlock);
		error = EIO;
		goto np_write_abort;
	}

	s = splprinter();
	if (sc->sc_state == PS_NEEDSINIT ||
	    sc->sc_state == PS_ERROR) {
		(void) splx(s);
		/*
		 * Some sort of error.  This should be handled by
		 * np_wait_printer_ready during the retry.
		 */
		np_cleargpout(sc, GPOUT_PRINT);	/* drop the print request */

		XDBG(("np_write - error waiting for VSREQ (retry)\n"));

		goto np_write_retry;
	}
	(void) splx(s);

	/*
	 * Now that we have the VSREQ signal, we set VSYNC and wait
	 * the magic amount of time.
	 */
	np_setgpout(sc, GPOUT_VSYNC);
	np_setstate(sc, PS_STARTING);

	tv.tv_usec = tv.tv_sec = 0;
	if (sc->sc_resolution == DPI300) {
		tv.tv_usec += (1797 * sc->sc_newmargin.top);
	} else {
		tv.tv_usec += (1348 * sc->sc_newmargin.top);
	}

	us_timeout(np_startdata, sc, &tv, CALLOUT_PRI_SOFTINT1);

	s = splprinter();
	while (sc->sc_state == PS_STARTING || sc->sc_state == PS_PRINTING) {
		sleep((int)&sc->sc_state, PRIBIO);
	}
	ASSERT((sc->sc_state == PS_WINDINGDOWN) || 
	       (sc->sc_state == PS_ERROR) ||
	       (sc->sc_state == PS_TIMEOUT) ||
	       (sc->sc_state == PS_NEEDSINIT) || 
	       (sc->sc_state == PS_UNDERRUN));

	/* 
	 * report/handle errors which occurred during printing 
	 */
	
	switch(sc->sc_state) {
	    case PS_UNDERRUN:
		(void) splx(s);
		if(--(sc->sc_retry)) {
			XDBG(("np_write - underrun retry\n"));
			printf("np%d: DMA Underrun; Reprinting Page\n",
			       sc - np_softc);
			/*
			 * Wait for VSREQ signal to go false.
			 */	
			np_setmask(sc, GPIN_VSREQ);
			s = splprinter();
			while ((sc->sc_gpin & GPIN_VSREQ) && 
			      !(sc->sc_state == PS_NEEDSINIT)) {
				np_gpinwait(sc, (5 * hz));
			}
			(void) splx(s);
			np_clearmask(sc, GPIN_VSREQ);

			/* note state may now be PS_NEEDSINIT..*/
			s = splprinter();
			if (sc->sc_state == PS_UNDERRUN) {
				(void) splx(s);
				if (error = np_setstate_rdyerr(sc)) {
					goto np_write_abort;
				}
			}
			(void) splx(s);
			goto np_write_retry;
		} else {
			/* don't retry forever */
			printf("np%d: DMA Underrun retries exhausted\n",
			    sc-np_softc);
			lock_write((lock_t)&sc->sc_powerlock);
			np_power_off(sc);
			lock_done((lock_t)&sc->sc_powerlock);
			error = EIO;
			goto np_write_abort;
		}
		
	    case PS_TIMEOUT:
		/*
		 * We assume this is because of a jam. Set the state
		 * depending on gpin and retry.
		 */
		(void) splx(s);
		
		if (error = np_setstate_rdyerr(sc)) {
			goto np_write_abort;
		}

		printf("np%d: Timeout waiting for Print Complete\n",
			sc-np_softc);
		goto np_write_retry;
		
	    case PS_NEEDSINIT:
		(void) splx(s);
		goto np_write_retry;

	    default:
		(void) splx(s);
	}

	/*
	 * Update the printer state.
	 */
	error = np_setstate_rdyerr(sc);

	sc->sc_flags &= ~(PF_MARGINSET);

	/* 
	 * come here to abort
	 */
np_write_abort:
	if (vm_map_remove(kerneltask_map, sc->sc_offset,
			  (sc->sc_offset + sc->sc_len))
	    != KERN_SUCCESS) {
		panic("np_write: can't remove vm");
	}

	kmem_free(kernel_map, (vm_offset_t) sc->sc_dl,
		  (sc->sc_dlsize *
		   sizeof(struct dma_hdr)));
	sc->sc_dl = NULL;

	/*
	 * Clean up the UIO struct.
	 */
	uio->uio_iov++;
	uio->uio_iovcnt--;
	iov->iov_base += iov->iov_len;
	uio->uio_offset += iov->iov_len;
	uio->uio_resid = 0;
	iov->iov_len = 0;

np_write_out:
	XDBG(("\t\tnp_write returning %d\n",error));
	return(error);
}

/* Autoconfiguration routines */

/*
 * Routine: np_probe
 * Function:
 *	Check to see if there is a printer interface
 * Implementation:
 *	Probe the CSR.
 *	Power on the printer, send a reset packet to its gate array
 *	and look for the return of the Copyright message, "COPR. NeXT
 *	1987"
 */
int
np_probe(register caddr_t regs,
	 int		  unit)
{
	register volatile struct np_regs *np;
	register volatile struct np_softc *sc = &np_softc[unit];

	/*
	 * init softc info
	 */
	bzero(sc, sizeof (struct np_softc));
	regs += slot_id;
	np = (struct np_regs *) regs;
	sc->sc_regs = np;

	/*
	 * Now probe for the CSR.
	 */
	return(probe_rb(np->np_csr.cmd)? (int)regs : 0);
	
}

/*
 * Routine: np_attach
 * Function:
 *	Attach to this device.
 * Implementation:
 *	Clean up CSR, set up GP signals on printer, install the interrupt
 *	handler.
 */
int
np_attach(register struct bus_device *bd)
{
	register volatile struct np_softc *sc = &np_softc[bd->bd_unit];
	register volatile struct np_regs *np = (struct np_regs *) bd->bd_addr;

	ASSERT(sc->sc_regs == np);
	/*
	 * Initialize the CSR.
	 */
	np->np_csr.reset = 0;

	/*
	 * Initialize the software control struct
	 */
	sc->sc_state = PS_OFF;
	sc->sc_flags = 0;
	sc->sc_wsel = sc->sc_esel = THREAD_NULL;
	lock_init((lock_t)(&sc->sc_seriallock), TRUE);
	lock_init((lock_t)(&sc->sc_powerlock), TRUE);

	/*
	 * Initialize the dma_chan and interrupt linkage.
	 */
	sc->sc_dc.dc_handler = np_dma_intr;
	sc->sc_dc.dc_hndlrarg = (int)sc;
	sc->sc_dc.dc_hndlrpri = CALLOUT_PRI_SOFTINT1;
	sc->sc_dc.dc_flags = DMACHAN_INTR; /* Interrupt at end of transfer. */
	sc->sc_dc.dc_ddp = (struct dma_dev *)P_PRINTER_CSR;
	bzero(sc->sc_dc.dc_tailbuf, sizeof(sc->sc_dc.dc_tailbuf));
	dma_init(&sc->sc_dc, I_PRINTER_DMA);

	return(0);	
}

#endif	NNP > 0
