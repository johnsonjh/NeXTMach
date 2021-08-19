/*
*****************************************************************************
	PC_FREEOBJ -  Free a DROBJ structure

					   
Summary
	
	#include <pcdisk.h>

	VOID pc_freeobj(pobj)
		DROBJ *pobj;


 Description
 	Return a drobj structure to the heap. Calls pc_freei to reduce the
 	open count of the finode structure it points to and return it to the
 	heap if appropriate.

 Returns
 	Nothing

Example:
	#include <pcdisk.h>

	pc_freeobj(pobj)
	
*****************************************************************************
*/
#include <pcdisk.h>
	
#ifndef ANSIFND
VOID pc_freeobj(pobj)
	FAST DROBJ *pobj;
	{
#else
VOID pc_freeobj(FAST DROBJ *pobj) /* _fn_ */
	{
#endif
	if (pobj)
		{
		if (!pobj->isroot)
			pc_freei(pobj->finode);
		pc_mfree(pobj);
		}
	else
		pr_er_putstr((TEXT *)"Bad free call to freeobj\n");
	}


