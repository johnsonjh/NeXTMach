/* 
 * Copyright (c) 1987, 1988, 1989 NeXT, Inc.
 */
/*
 *
 * HISTORY
 * 24-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Change kill_tasks to deal with processor_set cleanup properly.
 *
 * 9-July-90  Mike Paquette (mpaque) at NeXT
 *	Removed hardware bit fiddling from softint code, replaced with video calls.
 *
 *  4-Jun-90  Brian Pinkerton at NeXT
 *	Removed call to vm_object_shutdown on reboot/halt.  This is now handled in
 *	unmount_all() in proper sequence with the rest of the shutdown.
 *
 * 4-April-90  Mike Paquette (mpaque) at NeXT
 *	Added call to km_switch_to_vm() to startup, so console I/O will work.
 *
 * 19-Mar-90  Gregg Kellogg (gk) at NeXT
 *	defined boothowto here.  It's externed in the header file now.
 *
 * 15-Mar-90  John Seamons (jks) at NeXT
 *	Removed obsolete call to stoprtclock().
 *	Moved rtc_power_down() to next/clock.c.
 *
 * 06-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Changed kill_tasks to get tasks from processor sets, rather
 *	than all_tasks.
 *	Increased size of boot_param string from 32 to 128 bytes.
 *	Took out simple_unlock in softint_th, as this is done by
 *	thread_sleep().
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Added argument to boot() that optionally specifies the
 *	command to reboot with.
 *
 * Revision 2.19  89/10/11  14:56:08  dlb
 * 	Encase multiprocessor shutdown code in NCPUS > 1.
 * 	[89/09/06            dlb]
 * 	Replace should_exit with processor shutdown logic.
 * 	New calling sequence for vm_map_pageable.
 * 	[89/02/02            dlb]
 * 
 * 29-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes
 *
 * 15-Jun-89  Mike DeMoney (mike) at NeXT
 *	Moved microtime() to us_timer.c
 *
 * 16-Feb-88  John Seamons (jks) at NeXT
 *	Updated to Mach release 2.
 *
 *  4-Jan-88  Gregg Kellogg (gk) at NeXT
 *	softints may generate calls immediately with level == ..._NOW
 *
 * 22-Dec-87  Gregg Kellogg (gk) at NeXT
 *	Microtime() now get's it's microsecond time base from the
 *	microsecond counter.
 *
 *  5-Dec-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Condensed buffer size calculations.  Use one buffer per page,
 *	since pages are virtual-size.
 *
 * 26-Oct-87  Peter King (king) at NeXT
 *	SUN_VFS: Allocate space for dnlc cache instead of namei cache.
 *
 * 14-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added call to panic_init(), as directed by David Black.
 *
 *  4-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	CS_BUGFIX: Correct calculation of second-boundary rollover
 *	in microtime().
 *
 * 28-Aug-87  David Golub (dbg) at Carnegie-Mellon University
 *	Remove arg_zone - exec now uses kernel_pageable_map (allocated
 *	in init_main).
 *
 * 23-Dec-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 **********************************************************************
 */ 

#import <cpus.h>
#import <confdep.h>
#import <cputypes.h>
#import <quota.h>
#import <show_space.h>
#import <gdb.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)machdep.c	7.1 (Berkeley) 6/5/86
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/dnlc.h>
#import <sys/vm.h>
#import <sys/proc.h>
#import <sys/buf.h>
#import <sys/reboot.h>
#import <sys/conf.h>
#import <sys/vnode.h>
#import <ufs/inode.h>
#import <sys/file.h>
#import <sys/clist.h>
#import <sys/callout.h>
#import <sys/mbuf.h>
#import <sys/msgbuf.h>
#import <sys/ioctl.h>
#import <sys/signal.h>
#import <sys/tty.h>
#import <kern/task.h>
#if	QUOTA
#import <ufs/quotas.h>
#endif	QUOTA
#if	NCPUS > 1
#import <kern/processor.h>
#import <kern/thread.h>
#import <kern/lock.h>
#endif	NCPUS > 1
#import <kern/xpr.h>
#import <vm/vm_kern.h>
#import <vm/vm_param.h>
#import <vm/vm_page.h>
#import <next/vmparam.h>
#import <next/autoconf.h>
#import <next/cpu.h>
#import <next/reg.h>
#import <next/psl.h>
#import <next/pcb.h>
#import <next/scr.h>
#import <next/scb.h>
#import <next/cons.h>
#import <next/event_meter.h>
#import <next/us_timer.h>
#import	<next/bmap.h>
#import <nextdev/monreg.h>
#import <nextdev/kmreg.h>
#import <nextdev/video.h>
#import <nextdev/dma.h>
#import <mon/global.h>
#import <nextdev/snd_dspreg.h>

#import <machine/spl.h>

int	boothowto;
vm_offset_t	usrpt, eusrpt;
int	astune_rate, astune_calls;

/*
 * Declare these as initialized data so we can patch them.
 */
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
#ifdef	NRNODE
int	nrnode = NRNODE;
#else
int	nrnode = 0;
#endif
#if	SHOW_SPACE
int	show_space;
#endif	SHOW_SPACE

vm_map_t	buffer_map;
char *boot_action, boot_param[128];

mon_boot (args)
	char *args;
{
	extern char boot_dev[], boot_info[], boot_file[], *boot_args;

	strcat (boot_param, boot_dev);
	strcat (boot_param, " ");
	if (boot_info[0]) {
		strcat (boot_param, boot_info);
		strcat (boot_param, " ");
	}
	strcat (boot_param, boot_file);
	strcat (boot_param, " ");
	if (args) {
		strcat (boot_param, args);
		strcat (boot_param, " ");
	}
	strcat (boot_param, boot_args);
	mon_call (boot_param);
}

mon_call (action)
	char *action;
{
	boot_action = action;
	asm ("movl _boot_action, d0");
	asm ("trap #13");
}

