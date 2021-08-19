/*
*****************************************************************************
	PC_ALLOCOBJ -  Allocate a DROBJ structure

					   
Summary
	
	#include <pcdisk.h>

	DROBJ *pc_allocobj()


 Description
	Allocates and zeroes the space needed to store a DROBJ structure. Also
	allocates and zeroes a FINODE structure and links the two via the
	finode field in the DROBJ structure.

 Returns
 	Returns a valid pointer or NULL if no more core.

Example:
	#include <pcdisk.h>

	DROBJ *pobj;

	if (!(pobj = pc_allocobj()))
		printf("Out of core\n")

*****************************************************************************
*/

#include <pcdisk.h>

IMPORT FINODE *inoroot;


#ifndef ANSIFND
DROBJ *pc_allocobj()
	{
#else
DROBJ *pc_allocobj() /* _fn_ */
	{
#endif
	FAST DROBJ *pobj;

	if ( !(pobj = (DROBJ *) pc_malloc(sizeof(DROBJ))) )
		return (NULL);
	else
		{
		pc_memfill(pobj, sizeof(DROBJ) , (UTINY) 0);
		if ( !(pobj->finode = pc_alloci()) )
			{
			pc_mfree(pobj);
			return(NULL);
			}
		else
			{
			return (pobj);
			}
		}
	}			
