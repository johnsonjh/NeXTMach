NAME=panic
CFILES= panic_server.c
MIGCFILES=panicServer.c
MIGINCLUDES=panic.h
LD=../../cmds/kl_ld/kl_ld

CFLAGS= -g -O -MD -DKERNEL -DKERNEL_FEATURES -DMACH -I../../include

OFILES= $(CFILES:.c=.o) $(MIGCFILES:.c=.o)

all:	$(NAME)_reloc

$(NAME)_reloc : $(OFILES) Load_Commands.sect Unload_Commands.sect
	${LD} -n $(NAME) -l Load_Commands.sect -u Unload_Commands.sect \
		-i instance -d $(NAME)_loadable -o $@ $(OFILES)

.c.o:
	$(CC) $(CFLAGS) -c $*.c
	md -u Makedep -d $*.d

panicServer.c panic.h: panic.defs 
	mig ${MIGFLAGS} panic.defs

panic_server.o: panic.h

-include Makedep