int reboot_how;

halt_thread() {
	boot (RB_BOOT, reboot_how);
}

reboot_mach (how)
{
	if (kernel_task) {
		reboot_how = how;
		kernel_thread_noblock(kernel_task, halt_thread);
	} else
		boot(RB_BOOT, how|RB_NOSYNC);
}

/* nmi mini-monitor */
int	nmi_cont, nmi_gdb, nmi_mon, nmi_help, nmi_halt, nmi_msg, nmi_stay,
	nmi_reboot, nmi_big;
#if	XPR_DEBUG
int	nmi_xpr, nmi_xprclear;
#endif	XPR_DEBUG

nmi(ksp, usp, isp, pc, sr)
{
	register struct monitor *mon = (struct monitor*) P_MON;
	union kybd_event ke;

	ke.data = mon->mon_km_data;
	mon->mon_csr.km_clr_nmi = 1;
	if (ke.k.up_down == 1)		/* ignore up transitions */
		return;
	if (ke.k.command_left && ke.k.command_right)
		mini_mon("nmi", "NMI Mini-Monitor (type \"?\" for help)",
			ksp, usp, isp, pc, sr);
	else
	
	/* only allow command-right to cause restart request */
	if (ke.k.command_left == 0)
		mini_mon("restart", "Restart/Power-Off");
}

mini_mon(prompt, title, ksp, usp, isp, pc, sr)
	char *prompt, *title;
{
	register int i, saved = 0, s = splhigh(), restart, c, panic;
	char line[256];
	extern int *intrstat;
	extern struct mon_global *mon_global;
	struct mon_global *mg = mon_global;

	restart = strcmp(prompt, "restart") == 0;
	panic = strcmp(prompt, "panic") == 0;
	if (cons_tp != &cons || (km.flags & KMF_SEE_MSGS) == 0) {
		saved = 1;
		if (restart) {
			kmpopup(title, POPUP_ALERT, 50, 6, 0);
			restart = 1;
		} else
			kmpopup(title, POPUP_NOM, 0, 0, 0);
		if (strcmp(prompt, "panic") == 0) {
			kmioctl(0, KMIOCDUMPLOG, 0, 0);
			if ((ksp & RB_DEBUG) == 0)
				return;
		}
	} else
		nmi_prf("\n");
	if (restart) {
		nmi_prf("Restart or power off?  Type r to restart,\n");
		nmi_prf("press Power key to power off.  Type n to cancel.\n");
		while (1) {
			c = kmtrygetc();
			if (c == 'r')
				reboot_mach(RB_AUTOBOOT);
			if (c == 'h')
				reboot_mach(RB_HALT);
			if (c != -1)
				break;
			if ((*intrstat & I_BIT(I_POWER)) && rtc_intr()) {
				vidStopAnimation();	/* Signal the ROM */
				vidSuspendAnimation();	/* Mask retrace interrupts */
				reboot_mach(RB_POWERDOWN | RB_EJECT);
				break;
			}
		}
		goto out;
	}
	if (strcmp(prompt, "nmi") == 0)
		nmi_prf("nmi: pc 0x%x sr 0x%x isp 0x%x ksp 0x%x "
			"usp 0x%x\n", pc, sr, isp, ksp, usp);

	while (1) {
		nmi_prf("%s> ", prompt);
		
		/* check for network debugger connection */
		do {
			c = kmtrygetc();
#if	GDB
			if (kdbg_connect())
				goto cont;
#endif	GDB
		} while (c == -1);
		line[0] = c;
		if (c == '\n' || c == '\r')
			nmi_prf("\n");
		else {
			nmi_prf("%c", c);
			gets(line, &line[1]);
		}
		if (getargs(line, 1) || nmi_help) {
			nmi_help = 0;
			nmi_prf("commands are: (can be abbreviated)\n"
#if	GDB
				"\tgdb - break to debugger\n"
#endif	GDB
				"\t?, help - print this list\n");
#if	XPR_DEBUG
			nmi_prf(
				"\txpr - dump xprbufs to screen\n"
				"\txpr=n - dump last n xprbufs\n"
				"\txprclear - clear xprbufs\n");
#endif	XPR_DEBUG
			nmi_prf(
				"\tstay - keep this window around\n"
				"\treboot - sync and reboot system\n"
				"\thalt - sync and halt system\n"
				"\tmonitor - exit to monitor\n"
				"\tmsg - dump kernel msg buffer to screen\n"
				"\tmsg=n - dump last n msg buffers\n"
				"\tbig - make NMI window bigger\n"
				"\tcontinue - resume execution\n"
				"or set any kernel flag (e.g. debug=2)\n"
				"or examine any kernel flag (e.g. debug=)\n");
			continue;
		}
#if	GDB
		if (nmi_gdb) {
			nmi_gdb = 0;
			asm("trap #14");	/* return to gdb */
			break;
		} else
#endif	GDB
		if (nmi_big) {
			if (panic) {
				nmi_prf ("can't make window bigger after a panic\n");
				nmi_big = 0;
				continue;
			}
			break;
		} else
		if (nmi_cont) {
			nmi_cont = 0;
			break;
		} else
		if (nmi_mon) {
			nmi_mon = 0;
			mon_call("-h");
			km_clear_screen();
			break;
		} else
		if (nmi_halt) {
			nmi_halt = 0;
			reboot_mach(RB_HALT);
			goto out;
		} else
		if (nmi_reboot) {
			nmi_reboot = 0;
			reboot_mach(RB_AUTOBOOT);
			goto out;
		} else
		if (nmi_msg) {
			int nlines, col;
			char *mp, c;

			nlines = km.nc_h * 3/4;
			col = 0;
			if (pmsgbuf->msg_magic != MSG_MAGIC)
				continue;
			mp = &pmsgbuf->msg_bufc[pmsgbuf->msg_bufx];
			while (nmi_msg > 1) {
				do {
					if (mp == pmsgbuf->msg_bufc)
						mp = &pmsgbuf->msg_bufc
							[MSG_BSIZE];
					mp--;
				} while (*mp != '\n');
				nmi_msg--;
			}
			nmi_msg = 0;
			do {
				if (c = *mp) {
					cnputc (*mp);
					if (++col >= km.nc_w || *mp == '\n') {
						col = 0;
						nlines--;
					}
				}
				if (nlines <= 0) {
					if (c != '\n')
						cnputc ('\n');
					nmi_prf ("more? ");
					gets (line, line);
					if (*line == 'n' || *line == 'q')
						break;
					nlines = km.nc_h * 3/4;
				}
				if (++mp >= &pmsgbuf->msg_bufc[MSG_BSIZE])
					mp = pmsgbuf->msg_bufc;
			} while (mp != &pmsgbuf->msg_bufc[pmsgbuf->msg_bufx]);
			if (c != '\n')
				cnputc ('\n');
		}
#if	XPR_DEBUG
		else
		if (nmi_xpr) {
			extern struct xprbuf *xprptr, *xprlast, *xprbase;
			register struct xprbuf *x = xprptr;
			register int nlines, col, last_ts;
			static xpr_hdr;
			char *sp, c;

			if (!x) {
				nmi_prf("xpr tracing not enabled\n");
			}
			nlines = km.nc_h * 3/4;
			col = 0;
			xpr_hdr = 1;
			while (nmi_xpr > 1) {
				if (x == xprbase)
					x = xprlast;
				else
					x--;
				nmi_xpr--;
			}
			nmi_xpr = 0;
			last_ts = x->timestamp;
			while (1) {
				if (x->msg && (xpr_hdr || x->msg[0] == '\n')) {
					if (x->msg[0] == '\n' && col)
						cnputc ('\n');
					sprintf(line, "%u<%d>:\t",
					    x->timestamp,
					    x->timestamp - last_ts);
					col = strlen (line) + 7;
					nmi_prf (line);
					last_ts = x->timestamp;
					xpr_hdr = 0;
					if (x->msg[0] == '\n')
						nlines--;
				}
				if (x->msg) {
					if (x->msg[strlen(x->msg)-1] == '\n')
						xpr_hdr = 1;
					sp = line;
					sprintf(sp, x->msg, x->arg1, x->arg2,
					       x->arg3, x->arg4, x->arg5);
					while (*sp) {
						cnputc (c = *sp++);
						if (c == '\t')
							col += 7;
						if (++col >= km.nc_w ||
						    c == '\n') {
							col = 0;
							nlines--;
						}
					}
				}
				if (nlines <= 0) {
					if (c != '\n')
						cnputc ('\n');
					nmi_prf("more? ");
					gets(line, line);
					if (*line == 'n' || *line == 'q')
						break;
					nlines = km.nc_h * 3/4;
				}
				if (++x == xprlast)
					x = xprbase;
				if (x == xprptr) {
					if (xpr_hdr == 0)
						cnputc ('\n');
					break;
				}
			}
		} else
		if (nmi_xprclear) {
			extern struct xprbuf *xprptr, *xprlast, *xprbase;
			register struct xprbuf *x = xprbase;

			nmi_xprclear = 0;
			while (x != xprlast) {
				x->msg = 0;
				x++;
			}
			xprptr = xprbase;
		}
#endif	XPR_DEBUG
	}
cont:
	nmi_prf ("\n");
out:
	if (saved && !nmi_stay)
		kmrestore();
	nmi_stay = 0;
	resynctime();
	splx(s);
}

