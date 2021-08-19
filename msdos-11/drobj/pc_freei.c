/*
*****************************************************************************
	PC_FREEI -  Release an inode from service

					   
Summary
	
	#include <pcdisk.h>

	VOID pc_freei(pfi)
		FINODE *pfi;


Description
	If the FINODE structure is only being used by one file or DROBJ, unlink it
	from the internal active inode list and return it to the heap; otherwise
	reduce its open count.

 Returns
 	Nothing

Example:
	#include <pcdisk.h>

	pc_freei(pfi);
	
*****************************************************************************
*/

#include <pcdisk.h>

#ifndef ANSIFND
VOID pc_freei(pfi)
	FAST FINODE *pfi;
	{
#else
VOID pc_freei(FAST FINODE *pfi) /* _fn_ */
	{
#endif
	IMPORT FINODE *inoroot;

	if (!pfi)
		{
		pr_er_putstr((TEXT *)"Bad free call to freei\n");
		return;
		}

	if (pfi->opencount)
		{
		if (--pfi->opencount)/* Decrement opencount and return if non zero */
			return;
		else
			{
			if (pfi->pprev)		/* Pont the guy behind us at the guy in front*/
				{
				pfi->pprev->pnext = pfi->pnext;
				}
			else
				{
				inoroot = pfi->pnext; /* No prev, we were at the front so 
										 make the next guy the front */
				}

			if (pfi->pnext)			  /* Make the next guy point behind */
				{
				pfi->pnext->pprev = pfi->pprev;
				}
			}
		}

	pc_mfree(pfi);
	}


