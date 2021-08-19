/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes.  Removed dir.h
 *
 * 10-Feb-89	Doug Mitchell at NeXT
 *	Increased size of ptsputcbuf from 128 to 256
 *
 * 26-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Oops... select routines really do have to check whether
 *	the thread is waiting.  rsel or wsel could be non-zero from
 *	the last time...
 *
 * 15-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 31-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Support for multiple threads.
 *
 * 26-Aug-86  Robert V Baron (rvb) at Carnegie-Mellon University
 *	Copy TS_ONDELAY flag from control to slave pty in ptsopen.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 27-Sep-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_GENERIC: Expanded NPTY to (64+16).
 *
 * 14-May-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	Upgraded to 4.2BSD.  Carried over changes below. 
 *	[V1(1)]
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)tty_pty.c	7.1 (Berkeley) 6/5/86
 */

/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 */
#import <pty.h>

#if NPTY > 0
#import <sys/param.h>
#import <sys/systm.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/user.h>
#import <sys/conf.h>
#import <sys/file.h>
#import <sys/proc.h>
#import <sys/uio.h>
#import <sys/kernel.h>

#import <machine/spl.h>

#if NPTY == 1
#undef NPTY
#define	NPTY	(64+16)		/* crude XXX */
#endif

#define BUFSIZ 100		/* Chunk size iomoved to/from user */

/*
 * pts == /dev/tty[pqrs]?
 * ptc == /dev/pty[pqrs]?
 */
struct	tty pt_tty[NPTY];
struct	pt_ioctl {
	int	pt_flags;
	struct	proc *pt_selr, *pt_selw;
	u_char	pt_send;
	u_char	pt_ucntl;
} pt_ioctl[NPTY];
int	npty = NPTY;		/* for pstat -t */

static struct pt_soft {
	dev_t	ps_slavedev;
	int	ps_flags;
#define	PS_OPEN		0x01
} pt_soft[NPTY];

#define	PF_RCOLL	0x01
#define	PF_WCOLL	0x02
#define	PF_NBIO		0x04
#define	PF_PKT		0x08		/* packet mode */
#define	PF_STOPPED	0x10		/* user told stopped */
#define	PF_REMOTE	0x20		/* remote and flow controlled input */
#define	PF_NOSTOP	0x40
#define PF_UCNTL	0x80		/* user control mode */

/*ARGSUSED*/
ptsopen(dev, flag)
	dev_t dev;
{
	register struct tty *tp;
	struct pt_soft *psp;
	int error;

#ifdef lint
	npty = npty;
#endif
	if (minor(dev) >= NPTY)
		return (ENXIO);
	tp = &pt_tty[minor(dev)];
	psp = &pt_soft[minor(dev)];
	psp->ps_slavedev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);		/* Set up default chars */
		tp->t_ispeed = tp->t_ospeed = EXTB;
		tp->t_flags = 0;	/* No features (nor raw mode) */
	} else if (tp->t_state&TS_XCLUDE && u.u_uid != 0)
		return (EBUSY);
	if (tp->t_oproc)			/* Ctrlr still around. */
		tp->t_state |= TS_CARR_ON;
	if (flag & O_NDELAY)	/* Let X work:  Don't wait if O_NDELAY is on. */
		tp->t_state |= TS_ONDELAY;
	else
		while ((tp->t_state & TS_CARR_ON) == 0) {
			tp->t_state |= TS_WOPEN;
			sleep((caddr_t)&tp->t_rawq, TTIPRI);
		}
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error == 0)
		psp->ps_flags |= PS_OPEN;
	ptcwakeup(tp, FREAD|FWRITE);
	return (error);
}

ptsclose(dev)
	dev_t dev;
{
	register struct tty *tp;
	struct pt_soft *psp;

	tp = &pt_tty[minor(dev)];
	psp = &pt_soft[minor(dev)];
	if (psp->ps_flags & PS_OPEN) {
		(*linesw[tp->t_line].l_close)(tp);
		ttyclose(tp);
		psp->ps_flags = 0;
	}
	ptcwakeup(tp, FREAD|FWRITE);
}