/* keep nmi messages from getting written to message log */
nmi_prf(fmt, x1)
	char *fmt;
	unsigned x1;
{
	prf(fmt, &x1, 1, 0);
}

/*
 *	Routines that implement software interrupts.
 */
int	nsoftint = 100;			/* number of simultaneous softints */

struct	softint {
	struct	softint *si_next;	/* pointer to next cell */
	func	si_proc;		/* procedure to call */
	caddr_t	si_arg;			/* argument to called procedure */
} *softint, *softint_head[N_CALLOUT_PRI], *softint_tail[N_CALLOUT_PRI],
  *softint_free;

thread_t softint_thread;
decl_simple_lock_data(static,	softint_th_lock)

softint_sched (level, proc, arg)
	register int level;
	register func proc;
	register caddr_t arg;
{
	register struct softint *si, **head, **tail;
	register int s;
	int softint_run();

	if (level < 0 || level >= N_CALLOUT_PRI) {
		printf("softint_sched(%d, 0x%x, %d)\n", level, proc, arg);
		panic("softint_sched");
	}

	if (level == CALLOUT_PRI_NOW) {
		(*proc)(arg);
		return;
	}

	s = spldma();
	head = &softint_head[level];
	for (si = *head; si; si = si->si_next)
		if (proc == si->si_proc && arg == si->si_arg)
			goto out;		/* duplicate entry */
	if ((si = softint_free) == 0)
		panic("out of softints");
	softint_free = si->si_next;
	tail = &softint_tail[level];
	if (*head) {
		(*tail)->si_next = si;
		*tail = si;
	} else {
		*head = *tail = si;

		switch (level) {
		case CALLOUT_PRI_RETRACE:
			vidInterruptEnable( softint_run, (void *)CALLOUT_PRI_RETRACE);
			break;
		}
	}
	si->si_next = 0;
	si->si_proc = proc;
	si->si_arg = arg;

	switch (level) {
	case CALLOUT_PRI_SOFTINT0:
	case CALLOUT_PRI_SOFTINT1:
		*scr2 |= SCR2_LOCALINT * (level + 1);
		break;
	case CALLOUT_PRI_DSP:
		((struct dsp_regs *)P_DSP)->icr |= ICR_TREQ;
		break;
	case CALLOUT_PRI_THREAD:
		if (curipl() >= IPLSCHED)
			softint_sched(CALLOUT_PRI_SOFTINT0,
				thread_wakeup, (int)softint_thread);
		else
			thread_wakeup((int)softint_thread);
		break;
	}
out:
	splx(s);
}

