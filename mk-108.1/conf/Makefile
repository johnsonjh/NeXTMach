BUILDDIR= RELEASE

all depend tags mach mach.strip: builddir
	cd ../$(BUILDDIR); ${MAKE} $@

clean:
	-rm -rf ../$(BUILDDIR) $(BUILDDIR)

builddir: ../$(BUILDDIR)/Makefile

debug:
	${MAKE} BUILDDIR=DEBUG mach.strip

profile:
	doconf -cpu next $(BUILDDIR)
	mv $(BUILDDIR) PROFILE
	-mkdir ../PROFILE
	config -p PROFILE
	${MAKE} BUILDDIR=PROFILE mach.strip

config:
	cd ../src/config; make install

../$(BUILDDIR)/Makefile: MASTER.next Makefile.template Makefile.NeXT \
		files.NeXT files config
	-rm -f $(BUILDDIR) $(BUILDDIR).old ../$(BUILDDIR)/Makefile
	./doconf -c -cpu next $(BUILDDIR)

../$(BUILDDIR)/mach: builddir
	cd ../$(BUILDDIR); ${MAKE} all

install: builddir
	cd ../$(BUILDDIR); ${MAKE} DSTROOT=$$DSTROOT install

installhdrs: builddir
	cd ../$(BUILDDIR);  ${MAKE} DSTROOT=$$DSTROOT installhdrs

