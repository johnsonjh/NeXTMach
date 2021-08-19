#if	CMU
/* 
 **********************************************************************
 * Mach Operating System
 * Copyright (c) 1986 Carnegie-Mellon University
 *  
 * This software was developed by the Mach operating system
 * project at Carnegie-Mellon University's Department of Computer
 * Science. Software contributors as of May 1986 include Mike Accetta, 
 * Robert Baron, William Bolosky, Jonathan Chew, David Golub, 
 * Glenn Marcy, Richard Rashid, Avie Tevanian and Michael Young. 
 * 
 * Some software in these files are derived from sources other
 * than CMU.  Previous copyright and other source notices are
 * preserved below and permission to use such software is
 * dependent on licenses from those institutions.
 * 
 * Permission to use the CMU portion of this software for 
 * any non-commercial research and development purpose is
 * granted with the understanding that appropriate credit
 * will be given to CMU, the Mach project and its authors.
 * The Mach project would appreciate being notified of any
 * modifications and of redistribution of this software so that
 * bug fixes and enhancements may be distributed to users.
 *
 * All other rights are reserved to Carnegie-Mellon University.
 **********************************************************************
 * HISTORY
 * 17-May-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded previous changes to 4.3.
 *
 * 01-Jul-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_CONS:  hooked character input/output through optional
 *	virtual console.
 *
 **********************************************************************
 */

#endif	CMU
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)prf.c	7.1 (Berkeley) 6/5/86
 */

#import <sys/param.h>
#import <next/printf.h>
#import <mon/global.h>
#import <stand/varargs.h>

/*VARARGS1*/
sprintf(bp, fmt, va_alist)
	char *bp, *fmt;
	va_dcl
{
	va_list ap;

	va_start(ap);
	prf(fmt, bp, ap);
	va_end(ap);
}

/*VARARGS1*/
printf(fmt, x1)
	char *fmt;
	unsigned x1;
{

	prf(fmt, &x1);
}

prf(fmt, adx)
	register char *fmt;
	u_int *adx;
{
	register int b, c, i;
	char *s;
	int any;
loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			return;
		putchar(c);
	}
again:
	c = *fmt++;
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
		printn((u_long)*adx, b);
		break;
	case 'c':
		b = *adx;
		for (i = 24; i >= 0; i -= 8)
			if (c = (b >> i) & 0x7f)
				putchar(c);
		break;
	case 'b':
		b = *adx++;
		s = (char *)*adx;
		printn((u_long)b, *s++);
		any = 0;
		if (b) {
			while (i = *s++) {
				if (b & (1 << (i-1))) {
					putchar(any? ',' : '<');
					any = 1;
					for (; (c = *s) > 32; s++)
						putchar(c);
				} else
					for (; *s > 32; s++)
						;
			}
			if (any)
				putchar('>');
		}
		break;

	case 's':
		s = (char *)*adx;
		while (c = *s++)
			putchar(c);
		break;

	case '%':
		putchar('%');
		break;

	case 'C':
		b = *adx;
		for (i = 24; i >= 0; i -= 8)
			if (c = (b >> i) & 0xff)
				putchar(c);
		break;

	case 'r':
	case 'R':
		b = *adx++;
		s = (char *)*adx;
		if (c == 'R') {
			puts("0x");
			printn((u_long)b, 16);
		}
		any = 0;
		if (c == 'r' || b) {
			register struct reg_desc *rd;
			register struct reg_values *rv;
			unsigned field;

			putchar('<');
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
					putchar(',');
				if (rd->rd_name) {
					if (rd->rd_format || rd->rd_values
					    || field) {
						puts(rd->rd_name);
						any = 1;
					}
					if (rd->rd_format || rd->rd_values) {
						putchar('=');
						any = 1;
					}
				}
				if (rd->rd_format) {
					prf(rd->rd_format, &field);
					any = 1;
					if (rd->rd_values)
						putchar(':');
				}
				if (rd->rd_values) {
					any = 1;
					for (rv = rd->rd_values;
					    rv->rv_name;
					    rv++) {
						if (field == rv->rv_value) {
							puts(rv->rv_name);
							break;
						}
					}
					if (rv->rv_name == NULL)
						puts("???");
				}
			}
			putchar('>');
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
					puts(rv->rv_name);
					break;
				}
			}
			if (rv->rv_name == NULL)
				puts("???");
			if (c == 'N' || rv->rv_name == NULL) {
				putchar(':');
				printn((u_long)b, 10);
			}
		}
		break;

	}
	adx++;
	goto loop;
}

puts(s)
register char *s;
{
	register char c;

	while (c = *s++)
		putchar(c);
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
printn(n, b)
	u_long n;
{
	char prbuf[11];
	register char *cp;

	if (b == 10 && (int)n < 0) {
		putchar('-');
		n = (unsigned)(-(int)n);
	}
	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	do
		putchar(*--cp);
	while (cp > prbuf);
}

/*
 * Print a character on console.
 */
putchar(c)
{
	struct mon_global *mg = (struct mon_global*) restore_mg();

	mg->mg_putc (c);
	if (c == '\n')
		putchar('\r');
}

getchar()
{
	register c;
	struct mon_global *mg = (struct mon_global*) restore_mg();

	c = mg->mg_getc();
	if (c == '\r')
		c = '\n';
	putchar(c);
	return(c);
}

alert (s, p1, p2, p3, p4, p5, p6, p7, p8)
	char *s;
{
	struct mon_global *mg = (struct mon_global*) restore_mg();
	extern char line[];

	printf ("%s: ", line);
	printf (s, p1, p2, p3, p4, p5, p6, p7, p8);
#ifdef	notdef
	mg->mg_alert_confirm();		/* abandon alert scheme for now */
#endif
	_exit();
}

alert_msg (s, p1, p2, p3, p4, p5, p6, p7, p8)
	char *s;
{
	struct mon_global *mg = (struct mon_global*) restore_mg();

	printf (s, p1, p2, p3, p4, p5, p6, p7, p8);
}

gets(buf)
	char *buf;
{
	register char *lp;
	register c;
	struct mon_global *mg = (struct mon_global*) restore_mg();

	lp = buf;
	for (;;) {
		c = mg->mg_getc() & 0177;
		switch(c) {
		case '\n':
		case '\r':
			c = '\n';
			*lp++ = '\0';
			putchar (c);
			return;
		case '\b':
		case '\177':
			if (lp > buf) {
				lp--;
				printf ("\b \b");
			}
			continue;
		case '#':
			lp--;
			if (lp < buf)
				lp = buf;
			continue;
		case '@':
		case 'u'&037:
			lp = buf;
			putchar('\n');
			continue;
		default:
			*lp++ = c;
			putchar (c);
		}
	}
}
