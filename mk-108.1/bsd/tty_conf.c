/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)tty_conf.c	7.1 (Berkeley) 6/5/86
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/buf.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/conf.h>

int	nodev();
int	nulldev();

int	ttyopen(),ttylclose(),ttread(),ttwrite(),nullioctl(),ttstart();
int	ttymodem(), nullmodem(), ttyinput(), ttyblkin(), ttselect();


#define	NO_LINEDISC							\
	{								\
		nodev, nodev, nodev, nodev,				\
		nodev, nodev, nodev, nodev,				\
		nodev, nodev, nodev, 0					\
	}


struct	linesw linesw[] =
{
	{						/* 0 - OTTYDISC */
		ttyopen, ttylclose, ttread, ttwrite,
		nullioctl, ttyinput, ttyblkin, nulldev,
		ttstart, ttymodem, ttselect, 0
	},
	NO_LINEDISC,					/* 1 - unused */
	{						/* 2 - NTTYDISC */
		ttyopen, ttylclose, ttread, ttwrite,
		nullioctl, ttyinput, ttyblkin, nulldev,
		ttstart, ttymodem, ttselect, 0
	},
	NO_LINEDISC,					/* 3 - OLD TB */
	NO_LINEDISC,					/* 4 - loadable SLIP */
	NO_LINEDISC,					/* 5 - loadable PPP */
	NO_LINEDISC,					/* 6 - loadable */
	NO_LINEDISC,					/* 7 - loadable */
	NO_LINEDISC,					/* 8 - loadable */
	NO_LINEDISC					/* 9 - test slot */
};

int	nldisp = sizeof (linesw) / sizeof (linesw[0]);

/*
 * Do nothing specific version of line
 * discipline specific ioctl command.
 */
/*ARGSUSED*/
nullioctl(tp, cmd, data, flags)
	struct tty *tp;
	char *data;
	int flags;
{

#ifdef lint
	tp = tp; data = data; flags = flags;
#endif
	return (-1);
}
