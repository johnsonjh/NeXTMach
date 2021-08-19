/*
*****************************************************************************
	PC_SCANI -  Search for an inode in the internal inode list.

				   
Summary
	
	#include <pcdisk.h>

	FINODE *pc_scani(pdrive, sectorno, index)
		DDRIVE *pdrive;
		BLOCKT sectorno;
		COUNT  index;



Description
	Each inode is uniquely determined by DRIVE, BLOCK and Index into that 
	block. This routine searches the current active inode list to see
	if the inode is in use. If so the opencount is changed and a pointer is 
 	returned. This guarantees that two processes will work on the same 
 	information when manipulating the same file or directory.

Returns
 	A pointer to the FINODE for pdrive:sector:index or NULL if not found

Example:
	#include <pcdisk.h>

	Use the inode in the inode list if it exists; otherwise add it 
	to the list.

	if (pfi = pc_scani(pobj->pdrive, rbuf->blockno,
					   pd->my_index) )
		{
		pc_freei(pobj->finode);
		pobj->finode = pfi;
		}
	else
		{
		if (pfi = pc_alloci())
			{
			pc_freei(pobj->finode);	 Release the current 
			pobj->finode = pfi;
			pc_dos2inode(pobj->finode , pi );
			pc_marki (pobj->finode , pobj->pdrive , pd->my_block, 
					  pd->my_index );
			}


	
*****************************************************************************
*/
#include <pcdisk.h>
		
/* See if the inode for drive,sector , index is in the list. If so..
   bump its open count and return it. Else return NULL */
#ifndef ANSIFND
FINODE *pc_scani(pdrive, sectorno, index)
	FAST DDRIVE *pdrive;
	FAST BLOCKT sectorno;
	FAST COUNT  index;
	{
#else
FINODE *pc_scani(FAST DDRIVE *pdrive, FAST BLOCKT sectorno, FAST COUNT  index) /* _fn_ */
	{
#endif
	IMPORT FINODE *inoroot;
	FAST FINODE *pfi;

	pfi = inoroot;
	while (pfi)
		{
		if ( (pfi->my_drive == pdrive) &&
		     (pfi->my_block == sectorno) &&
		     (pfi->my_index == index) )
				{
				pfi->opencount += 1;
				return (pfi);
				}
		pfi = pfi->pnext;
		}
	return (NULL);
	}


