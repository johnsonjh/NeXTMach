/*
 * merged version of EBS usr/}pc_isdir, pc_mkdir, pc_rmdir, pc_unlin, pc_mv}.c
 *
*****************************************************************************
	PC_ISDIR - Test if a path name is a directory 

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_isdir(path)
		TEXT *path;


 Description
 	Test to see if a path specification ends at a subdirectory or a
 	file.
 	Note: "\" is a directory.

 Returns
 	Returns YES if it is a directory.
Example:
	#include <pcdisk.h>

	if (pc_isdir("\USR\MEMOS"))
		printf("\USR\MEMOS is a directory");

*****************************************************************************
*/
#include <pcdisk.h>
#import <header/posix.h>
#import <nextdos/dosdbg.h>

IMPORT INT p_errno;

#ifndef ANSIFND
BOOL pc_isdir(path)
	TEXT *path;
#else
BOOL pc_isdir(TEXT *path) /* _fn_ */
#endif
	{
	DROBJ  *pobj;
	BOOL   ret_val = NO;
	if (pobj = pc_fndnode(path)) 
		{
		ret_val = pc_isadir(pobj);
		pc_freeobj(pobj);
		}
	return (ret_val);
	}
/*
*****************************************************************************
	PC_MKDIR	-  Create a directory.

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_mkdir(name)
		TEXT *name;


Description
	Create a sudirectory in the path specified by name. Fails if a
	file or directory of the same name already exists or if the path
	is not found.


Returns
 	Returns YES if it was able to create the directory, otherwise
 	it returns NO.

Example
	#include <pcdisk.h>

	if (!pc_mkdir("\USR\LIB\HDRS\SYS")
		printf("Cant create subdirectory\n");

*****************************************************************************
*/

#ifndef ANSIFND
BOOL	pc_mkdir(path)
	TEXT 	*path;
#else
BOOL	pc_mkdir(TEXT 	*path) /* _fn_ */
#endif
	{
	FAST DROBJ *pobj;

	dbg_api(("pc_mkdir: path <%s>\n", path));
	/* Fail if the directory exists */
	if (pobj = pc_fndnode(path) )
		{
		pc_freeobj(pobj);
		return (NO);
  		}
  	else if (pobj = pc_mknode(path ,ADIRENT) )
		{
		pc_freeobj(pobj);
		return (YES);
		}
	else
		return(NO);

	}
/*
*****************************************************************************
	PC_RMDIR - Delete a directory.

					   
Summary

	
	#include <pcdisk.h>

	BOOL pc_rmdir(name)
		TEXT 	*name;



Description

 	Delete the directory specified in name. Fail if name is not a directory,
	is read	only or contains more than the entries . and ..


 Returns
 	Returns YES if the directory was successfully removed.

Example:
	#include <pcdisk.h>


	if (!pc_rmdir("D:\USR\TEMP")
		printf("Cant delete directory\n");

*****************************************************************************
*/

/* Remove a directory */
#ifndef ANSIFND
BOOL pc_rmdir(name)
	TEXT	*name;
#else
BOOL pc_rmdir(TEXT	*name) /* _fn_ */
#endif
	{
	FAST DROBJ *pobj;
	FAST DROBJ *pchild;
	BOOL ret_val = YES;

	pchild = NULL;
	pobj = NULL;

	dbg_api(("pc_rmdir: name <%s>\n", name));
	/* Find the file and init the structure */
	if (pobj = pc_fndnode(name))
		/* Found it. Check access permissions */
		{
		if (!pc_isadir(pobj))
			{
			ret_val = NO;
			}
		else
			{
			/* Search through the directory. look at all files */
			/* Any file that is not '.' od '..' is a problem */
			/* Call pc_get_inode with NULL to give us an obj */
			if (pchild = pc_get_inode(NULL, pobj, "*", "*"))
				{
				do 
					{
					if (!(pc_isdot(pchild->finode->fname, 
					    pchild->finode->fext)))
						if(!(pc_isdotdot(
						    pchild->finode->fname, 
						    pchild->finode->fext)))
							{
							ret_val = NO;
							p_errno = PENOTEMPTY;
							break;
							}
					}
					while (pc_get_inode(pchild, pobj, 
					    "*", "*"));
				}
			if (ret_val)
				ret_val = pc_rmnode(pobj);
			}
		}
	else				/* Directory not found */
		ret_val = NO;

	if (pobj)
		pc_freeobj(pobj);
	if (pchild)
		pc_freeobj(pchild);
	return(ret_val);
	}
/*
*****************************************************************************
	PC_UNLINK - Delete a file.

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_unlink(name)
		TEXT *name;

 Description
	Delete the file in name. Fail if not a simple file,if it is open,
	does not exist or is read only.

 Returns
 	Returns YES if it successfully deleted the file.


Example
	#include <pcdisk.h>


	if (! pc_unlink("B:\USR\TEMP\TMP001.PRN") )
		printf("Cant delete file \n")

*****************************************************************************
*/

/* Delete a file */
#ifndef ANSIFND
BOOL pc_unlink(name)
	TEXT	*name;
#else
BOOL pc_unlink(TEXT	*name) /* _fn_ */
#endif
	{
	FAST DROBJ *pobj;
	BOOL ret_val;

	dbg_api(("pc_unlink: name <%s>\n", name));
	/* Find the file and init the structure */
	if ( pobj = pc_fndnode(name) )
		{
		/* Be sure it is not the root. Since the root is an abstraction 
		   we can not delete it */
		if (pc_isroot(pobj))
			{
			ret_val = NO;
			}
		/* Check access permissions */
		else if(!(pobj->finode->fattribute & ( ARDONLY | AVOLUME | ADIRENT)) )
			ret_val = pc_rmnode(pobj);
		pc_freeobj(pobj);
		return(ret_val);
  		}
  	else
		return(NO);
	}

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

/* Rename a file */
#ifndef ANSIFND
BOOL pc_mv(name, newname)
	TEXT 	*name;
	TEXT 	*newname;
#else
BOOL pc_mv(TEXT 	*name, TEXT 	*newname) /* _fn_ */
#endif
	{
	FAST DROBJ *pobj;
	TEXT mompath[EMAXPATH];
	TEXT filename[9];
	TEXT fileext[4];
	BOOL ret_val;	

	dbg_api(("pc_mv: name <%s> newname <%s>\n", name, newname));
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
