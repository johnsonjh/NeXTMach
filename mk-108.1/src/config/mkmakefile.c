/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 **********************************************************************
 * HISTORY
 * 27-Jul-90  Brian Pinkerton (bpinker) at NeXT
 *	change name of raise(str) to allCaps(str) because of library conflicts
 *
 * 13-Feb-88  John Seamons (jks) at NeXT
 *	New variant of "makeoptions" keyword that allows a simple parameter
 *	(e.g. makeoptions "RELOC=04000000").
 *	NeXT: unlink files before creating to break SGS link.
 *
 * 04-Nov-88  John Seamons (jks) at NeXT
 *	Added machine type NeXT.
 *
 * 27-Oct-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Treat balance drivers like any other driver.
 *
 * 10-Oct-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Define LOAD variable (with all target system names) at end of
 *	%LOAD macro expansion.
 *	[ V5.1(XF18) ]
 *
 * 29-May-87  David Black (dlb) at Carnegie-Mellon University
 *	mmax profiling changes: use -p because -pg doesn't work,
 *		don't define GPROF.EX.
 *
 * 27-Apr-87  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Removed code which knew about RDB and TCC symbols, since
 *	they weren't doing anything useful anymore anyway.
 *
 * 22-Apr-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Reduce minimum users for VAX from 8 to 2.
 *	[ V5.1(F10) ]
 *
 * 27-Mar-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	allow profiling for mmax and sqt
 *
 *  2-Mar-87  Rich Sanzi (sanzi) at Carnegie-Mellon University
 *	Added code in do_machdep() to redefine LIBS when we are building
 *	a profiled kernel.
 *
 * 27-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Make C and S rule change apply for ns32000 only (which includes
 *	Sequent at this time).  This should be done a better way.
 *
 * 27-Feb-87  Rich Sanzi (sanzi) at Carnegie-Mellon University
 *	Added MACHINE_ROMP to types of machines which we can profile.
 *
 * 24-Feb-87  David L. Black (dlb) at Carnegie-Mellon University
 *	Also do C and S rule change of 21-Feb-87 on Multimax (ns32000).
 *
 * 21-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	The name of the Makefile template is now Makefile.template.
 *
 * 21-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Cause C and S rules to be generated on the "same" line... they
 *	are actually on different lines, but use \ at the end to make
 *	think they are on the same line (only done if running on Sequent).
 *
 * 13-Feb-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added leading '#' character comment line capability to
 *	read_files() so that we can start maintaining history logs in
 *	those files also.
 *	[ V5.1(F2) ]
 *
 * 29-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Umax_memconf -> memconf.mmax
 *
 * 19-Jan-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Changed to use ../conf/Makefile as the standard machine
 *	independent template makefile but to then substitute the
 *	contents of ../conf/Makefile.<machinename> for a "%MACHDEP"
 *	(although the size of the machine dependent descriptions is now
 *	typically very small).
 *	[ V5.1(F1) ]
 *
 * 08-Jan-87  Robert Beck (beck) at Sequent Computer Systems, Inc.
 *	Add MACHINE_SQT cases.  SQT assumes "sedit" for now.
 *
 * 16-Nov-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Made a new routine (do_files) which subsumes do_cfiles and
 *	do_sfiles.  This is also now used for handling binary-only files
 *	which have a .b extension and are referenced with BFILES.
 *
 * 13-Nov-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Get gmon.ex in ../machine instead of /usr/src.
 *
 * 27-Oct-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Merged in David Black's changes for the Multimax.
 *
 * 22-Oct-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Merged in Dave Golub's and my changes for the Sun:
 *		1) Added code to allow listing in the "files" file any
 *		   object files that don't have a corresponding source
 *		   file.
 *		2) Changed "do_rules" to produce an action to have the
 *		   preprocessor create an intermediate file to pass to
 *		   the assembler rather than pipe it in because the Sun
 *		   68020 assembler must have an input file.
 *		3) Modified "makefile" to include name of machine in 
 *		   definition of "IDENT".
 *		4) Modified "do_systemspec" to link mbglue.o into vmunix.
 *
 *  4-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Changed all references of CMUCS to CMU.
 *
 *  4-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Change kernel build to not execute the "make Makefile."
 *	Instead, it directly executes the "md" command, saving the time
 *	for make to search the Makefile for the command.
 *
 * 27-Sep-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Removed %MAKEFILE definition (now defined again in
 *	template); changed to generate %RULES in terms of macros
 *	which are also defined in template rather than wiring the
 *	commands in here; changed to generate only minimal load and
 *	swap rules so the details can again all be defined in the
 *	template;  changed to send parameters to a header file
 *	rather than the PARAM variable; fixed copy_dependencies() to
 *	avoid replicating the salutation and blank lines every time.
 *
 *	N.B.  The machine type specific constructs for the SUN and
 *	RT which used to be generated here and were removed have not
 *	been merged into the template makefiles for each of these
 *	machines since there was no simple way to test them on a Vax.
 *	I'll be happy to assist whoever ends up merging them for these
 *	machines.  Also, the various template makefiles need to be
 *	generated from a common master source before much longer but I
 *	don't have any good ideas at the moment.
 *
 * 10-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	We also create an empty "M.n" file to avoid startup errors.
 *
 *  4-Sep-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Eliminated emulate.o if not makeing for a vax.
 *
 * 29-Aug-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added "Makefile" to the list of objects for "all".
 *
 *  6-Aug-86  David Golub (dbg) at Carnegie-Mellon University
 *	added lines for newvers > vers.c.  Also dropped "first" check
 *	   for stepping vers.
 *
 * 10-Jul-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Removed cputype initialization in the IDENT variable.  It is
 *	now sent to a header file instead.
 *
 * 24-Jun-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Fixed not to close a NULL file pointer if no previous
 *	makefile exists when attempting to copy dependencies.
 *
 * 20-Jun-86  Robert V. Baron (rvb) at Carnegie-Mellon University
 *	3# And dependencies from an existing Makefile are appended.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 17-Oct-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	Changed to turn OPTIONS symbols internally into pseudo-device
 *	entries for file lines beginning with "OPTIONS/" so that
 *	include files are generated for these symbols.
 *
 * 12-Aug-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Modified to process files ending in ".s" with the preprocessor
 *	and assembler.
 *
 **********************************************************************
 */

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)mkmakefile.c	5.9 (Berkeley) 5/6/86";
#endif not lint