void softint_th(void)
{
	int s;
	simple_lock_init(&softint_th_lock);

	while (1) {
		softint_run(CALLOUT_PRI_THREAD);
		s = splsched();
		if (softint_head[CALLOUT_PRI_THREAD] == 0) {
			simple_lock(&softint_th_lock);
			thread_sleep((int)softint_thread,
				     simple_lock_addr(softint_th_lock),
				     TRUE);
		}
		splx(s);
	}
}

softint_run (level)
	register int level;
{
	register struct softint *si, *psi, *ssi, **head;
	register func proc;
	register caddr_t arg;
	register int s;
#ifdef DEBUG
	int ipl;

	switch (level) {
		default:
			ASSERT(curipl() == level + 1);
			break;
		case CALLOUT_PRI_THREAD:
			ASSERT(curipl() == 0);
			break;
	}
#endif DEBUG

	if (nmi_big) {
		int km_big();
		
		nmi_big = 0;
		softint_sched(CALLOUT_PRI_THREAD, km_big, 0);
	}

	switch (level) {
	case CALLOUT_PRI_SOFTINT0:
	case CALLOUT_PRI_SOFTINT1:

		/* ack interrupt */
		s = spldma();
		*scr2 &= ~(SCR2_LOCALINT * (level + 1));
		splx(s);

		/* fall into ... */

	case CALLOUT_PRI_THREAD:
	case CALLOUT_PRI_DSP:		/* dsp does intr ack. */
		while (1) {
			s = spldma();
			head = &softint_head[level];
			if (si = *head) {
				proc = si->si_proc;
				arg = si->si_arg;
				*head = si->si_next;
				si->si_next = softint_free;
				softint_free = si;
				if (*head == 0)
					softint_tail[level] = 0;
			}
			splx(s);
			if (si == 0)
				return;
			(proc)(arg);
#ifdef DEBUG
			if (curipl() != srtoipl(s)) {
				XPR(XPR_MEAS, ("BAD CALLOUT IPL: func 0x%x "
				    "old ipl %d new ipl %d\n",
				    proc, srtoipl(s), curipl()));
			}
#endif DEBUG

		}
		break;

	case CALLOUT_PRI_RETRACE:
#ifdef DEBUG
		ipl = curipl();
#endif DEBUG
		/* run routine once, dequeueing only if requested */
		head = &softint_head[level];
		si = *head;
		psi = 0;
		while (si) {
			proc = si->si_proc;
			arg = si->si_arg;
			if ((proc)(arg) == 0) {
				if (psi)
					psi->si_next = si->si_next;
				else
					*head = softint_tail[level] = 0;
				s = spldma();
				ssi = si->si_next;
				si->si_next = softint_free;
				softint_free = si;
				si = ssi;
				splx(s);
			} else {
				psi = si;
				si = si->si_next;
			}
#ifdef DEBUG
			if (curipl() != ipl) {
				XPR(XPR_MEAS, ("BAD CALLOUT IPL: func 0x%x "
				    "old ipl %d new ipl %d\n",
				    proc, ipl, curipl()));
			}
#endif DEBUG
		}
		break;
	}
}

/*
 * Machine-dependent early startup code
 */
startup_early()
{
	register caddr_t firstaddr, v;

	v = firstaddr = (caddr_t) mem_region[0].first_phys_addr;
#if	SHOW_SPACE
#define	valloc(name, type, num)	\
	(name) = (type *)(v); (v) = (caddr_t)((name)+(num));\
	if (show_space) \
		printf(#name " = %d(0x%x) bytes @%x, %d cells @ %d bytes\n",\
		 num*sizeof(type), num*sizeof(type), name, num, sizeof(type))
#define	valloclim(name, type, num, lim)	\
	(name) = (type *)(v); (v) = (caddr_t)((lim) = ((name)+(num)));\
	if (show_space) \
		printf(#name " = %d(0x%x) bytes @%x, %d cells @ %d bytes\n",\
		 num*sizeof(type), num*sizeof(type), name, num, sizeof(type))
#else	SHOW_SPACE
#define	valloc(name, type, num)	\
	(name) = (type *)(v); (v) = (caddr_t)((name)+(num));
#define	valloclim(name, type, num, lim)	\
	(name) = (type *)(v); (v) = (caddr_t)((lim) = ((name)+(num)));
#endif	SHOW_SPACE
	valloc(cfree, struct cblock, nclist);
	valloc(callout, struct callout, ncallout);
	valloc(us_callout, struct callout, ncallout);
	valloc(softint, struct softint, nsoftint);
	valloc(ncache, struct ncache, ncsize);
#if	QUOTA
	valloclim(dquot, struct dquot, ndquot, dquotNDQUOT);
#endif	QUOTA
	
#define	PRIVATE_BUFS 0 /* Don't turn this on until we have fault ahead etc. */
	/*
	 * Use a small number of bufpages to support indirect blocks
	 * and a large number of buf headers to support the pager's use
	 * of private pages.
	 */
 	if (bufpages == 0)
		bufpages = atop(mem_size / 10);
	if (nbuf == 0)
#if	PRIVATE_BUFS
		nbuf = 100;
#else	PRIVATE_BUFS
		nbuf = 16;
#endif	PRIVATE_BUFS
	if (bufpages > nbuf * (MAXBSIZE / PAGE_SIZE))
		bufpages = nbuf * (MAXBSIZE / PAGE_SIZE);
	valloc(buf, struct buf, nbuf);
#undef	valloc
#undef	valloclim

	/*
	 * Clear space allocated thus far.
	 */
	bzero(firstaddr, v - firstaddr);
	mem_region[0].first_phys_addr = (int) v;

#if	SHOW_SPACE
	if (show_space)
		printf("early data structures %d@%x-%x\n",
			v - firstaddr, firstaddr, v);
#endif	SHOW_SPACE
}

