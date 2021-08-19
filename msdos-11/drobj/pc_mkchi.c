/*
*****************************************************************************
	PC_MKCHILD -  Allocate a DROBJ and fill in based on parent object.

					   
Summary
	#include <pcdisk.h>


	DROBJ *pc_mkchild(pmom)
		DROBJ *pmom;


 Description
 	Allocate an object and fill in as much of the the block pointer section 
	as possible based on the parent.

 Returns
 	Returns a partially initialized DROBJ if enough core available and 
 	pmom was a valid subdirectory.

Example:
	#include <pcdisk.h>

	Create the child if just starting
	if (!pobj)
		{
		starting = YES;
		if (! (pobj = pc_mkchild(pmom)) )
			return(NULL);
		}
				
*****************************************************************************
*/
#include <pcdisk.h>


#ifndef ANSIFND
DROBJ *pc_mkchild(pmom)
	FAST DROBJ *pmom;
	{
#else
DROBJ *pc_mkchild(FAST DROBJ *pmom) /* _fn_ */
	{
#endif
	FAST DROBJ *pobj;
	FAST DIRBLK *pd;
	FAST DIRBLK *pm;


	/* Mom must be a directory */
	if (!pc_isadir(pmom))
		return(NULL);

	/* init the object - */
	if (!(pobj = pc_allocobj()))
		return (NULL);

	pd = &pobj->blkinfo;
	pm = &pmom->blkinfo;

	pobj->isroot = NO;				/* Child can not be root */
	pobj->pdrive = 	pmom->pdrive;	/* Child inherets moms drive */

	/* Now initialize the fields storing where the child inode lives */
	pd->my_index = 0;
	if (! (pd->my_block = pd->my_frstblock = pc_firstblock(pmom) ) )
		{
		pc_freeobj(pobj);
	    return (NULL);
	    }

	return (pobj);
	}