/*
 * Build the makefile for the system, from
 * the information in the files files and the
 * additional files for the machine being compiled to.
 */

#include <stdio.h>
#include <ctype.h>
#include "y.tab.h"
#include "config.h"

#define next_word(fp, wd) \
	{ register char *word = get_word(fp); \
	  if (word == (char *)EOF) \
		return; \
	  else \
		wd = word; \
	}

static	struct file_list *fcur;
char *tail();
char *allCaps(char *str);

/*
 * Lookup a file, by name.
 */
struct file_list *
fl_lookup(file)
	register char *file;
{
	register struct file_list *fp;

	for (fp = ftab ; fp != 0; fp = fp->f_next) {
		if (eq(fp->f_fn, file))
			return (fp);
	}
	return (0);
}

/*
 * Lookup a file, by final component name.
 */
struct file_list *
fltail_lookup(file)
	register char *file;
{
	register struct file_list *fp;

	for (fp = ftab ; fp != 0; fp = fp->f_next) {
		if (eq(tail(fp->f_fn), tail(file)))
			return (fp);
	}
	return (0);
}

/*
 * Make a new file list entry
 */
struct file_list *
new_fent()
{
	register struct file_list *fp;

	fp = (struct file_list *) malloc(sizeof *fp);
	fp->f_needs = 0;
	fp->f_next = 0;
	fp->f_flags = 0;
	fp->f_type = 0;
	if (fcur == 0)
		fcur = ftab = fp;
	else
		fcur->f_next = fp;
	fcur = fp;
	return (fp);
}

char	*COPTS;
static	struct users {
	int	u_default;
	int	u_min;
	int	u_max;
} users[] = {
#if	CMU
	{ 24, 2, 1024 },		/* MACHINE_VAX */
	{  8, 2, 32 },			/* MACHINE_SUN */
	{ 16, 4, 32 },			/* MACHINE_ROMP */
	{  8, 2, 32 },			/* MACHINE_SUN2 */
	{  8, 2, 32 },			/* MACHINE_SUN3 */
	{ 24, 8, 1024},			/* MACHINE_MMAX */
	{ 32, 8, 1024},			/* MACHINE_SQT */
	{  8, 2, 32 },			/* MACHINE_NeXT */
#else	CMU
	{ 24, 8, 1024 },		/* MACHINE_VAX */
#endif	CMU
};
#define	NUSERS	(sizeof (users) / sizeof (users[0]))

/*
 * Build the makefile from the skeleton
 */