startup(firstaddr)
	caddr_t	firstaddr;
{
	register unsigned int i;
	register caddr_t v;
	int base, residual, e;
	vm_size_t	size;
	kern_return_t	ret;
	vm_offset_t	trash_offset;
	extern long map_size;
	extern struct mon_global *mon_global;
	struct mon_global *mg = mon_global;
	extern int console_o;
	int as_tune(), msize;
	extern u_int fpu_version;
#if	SHOW_SPACE
	extern int	vm_page_wire_count;
#endif	SHOW_SPACE

	km_switch_to_vm();	/* Have km package remap console frame buffer */
#if	SHOW_SPACE
	if (show_space)
		printf("startup: enter wire %d\n", vm_page_wire_count);
#endif	SHOW_SPACE

	panic_init();

	/*
	 * Good {morning,afternoon,evening,night}.
	 */

	printf(version);
	printf("FPU version 0x%x\n", fpu_version >> 24);

#define MEG	(1024*1024)
	msize = roundup(mem_size, MEG);
	printf("physical memory = %d.%d%d megabytes.\n", msize/MEG,
		((msize%MEG)*10)/MEG,
		((msize%(MEG/10))*100)/MEG);

	/*
	 * Allocate space for system data structures.
	 * The first available real memory address is in "firstaddr".
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 * An index into the kernel page table corresponding to the
	 * virtual memory address maintained in "v" is kept in "mapaddr".
	 */
	/*
	 *	It is possible that someone has allocated some stuff
	 *	before here, (like vcons_init allocates the unibus map),
	 *	so, we look for enough space to put the dynamic data
	 *	structures, then free it with the assumption that we
	 *	can just get it back later (at the same address).
	 */
	firstaddr = (caddr_t) round_page(firstaddr);
	/*
	 *	Between the following find, and the next one below
	 *	we can't cause any other memory to be allocated.  Since
	 *	below is the first place we really need an object, it
	 *	will cause the object zone to be expanded, and will
	 *	use our memory!  Therefore we allocate a dummy object
	 *	here.  This is all a hack of course.
	 */
	ret = vm_map_find(kernel_map, vm_object_allocate(0), (vm_offset_t) 0,
		&firstaddr,
		8*1024*1024, TRUE);	/* size big enough for worst case */
	ASSERT(ret == KERN_SUCCESS);
	vm_map_remove(kernel_map, firstaddr, firstaddr + 8*1024*1024);
	v = firstaddr;
#if	SHOW_SPACE
#define	valloc(name, type, num)	\
	(name) = (type *)(v); (v) = (caddr_t)((name)+(num));\
	if (show_space) \
		printf(#name " = %d(0x%x) bytes @%x, %d cells @ %d bytes\n",\
		 num*sizeof(type), num*sizeof(type), name, num, sizeof(type))
#define	valloclim(name, type, num, lim)	\
	(name) = (type *)(v); (v) = (caddr_t)((lim) = ((name)+(num)));\
	if (show_space) \
		printf(#name " = %d(0x%x) bytes @%x, %d cells @ %d bytes\n",\
		 num*sizeof(type), num*sizeof(type), name, num, sizeof(type))
#else	SHOW_SPACE
#define	valloc(name, type, num)	\
	(name) = (type *)(v); (v) = (caddr_t)((name)+(num));
#define	valloclim(name, type, num, lim)	\
	(name) = (type *)(v); (v) = (caddr_t)((lim) = ((name)+(num)));
#endif	SHOW_SPACE

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	valloc(buffers, char, MAXBSIZE * nbuf);
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
#if	SHOW_SPACE
	if (show_space)
		printf("bufpages %d nbuf %d base %d residual %d\n",
			bufpages, nbuf, base, residual);
#endif	SHOW_SPACE

	/*
	 *	Allocate virtual memory for buffer pool.
	 */
	v = (caddr_t) round_page(v);
	size = (vm_size_t) (v - firstaddr);
	buffer_map = kmem_suballoc(kernel_map, &firstaddr,
		&trash_offset /* max */, size, TRUE);
	ret = vm_map_find(buffer_map, vm_object_allocate(size),
		(vm_offset_t) 0, &firstaddr, size, FALSE);
	ASSERT(ret == KERN_SUCCESS);
#if	SHOW_SPACE
	if (show_space)
		printf("vm buffer pool wire %d\n", vm_page_wire_count);
#endif	SHOW_SPACE

	for (i = 0; i < nbuf; i++) {
		vm_size_t	thisbsize;
		vm_offset_t	curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */

		thisbsize = PAGE_SIZE*(i < residual ? base+1 : base);
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		vm_map_pageable(buffer_map, curbuf, curbuf+thisbsize, FALSE);
	}

#if	SHOW_SPACE
	if (show_space)
		printf("buffers wired wire %d\n", vm_page_wire_count);
#endif	SHOW_SPACE

	{
		register int	nbytes;
		extern int	vm_page_free_count;

		nbytes = ptoa(vm_page_free_count);
		printf("available memory = %d.%d%d megabytes.\n", nbytes/MEG,
			((nbytes%MEG)*10)/MEG,
			((nbytes%(MEG/10))*100)/MEG);
		nbytes = ptoa(bufpages);
		printf("using %d buffers containing %d.%d%d megabytes of memory\n",
			nbuf,
			nbytes/MEG,
			((nbytes%MEG)*10)/MEG,
			((nbytes%(MEG/10))*100)/MEG);
	}

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[ncallout-1].c_next = 0;
	
	us_callfree = us_callout;
	for (i = 1; i < ncallout; i++)
		us_callout[i-1].c_next = &us_callout[i];
	us_callout[ncallout-1].c_next = 0;

	/*
	 * Initialize softints
	 */
	softint_free = softint;
	for (i = 1; i < nsoftint; i++)
		softint[i-1].si_next = &softint[i];
	softint[nsoftint-1].si_next = 0;
	
	/*
	 * Initialize memory allocator and swap
	 * and user page table maps.
	 */
	mb_map = kmem_suballoc(kernel_map, &mbutl, &embutl,
		ptoa((nmbclusters)), FALSE);

	/* boot icon animation */
	if ((km.flags & KMF_SEE_MSGS) == 0 && console_o == CONS_O_BITMAP &&
	    *MG(short*, MG_seq) >= 24 && *MG(short*, MG_seq) <= 99 &&
	    *MG(int*, MG_anim_run))
		softint_sched(CALLOUT_PRI_RETRACE,
			*MG(int*, MG_anim_run), 0);

	/*
	 *	Just tune once within 1/hz seconds as periodic
	 *	tuning breaks DMA for reasons we don't understand.
	 */
	if (bmap_chip) {
		if (astune_rate == 0)
			astune_rate = 1;
		timeout (as_tune, 0, astune_rate*hz);
	}

	/*
	 * Configure the system.
	 */
	configure();

#if	SHOW_SPACE
	if (show_space)
		printf("startup: exit wire %d\n", vm_page_wire_count);
#endif	SHOW_SPACE
}

