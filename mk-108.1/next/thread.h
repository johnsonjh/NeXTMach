/* 
 * Copyright (c) 1987 NeXT, Inc.
 */ 

#ifndef	_NeXT_THREAD_
#define	_NeXT_THREAD_

#if	FIXME
/*
 *	The current_thread is saved in the lower 24 bits of the
 *	68030  master stack pointer, so signifiy that there is a
 *	machine specific way to get this.
 *	FIXME: do we have to use inline to get this?
 */
#define CURRENT_THREAD
#define	current_thread
#endif	FIXME

#endif	_NeXT_THREAD_