makefile()
{
	FILE *ifp, *ofp;
#if	CMU
	FILE *dfp;
#endif	CMU
	char line[BUFSIZ];
	struct opt *op;
	struct users *up;

	read_files();
#if	CMU
	strcpy(line, "../conf/Makefile.template");
#else	CMU
	strcpy(line, "../conf/Makefile.");
	(void) strcat(line, machinename);
#endif	CMU
	ifp = fopen(line, "r");
	if (ifp == 0) {
		perror(line);
		exit(1);
	}
#if	CMU
	dfp = fopen(path("Makefile"), "r");
	unlink(path("Makefile"));
#if	NeXT
	unlink(path("M.d"));
#endif	NeXT
	if ((ofp = fopen(path("M.d"), "w")) == NULL) {
		perror(path("M.d"));
		/* We'll let this error go */
	}
	else
	 	fclose(ofp);
#endif	CMU
	ofp = fopen(path("Makefile"), "w");
	if (ofp == 0) {
		perror(path("Makefile"));
		exit(1);
	}
#ifdef	CMU
	if (machine == MACHINE_SUN || machine == MACHINE_SUN2 
	    || machine == MACHINE_SUN3 || machine == MACHINE_NeXT)
		fprintf(ofp, "IDENT=-D%s -D%s", machinename, allCaps(ident));
	else
		fprintf(ofp, "IDENT=-D%s", allCaps(ident));
#else	CMU
	fprintf(ofp, "IDENT=-D%s", allCaps(ident));
#endif	CMU
	if (profiling)
		fprintf(ofp, " -DGPROF");
	if (cputype == 0) {
		printf("cpu type must be specified\n");
		exit(1);
	}
#if	CMU
	{ int build_cputypes();
	  do_build("cputypes.h", build_cputypes);
	}
#else	CMU
	{ struct cputype *cp;
	  for (cp = cputype; cp; cp = cp->cpu_next)
		fprintf(ofp, " -D%s", cp->cpu_name);
	}
#endif	CMU
	for (op = opt; op; op = op->op_next)
		if (op->op_value)
			fprintf(ofp, " -D%s=\"%s\"", op->op_name, op->op_value);
		else
			fprintf(ofp, " -D%s", op->op_name);
	fprintf(ofp, "\n");
	if (hadtz == 0)
		printf("timezone not specified; gmt assumed\n");
	if ((unsigned)machine > NUSERS) {
		printf("maxusers config info isn't present, using vax\n");
		up = &users[MACHINE_VAX-1];
	} else
		up = &users[machine-1];
	if (maxusers == 0) {
		printf("maxusers not specified; %d assumed\n", up->u_default);
		maxusers = up->u_default;
	} else if (maxusers < up->u_min) {
		printf("minimum of %d maxusers assumed\n", up->u_min);
		maxusers = up->u_min;
	} else if (maxusers > up->u_max)
		printf("warning: maxusers > %d (%d)\n", up->u_max, maxusers);
#if	CMU
	{ int build_confdep();
	  do_build("confdep.h", build_confdep);
	}
#else	CMU
	fprintf(ofp, "PARAM=-DTIMEZONE=%d -DDST=%d -DMAXUSERS=%d\n",
	    timezone, dst, maxusers);
#endif	CMU
	for (op = mkopt; op; op = op->op_next)
		if (op->op_value)
			fprintf(ofp, "%s=%s\n", op->op_name, op->op_value);
		else
			fprintf(ofp, "%s\n", op->op_name);
	while (fgets(line, BUFSIZ, ifp) != 0) {
		if (*line == '%')
			goto percent;
		if (profiling && strncmp(line, "COPTS=", 6) == 0) {
			register char *cp;
#if	CMU
			if (machine != MACHINE_MMAX)
			    fprintf(ofp,
#if	NeXT
				"GPROF.EX=../machine/gmon.ex\n");
#else	NeXT
				"GPROF.EX=../%s/gmon.ex\n", machinename);
#endif	NeXT
#else	CMU
			fprintf(ofp, 
			    "GPROF.EX=/usr/src/lib/libc/%s/csu/gmon.ex\n",
			    machinename);
#endif	CMU
			cp = index(line, '\n');
			if (cp)
				*cp = 0;
			cp = line + 6;
			while (*cp && (*cp == ' ' || *cp == '\t'))
				cp++;
			COPTS = malloc((unsigned)(strlen(cp) + 1));
			if (COPTS == 0) {
				printf("config: out of memory\n");
				exit(1);
			}
			strcpy(COPTS, cp);
#if	CMU
			if (machine == MACHINE_MMAX)
				fprintf(ofp, "%s -p\n",line);
			else
				fprintf(ofp, "%s -pg\n", line);
#else	CMU
			fprintf(ofp, "%s -pg\n", line);
#endif	CMU
			continue;
		}
		fprintf(ofp, "%s", line);
		continue;
	percent:
		if (eq(line, "%OBJS\n"))
			do_objs(ofp);
		else if (eq(line, "%CFILES\n"))
#if	CMU
			do_files(ofp, "CFILES=", 'c');
		else if (eq(line, "%SFILES\n"))
			do_files(ofp, "SFILES=", 's');
		else if (eq(line, "%BFILES\n"))
			do_files(ofp, "BFILES=", 'b');
		else if (eq(line, "%MACHDEP\n"))
			do_machdep(ofp);
#else	CMU
			do_cfiles(ofp);
#endif	CMU
		else if (eq(line, "%RULES\n"))
			do_rules(ofp);
		else if (eq(line, "%LOAD\n"))
			do_load(ofp);
		else
			fprintf(stderr,
			    "Unknown %% construct in generic makefile: %s",
			    line);
	}
#if	CMU
	if (dfp != NULL)
	{
		copy_dependencies(dfp, ofp);
		(void) fclose(dfp);
	}
#endif	CMU
	(void) fclose(ifp);
	(void) fclose(ofp);
}

/*
 * Read in the information about files used in making the system.
 * Store it in the ftab linked list.
 */
