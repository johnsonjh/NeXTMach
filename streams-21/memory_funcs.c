#ifdef SHLIB
#include "shlib.h"
#endif
/*
 *	File:	memory_funcs.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Streams implemented as buffered memory operations.
 */

#include "defs.h"
#include <mach.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

extern int open(const char *path, int flags, int mode);
extern int close(int fd);
extern int write(int fd, const void *buf, int size);
extern int fsync(int fd);
extern int ftruncate(int fd, off_t length);
extern kern_return_t map_fd(int fd, vm_offset_t offset, vm_offset_t *addr,
				boolean_t find_space, vm_size_t numbytes);

static int memory_flush(register NXStream *s);
static int memory_fill(register NXStream *s);
static void memory_change(register NXStream *s);
static void memory_seek(register NXStream *s, long offset);
static void memory_close(NXStream *s);

static void verify_memory_stream(NXStream *s);

static struct stream_functions memory_functions = {
	NXDefaultRead,
	NXDefaultWrite,
	memory_flush,
	memory_fill,
	memory_change,
	memory_seek,
	memory_close,
};


/* verifies we have a memory stream.  Call this in all memory stream
   specific routines AFTER calling _NXVerifyStream.
 */
static void verify_memory_stream(NXStream *s)
{
    if (s->functions != &memory_functions)
	NX_RAISE(NX_illegalStream, s, 0);
}


#define	CHUNK_SIZE	(256*1024)	/* XXX */
/*
 *	memory_extend: Extend the memory region to at least the
 *	specified size.
 */

static void memory_extend(register NXStream *s, int size)
{
    vm_size_t       new_size;
    vm_offset_t     new_addr;
    int             cur_offset;
    kern_return_t   ret;

    new_size = (size + CHUNK_SIZE) & (~(vm_page_size - 1));
    ret = vm_allocate(task_self(), &new_addr, new_size, TRUE);
    if (ret != KERN_SUCCESS)
	NX_RAISE(NX_streamVMError, s, (void *)ret);
    cur_offset = 0;
    if (s->buf_base) {
	int             copySize;

	copySize = s->buf_size;
	if (copySize % vm_page_size)
	    copySize += vm_page_size - (copySize % vm_page_size);
	ret = vm_copy(task_self(),
		      (vm_offset_t)s->buf_base,
		      (vm_size_t)copySize,
		      (vm_offset_t)new_addr);
	if (ret != KERN_SUCCESS)
	    NX_RAISE(NX_streamVMError, s, (void *)ret);
	ret = vm_deallocate(task_self(),
			    (vm_offset_t)s->buf_base,
			    (vm_size_t)s->buf_size);
	if (ret != KERN_SUCCESS)
	    NX_RAISE(NX_streamVMError, s, (void *)ret);
	cur_offset = s->buf_ptr - s->buf_base;
    }
    s->buf_base = (unsigned char *)new_addr;
    s->buf_size = new_size;
    s->buf_ptr = s->buf_base + cur_offset;
    s->buf_left = new_size - size;
    s->flags &= ~NX_USER_OWNS_BUF;
}

/*
 *	memory_flush:  flush a stream buffer.
 */

static int memory_flush(register NXStream *s)
{

    int bufSize = s->buf_size - s->buf_left;
    int reading = (s->flags & NX_READFLAG);

    if (reading)
	return 0;
    if (bufSize > s->eof)
	s->eof = bufSize;
    if (s->buf_left > 0)
	return bufSize;
    memory_extend(s, bufSize);
    if (bufSize > s->eof)
	s->eof = bufSize;
    return bufSize;
}

/*
 *	memory_fill:  fill a stream buffer, return how much
 *	we filled.
 */

static int memory_fill(register NXStream *s)
{
    return 0;
}

static void memory_change(register NXStream *s)
{
    long offset = NXTell(s);

    if (offset > s->eof)
	s->eof = offset;
    s->buf_ptr = s->buf_base + offset;
    if (s->flags & NX_WRITEFLAG) {
	s->buf_left = s->eof - offset;
    } else {
	s->buf_left = s->buf_size - offset;
    }
}

static void memory_seek(register NXStream *s, long offset)
{
    int bufSize = s->buf_size - s->buf_left;

    if (s->flags & NX_READFLAG) {
	if (offset > s->eof)
	    NX_RAISE(NX_illegalSeek, s, 0);
	s->buf_ptr = s->buf_base + offset;
	s->buf_left = s->eof - offset;
    } else {
	if (bufSize > s->eof)
	    s->eof = bufSize;
	while (s->buf_size < offset)
	    memory_extend(s, s->buf_size);
	if (offset > s->eof)
	    s->eof = offset;
	s->buf_ptr = s->buf_base + offset;
	s->buf_left = s->buf_size - offset;
    }
}


