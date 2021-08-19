SUBDIR=	log_test simple simple_server thread panic
EXDIR=NextDeveloper/Examples/KernelServer

all:

tags clean:
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	done

install:
#	mkdirs $(DSTROOT)$(EXDIR)
#	@for i in ${SUBDIR}; \
#	do \
#		echo ================= make installsrc for $$i =================; \
#		(cd $$i; ${MAKE} installsrc SRCDIR=$(EXDIR)/$$i ) || exit $?; \
#	done

installsrc:  $(SRCDIR)
	cp -p Makefile ${SRCDIR}
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} SRCDIR=$(SRCDIR)/$$i $@) || exit $?; \
	done

$(SRCDIR) $(DSTROOT)$(EXDIR):
	mkdirs $@