read_files()
{
	FILE *fp;
	register struct file_list *tp, *pf;
	register struct device *dp;
	register struct opt *op;
	char *wd, *this, *needs, *devorprof;
	char fname[32];
#if	CMU
	int options;
	int not_option;
	int fastobj, sedit;		/* SQT */
#endif	CMU
	int nreqs, first = 1, configdep, isdup;

	ftab = 0;
#if	CMU
	(void) strcpy(fname, "../conf/files");
#else	CMU
	(void) strcpy(fname, "files");
#endif	CMU
openit:
	fp = fopen(fname, "r");
	if (fp == 0) {
		perror(fname);
		exit(1);
	}
next:
#if	CMU
	options = 0;
#endif	CMU
	/*
	 * filename	[ standard | optional ] [ config-dependent ]
	 *	[ dev* | profiling-routine ] [ device-driver]
	 */
#if	CMU
	/*
	 * MACHINE_SQT ONLY:
	 *
	 * filename	[ standard | optional ] 
	 *	[ config-dependent | fastobj | sedit ]
	 *	[ dev* | profiling-routine ] [ device-driver]
	 */
#endif	CMU
	wd = get_word(fp);
	if (wd == (char *)EOF) {
		(void) fclose(fp);
		if (first == 1) {
#if	CMU
			(void) sprintf(fname, "../conf/files.%s", machinename);
#else	CMU
			(void) sprintf(fname, "files.%s", machinename);
#endif	CMU
			first++;
			goto openit;
		}
		if (first == 2) {
			(void) sprintf(fname, "files.%s", allCaps(ident));
			first++;
			fp = fopen(fname, "r");
			if (fp != 0)
				goto next;
		}
		return;
	}
	if (wd == 0)
		goto next;
#if	CMU
	/*
	 *  Allow comment lines beginning witha '#' character.
	 */
	if (*wd == '#')
	{
		while ((wd=get_word(fp)) && wd != (char *)EOF)
			;
		goto next;
	}
#endif	CMU
	this = ns(wd);
	next_word(fp, wd);
	if (wd == 0) {
		printf("%s: No type for %s.\n",
		    fname, this);
		exit(1);
	}
	if ((pf = fl_lookup(this)) && (pf->f_type != INVISIBLE || pf->f_flags))
		isdup = 1;
	else
		isdup = 0;
	tp = 0;
	if (first == 3 && (tp = fltail_lookup(this)) != 0)
		printf("%s: Local file %s overrides %s.\n",
		    fname, this, tp->f_fn);
	nreqs = 0;
	devorprof = "";
	configdep = 0;
#if	CMU
	sedit = fastobj = 0;			/* SQT */
	sedit++;				/* SQT: assume sedit for now */
#endif	CMU
	needs = 0;
	if (eq(wd, "standard"))
		goto checkdev;
	if (!eq(wd, "optional")) {
		printf("%s: %s must be optional or standard\n", fname, this);
		exit(1);
	}
#if	CMU
	if (strncmp(this, "OPTIONS/", 8) == 0)
		options++;
	not_option = 0;
#endif	CMU
nextopt:
	next_word(fp, wd);
	if (wd == 0)
		goto doneopt;
	if (eq(wd, "config-dependent")) {
		configdep++;
		goto nextopt;
	}
#if	CMU
	if (machine == MACHINE_SQT && eq(wd, "fastobj")) {
		fastobj++;
		goto nextopt;
	}
	if (machine == MACHINE_SQT && eq(wd, "sedit")) {
		sedit++;
		goto nextopt;
	}
	if (eq(wd, "not")) {
		not_option = !not_option;
		goto nextopt;
	}
#endif	CMU
	devorprof = wd;
	if (eq(wd, "device-driver") || eq(wd, "profiling-routine")) {
		next_word(fp, wd);
		goto save;
	}
	nreqs++;
	if (needs == 0 && nreqs == 1)
		needs = ns(wd);
	if (isdup)
		goto invis;
#if	CMU
	if (options)
	{
		register struct opt *op;
		struct opt *lop = 0;
		struct device tdev;

		/*
		 *  Allocate a pseudo-device entry which we will insert into
		 *  the device list below.  The flags field is set non-zero to
		 *  indicate an internal entry rather than one generated from
		 *  the configuration file.  The slave field is set to define
		 *  the corresponding symbol as 0 should we fail to find the
		 *  option in the option list.
		 */
		init_dev(&tdev);
		tdev.d_name = ns(wd);
		tdev.d_type = PSEUDO_DEVICE;
		tdev.d_flags++;
		tdev.d_slave = 0;

		for (op=opt; op; lop=op, op=op->op_next)
		{
			char *od = allCaps(ns(wd));

			/*
			 *  Found an option which matches the current device
			 *  dependency identifier.  Set the slave field to
			 *  define the option in the header file.
			 */
			if (strcmp(op->op_name, od) == 0)
			{
				tdev.d_slave = 1;
				if (lop == 0)
					opt = op->op_next;
				else
					lop->op_next = op->op_next;
				free(op);
				op = 0;
			 }
			free(od);
			if (op == 0)
				break;
		}
		newdev(&tdev);
	}
 	for (dp = dtab; dp != 0; dp = dp->d_next) {
		if (eq(dp->d_name, wd) && (dp->d_type != PSEUDO_DEVICE || dp->d_slave)) {
			if (not_option)
				goto invis;	/* dont want file if option present */
			else
				goto nextopt;
		}
	}
	if (not_option)
		goto nextopt;		/* want file if option missing */
#else	CMU
	for (dp = dtab; dp != 0; dp = dp->d_next)
		if (eq(dp->d_name, wd))
			goto nextopt;
#endif	CMU
	for (op = opt; op != 0; op = op->op_next)
		if (op->op_value == 0 && opteq(op->op_name, wd)) {
			if (nreqs == 1) {
				free(needs);
				needs = 0;
			}
			goto nextopt;
		}
invis:
	while ((wd = get_word(fp)) != 0)
		;
	if (tp == 0)
		tp = new_fent();
	tp->f_fn = this;
	tp->f_type = INVISIBLE;
	tp->f_needs = needs;
	tp->f_flags = isdup;
	goto next;

doneopt:
	if (nreqs == 0) {
		printf("%s: what is %s optional on?\n",
		    fname, this);
		exit(1);
	}

checkdev:
	if (wd) {
		next_word(fp, wd);
		if (wd) {
			if (eq(wd, "config-dependent")) {
				configdep++;
				goto checkdev;
			}
#if	CMU
			if (machine == MACHINE_SQT && eq(wd, "fastobj")) {
				fastobj++;
				goto checkdev;
			}
			if (machine == MACHINE_SQT && eq(wd, "sedit")) {
				sedit++;
				goto checkdev;
			}
#endif	CMU
			devorprof = wd;
			next_word(fp, wd);
		}
	}

save:
	if (wd) {
		printf("%s: syntax error describing %s\n",
		    fname, this);
		exit(1);
	}
	if (eq(devorprof, "profiling-routine") && profiling == 0)
		goto next;
	if (tp == 0)
		tp = new_fent();
	tp->f_fn = this;
#if	CMU
	if (options)
		tp->f_type = INVISIBLE;
	else
#endif	CMU
	if (eq(devorprof, "device-driver"))
		tp->f_type = DRIVER;
	else if (eq(devorprof, "profiling-routine"))
		tp->f_type = PROFILING;
	else
		tp->f_type = NORMAL;
	tp->f_flags = 0;
	if (configdep)
		tp->f_flags |= CONFIGDEP;
#if	CMU
	if (fastobj)				/* SQT */
		tp->f_flags |= FASTOBJ;
	if (sedit)				/* SQT */
		tp->f_flags |= SEDIT;
#endif	CMU
	tp->f_needs = needs;
	if (pf && pf->f_type == INVISIBLE)
		pf->f_flags = 1;		/* mark as duplicate */
	goto next;
}

