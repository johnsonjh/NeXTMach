# 
#	top-level Makefile for CDROM project
#
SUBDIRS= cfs CDROM.util
RELNOTESDIR=/usr/local/RelNotes
RELNOTES=CdromNotes.wn

all debug clean:
	@for i in ${SUBDIRS}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	done

install: DSTROOT ${DSTROOT} ${RELNOTESDIR}
	cp -p ${RELNOTES} ${RELNOTESDIR}
	chmod 444 ${RELNOTESDIR}/${RELNOTES}
	@CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
	for i in ${SUBDIRS}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} DSTROOT=$$DSTROOT $@) || exit $?; \
	done

installsrc: SRCROOT ${SRCROOT}
	-rm -rf ${SRCROOT}/*
	cp -p Makefile ${SRCROOT}
	cp -p ${RELNOTES} ${SRCROOT}
	@CWD=`pwd`; cd ${SRCROOT}; SRCROOT=`pwd`; cd $$CWD; \
	for i in ${SUBDIRS}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} SRCDIR=$$SRCROOT/$$i $@) || exit $?; \
	done

SRCROOT DSTROOT:
	@if [ -n "${$@}" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi

$(SRCROOT) $(DSTROOT) $(RELNOTESDIR):
	mkdirs $@

