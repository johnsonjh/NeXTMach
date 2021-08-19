/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 * 13-Apr-90  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef	_KERNLOAD_LOG_
#define	_KERNLOAD_LOG_
#import <streams/streams.h>

/*
 * Kern loader message logging interface.
 */
void klinit(const char *server_name);
void kllog(int priority, const char *message, ...);
void kllog_stream(int priority, NXStream *stream);
void kladdport(port_name_t port);
void klremoveport(port_name_t port);

#endif	_KERNLOAD_LOG_

