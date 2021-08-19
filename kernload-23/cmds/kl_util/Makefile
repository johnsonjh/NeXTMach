PROJECT=kl_util
CFILES=$(PROJECT).c
OFILES= $(CFILES:.c=.o)
DSTDIR=/usr/etc
DEBUG=
SOURCEFILES= Makefile $(CFILES) $(HFILES)
LIBS=../../libkernload/libkernload.a -lsys_s

CFLAGS= -g -O -MD -Wimplicit -I../../include

all:	$(PROJECT)

install: $(DSTROOT)$(DSTDIR) $(PROJECT)
	install -s $(PROJECT) $(DSTROOT)$(DSTDIR)

installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

clean:
	-rm -f $(OFILES) $(PROJECT) $(MIGCFILES) vers.? *.d Makedep

$(DSTROOT)$(DSTDIR) $(SRCDIR):
	mkdirs $@

$(PROJECT): $(OFILES) vers.o
	${CC} ${CFLAGS} -o $@ $(OFILES) vers.o ${LIBS}

$(OFILES): $$(@:.o=.c)
	$(CC) $(CFLAGS) -c $*.c
	md -u Makedep -d $*.d

vers.c: $(OFILES)
	@rm -f $@ $*.d
	vers_string -c $(PROJECT) > $@

-include Makedep
