
/*
*****************************************************************************
	P_ERRNO -  Error number.

					   
Summary
	#include <posix.h>

	IMPORT INT p_errno;

 Description
 	Error values returned from posix style functions.
 Errno values 
	 PEBADF		9	 Invalid file descriptor
	 PENOENT	2	 File not found or path to file not found
	 PEMFILE	24	 No file descriptors available (too many files open)
	 PEEXIST	17	 Exclusive access requested but file already exists.
	 PEACCES	13	 Attempt to open a read only file or a special (directory)
	 PEINVAL	22	 Seek to negative file pointer attempted.
	 PENOSPC	28	 Write failed. Presumably because of no space

 Returns

Example:
	#include <posix.h>
	IMPORT INT p_errno;

	printf("Error:%i\n",p_errno)

*****************************************************************************
*/

#include <posix.h>

GLOBAL INT p_errno = 0;

