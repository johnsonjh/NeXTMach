#pragma CC_NO_MACH_TEXT_SECTIONS

#ifdef SHLIB
#include "shlib.h"
#endif
/*
    globals.c

    This file has the stream globals.

    Copyright (c) 1988 NeXT, Inc. as an unpublished work.
    All rights reserved.
*/

#import <stddef.h>

/*
 * The size of this first pad was changed from 256 to 252 to make room for the
 * 4 byte text symbol ('GMT') in ctime_data.o from the libc project for which
 * it and this file both belong to the shared libsys.  This change was made in
 * response to using the new NeXT Mach-O link editor.  See the comments in
 * libc/gen/ctime_data.c for how and why this got changed.
 */
/*
 * Global const data would go here and would look like:
 * 	const int foo = 1;
 */	
static const char _NXStreamsTextPad[252] = { 0 };

/*
 * Static (literal) const data goes here and would look like:
 * 
 *	static const char _literal[]= "literal";
 * 
 * and can be used to initialize const or not-const data pointing
 * constant strings.
 */
static const char _NXStreamsLiteralTextPad[256] = { 0 };

/* buffer and curr-ptr for stack allocation of error buffer */
char *_NXStreamsUnused1 = NULL;
int _NXStreamsUnused2 = 0;

/* Handles RAISE's with nowhere to longjmp to */
void (*_NXUncaughtExceptionHandler)() = NULL;

char _NXStreamsDataPad[500] = { 0 };	/* 40 bytes of padding */