opteq(cp, dp)
	char *cp, *dp;
{
	char c, d;

	for (; ; cp++, dp++) {
		if (*cp != *dp) {
			c = isupper(*cp) ? tolower(*cp) : *cp;
			d = isupper(*dp) ? tolower(*dp) : *dp;
			if (c != d)
				return (0);
		}
		if (*cp == 0)
			return (1);
	}
}

do_objs(fp)
	FILE *fp;
{
	register struct file_list *tp, *fl;
	register int lpos, len;
	register char *cp, och, *sp;
	char swapname[32];

	fprintf(fp, "OBJS=");
	lpos = 6;
	for (tp = ftab; tp != 0; tp = tp->f_next) {
		if (tp->f_type == INVISIBLE)
			continue;
#ifdef	CMU
		if (tp->f_flags & FASTOBJ)		/* SQT */
			continue;

		/*
		 *	Check for '.o' file in list
		 */
		cp = tp->f_fn + (len = strlen(tp->f_fn)) - 1;
		if (*cp == 'o') {
			if (len + lpos > 72) {
				lpos = 8;
				fprintf(fp, "\\\n\t");
			}
			fprintf(fp, "../%s ", tp->f_fn);
			lpos += len + 1;
			continue;
		}
#endif	CMU
		/*
		 *	Check for '.o' file in list
		 */
		cp = tp->f_fn + (len = strlen(tp->f_fn)) - 1;
		if (*cp == 'o') {
			if (len + lpos > 72) {
				lpos = 8;
				fprintf(fp, "\\\n\t");
			}
			fprintf(fp, "../%s ", tp->f_fn);
			lpos += len + 1;
			continue;
		}
		sp = tail(tp->f_fn);
		for (fl = conf_list; fl; fl = fl->f_next) {
			if (fl->f_type != SWAPSPEC)
				continue;
			sprintf(swapname, "swap%s.c", fl->f_fn);
			if (eq(sp, swapname))
				goto cont;
		}
		cp = sp + (len = strlen(sp)) - 1;
		och = *cp;
		*cp = 'o';
		if (len + lpos > 72) {
			lpos = 8;
			fprintf(fp, "\\\n\t");
		}
		fprintf(fp, "%s ", sp);
		lpos += len + 1;
		*cp = och;
cont:
		;
	}
	if (lpos != 8)
		putc('\n', fp);
}

#if	CMU
do_fastobjs(fp)
	FILE *fp;
{
	register struct file_list *tp, *fl;
	register int lpos, len;
	register char *cp, och, *sp;
	char swapname[32];

	fprintf(fp, "FASTOBJS=");
	lpos = 10;
	for (tp = ftab; tp != 0; tp = tp->f_next) {
		if ((tp->f_flags & FASTOBJ) != FASTOBJ)
			continue;
		sp = tail(tp->f_fn);
		cp = sp + (len = strlen(sp)) - 1;
		och = *cp;
		*cp = 'o';
		if (len + lpos > 72) {
			lpos = 8;
			fprintf(fp, "\\\n\t");
		}
		fprintf(fp, "%s ", sp);
		lpos += len + 1;
		*cp = och;
cont:
		;
	}
	if (lpos != 8)
		putc('\n', fp);
}