/* retune AS periodically while the BMAP chip warms up */
as_tune (calls)
{
	extern struct mon_global *mon_global;
	struct mon_global *mg = mon_global;
	int s;

	s = splhigh();
	if (mg->mg_seq >= 46 && mg->mg_as_tune)
		mg->mg_as_tune();
	splx(s);
	if (++calls < astune_calls)
		timeout (as_tune, calls, astune_rate*hz);
}

#ifdef PGINPROF
/*
 * Return the difference (in microseconds)
 * between the  current time and a previous
 * time as represented  by the arguments.
 * If there is a pending clock interrupt
 * which has not been serviced due to high
 * ipl, return error code.
 */
vmtime(otime, olbolt, oicr)
	register int otime, olbolt;
{
	struct timeval tv;
	microtime(&tv);
	return(((tv-otime)*HZ + lbolt-olbolt)*(1000000/HZ));
}
#endif

/*
 * Clear registers on exec
 */
setregs(entry)
	u_long entry;
{
	register struct regs *r = (struct regs*) u.u_ar0;
	register int *rp;

	for (rp = &r->r_dreg[0]; rp < &r->r_areg[7];)
		*rp++ = 0;
	r->r_pc = entry;
	r->r_sr = SR_USER;
	u.u_eosys = JUSTRETURN;
}

#if	DEBUG
int signaltrace;
#endif	DEBUG

/*
 * Send an interrupt to process.
 *
 * The stack is setup to call the signal handler which is
 * really a signal trampoline created by libc.
 * The library does this by intercepting the sigvec
 * system call, saving the address of the user's signal
 * handler, and specifing sigtramp as the signal handler
 * to the actual sigvec syscall.  When sigtramp gets called in
 * response to the signal it calls the user's signal handler
 * and then does a syscall to the sigreturn routine below.
 * This scheme keeps the signal trampoline out of the kernel
 * u area or PCB and in libc where it belongs.  In the library
 * it's easier to save state (like the FPC) than if the
 * trampoline code is in the kernel.
 *
 * In the code below a prototype stack is built and copied
 * to the user stack with copyout because we have no direct
 * access to the user stack.
 */
sendsig(p, sig, mask)
	int (*p)(), sig, mask;
{
	register struct regs *r;
	struct sigframe {
		int	sf_signum;
		int	sf_code;
		struct	sigcontext *sf_scp;
	} sf;
	register struct sigframe *sfp;
	struct sigcontext sc;
	register struct sigcontext *scp;
	int oonstack;

	r = (struct regs*) u.u_ar0;
	oonstack = u.u_onstack;
	if (!u.u_onstack && (u.u_sigonstack & sigmask(sig))) {
		scp = (struct sigcontext *)u.u_sigsp - 1;
		u.u_onstack = 1;
	} else
		scp = (struct sigcontext *)r->r_sp - 1;
	sfp = (struct sigframe *)scp - 1;
	/* 
	 * Build the argument list for the signal handler.
	 */
	sf.sf_signum = sig;
	if (sig == SIGILL || sig == SIGFPE || sig == SIGEMT) {
		sf.sf_code = u.u_code;
		u.u_code = 0;
	} else
		sf.sf_code = 0;
	sf.sf_scp = scp;
	if (copyout((caddr_t) &sf, (caddr_t) sfp, sizeof(sf)))
		goto bad;
	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sc.sc_onstack = oonstack;
	sc.sc_mask = mask;
	sc.sc_sp = r->r_sp;
	sc.sc_pc = r->r_pc;
	sc.sc_ps = r->r_sr;
	sc.sc_d0 = r->r_d0;
#if	DEBUG
	if (signaltrace) {
		printf("sendsig: sigcontext: sp 0x%x sr 0x%x pc 0x%x, sighndlr=0x%x\n",
			r->r_sp, r->r_sr, r->r_pc, p);
		printf("\tsignum %d code %d scp 0x%x\n", sig, sf.sf_code, scp);
	}
#endif	DEBUG

	/*
	 *	If tracing for AST, then don't restore with tracing
	 *	enabled in case a future AST request is made which
 	 *	would interpret the existing tracing as user requested
	 *	instead of AST requested.
	 */

	if ((current_thread()->pcb->pcb_flags & TRACE_AST) &&
	    (r->r_sr & SR_TSINGLE)) {
		sc.sc_ps &= ~SR_TSINGLE;
	}
	if (copyout((caddr_t) &sc, (caddr_t) scp, sizeof(sc)))
		goto bad;
	r->r_sp = (int)sfp;
	r->r_pc = (int)p;
	return;
bad:
	u.u_signal[SIGILL] = SIG_DFL;
	sig = sigmask(SIGILL);
	u.u_procp->p_sigignore &= ~sig;
	u.u_procp->p_sigcatch &= ~sig;
	u.u_procp->p_sigmask &= ~sig;
	psignal(u.u_procp, SIGILL);
	return;
}

