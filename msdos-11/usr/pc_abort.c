				
/*
*****************************************************************************
	PC_DISKABORT -  Abort all operations on a disk

					   
Summary
	
	#include <posix.h>

	VOID pc_diskabort(path)
		TEXT 	*path;

 Description

	If an application senses that there are problems with a disk, it
	should call pc_diskabort("D:"). This will cause all resources
	associated with that drive to be freed, but no disk writes will
	be attempted. All file descriptors associated with the drive
	become invalid. After correcting the problem call pc_diskopen("D:")
	to re-mount the disk and re-open your files.

Returns
	Nothing

Example:
	#include <posix.h>
trying = YES;
while (trying)
	{
	if (pcfd = po_open("A:\USR\MYFILE",(PO_CREAT|PO_WRONLY),PS_IWRITE)<0))
		{
		trying = NO;
		printf("Cant create file error:%i\n",p_errno);
		See if there is a disk error.
		if (ask_driver_if_there_is_a_problem("A:"))
			{
			Clear everything so we can get a fresh start
			pc_diskabort("A:");
			try to fix it 
			if (driver_fix_it("A:")))
				{
				if (pc_diskopen("A:"))
					trying = YES;
				}
			}
	}
			
*****************************************************************************
*/
#include <posix.h>

/* Free all resources belonging to a drive without flushing anything */ 
#ifndef ANSIFND
VOID pc_diskabort(path)
	TEXT *path;
	{
#else
VOID pc_diskabort(TEXT *path) /* _fn_ */
	{
#endif
	COUNT driveno;

	if (!pc_parsedrive( &driveno, path ))
		return;

	/* Release the drive unconditionally */
	pc_dskfree(driveno,YES);

	return;
	}
