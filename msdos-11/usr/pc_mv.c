/*
*****************************************************************************
	PC_MV -  Rename a file.

		   
Summary
	
	#include <posix.h>

	BOOL pc_mv(name, newname)
		TEXT 	*name;
		TEXT 	*newname;

 Description
 	Renames the file in path (name) to newname. Fails if name is invalid,
 	newname already exists or path not found. Does not test if name is a simple
	file. It is possible to rename volumes and directories. (This may change in 
	the multiuser version)

 Returns
 	Returns YES if the file was renamed. Or no if the name not found.

Example:
	#include <pcdisk.h>

	if (!pc_mv("\USR\TXT\LETTER.TXT", "LETTER.OLD"))
		printf("Cant rename LETTER.TXT");

*****************************************************************************
*/
#include <pcdisk.h>

/* Rename a file */
#ifndef ANSIFND
BOOL pc_mv(name, newname)
	TEXT 	*name;
	TEXT 	*newname;
	{
#else
BOOL pc_mv(TEXT 	*name, TEXT 	*newname) /* _fn_ */
	{
#endif
	FAST DROBJ *pobj;
	TEXT mompath[EMAXPATH];
	TEXT filename[9];
	TEXT fileext[4];
	BOOL ret_val;	

	/* Check to see if mompath exists */
	if (!pc_parsepath(mompath,filename,fileext,newname))
		return (NO);
	pc_strcat (mompath, BACKSLASH_STR);
	pc_strcat (mompath, newname);
	if (pobj = pc_fndnode(mompath))
		{
		pc_freeobj(pobj);
		return (NO);
		}


	/* Find the file and init the structure */
	if (pobj = pc_fndnode(name) )
		{
		/* Get out the filename and extension */
		if (!pc_parsepath(mompath,filename,fileext,newname))
			ret_val = NO;
		else
			{
			copybuff(pobj->finode->fname , filename, 8 );
			copybuff(pobj->finode->fext , fileext, 3 );
			ret_val = pc_update_inode(pobj);
			}
		pc_freeobj(pobj);
		return (ret_val);
		}
	else
		return(NO);
	}
