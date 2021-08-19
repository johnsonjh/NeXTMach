/*
*****************************************************************************
	PC_ISROOT -  Test a DROBJ to see if it is the root directory

	   
Summary
	
	#include <pcdisk.h>

	BOOL pc_isroot(pobj)
		DROBJ *pobj;


 Description
 	Looks at the appropriate elements in pobj and determines if it is a root
 	directory.


 Returns
 	Returns NO if the obj does not point to the root directory.

 Example:

	#include <pcdisk.h>
	DROBJ *pmom;

	if (pc_isroot(pobj)  )
		printf("Already at root \n);


*****************************************************************************
*/
#include <pcdisk.h>

/* Get  the first block of a root or subdir */
#ifndef ANSIFND
BOOL pc_isroot(pobj)
	FAST DROBJ *pobj;
	{
#else
BOOL pc_isroot(FAST DROBJ *pobj) /* _fn_ */
	{
#endif
	return(pobj->isroot);
	}

