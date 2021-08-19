
/*
*****************************************************************************
	PC_MARKI -  Set dr:sec:index info and stitch a FINODE into the inode list

				   
Summary
	
	#include <pcdisk.h>

	VOID pc_marki(pfi , pdrive, sectorno, index)
		FINODE *pfi;
		DDRIVE *pdrive;
		BLOCKT sectorno;
		COUNT  index;
		{


Description
	Each inode is uniquely determined by DRIVE, BLOCK and Index into that 
	block. This routine takes an inode structure assumed to contain the
	equivalent of a DOS directory entry. And stitches it into the current
	active inode list. Drive block and index are stored for later calls
	to pc_scani and the inode's opencount is set to one.

Returns
	Nothing

Example:
	#include <pcdisk.h>


	if (pfi = pc_scani(pobj->pdrive, rbuf->blockno,   Found it
					   pd->my_index) )
		{
		pc_freei(pobj->finode);
		pobj->finode = pfi;                           So use it
		}
	else
		{
		if (pfi = pc_alloci())                      Create a new one
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

/* Take an unlinked inode and link it in to the inode chain. Initialize
   the open count and sector locator info. */	
#ifndef ANSIFND
VOID pc_marki(pfi , pdrive, sectorno, index)
	FAST FINODE *pfi;
	DDRIVE *pdrive;
	BLOCKT sectorno;
	COUNT  index;
	{
#else
VOID pc_marki(FAST FINODE *pfi, DDRIVE *pdrive, BLOCKT sectorno, COUNT  index) /* _fn_ */
	{
#endif
	IMPORT FINODE *inoroot;

	pfi->my_drive = pdrive;
	pfi->my_block = sectorno;
	pfi->my_index = index;
	pfi->opencount = 1;

	/* Stitch the inode at the front of the list */
	if (inoroot)
		inoroot->pprev = pfi;

	pfi->pprev = NULL;
	pfi->pnext = inoroot;

	inoroot = pfi;
	}


