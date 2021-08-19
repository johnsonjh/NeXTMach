NAME=simple
CFILES= simple_server.c simple_handler.c
MIGCFILES= 
MIGINCLUDES= simpleServer.c simple.h
HFILES= simple_types.h
LD=../../cmds/kl_ld/kl_ld

CFLAGS= -g -O -MD -DKERNEL -DKERNEL_FEATURES -DMACH -I../../include

OFILES= simple_server.o simple_handler.o

all:	$(NAME)_reloc

$(NAME)_reloc : $(OFILES) Load_Commands.sect Unload_Commands.sect
	${LD} -n $(NAME) -l Load_Commands.sect -u Unload_Commands.sect \
		-i instance -d $(NAME)_loadable -o $@ $(OFILES)

.c.o:
	$(CC) $(CFLAGS) -c $*.c
	md -u Makedep -d $*.d

simple_handler.o: simpleServer.c

simpleServer.c simple.h: simple.defs 
	mig ${MIGFLAGS} simple.defs

-include Makedep