ptsread(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	register struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int error = 0;

again:
	if (pti->pt_flags & PF_REMOTE) {
		while (tp == u.u_ttyp && u.u_procp->p_pgrp != tp->t_pgrp) {
			if ((u.u_procp->p_sigignore & sigmask(SIGTTIN)) ||
			    (u.u_procp->p_sigmask & sigmask(SIGTTIN)) ||
			    u.u_procp->p_flag&SVFORK)
				return (EIO);
			gsignal(u.u_procp->p_pgrp, SIGTTIN);
			sleep((caddr_t)&lbolt, TTIPRI);
		}
		if (tp->t_canq.c_cc == 0) {
			if (tp->t_state & TS_NBIO)
				return (EWOULDBLOCK);
			sleep((caddr_t)&tp->t_canq, TTIPRI);
			goto again;
		}
		while (tp->t_canq.c_cc > 1 && uio->uio_resid > 0)
			if (ureadc(getc(&tp->t_canq), uio) < 0) {
				error = EFAULT;
				break;
			}
		if (tp->t_canq.c_cc == 1)
			(void) getc(&tp->t_canq);
		if (tp->t_canq.c_cc)
			return (error);
	} else
		if (tp->t_oproc)
			error = (*linesw[tp->t_line].l_read)(tp, uio);
	ptcwakeup(tp, FWRITE);
	return (error);
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls ptsstart.
 */
ptswrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register struct tty *tp;

	tp = &pt_tty[minor(dev)];
	if (tp->t_oproc == 0)
		return (EIO);
	return ((*linesw[tp->t_line].l_write)(tp, uio));
}

ptsselect(dev, rw)
	dev_t dev;
	int rw;
{
	register struct tty *tp;

	tp = &pt_tty[minor(dev)];
	return ((*linesw[tp->t_line].l_select)(tp, rw));
}

/*
 * Start output on pseudo-tty.
 * Wake up process selecting or sleeping for input from controlling tty.
 */
ptsstart(tp)
	struct tty *tp;
{
	register struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];

	if (tp->t_state & TS_TTSTOP)
		return;
	if (pti->pt_flags & PF_STOPPED) {
		pti->pt_flags &= ~PF_STOPPED;
		pti->pt_send = TIOCPKT_START;
	}
	ptcwakeup(tp, FREAD);
}

ptcwakeup(tp, flag)
	struct tty *tp;
{
	struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];

	if (flag & FREAD) {
		if (pti->pt_selr) {
			selwakeup(pti->pt_selr, pti->pt_flags & PF_RCOLL);
			pti->pt_selr = 0;
			pti->pt_flags &= ~PF_RCOLL;
		}
		wakeup((caddr_t)&tp->t_outq.c_cf);
	}
	if (flag & FWRITE) {
		if (pti->pt_selw) {
			selwakeup(pti->pt_selw, pti->pt_flags & PF_WCOLL);
			pti->pt_selw = 0;
			pti->pt_flags &= ~PF_WCOLL;
		}
		wakeup((caddr_t)&tp->t_rawq.c_cf);
	}
}

