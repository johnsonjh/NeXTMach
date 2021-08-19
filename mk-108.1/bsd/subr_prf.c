/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * HISTORY
 *  2-Feb-90  Gregg Kellogg (gk) at NeXT
 *	ANSIized interfaces to routines.
 *	Made putchar() take a single argument, old version is now
 *	a static function called tputchar.  logchar is like putchar
 *	but it uses TOLOG.
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Removed dir.h
 *
 *  6-Aug-89  John Seamons (jks) at NeXT
 *	Added a sprintf function by adding a new flag type to putchar.
 *
 * 30-Sep-87  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Added include of "machine/cpu.h" for the definition of
 *	cpu_number() (for non-multiprocessor systems).
 *
 * 31-Aug-87  David Black (dlb) at Carnegie-Mellon University
 *	MACH_MP: multiprocessor panic logic.  If a second panic occurs
 *	on a different cpu, immediately stop that cpu.
 *
 * 27-Apr-87  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Changed dependency on command-line defined symbol RDB to
 *	include file symbol ROMP_RDB.  Include romp_rdb.h if compiling
 *	for romp.
 *
 * 27-Mar-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	allow a field size spec after the % for numeric fields
 *
 * 30-Jan-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_GENERIC:  Changed log() in the absence of a syslog daemon to
 *	only log messages on the console which are LOG_WARNING or
 *	higher priority (this requires a companion change to the syslog
 *	configuration file in order to have the same effect when syslog
 *	is running).
 *	[ V5.1(F1) ]
 *
 * 08-Jan-87  Robert Beck (beck) at Sequent Computer Systems, Inc.
 *	Add BALANCE case to panic().
 *
 * 21-Oct-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Merged in change for Sun to trap into kdb in panic().
 *
 *  7-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	Merged in Multimax changes; mostly due to register parameter passing.
 *
 * 13-Apr-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	CS_GENERIC:  print table full messages on console in addition to
 *	loging them.
 *
 * 15-Feb-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Added different definitions of the parameter list for printf and
 *	uprintf under the assumption that the Sailboat c compiler needs
 *	them.  Switched on by romp.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 *  8-Sep-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	On a panic, issue a break point trap if kernel debugger present
 *	(and active).
 *
 * 26-Jun-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_KDB:  Modified panic() to generate a breakpoint trap
 *	before rebooting.
 *	[V1(1)]
 *
 * 14-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added (and modified) mprintf from Ultrix.
 *
 * 15-Sep-84  Robert V Baron (rvb) at Carnegie-Mellon University
 *	Spiffed up printf b format. you can also give an entry of the form
 *	\#1\#2Title
 *	will print Title then extract the bits from #1 to #2 as a field and
 *	print it -- some devices have fields in their csr. consider
 *	vaxmpm/mpmreg.h as an example.
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)subr_prf.c	7.1 (Berkeley) 6/5/86
 */

#if	NeXT
#import <mach_ldebug.h>
#endif	NeXT

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/conf.h>
#import <sys/reboot.h>
#import <sys/msgbuf.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/ioctl.h>
#import <sys/syslog.h>
#import <sys/printf.h>
#import <kern/lock.h>

#import <machine/spl.h>

#import <machine/cpu.h>	/* for cpu_number() */

#ifdef	NeXT
#import <next/printf.h>
#import <mon/global.h>
#endif	NeXT

#define TOCONS	0x1
#define TOTTY	0x2
#define TOLOG	0x4
#define	TOSTR	0x8

static void logpri(int level);
static void puts(const char *s, int flags, struct tty *ttyp);
static void printn(u_long n, int b, int flags, struct tty *ttyp, int zf, int fld_size);
static void tputchar(int c, int flags, struct tty * tp);

/*
 * In case console is off,
 * panicstr contains argument to last
 * call to panic.
 */
const char	*panicstr;

/*
 *	Record cpu that panic'd and lock around panic data
 */
simple_lock_data_t panic_lock;
int paniccpu;

/*
 * Scaled down version of C Library printf.
 * Used to print diagnostic information directly on console tty.
 * Since it is not interrupt driven, all system activities are
 * suspended.  Printf should not be used for chit-chat.
 *
 * One additional format: %b is supported to decode error registers.
 * Usage is:
 *	printf("reg=%b\n", regval, "<base><arg>*");
 * Where <base> is the output base expressed as a control character,
 * e.g. \10 gives octal; \20 gives hex.  Each arg is a sequence of
 * characters, the first of which gives the bit number to be inspected
 * (origin 1), and the next characters (up to a control character, i.e.
 * a character <= 32), give the name of the register.  Thus
 *	printf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 * would produce output:
 *	reg=3<BITTWO,BITONE>
 */
