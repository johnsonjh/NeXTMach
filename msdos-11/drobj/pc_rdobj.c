/*
*****************************************************************************
	PC_READ_OBJ - Assign an initialized BLKBUFF to a DROBJ

					   
Summary
	
	#include <pcdisk.h>


	BLKBUFF *pc_read_obj(pobj)
		DROBJ *pobj;
		{


Description
	Use the pdrive and myblock fields of pobj to determine what block to 
	read. Add block to the buffer pool, if needed, point pobj at the
	buffer and return the buffer.

	Note: After reading, you "own" the buffer. You must release it by
	calling pc_free_buff() before others can use it.

 Returns
 	Returns a valid pointer or NULL if block not found and not readable.

Example:
	#include <pcdisk.h>
	BLKBUFF *pblk;
	
	if (pblk = pc_read_obj(pobj))
		{
		copybuff(result,pblk->pdata,512);
		pc_free_buf(pblk);
		}


*****************************************************************************
*/

#include <pcdisk.h>

#ifndef ANSIFND
BLKBUFF *pc_read_obj(pobj)
	DROBJ *pobj;
	{
#else
BLKBUFF *pc_read_obj(DROBJ *pobj) /* _fn_ */
	{
#endif
	BLKBUFF *pc_read_blk();

	if (!pobj)
		return (NULL);
	return(pobj->pblkbuff = pc_read_blk(pobj->pdrive, pobj->blkinfo.my_block));
	}