/*ARGSUSED*/
ptcopen(dev, flag)
	dev_t dev;
	int flag;
{
	register struct tty *tp;
	struct pt_ioctl *pti;

	if (minor(dev) >= NPTY)
		return (ENXIO);
	tp = &pt_tty[minor(dev)];
	if (tp->t_oproc)
		return (EIO);
	tp->t_oproc = ptsstart;
	(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	tp->t_state &= ~TS_EXTPROC;
	tp->t_state |= TS_CARR_ON;
	pti = &pt_ioctl[minor(dev)];
	pti->pt_flags = 0;
	pti->pt_send = 0;
	pti->pt_ucntl = 0;
	return (0);
}

ptcclose(dev)
	dev_t dev;
{
	register struct tty *tp;
	struct pt_soft *psp;

	tp = &pt_tty[minor(dev)];
	psp = &pt_soft[minor(dev)];
	(void)(*linesw[tp->t_line].l_modem)(tp, 0);
	if (psp->ps_flags & PS_OPEN) {
		forceclose(psp->ps_slavedev);
		ptsclose(psp->ps_slavedev);
	}
	tp->t_oproc = 0;		/* mark closed */
}

ptcread(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	char buf[BUFSIZ];
	int error = 0, cc;

	/*
	 * We want to block until the slave
	 * is open, and there's something to read;
	 * but if we lost the slave or we're NBIO,
	 * then return the appropriate error instead.
	 */
	for (;;) {
		if (tp->t_state&TS_ISOPEN) {
			if (pti->pt_flags&PF_PKT && pti->pt_send) {
				error = ureadc((int)pti->pt_send, uio);
				if (error)
					return (error);
				if (pti->pt_send & TIOCPKT_IOCTL) {
					struct xx {
						struct sgttyb a;
						struct tchars b;
						struct ltchars c;
						int d;
						int e;
					} cb;
					cb.a.sg_ispeed = tp->t_ispeed;
					cb.a.sg_ospeed = tp->t_ospeed;
					cb.a.sg_erase = tp->t_erase;
					cb.a.sg_kill = tp->t_kill;
					cb.a.sg_flags = tp->t_flags;
					bcopy((caddr_t)&tp->t_intrc,
					      (caddr_t)&cb.b, sizeof(cb.b));
					bcopy((caddr_t)&tp->t_suspc,
					      (caddr_t)&cb.c, sizeof(cb.c));
					cb.d = tp->t_state;
					cb.e = ((unsigned)tp->t_flags)>>16;
					cc = MIN(uio->uio_resid, sizeof(cb));
					uiomove(&cb, cc, UIO_READ, uio);
				}
				pti->pt_send = 0;
				return (0);
			}
			if (pti->pt_flags&PF_UCNTL && pti->pt_ucntl) {
				error = ureadc((int)pti->pt_ucntl, uio);
				if (error)
					return (error);
				pti->pt_ucntl = 0;
				return (0);
			}
			if (tp->t_outq.c_cc && (tp->t_state&TS_TTSTOP) == 0)
				break;
		}
		if ((tp->t_state&TS_CARR_ON) == 0)
			return (EIO);
		if (pti->pt_flags&PF_NBIO)
			return (EWOULDBLOCK);
		sleep((caddr_t)&tp->t_outq.c_cf, TTIPRI);
	}
	if (pti->pt_flags & (PF_PKT|PF_UCNTL))
		error = ureadc(0, uio);
	while (uio->uio_resid > 0 && error == 0) {
		cc = q_to_b(&tp->t_outq, buf, MIN(uio->uio_resid, BUFSIZ));
		if (cc <= 0)
			break;
		error = uiomove(buf, cc, UIO_READ, uio);
	}
	if (tp->t_outq.c_cc <= TTLOWAT(tp)) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		if (tp->t_wsel) {
			selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
			tp->t_wsel = 0;
			tp->t_state &= ~TS_WCOLL;
		}
	}
	return (error);
}

ptsstop(tp, flush)
	register struct tty *tp;
	int flush;
{
	struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];
	int flag;

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pti->pt_flags |= PF_STOPPED;
	} else
		pti->pt_flags &= ~PF_STOPPED;
	pti->pt_send |= flush;
	/* change of perspective */
	flag = 0;
	if (flush & FREAD)
		flag |= FWRITE;
	if (flush & FWRITE)
		flag |= FREAD;
	ptcwakeup(tp, flag);
}

