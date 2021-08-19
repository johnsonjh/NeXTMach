/*
*****************************************************************************
	PC_GET_MOM -  Find the parent inode of a subdirectory.

					   
Summary
	#include <pcdisk.h>

	DROBJ *pc_get_mom(pdotdot) 
		DROBJ *pdotdot;
		{

 Description
 	Given a DROBJ initialized with the contents of a subdirectory's ".."
 	entry, initialize a DROBJ which is the parent of the current directory.

 Returns
 	Returns a DROBJ pointer or NULL if could something went wrong.

Example:
	#include <pcdisk.h>
	if (!(pchild = pc_get_inode(NULL, pobj, filename, fileext) ) )
		pc_freeobj(pobj);
	else
		{			
		if (pc_isdotdot( filename, fileext ))
			{
			 Find pobj's parent. By looking back from ".." 
			 if ( !(pobj = pc_get_mom(pchild)) ) 
			 	{
				pc_freeobj(pchild);
				return (NULL);
				}
			}
		}
				
*****************************************************************************
*/

/*
Get mom:
	if (!dotodot->cluster)  Mom is root. 
		getroot()
	else					cluster points to mom.
		find .. in mom
		then search through the directory pointed to by moms .. until
		you find mom. This will be current block startblock etc for mom.
*/
#include <pcdisk.h>

#ifndef ANSIFND
DROBJ *pc_get_mom(pdotdot) 
	FAST DROBJ *pdotdot;
	{
#else
DROBJ *pc_get_mom(FAST DROBJ *pdotdot) /* _fn_ */
	{
#endif
	FAST DROBJ *pmom;
	DDRIVE *pdrive = pdotdot->pdrive;
	BLOCKT sectorno;
	BLKBUFF *rbuf;
	FAST DIRBLK *pd;
	FAST DOSINODE *pi;
	FAST FINODE *pfi;


	/* We have to be a subdir */
	if (!pc_isadir(pdotdot))
		return(NULL);

	/* If ..->cluster is zero then parent is root */
	if (!pdotdot->finode->fcluster)
		return(pc_get_root(pdrive));

	/* Otherwise : cluster points to the beginning of our parent.
		    	   we also need the position of our parent in it's parent	*/

	if (!(pmom = pc_allocobj()) )
		return (NULL);

	pmom->pdrive = pdrive;
	/* Find .. in our parent's directory */
	sectorno = pc_cl2sector(pdrive,pdotdot->finode->fcluster); 
	/* We found .. in our parents dir. */
	pmom->pdrive = pdrive;
	pmom->blkinfo.my_frstblock =  sectorno; 
	pmom->blkinfo.my_block     =  sectorno;
	pmom->blkinfo.my_index 	 =	0;
	pmom->isroot = NO;
	pd = &pmom->blkinfo;

	/* See if the inode is in the buffers */
	if (pfi = pc_scani(pdrive, sectorno, 0) )
		{
		pc_freei(pmom->finode);
		pmom->finode = pfi;
		return (pmom);
		}
	/* Otherwise read it in */
	else if (rbuf = pc_read_obj(pmom))
		{
		pi = (DOSINODE *) rbuf->pdata;
		pc_dos2inode(pmom->finode , pi );
		pc_marki (pmom->finode , pmom->pdrive , pd->my_block, 
				  pd->my_index );
		pc_free_buf(rbuf,NO);
		return (pmom);
		}
	else 	/* Error, something did'nt work */
		{
		pc_freeobj(pmom);
		return (NULL);
		}
	}

