/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * Revision 2.7  89/01/30  22:02:58  rpd
 * 	Added declaration of selwait.  (The one in sys/systm.h is extern now.)
 * 	[89/01/25  14:52:22  rpd]
 * 
 * 04-Dec-89  Mike DeMoney (mike) at NeXT
 *  Added support for 8 bit through in cooked mode (for EUC support)
 *
 * 26-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes. Removed dir.h
 *
 * 27-Feb-88  John Seamons (jks) at NeXT
 *	Changed macros to conform to ANSI C.
 *
 * 27-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	MACH_TT: select routines really do have to check whether the
 *	thread is waiting.  Pointer to select()'ing thread could be
 *	non-zero from the last time a select was done.
 *
 * 20-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Changed inode.h to vnode.h
 *
 * 17-Jun-87  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Added several 'spltty's around code which must atomically
 *	set and clear bits in memory, since some machines don't
 *	have instructions for this.  Intentionally neglected to
 *	include CS_BUGFIX conditionals, as they would have been
 *	excessively ugly.
 *
 *  1-Apr-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Well, dynix code thinks NTTYDISC is 1 and we think it is 2.  So
 *	amuse dynix for now.
 *
 *  5-Mar-87  David L. Black (dlb) at Carnegie-Mellon University
 *	Can't switch consoles on Multimax.
 *
 * 31-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Support for multiple threads.
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)tty.c	7.1 (Berkeley) 6/5/86
 */

#import <machine/reg.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/proc.h>
#import <sys/vnode.h>
#import <sys/file.h>
#import <sys/conf.h>
#import <sys/buf.h>
#import <sys/dk.h>
#import <sys/uio.h>
#import <sys/kernel.h>

#import <machine/spl.h>

#if	NeXT
#import <next/cons.h>
#import <nextdev/kmreg.h>
#endif	NeXT

int selwait;

/*
 * Table giving parity for characters and indicating
 * character classes to tty driver.  In particular,
 * if the low 6 bits are 0, then the character needs
 * no special processing on output.
 */

char partab[] = {
	0001,0201,0201,0001,0201,0001,0001,0201,
	0202,0004,0003,0201,0005,0206,0201,0001,
	0201,0001,0001,0201,0001,0201,0201,0001,
	0001,0201,0201,0001,0201,0001,0001,0201,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0201,

	/*
	 * 7 bit ascii ends with the last character above,
	 * but we contine through all 256 codes for the sake
	 * of the tty output routines which use special vax
	 * instructions which need a 256 character trt table.
	 */

	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007
};

/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Mostly used for
 * upper-case only terminals.
 */
char	maptab[] ={
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,'|',000,000,000,000,000,'`',
	'{','}',000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,'~',000,
	000,'A','B','C','D','E','F','G',
	'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W',
	'X','Y','Z',000,000,000,000,000,
};

short	tthiwat[16] =
   /*  0  50  75 110 134 150 200 300 600 1200 1800 2400 4800 9600 19.2 38.4 */
   { 100,100,100,100,100,100,100,200,200, 400, 400, 400, 650, 650,1300,2000 };
short	ttlowat[16] =
   /*  0  50  75 110 134 150 200 300 600 1200 1800 2400 4800 9600 19.2 38.4 */
   {  30, 30, 30, 30, 30, 30, 30, 50, 50, 120, 120, 120, 200, 200, 400, 600 };
short	tthog[16] =
   /*  0  50  75 110 134 150 200 300 600 1200 1800 2400 4800 9600 19.2 38.4 */
   { 255,255,255,255,255,255,255,255,255, 500, 500, 500,1000,1300,1500,2000 };

struct	ttychars ttydefaults = {
	CERASE,	CKILL,	CINTR,	CQUIT,	CSTART,	CSTOP,	CEOF,
	CBRK,	CSUSP,	CDSUSP, CRPRNT, CFLUSH, CWERASE,CLNEXT
};

ttysetspec(tp)
register struct tty *tp;
{
	register char *cp;
	register int i;

	for (i = 0; i < 8; i++)
		tp->t_spec[i] = 0;
	for (cp = (char *)&tp->t_chars; cp < (char *)(&tp->t_chars+1); cp++) {
		i = *cp & 0xff;
		tp->t_spec[i >> 5] |= 1 << (i & 0x1F);
	}
	/* \n and \r are always special */
	tp->t_spec['\n' >> 5] |= 1 << ('\n' & 0x1F);
	tp->t_spec['\r' >> 5] |= 1 << ('\r' & 0x1F);

	/*
	 * \377 can never be special, since it indicates that a special
	 * char is not bound.
	 */
	tp->t_spec[0377 >> 5] &= ~(1 << (0377 & 0x1f));
}

static inline int
ttyspec(tp, c)
struct tty *tp;
char c;
{
	return(tp->t_spec[(c & 0xFF) >> 5] & (1 << (c & 0x1F)));
}
	

ttychars(tp)
	struct tty *tp;
{
	tp->t_chars = ttydefaults;
	ttysetspec(tp);
}

/*
 * Wait for output to drain, then flush input waiting.
 */
ttywflush(tp)
	register struct tty *tp;
{

	ttywait(tp);
	ttyflush(tp, FREAD);
}

ttywait(tp)
	register struct tty *tp;
{
	register int s = spltty();

	while ((tp->t_outq.c_cc || tp->t_state&TS_BUSY) &&
	    tp->t_state&TS_CARR_ON) {
		(*tp->t_oproc)(tp);
		tp->t_state |= TS_ASLEEP;
		sleep((caddr_t)&tp->t_outq, TTOPRI);
	}
	splx(s);
}

/*
 * Flush all TTY queues
 */
ttyflush(tp, rw)
	register struct tty *tp;
{
	register s;

	s = spltty();
	if (rw & FREAD) {
		while (getc(&tp->t_canq) >= 0)
			;
		wakeup((caddr_t)&tp->t_rawq);
	}
	if (rw & FWRITE) {
		wakeup((caddr_t)&tp->t_outq);
		tp->t_state &= ~TS_TTSTOP;
		(*cdevsw[major(tp->t_dev)].d_stop)(tp, rw);
		while (getc(&tp->t_outq) >= 0)
			;
	}
	if (rw & FREAD) {
		while (getc(&tp->t_rawq) >= 0)
			;
		tp->t_rocount = 0;
		tp->t_rocol = 0;
		tp->t_state &= ~(TS_LOCAL|TS_INPUTFULL);
	}
	splx(s);
}

/*
 * Restart typewriter output following a delay
 * timeout.
 * The name of the routine is passed to the timeout
 * subroutine and it is called during a clock interrupt.
 */
ttrstrt(tp)
	register struct tty *tp;
{
	register int ipl = spltty();

	if (tp == 0)
		panic("ttrstrt");
	tp->t_state &= ~TS_TIMEOUT;
	(*linesw[tp->t_line].l_start)(tp);
        splx(ipl);
}

/*
 * Start output on the typewriter. It is used from the top half
 * after some characters have been put on the output queue,
 * from the interrupt routine to transmit the next
 * character, and after a timeout has finished.
 */
ttstart(tp)
	register struct tty *tp;
{
	register s;

	s = spltty();
	if ((tp->t_state & (TS_TIMEOUT|TS_TTSTOP|TS_BUSY)) == 0 &&
	    tp->t_oproc)		/* kludge for pty */
		(*tp->t_oproc)(tp);
	splx(s);
}

/*
 * Common code for tty ioctls.
 */
