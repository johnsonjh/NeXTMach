/*
*****************************************************************************
	PC_UPDATE_INODE - Flush an inode to disk

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_update_inode(pobj)
		DROBJ *pobj;

Description
 	Read the disk inode information stored in pobj and write it to
 	the block and offset on the disk where it belongs. The disk is
 	first read to get the block and then the inode info is merged in
 	and the block is written. (see also pc_mknode() )


Returns
 	Returns YES if all went well, no on a write error.


Example:
	#include <pcdisk.h>


	Write an inode to disk.
	DROBJ *pobj;


	if (!pc_update_inode(pobj))
		printf("Cant Update disk\n");

*****************************************************************************
*/
#include <pcdisk.h>

/* Take a DROBJ that contains correct my_index & my_block. And an inode.
   Load the block. Copy the inode in and write it back out */
#ifndef ANSIFND
BOOL pc_update_inode(pobj)
	FAST DROBJ *pobj;
	{
#else
BOOL pc_update_inode(FAST DROBJ *pobj) /* _fn_ */
	{
#endif
	BLKBUFF *pbuff;
	BLKBUFF *pc_read_obj();
	FAST DOSINODE *pi;
	FAST COUNT i;
	BOOL retval;
	DIRBLK *pd;

	pd = &pobj->blkinfo;
	i	= pd->my_index;
	if ( i >= INOPBLOCK || i < 0 )  /* Index into block */
		return (NO);

	/* Read the data */
	if (pbuff = pc_read_obj(pobj))
		{
		pi = (DOSINODE *) pbuff->pdata;
		/* Copy it off and write it */
		pc_ino2dos( (pi+i), pobj->finode );
		retval =  pc_write_obj( pobj );
		/* Free the buff. If retval == NO(fail), pass a yes to
		   freebuf so it will discard the buffer. */
		pc_free_buf(pbuff, !retval);
		return (retval);
		}
	return (NO);
	}

