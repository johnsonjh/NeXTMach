/*
*****************************************************************************
	PC_FINDIN -  Find a filename in the same directory as the argument.

					   
Summary
	#include <pcdisk.h>

	BOOL pc_findin(pobj, filename, fileext)
		DROBJ *pobj;	
		TEXT *filename;
		TEXT *fileext;
		{

 Description
 	Look for the next match of filename or pattern filename:ext in the 
 	subdirectory containing pobj. If found update pobj to contain the 
 	new information  (essentially getnext.) Called by pc_get_inode().

	Note: Filename and ext must be right filled with spaces to 8 and 3 bytes
		  respectively. Null termination does not matter.

 Returns
 	Returns YES if found or NO.

Example:
	#include <pcdisk.h>

	Count files in the root directory.

	pmom = pc_get_root(pdrive)
	pobj = NULL;
	count = 0;

	if (pobj = pc_get_inode( pobj, pmom, "*       ", "*  "))
		{
		count++;
		while(pc_findin( pobj,"*       ", "*  "))
			count++;
		}
				
*****************************************************************************
*/
#include <pcdisk.h>

/* Find filename in the directory containing pobj. If found, load the inode
section of pobj. If the inode is already in the inode buffers we free the current inode
and stitch the existing one in, bumping its open count */
#ifndef ANSIFND
BOOL pc_findin(pobj, filename, fileext)
	FAST DROBJ *pobj;
	TEXT *filename;
	TEXT *fileext;
	{
#else
BOOL pc_findin(FAST DROBJ *pobj, TEXT *filename, TEXT *fileext) /* _fn_ */
	{
#endif
	BLKBUFF *rbuf;
	FAST DIRBLK *pd;
	FAST DOSINODE *pi;
	FINODE *pfi;

	/* For convenience. We want to get at block info here */
	pd = &pobj->blkinfo;
	
	/* Read the data */
	while (rbuf = pc_read_obj(pobj))
		{
		pi = (DOSINODE *) rbuf->pdata;

		/* Look at the current inode */
		pi += pd->my_index;

		/* And look for a match */
		while ( pd->my_index < INOPBLOCK )
			{
			/* End of dir if name is 0 */
			if (!pi->fname[0])
				{
				pc_free_buf(rbuf,NO);
				return(NO);
				}
			/* Note: Patcmp won't match on deleted */
			if (pc_patcmp(pi->fname, filename, 8) )
				if (pc_patcmp(pi->fext, fileext, 3) )
					{
					/* We found it */
					/* See if it already exists in the inode list.
					   If so.. we use the copy from the inode list */
					if (pfi = pc_scani(pobj->pdrive, rbuf->blockno,
									   pd->my_index) )
						{
						pc_freei(pobj->finode);
						pobj->finode = pfi;
						}
					else	/* No inode in the inode list. Copy the data over
							   and mark where it came from */
						{
						if (pfi = pc_alloci())
							{
							pc_freei(pobj->finode);	/* Release the current */
							pobj->finode = pfi;
							pc_dos2inode(pobj->finode , pi );
							pc_marki (pobj->finode , pobj->pdrive , pd->my_block, 
									  pd->my_index );
							}
						else
							{
							pc_free_buf(rbuf,NO);
							return (NO);
							}
						}
					/* Free, no error */
					pc_free_buf(rbuf,NO);
					return (YES);
					}
			 pd->my_index++;
			 pi++;
			}
		/* Not in that block. Try again */
		pc_free_buf(rbuf,NO);
		/* Update the objects block pointer */
		if (!pc_next_block(pobj))
			break;
		pd->my_index = 0;
		}

	return (NO);
	}

