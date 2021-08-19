/*
*****************************************************************************
	PC_FREE_ALL_I -  Release all inode buffers associated with a drive.

				   
Summary
	
	#include <pcdisk.h>

	VOID	pc_free_all_i(pdrive)
		DDRIVE *pdrive;

Description
	Called by pc_dskclose().
	For each internally buffered finode (dirent) check if it exists on
	pdrive. If so delete it. In debug mode print a message since all
	finodes should be freed before pc_dskclose is called.

Returns
	Nothing

Example:
	#include <pcdisk.h>
		
		pc_free_all_i(pdrive);
			
	
*****************************************************************************
*/
#include <pcdisk.h>

#ifndef ANSIFND
VOID	pc_free_all_i(pdrive)
	FAST DDRIVE *pdrive;
	{
#else
VOID	pc_free_all_i(FAST DDRIVE *pdrive) /* _fn_ */
	{
#endif
	IMPORT FINODE *inoroot;
	FAST FINODE *pfi;

	pfi = inoroot;
	while (pfi)
		{
		if ( (pfi->my_drive == pdrive) )
			{
			pfi->fname[7] = '\0';
			pr_db_str((TEXT *)"Warning: pc_free_all_i, freeing a finode:",pfi->fname);
			pc_freei(pfi);
			/* Since we changed the list go back to the top */
			pfi = inoroot;
			}
		else
			pfi = pfi->pnext;
		}
	}