#if	NeXT
/*
 * See ../next/printf.h for a description of %r, %R, %n, %N, and %C formats.
 */
#endif	NeXT
int printf(const char *format, ...)
{

	if (prf(format, ((u_int *)&format)+1, TOCONS | TOLOG, (struct tty *)0))
		logwakeup();
	return 0;
}

/*
 * Uprintf prints to the current user's terminal.
 * It may block if the tty queue is overfull.
 * Should determine whether current terminal user is related
 * to this process.
 */
int uprintf(const char *format, ...)
{
#ifdef notdef
	register struct proc *p;
#endif
	register struct tty *tp;

	if ((tp = u.u_ttyp) == NULL)
		return;
#ifdef notdef
	if (tp->t_pgrp && (p = pfind(tp->t_pgrp)))
		if (p->p_uid != u.u_uid)	/* doesn't account for setuid */
			return -1;
#endif
	(void)ttycheckoutq(tp, 1);
	prf(format, ((u_int *)&format)+1, TOTTY, tp);
	return 0;
}

/*
 * tprintf prints on the specified terminal (console if none)
 * and logs the message.  It is designed for error messages from
 * single-open devices, and may be called from interrupt level
 * (does not sleep).
 */
int tprintf(struct tty *tp, const char *format, ...)
{
	int flags = TOTTY | TOLOG;
	extern struct tty cons;

	logpri(LOG_INFO);
	if (tp == (struct tty *)NULL)
		tp = &cons;
	if (ttycheckoutq(tp, 0) == 0)
		flags = TOLOG;
	prf(format, ((u_int *)&format)+1, flags, tp);
	logwakeup();
	return 0;
}

/*
 * Log writes to the log buffer,
 * and guarantees not to sleep (so can be called by interrupt routines).
 * If there is no process reading the log yet, it writes to the console also.
 */
int log(int level, const char *format, ...)
{
	register s = splhigh();
	extern int log_open;

	logpri(level);
	prf(format, ((u_int *)&format)+1, TOLOG, (struct tty *)0);
	splx(s);
	if (!log_open)
		prf(format, ((u_int *)&format)+1, TOCONS, (struct tty *)0);
	logwakeup();
	return 0;
}

static void logpri(int level)
{

	logchar('<');
	printn((u_long)level, 10, TOLOG, (struct tty *)0, 0, 0);
	logchar('>');
}

int prf(const char *fmt, u_int *adx, int flags, struct tty *ttyp)
{
	register int b, c, i;
	char *s;
	int any;
	int zf = 0, fld_size, log_wakeup = 1;
loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			return (log_wakeup);
		tputchar(c, flags, ttyp);
	}