/*ARGSUSED*/
ttioctl(tp, com, data, flag)
	register struct tty *tp;
	caddr_t data;
{
	int dev = tp->t_dev;
	extern int nldisp;
	extern nodev();
	int s;
	register int newflags;

	/*
	 * If the ioctl involves modification,
	 * hang if in the background.
	 */
	switch (com) {

	case TIOCSETD:
	case TIOCSETP:
	case TIOCSETN:
	case TIOCFLUSH:
	case TIOCSETC:
	case TIOCSLTC:
	case TIOCSPGRP:
	case TIOCLBIS:
	case TIOCLBIC:
	case TIOCLSET:
	case TIOCSTI:
	case TIOCSWINSZ:
		while (tp->t_line == NTTYDISC &&
		   u.u_procp->p_pgrp != tp->t_pgrp && tp == u.u_ttyp &&
		   (u.u_procp->p_flag&SVFORK) == 0 &&
		   !(u.u_procp->p_sigignore & sigmask(SIGTTOU)) &&
		   !(u.u_procp->p_sigmask & sigmask(SIGTTOU))) {
			gsignal(u.u_procp->p_pgrp, SIGTTOU);
			sleep((caddr_t)&lbolt, TTOPRI);
		}
		break;
	}

	/*
	 * Process the ioctl.
	 */
	switch (com) {

	/* get internal state - needed for TS_EXTPROC bit */
	case TIOCGSTATE:
		*(int *)data = tp->t_state;
		break;

	/* get discipline number */
	case TIOCGETD:
		*(int *)data = tp->t_line;
		break;

	/* set line discipline */
	case TIOCSETD: {
		register int t = *(int *)data;
		int error = 0;

		if ((unsigned) t >= nldisp)
			return (ENXIO);
#if	NeXT
		if (linesw[t].l_open == nodev)
			return ENXIO;
#endif	NeXT
		if (t != tp->t_line) {
			s = spltty();
			(*linesw[tp->t_line].l_close)(tp);
#if	NeXT
			tp->t_ldisc_data = NULL;
#endif	NeXT
			error = (*linesw[t].l_open)(dev, tp);
			if (error) {
#if	NeXT
				tp->t_ldisc_data = NULL;
#endif	NeXT
				(void) (*linesw[tp->t_line].l_open)(dev, tp);
				splx(s);
				return (error);
			}
			tp->t_line = t;
			splx(s);
		}
		break;
	}

	/* prevent more opens on channel */
	case TIOCEXCL:
		s = spltty();
		tp->t_state |= TS_XCLUDE;
		splx(s);
		break;

	case TIOCNXCL:
		s = spltty();
		tp->t_state &= ~TS_XCLUDE;
		splx(s);
		break;

	/* hang up line on last close */
	case TIOCHPCL:
		s = spltty();
		tp->t_state |= TS_HUPCLS;
		splx(s);
		break;

	case TIOCFLUSH: {
		register int flags = *(int *)data;

		if (flags == 0)
			flags = FREAD|FWRITE;
		else
			flags &= FREAD|FWRITE;
		ttyflush(tp, flags);
		break;
	}

	/* return number of characters immediately available */
	case FIONREAD:
		s = spltty();
		*(off_t *)data = ttnread(tp);
		splx(s);
		break;

	case TIOCOUTQ:
		*(int *)data = tp->t_outq.c_cc;
		break;

	case TIOCSTOP:
		s = spltty();
		if ((tp->t_state&TS_TTSTOP) == 0) {
			tp->t_state |= TS_TTSTOP;
			(*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
		}
		splx(s);
		break;

	case TIOCSTART:
		s = spltty();
		if ((tp->t_state&TS_TTSTOP) || (tp->t_flags&FLUSHO)) {
			tp->t_state &= ~TS_TTSTOP;
			tp->t_flags &= ~FLUSHO;
			ttstart(tp);
		}
		splx(s);
		break;

	/*
	 * Simulate typing of a character at the terminal.
	 */
	case TIOCSTI:
		if (u.u_uid && (flag & FREAD) == 0)
			return (EPERM);
		if (u.u_uid && u.u_ttyp != tp)
			return (EACCES);
		s = spltty();
		(*linesw[tp->t_line].l_rint)(*(char *)data, tp);
		splx(s);
		break;

	case TIOCSETP:
	case TIOCSETN: {
		register struct sgttyb *sg = (struct sgttyb *)data;

		tp->t_erase = sg->sg_erase;
		tp->t_kill = sg->sg_kill;
		tp->t_ispeed = sg->sg_ispeed;
		tp->t_ospeed = sg->sg_ospeed;
		newflags = (tp->t_flags&0xffff0000) | (sg->sg_flags&0xffff);
		s = spltty();
		if (tp->t_flags&RAW || newflags&RAW || com == TIOCSETP) {
			ttywait(tp);
			ttyflush(tp, FREAD);
		} else if ((tp->t_flags&CBREAK) != (newflags&CBREAK)) {
			if (newflags&CBREAK) {
				struct clist tq;

				catq(&tp->t_rawq, &tp->t_canq);
				tq = tp->t_rawq;
				tp->t_rawq = tp->t_canq;
				tp->t_canq = tq;
			} else {
				tp->t_flags |= PENDIN;
				newflags |= PENDIN;
				ttwakeup(tp);
			}
		}
		tp->t_flags = newflags;
		ttysetspec(tp);
		if (tp->t_flags&RAW) {
			tp->t_state &= ~TS_TTSTOP;
			ttstart(tp);
		}
		splx(s);
		break;
	}

	/* send current parameters to user */
	case TIOCGETP: {
		register struct sgttyb *sg = (struct sgttyb *)data;

		sg->sg_ispeed = tp->t_ispeed;
		sg->sg_ospeed = tp->t_ospeed;
		sg->sg_erase = tp->t_erase;
		sg->sg_kill = tp->t_kill;
		sg->sg_flags = tp->t_flags;
		break;
	}

	case FIONBIO:
		s = spltty();
		if (*(int *)data)
			tp->t_state |= TS_NBIO;
		else
			tp->t_state &= ~TS_NBIO;
		splx(s);
		break;

	case FIOASYNC:
		s = spltty();
		if (*(int *)data)
			tp->t_state |= TS_ASYNC;
		else
			tp->t_state &= ~TS_ASYNC;
		splx(s);
		break;

	case TIOCGETC:
		bcopy((caddr_t)&tp->t_intrc, data, sizeof (struct tchars));
		break;

	case TIOCSETC:
		bcopy(data, (caddr_t)&tp->t_intrc, sizeof (struct tchars));
		ttysetspec(tp);
		break;

	/* set/get local special characters */
	case TIOCSLTC:
		bcopy(data, (caddr_t)&tp->t_suspc, sizeof (struct ltchars));
		ttysetspec(tp);
		break;

	case TIOCGLTC:
		bcopy((caddr_t)&tp->t_suspc, data, sizeof (struct ltchars));
		break;

	/*
	 * Modify local mode word.
	 */
	case TIOCLBIS:
		tp->t_flags |= *(int *)data << 16;
		break;

	case TIOCLBIC:
		tp->t_flags &= ~(*(int *)data << 16);
		break;

	case TIOCLSET:
		tp->t_flags &= 0xffff;
		tp->t_flags |= *(int *)data << 16;
		break;

	case TIOCLGET:
		*(int *)data = ((unsigned) tp->t_flags) >> 16;
		break;

	/*
	 * Allow SPGRP only if tty is open for reading.
	 * Quick check: if we can find a process in the new pgrp,
	 * this user must own that process.
	 * SHOULD VERIFY THAT PGRP IS IN USE AND IS THIS USER'S.
	 */
	case TIOCSPGRP: {
		struct proc *p;
		int pgrp = *(int *)data;

		if (u.u_uid && (flag & FREAD) == 0)
			return (EPERM);
#if	NeXT
		/*
		 *  This just doesn't work.  When a login csh exits
		 *  it will try to restore the pgrp to that of /etc/init (=1).
		 *  Since init is not inferior to the csh this call fails
		 *  and a subsequent getty will find itself in the wrong
		 *  pgrp causing EIO failures when reading from /etc/tty.
		 */
#else	NeXT
		p = pfind(pgrp);
		if (p && p->p_pgrp == pgrp &&
		    p->p_uid != u.u_uid && u.u_uid && !inferior(p)) {
			return (EPERM);
#endif	NeXT
		tp->t_pgrp = pgrp;
		break;
	}

	case TIOCGPGRP:
		*(int *)data = tp->t_pgrp;
		break;

	case TIOCSCONS:
		/* Set current console device to this line */
		cons_tp = tp;
#if	NeXT
		/*  no longer anyone to display kernel messages */
		if (cons_tp != &cons)
			km.flags &= ~KMF_SEE_MSGS;
#endif	NeXT
		break;

	case TIOCSWINSZ:
		if (bcmp((caddr_t)&tp->t_winsize, data,
		    sizeof (struct winsize))) {
			tp->t_winsize = *(struct winsize *)data;
			gsignal(tp->t_pgrp, SIGWINCH);
		}
		break;

	case TIOCGWINSZ:
		*(struct winsize *)data = tp->t_winsize;
		break;

	default:
		return (-1);
	}
	return (0);
}

ttnread(tp)
	struct tty *tp;
{
	int nread = 0;

	if (tp->t_flags & PENDIN)
		ttypend(tp);
	nread = tp->t_canq.c_cc;
	if (tp->t_flags & (RAW|CBREAK))
		nread += tp->t_rawq.c_cc;
	return (nread);
}

ttselect(tp, rw)
	struct tty *tp;
	int rw;
{
	int nread;
	int s = spltty();

	switch (rw) {

	case FREAD:
		nread = ttnread(tp);
		if ((nread > 0) || ((tp->t_state & TS_CARR_ON) == 0))
			goto win;
		if (tp->t_rsel &&
			((thread_t)(tp->t_rsel))->wait_event
				== (int)&selwait)
			tp->t_state |= TS_RCOLL;
		else
			tp->t_rsel = (struct proc *) current_thread();
		break;

	case FWRITE:
		if (tp->t_outq.c_cc <= TTLOWAT(tp))
			goto win;
		if (tp->t_wsel &&
			((thread_t)(tp->t_wsel))->wait_event
				== (int)&selwait)
			tp->t_state |= TS_WCOLL;
		else
			tp->t_wsel = (struct proc *) current_thread();
		break;
	}
	splx(s);
	return (0);
win:
	splx(s);
	return (1);
}

/*
 * Initial open of tty, or (re)entry to line discipline.
 * Establish a process group for distribution of
 * quits and interrupts from the tty.
 */
ttyopen(dev, tp)
	dev_t dev;
	register struct tty *tp;
{
	register struct proc *pp;
        int oldipl;

	pp = u.u_procp;
	tp->t_dev = dev;
	if (pp->p_pgrp == 0) {
		u.u_ttyp = tp;
		u.u_ttyd = dev;
		if (tp->t_pgrp == 0) {
			tp->t_pgrp = pp->p_pid;
		}
		pp->p_pgrp = tp->t_pgrp;
	}
	oldipl = spltty();
	tp->t_state &= ~TS_WOPEN;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_ISOPEN;
		splx(oldipl);
		bzero((caddr_t)&tp->t_winsize, sizeof(tp->t_winsize));
		if (tp->t_line != NTTYDISC)
			ttywflush(tp);
	} else splx(oldipl);
	return (0);
}

/*
 * "close" a line discipline
 */
ttylclose(tp)
	register struct tty *tp;
{

	ttywflush(tp);
	tp->t_line = 0;
}

/*
 * clean tp on last close
 */
ttyclose(tp)
	register struct tty *tp;
{

#if	NeXT
	if (cons_tp == tp) {
		cons_tp = &cons;

		/* no longer anyone to display kernel messages */
		km.flags &= ~KMF_SEE_MSGS;
	}
#endif	NeXT

	ttyflush(tp, FREAD|FWRITE);
	tp->t_pgrp = 0;
	tp->t_state = 0;
#if	NeXT
	tp->t_line = 0;
	tp->t_ldisc_data = NULL;
#endif	NeXT
}

/*
 * Handle modem control transition on a tty.
 * Flag indicates new state of carrier.
 * Returns 0 if the line should be turned off, otherwise 1.
 */
ttymodem(tp, flag)
	register struct tty *tp;
{

	if ((tp->t_state&TS_WOPEN) == 0 && (tp->t_flags & MDMBUF)) {
		/*
		 * MDMBUF: do flow control according to carrier flag
		 */
		if (flag) {
			tp->t_state &= ~TS_TTSTOP;
			ttstart(tp);
		} else if ((tp->t_state&TS_TTSTOP) == 0) {
			tp->t_state |= TS_TTSTOP;
			(*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
		}
	} else if (flag == 0) {
		/*
		 * Lost carrier.
		 */
		tp->t_state &= ~TS_CARR_ON;
		if (tp->t_state & TS_ISOPEN) {
#ifdef NeXT
			/*
			 * This makes selects work correctly
			 * when carrier goes away (they should return)
			 */
			ttwakeup(tp);
#endif NeXT
			if ((tp->t_flags & NOHANG) == 0) {
				gsignal(tp->t_pgrp, SIGHUP);
				gsignal(tp->t_pgrp, SIGCONT);
				ttyflush(tp, FREAD|FWRITE);
				return (0);
			}
		}
	} else {
		/*
		 * Carrier now on.
		 */
		tp->t_state |= TS_CARR_ON;
		wakeup((caddr_t)&tp->t_rawq);
	}
	return (1);
}

/*
 * Default modem control routine (for other line disciplines).
 * Return argument flag, to turn off device on carrier drop.
 */
nullmodem(tp, flag)
	register struct tty *tp;
	int flag;
{
	
	if (flag)
		tp->t_state |= TS_CARR_ON;
	else
		tp->t_state &= ~TS_CARR_ON;
	return (flag);
}

/*
 * reinput pending characters after state switch
 * call at spltty().
 */
ttypend(tp)
	register struct tty *tp;
{
	struct clist tq;
	register c;

	tp->t_flags &= ~PENDIN;
	tp->t_state |= TS_TYPEN;
	tq = tp->t_rawq;
	tp->t_rawq.c_cc = 0;
	tp->t_rawq.c_cf = tp->t_rawq.c_cl = 0;
	while ((c = getc(&tq)) >= 0)
		ttyinput(c, tp);
	tp->t_state &= ~TS_TYPEN;
}

/*
 * Handle ttyinput flow control
 */
static inline void
ttyflowctl(tp)
struct tty *tp;
{
	/*
	 * Block further input iff:
	 * Current input > threshold AND input is available to user program
	 */
	if (tp->t_rawq.c_cc + tp->t_canq.c_cc >= TTYHOG(tp)/2 && 
	    ((tp->t_flags & (RAW|CBREAK)) || (tp->t_canq.c_cc > 0))) {
		if ((tp->t_flags&TANDEM) && putc(tp->t_stopc, &tp->t_outq) == 0) {
			tp->t_state |= TS_TBLOCK;
			ttstart(tp);
		}
		tp->t_state |= TS_INPUTFULL;
	}
}

/*
 * Place a character on raw TTY input queue,
 * putting in delimiters and waking up top
 * half as needed.  Also echo if required.
 * The arguments are the character and the
 * appropriate tty structure.
 */
ttyinput(c, tp)
	register c;
	register struct tty *tp;
{
	/*
	 * If input is pending take it first.
	 */
	if (tp->t_flags & PENDIN)
		ttypend(tp);
	tk_nin++;
	c &= 0377;

	if (tp->t_flags & RAW) {
		/*
		 * Raw mode, just put character
		 * in input q w/o interpretation.
		 */
		if (tp->t_rawq.c_cc > TTYHOG(tp)) {
#ifndef NeXT
			ttyflush(tp, FREAD|FWRITE);
#endif NeXT
		} else {
			if (putc(c, &tp->t_rawq) >= 0)
				ttwakeup(tp);
			ttyecho(c, tp);
		}
		tp->t_flags &= ~FLUSHO;
		if ((tp->t_flags & DECCTQ) == 0 || tp->t_startc == tp->t_stopc)
		   	tp->t_state &= ~TS_TTSTOP;
	} else
		ttcooked(c, tp);
	ttyflowctl(tp);
	ttstart(tp);
}

ttyblkin(cp, n, tp)
char *cp;
int n;
struct tty *tp;
{
	/*
	 * If input is pending take it first.
	 */
	if (tp->t_flags&PENDIN)
		ttypend(tp);
	tk_nin += n;

	if (tp->t_flags&RAW) {
		/*
		 * Raw mode, just put character
		 * in input q w/o interpretation.
		 */
		if (tp->t_rawq.c_cc+n > TTYHOG(tp)) {
#ifdef NeXT
			n = TTYHOG(tp) - tp->t_rawq.c_cc;
			if (n < 0)
				n = 0;
#else	NeXT
			ttyflush(tp, FREAD|FWRITE);
			n = 0;
#endif NeXT
		}
		if (n - b_to_q(cp, n, &tp->t_rawq) > 0)
			ttwakeup(tp);
		tp->t_flags &= ~FLUSHO;
		if (tp->t_flags & ECHO)
			tk_nout += b_to_q(cp, n, &tp->t_outq);
		/*
		 * If DEC-style start/stop is enabled don't restart
		 * output until seeing the start character.
		 */
		if ((tp->t_flags & DECCTQ) == 0 || tp->t_startc == tp->t_stopc)
			tp->t_state &= ~TS_TTSTOP;
	} else {
		while (n-- > 0)
			ttcooked(*cp++, tp);
	}
	ttyflowctl(tp);
	ttstart(tp);
}

ttcooked(c, tp)
register struct tty *tp;
{
	register int t_flags = tp->t_flags;
	int i;
	int is_special;
	int quote_mask;
	
	/*
	 * Ignore any high bit added during
	 * previous ttyinput processing.
	 */
	if ((tp->t_state&TS_TYPEN) == 0 && (t_flags&PASS8) == 0)
		c &= 0177;

	quote_mask = (t_flags & PASS8) ? 0 : 0200;
	is_special = (t_flags & LCASE) ? 1 : ttyspec(tp, c);
	/*
	 * Check for literal nexting very first
	 */
	if (tp->t_state&TS_LNCH) {
		c |= quote_mask;
		is_special = 0;
		tp->t_state &= ~TS_LNCH;
	}

	/*
	 * Scan for special characters.  This code
	 * is really just a big case statement with
	 * non-constant cases.  The bottom of the
	 * case statement is labeled ``endcase'', so goto
	 * it after a case match, or similar.
	 */
	if (! is_special || (tp->t_state & TS_EXTPROC))
		goto checkcbreak;
	if (tp->t_line == NTTYDISC) {
		if (c == tp->t_lnextc) {
			if (t_flags&ECHO)
				ttyout("^\b", tp);
			tp->t_state |= TS_LNCH;
			goto endcase;
		}
		if (c == tp->t_flushc) {
			if (t_flags&FLUSHO)
				tp->t_flags &= ~FLUSHO;
			else {
				ttyflush(tp, FWRITE);
				ttyecho(c, tp);
				if (tp->t_rawq.c_cc + tp->t_canq.c_cc)
					ttyretype(tp);
				tp->t_flags |= FLUSHO;
			}
			goto startoutput;
		}
		if (c == tp->t_suspc) {
			if ((t_flags&NOFLSH) == 0)
				ttyflush(tp, FREAD);
			ttyecho(c, tp);
			gsignal(tp->t_pgrp, SIGTSTP);
			goto endcase;
		}
	}

	/*
	 * Handle start/stop characters.
	 */
	if (c == tp->t_stopc) {
		if ((tp->t_state&TS_TTSTOP) == 0) {
			tp->t_state |= TS_TTSTOP;
			(*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
			return;
		}
		if (c != tp->t_startc)
			return;
		goto endcase;
	}
	if (c == tp->t_startc)
		goto restartoutput;

	/*
	 * Look for interrupt/quit chars.
	 */
	if (c == tp->t_intrc || c == tp->t_quitc) {
		if ((t_flags&NOFLSH) == 0)
			ttyflush(tp, FREAD|FWRITE);
		ttyecho(c, tp);
		gsignal(tp->t_pgrp, c == tp->t_intrc ? SIGINT : SIGQUIT);
		goto endcase;
	}

	if ((tp->t_flags & LCASE) && c <= 0177) {
		if (tp->t_state&TS_BKSL) {
			ttyrub(unputc(&tp->t_rawq), tp);
			if (maptab[c])
				c = maptab[c];
			c |= quote_mask;
			is_special = 0;
			tp->t_state &= ~(TS_BKSL|TS_QUOT);
		} else if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		else if (c == '\\')
			tp->t_state |= TS_BKSL;
	}

checkcbreak:
	/*
	 * Cbreak mode, don't process line editing
	 * characters; check high water mark for wakeup.
	 */
	if (t_flags&CBREAK) {
		if (tp->t_rawq.c_cc > TTYHOG(tp)) {
			if (tp->t_outq.c_cc < TTHIWAT(tp) &&
			    tp->t_line == NTTYDISC)
				(void) ttyoutput(CTRL('g'), tp);
		} else if (putc(c, &tp->t_rawq) >= 0) {
			ttwakeup(tp);
			ttyecho(c, tp);
		}
		goto endcase;
	}

	/*
	 * From here on down cooked mode character
	 * processing takes place.
	 */
	if (! is_special || (tp->t_state & TS_EXTPROC))
		goto putit;
	if ((tp->t_state&TS_QUOT) &&
	    (c == tp->t_erase || c == tp->t_kill)) {
		ttyrub(unputc(&tp->t_rawq), tp);
		c |= quote_mask;
		goto putit;
	}
	if (c == tp->t_erase) {
		if (tp->t_rawq.c_cc) {
			ttyrub(c = unputc(&tp->t_rawq), tp);
			if ((t_flags & EUCBKSP) && (c & 0200)
			  && tp->t_rawq.c_cc)
				ttyrub(unputc(&tp->t_rawq), tp);
		}
		goto endcase;
	}
	if (c == tp->t_kill) {
		if (t_flags&CRTKIL &&
		    tp->t_rawq.c_cc == tp->t_rocount) {
			while (tp->t_rawq.c_cc)
				ttyrub(unputc(&tp->t_rawq), tp);
		} else {
			ttyecho(c, tp);
			ttyecho('\n', tp);
			while (getc(&tp->t_rawq) > 0)
				;
			tp->t_rocount = 0;
		}
		tp->t_state &= ~TS_LOCAL;
		goto endcase;
	}

	/*
	 * New line discipline,
	 * check word erase/reprint line.
	 */
	if (tp->t_line == NTTYDISC) {
		if (c == tp->t_werasc) {
			if (tp->t_rawq.c_cc == 0)
				goto endcase;
			do {
				c = unputc(&tp->t_rawq);
				if (c != ' ' && c != '\t')
					goto erasenb;
				ttyrub(c, tp);
			} while (tp->t_rawq.c_cc);
			goto endcase;
	erasenb:
			do {
				ttyrub(c, tp);
				if (tp->t_rawq.c_cc == 0)
					goto endcase;
				c = unputc(&tp->t_rawq);
			} while (c != ' ' && c != '\t');
			(void) putc(c, &tp->t_rawq);
			goto endcase;
		}
		if (c == tp->t_rprntc) {
			ttyretype(tp);
			goto endcase;
		}
	}

putit:
	/*
	 * Check for input buffer overflow
	 */
	if (tp->t_rawq.c_cc+tp->t_canq.c_cc >= TTYHOG(tp)) {
		if (tp->t_line == NTTYDISC)
			(void) ttyoutput(CTRL('g'), tp);
		goto endcase;
	}

	/*
	 * Put data char in q for user and
	 * wakeup on seeing a line delimiter.
	 */
	if (putc(c, &tp->t_rawq) >= 0) {
		if (is_special && ttbreakc(c, tp)) {
			tp->t_rocount = 0;
			catq(&tp->t_rawq, &tp->t_canq);
			ttwakeup(tp);
		} else if (tp->t_rocount++ == 0)
			tp->t_rocol = tp->t_col;
		tp->t_state &= ~TS_QUOT;
	    if ((tp->t_state&TS_EXTPROC) == 0) {
		if (c == '\\')
			tp->t_state |= TS_QUOT;
		if (tp->t_state&TS_ERASE) {
			tp->t_state &= ~TS_ERASE;
			(void) ttyoutput('/', tp);
		}
		i = tp->t_col;
		ttyecho(c, tp);
		if (c == tp->t_eofc && t_flags&ECHO) {
			i = MIN(2, tp->t_col - i);
			while (i > 0) {
				(void) ttyoutput('\b', tp);
				i--;
			}
		}
	    }
	}
endcase:
	/*
	 * If DEC-style start/stop is enabled don't restart
	 * output until seeing the start character.
	 */
	if (t_flags&DECCTQ && tp->t_state&TS_TTSTOP &&
	    tp->t_startc != tp->t_stopc)
		return;
restartoutput:
	tp->t_state &= ~TS_TTSTOP;
	tp->t_flags &= ~FLUSHO;
startoutput:
	return;
}

/*
 * Put character on TTY output queue, adding delays,
 * expanding tabs, and handling the CR/NL bit.
 * This is called both from the top half for output,
 * and from interrupt level for echoing.
 * The arguments are the character and the tty structure.
 * Returns < 0 if putc succeeds, otherwise returns char to resend
 * Must be recursive.
 */
ttyoutput(c, tp)
	register c;
	register struct tty *tp;
{
	register char *colp;
	register ctype;

	if (tp->t_flags & (RAW|LITOUT)) {
		if (tp->t_flags&FLUSHO)
			return (-1);
		if (putc(c, &tp->t_outq))
			return (c);
		tk_nout++;
		return (-1);
	}

	/*
	 * Ignore EOT in normal mode to avoid
	 * hanging up certain terminals.
	 */
	c &= (tp->t_flags & PASS8OUT) ? 0377 : 0177;
	if (c == CEOT && (tp->t_flags&CBREAK) == 0)
		return (-1);
	/*
	 * Turn tabs to spaces as required
	 *
	 * Special case if we have external processing, we don't
	 * do the tab expansion because we'll probably get it
	 * wrong.  If tab expansion needs to be done, let it
	 * happen externally.
	 */
	if ((tp->t_state&TS_EXTPROC) == 0 &&
	    c == '\t' && (tp->t_flags&TBDELAY) == XTABS) {
		register int s;

		c = 8 - (tp->t_col&7);
		if ((tp->t_flags&FLUSHO) == 0) {
			s = spltty();		/* don't interrupt tabs */
			c -= b_to_q("        ", c, &tp->t_outq);
			tk_nout += c;
			splx(s);
		}
		tp->t_col += c;
		return (c ? -1 : '\t');
	}
	tk_nout++;
	/*
	 * for upper-case-only terminals,
	 * generate escapes.
	 */
	if (tp->t_flags&LCASE) {
		colp = "({)}!|^~'`";
		while (*colp++)
			if (c == *colp++) {
				if (ttyoutput('\\', tp) >= 0)
					return (c);
				c = colp[-2];
				break;
			}
		if ('A' <= c && c <= 'Z') {
			if (ttyoutput('\\', tp) >= 0)
				return (c);
		} else if ('a' <= c && c <= 'z')
			c += 'A' - 'a';
	}

	/*
	 * turn <nl> to <cr><lf> if desired.
	 */
	if (c == '\n' && tp->t_flags&CRMOD)
		if (ttyoutput('\r', tp) >= 0)
			return (c);
#if	NeXT
	/* NeXT stole the TILDE bit to use as the EUC bit */
#else	NeXT
	if (c == '~' && tp->t_flags&TILDE)
		c = '`';
#endif	NeXT
	if ((tp->t_flags&FLUSHO) == 0 && putc(c, &tp->t_outq))
		return (c);
	/*
	 * Calculate delays.
	 * The numbers here represent clock ticks
	 * and are not necessarily optimal for all terminals.
	 * The delays are indicated by characters above 0200.
	 * In raw mode there are no delays and the
	 * transmission path is 8 bits wide.
	 *
	 * SHOULD JUST ALLOW USER TO SPECIFY DELAYS
	 */
	colp = &tp->t_col;
	ctype = partab[c];
	c = 0;
	switch (ctype&077) {

	case ORDINARY:
		(*colp)++;

	case CONTROL:
		break;

	case BACKSPACE:
		if (*colp)
			(*colp)--;
		break;

	/*
	 * This macro is close enough to the correct thing;
	 * it should be replaced by real user settable delays
	 * in any event...
	 */
#define	mstohz(ms)	(((ms) * hz) >> 10)
	case NEWLINE:
		ctype = (tp->t_flags >> 8) & 03;
		if (ctype == 1) { /* tty 37 */
			if (*colp > 0) {
				c = (((unsigned)*colp) >> 4) + 3;
				if ((unsigned)c > 6)
					c = 6;
			}
		} else if (ctype == 2) /* vt05 */
			c = mstohz(100);
		*colp = 0;
		break;

	case TAB:
		ctype = (tp->t_flags >> 10) & 03;
		if (ctype == 1) { /* tty 37 */
			c = 1 - (*colp | ~07);
			if (c < 5)
				c = 0;
		}
		*colp |= 07;
		(*colp)++;
		break;

	case VTAB:
		if (tp->t_flags&VTDELAY) /* tty 37 */
			c = 0177;
		break;

	case RETURN:
		ctype = (tp->t_flags >> 12) & 03;
		if (ctype == 1) /* tn 300 */
			c = mstohz(83);
		else if (ctype == 2) /* ti 700 */
			c = mstohz(166);
		else if (ctype == 3) { /* concept 100 */
			int i;

			if ((i = *colp) >= 0)
				for (; i < 9; i++)
					(void) putc(0177, &tp->t_outq);
		}
		*colp = 0;
	}
	if (c && (tp->t_flags & (FLUSHO|PASS8OUT)) == 0)
		(void) putc(c|0200, &tp->t_outq);
	return (-1);
}
#undef mstohz

/*
 * Called from device's read routine after it has
 * calculated the tty-structure given as argument.
 */
ttread(tp, uio)
	register struct tty *tp;
	struct uio *uio;
{
	register struct clist *qp;
	register c, t_flags;
	int s, first, error = 0;

loop:
	/*
	 * Take any pending input first.
	 */
	s = spltty();
	if (tp->t_flags&PENDIN)
		ttypend(tp);
	splx(s);

	while ((tp->t_state&TS_CARR_ON)==0)
		if (tp->t_state&TS_ONDELAY) { /* open O_NDELAY */
			if (tp->t_state&TS_NBIO)
				return(EWOULDBLOCK);
			else 
				/* wake up on carrier transition */
				sleep((caddr_t)&tp->t_rawq, TTIPRI);
		}
		else 
			return(EIO);

	/*
	 * Hang process if it's in the background.
	 */
	if (tp == u.u_ttyp && u.u_procp->p_pgrp != tp->t_pgrp) {
		if ((u.u_procp->p_sigignore & sigmask(SIGTTIN)) ||
		   (u.u_procp->p_sigmask & sigmask(SIGTTIN)) ||
		    u.u_procp->p_flag&SVFORK)
			return (EIO);
		gsignal(u.u_procp->p_pgrp, SIGTTIN);
		sleep((caddr_t)&lbolt, TTIPRI);
		goto loop;
	}
	t_flags = tp->t_flags;

	/*
	 * In raw mode take characters directly from the
	 * raw queue w/o processing.  Interlock against
	 * device interrupts when interrogating rawq.
	 */
	if (t_flags&RAW) {
		s = spltty();
		if (tp->t_rawq.c_cc <= 0) {
			if ((tp->t_state&TS_CARR_ON) == 0 ||
			    (tp->t_state&TS_NBIO)) {
				splx(s);
				return (EWOULDBLOCK);
			}
			sleep((caddr_t)&tp->t_rawq, TTIPRI);
			splx(s);
			goto loop;
		}
		splx(s);
 		while (!error && tp->t_rawq.c_cc && uio->uio_resid)
 			error = ureadc(getc(&tp->t_rawq), uio);
		goto checktandem;
	}

	/*
	 * In cbreak mode use the rawq, otherwise
	 * take characters from the canonicalized q.
	 */
	qp = t_flags&CBREAK ? &tp->t_rawq : &tp->t_canq;

	/*
	 * No input, sleep on rawq awaiting hardware
	 * receipt and notification.
	 */
	s = spltty();
	if (qp->c_cc <= 0) {
		if ((tp->t_state&TS_CARR_ON) == 0 ||
		    (tp->t_state&TS_NBIO)) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sleep((caddr_t)&tp->t_rawq, TTIPRI);
		splx(s);
		goto loop;
	}
	splx(s);

	/*
	 * Input present, perform input mapping
	 * and processing (we're not in raw mode).
	 */
	first = 1;
	while ((c = getc(qp)) >= 0) {
		if (t_flags&CRMOD && c == '\r')
			c = '\n';
		/*
		 * Check for delayed suspend character.
		 */
		if (tp->t_line == NTTYDISC && c == tp->t_dsuspc) {
			gsignal(tp->t_pgrp, SIGTSTP);
			if (first) {
				sleep((caddr_t)&lbolt, TTIPRI);
				goto loop;
			}
			break;
		}
		/*
		 * Interpret EOF only in cooked mode.
		 */
		if (c == tp->t_eofc && (t_flags&CBREAK) == 0)
			break;
		/*
		 * Give user character.
		 */
 		error = ureadc(t_flags&PASS8 ? c : c & 0177, uio);
		if (error)
			break;
 		if (uio->uio_resid == 0)
			break;
		/*
		 * In cooked mode check for a "break character"
		 * marking the end of a "line of input".
		 */
		if ((t_flags&CBREAK) == 0 && ttbreakc(c, tp))
			break;
		first = 0;
	}

checktandem:
	/*
	 * Look to unblock input now that (presumably)
	 * the input queue has gone down.
	 */
	if (tp->t_rawq.c_cc < TTYHOG(tp)/5) {
		s = spltty();
		tp->t_state &= ~TS_INPUTFULL;
		splx(s);
		if ((tp->t_state&TS_TBLOCK)
		 && putc(tp->t_startc, &tp->t_outq) == 0) {
			s = spltty();
			tp->t_state &= ~TS_TBLOCK;
			splx(s);
			ttstart(tp);
		}
	}
	return (error);
}

/*
 * Check the output queue on tp for space for a kernel message
 * (from uprintf/tprintf).  Allow some space over the normal
 * hiwater mark so we don't lose messages due to normal flow
 * control, but don't let the tty run amok.
 */
ttycheckoutq(tp, wait)
	register struct tty *tp;
	int wait;
{
	int hiwat, s;

	hiwat = TTHIWAT(tp);
	s = spltty();
	if (tp->t_outq.c_cc > hiwat + 200)
	    while (tp->t_outq.c_cc > hiwat) {
		ttstart(tp);
		if (wait == 0) {
			splx(s);
			return (0);
		}
		tp->t_state |= TS_ASLEEP;
		sleep((caddr_t)&tp->t_outq, TTOPRI);
	}
	splx(s);
	return (1);
}

/*
 * Called from the device's write routine after it has
 * calculated the tty-structure given as argument.
 */
ttwrite(tp, uio)
	register struct tty *tp;
	register struct uio *uio;
{
	register char *cp;
	register int cc, ce, c;
	int i, hiwat, cnt, error, s;
	char obuf[OBUFSIZ];

	hiwat = TTHIWAT(tp);
	cnt = uio->uio_resid;
	error = 0;
loop:
	while ((tp->t_state&TS_CARR_ON)==0)
		if (tp->t_state&TS_ONDELAY) { /* assume O_NDELAY */
			if (tp->t_state&TS_NBIO)
				return(EWOULDBLOCK);
			else 
				/* wake up on carrier transition */
				sleep((caddr_t)&tp->t_rawq, TTIPRI);
		}
		else 
			return(EIO);
	/*
	 * Hang the process if it's in the background.
	 */
	if (u.u_procp->p_pgrp != tp->t_pgrp && tp == u.u_ttyp &&
	    (tp->t_flags&TOSTOP) && (u.u_procp->p_flag&SVFORK)==0 &&
	    !(u.u_procp->p_sigignore & sigmask(SIGTTOU)) &&
	    !(u.u_procp->p_sigmask & sigmask(SIGTTOU))) {
		gsignal(u.u_procp->p_pgrp, SIGTTOU);
		sleep((caddr_t)&lbolt, TTIPRI);
		goto loop;
	}

	/*
	 * Process the user's data in at most OBUFSIZ
	 * chunks.  Perform lower case simulation and
	 * similar hacks.  Keep track of high water
	 * mark, sleep on overflow awaiting device aid
	 * in acquiring new space.
	 */
	while (uio->uio_resid > 0) {
		/*
		 * Grab a hunk of data from the user.
		 */
		cc = uio->uio_iov->iov_len;
		if (cc == 0) {
			uio->uio_iovcnt--;
			uio->uio_iov++;
			if (uio->uio_iovcnt <= 0)
				panic("ttwrite");
			continue;
		}
		if (cc > OBUFSIZ)
			cc = OBUFSIZ;
		cp = obuf;
		error = uiomove(cp, cc, UIO_WRITE, uio);
		if (error)
			break;
		if (tp->t_outq.c_cc > hiwat)
			goto ovhiwat;
		if (tp->t_flags&FLUSHO)
			continue;
		/*
		 * If we're mapping lower case or kludging tildes,
		 * then we've got to look at each character, so
		 * just feed the stuff to ttyoutput...
		 */
#if	NeXT
		/* NeXT stole the TILDE bit to use as the EUC bit */
		if (tp->t_flags & LCASE)
#else	NeXT
		if (tp->t_flags & (LCASE|TILDE))
#endif	NeXT
		{
			while (cc > 0) {
				c = *cp++;
				tp->t_rocount = 0;
				while ((c = ttyoutput(c, tp)) >= 0) {
					/* out of clists, wait a bit */
					ttstart(tp);
					sleep((caddr_t)&lbolt, TTOPRI);
					tp->t_rocount = 0;
					if (cc != 0) {
						uio->uio_iov->iov_base -= cc;
						uio->uio_iov->iov_len += cc;
						uio->uio_resid += cc;
						uio->uio_offset -= cc;
					}
					goto loop;
				}
				--cc;
				if (tp->t_outq.c_cc > hiwat)
					goto ovhiwat;
			}
			continue;
		}
		/*
		 * If nothing fancy need be done, grab those characters we
		 * can handle without any of ttyoutput's processing and
		 * just transfer them to the output q.  For those chars
		 * which require special processing (as indicated by the
		 * bits in partab), call ttyoutput.  After processing
		 * a hunk of data, look for FLUSHO so ^O's will take effect
		 * immediately.
		 */
		while (cc > 0) {
			if (tp->t_flags & (RAW|LITOUT))
				ce = cc;
			else {
				ce = cc - scanc((unsigned)cc, (caddr_t)cp,
				   (caddr_t)partab, 077);
				/*
				 * If ce is zero, then we're processing
				 * a special character through ttyoutput.
				 */
				if (ce == 0) {
					tp->t_rocount = 0;
					if (ttyoutput(*cp, tp) >= 0) {
					    /* no c-lists, wait a bit */
					    ttstart(tp);
					    sleep((caddr_t)&lbolt, TTOPRI);
					    if (cc != 0) {
					        uio->uio_iov->iov_base -= cc;
					        uio->uio_iov->iov_len += cc;
					        uio->uio_resid += cc;
						uio->uio_offset -= cc;
					    }
					    goto loop;
					}
					cp++, cc--;
					if (tp->t_flags&FLUSHO ||
					    tp->t_outq.c_cc > hiwat)
						goto ovhiwat;
					continue;
				}
			}
			/*
			 * A bunch of normal characters have been found,
			 * transfer them en masse to the output queue and
			 * continue processing at the top of the loop.
			 * If there are any further characters in this
			 * <= OBUFSIZ chunk, the first should be a character
			 * requiring special handling by ttyoutput.
			 */
			tp->t_rocount = 0;
			i = b_to_q(cp, ce, &tp->t_outq);
			ce -= i;
			tp->t_col += ce;
			cp += ce, cc -= ce, tk_nout += ce;
			if (i > 0) {
				/* out of c-lists, wait a bit */
				ttstart(tp);
				sleep((caddr_t)&lbolt, TTOPRI);
				uio->uio_iov->iov_base -= cc;
				uio->uio_iov->iov_len += cc;
				uio->uio_resid += cc;
				uio->uio_offset -= cc;
				goto loop;
			}
			if (tp->t_flags&FLUSHO || tp->t_outq.c_cc > hiwat)
				goto ovhiwat;
		}
	}
	ttstart(tp);
	return (error);

ovhiwat:
	s = spltty();
	if (cc != 0) {
		uio->uio_iov->iov_base -= cc;
		uio->uio_iov->iov_len += cc;
		uio->uio_resid += cc;
		uio->uio_offset -= cc;
	}
	/*
	 * This can only occur if FLUSHO
	 * is also set in t_flags.
	 */
	ttstart(tp);
	if (tp->t_outq.c_cc <= hiwat) {
		splx(s);
		goto loop;
	}
	if (tp->t_state&TS_NBIO) {
		splx(s);
		if (uio->uio_resid == cnt)
			return (EWOULDBLOCK);
		return (0);
	}
	tp->t_state |= TS_ASLEEP;
	sleep((caddr_t)&tp->t_outq, TTOPRI);
	splx(s);
	goto loop;
}

/*
 * Rubout one character from the rawq of tp
 * as cleanly as possible.
 */
ttyrub(c, tp)
	register c;
	register struct tty *tp;
{
	register char *cp;
	register int savecol;
	int s;
	char *nextc();

	if ((tp->t_flags&ECHO) == 0 || (tp->t_state&TS_EXTPROC))
		return;
	tp->t_flags &= ~FLUSHO;
	c &= 0377;
	if (tp->t_flags&CRTBS) {
		if (tp->t_rocount == 0) {
			/*
			 * Screwed by ttwrite; retype
			 */
			ttyretype(tp);
			return;
		}
		if (!(tp->t_flags & PASS8) && (c == ('\t'|0200) || c == ('\n'|0200)))
			ttyrubo(tp, 2);
		else switch (partab[c&=0177]&0177) {

		case ORDINARY:
			if (tp->t_flags&LCASE && c >= 'A' && c <= 'Z')
				ttyrubo(tp, 2);
			else
				ttyrubo(tp, 1);
			break;

		case VTAB:
		case BACKSPACE:
		case CONTROL:
		case RETURN:
			if (tp->t_flags&CTLECH)
				ttyrubo(tp, 2);
			break;

		case TAB:
			if (tp->t_rocount < tp->t_rawq.c_cc) {
				ttyretype(tp);
				return;
			}
			s = spltty();
			savecol = tp->t_col;
			tp->t_state |= TS_CNTTB;
			tp->t_flags |= FLUSHO;
			tp->t_col = tp->t_rocol;
			cp = tp->t_rawq.c_cf;
			for (; cp; cp = nextc(&tp->t_rawq, cp))
				ttyecho(*cp, tp);
			tp->t_flags &= ~FLUSHO;
			tp->t_state &= ~TS_CNTTB;
			splx(s);
			/*
			 * savecol will now be length of the tab
			 */
			savecol -= tp->t_col;
			tp->t_col += savecol;
			if (savecol > 8)
				savecol = 8;		/* overflow screw */
			while (--savecol >= 0)
				(void) ttyoutput('\b', tp);
			break;

		default:
			panic("ttyrub");
		}
	} else if (tp->t_flags&PRTERA) {
		if ((tp->t_state&TS_ERASE) == 0) {
			(void) ttyoutput('\\', tp);
			tp->t_state |= TS_ERASE;
		}
		ttyecho(c, tp);
	} else
		ttyecho(tp->t_erase, tp);
	tp->t_rocount--;
}

/*
 * Crt back over cnt chars perhaps
 * erasing them.
 */
ttyrubo(tp, cnt)
	register struct tty *tp;
	int cnt;
{
	register char *rubostring = tp->t_flags&CRTERA ? "\b \b" : "\b";

	while (--cnt >= 0)
		ttyout(rubostring, tp);
}

/*
 * Reprint the rawq line.
 * We assume c_cc has already been checked.
 */
ttyretype(tp)
	register struct tty *tp;
{
	register char *cp;
	char *nextc();
	int s;

	if (tp->t_rprntc != (char)0377)
		ttyecho(tp->t_rprntc, tp);
	(void) ttyoutput('\n', tp);
	s = spltty();
	for (cp = tp->t_canq.c_cf; cp; cp = nextc(&tp->t_canq, cp))
		ttyecho(*cp, tp);
	for (cp = tp->t_rawq.c_cf; cp; cp = nextc(&tp->t_rawq, cp))
		ttyecho(*cp, tp);
	tp->t_state &= ~TS_ERASE;
	splx(s);
	tp->t_rocount = tp->t_rawq.c_cc;
	tp->t_rocol = 0;
}

/*
 * Echo a typed character to the terminal
 */
ttyecho(c, tp)
	register c;
	register struct tty *tp;
{
	if ((tp->t_state&TS_CNTTB) == 0)
		tp->t_flags &= ~FLUSHO;
	if ((tp->t_flags&ECHO) == 0 || (tp->t_state&TS_EXTPROC))
		return;
	if (tp->t_flags&RAW) {
		(void) ttyoutput(c, tp);
		return;
	}
	c &= (tp->t_flags & PASS8) ? 0377 : 0177;
	if (c == '\r' && tp->t_flags&CRMOD)
		c = '\n';
	if (tp->t_flags&CTLECH) {
		if (c <= 037 && c!='\t' && c!='\n' || c == 0177) {
			(void) ttyoutput('^', tp);
			if (c == 0177)
				c = '?';
			else if (tp->t_flags&LCASE)
				c += 'a' - 1;
			else
				c += 'A' - 1;
		}
	}
	/*
	 * Do not echo non-printing control characters.  Mainly
	 * to stop causing special actions on most terminals.
	 */
	if ((040 <= c && ((tp->t_flags & PASS8) || c <= 0176))
	  || (07 <= c && c <= 012) || c == 015)
	    (void) ttyoutput(c, tp);
}

/*
 * Is c a break char for tp?
 */
ttbreakc(c, tp)
	register c;
	register struct tty *tp;
{
	return (c == '\n' || c == tp->t_eofc || c == tp->t_brkc ||
		c == '\r' && (tp->t_flags&CRMOD));
}

/*
 * send string cp to tp
 */
ttyout(cp, tp)
	register char *cp;
	register struct tty *tp;
{
	register char c;

	while (c = *cp++)
		(void) ttyoutput(c, tp);
}

ttwakeup(tp)
	struct tty *tp;
{

	if (tp->t_rsel) {
		selwakeup(tp->t_rsel, tp->t_state&TS_RCOLL);
		tp->t_state &= ~TS_RCOLL;
		tp->t_rsel = 0;
	}
	if (tp->t_state & TS_ASYNC)
		gsignal(tp->t_pgrp, SIGIO); 
	wakeup((caddr_t)&tp->t_rawq);
}

#if	NeXT

int
tty_ld_install(int ld_number,
	int ld_kind,
	int (*ld_open)(dev_t dev, struct tty *tp),
	int (*ld_close)(struct tty *tp),
	int (*ld_read)(struct tty *tp, struct uio *uiop),
	int (*ld_write)(struct tty *tp, struct uio *uiop),
	int (*ld_ioctl)(struct tty *tp, int command, void *dataptr, int flag),
	int (*ld_rint)(int c, struct tty *tp),
	int (*ld_rend)(char *cp, int n, struct tty *tp),
	int (*ld_start)(struct tty *tp),
	int (*ld_modem)(struct tty *tp, int dcd_on),
	int (*ld_select)(struct tty *tp, int rw)
) {
	extern nodev();
	extern int nldisp;
	int s;

	if (ld_number < 0 || ld_number >= nldisp)
		return -1;
	if (linesw[ld_number].l_open != nodev 
	 || linesw[ld_number].l_close != nodev
	 || linesw[ld_number].l_read != nodev
	 || linesw[ld_number].l_write != nodev
	 || linesw[ld_number].l_ioctl != nodev
	 || linesw[ld_number].l_rint != nodev
	 || linesw[ld_number].l_rend != nodev
	 || linesw[ld_number].l_start != nodev
	 || linesw[ld_number].l_modem != nodev
	 || linesw[ld_number].l_select != nodev)
		return -1;

	s = spltty();
	linesw[ld_number].l_kind = ld_kind;
	linesw[ld_number].l_open = (int (*)())ld_open;
	linesw[ld_number].l_close = (int (*)())ld_close;
	linesw[ld_number].l_read = (int (*)())ld_read;
	linesw[ld_number].l_write = (int (*)())ld_write;
	linesw[ld_number].l_ioctl = (int (*)())ld_ioctl;
	linesw[ld_number].l_rint = (int (*)())ld_rint;
	linesw[ld_number].l_rend = (int (*)())ld_rend;
	linesw[ld_number].l_start = (int (*)())ld_start;
	linesw[ld_number].l_modem = (int (*)())ld_modem;
	linesw[ld_number].l_select = (int (*)())ld_select;
	splx(s);
	return 0;
}

void tty_ld_remove(int ld_number)
{
	int s;
	extern nodev();
	extern int nldisp;
	
	if (ld_number < 0 || ld_number >= nldisp)
		return;

	s = spltty();
	linesw[ld_number].l_open = nodev;
	linesw[ld_number].l_close = nodev;
	linesw[ld_number].l_read = nodev;
	linesw[ld_number].l_write = nodev;
	linesw[ld_number].l_ioctl = nodev;
	linesw[ld_number].l_rint = nodev;
	linesw[ld_number].l_rend = nodev;
	linesw[ld_number].l_start = nodev;
	linesw[ld_number].l_modem = nodev;
	linesw[ld_number].l_select = nodev;
	splx(s);
}

void
ttydevstart(struct tty *tp)
{
	if (tp->t_oproc)
		(*tp->t_oproc)(tp);
}

void
ttydevstop(struct tty *tp)
{
	(*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
}

void
ttyselwait(struct tty *tp, int rw)
{
	int s = spltty();

	switch (rw) {

	case FREAD:
		if (tp->t_rsel &&
			((thread_t)(tp->t_rsel))->wait_event
				== (int)&selwait)
			tp->t_state |= TS_RCOLL;
		else
			tp->t_rsel = (struct proc *) current_thread();
		break;

	case FWRITE:
		if (tp->t_wsel &&
			((thread_t)(tp->t_wsel))->wait_event
				== (int)&selwait)
			tp->t_state |= TS_WCOLL;
		else
			tp->t_wsel = (struct proc *) current_thread();
		break;
	}
	splx(s);
}

void
ttselwakeup(struct tty *tp)
{
	int s = spltty();

	if (tp->t_rsel) {
		selwakeup(tp->t_rsel, tp->t_state&TS_RCOLL);
		tp->t_state &= ~TS_RCOLL;
		tp->t_rsel = 0;
	}
	splx(s);
}

#endif	NeXT
