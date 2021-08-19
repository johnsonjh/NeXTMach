/*
*****************************************************************************
	PC_PWD	-  Return a string containing the current working directory for
			   a drive.

					   
Summary
	#include <pcdisk.h>

	BOOL pc_pwd (drive , path)
		TEXT *drive;			contains D: when called
		TEXT *path;				must contain enough space to hold the 
								path spec.
		{


 Description
	Fill in a string with the full path name of the current working directory.
	Return NO if something went wrong.
	If *drive is null or an invalid drive specifier the default drive is used.


 Returns
 	Returns the path name in path. The function returns YES on success
 	no on failure.

Example:

	TEXT pwd[MAXPATH];

	if (pc_pwd("A:", pwd))
		printf ("Working dir is %s",  pwd);
	else
		printf ("Can't find working dir for A:");

*****************************************************************************
*/

#include <pcdisk.h>

LOCAL BOOL pc_l_pwd(TEXT *, DROBJ *);
LOCAL BOOL pc_gm_name(TEXT *path, DROBJ *pmom, DROBJ *pdotdot);

#ifndef ANSIFND
BOOL pc_pwd(drive,path)
	TEXT *drive;
	TEXT *path;	
	{
#else
BOOL pc_pwd(TEXT *drive, TEXT *path) /* _fn_ */
	{
#endif
	COUNT  driveno;
	DDRIVE *pdrive;
	FAST DROBJ *pobj;

	if (!pc_parsedrive( &driveno, drive)) 
		driveno = pc_getdfltdrvno();
	/* Find the drive */
	if ((pdrive = pc_drno2dr(driveno)) == NULL)
		return (NO);
	else if ((pobj = pc_get_cwd(pdrive)) == NULL)
		return (NO);
	else
		return (pc_l_pwd(path, pobj));
	}

#ifndef ANSIFND
LOCAL BOOL pc_l_pwd(path, pobj)
	TEXT *path;
	DROBJ *pobj;
	{
#else
LOCAL BOOL pc_l_pwd(TEXT *path, DROBJ *pobj) /* _fn_ */
	{
#endif
	DROBJ *pchild;
	TEXT  lname[12];

	if (pc_isroot(pobj))
		{
		*path++ = BACKSLASH;
		*path = '\0';
		pc_freeobj(pobj);
		return(YES);
		}
	else
		{
		/* Find '..' so we can find the parent */
		if (!(pchild = pc_get_inode(NULL, pobj, "..      ", "   ") ) )
			{
			pc_freeobj(pobj);
			return (NO);
			}
		else
			{
			pc_freeobj(pobj);
			if ( (pobj = pc_get_mom(pchild)) == NULL)
				{
				pc_freeobj(pchild);
				return (NO);
				}
			if (!pc_gm_name(lname ,pobj, pchild))
				{
				pc_freeobj(pchild);
				pc_freeobj(pobj);
				return (NO);
				}
			else
				{
				pc_freeobj(pchild);
				if (pc_l_pwd(path, pobj))
					{
					pc_strcat(path, lname);
					pc_strcat(path, BACKSLASH_STR);
					return (YES);
					}
				else
					{
					return (NO);
					}
				}
			}
		}
	}


#ifndef ANSIFND
LOCAL BOOL pc_gm_name(path, pmom, pdotdot)
	TEXT *path;
	DROBJ *pmom;
	DROBJ *pdotdot;
	{
#else
LOCAL BOOL pc_gm_name(TEXT *path, DROBJ *pmom, DROBJ *pdotdot) /* _fn_ */
	{
#endif
	UCOUNT clusterno;
	UCOUNT fcluster;
	BLKBUFF *rbuf;
	FAST DIRBLK *pd;
	FAST DOSINODE *pi;
	FAST COUNT i;

	clusterno = pc_sec2cluster(pdotdot->pdrive, pdotdot->blkinfo.my_frstblock);

	/* For convenience. We want to get at block info here */
	pd = &pmom->blkinfo;
	/* Read the data */
	while (rbuf = pc_read_obj(pmom))
		{
		pi = (DOSINODE *) rbuf->pdata;
		/* Look for a match */
		for (i = 0; i <  INOPBLOCK; i++, pi++)
			{
			/* End of dir if name is 0 */
			if (!pi->fname[0])
				{
				pc_free_buf(rbuf,NO);
				return(NO);
				}

			if (pi->fname[0] != PCDELETE)
				{
				fcluster = to_WORD((UTINY *) &pi->fcluster);
				if (fcluster == clusterno)
					{
					pc_mfile (path, pi->fname, pi->fext);
					pc_free_buf(rbuf,NO);
					return(YES);
					}
				}
			}
		/* Not in that block. Try again */
		pc_free_buf(rbuf,NO);
		/* Update the objects block pointer */
		if (!pc_next_block(pmom))
			break;
		pd->my_index = 0;
		}
	return (NO);
	}

