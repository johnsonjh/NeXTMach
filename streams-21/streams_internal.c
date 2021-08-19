#ifdef SHLIB
#include "shlib.h"
#endif
/*
 *	File:	streams_internal.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Miscellaneous internal stream support routines.
 */

#include "defs.h"
#include <stdlib.h>
#include <string.h>

/*
 *	_NXStreamFlushBuffer: flush all buffered data in a stream but place
 *	specified char in buffer first.  Called by NXPutc.
 */

extern int _NXStreamFlushBuffer(NXStream *s, unsigned char c)
{
    _NXVerifyStream(s);
    s->buf_left++;		/* compensate for NXPutc */
    (void)NXFlush(s);
    *s->buf_ptr++ = c;
    s->buf_left--;
    return (int)c;
}

/*
 *	_NXStreamFillBuffer: fill the buffer, and return first char.
 *	Called by NXGetc.
 */

extern int _NXStreamFillBuffer(NXStream *s)
{
    int             n;

    _NXVerifyStream(s);
    if (s->flags & NX_EOS)
	return -1;
    if (s->buf_left < 0)
	s->buf_left = 0;
    n = NXFill(s);
    if (n <= 0) {
	s->flags |= NX_EOS;
	return -1;
    }
    s->buf_left--;
    return ((int)((*(s->buf_ptr++)) & 0xff));
}

extern int _NXStreamChangeBuffer(NXStream *s, unsigned char ch)
{
    int wasReading;

    _NXVerifyStream(s);
    wasReading = (s->flags & NX_READFLAG);
    NXChangeBuffer(s);
    if (wasReading)
	NXPutc(s, ch);
    else
	ch = NXGetc(s);
    return ((int)(ch));
}


/*
 *	NXDefaultWrite: write data into a stream.
 */

int NXDefaultWrite(register NXStream *s, register const void *buf, 
    register int count)
{
    register int n;
    int total = count;
    int curPos;
    const char * volatile bufPtr = buf;
    NXHandler exception;

    _NXVerifyStream(s);
 /*
  * Loop until copying complete. 
  */
    exception.code = 0;
    if (s->flags & NX_READFLAG) 
	NXChangeBuffer(s);
    while (count > 0) {
    /*
     * Flush buffer if necessary. 
     */
	if (s->buf_left == 0) {
	    NX_DURING {
		(void)NXFlush(s);
	    } NX_HANDLER {
		exception = NXLocalHandler;
		break;
	    } NX_ENDHANDLER
	}
    /*
     * Figure out how much to copy this time. 
     */
	n = count;
	if (n > s->buf_left)
	    n = s->buf_left;
	bcopy((const char *)bufPtr, s->buf_ptr, n);
    /*
     * Update all pointers. 
     */
	s->buf_ptr += n;
	s->buf_left -= n;
	bufPtr += n;
	count -= n;
    }
    curPos = NXTell(s);
    if (curPos > s->eof)
	s->eof = curPos;
    if (exception.code)
	NX_RAISE(exception.code, exception.data1, exception.data2);
    return (total - count);
}

/*
 *	NXDefaultRead: read data into specified buffer, return amount
 *	of data read.
 */
int NXDefaultRead(register NXStream *s, register void *buf, register int count)
{
    register int    n, total;
    char *bufPtr = buf;

    _NXVerifyStream(s);
    if (s->flags & NX_EOS)
	return (0);

    if (s->flags & NX_WRITEFLAG) 
	NXChangeBuffer(s);
    total = 0;
    while (count > 0) {
    /*
     * Fill buffer if necessary. 
     */
	if (s->buf_left == 0) {
	    n = NXFill(s);
	    if (n <= 0) {
		s->flags |= NX_EOS;
		return (total ? total : n);
	    }
	}
    /*
     * Figure out how much to copy. 
     */
	n = count;
	if (n > s->buf_left)
	    n = s->buf_left;
	bcopy(s->buf_ptr, bufPtr, n);
    /*
     * Update all pointers. 
     */
	s->buf_ptr += n;
	s->buf_left -= n;
	bufPtr += n;
	count -= n;
	total += n;
    }
    return total;
}

/*
 *	NXStreamCreate:
 *
 *	Create a new stream.  Mode specifies the intended use of the stream.
 */

NXStream *NXStreamCreate(int mode, int createBuf)
{
    return NXStreamCreateFromZone(mode, createBuf, NXDefaultMallocZone());
}
NXStream *NXStreamCreateFromZone(int mode, int createBuf, NXZone *zone)
{
    register NXStream *s;
    register unsigned char *buf;

    s = (NXStream *) NXZoneMalloc(zone, sizeof(NXStream));
    bzero(s, sizeof(NXStream));
    s->magic_number = MAGIC_NUMBER;
    if (createBuf) {
	buf = (unsigned char *)NXZoneMalloc(zone, NX_DEFAULTBUFSIZE);
	s->buf_base = buf;
	s->buf_size = NX_DEFAULTBUFSIZE;
	s->buf_ptr = buf;
    } else
	s->flags |= NX_NOBUF;
    switch (mode) {
	case NX_READONLY:
	    s->flags |= (NX_CANREAD | NX_READFLAG);
	    break;
	case NX_WRITEONLY:
	    s->flags |= (NX_CANWRITE | NX_WRITEFLAG);
	    s->buf_left = NX_DEFAULTBUFSIZE;
	    break;
	case NX_READWRITE:
	    s->flags |= 
	        (NX_CANWRITE | NX_CANREAD | NX_READFLAG);
	    break;
    }
    return s;
}

/*
 *	NXStreamDestroy:
 *
 *	Destory an exisiting stream.  Stream should already by flushed.
 */

void NXStreamDestroy(NXStream *s)
{
    _NXVerifyStream(s);
    if (!(s->flags & NX_NOBUF))
	free(s->buf_base);
    if (s->info) {
	free(s->info);
	s->info = 0;
    }
    free(s);
}


/* verifies that a pointer really points to a stream */
extern void _NXVerifyStream(NXStream *s)
{
    if (!s || s->magic_number != MAGIC_NUMBER)
	NX_RAISE(NX_illegalStream, s, 0);
}