do_files(fp, msg, ext)
	FILE	*fp;
	char	*msg;
	char	ext;
#else	CMU
do_cfiles(fp)
	FILE *fp;
#endif	CMU
{
	register struct file_list *tp;
	register int lpos, len;

#if	CMU
	fprintf(fp, msg);
#else	CMU
	fprintf(fp, "CFILES=");
#endif	CMU
	lpos = 8;
	for (tp = ftab; tp != 0; tp = tp->f_next) {
		if (tp->f_type == INVISIBLE)
			continue;
#if	CMU
		if (tp->f_fn[strlen(tp->f_fn)-1] != ext)
#else	CMU
		if (tp->f_fn[strlen(tp->f_fn)-1] != 'c')
#endif	CMU
			continue;
		if ((len = 3 + strlen(tp->f_fn)) + lpos > 72) {
			lpos = 8;
			fprintf(fp, "\\\n\t");
		}
		fprintf(fp, "../%s ", tp->f_fn);
		lpos += len + 1;
	}
	if (lpos != 8)
		putc('\n', fp);
}

#if	CMU
/*
 *  Include machine dependent makefile in output
 */

do_machdep(ofp)
	FILE *ofp;
{
	int c;
	FILE *ifp;
	char line[BUFSIZ];

	(void) sprintf(line, "../conf/Makefile.%s", machinename);
	ifp = fopen(line, "r");
	if (ifp == 0) {
		perror(line);
		exit(1);
	}
	while (fgets(line, BUFSIZ, ifp) != 0) {
		if (machine == MACHINE_SQT && eq(line, "%FASTOBJS\n"))
			do_fastobjs(ofp);
		else {
			if (profiling && (strncmp(line, "LIBS=", 5) == 0)) 
				fprintf(ofp,"LIBS=${LIBS_P}\n");
			else fputs(line, ofp);
		}
	}
	fclose(ifp);
}



/*
 *  Format configuration dependent parameter file.
 */

build_confdep(fp)
	FILE *fp;
{
	fprintf(fp,
	    "#define TIMEZONE %d\n#define MAXUSERS %d\n#define DST %d\n",
	    timezone, maxusers, dst);
}



/*
 *  Format cpu types file.
 */

build_cputypes(fp)
	FILE *fp;
{
	struct cputype *cp;

	for (cp = cputype; cp; cp = cp->cpu_next)
		fprintf(fp, "#define\t%s\t1\n", cp->cpu_name);
}



/*
 *  Build a define parameter file.  Create it first in a temporary location and
 *  determine if this new contents differs from the old before actually
 *  replacing the original (so as not to introduce avoidable extraneous
 *  compilations).
 */

do_build(name, format)
	char *name;
	int (*format)();
{
	static char temp[]="#config.tmp";
	FILE *tfp, *ofp;
	int c;

#if	NeXT
	unlink(path(temp));
#endif	NeXT
	tfp = fopen(path(temp), "w+");
	if (tfp == 0) {
		perror(path(temp));
		exit(1);
	}
	unlink(path(temp));
	(*format)(tfp);
	ofp = fopen(path(name), "r");
	if (ofp != 0)
	{
		fseek(tfp, 0, 0);
		while ((c = fgetc(tfp)) != EOF)
			if (fgetc(ofp) != c)
				goto copy;
		if (fgetc(ofp) == EOF)
			goto same;
		
	}
copy:
	if (ofp)
		fclose(ofp);
#if	NeXT
	unlink(path(name));
#endif	NeXT
	ofp = fopen(path(name), "w");
	if (ofp == 0) {
		perror(path(name));
		exit(1);
	}
	fseek(tfp, 0, 0);
	while ((c = fgetc(tfp)) != EOF)
		fputc(c, ofp);
same:
	fclose(ofp);
	fclose(tfp);
}

#endif CMU
char *
tail(fn)
	char *fn;
{
	register char *cp;

	cp = rindex(fn, '/');
	if (cp == 0)
		return (fn);
	return (cp+1);
}

/*
 * Create the makerules for each file
 * which is part of the system.
 * Devices are processed with the special c2 option -i
 * which avoids any problem areas with i/o addressing
 * (e.g. for the VAX); assembler files are processed by as.
 */