/*
 * System call to cleanup state after a signal has been taken.
 * Called by signal trampoline code in libc.
 * Reset signal mask and stack state from context left by sendsig (above).
 * Return to previous pc and sr as specified by context left by sendsig.
 * Check carefully to make sure that the user has not modified the
 * sr to gain improper priviledges or to cause a machine fault.
 *
 * FIXME: when Sun binary compatibility isn't an issue anymore modify
 * to use 4.3 syscall semantics: syscall code #139 -> #103, passes
 * sigcontext argument.  Modify libc/sigtramp accordingly!
 */

sigreturn()
{
	struct sigcontext sc, *scp = 0;
	register struct regs *r = (struct regs*) u.u_ar0;

	/* + NBPW skips over syscall return pc */
	if (copyin((caddr_t) r->r_sp + NBPW, (caddr_t) &scp, sizeof(scp)))
		goto bad;
	if (copyin ((caddr_t) scp, (caddr_t) &sc, sizeof(sc)))
		goto bad;
	u.u_eosys = JUSTRETURN;
	u.u_onstack = sc.sc_onstack & 01;
	u.u_procp->p_sigmask = sc.sc_mask &~
	    (sigmask(SIGKILL)|sigmask(SIGCONT)|sigmask(SIGSTOP));
	r->r_sp = sc.sc_sp;
	r->r_pc = sc.sc_pc;
	r->r_sr = sc.sc_ps;
	r->r_d0 = sc.sc_d0;
#if	DEBUG
	if (signaltrace)
		printf("sigreturn: sigcontext: sp 0x%x sr 0x%x pc 0x%x\n",
			r->r_sp, r->r_sr, r->r_pc);
#endif	DEBUG
	r->r_sr &= ~SR_USERCLR;
bad:
	return;
}

#import <sys/machine.h>
halt_cpu()
{
	machine_slot[cpu_number()].running = FALSE;
	while (1)
		asm("stop #0");		/* FIXME: good enough? */
}

int	waittime = -1;

boot(paniced, howto, command)
	int paniced, howto;
	char *command;
{
	register int i;
	int s;
	extern struct vnode *acctp;

	if ((howto&RB_NOSYNC)==0 && waittime < 0 && bfreelist[0].b_forw) {
		waittime = 0;

		/*
		 * Disable accounting before unmounting so we can be
		 * sure everything will be unmounted cleanly.
		 * Ultimately we should call something in kern_acct to
		 * do this work instead of having to understand how it
		 * works ourselves.
		 */
		if (acctp) {
			struct vnode *temp;

			temp = acctp;
			acctp = NULL;
			VN_RELE(temp);
		}
		
		/*
		 * Call sync() to flush any NFS buffers, then wait awhile
		 * before actually disabling the network interfaces to
		 * prevent further I/O requests.
		 */
		sync();
		unmount_all();

		/*
		 * Can't just use an splnet() here to disable the network
		 * because that will lock out softints which the disk
		 * drivers depend on to finish DMAs.
		 */
#import <en.h>
		for (i = 0; i < NEN; i++)
			en_down(i);

		{ register struct buf *bp;
		  int iter, nbusy;
		  int obusy = 0;

		  for (iter = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; )
				if ((bp->b_flags & (B_BUSY|B_DONE)) == B_BUSY)
					nbusy++;
			if (nbusy == 0)
				break;
			printf("%d ", nbusy);
		        if (nbusy != obusy)
				iter = 0;
			obusy = nbusy;
			DELAY(40000 * iter);
		  }
		}
	}
#import	<od.h>
#if	NOC > 0

	/*
	 *  Update optical tables after regular I/O has finished.
	 */
	od_update();
	if ((howto & RB_EJECT) && kernel_task)
		od_eject();
#endif	NOC > 0
#if	NCPUS > 1
#define	LOOPCOUNT	1000
#define	WAITCOUNT	1000

	for (i = 0; i < NCPUS; i++) {
		processor_t	thisprocessor;

		thisprocessor = cpu_to_processor(i);
		if (thisprocessor->state != PROCESSOR_OFF_LINE &&
		    thisprocessor != current_processor()) {
			register int j,k;

			processor_shutdown(thisprocessor);

			j = 0;
			while ( machine_slot[i].running && j < LOOPCOUNT) {
			    for (k=0; k< WAITCOUNT; k++) DELAY(1);
			    j++;
			}
			
			if (!machine_slot[i].running)
				printf("Processor %d Halted.\n", i);
			else
				printf("Processor %d won't halt.\n", i);
		}
	}
#endif	NCPUS > 1
	if (howto & RB_POWERDOWN) {
		rtc_power_down();
	}
	s = splhigh();			/* extreme priority */
	if (howto & RB_HALT) {
		printf ("halting...\n");
		mon_call ("-h");
	} else {
		if (paniced == RB_PANIC) {
#if	0
			dumpsys();	/* FIXME: make this work someday? */
#endif
		}
		printf("rebooting Mach...\n");
		if (howto & RB_COMMAND)
			mon_call(command);
		else
			mon_boot(howto & RB_SINGLE? "-s" : 0);
	}
	splx(s);
}

#import <sys/vfs.h>

