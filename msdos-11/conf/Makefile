#
# makefile for msdos config directory
#
SOURCEFILES=Makefile.master config.c Makefile Makefile.DEBUG Makefile.RELEASE files.msdos Makefile.srcdir Load_Commands.sect Unload_Commands.sect

all install: 	RELEASE

RELEASE:	config ../RELEASE/Makefile

DEBUG:		config ../DEBUG/Makefile

clean:
	rm -rf config
	
config: config.c
	cc -g -o config config.c

../RELEASE/Makefile:	files.msdos Makefile.master Makefile.RELEASE Load_Commands.sect Unload_Commands.sect
		./config RELEASE
	
../DEBUG/Makefile:	files.msdos Makefile.master Makefile.DEBUG Load_Commands.sect Unload_Commands.sect
		./config DEBUG
	
installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

installhdrs:

$(SRCDIR):
	mkdirs $@
		