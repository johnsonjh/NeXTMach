PROJECT=kl_ld
DSTDIR=/usr/bin
SOURCEFILES= Makefile $(PROJECT)

all:

install: $(DSTROOT)$(DSTDIR) $(PROJECT)
	install -m 755 -c $(PROJECT) $(DSTROOT)$(DSTDIR)

installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

clean:
	-rm -f $(OFILES)

$(DSTROOT)$(DSTDIR) $(SRCDIR):
	mkdirs $@
