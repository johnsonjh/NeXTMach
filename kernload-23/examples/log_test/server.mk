NAME=log
CFILES= log_server.c
MIGCFILES=logServer.c
MIGINCLUDES=log.h
HFILES= log_types.h
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

logServer.c log.h: log.defs 
	mig ${MIGFLAGS} log.defs

log_server.o: log.h

-include Makedep



