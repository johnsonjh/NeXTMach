/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 *  9-Jan-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import "mach.h"
#import "async.h"
#import <libc.h>
#import <cthreads.h>
#import <kernserv/kern_server_reply_handler.h>
#import "kern_loader_handler.h"

extern kern_server_reply_t kern_server_reply;
extern kern_loader_t kern_loader;

typedef struct {
	msg_header_t		*m;
	kern_server_reply_t	r;
} handler_call_reply_t;

typedef struct {
	msg_header_t	*m;
} handler_call_user_t;

static void handler_call_reply(handler_call_reply_t *h)
{
	kern_server_reply_handler(h->m, &h->r);
	free(h->m);
	free(h);
}

void handler_fork_reply(msg_header_t *msg, void *s)
{
	handler_call_reply_t *h = malloc(sizeof(*h));
	h->m = (msg_header_t *)malloc(msg->msg_size);
	bcopy((char *)msg, (char *)h->m, msg->msg_size);
	h->r = kern_server_reply;
	h->r.arg = s;

	cthread_detach(cthread_fork((cthread_fn_t)handler_call_reply, h));
}

static void handler_call_user(handler_call_user_t *h)
{
	kern_loader_handler(h->m, &kern_loader);
	free(h->m);
	free(h);
}

void handler_fork_user(msg_header_t *msg)
{
	handler_call_user_t *h = malloc(sizeof(*h));
	h->m = (msg_header_t *)malloc(msg->msg_size);
	bcopy((char *)msg, (char *)h->m, msg->msg_size);

	cthread_detach(cthread_fork((cthread_fn_t)handler_call_user, h));
}
