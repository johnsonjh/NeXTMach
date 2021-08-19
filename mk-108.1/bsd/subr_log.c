/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 * 26-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: remove dir.h
 *
 *  1-Aug-88  John Seamons (jks) at NeXT
 *	NeXT: logwakeup() can't call wakeup() directly because it might
 *	be called from an interrupt level higher that the splsched() in
 *	thread_wakeup(), violating the splsched() and causing the
 *	interrupt level to be incorrectly lowered.  Use a software
 *	interrupt instead.
 *
 * 16-Jun-88  John Seamons (jks) at NeXT
 *	MACH: made logsoftc.sc_selp be a pointer to a thread.
 */ 
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)subr_log.c	7.1 (Berkeley) 6/5/86
 */

/*
 * Error log buffer for kernel printf's.
 */

#import <sys/param.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/ioctl.h>
#import <sys/msgbuf.h>
#import <sys/file.h>
#import <sys/errno.h>
#import <sys/uio.h>

#import <machine/spl.h>

struct msgbuf *pmsgbuf;

#define LOG_RDPRI	(PZERO + 1)

#define LOG_NBIO	0x02
#define LOG_ASYNC	0x04
#define LOG_RDWAIT	0x08

struct logsoftc {
	int	sc_state;		/* see above for possibilities */
	struct	proc *sc_selp;		/* process waiting on select call */
	int	sc_pgrp;		/* process group for async I/O */
} logsoftc;

int	log_open;			/* also used in log() */

/*ARGSUSED*/
logopen(dev)
	dev_t dev;
{

	if (log_open)
		return (EBUSY);
	log_open = 1;
	logsoftc.sc_selp = 0;
	logsoftc.sc_pgrp = u.u_procp->p_pgrp;
	/*
	 * Potential race here with putchar() but since putchar should be
	 * called by autoconf, msg_magic should be initialized by the time
	 * we get here.
	 */
#if	mips || NeXT
	if (pmsgbuf->msg_magic != MSG_MAGIC) {
		register int i;

		pmsgbuf->msg_magic = MSG_MAGIC;
		pmsgbuf->msg_bufx = pmsgbuf->msg_bufr = 0;
		for (i=0; i < MSG_BSIZE; i++)
			pmsgbuf->msg_bufc[i] = 0;
	}
#else	mips || NeXT
	if (msgbuf.msg_magic != MSG_MAGIC) {
		register int i;

		msgbuf.msg_magic = MSG_MAGIC;
		msgbuf.msg_bufx = msgbuf.msg_bufr = 0;
		for (i=0; i < MSG_BSIZE; i++)
			msgbuf.msg_bufc[i] = 0;
	}
#endif	mips || NeXT
	return (0);
}

/*ARGSUSED*/
logclose(dev, flag)
	dev_t dev;
{
	log_open = 0;
	logsoftc.sc_state = 0;
	logsoftc.sc_selp = 0;
	logsoftc.sc_pgrp = 0;
}

