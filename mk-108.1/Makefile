RELNOTESDEST= /usr/local/RelNotes
RELNOTESSOURCE= KernelNotes.rtf
BOMDEST = /usr/local/BOMs
USERBOMSOURCE = mk.user_bom

SUBDIR=	cmds src stand fpsp conf
KERNELS= DEBUG RELEASE
SRCFILES= Makefile ${RELNOTESSOURCE} ${USERBOMSOURCE} \
	bin bsd cmds conf kern kernserv krpc machine mon net \
	netimp netinet netns next nextdev nextif nfs rpc rpcsvc specfs \
	src stand sys ufs uxkern vm fpsp

all clean depend tags:
	 @for i in ${SUBDIR}; \
	 do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	 done

installhdrs:
	echo ================= make $@  =================
	cd conf; ${MAKE} DSTROOT=$$DSTROOT installhdrs

install:	DSTROOT installrelnotes installuserbom
	@CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
	for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} DSTROOT=$$DSTROOT $@) || exit $?; \
	done

accessories:
	 @for i in cmds src stand fpsp; \
	 do \
		echo ================= make all for $$i =================; \
		(cd $$i; ${MAKE} all) || exit $?; \
	 done

kernels:	accessories ${KERNELS}

${KERNELS}:	ALWAYS
	echo ================= make $@ kernel =================
	(cd conf; ${MAKE} ../$@/Makefile)
	(cd $@; ${MAKE} rebuild)


installsrc:	SRCROOT
	tar cf - ${SRCFILES} | (cd ${SRCROOT}; tar xfBp -)

installrelnotes: DSTROOT ${RELNOTESSOURCE}
	mkdirs ${DSTROOT}${RELNOTESDEST}
	cp -p ${RELNOTESSOURCE} ${DSTROOT}${RELNOTESDEST}

installuserbom: DSTROOT ${USERBOMSOURCE}
	mkdirs ${DSTROOT}${BOMDEST}
	cp -p ${USERBOMSOURCE} ${DSTROOT}${BOMDEST}


SRCROOT DSTROOT:
	@if [ -n "${$@}" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi

ALWAYS:
