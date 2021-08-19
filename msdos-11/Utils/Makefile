# 
# msdos Utilities
#
 
CFILES=lfs.c rd.c
HFILES=
SOURCEFILES= Makefile $(CFILES) 
OFILES= $(CFILES:.c=.o)
LD=/bin/ld

CFLAGS=-g -O $(DEBUG) -MD -Wimplicit -DMACH -DNeXT_MOD -I.. -I../header

all debug:	lfs rd

installhdrs:

install:
	
installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

clean:
	-rm -f $(OFILES) lfs rd Makedep

$(SRCDIR):
	mkdirs $@

lfs:	 	lfs.o
		${CC} ${CFLAGS} -o $@ $@.o

rd:	 	rd.o
		${CC} ${CFLAGS} -o $@ $@.o

$(OFILES): $$(@:.o=.c)
	$(CC) $(CFLAGS) -c -o $@ $*.c
	md -u Makedep -d $*.d

-include Makedep


