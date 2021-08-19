/*
*****************************************************************************
	PC_L_NEXT_BLOCK - Calculate the next block in a chain.

   
Summary
	#include <pcdisk.h>

	BLOCKT pc_l_next_block(pdrive, curblock)
		DDRIVE *pdrive;
		BLOCKT curblock;


 Description
 	Find the next block in either the root or a cluster	chain.

 Returns
 	Returns 0 on end of root dir or chain.

Example:
	#include <pcdisk.h>

	if (nxt = pc_l_next_block(pobj->pdrive, pobj->blkinfo.my_block) )
		{
		pobj->blkinfo.my_block = nxt;
		return (YES);
		}

				
*****************************************************************************
*/
#include <pcdisk.h>

	/* Return the next block in a chain */
#ifndef ANSIFND
BLOCKT pc_l_next_block(pdrive, curblock)
	DDRIVE *pdrive;
	BLOCKT curblock;
	{
#else
BLOCKT pc_l_next_block(DDRIVE *pdrive, BLOCKT curblock) /* _fn_ */
	{
#endif
	UCOUNT cluster;
	UCOUNT pc_sec2cluster();
	UCOUNT pc_sec2index();
	/* BLOCKT pc_cl2sector(); */

	/* If the block is in the root area */
	if (curblock < pdrive->firstclblock)
		{
		if (curblock < pdrive->rootblock)
			return (0);
		else if (++curblock < pdrive->firstclblock)
			return (curblock);
		else
			return (BLOCKEQ0);
		}
		/* In cluster space */
	else
		{
		if (curblock >= pdrive->numsecs)
			return (BLOCKEQ0);
		/* Get the next block */
		curblock += 1;

		/* If the next block is not on a cluster edge then it must be
		   in the same cluster as the current. - otherwise we have to
		   get the firt block from the next cluster in the chain */
		if (pc_sec2index(pdrive, curblock))
			return (curblock);
	else
			{
			curblock -= 1;
			/* Get the old cluster number - No error test needed */
			cluster = pc_sec2cluster(pdrive,curblock);
			/* Consult the fat for the next cluster */
			if ( !(cluster = pc_clnext(pdrive, cluster)) )
				return (BLOCKEQ0); /* End of chain */
			else
				return (pc_cl2sector(pdrive, cluster));
			}
		}
	}

