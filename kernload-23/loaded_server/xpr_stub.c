/* 
 * Copyright (c) 1989, 1988 NeXT, Inc.
 *
 * HISTORY
 * 04-Dec-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 */ 
/*
 * (Stub for) xpr silent tracing circular buffer.
 */

int xprflags;
int nxprbufs = 0;
void *xprbase;	/* Pointer to circular buffer nxprbufs*sizeof(xprbuf)*/
void *xprptr;	/* Currently allocated xprbuf */
void *xprlast;	/* Pointer to end of circular buffer */

xpr(char *msg, ...) {}