ptcselect(dev, rw)
	dev_t dev;
	int rw;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int s;

	if ((tp->t_state&TS_CARR_ON) == 0)
		return (1);
	switch (rw) {

	case FREAD:
		/*
		 * Need to block timeouts (ttrstart).
		 */
		s = spltty();
		if ((tp->t_state&TS_ISOPEN) &&
		     tp->t_outq.c_cc && (tp->t_state&TS_TTSTOP) == 0) {
			splx(s);
			return (1);
		}
		splx(s);
		/* FALLTHROUGH */

	case 0:					/* exceptional */
		if ((tp->t_state&TS_ISOPEN) &&
		    (pti->pt_flags&PF_PKT && pti->pt_send ||
		     pti->pt_flags&PF_UCNTL && pti->pt_ucntl))
			return (1);
		if (pti->pt_selr &&
			((thread_t)(pti->pt_selr))->wait_event
				== (int)&selwait)
			pti->pt_flags |= PF_RCOLL;
		else
			pti->pt_selr = (struct proc *) current_thread();
		break;


	case FWRITE:
		if (tp->t_state&TS_ISOPEN) {
			if (pti->pt_flags & PF_REMOTE) {
			    if (tp->t_canq.c_cc == 0)
				return (1);
			} else {
			    if (tp->t_rawq.c_cc+tp->t_canq.c_cc < TTYHOG(tp)-2)
				    return (1);
			    if (tp->t_canq.c_cc == 0 &&
			        (tp->t_flags & (RAW|CBREAK)) == 0)
				    return (1);
			}
		}
		if (pti->pt_selw &&
			((thread_t)(pti->pt_selw))->wait_event
				== (int)&selwait)
			pti->pt_flags |= PF_WCOLL;
		else
			pti->pt_selw = (struct proc *) current_thread();
		break;

	}
	return (0);
}

ptcwrite(dev, uio)
	dev_t dev;
	register struct uio *uio;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	register struct iovec *iov;
	register char *cp;
	register int cc = 0;
	char locbuf[BUFSIZ];
	int cnt = 0;
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int error = 0;

again:
	if ((tp->t_state&TS_ISOPEN) == 0)
		goto block;
	if (pti->pt_flags & PF_REMOTE) {
		if (tp->t_canq.c_cc)
			goto block;
		while (uio->uio_iovcnt > 0 && tp->t_canq.c_cc < TTYHOG(tp)-1) {
			iov = uio->uio_iov;
			if (iov->iov_len == 0) {
				uio->uio_iovcnt--;	
				uio->uio_iov++;
				continue;
			}
			if (cc == 0) {
				cc = MIN(iov->iov_len, BUFSIZ);
				cc = MIN(cc, TTYHOG(tp)-1-tp->t_canq.c_cc);
				cp = locbuf;
				error = uiomove(cp, cc, UIO_WRITE, uio);
				if (error)
					return (error);
				/* check again for safety */
				if ((tp->t_state&TS_ISOPEN) == 0)
					return (EIO);
			}
			if (cc)
				(void) b_to_q(cp, cc, &tp->t_canq);
			cc = 0;
		}
		(void) putc(0, &tp->t_canq);
		ttwakeup(tp);
		wakeup((caddr_t)&tp->t_canq);
		return (0);
	}
	while (uio->uio_iovcnt > 0) {
		iov = uio->uio_iov;
		if (cc == 0) {
			if (iov->iov_len == 0) {
				uio->uio_iovcnt--;	
				uio->uio_iov++;
				continue;
			}
			cc = MIN(iov->iov_len, BUFSIZ);
			cp = locbuf;
			error = uiomove(cp, cc, UIO_WRITE, uio);
			if (error)
				return (error);
			/* check again for safety */
			if ((tp->t_state&TS_ISOPEN) == 0)
				return (EIO);
		}
		while (cc > 0) {
			if ((tp->t_rawq.c_cc+tp->t_canq.c_cc) >= TTYHOG(tp)-2 &&
			   (tp->t_canq.c_cc > 0 ||
			      tp->t_flags & (RAW|CBREAK))) {
				wakeup((caddr_t)&tp->t_rawq);
				goto block;
			}
			(*linesw[tp->t_line].l_rint)(*cp++, tp);
			cnt++;
			cc--;
		}
		cc = 0;
	}
	return (0);
