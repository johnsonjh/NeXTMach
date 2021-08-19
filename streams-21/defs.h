/*
 *	File:	defs.h
 *	Author:	Trey Matteson
 *
 *	Private defs for streams package
 */

#import "streams.h"
#import "streamsimpl.h"
#import <objc/error.h>
#import <stddef.h>

#define MAGIC_NUMBER	0xbead0369

extern void _NXVerifyStream(NXStream *s);
				
