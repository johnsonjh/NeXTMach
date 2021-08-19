SUBDIR=	kl_log kl_util kl_ld

all tags clean install:
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	done

installhdrs install:

installsrc: ${SRCDIR}
	cp -p Makefile ${SRCDIR}
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} SRCDIR=$(SRCDIR)/$$i $@) || exit $?; \
	done

$(SRCDIR):
	mkdirs $@

