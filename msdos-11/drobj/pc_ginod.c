/*
*****************************************************************************
	PC_GET_INODE -  Find a filename within a subdirectory 

					   
Summary
	#include <pcdisk.h>


	DROBJ *pc_get_inode( pobj, pmom, filename, fileext)
		FAST DROBJ *pobj;
		FAST DROBJ *pmom;
		TEXT *filename;
		TEXT *fileext;
		

 Description
 	Search the directory pmom for the pattern or name in filename:ext and
 	return the an initialized object. If pobj is NULL start the search at
 	the top of pmom (getfirst) and allocate pobj before returning it. 
	Otherwise start	the search at pobj (getnext). (see also pc_gfirst,
	pc_gnext)


	Note: Filename and ext must be right filled with spaces to 8 and 3 bytes
		  respectively. Null termination does not matter.

 Returns
 	Returns a drobj pointer or NULL if file not found.

Example:
	#include <pcdisk.h>

	Count files in the root directory.

	pmom = pc_get_root(pdrive)
	pobj = NULL;
	count = 0;

	while (pobj = pc_get_inode( pobj, pmom, "*       ", "*  "))
		count++;
				
*****************************************************************************
*/

/* Give a directory mom. And a file name and extension. 
   Find find the file or dir and initialize pobj.
    If pobj is NULL. We allocate and initialize the object otherwise we get the
	next item in the chain of dirents.
*/
#include <pcdisk.h>

#ifndef ANSIFND
DROBJ *pc_get_inode( pobj, pmom, filename, fileext)
	FAST DROBJ *pobj;
	FAST DROBJ *pmom;
	TEXT *filename;
	TEXT *fileext;
	{
#else
DROBJ *pc_get_inode(FAST DROBJ *pobj, FAST DROBJ *pmom, TEXT *filename, TEXT *fileext) /* _fn_ */
	{
#endif
	BOOL  starting = NO;


	/* Create the child if just starting */
	if (!pobj)
		{
		starting = YES;
		if (! (pobj = pc_mkchild(pmom)) )
			return(NULL);
		}
	else	/* If doing a gnext don't get stuck in and endless loop */
		{
		if ( ++(pobj->blkinfo.my_index) >= INOPBLOCK )
			{
			if (!pc_next_block(pobj))
				{
				return (NULL);
				}
			else
				pobj->blkinfo.my_index = 0;
			}
		}

	if (pc_findin(pobj, filename, fileext))
		{
		return (pobj);
		}
	else
		{
		if (starting)
			pc_freeobj(pobj);
		return (NULL);
		}
	}

