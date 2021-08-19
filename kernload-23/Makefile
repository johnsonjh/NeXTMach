RELNOTESDIR=/usr/local/RelNotes/
RELNOTES=KernloadNotes.rtf
SUBDIR=	mig libkernload kern_loader loaded_server cmds include examples
BOMDIR=/usr/local/BOMs
USERBOM=kernload.user_bom
SOURCES= Makefile ${RELNOTES} ${USERBOM}

all tags clean:
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	done

installhdrs: DSTROOT ${DSTROOT}
	echo ================= make $@  =================
	(cd mig; ${MAKE} all)
	(cd include; ${MAKE} DSTROOT=$$DSTROOT install)

install: DSTROOT ${DSTROOT} ${DSTROOT}${RELNOTESDIR} ${DSTROOT}${BOMDIR}
	install -c -m 444 ${RELNOTES} ${DSTROOT}${RELNOTESDIR}${RELNOTES}
	install -c -m 444 ${USERBOM} ${DSTROOT}${BOMDIR}
	@CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
	for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} DSTROOT=$$DSTROOT $@) || exit $?; \
	done

installsrc: SRCROOT ${SRCROOT}
	-rm -rf ${SRCROOT}/*
	cp -p ${SOURCES} ${SRCROOT}
	@CWD=`pwd`; cd ${SRCROOT}; SRCROOT=`pwd`; cd $$CWD; \
	for i in ${SUBDIR}; \
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

$(SRCROOT) $(DSTROOT) ${DSTROOT}${RELNOTESDIR} ${DSTROOT}${BOMDIR}:
	mkdirs $@


