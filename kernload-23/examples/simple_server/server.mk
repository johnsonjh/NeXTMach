NAME=simple
CFILES= simple_server.c
MIGCFILES= simpleServer.c
MIGINCLUDES= simpleServer.c simple.h
HFILES= simple_types.h
LD=../../cmds/kl_ld/kl_ld

CFLAGS= -g -O -MD -DKERNEL -DKERNEL_FEATURES -DMACH -I../../include

OFILES= simple_server.o simpleServer.o

all:	$(NAME)_reloc

$(NAME)_reloc : $(OFILES) Load_Commands.sect Unload_Commands.sect
	${LD} -n $(NAME) -l Load_Commands.sect -u Unload_Commands.sect \
		-i instance -o $@ $(OFILES)

.c.o:
	$(CC) $(CFLAGS) -c $*.c
	md -u Makedep -d $*.d

simple_server.o: simple.h

simpleServer.c simple.h: simple.defs 
	mig ${MIGFLAGS} simple.defs

-include Makedep