do_rules(f)
	FILE *f;
{
	register char *cp, *np, och, *tp;
	register struct file_list *ftp;
	char *extras;
#if	CMU
#ifdef	ns32000
	char *nl = "; \\";
#else	ns32000
	char *nl = "";
#endif	ns32000
#endif	CMU

for (ftp = ftab; ftp != 0; ftp = ftp->f_next) {
	if (ftp->f_type == INVISIBLE)
		continue;
	cp = (np = ftp->f_fn) + strlen(ftp->f_fn) - 1;
	och = *cp;
#if	CMU
	/*
	 *	Don't compile '.o' files
	 */
	if (och == 'o')
		continue;
#endif	CMU
	*cp = '\0';
	if (och == 'o') {
#if	CMU
		fprintf(f, "%so: %so\n\t${O_RULE_1A}../%.*s${O_RULE_1B}\n\n",
			   tail(np), np, tp-np, np);
#else	CMU
		fprintf(f, "%so:\n\t-cp ../%so .\n", tail(np), np);
#endif	CMU
		continue;
	}
	fprintf(f, "%so: ../%s%c\n", tail(np), np, och);
	tp = tail(np);
	if (och == 's') {
#if	CMU
		fprintf(f, "\t${S_RULE_1A}../%.*s${S_RULE_1B}%s\n", tp-np,
							np, nl);
		fprintf(f, "\t${S_RULE_2}%s\n", nl);
		fprintf(f, "\t${S_RULE_3}\n\n");
#else	CMU
		fprintf(f, "\t-ln -s ../%ss %sc\n", np, tp);
		fprintf(f, "\t${CC} -E ${COPTS} %sc | ${AS} -o %so\n",
			tp, tp);
		fprintf(f, "\trm -f %sc\n\n", tp);
#endif	CMU
		continue;
	}
#if	CMU
	if (och == 'b') {
		fprintf(f, "\t${B_RULE_1A}../%.*s${B_RULE_1B}\n\n", tp-np, np);
		continue;
	}
#endif	CMU
	if (ftp->f_flags & CONFIGDEP)
#if	CMU
	{
	    fprintf(stderr,
	     "config: %s%c: \"config-dependent\" obsolete; include \"confdep.h\" instead\n", np, och);
	}
#else	CMU
		extras = "${PARAM} ";
#endif	CMU
	else
		extras = "";
	switch (ftp->f_type) {

	case NORMAL:
		switch (machine) {

#if	CMU
#if	0
		case MACHINE_SQT:
			if (ftp->f_flags & SEDIT) {
				fprintf(f, "\t${CC} -SO ${COPTS} %s../%sc | \\\n", extras, np);
				fprintf(f, "\t${SEDCMD} | ${C2} | ${AS} ${CAFLAGS} -o %so\n\n", tp);
			} else {
				fprintf(f, "\t${CC} -c -O ${COPTS} %s../%sc\n\n",
					extras, np);
			}
			break;
#endif	0
		default:
			goto common;
#else	CMU
		case MACHINE_VAX:
			fprintf(f, "\t${CC} -c -S ${COPTS} %s../%sc\n",
				extras, np);
			fprintf(f, "\t${C2} %ss | ${INLINE} | ${AS} -o %so\n",
			    tp, tp);
			fprintf(f, "\trm -f %ss\n\n", tp);
			break;
#endif	CMU
		}
		break;

	case DRIVER:
		switch (machine) {

#if	CMU
		default:
			extras = "_D";
			goto common;
#else	CMU
		case MACHINE_VAX:
			fprintf(f, "\t${CC} -c -S ${COPTS} %s../%sc\n",
				extras, np);
			fprintf(f,"\t${C2} -i %ss | ${INLINE} | ${AS} -o %so\n",
			    tp, tp);
			fprintf(f, "\trm -f %ss\n\n", tp);
			break;
#endif	CMU
		}
		break;

	case PROFILING:
		if (!profiling)
			continue;
		if (COPTS == 0) {
			fprintf(stderr,
			    "config: COPTS undefined in generic makefile");
			COPTS = "";
		}
#if	CMU
		if ((machine != MACHINE_VAX) && (machine != MACHINE_ROMP) &&
		    (machine != MACHINE_SQT) && (machine != MACHINE_MMAX) &&
		    (machine != MACHINE_NeXT))
		{
			fprintf(stderr,
			    "config: don't know how to profile kernel on this cpu\n");
			break;
		}
		extras = "_P";
		goto common;

	common:
		fprintf(f, "\t${C_RULE_1A%s}../%.*s${C_RULE_1B%s}%s\n",
			   extras, (tp-np), np, extras, nl);
		fprintf(f, "\t${C_RULE_2%s}%s\n", extras, nl);
		fprintf(f, "\t${C_RULE_3%s}%s\n", extras, nl);
		fprintf(f, "\t${C_RULE_4%s}\n\n", extras);
		break;
#else	CMU
		switch (machine) {

		case MACHINE_VAX:
			fprintf(f, "\t${CC} -c -S %s %s../%sc\n",
				COPTS, extras, np);
			fprintf(f, "\tex - %ss < ${GPROF.EX}\n", tp);
			fprintf(f, "\t${INLINE} %ss | ${AS} -o %so\n", tp, tp);
			fprintf(f, "\trm -f %ss\n\n", tp);
			break;
		}
		break;
#endif	CMU

	default:
		printf("Don't know rules for %s\n", np);
		break;
	}
	*cp = och;
}
}

/*
 * Create the load strings
 */
do_load(f)
	register FILE *f;
{
	register struct file_list *fl;
	int first = 1;
	struct file_list *do_systemspec();

	fl = conf_list;
	while (fl) {
		if (fl->f_type != SYSTEMSPEC) {
			fl = fl->f_next;
			continue;
		}
		fl = do_systemspec(f, fl, first);
		if (first)
			first = 0;
	}
#if	CMU
	fprintf(f, "LOAD =");
#else	CMU
	fprintf(f, "all:");
#endif	CMU
	for (fl = conf_list; fl != 0; fl = fl->f_next)
		if (fl->f_type == SYSTEMSPEC)
			fprintf(f, " %s", fl->f_needs);
#if	CMU
	fprintf(f, "\n\nall: ${LOAD} Makefile\n");
#endif	CMU
	fprintf(f, "\n");
}

