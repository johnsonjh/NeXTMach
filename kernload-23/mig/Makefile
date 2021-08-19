#
# Makefile for the generic kernel server
#

INCDIR= ../include
KERNSERV= $(INCDIR)/kernserv
MIDI= $(INCDIR)/midi
MIG= mig

MIGFILES=kern_loader.defs kern_loader_reply.defs
LHFILES= $(MIGFILES:.defs=.h)
HFILES=	$(INCDIR)/kern_loader.h $(INCDIR)/kernserv/kern_loader.h \
	$(INCDIR)/kern_loader_reply.h $(INCDIR)/kernserv/kern_loader_reply.h

MIGFLAGS= -I$(INCDIR)
SOURCEFILES= Makefile $(MIGFILES)
.SUFFIXES: .defs .h

all:	$(HFILES)

$(HFILES): $(LHFILES)
	@rm -f $@
	cp -p $*.h $@

.defs.h:
	${MIG} ${MIGFLAGS} $<

clean:
	rm -f *User.c *Server.c *.h

install: all

installsrc: $(SRCDIR)
	cp -p $(SOURCEFILES) $(SRCDIR)

$(SRCDIR):
	mkdirs $(SRCDIR)

