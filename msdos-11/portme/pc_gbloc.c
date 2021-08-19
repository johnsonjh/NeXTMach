/*
*****************************************************************************
	GBLOCK -  Read Block(s) from a drive. (MUST BE USER SUPPLIED !)

					   
Summary
	
	#include <posix.h>

	 BOOL gblock(driveno, block, buffer,count)
		UCOUNT driveno; 	drive #
		UCOUNT block; 		block to read
		UTINY *buffer;		dest address
		UCOUNT count		# blocks to read

 Description
 	When the system needs data it will call this routine. Driveno will
 	be zero for A: 1 for B: etc. It is up to you to transfer - count - 
 	blocks from block number - block - to the memory area at - buffer -
 	if all went well return YES else return NO. If you wish to retry
 	failed reads you should do it at this level. The system will not
 	request retries if you return NO. If your hardware needs init-
 	ialization before reading/writing you may want to store a table of local
 	"ISINITTED" flags, one per drive.
 	As shipped this routine uses MS-DOS int 0x25 to read data from the disk
 	and is obviously non-portable.

	Note: BLOCKS are 512 byte entities. If you wish to use larger
	or smaller sector sizes then you should virtualize inside this routine.

 Returns
 	Returns YES if all went well. Else no.

Example:
	#include <pcdisk.h>

	UTINY mybuff[1024];

	if (!gblock(0, 5, mybuff, 2))
		printf("Cant create blocks 5 & 6 from drive 0\n");

*****************************************************************************
*/

#include <pcdisk.h>
#if	NeXT
#import <nextdos/next_proto.h>
#import <nextdos/msdos.h>
#import <nextdos/dosdbg.h>
#else	NeXT
#include <bios.h>
#include <dos.h>
#endif	NeXT


/* Read a block from disk at cwdrive to buffer */
#ifndef ANSIFND
BOOL gblock(driveno,block, buffer, count)
	UCOUNT driveno; 				
	BLOCKT block; 				
	VOID *buffer;
	COUNT count;
	{
#else
BOOL gblock(UCOUNT driveno, BLOCKT block, VOID *buffer, COUNT count) /* _fn_ */
	{
#endif
	return(unix_read(&dos_drive_info[driveno], block, count, buffer));
}