again:
	c = *fmt++;
	if (c == '0')
		zf = '0';
	fld_size = 0;
	for (;c <= '9' && c >= '0'; c = *fmt++)
		fld_size = fld_size * 10 + c - '0';
	
	/* THIS CODE IS VAX DEPENDENT IN HANDLING %l? AND %c */
	switch (c) {

	case 'l':
		goto again;
	case 'x': case 'X':
		b = 16;
		goto number;
	case 'd': case 'D':
	case 'u':		/* what a joke */
		b = 10;
		goto number;
	case 'o': case 'O':
		b = 8;
number:
		printn((u_long)*adx, b, flags, ttyp, zf, fld_size);
		break;
	case 'c':
		b = *adx;
		for (i = 24; i >= 0; i -= 8)
			if (c = (b >> i) & 0x7f)
				tputchar(c, flags, ttyp);
		break;
	case 'b':
		b = *adx++;
		s = (char *)*adx;
		printn((u_long)b, *s++, flags, ttyp, 0, 0);
		any = 0;
		if (b) {
			while (i = *s++) {
				if (*s <= 32) {
					register int j;

					if (any++)
						tputchar(',', flags, ttyp);
					j = *s++ ;
					for (; (c = *s) > 32 ; s++)
						tputchar(c, flags, ttyp);
					printn( (u_long)( (b >> (j-1)) &
							 ( (2 << (i-j)) -1)),
						 8, flags, ttyp, 0, 0);
				} else if (b & (1 << (i-1))) {
					tputchar(any? ',' : '<', flags, ttyp);
					any = 1;
					for (; (c = *s) > 32; s++)
						tputchar(c, flags, ttyp);
				} else
					for (; *s > 32; s++)
						;
			}
			tputchar('>', flags, ttyp);
		}
		break;

	case 's':
		s = (char *)*adx;
		while (c = *s++)
			tputchar(c, flags, ttyp);
		break;

	case '%':
		tputchar('%', flags, ttyp);
		goto loop;
#if	NeXT
	case 'C':
		b = *adx;
		for (i = 24; i >= 0; i -= 8)
			if (c = (b >> i) & 0xff)
				tputchar(c, flags, ttyp);
		break;

	case 'r':
	case 'R':
		b = *adx++;
		s = (char *)*adx;
		if (c == 'R') {
			puts("0x", flags, ttyp);
			printn((u_long)b, 16, flags, ttyp, 0, 0);
		}
		any = 0;
		if (c == 'r' || b) {
			register struct reg_desc *rd;
			register struct reg_values *rv;
			unsigned field;

			tputchar('<', flags, ttyp);
			for (rd = (struct reg_desc *)s; rd->rd_mask; rd++) {
				field = b & rd->rd_mask;
				field = (rd->rd_shift > 0)
				    ? field << rd->rd_shift
				    : field >> -rd->rd_shift;
				if (any &&
				      (rd->rd_format || rd->rd_values
				         || (rd->rd_name && field)
				      )
				)
					tputchar(',', flags, ttyp);
				if (rd->rd_name) {
					if (rd->rd_format || rd->rd_values
					    || field) {
						puts(rd->rd_name, flags, ttyp);
						any = 1;
					}
					if (rd->rd_format || rd->rd_values) {
						tputchar('=', flags, ttyp);
						any = 1;
					}
				}
				if (rd->rd_format) {
					prf(rd->rd_format, &field, flags, ttyp);
					any = 1;
					if (rd->rd_values)
						tputchar(':', flags, ttyp);
				}
				if (rd->rd_values) {
					any = 1;
					for (rv = rd->rd_values;
					    rv->rv_name;
					    rv++) {
						if (field == rv->rv_value) {
							puts(rv->rv_name, flags,
							    ttyp);
							break;
						}
					}
					if (rv->rv_name == NULL)
						puts("???", flags, ttyp);
				}
			}
			tputchar('>', flags, ttyp);
		}
		break;

	case 'n':
	case 'N':
		{
			register struct reg_values *rv;

			b = *adx++;
			s = (char *)*adx;
			for (rv = (struct reg_values *)s; rv->rv_name; rv++) {
				if (b == rv->rv_value) {
					puts(rv->rv_name, flags, ttyp);
					break;
				}
			}
			if (rv->rv_name == NULL)
				puts("???", flags, ttyp);
			if (c == 'N' || rv->rv_name == NULL) {
				tputchar(':', flags, ttyp);
				printn((u_long)b, 10, flags, ttyp, 0, 0);
			}
		}
		break;
	
	case 'L':
		/* keep log daemon from being waking up */
		log_wakeup = 0;
		goto loop;
#endif	NeXT
	}
	adx++;
	goto loop;
}

