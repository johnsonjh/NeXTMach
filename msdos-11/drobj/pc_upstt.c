/*
*****************************************************************************
	PC_UPSTAT - Copy private information to public fields for a DSTAT struc.

Summary
	
	#include <pcdisk.h>

	VOID pc_upstat(statobj)
		DSTAT *statobj;

 Description
	Given a pointer to a DSTAT structure that contains a pointer to an 
	initialized DROBJ structure, load the public elements of DSTAT with
	name filesize, date of modification et al. (Called by pc_gfirst &
	pc_gnext)

 Returns
 	Nothing

Example:
	#include <pcdisk.h>

	DSTAT *statobj;

	find the next instance of pattern in the directory 
	if (nextobj = pc_get_inode(statobj->pobj, statobj->pmom, statobj->pname,
							   statobj->pext))
		{
		statobj->pobj = nextobj;
		And update the stat structure 
		pc_upstat(statobj);

*****************************************************************************
*/
#include <pcdisk.h>

/* Copy internal stuf so the outside world can see it */
#ifndef ANSIFND
VOID pc_upstat( statobj)
	DSTAT *statobj;
	{
#else
VOID pc_upstat(DSTAT *statobj) /* _fn_ */
	{
#endif
	DROBJ *pobj;
	FAST FINODE *pi;
	pobj = statobj->pobj;

	pi = pobj->finode;
	
	copybuff( statobj->fname, pi->fname, 8);
	statobj->fname[8] = '\0';
	copybuff( statobj->fext, pi->fext, 3);
	statobj->fext[3] = '\0';

	statobj->fattribute = pi->fattribute;
	statobj->ftime = pi->ftime;
	statobj->fdate = pi->fdate;
	statobj->fsize = pi->fsize;
	}

