######################################################################
# HISTORY
# 09-May-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Replaced archive with NEWSRC rule for new archiving mechanism.
#	[ V5.1(XF11) ]
#
# 19-Jan-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Added "archive" rule to check-in all modified sources.
#	[ V5.1(F1) ]
#
# 08-Jan-87  Robert Beck (beck) at Sequent Computer Systems, Inc.
#	Mod config.x rule to take $P (for parallel make).
#
# 09-Dec-86  Mike Accetta (mja) at Carnegie-Mellon University
#	Install in bin/config with the other kernel maintenance
#	programs.
#
# 31-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
#	Build config.x instead of config.  Make install move config.x to
#	config (../../conf/config is a symbolic link to here).
#
# 19-Jul-86  Mike Accetta (mja) at Carnegie-Mellon University
#	Added -q to install and moved destination from /etc to conf.
#
######################################################################
#
# Copyright (c) 1980 Regents of the University of California.
# All rights reserved.  The Berkeley software License Agreement
# specifies the terms and conditions for redistribution.
#
#	@(#)Makefile	5.3 (Berkeley) 2/18/86
#
#

CFILES_SRC =	main.c mkioconf.c mkmakefile.c \
		mkglue.c mkheaders.c mkswapconf.c
CFILES=	y.tab.c lex.yy.c ${CFILES_SRC}
OBJS=	y.tab.o main.o lex.yy.o mkioconf.o mkmakefile.o \
	mkglue.o mkheaders.o mkswapconf.o

CFLAGS=-O -bsd -DCMU -I../..
LDFLAGS=-ll

all:	config.x install

config.x:$P ${OBJS}
	${CC} ${OBJS} -o config.x ${LDFLAGS}

lex.yy.c: config.l
	lex config.l

y.tab.h y.tab.c: config.y
	yacc -d config.y

clean:
	rm -f ${OBJS} lex.yy.c y.tab.c y.tab.h config.x

install: config.x
	-rm -f ../../bin/config
	install -q -s config.x ../../bin/config

lint: ${CFILES}
	lint ${CFILES}


#  Display any newly modified source files (e.g. for archive)

NEWSRC=.

NEWSRC: ${NEWSRC}

${NEWSRC}: Makefile config.h ${CFILES_SRC}
	@echo $?

	
depend:
	grep '^#include' ${CFILES} | grep -v '<' | \
	sed -e 's/:[^"]*"\([^"]*\)".*/: \1/' \
	    -e 's/\.c/.o/' \
	    -e 's,../[a-zA-Z]*/,,' | \
	awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$2) > 78) { print rec; rec = $$0; } \
		       else rec = rec " " $$2 } } \
	      END { print rec } ' > makedep
	echo '$$r makedep' >>eddep
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	ed - Makefile < eddep
	rm eddep makedep

# DO NOT DELETE THIS LINE -- make depend uses it

main.o: y.tab.h config.h
mkioconf.o: y.tab.h config.h
mkmakefile.o: y.tab.h config.h
mkglue.o: config.h y.tab.h
mkheaders.o: config.h y.tab.h
mkswapconf.o: config.h
