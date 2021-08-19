/*****************************************************************************
*Filename: POSIX.H - Defines & structures for ms-dos utilities (po_) functions
*					   
*
* EBS - Embedded file manager -
*
* Copyright Peter Van Oudenaren , 1989
* This code may not be redistributed in source or linkable object form
* without the consent of its author.
*
*
*
* Description: 
*	
*
*
*
*
*****************************************************************************/


#ifndef __POSIX__
#define __POSIX__ 1

#ifndef __PCDISK__
#include <pcdisk.h>
#endif


/* File creation permissions for open (mode) */
/* Note: OCTAL */
#define PS_IWRITE 0000400		/* Write permitted 	*/
#define PS_IREAD  0000200 		/* Read permitted. (Always true 
					 * anyway)*/


/* File access flags (flags) */
#define PO_BINARY 0x8000		/* Ignored. All file access is binary*/
#define PO_TEXT	  0x4000		/* Ignored */
#define PO_RDONLY 0x0000		/* Open for read only*/
#define PO_RDWR   0x0002 		/* Read/write access allowed.*/
#define PO_WRONLY 0x0001		/* Open for write only*/
#define PO_APPEND 0x0008		/* Seek to eof on each write*/
#define PO_CREAT  0x0100		/* Create the file if it does not
					 * exist.*/
#define PO_EXCL   0x0400		/* Fail if creating and already 
					 * exists*/
#define PO_TRUNC  0x0200		/* Truncate the file if it already
					 * exists*/


/* Errno values */
#define PEBADF	9		/* Invalid file descriptor*/
#define PENOENT	2		/* File not found or path to file not found*/
#define PEMFILE	24		/* No file descriptors available (too many 
				 * files open)*/
#define PEEXIST	17		/* Exclusive access requested but file already
				 * exists.*/
#define PEACCES	13		/* Attempt to open a read only file or a 
				 * special (directory)*/
#define PEINVAL	22		/* Seek to negative file pointer attempted.*/
#define PENOSPC	28		/* Write failed. Presumably because of no 
				 * space*/
#define PENOTEMPTY  66		/* directory not empty */

/* Arguments to SEEK */
#define PSEEK_SET	0	/* offset from begining of file*/
#define PSEEK_CUR	1	/* offset from current file pointer*/
#define PSEEK_END	2	/* offset from end of file*/

#define MAXUFILES 40    	/* Maximum number of user files in the 
				 * SYSTEM */

#endif