struct file_list *
do_systemspec(f, fl, first)
	FILE *f;
	register struct file_list *fl;
	int first;
{

#if	CMU
	fprintf(f, "%s: %s.sys\n", fl->f_needs, fl->f_needs);
	fprintf(f, "\t${SYS_RULE_1}\n");
	fprintf(f, "\t${SYS_RULE_2}\n");
	fprintf(f, "\t${SYS_RULE_3}\n");
	fprintf(f, "\t${SYS_RULE_4}\n\n");
	do_swapspec(f, fl->f_fn, fl->f_needs);
	for (fl = fl->f_next; fl != NULL && fl->f_type == SWAPSPEC; fl = fl->f_next)
#else	CMU
	fprintf(f, "%s: Makefile", fl->f_needs);
	fprintf(f, " locore.o emulate.o ${OBJS} param.o ioconf.o swap%s.o\n",
	    fl->f_fn);
	fprintf(f, "\t@echo loading %s\n\t@rm -f %s\n",
	    fl->f_needs, fl->f_needs);
	if (first) {
		fprintf(f, "\t@sh ../conf/newvers.sh\n");
		fprintf(f, "\t@${CC} $(CFLAGS) -c vers.c\n");
	}
	switch (machine) {

	case MACHINE_VAX:
		fprintf(f, "\t@${LD} -n -o %s -e start -x -T 80000000 ",
			fl->f_needs);
		break;
	}
	fprintf(f, "locore.o emulate.o ${OBJS} vers.o ioconf.o param.o ");
	fprintf(f, "swap%s.o\n", fl->f_fn);
	fprintf(f, "\t@echo rearranging symbols\n");
	fprintf(f, "\t@-symorder ../%s/symbols.sort %s\n",
	    machinename, fl->f_needs);
	fprintf(f, "\t@size %s\n", fl->f_needs);
	fprintf(f, "\t@chmod 755 %s\n\n", fl->f_needs);
	do_swapspec(f, fl->f_fn);
	for (fl = fl->f_next; fl->f_type == SWAPSPEC; fl = fl->f_next)
#endif	CMU
		;
	return (fl);
}

#if	CMU
do_swapspec(f, name, system)
	char *system;
#else	CMU
do_swapspec(f, name)
#endif	CMU
	FILE *f;
	register char *name;
{

#if	CMU
	char *gdir = eq(name, "generic")?"../machine/":"";

	fprintf(f, "%s.swap:${P} ${LDOBJS} swap%s.o ${LDDEPS}\n",
		   system, name);
#if	NeXT
	fprintf(f, "\t@rm -f $@\n");
#endif	NeXT
	fprintf(f, "\t@cp swap%s.o $@\n\n", name);
	fprintf(f, "swap%s.o: %sswap%s.c ${SWAPDEPS}\n", name, gdir, name);
	fprintf(f, "\t${C_RULE_1A}%s${C_RULE_1B}\n", gdir);
	fprintf(f, "\t${C_RULE_2}\n");
	fprintf(f, "\t${C_RULE_3}\n");
	fprintf(f, "\t${C_RULE_4}\n\n");
#else	CMU
	if (!eq(name, "generic")) {
		fprintf(f, "swap%s.o: swap%s.c\n", name, name);
		fprintf(f, "\t${CC} -c -O ${COPTS} swap%s.c\n\n", name);
		return;
	}
	fprintf(f, "swapgeneric.o: ../%s/swapgeneric.c\n", machinename);
	switch (machine) {

	case MACHINE_VAX:
		fprintf(f, "\t${CC} -c -S ${COPTS} ");
		fprintf(f, "../%s/swapgeneric.c\n", machinename);
		fprintf(f, "\t${C2} swapgeneric.s | ${INLINE}");
		fprintf(f, " | ${AS} -o swapgeneric.o\n");
		fprintf(f, "\trm -f swapgeneric.s\n\n");
		break;
	}
#endif	CMU
}

char *
allCaps(str)
	register char *str;
{
	register char *cp = str;

	while (*str) {
		if (islower(*str))
			*str = toupper(*str);
		str++;
	}
	return (cp);
}
#if	CMU

#define OLDSALUTATION "# DO NOT DELETE THIS LINE"

#define LINESIZE 1024
static char makbuf[LINESIZE];		/* one line buffer for makefile */

copy_dependencies(makin, makout)
register FILE *makin, *makout;
{
	register int oldlen = (sizeof OLDSALUTATION - 1);

	while (fgets(makbuf, LINESIZE, makin) != NULL) {
		if (! strncmp(makbuf, OLDSALUTATION, oldlen))
			break;
	}
	while (fgets(makbuf, LINESIZE, makin) != NULL) {
		if (oldlen != 0)
		{
			if (makbuf[0] == '\n')
				continue;
			else
				oldlen = 0;
		}
		fputs(makbuf, makout);
	}
}
#endif	CMU
