# 
#	top-level Makefile for dosfs project
#
BUILDDIRS=RELEASE
OBJDIRS= conf DOS.util Utils
SOURCEDIRS= block drobj header lowl portme usr util nextdos
INSTALLDIRS=  $(OBJDIRS) $(BUILDDIRS)
SUBDIRS= ${OBJDIRS} ${SOURCEDIRS}
RELNOTESDIR=/usr/local/RelNotes
RELNOTES=DosfsNotes.wn

all debug:
	@for i in ${INSTALLDIRS}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	done

clean:
	rm -rf $(BUILDDIRS) DEBUG
	@for i in ${OBJDIRS}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	done

install: DSTROOT ${DSTROOT} ${RELNOTESDIR}
	cp -p ${RELNOTES} ${RELNOTESDIR}
	chmod 444 ${RELNOTESDIR}/${RELNOTES}
	@CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
	for i in ${INSTALLDIRS}; \
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