block:
	/*
	 * Come here to wait for slave to open, for space
	 * in outq, or space in rawq.
	 */
	if ((tp->t_state&TS_CARR_ON) == 0)
		return (EIO);
	if (pti->pt_flags & PF_NBIO) {
		iov->iov_base -= cc;
		iov->iov_len += cc;
		uio->uio_resid += cc;
		uio->uio_offset -= cc;
		if (cnt == 0)
			return (EWOULDBLOCK);
		return (0);
	}
	sleep((caddr_t)&tp->t_rawq.c_cf, TTOPRI);
	goto again;
}

/*ARGSUSED*/
ptyioctl(dev, cmd, data, flag)
	caddr_t data;
	dev_t dev;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	register struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int stop, error;
	extern ttyinput();

	/*
	 * IF CONTROLLER STTY THEN MUST FLUSH TO PREVENT A HANG.
	 * ttywflush(tp) will hang if there are characters in the outq.
	 */
	if (cmd == TIOCEXT) {
		/*
		 * When the TS_EXTPROC bit is being toggled, we need
		 * to send an TIOCPKT_IOCTL if the packet driver
		 * is turned on.
		 */
		if (*(int *)data) {
			if (pti->pt_flags & PF_PKT) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp);
			}
			tp->t_state |= TS_EXTPROC;
		} else {
			if ((tp->t_state & TS_EXTPROC) &&
			    (pti->pt_flags & PF_PKT)) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp);
			}
			tp->t_state &= ~TS_EXTPROC;
		}
		return (0);
	} else
	if (cdevsw[major(dev)].d_open == ptcopen)
		switch (cmd) {

		case TIOCPKT:
			if (*(int *)data) {
				if (pti->pt_flags & PF_UCNTL)
					return (EINVAL);
				pti->pt_flags |= PF_PKT;
			} else
				pti->pt_flags &= ~PF_PKT;
			return (0);

		case TIOCUCNTL:
			if (*(int *)data) {
				if (pti->pt_flags & PF_PKT)
					return (EINVAL);
				pti->pt_flags |= PF_UCNTL;
			} else
				pti->pt_flags &= ~PF_UCNTL;
			return (0);

		case TIOCREMOTE:
			if (*(int *)data)
				pti->pt_flags |= PF_REMOTE;
			else
				pti->pt_flags &= ~PF_REMOTE;
			ttyflush(tp, FREAD|FWRITE);
			return (0);

		case FIONBIO:
			if (*(int *)data)
				pti->pt_flags |= PF_NBIO;
			else
				pti->pt_flags &= ~PF_NBIO;
			return (0);

		case TIOCSETP:
		case TIOCSETN:
		case TIOCSETD:
			while (getc(&tp->t_outq) >= 0)
				;
			break;

		case TIOCSIG:
			if (*(unsigned int *)data >= NSIG)
				return(EINVAL);
			if ((tp->t_flags&NOFLSH) == 0)
				ttyflush(tp, FREAD|FWRITE);
			gsignal(tp->t_pgrp, *(unsigned int *)data);
			return(0);
		}
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag);
	if (error >= 0)
	    return (error);
	error = ttioctl(tp, cmd, data, flag);
	/*
	 * Since we use the tty queues internally,
	 * pty's can't be switched to disciplines which overwrite
	 * the queues.  We can't tell anything about the discipline
	 * from here...
	 */
#if	NeXT
	if (linesw[tp->t_line].l_kind)
#else	NeXT
	if (linesw[tp->t_line].l_rint != ttyinput)