static void puts(const char *s, int flags, struct tty *ttyp)
{
	register char c;

	while (c = *s++)
		tputchar(c, flags, ttyp);
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
static void printn(u_long n, int b, int flags, struct tty *ttyp, int zf, int fld_size)
{
	char prbuf[11];
	register char *cp;

	if (b == 10 && (int)n < 0) {
		tputchar('-', flags, ttyp);
		n = (unsigned)(-(int)n);
	}
	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	if (fld_size) {
		for (fld_size -= cp - prbuf; fld_size > 0; fld_size--)
			if (zf)
				tputchar('0', flags, ttyp);
			else
				tputchar(' ', flags, ttyp);
	}
	do
		tputchar(*--cp, flags, ttyp);
	while (cp > prbuf);
}

void panic_init(void)
{
  simple_lock_init(&panic_lock);
}

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then reboots.
 * If we are called twice, then we avoid trying to
 * sync the disks as this often leads to recursive panics.
 */
void panic(const char *s)
{
	int bootopt = RB_AUTOBOOT;
#if	NeXT && MACH_LDEBUG
	extern int mach_ldebug;
#endif	NeXT && MACH_LDEBUG
#if	NeXT
	extern struct mon_global *mon_global;
	struct mon_global *mg = mon_global;
#endif	NeXT

	simple_lock(&panic_lock);
	if (panicstr) {
	    if (cpu_number() == paniccpu) {
		bootopt |= RB_NOSYNC;
	    }
	    else {
		simple_unlock(&panic_lock);
		halt_cpu();
		/* NOTREACHED */
	    }
	}
	else {
	    panicstr = s;
	    paniccpu = cpu_number();
	}
	simple_unlock(&panic_lock);

#if	NeXT && MACH_LDEBUG
	mach_ldebug = 0;		// turn off simple lock debugging.
#endif	NeXT && MACH_LDEBUG

	printf("panic: (Cpu %d) %s\n", paniccpu, s);
#if	NeXT
	printf ("NeXT ROM Monitor %d.%d v%d\n",
		mg->mg_major, mg->mg_minor, mg->mg_seq);
	printf("panic: %s\n", version);
#endif	NeXT
#ifdef	vax
	if (boothowto&RB_KDB)
	    asm("bpt");
#endif	vax
#ifdef	sun
	if (boothowto & RB_KDB)
	    asm("trap #15");
#endif	sun
#ifdef	multimax
	if (boothowto&RB_DEBUG)
	    bpt("panic");
#endif	multimax
#if	ROMP_RDB
	Debugger("panic");
	printf("panic: calling boot...");
#endif	ROMP_RDB
#if	BALANCE
	sqtpanic(s);
#endif	BALANCE
#if	NeXT
	mini_mon ("panic", "System Panic", boothowto);
#endif	NeXT
	boot(RB_PANIC, bootopt);
}

/*
 * Warn that a system table is full.
 */
void tablefull(const char *tab)
{

	printf("%s: table is full\n", tab);
	log(LOG_ERR, "%s: table is full\n", tab);
}

/*
 * Hard error is the preface to plaintive error messages
 * about failing disk transfers.
 */
void harderr(struct buf *bp, const char *cp)
{

	printf("%s%d%c: hard error sn%d ", cp,
	    minor(bp->b_dev) >> 3, 'a'+(minor(bp->b_dev)&07), bp->b_blkno);
}

/*
 * Print a character on console or users terminal.
 * If destination is console then the last MSGBUFS characters
 * are saved in msgbuf for inspection later.
 */
int putchar(int c)
{
	tputchar(c, 0, (struct tty *)0);
	return 0;
}

void logchar(int c)
{
	tputchar(c, TOLOG, (struct tty *)0);
}

static void tputchar(int c, int flags, struct tty * tp)
{
	char **sp = (char**) tp;

	if (flags & TOTTY) {
		register s = spltty();

		if (tp && (tp->t_state & (TS_CARR_ON | TS_ISOPEN)) ==
		    (TS_CARR_ON | TS_ISOPEN)) {
			if (c == '\n')
				(void) ttyoutput('\r', tp);
			(void) ttyoutput(c, tp);
			ttstart(tp);
		}
		splx(s);
	}
	if ((flags & TOLOG) && c != '\0' && c != '\r' && c != 0177
#ifdef vax
	    && mfpr(MAPEN)
#endif
	    ) {
#if	mips || NeXT
		if (pmsgbuf->msg_magic != MSG_MAGIC) {
			register int i;

			pmsgbuf->msg_magic = MSG_MAGIC;
			pmsgbuf->msg_bufx = pmsgbuf->msg_bufr = 0;
			for (i=0; i < MSG_BSIZE; i++)
				pmsgbuf->msg_bufc[i] = 0;
		}
		pmsgbuf->msg_bufc[pmsgbuf->msg_bufx++] = c;
		if (pmsgbuf->msg_bufx < 0 || pmsgbuf->msg_bufx >= MSG_BSIZE)
			pmsgbuf->msg_bufx = 0;
#else	mips || NeXT
		if (msgbuf.msg_magic != MSG_MAGIC) {
			register int i;

			msgbuf.msg_magic = MSG_MAGIC;
			msgbuf.msg_bufx = msgbuf.msg_bufr = 0;
			for (i=0; i < MSG_BSIZE; i++)
				msgbuf.msg_bufc[i] = 0;
		}
		msgbuf.msg_bufc[msgbuf.msg_bufx++] = c;
		if (msgbuf.msg_bufx < 0 || msgbuf.msg_bufx >= MSG_BSIZE)
			msgbuf.msg_bufx = 0;
#endif	mips || NeXT
	}
	if ((flags & TOCONS) && c != '\0')
		cnputc(c);
	if (flags & TOSTR) {
		**sp = c;
		(*sp)++;
	}
}

int sprintf(char *s, const char *format, ...)
{
	char *s0 = s;
	prf(format, ((u_int *)&format)+1, TOSTR, (struct tty *)&s);
	*s++ = 0;
	return s - s0;
}














