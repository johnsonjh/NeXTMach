/*
*****************************************************************************
	PC_FIRSTBLOCK -  Return the absolute block number of a directory's 
					 contents.

					   
Summary
	
	#include <pcdisk.h>

	BLOCKT pc_firstblock(pobj)
		DROBJ *pobj;
		{

 Description
 	Returns the block number of the first inode in the subdirectory. If
 	pobj is the root directory the first block of the root will be returned.


 Returns
 	Returns 0 if the obj does not point to a directory, otherwise the
 	first block in the directory is returned.

 Example:

	#include <pcdisk.h>
	DROBJ *pmom;

	Get the start of the directory represented by pmom.
	if (! (firstblock = pc_firstblock(pmom) ) )
		{
	    return (NO);
		}
	else
		printf("First block = %i\n",firstblock);


*****************************************************************************
*/
#include <pcdisk.h>


/* Get  the first block of a root or subdir */
#ifndef ANSIFND
BLOCKT pc_firstblock(pobj)
	FAST DROBJ *pobj;
	{
#else
BLOCKT pc_firstblock(FAST DROBJ *pobj) /* _fn_ */
	{
#endif
	if (!pc_isadir(pobj))
		return (BLOCKEQ0);

	/* Root dir ? */
	if (pobj->isroot)
		{
		return (pobj->blkinfo.my_frstblock);
		}
	else
		{
	    return (pc_cl2sector(pobj->pdrive , pobj->finode->fcluster) );
	    }
	}

