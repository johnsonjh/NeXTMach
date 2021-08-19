/*
*****************************************************************************
	PC_FNDNODE -  Find a file or directory on disk and return a DROBJ.

					   
Summary
	#include <pcdisk.h>


	DROBJ *pc_fndnode(path)
		TEXT *path;
		{


 Description
	Take a full path name and traverse the path until we get to the file
	or subdir at the end of the path spec. When found allocate and init-
	ialize (OPEN) a DROBJ.


 Returns
 	Returns a pointer to a DROBJ if the file was found, otherwise NULL.

Example:
	Test a file for read only.

	if ( (pobj = pc_fndnode( name) ))
		if(pc_isroot(pobj)||(pobj->finode->fattribute & (AVOLUME | ADIRENT)) )
			return(NO);		 it is a directory
		if( (flag & (O_WRONLY|O_RDWR)) &&(pobj->finode->fattribute & ARDONLY))
			return(NO);		read only file


*****************************************************************************
*/

#include <pcdisk.h>
/* Find "path" and create a DROBJ structure if found */
#ifndef ANSIFND
DROBJ *pc_fndnode(path)
	TEXT *path;
	{
#else
DROBJ *pc_fndnode(TEXT *path) /* _fn_ */
	{
#endif
	FAST DROBJ *pobj;
	FAST DROBJ *pchild;
	COUNT  driveno;
	DDRIVE *pdrive;
	TEXT filename[9];
	TEXT fileext[4];

	/* Get past D: plust get drive number if there */
	if (! (path = pc_parsedrive( &driveno, path ) ) )
		return (NULL);
	
	/* Find the drive */
	if (! (pdrive = pc_drno2dr( driveno) ) )
		return (NULL);

	/* Get the top of the current path */
	if ( *path == BACKSLASH )
		{
		if (!(pobj = pc_get_root(pdrive)) )
			return (NULL);
		path++;
		}
	else
		if ( !(pobj = pc_get_cwd(pdrive)) )
			return (NULL);

	/* Search through the path til exausted */
	while (*path)
		{
		if (! (path = pc_nibbleparse( filename, fileext, path ) ) )
			{
			pc_freeobj(pobj);
			return (NULL);
			}
		else 	/* "." is a no-op. */
			if (pc_isdot( &filename[0], &fileext[0] ))
				;
		else
			{
			/* Find Filename in pobj. and initialize lpobj with result */
			if (!(pchild = pc_get_inode(NULL, pobj, filename, fileext) ) )
				{
				pc_freeobj(pobj);
				return (NULL);
				}
			/* We're done with pobj for now */
			pc_freeobj(pobj);
			
			/* We found it. We have one special case. if "..". we need
			   to shift up a level so we are not the child of mom
			   but of grand mom. */
			if (pc_isdotdot( &filename[0], &fileext[0] ))
				{
				 /* Find pobj's parent. By looking back from ".." */
				 if ( !(pobj = pc_get_mom(pchild)) ) 
				 	{
					pc_freeobj(pchild);
					return (NULL);
					}
				else
					{
					/* We found the parent now free the child */
					pc_freeobj(pchild);
					}
				}
			else
				/* Make sure pobj points at the next inode */
				pobj = pchild;
			}
		}
	return (pobj);
	}

