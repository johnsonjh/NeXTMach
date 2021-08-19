/*
 *	Stream implementation data structure definitions.
 *
 *	These definitions are not necessary if you only want to use
 *	the streams package. You will need these definitions if you 
 *	implement a stream.
 *
 */
 
#import "streams.h"
#import <zone.h>

#ifndef STREAMS_IMPL_H
#define STREAMS_IMPL_H

#define NX_DEFAULTBUFSIZE	(16 * 1024)

/*
 *	Procedure declarations used in implementing streams.
 */

extern NXStream *NXStreamCreate(int mode, int createBuf);
extern NXStream *NXStreamCreateFromZone(int mode, int createBuf, NXZone *zone);
extern void NXStreamDestroy(NXStream *stream);
extern void NXChangeBuffer(NXStream *stream);
extern int NXFill(NXStream *stream);
    /* NXFill should only be called when the buffer is empty */
    
extern int NXDefaultWrite(NXStream *stream, const void *buf, int count);
extern int NXDefaultRead(NXStream *stream, void *buf, int count);

#endif