#endif	NeXT
	{
		(*linesw[tp->t_line].l_close)(tp);
		tp->t_line = 0;
		(void)(*linesw[tp->t_line].l_open)(dev, tp);
		error = ENOTTY;
	}
	if (error < 0) {
		if (pti->pt_flags & PF_UCNTL &&
		    (cmd & ~0xff) == UIOCCMD(0)) {
			if (cmd & 0xff) {
				pti->pt_ucntl = (u_char)cmd;
				ptcwakeup(tp, FREAD);
			}
			return (0);
		}
		error = ENOTTY;
	}
	/*
	 * If external processing and packet mode send ioctl packet.
	 */
	if ((tp->t_state & TS_EXTPROC) && (pti->pt_flags & PF_PKT)) {
		switch(cmd) {
		case TIOCSETP:
		case TIOCSETN:
		case TIOCSETC:
		case TIOCSLTC:
		case TIOCLBIS:
		case TIOCLBIC:
		case TIOCLSET:
			pti->pt_send |= TIOCPKT_IOCTL;
		default:
			break;
		}
	}
	stop = (tp->t_flags & RAW) == 0 &&
#ifdef	__STDC__
	    tp->t_stopc == CTRL('s') && tp->t_startc == CTRL('q');
#else	__STDC__
	    tp->t_stopc == CTRL(s) && tp->t_startc == CTRL(q);
#endif	__STDC__
	if (pti->pt_flags & PF_NOSTOP) {
		if (stop) {
			pti->pt_send &= ~TIOCPKT_NOSTOP;
			pti->pt_send |= TIOCPKT_DOSTOP;
			pti->pt_flags &= ~PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	} else {
		if (!stop) {
			pti->pt_send &= ~TIOCPKT_DOSTOP;
			pti->pt_send |= TIOCPKT_NOSTOP;
			pti->pt_flags |= PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	}
	return (error);
}

#if	NeXT
/*
 * Process queued entries to ptsputc via software interrupt.
 */
static char ptsputcbuf[256], *ptsputcp = ptsputcbuf;
static void pts_process_putc(tp)
	struct tty *tp;
{
	register char *optr;
	register int s, n;

    again:
	optr = ptsputcp;
	if (b_to_q(ptsputcbuf, optr - ptsputcbuf, &tp->t_outq) == 0)
		ptsstart(tp);

	/*
	 * To guard against re-entrency we took a snapshot of ptsputcp
	 * above.  If it's not the same, copy the newly added stuff to
	 * the beginning of the buffer and try again.
	 */
	s = splhigh();
	if (n = ptsputcp - optr) {
		bcopy(optr, ptsputcbuf, n);
		ptsputcp = ptsputcbuf + n;
		splx(s);
		goto again;
	}
	ptsputcp = ptsputcbuf + n;
	splx(s);
}
#endif	NeXT

/*
 * Put and Get routines for ptys.  Used for kernel access to the
 * pty slave queues.
 */
ptsputc (dev, c)
	dev_t dev;
	register int c;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	char buf[2];

	buf[0] = c;
	c = 1;
	if (buf[0] == '\n') {
		buf[1] = '\r';
		c++;
	}

#if	NeXT
	/*
	 * We can't add to the queue if at ipl >= spltty().  Buffer
	 * the data and flag a software interrupt if there's not already
	 * data buffered.  The software interrupt can then cleanup
	 * the stuff.
	 */
	if (curipl() >= IPLTTY) {
		int s = splhigh();
		switch (ptsputcp-ptsputcbuf) {
		case 0:
			/*
			 * Start up the software interrupt on first character
			 * posted to buffer.
			 */
			softint_sched(0, pts_process_putc, (int)tp);
			break;
		case sizeof(ptsputcbuf):
			/*
			 * The thing's already full, tough luck.
			 */
			splx(s);
			return;
		}
		*ptsputcp++ = buf[0];
		if (c == 2 && ptsputcp != &ptsputcbuf[sizeof(ptsputcbuf)])
			*ptsputcp++ = buf[1];
		splx(s);
		return;
	}
#endif	NeXT
	if (b_to_q(buf, c, &tp->t_outq) == 0)
		ptsstart(tp);
}
#endif
