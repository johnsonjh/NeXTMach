NAME=log
SECTFILES=Load_Commands.sect Unload_Commands.sect
CFILES= log.c log_server.c
MIGFILES= log.defs
MIGCFILES= logUser.c 
MIGINCLUDES= logServer.c log.h
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

