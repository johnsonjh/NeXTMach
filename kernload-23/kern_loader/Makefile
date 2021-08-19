DSTDIR=/usr/etc
CFILES= async.c kern_loader.c kern_loader_handler.c \
	load_cmds.c log.c misc.c obj.c \
	server.c
MIGCFILES= kern_loader_replyUser.c
MIGINCLUDES= kern_loaderServer.c
OFILES= $(CFILES:.c=.o) $(MIGCFILES:.c=.o)
HFILES= async.h kern_loader_handler.h load_cmds.h log.h misc.h obj.h server.h
SOURCEFILES= Makefile $(CFILES) $(HFILES)
LIBS=-lsys_s ../libkernload/libkernload.a
OFILE_DIR= obj
VPATH = $(OFILE_DIR)

PROJECT=kern_loader

CFLAGS= -g -O -MD -Wall -I../include -DMACH

all:	$(PROJECT) #debug

debug:
	make $(PROJECT)_g  "OFILE_DIR = debug_obj"  \
		"CFLAGS = $(CFLAGS) -DDEBUG"

install: $(DSTROOT)$(DSTDIR) $(PROJECT)
	install -s $(PROJECT) $(DSTROOT)$(DSTDIR)

installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

clean:
	-rm -rf $(OFILES) $(PROJECT) $(PROJECT)_g \
		$(MIGCFILES) $(MIGINCLUDES) $(MIGJUNK) *.d Makedep *obj

$(OFILE_DIR) $(DSTROOT)$(DSTDIR) $(SRCDIR):
	mkdirs $@

$(PROJECT) $(PROJECT)_g: $(OFILE_DIR) $(OFILES) vers.o
	${CC} ${CFLAGS} -o $@ $(OFILES) vers.o ${LIBS}

$(OFILES): $$(@:.o=.c)
	$(CC) $(CFLAGS) -c $*.c -o $(OFILE_DIR)/$*.o
	md -u Makedep -d $*.d

vers.o: $(OFILES)
	@rm -f $(OFILE_DIR)/$*.c
	vers_string -c $(PROJECT) > $(OFILE_DIR)/$*.c
	$(CC) -c $(OFILE_DIR)/$*.c -o $(OFILE_DIR)/$*.o

kern_loader_handler.o: kern_loaderServer.c

kern_loaderServer.c: ../mig/kern_loaderServer.c
	@rm -f $@
	cp ../mig/$@ $@

../mig/kern_loaderServer.c: ../mig/kern_loader.defs
	cd ../mig; make

kern_loader_replyUser.c: ../mig/kern_loader_replyUser.c
	@rm -f $@
	cp ../mig/$@ $@

../mig/kern_loader_replyUser.c: ../mig/kern_loader_reply.defs
	cd ../mig; make

-include Makedep
