/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 **********************************************************************
 * HISTORY
 * 13-Feb-88  John Seamons (jks) at NeXT
 *	NeXT: unlink files before creation to break SGS link.
 *
 * 07-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Flushed partial support for creating the features.h file since
 *	it only half worked and the Makefile accomplishes the same task
 *	more correctly.
 *	[ V5.1(F5) ]
 *
 * 05-Jun-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Merged in changes for Sun 2 and 3.
 *
 *  4-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Changed all references of CMUCS to CMU.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 25-Jul-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	Fixed bug in tomacro() which neglected to verify a lower case
 *	character before converting to upper case.
 *
 **********************************************************************
 */

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)mkheaders.c	5.1 (Berkeley) 5/8/85";
#endif not lint

/*
 * Make all the .h files for the optional entries
 */

#include <stdio.h>
#include <ctype.h>
#include "config.h"
#include "y.tab.h"

#if	CMU
extern char	*toheader();
#endif	CMU
headers()
{
	register struct file_list *fl;

	for (fl = ftab; fl != 0; fl = fl->f_next)
		if (fl->f_needs != 0)
			do_count(fl->f_needs, fl->f_needs, 1);
}

/*
 * count all the devices of a certain type and recurse to count
 * whatever the device is connected to
 */
do_count(dev, hname, search)
	register char *dev, *hname;
	int search;
{
	register struct device *dp, *mp;
	register int count;

	for (count = 0,dp = dtab; dp != 0; dp = dp->d_next)
		if (dp->d_unit != -1 && eq(dp->d_name, dev)) {
#ifdef	CMU
			/*
			 * Avoid making .h files for bus types on sun machines
			 */
			if ((machine == MACHINE_SUN2 ||
					machine == MACHINE_SUN3)
			    && dp->d_conn == TO_NEXUS){
				return;
			}
#endif	CMU
			if (dp->d_type == PSEUDO_DEVICE) {
				count =
				    dp->d_slave != UNKNOWN ? dp->d_slave : 1;
#if	CMU
				if (dp->d_flags)
					dev = NULL;
#endif	CMU
				break;
			}
#ifdef	CMU
                        if (machine != MACHINE_SUN2 && machine != MACHINE_SUN3)
				/* avoid ie0,ie0,ie1 setting NIE to 3 */
#endif	CMU
			count++;
			/*
			 * Allow holes in unit numbering,
			 * assumption is unit numbering starts
			 * at zero.
			 */
			if (dp->d_unit + 1 > count)
				count = dp->d_unit + 1;
			if (search) {
				mp = dp->d_conn;
#ifdef	CMU
                                if (mp != 0 && mp != TO_NEXUS &&
				    mp->d_conn != TO_NEXUS) {
                                        /*
					 * Check for the case of the
					 * controller that the device
					 * is attached to is in a separate
					 * file (e.g. "sd" and "sc").
					 * In this case, do NOT define
					 * the number of controllers
					 * in the hname .h file.
					 */
					if (!file_needed(mp->d_name))
#else	CMU
				if (mp != 0 && mp != (struct device *)-1 &&
				    mp->d_conn != (struct device *)-1) {
#endif	CMU
					do_count(mp->d_name, hname, 0);
					search = 0;
				}
			}
		}
	do_header(dev, hname, count);
}

#ifdef	CMU
/*
 * Scan the file list to see if name is needed to bring in a file.
 */
file_needed(name)
	char *name;
{
	register struct file_list *fl;

	for (fl = ftab; fl != 0; fl = fl->f_next) {
		if (fl->f_needs && strcmp(fl->f_needs, name) == 0)
			return (1);
	}
	return (0);
}
#endif	CMU

do_header(dev, hname, count)
	char *dev, *hname;
	int count;
{
	char *file, *name, *inw, *toheader(), *tomacro();
	struct file_list *fl, *fl_head;
	FILE *inf, *outf;
	int inc, oldcount;

	file = toheader(hname);
#if	CMU
	name = tomacro(dev?dev:hname) + (dev == NULL);
#else	CMU
	name = tomacro(dev);
#endif	CMU
	inf = fopen(file, "r");
	oldcount = -1;
	if (inf == 0) {
#if	NeXT
		unlink(file);
#endif	NeXT
		outf = fopen(file, "w");
		if (outf == 0) {
			perror(file);
			exit(1);
		}
		fprintf(outf, "#define %s %d\n", name, count);
		(void) fclose(outf);
		return;
	}
	fl_head = 0;
	for (;;) {
		char *cp;
		if ((inw = get_word(inf)) == 0 || inw == (char *)EOF)
			break;
		if ((inw = get_word(inf)) == 0 || inw == (char *)EOF)
			break;
		inw = ns(inw);
		cp = get_word(inf);
		if (cp == 0 || cp == (char *)EOF)
			break;
		inc = atoi(cp);
		if (eq(inw, name)) {
			oldcount = inc;
			inc = count;
		}
		cp = get_word(inf);
		if (cp == (char *)EOF)
			break;
		fl = (struct file_list *) malloc(sizeof *fl);
		fl->f_fn = inw;
		fl->f_type = inc;
		fl->f_next = fl_head;
		fl_head = fl;
	}
	(void) fclose(inf);
	if (count == oldcount) {
		for (fl = fl_head; fl != 0; fl = fl->f_next)
			free((char *)fl);
		return;
	}
	if (oldcount == -1) {
		fl = (struct file_list *) malloc(sizeof *fl);
		fl->f_fn = name;
		fl->f_type = count;
		fl->f_next = fl_head;
		fl_head = fl;
	}
#if	NeXT
	unlink(file);
#endif	NeXT
	outf = fopen(file, "w");
	if (outf == 0) {
		perror(file);
		exit(1);
	}
	for (fl = fl_head; fl != 0; fl = fl->f_next) {
		fprintf(outf, "#define %s %d\n",
		    fl->f_fn, count ? fl->f_type : 0);
		free((char *)fl);
	}
	(void) fclose(outf);
}

/*
 * convert a dev name to a .h file name
 */
char *
toheader(dev)
	char *dev;
{
	static char hbuf[80];

	(void) strcpy(hbuf, path(dev));
	(void) strcat(hbuf, ".h");
	return (hbuf);
}

/*
 * convert a dev name to a macro name
 */
char *tomacro(dev)
	register char *dev;
{
	static char mbuf[20];
	register char *cp;

	cp = mbuf;
	*cp++ = 'N';
	while (*dev)
#if	CMU
		if (!islower(*dev))
			*cp++ = *dev++;
		else
#endif	CMU
		*cp++ = toupper(*dev++);
	*cp++ = 0;
	return (mbuf);
}