static void memory_close(NXStream *s)
{
    vm_offset_t     end;
    int             userBuf;
    kern_return_t   ret;

 /*
  * Free extra pages after end of buffer. 
  */
    userBuf = (s->flags & NX_USER_OWNS_BUF) != 0;
    if ((s->flags & NX_CANWRITE) && !userBuf) {
	int remove;
	int bufSize = s->buf_size - s->buf_left;

	if (bufSize > s->eof)
	    s->eof = bufSize;
	end = round_page(s->buf_base + s->eof);
	remove = s->buf_size - (end - (vm_offset_t) s->buf_base);
	ret = vm_deallocate(task_self(), end, remove);
	if (ret != KERN_SUCCESS)
	    NX_RAISE(NX_streamVMError, s, (void *)ret);
	s->buf_size -= remove;
    }
}

/*
 *	Begin memory stream specific exported routines.
 */

/*
 *	NXOpenMemory:
 *
 *	open a stream using a memory backing.  Initial address and
 *	size are specified for a buffer.  Address must be paged aligned
 *	and size must be a multiple of a page!  (They should be the
 *	result of a vm_allocate).  Address and size may be 0.
 */

NXStream *NXOpenMemory(const char *addr, int size, int mode)
{
    NXStream		*s;
    int			newMode = mode;

    if ((int)addr % vm_page_size && (mode != NX_READONLY))
	return 0;

    s = NXStreamCreate(newMode, FALSE);
    s->functions = &memory_functions;
    s->buf_base = (unsigned char *)addr;
    s->buf_size = size;
    s->buf_left = size;
    s->buf_ptr = (unsigned char *)addr;
    s->eof = size;
    if (addr)
	s->flags |= (NX_USER_OWNS_BUF | NX_CANSEEK);
    else
	switch(mode) {
	    case NX_READONLY:
		break;
	    case NX_WRITEONLY:
		memory_flush(s);	/* get initial buffer */
		break;
	    case NX_READWRITE:
		if (s->flags & NX_READFLAG) {
		    NXChangeBuffer(s);
		    memory_flush(s);	/* get initial buffer */
		}
		break;
	}
    return (s);
}

NXStream *NXMapFile(const char *name, int mode)
{
    int             fd;
    char           *buf;
    struct stat     info;
    NXStream 	   *s = NULL;

    fd = open(name, O_RDONLY, 0666);
    if (fd >= 0) {
	if (fstat(fd, &info) >= 0) {
	    if (info.st_size > 0 || (info.st_mode & S_IFMT) == S_IFDIR) {
		if (map_fd(fd, 0, (vm_offset_t *)&buf, TRUE, info.st_size) ==
								KERN_SUCCESS) {
		    s = NXOpenMemory(buf, info.st_size, mode);
		    s->flags &= ~NX_USER_OWNS_BUF;
		}
	    } else {
		s = NXOpenMemory(NULL, 0, mode);
	    }
	}
	if (close(fd) < 0) {
	    NXCloseMemory(s, NX_FREEBUFFER);
	    s = NULL;
	}
    }
    return s;
}


int NXSaveToFile(register NXStream *s, const char *name )
{
    int             fd;
    char           *buf;
    int             len, max, retval;

    _NXVerifyStream(s);
    verify_memory_stream(s);
    fd = open(name, O_WRONLY | O_CREAT, 0666);
    if (fd >= 0) {
	NXGetMemoryBuffer(s, &buf, &len, &max);
	retval = write(fd, buf, len);
	if (retval < 0)
	    return retval;
	retval = ftruncate(fd, len);
	if (retval < 0)
	    return retval;
	retval = fsync(fd);
	if (retval < 0)
	    return retval;
	retval = close(fd);
	if (retval < 0)
	    return retval;
    } else
	return -1;
    return 0;
}

void NXCloseMemory(register NXStream *s, int option)
{
    int userBuf;
    kern_return_t ret;

    _NXVerifyStream(s);
    verify_memory_stream(s);
    userBuf = (s->flags & NX_USER_OWNS_BUF) != 0;
    if (!userBuf) {
	switch (option) {
	    case NX_FREEBUFFER:
		ret = vm_deallocate(task_self(),
				    (vm_offset_t)s->buf_base,
				    (vm_size_t)s->buf_size);
		if (ret != KERN_SUCCESS)
		    NX_RAISE(NX_streamVMError, s, (void *)ret);
		break;
	    case NX_TRUNCATEBUFFER:
		memory_close(s);
		break;
	    case NX_SAVEBUFFER: break;
	}
    }
    NXStreamDestroy(s);
}


void NXGetMemoryBuffer(NXStream *s, char **addr, int *len, int *maxlen)
{
    int bufSize = s->buf_size - s->buf_left;
    
    _NXVerifyStream(s);
    verify_memory_stream(s);
    *addr = (char *)s->buf_base;
    *len = (bufSize > s->eof) ? bufSize : s->eof;
    *maxlen = s->buf_size;
}