unmount_all()
{
	struct vfs		*vfsp, *next;
	extern struct vfs	*rootvfs;
	extern struct vnode	*rootdir;
	int			error;

	proc_shutdown();		/* handle live procs (deallocate their
					   root and current directories). */
	kill_tasks();			/* get rid of all task memory */
	mfs_cache_clear();		/* clear the MFS cache */
	vm_object_cache_clear();	/* clear the object cache */
	fd_shutdown();			/* close all open file descriptors */

	vm_object_shutdown();
	vnode_pager_shutdown();		/* NO MORE PAGING - release vnodes */

	vfsp = rootvfs->vfs_next;	/* skip root */
	while (vfsp != (struct vfs *)0) {
		printf("unmounting /%s ... ", vfsp->vfs_name);
		next = vfsp->vfs_next;
		error = dounmount(vfsp);
		if (error)
			printf("FAILED\n");
		else
			printf("done\n");
		vfsp = next;
	}
	VN_RELE(rootdir);	/* release reference from bootstrap */
	error = dounmount(rootvfs);
	if (error)
		printf("Root unmount FAILED\n");
}

kill_tasks()
{
	processor_set_t	pset;
	task_t		task;
	vm_map_t	map, empty_map;

	empty_map = vm_map_create(pmap_create(0), 0, 0, TRUE);

	/*
	 * Destroy all but the default processor_set.
	 */
	simple_lock(&all_psets_lock);
	pset = (processor_set_t) queue_first(&all_psets);
	while (!queue_end(&all_psets, (queue_entry_t) pset)) {
		if (pset == &default_pset) {
			pset = (processor_set_t) queue_next(&pset->all_psets);
			continue;
		}
		simple_unlock(&all_psets_lock);
		processor_set_destroy(pset);
		simple_lock(&all_psets_lock);
	}
	simple_unlock(&all_psets_lock);

	/*
	 * Kill all the tasks in the default processor set.
	 */
	pset = &default_pset;
	pset_lock(pset);
	task = (task_t) queue_first(&pset->tasks);
	while (pset->task_count) {
		pset_remove_task(pset, task);
		map = task->map;
		if ((map != kernel_map) && (map != empty_map)) {
			task->map = empty_map;
			vm_map_reference(empty_map);
			pset_unlock(pset);
			vm_map_remove(map, vm_map_min(map), vm_map_max(map));
			pset_lock(pset);
		}
		task = (task_t) queue_first(&pset->tasks);
	}
	pset_unlock(pset);
}

#if 0
int	dumpmag = 0x8fca0101;	/* magic number for savecore */
long	dumpsize = 0;		/* also for savecore */

/*
 *	Dump the system by writing out all physical regions of memory.
 *	Kernel debuggers will have to reconstruct the "holes" between
 *	regions by looking at the information in NeXT_phys_region.
 */
dumpsys()
{
	register int i, bno, err;
	register vm_offset_t pa;
	register mem_region_t rp;
	int (*dumper)(), blkinc;

	if (dumpdev == NODEV) {
		printf("no dump device set\n");
		return;
	}
	dumpsize = btoc(mem_size);
	printf("\ndumping to dev (%d,%d) offset %d\n",
		major(dumpdev), minor(dumpdev), dumplo);

	dumper = bdevsw[major(dumpdev)].d_dump;
	bno = dumplo;
	for (i = 0; i < num_regions && !err; i++) {
		rp = &mem_region[i];
		for (pa = rp->base_phys_addr; pa < rp->last_phys_addr && !err;
		    pa += NeXT_page_size) {
			err = (*dumper)(dumpdev, pa, bno, &blkinc);
			bno += blkinc;
		}
	}

	printf("dump ");
	switch (err) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("failed: unknown error\n");
		break;
	}
}
#endif

physstrat(bp, strat, prio)
	struct buf *bp;
	int (*strat)(), prio;
{
	int s;

	(*strat)(bp);
	/* pageout daemon doesn't wait for pushed pages */
	if (bp->b_flags & B_DIRTY)
		return;
	s = splbio();
	while ((bp->b_flags & B_DONE) == 0)
		sleep((caddr_t)bp, prio);
	splx(s);
}

#if	NCPUS > 1
/*
 *	cpu_number() provides a convenient way to get the current cpu number
 *	without the hassle of including a bunch of header files.
 */
cpu_number()
{
	extern int slot_id;

	return(slot_id >> 25);
}
#endif	NCPUS > 1

#if	DEBUG
int light_acquired = 0;
#endif	DEBUG

light_on()
{
	register int s;

#if	DEBUG
	if (light_acquired)
		return;
#endif	DEBUG
#if	EVENTMETER
	if (em.flags & EMF_LEDS)
		km_send(MON_KM_USRREQ, KM_SET_LEDS(0, 3));
#endif	EVENTMETER
	s = spldma();
	*scr2 |= SCR2_EKG_LED;
	splx(s);
}

light_off()
{
	register int s;

#if	DEBUG
	if (light_acquired)
		return;
#endif	DEBUG
#if	EVENTMETER
	if (em.flags & EMF_LEDS)
		km_send(MON_KM_USRREQ, KM_SET_LEDS(0, 0));
#endif	EVENTMETER
	s = spldma();
	*scr2 &= ~SCR2_EKG_LED;
	splx(s);
}

/* FIXME: efficient enough? */
addupc(pc, pr, ticks)
	register int pc;
	register struct uprof *pr;
{
	register short *cell;
	short count;
	long off;

	off = (((pc - pr->pr_off)&0xffff0000) >> 16)*pr->pr_scale +
		((((pc - pr->pr_off)&0xffff)*pr->pr_scale)>>16);

/*	cell = pr->pr_base +
		(((pc - pr->pr_off) * pr->pr_scale) >> 16) / (sizeof *cell);*/
	cell = pr->pr_base + off / (sizeof *cell);
	if (cell >= pr->pr_base &&
	    cell < (short*) (pr->pr_size + (int) pr->pr_base)) {
		if (copyin (cell, (caddr_t) &count, sizeof(count))) {
			pr->pr_scale = 0;
		} else {
			count += ticks;
			copyout((caddr_t) &count, cell, sizeof(count));
		}
	}
}


