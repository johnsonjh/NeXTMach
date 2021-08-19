#
# Makefile for the generic kernel server

SUBDIR= kern

NAME= loadserv
DSTDIR= /usr/lib/
LCLDIR= /usr/local/lib/

CFILES= xpr_stub.c
SOURCEFILES= Makefile $(CFILES) $(HFILES)

CFLAGS= -g -O -MD -Wimplicit -DKERNEL -DMACH -DKERNEL_FEATURES -I../include
ARFLAGS= ruv
IFLAGS= -m 644
OFILE_DIR= obj
VPATH = $(OFILE_DIR)
OFILES= $(CFILES:.c=.o) $(MIGCFILES:.c=.o)

all:	$(NAME) debug

debug: $(SUBDIR)
	make lib$(NAME)_g.a  "OFILE_DIR = debug_obj"  \
		"CFLAGS = -DDEBUG -DXPR_DEBUG $(CFLAGS)"

meas: $(SUBDIR)
	make lib$(NAME)_m.a  "OFILE_DIR = meas_obj"  \
		"CFLAGS = -DIPLMEAS -DDEBUG -DXPR_DEBUG $(CFLAGS)"

$(NAME): kern lib$(NAME).a

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $(OFILE_DIR)/$*.o
	md -u Makedep -d $*.d

installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} SRCDIR=$(SRCDIR)/$$i $@) || exit $?; \
	done

lib$(NAME).a.stripped: $(NAME)
	striplib lib$(NAME).a
	mv lib$(NAME).a lib$(NAME).a.stripped
	mv lib$(NAME).a.unstripped lib$(NAME).a

install: $(DSTROOT)$(DSTDIR) $(DSTROOT)$(LCLDIR) lib$(NAME).a.stripped debug
	install $(IFLAGS) lib$(NAME).a.stripped $(DSTROOT)$(DSTDIR)/lib$(NAME).a
	install $(IFLAGS) lib$(NAME)_g.a $(DSTROOT)$(LCLDIR)

$(OFILE_DIR) $(DSTROOT)$(DSTDIR) $(DSTROOT)$(LCLDIR) $(SRCDIR):
	mkdirs $@

vers.o: $(OFILES)
	@rm -f $(OFILE_DIR)/$*.c
	vers_string -l lib$(NAME).a > $(OFILE_DIR)/$*.c
	$(CC) -c $(OFILE_DIR)/$*.c -o $(OFILE_DIR)/$*.o

clean:
	-rm -rf $(OFILES) $(MIGCFILES) $(MIGCINCLUDES) *obj Makedep *.d \
		lib$(NAME)*.a*
	@for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} SRCDIR=$(SRCDIR)/$$i $@) || exit $?; \
	done

$(SUBDIR):	FRC
	@for i in ${SUBDIR}; \
	do \
		echo ================= make all for $$i =================; \
		(cd $$i; ${MAKE} all) || exit $?; \
	done

lib$(NAME).a: $(OFILE_DIR) $(OFILES) vers.o kern/*.o
	@rm -f lib$(NAME).a
	$(AR) $(ARFLAGS) lib$(NAME).a $(OFILES) kern/*.o vers.o
	ranlib lib$(NAME).a

lib$(NAME)_g.a: $(OFILE_DIR) $(OFILES) vers.o kern/*.o
	@rm -f lib$(NAME)_g.a
	$(AR) $(ARFLAGS) lib$(NAME)_g.a $(OFILES) kern/*.o vers.o
	ranlib lib$(NAME)_g.a

lib$(NAME)_m.a: $(OFILE_DIR) $(OFILES) vers.o kern/*.o
	@rm -f lib$(NAME)_m.a
	$(AR) $(ARFLAGS) lib$(NAME)_m.a $(OFILES) kern/*.o vers.o
	ranlib lib$(NAME)_m.a

FRC:

-include Makedep
