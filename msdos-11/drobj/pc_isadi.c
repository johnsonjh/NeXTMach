/*
*****************************************************************************
	PC_ISADIR -  Test a DROBJ to see if it is a root or subdirectory

		   
Summary
	
	#include <pcdisk.h>

	BOOL pc_isadir(pobj)
		DROBJ *pobj;


 Description
 	Looks at the appropriate elements in pobj and determines if it is a root
 	or subdirectory.


 Returns
 	Returns NO if the obj does not point to a directory.

 Example:

	#include <pcdisk.h>
	DROBJ *pmom;

	if (! (pc_isadir(pmom)  )
		printf("Directory operations not allowed on non directories \n);


*****************************************************************************
*/
#include <pcdisk.h>

#ifndef ANSIFND
BOOL pc_isadir(pobj)
	FAST DROBJ *pobj;
	{
#else
BOOL pc_isadir(FAST DROBJ *pobj) /* _fn_ */
	{
#endif
	return ( (pobj->isroot) || (pobj->finode->fattribute & ADIRENT)  );
	}

