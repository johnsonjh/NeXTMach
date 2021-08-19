SUBDIR= kernserv
HFILES=
DSTDIR=/usr/include
MIG_GENERATED=kern_loader.h kern_loader_reply.h \
	kern_server.h kern_server_reply.h
SOURCEFILES=Makefile $(HFILES)
all:

clean:
	rm -f $(MIG_GENERATED)
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	done

installhdrs install:  $(DSTROOT)$(DSTDIR)
	@CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
	for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} DSTROOT=$$DSTROOT $@) || exit $?; \
	done

installsrc:  $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} SRCDIR=$(SRCDIR)/$$i $@) || exit $?; \
	done

$(SRCDIR) $(DSTROOT)$(DSTDIR):
	mkdirs $@

