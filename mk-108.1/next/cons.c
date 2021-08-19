/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

/*
 * Indirect driver for console.
 */
#import <sys/param.h>
#import <sys/systm.h>
#import <sys/conf.h>
#import <sys/user.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/proc.h>
#import <sys/uio.h>
#import <next/cons.h>

/*ARGSUSED*/
cnopen(dev, flag)
	dev_t dev;
	int flag;
{
	dev_t device;
	register struct proc *pp;

	device = cons_tp->t_dev;
	
	/*
	 *  Must setup pgrp/controlling tty here because we cannot pass
	 *  "dev" as the first argument to the open routine (must be
	 *  "device" otherwise we loop in ttyopen).  We want the
	 *  u.u_ttyd to be set to the indirect "dev" and not the dereferenced
	 *  "device".
	 */
	pp = u.u_procp;
	if (pp->p_pgrp == 0) {
		u.u_ttyp = cons_tp;
		u.u_ttyd = dev;
		if (cons_tp->t_pgrp == 0) {
			cons_tp->t_pgrp = pp->p_pid;
		}
		pp->p_pgrp = cons_tp->t_pgrp;
	}
	return ((*cdevsw[major(device)].d_open)(device, flag));
}

/*ARGSUSED*/
cnread(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	dev_t device;

	device = cons_tp->t_dev;
	return ((*cdevsw[major(device)].d_read)(device, uio));
}

/*ARGSUSED*/
cnwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	dev_t device;

	device = cons_tp->t_dev;
	return ((*cdevsw[major(device)].d_write)(device, uio));
}

/*ARGSUSED*/
cnioctl(dev, cmd, addr, flag)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
{
	dev_t device;

	if (cmd == TIOCNOTTY) {
		cons_tp = &cons;
		return (0);
	}
	device = cons_tp->t_dev;
	return ((*cdevsw[major(device)].d_ioctl)(device, cmd, addr, flag));
}

/*ARGSUSED*/
cnselect(dev, flag)
	dev_t dev;
	int flag;
{
	dev_t device;

	device = cons_tp->t_dev;
	return ((*cdevsw[major(device)].d_select)(device, flag));
}

cngetc()
{
	dev_t device;

	device = cons_tp->t_dev;
	return ((*cdevsw[major(device)].d_getc)(device));
}

/*ARGSUSED*/
cnputc(c)
	char c;
{
	dev_t device;

	device = cons_tp->t_dev;
	return ((*cdevsw[major(device)].d_putc)(device, c));
}

#if	NCPUS > 1
slave_cnenable()
{
	/* FIXME: what to do here? */
}
#endif	NCPUS > 1
