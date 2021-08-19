/*
*****************************************************************************
	PC_WRITE_OBJ - Write a DROBJ's BLKBUFF to disk

					   
Summary
	
	#include <posix.h>


	BOOL *pc_write_obj(pobj)
		DROBJ *pobj;
		{


Description
	Write the data in the BLKBUFF for pobj to disk.

 Returns
 	Returns YES if the write succeeded.

Example:
	#include <pcdisk.h>
	
	if (!(pc_write_obj(pobj)))
		printf("Failed writing buffer \n");

*****************************************************************************
*/
#include <pcdisk.h>


#ifndef ANSIFND
BOOL pc_write_obj(pobj)
	DROBJ *pobj;
	{
#else
BOOL pc_write_obj(DROBJ *pobj) /* _fn_ */
	{
#endif
	BOOL pc_write_blk();

	if (!pobj)
		return (NO);
	return (pc_write_blk(pobj->pblkbuff));
	}

