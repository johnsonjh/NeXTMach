NAME=panic
SECTFILES=Load_Commands.sect Unload_Commands.sect
CFILES= panic.c panic_server.c
MIGFILES= panic.defs
MIGCFILES= panic User.c 
MIGINCLUDES= panicServer.c panic.h
HFILES=
SOURCEFILES= Makefile $(CFILES) $(SECTFILES) $(HFILES) \
	README server.mk user.mk $(MIGFILES)

all:	$(NAME)_reloc $(NAME)

installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

$(SRCDIR):
	mkdirs $@

clean:
	-rm -rf $(NAME) $(NAME)_reloc $(NAME)_loadable $(SOFILES) $(UOFILES) \
		$(MIGCFILES) $(MIGINCLUDES) *.d *.o Makedep

$(NAME)_reloc: FRC
	$(MAKE) -f server.mk $(NAME)_reloc

$(NAME): FRC
	$(MAKE) -f user.mk $(NAME)

FRC:

