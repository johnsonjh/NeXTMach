# 
# Mach Operating System
# Copyright (c) 1990 NeXT, Inc.
#
# HISTORY
#

######################################################################
#
#	Builds RPC interfaces for kernserv functions
#
######################################################################

MIG= mig
MIGOPTS = -I.. -DKERNEL

SERVER_INTERFACE =  kern_server.h kern_server_server.c kern_server_user.h
REPLY_INTERFACE = kern_server_reply.h kern_server_reply_server.c kern_server_reply_user.c

all: ${SERVER_INTERFACE} ${REPLY_INTERFACE}

$(SERVER_INTERFACE):	kern_server.defs
	${MIG} ${MIGOPTS} ${MIGKERNEL} -server kern_server_server.c -header kern_server.h kern_server.defs

$(REPLY_INTERFACE):	kern_server_reply.defs
	${MIG} ${MIGOPTS} ${MIGKERNEL} -user kern_server_reply_user.c -header kern_server_reply.h kern_server_reply.defs
clean:
	rm -f $(SERVER_INTERFACE)
	rm -f $(REPLY_INTERFACE)

