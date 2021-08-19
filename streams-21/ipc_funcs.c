#ifdef SHLIB
#include "shlib.h"
#endif
/*
	File:	ipc_funcs.c	
	Author:	Trey Matteson
	
	Copyright (c) 1988 NeXT, Inc. as an unpublished work.
	All rights reserved.
 
 	Streams implemented as IPC operations.
*/

#include "defs.h"
#include <stdlib.h>
#include <mach.h>
#include <sys/message.h>

/* ??? tune these */
#define BUFSIZE	 (4*1024 - 8)		

static int ipc_flush(register NXStream *s);
static int ipc_fill(register NXStream *s);
static void ipc_change(register NXStream *s);
static void ipc_seek(register NXStream *s, register long offset);
static void ipc_close(register NXStream *s);

static const struct stream_functions ipc_functions = {
    NXDefaultRead,
    NXDefaultWrite,
    ipc_flush,
    ipc_fill,
    ipc_change,
    ipc_seek,
    ipc_close,
};

typedef struct {
    msg_header_t header;
    msg_type_long_t type;
    unsigned char data[BUFSIZE];
} InlineMsg;


/*
 *	ipc_flush:  flush a stream buffer.
 */
static int ipc_flush(register NXStream *s)
{
    InlineMsg *msg;
    kern_return_t ret;
    int flushSize;

    flushSize = s->buf_size - s->buf_left;
    if (flushSize) {
	msg = (InlineMsg *)s->info;
	msg->header.msg_size = flushSize + sizeof(msg_header_t) +
					    sizeof(msg_type_long_t);
	msg->type.msg_type_long_number = flushSize;
	ret = msg_send( (msg_header_t *)msg, MSG_OPTION_NONE, 0 );
	if(ret == SEND_SUCCESS) {
	    s->buf_ptr = s->buf_base;
	    s->buf_left = s->buf_size;
	    s->offset += flushSize;
	    return flushSize;
	} else
	    return -1;
    } else
	return 0;
}


/*
 *	ipc_fill:  fill a stream buffer, return how much
 *	we filled.
 */
static int ipc_fill(register NXStream *s)
{
    InlineMsg *msg;
    kern_return_t ret;

    msg = (InlineMsg *)s->info;
    msg->header.msg_size = sizeof(InlineMsg);
    ret = msg_receive((msg_header_t *)msg, MSG_OPTION_NONE, 0);

    if (ret != KERN_SUCCESS)
	return -1;
    else
	return msg->type.msg_type_long_number;
}


static void ipc_change(register NXStream *s)
{
    /* NOP for IPC streams */
}


static void ipc_seek(register NXStream *s, register long offset)
{
    /* NOP for IPC streams */
}


/*
 *	ipc_close:	shut down an ipc stream.
 *
 *	Send a zero length packet to signify end.
 */
static void ipc_close(register NXStream *s)
{
    InlineMsg *msg;

    msg = (InlineMsg *)s->info;
    msg->header.msg_size = sizeof(msg_header_t) + sizeof(msg_type_long_t);
    msg->type.msg_type_long_number = 0;
    (void)msg_send( (msg_header_t *)msg, MSG_OPTION_NONE, 0 );
}


/*
 *	NXOpenPort:
 *
 *	open a stream using a ipc descriptor.
 */
NXStream *
NXOpenPort(port_t port, int mode)
{
    NXStream *s;
    InlineMsg *msg;

/*??? set the flags */
    if (mode == NX_READWRITE)
	return NULL;
    s = NXStreamCreate(mode, FALSE);
    s->functions = &ipc_functions;
    msg = (InlineMsg *)malloc(sizeof(InlineMsg));
    s->info = (char *)msg;
    if (mode == NX_READONLY) {
	msg->header.msg_local_port = port;
	s->buf_left = 0;
    } else {
	msg->header.msg_simple = TRUE;
	msg->header.msg_type = MSG_TYPE_NORMAL;
	msg->header.msg_local_port = PORT_NULL;
	msg->header.msg_remote_port = port;
	msg->type.msg_type_header.msg_type_inline = TRUE;
	msg->type.msg_type_header.msg_type_longform = TRUE;
	msg->type.msg_type_header.msg_type_deallocate = FALSE;
	msg->type.msg_type_long_name = MSG_TYPE_CHAR;
	msg->type.msg_type_long_size = 8;
	s->buf_left = BUFSIZE;
    }
    s->buf_base = s->buf_ptr = msg->data;
    s->buf_size = BUFSIZE;
    s->flags &= ~NX_CANSEEK;
    return s;
}

