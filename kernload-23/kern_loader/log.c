/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 20-Jul-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import "server.h"
#import "log.h"
#import <stdlib.h>
#import <stddef.h>
#import <stdio.h>
#import <fcntl.h>
#import <stdarg.h>
#import <syslog.h>
#import <sys/ioctl.h>
#import <signal.h>
#import <strings.h>
#import <mach_error.h>
#import <mig_errors.h>
#import "server.h"
#import <kernserv/kern_loader_reply.h>

static const char *name;
queue_head_t port_queue;
typedef struct {
	port_name_t	port;
	queue_chain_t	link;
} log_port_t;

extern boolean_t verbose, debug;

void klinit(const char *server_name)
{
	name = server_name;
	queue_init(&port_queue);
}

void kllog(int priority, const char *message, ...)
{
	va_list ap;
	NXStream *logstream= NXOpenMemory(NULL, 0, NX_READWRITE);

	/*
	 * Format the message.
	 */
	va_start(ap, message);

	if (priority < LOG_NOTICE)
		NXPrintf(logstream, "%s: ", name);

	NXVPrintf(logstream, message, ap);

	va_end(ap);

	kllog_stream(priority, logstream);

	NXCloseMemory(logstream, NX_FREEBUFFER);
}

void kllog_stream(int priority, NXStream *stream)
{
	log_port_t *lp;
	char *buf_start;
	int buf_len, buf_max;

	NXGetMemoryBuffer(stream, &buf_start,
		&buf_len, &buf_max);

	unix_lock();
	/*
	 * Send the thing to syslog.
	 */
	syslog(priority, "%s", buf_start);
	unix_unlock();

	if (!verbose && priority > LOG_INFO)
		return;

	unix_lock();
	/*
	 * Send the thing to syslog.
	 */
	syslog(priority, "%s", buf_start);
	unix_unlock();

	if (!verbose && priority > LOG_INFO)
		return;

	if (debug) {
		unix_lock();
		switch (priority) {
		case LOG_EMERG: fputs("(emerg)\t", stderr); break;
		case LOG_ALERT: fputs("(alert)\t", stderr); break;
		case LOG_CRIT: fputs("(crit)\t", stderr); break;
		case LOG_ERR: fputs("(err)\t", stderr); break;
		case LOG_WARNING: fputs("(warn)\t", stderr); break;
		case LOG_NOTICE: fputs("(notice)\t", stderr); break;
		case LOG_INFO: fputs("(info)\t", stderr); break;
		case LOG_DEBUG: fputs("(debug)\t", stderr); break;
		}
		fputs(buf_start, stderr);
		fflush(stderr);
		unix_unlock();
	}

	/*
	 * Send the message to each port that's logged for receiving messages.
	 */
	for (  lp = (log_port_t *)queue_first(&port_queue)
	     ; !queue_end(&port_queue, (queue_entry_t)lp)
	     ; lp = (log_port_t *)queue_next(&lp->link))
	{
		kern_loader_reply_string(lp->port, buf_start, buf_len,
			priority);
	}
}

void kladdport(port_name_t port)
{
	log_port_t *lp;

	lp = (log_port_t *)malloc(sizeof(*lp));	
	lp->port = port;
	queue_enter(&port_queue, lp, log_port_t *, link);
}

void klremoveport(port_name_t port)
{
	log_port_t *lp;

	for (  lp = (log_port_t *)queue_first(&port_queue)
	     ;    !queue_end(&port_queue, (queue_entry_t)lp)
	       && lp->port != port
	     ; lp = (log_port_t *)queue_next(&lp->link))
		;

	if (queue_end(&port_queue, (queue_entry_t)lp))
		return;

	queue_remove(&port_queue, lp, log_port_t *, link);
	free(lp);
}
