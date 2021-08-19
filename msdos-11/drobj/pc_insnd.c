
/*
*****************************************************************************
	PC_INSERT_INODE - Insert a new inode into an existing directory inode.

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_insert_inode(pobj, pmom)
		DROBJ *pobj;
		DROBJ *pmom;
		{

Description
	 Take mom , a fully defined DROBJ, and pobj, a DROBJ with a finode
   	 containing name, ext, etc, but not yet stitched into the inode buffer
     pool, and fill in pobj and its inode, write it to disk and make the inode
     visible in the inode buffer pool. (see also pc_mknode() )



Returns
 	Returns YES if all went well, NO on a write error, disk full error or 
 	root directory full.

Example:
	#include <pcdisk.h>

	Load the inode copy name,ext,attr,cluster, size,datetime
	pc_init_inode( pobj->finode, filename, fileext, 
					attr, cluster, 0L ,pc_getsysdate(&crdate) );

	Convert pobj to native and stitch it in to mom 
	if (!pc_insert_inode(pobj , pmom))
		{
		pc_freeobj(pobj);
		pc_freeobj(pmom);
		pc_clrelease(pdrive , cluster);
		return (NULL);
		}


*****************************************************************************
*/
#include <pcdisk.h>

#ifndef ANSIFND
BOOL pc_insert_inode(pobj, pmom)
	FAST DROBJ *pobj;
	FAST DROBJ *pmom;
	{
#else
BOOL pc_insert_inode(FAST DROBJ *pobj, FAST DROBJ *pmom) /* _fn_ */
	{
#endif
	BLKBUFF *pbuff;
	FAST DIRBLK *pd;
	FAST DOSINODE *pi;
	FAST COUNT i;
	BOOL retval;
	UCOUNT cluster;

	/* Set up pobj */	
	pobj->pdrive = pmom->pdrive;
	pobj->isroot = NO;
	pd = &pobj->blkinfo;

	/* Now get the start of the dir */
	if (! (pd->my_block = pd->my_frstblock = pc_firstblock(pmom) ) )
		{
	    return (NO);
		}
	else
		pd->my_index = 0;

	/* Read the data */
	while (pbuff = pc_read_obj(pobj))
		{
		i = pd->my_index = 0;
		pi = (DOSINODE *) pbuff->pdata;
		/* look for a slot */
		while ( i < INOPBLOCK )
			{
			/* End of dir if name is 0 */
			if ( (pi->fname[0] == '\0') || (pi->fname[0] == PCDELETE) )
				{
				pd->my_index = i;

				/* Update the DOS disk */
				pc_ino2dos( pi, pobj->finode );
				/* Mark the inode in the inode buffer, if the write works */
				if (retval =  pc_write_obj( pobj ))
					pc_marki (pobj->finode , pobj->pdrive , pd->my_block, 
						  pd->my_index );

				pc_free_buf(pbuff, !retval);
				return (retval);
				}
			i++;
			pi++;
			}
		/* Not in that block. Try again */
		pc_free_buf(pbuff,NO);
		/* Update the objects block pointer */
		if (!pc_next_block(pobj))
			break;
		}

	/* Hmmm - root full ??. This is a problem */
	if (pc_isroot(pmom))
		return (NO);

	/* Ok:There ar no slots in mom. We have to make one. And copy our stuf in*/

	if (!(cluster = pc_clgrow(pobj->pdrive , pmom->finode->fcluster) ) )
			return (NO);

	/* Zero out the cluster  */
	if (!pc_clzero( pobj->pdrive , cluster ) )
		{
		pc_clrelease(pobj->pdrive , cluster);
		return (NO);
		}

	/* Don't forget where the new item is */
	pd->my_block = pc_cl2sector(pobj->pdrive , cluster);
	pd->my_index = 0;

	/* Copy the item into the first block */
	if (!(pbuff = pc_init_blk( pobj->pdrive , pd->my_block)))
		{
		pc_clrelease(pobj->pdrive , cluster);
		return (NO);
		}

	pc_ino2dos (  (DOSINODE *) pbuff->pdata ,  pobj->finode ) ;

	/* Write it out */
	if ( !pc_write_blk ( pbuff ) )
		{
		pc_free_buf(pbuff,YES);
		pc_clrelease(pobj->pdrive , cluster);
		return (NO);
		}

	/* We made a new slot. Mark the inode as belonging there */
	pc_marki (pobj->finode , pobj->pdrive , pd->my_block, pd->my_index );

	pc_free_buf(pbuff,NO);
	return (YES);
	}


