/*
*****************************************************************************
	PC_ALLOCI -  Allocate a FINODE structure

					   
Summary
	
	#include <pcdisk.h>

	FINODE *pc_alloci()


 Description
	Allocates and zeroes a FINODE structure.

 Returns
 	Returns a valid pointer or NULL if no more core.

Example:
	#include <pcdisk.h>

	FINODE *pfi;

	if (!(pfi = pc_alloci()))
		printf("Out of core\n")

*****************************************************************************
*/
#include <pcdisk.h>


#ifndef ANSIFND
FINODE *pc_alloci()
	{
#else
FINODE *pc_alloci() /* _fn_ */
	{
#endif
	FAST FINODE *pi;

	if (pi = (FINODE *)(pc_malloc(sizeof(FINODE))) )
		pc_memfill(pi, sizeof(FINODE) , (UTINY) 0);
	
	return (pi);
	}

