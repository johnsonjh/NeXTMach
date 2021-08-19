NAME=panic
CFILES= panic.c
MIGCFILES= panicUser.c 
MIGINCLUDES= panic.h

CFLAGS= -g -O -MD -DMACH -I../../include
OFILES= $(CFILES:.c=.o) $(MIGCFILES:.c=.o)


all:	$(NAME)

$(NAME): $(OFILES)
	cc -o $(CFLAGES) $(NAME) $(OFILES)

.c.o:
	$(CC) $(CFLAGS) -c $*.c
	md -u Makedep -d $*.d

panicUser.c panic.h: panic.defs 
	mig ${MIGFLAGS} panic.defs

-include Makedep


