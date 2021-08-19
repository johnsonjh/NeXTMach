/*
*****************************************************************************
	PC_GET_ROOT -  Create the special ROOT object for a drive.

					   
Summary
	#include <pcdisk.h>


	DROBJ *pc_get_root(pdrive)
		DDRIVE *pdrive;


 Description
 	Use the information in pdrive to create a special object for accessing
 	files in the root directory.


 Returns
 	Returns a pointer to a DROBJ, or NULL if no core available.

Example:
	#include <pcdisk.h>

	pobj = pc_get_root(pdrive)

*****************************************************************************
*/
#include <pcdisk.h>

/* Initialize the special "root" object 
   Note: We do mot read any thing in here we just set up 
   the block pointers. */
#ifndef ANSIFND
DROBJ *pc_get_root(pdrive)
	FAST DDRIVE *pdrive;
	{
#else
DROBJ *pc_get_root(FAST DDRIVE *pdrive) /* _fn_ */
	{
#endif
	FAST DIRBLK *pd;
	FAST DROBJ *pobj;

	if (!(pobj = pc_allocobj()) )
		return (NULL);

	/* Add a TEST FOR DRIVE INIT Here later */
	pobj->pdrive = pdrive;

	/* Root is an abstraction. It doesn't have an inode */
	/* Free the inode that comes with alloci */
	pc_freei(pobj->finode);
	pobj->finode = NULL;

	/* Set up the tree stuf so we know it is the root */
	pd = &pobj->blkinfo;
	pd->my_frstblock = pdrive->rootblock;
	pd->my_block = pdrive->rootblock;
	pd->my_index = 0;
	pobj->isroot = YES;

	return (pobj);
	}