/*ARGSUSED*/
logread(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register long l;
	register int s;
	int error = 0;

	s = splhigh();
#if	mips || NeXT
	while (pmsgbuf->msg_bufr == pmsgbuf->msg_bufx) {
		if (logsoftc.sc_state & LOG_NBIO) {
			splx(s);
			return (EWOULDBLOCK);
		}
		logsoftc.sc_state |= LOG_RDWAIT;
		sleep((caddr_t)pmsgbuf, LOG_RDPRI);
	}
#else	mips || NeXT
	while (msgbuf.msg_bufr == msgbuf.msg_bufx) {
		if (logsoftc.sc_state & LOG_NBIO) {
			splx(s);
			return (EWOULDBLOCK);
		}
		logsoftc.sc_state |= LOG_RDWAIT;
		sleep((caddr_t)&msgbuf, LOG_RDPRI);
	}
#endif	mips || NeXT
	splx(s);
	logsoftc.sc_state &= ~LOG_RDWAIT;

#if	mips || NeXT
	while (uio->uio_resid > 0) {
		l = pmsgbuf->msg_bufx - pmsgbuf->msg_bufr;
		if (l < 0)
			l = MSG_BSIZE - pmsgbuf->msg_bufr;
		l = MIN(l, uio->uio_resid);
		if (l == 0)
			break;
		error = uiomove((caddr_t)&pmsgbuf->msg_bufc[pmsgbuf->msg_bufr],
			(int)l, UIO_READ, uio);
		if (error)
			break;
		pmsgbuf->msg_bufr += l;
		if (pmsgbuf->msg_bufr < 0 || pmsgbuf->msg_bufr >= MSG_BSIZE)
			pmsgbuf->msg_bufr = 0;
	}
#else	mips || NeXT
	while (uio->uio_resid > 0) {
		l = msgbuf.msg_bufx - msgbuf.msg_bufr;
		if (l < 0)
			l = MSG_BSIZE - msgbuf.msg_bufr;
		l = MIN(l, uio->uio_resid);
		if (l == 0)
			break;
		error = uiomove((caddr_t)&msgbuf.msg_bufc[msgbuf.msg_bufr],
			(int)l, UIO_READ, uio);
		if (error)
			break;
		msgbuf.msg_bufr += l;
		if (msgbuf.msg_bufr < 0 || msgbuf.msg_bufr >= MSG_BSIZE)
			msgbuf.msg_bufr = 0;
	}
#endif	mips || NeXT
	return (error);
}

/*ARGSUSED*/
logselect(dev, rw)
	dev_t dev;
	int rw;
{
	int s = splhigh();

	switch (rw) {

	case FREAD:
#if	mips || NeXT
		if (pmsgbuf->msg_bufr != pmsgbuf->msg_bufx) {
			splx(s);
			return (1);
		}
#else	mips || NeXT
		if (msgbuf.msg_bufr != msgbuf.msg_bufx) {
			splx(s);
			return (1);
		}
#endif	mips || NeXT
		logsoftc.sc_selp = (struct proc*) current_thread();
		break;
	}
	splx(s);
	return (0);
}

#if	NeXT
logwakeup()
{
	int log_wakeup();
	extern int softint_free;

	if (softint_free)
		softint_sched (1, log_wakeup, 0);
}

log_wakeup()
#else	NeXT
logwakeup()
#endif	NeXT
{

	if (!log_open)
		return;
	if (logsoftc.sc_selp) {
		selwakeup(logsoftc.sc_selp, 0);
		logsoftc.sc_selp = 0;
	}
	if (logsoftc.sc_state & LOG_ASYNC)
		gsignal(logsoftc.sc_pgrp, SIGIO); 
	if (logsoftc.sc_state & LOG_RDWAIT) {
#if	mips || NeXT
		wakeup((caddr_t)pmsgbuf);
#else	mips || NeXT
		wakeup((caddr_t)&msgbuf);
#endif	mips || NeXT
		logsoftc.sc_state &= ~LOG_RDWAIT;
	}
}

/*ARGSUSED*/
logioctl(com, data, flag)
	caddr_t data;
{
	long l;
	int s;

	switch (com) {

	/* return number of characters immediately available */
	case FIONREAD:
		s = splhigh();
#if	mips || NeXT
		l = pmsgbuf->msg_bufx - pmsgbuf->msg_bufr;
#else	mips || NeXT
		l = msgbuf.msg_bufx - msgbuf.msg_bufr;
#endif	mips || NeXT
		splx(s);
		if (l < 0)
			l += MSG_BSIZE;
		*(off_t *)data = l;
		break;

	case FIONBIO:
		if (*(int *)data)
			logsoftc.sc_state |= LOG_NBIO;
		else
			logsoftc.sc_state &= ~LOG_NBIO;
		break;

	case FIOASYNC:
		if (*(int *)data)
			logsoftc.sc_state |= LOG_ASYNC;
		else
			logsoftc.sc_state &= ~LOG_ASYNC;
		break;

	case TIOCSPGRP:
		logsoftc.sc_pgrp = *(int *)data;
		break;

	case TIOCGPGRP:
		*(int *)data = logsoftc.sc_pgrp;
		break;

	default:
		return (-1);
	}
	return (0);
}



