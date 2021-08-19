######################################################################
# HISTORY
# 09-May-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Replaced archive with NEWSRC rule for new archiving mechanism.
#	[ V5.1(XF11) ]
#
#  6-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
#	Removed md... this is a standard Mach utility and not part of
#	the kernel sources proper.
#
# 28-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Removed obsolete references to "as".
#	[ V5.1(F8) ]
#
# 25-Feb-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Removed assumption of uk,uk as owner of installed files.
#	[ V5.1(F3) ]
#
# 13-Feb-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Added "archive" rule to check-in all modified sources.
#	[ V5.1(F2) ]
#
# 09-Dec-86  Mike Accetta (mja) at Carnegie-Mellon University
#	Created.
#	[ V5.1(F1) ]
#
######################################################################
#

.SUFFIXES: .csh .c .x .install

DESTDIR=
CFLAGS=	-O
INSTALL=install -q
DDIR=	${DESTDIR}../bin

BASEDIR=

SUBDIR=	config 
ISUBDIR= config.install

ALL=
IALL=	arch.install check_cond.install doconf.install \
	domerge.install hinstall.install remerge.install prune.install

all:	${ALL} ${SUBDIR}

install: ${IALL} ${ISUBDIR}

${SUBDIR}: FRC
	cd $@; make ${MFLAGS}

${ISUBDIR}: FRC
	cd $*; make ${MFLAGS} DESTDIR=${DESTDIR} install

FRC:

.c.x:	$*.c
	${CC} ${CFLAGS} -o $*.x $*.c

.csh.install: $*.csh
	-rm -f $(DDIR)/$*
	${INSTALL} -xc $*.csh ${DDIR}/$*

.x.install: $*.x
	-rm -f ${DDIR}/$*
	${INSTALL} -s $*.x ${DDIR}/$*

lclean:
	rm -f *.o *.x
	rm -f ../bin/config

clean: lclean
	@for i in ${SUBDIR}; do echo "[ $$i ]";(cd $$i; make clean); done


#  Display any newly modified source files (e.g. for archive)

NEWSRC=.

NEWSRC: ${NEWSRC}

${NEWSRC}: Makefile *.csh
	@echo $?
