
/*
*****************************************************************************
	PBLOCK -  Write Block(s) to a drive. (MUST BE USER SUPPLIED !)

					   
Summary
	
	#include <posix.h>

	 BOOL pblock(driveno, block, buffer,count)
		UCOUNT driveno; 	drive #
		UCOUNT block; 		block to write
		UTINY *buffer;		data address
		UCOUNT count		# blocks to read

 Description
 	When the system wants to write data it will call this routine.
	Driveno will be zero for A: 1 for B: etc. It is up to you to
	transfer - count - 	blocks from to the memory area at - buffer -
	to block number - block - on the disk. If all went well return YES
	else return NO. If you wish to retry failed writesyou should do it
	at this level. The system will not request retries if you return NO.
	If your hardware needs initialization before reading/writing you may
	want to store a table of local 	"ISINITTED" flags, one per drive.
 	As shipped this routine uses MS-DOS int 0x26 to write data to the disk
 	and is obviously non-portable.

	Note: BLOCKS are 512 byte entities. If you wish to use larger
	or smaller sector sizes then you should virtualize inside this routine.

 Returns
 	Returns YES if all went well. Else no.

Example:
	#include <pcdisk.h>

	UTINY mybuff[1024];

	zero out two blocks
	pc_memfille(mybuff, 1024, (UTINY) 0);
	if (!pblock(3, 5, mybuff, 2))
		printf("Cant write data to blocks 5 & 6 on drive 3\n");

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


/* Write a block to disk at cwdrive from buffer */
#ifndef ANSIFND
BOOL pblock(driveno,block, buffer, count)
	UCOUNT driveno; 				
	BLOCKT block; 				
	VOID *buffer;
	COUNT count;
	{
#else
BOOL pblock(UCOUNT driveno, BLOCKT block, VOID *buffer, COUNT count) /* _fn_ */
	{
#endif
	return(unix_write(&dos_drive_info[driveno], block, count, buffer));
	}