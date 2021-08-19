/* 
 * Copyright (c) 1988 NeXT, Inc.
 */ 

/*
 *	The NeXT timestamp implementation uses the hardware supported
 *	microsecond counter.
 *
 *	The format of the timestamp structure is:
 *
 *		low_val - microseconds.
 *		high_val - Always zero.
 */
#ifndef _TIME_STAMP_NEXT_
#define _TIME_STAMP_NEXT_

/* #import <next/eventc.h> */
#define TS_FORMAT TS_FORMAT_NeXT
#endif _TIME_STAMP_NEXT


