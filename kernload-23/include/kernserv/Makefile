DSTDIR=/usr/include/kernserv
INSTFLAGS=-c -m 444
HFILES= kern_loader_types.h kern_loader_reply_handler.h kern_loader_error.h \
	kern_server_reply_handler.h

MIG_GENERATED= kern_loader.h kern_loader_reply.h
SOURCEFILES= Makefile $(HFILES) 

all:

clean:
	-rm -f $(MIG_GENERATED)

install: $(DSTROOT)$(DSTDIR)
	for i in $(HFILES) $(MIG_GENERATED); \
	do \
		install ${INSTFLAGS} $$i ${DSTROOT}/${DSTDIR}; \
	done

install:

installsrc: $(SRCDIR)
	cp -p $(SOURCEFILES) $(SRCDIR)

$(SRCDIR) $(DSTROOT)$(DSTDIR):
	mkdirs $@



