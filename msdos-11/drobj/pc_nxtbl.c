/*
*****************************************************************************
	PC_NEXT_BLOCK - Calculate the next block owned by an object.

					   
Summary
	#include <pcdisk.h>

	BOOL pc_next_block(pobj)
		DROBJ *pobj;


 Description
 	Find the next block owned by an object in either the root or a cluster
 	chain and update the blockinfo section of the object.

 Returns
 	Returns YES or NO on end of chain.

Example:
	#include <pcdisk.h>

	Not in that block. Try again 
	pc_free_buf(rbuf,NO);
	Update the objects block pointer
	if (!pc_next_block(pobj))
		break;
				
*****************************************************************************
*/
#include <pcdisk.h>

/* Calculate the next block in an object */
#ifndef ANSIFND
BOOL pc_next_block(pobj)
	FAST DROBJ *pobj;
	{
#else
BOOL pc_next_block(FAST DROBJ *pobj) /* _fn_ */
	{
#endif
	FAST BLOCKT nxt;
	BLOCKT pc_l_next_block();

	if (nxt = pc_l_next_block(pobj->pdrive, pobj->blkinfo.my_block) )
		{
		pobj->blkinfo.my_block = nxt;
		return (YES);
		}
	else
		return (NO);
	}

