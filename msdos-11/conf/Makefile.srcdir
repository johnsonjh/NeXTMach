#
#	Makefile for msdos source directories. All of the "source only" 
#	directories have a soft link to this as their Makefile.
#

all installhdrs install clean:

installsrc: $(SRCDIR)
	tar cf - *.[ch] | (cd $(SRCDIR); tar xpf -)
	(cd $(SRCDIR); ln -s ../conf/Makefile.srcdir Makefile)
	
$(SRCDIR):
	mkdirs $@
