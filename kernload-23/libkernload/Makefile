#
# Makefile for the kern_loader interface library
#

NAME= kernload
DSTDIR= /usr/lib/
KERN_SERVER_DEFS=/usr/include/kernserv/kern_server.defs
#KERN_SERVER_REPLY_DEFS=/usr/include/kernserv/kern_server_reply.defs
KERN_SERVER_REPLY_DEFS=/usr/include/kernserv/kern_server_reply.defs

CFILES= kern_loader_reply_handler.c kern_loader_error.c kern_loader_look_up.c \
	kern_server_reply_handler.c
MIGCFILES= kern_loaderUser.c kern_serverUser.c
MIGINCLUDES= kern_loader_replyServer.c kern_server_replyServer.c
MIGJUNK= kern_server_replyUser.c kern_serverServer.c \
	kern_server.h kern_server_reply.h

SOURCEFILES= Makefile $(CFILES)

CFLAGS= -g -O -MD -Wall -DMACH -I../include
ARFLAGS= ruv
IFLAGS= -m 644
OFILE_DIR= obj
VPATH = $(OFILE_DIR)
OFILES= $(CFILES:.c=.o) $(MIGCFILES:.c=.o)

all:	$(NAME)

$(NAME): lib$(NAME).a

profile:
	make lib$(NAME)_p.a  "OFILE_DIR = prof_obj"  \
				"CFLAGS = -pg $(CFLAGS)"

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $(OFILE_DIR)/$*.o
	md -u Makedep -d $*.d

kern_loader_reply_handler.o:kern_loader_replyServer.c

kern_loaderUser.c: ../mig/kern_loaderUser.c
	@rm -f $@
	cp ../mig/$@ $@

../mig/kern_loaderUser.c: ../mig/kern_loader.defs
	cd ../mig; make

kern_loader_replyServer.c: ../mig/kern_loader_replyServer.c
	@rm -f $@
	cp ../mig/$@ $@

../mig/kern_loader_replyServer.c: ../mig/kern_loader_reply.defs
	cd ../mig; make

kern_server_reply_handler.o: kern_server_replyServer.c

kern_serverUser.c kern_server.h: $(KERN_SERVER_DEFS)
	mig $(KERN_SERVER_DEFS)

kern_server_replyServer.c kern_server_reply.h: $(KERN_SERVER_REPLY_DEFS)
	mig $(KERN_SERVER_REPLY_DEFS)

installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

lib$(NAME).a.stripped: lib$(NAME).a
	striplib lib$(NAME).a
	mv lib$(NAME).a lib$(NAME).a.stripped
	mv lib$(NAME).a.unstripped lib$(NAME).a

install: $(DSTROOT)$(DSTDIR) lib$(NAME).a.stripped
	install $(IFLAGS) lib$(NAME).a.stripped $(DSTROOT)$(DSTDIR)/lib$(NAME).a

$(OFILE_DIR) $(DSTROOT)$(DSTDIR) $(SRCDIR):
	mkdirs $@

vers.o: $(OFILES)
	@rm -f $(OFILE_DIR)/$*.c
	vers_string -l lib$(NAME).a > $(OFILE_DIR)/$*.c
	$(CC) -c $(OFILE_DIR)/$*.c -o $(OFILE_DIR)/$*.o

clean:
	-rm -rf $(OFILES) $(MIGCFILES) $(MIGINCLUDES) $(MIGJUNK) \
		*obj Makedep *.d \
		lib$(NAME)*.a lib$(NAME)*.a.stripped

lib$(NAME).a lib$(NAME)_p.a: $(OFILE_DIR) $(OFILES) vers.o
	@rm -f $@
	$(AR) $(ARFLAGS) $@ $(OFILES) vers.o
	ranlib $@

-include Makedep
