# 
# DOS.util Makefile
#
 
CFILES=DOS.util.c kl_com.c
HFILES=DOS.util.h
SOURCEFILES= Makefile $(CFILES) $(HFILES) DOS.name DOS.fs.tiff DOS.openfs.tiff
OFILES= $(CFILES:.c=.o)
# keep DSTDIR up to date with FS_DIR_LOCATION in 
#	/usr/include/next/loadable_fs.h
DSTDIR=/usr/filesystems/DOS.fs
INSTALL_BIN=DOS.util 
INSTALL_RAW=DOS.name DOS.fs.tiff DOS.openfs.tiff
LD=/bin/ld
DEBUG=

CFLAGS=-g -O $(DEBUG) -MD -Wimplicit -DMACH -DNeXT_MOD -lsys_s -I.. -I../header

all debug:	DOS.util

installhdrs:

install: $(DSTROOT)$(DSTDIR) $(INSTALL_BIN) $(INSTALL_RAW)
	install -s -m 04755 $(INSTALL_BIN) $(DSTROOT)$(DSTDIR)
#	chmod 4755 $(DSTROOT)$(DSTDIR)/$(INSTALLBIN)
	tar cf - $(INSTALL_RAW) | (cd $(DSTROOT)$(DSTDIR); tar xpf -)
	
installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

clean:
	-rm -f $(OFILES) $(MIGCFILES) Makedep

$(SRCDIR) $(DSTROOT)$(DSTDIR):
	mkdirs $@

DOS.util: 	DOS.util.o kl_com.o
		${CC} -o $@ DOS.util.o kl_com.o -lsys_s -lm -lkernload 
		
$(OFILES): $$(@:.o=.c)
	$(CC) $(CFLAGS) -c -o $@ $*.c
	md -u Makedep -d $*.d

-include Makedep


