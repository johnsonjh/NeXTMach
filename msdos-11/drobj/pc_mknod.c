/*
*****************************************************************************
	PC_MKNODE -  Create an empty subdirectory or file.

					   
Summary
	
	#include <pcdisk.h>

	DROBJ *pc_mknode(path, attributes)
		TEXT *path;
		UTINY attributes;

 Description
	Creates a file or subdirectory ("inode") depending on the flag values in
	attributes.	A pointer to an inode is returned for further processing. See
	po_open(),po_close(), pc_mkdir() et al for examples.

	Note: After processing, the DROBJ must be released by calling pc_freeobj.


 Returns
 	Returns a pointer to a DROBJ structure for further use, or NULL if the
 	inode name already exists or path not found.

Example:

	#include <pcdisk.h>
	DROBJ *pobj;

	Make an empty directory 
  	if (pobj = pc_mknode("D:\usr\pcdos\devt",ADIRENT) )
		{
		pc_freeobj(pobj);
		return (YES);
		}

	Make an volume label 
  	if (pobj = pc_mknode("D:MYDISK",AVOLUME ))
		{
		pc_freeobj(pobj);
		return (YES);
		}

	 Make an empty file and return the object for processing 
  	if (pobj = pc_mknode("D:MYFILE",ANORMAL ))
		return(pobj);


*****************************************************************************
*/

#include <pcdisk.h>


/* Make a node from path and attribs create and fill in pobj */
#ifndef ANSIFND
DROBJ *pc_mknode(path, attributes)
	TEXT *path;
	UTINY attributes;
	{
#else
DROBJ *pc_mknode(TEXT *path, UTINY attributes) /* _fn_ */
	{
#endif
	FAST DROBJ *pmom;
	FAST DROBJ *pobj;
	BOOL ret_val;
 	DOSINODE *pdinodes;
 	FINODE lfinode;
	UCOUNT cluster;
	DATESTR crdate;
	BLKBUFF *pbuff;
	TEXT mompath[EMAXPATH];
	TEXT filename[9];
	TEXT fileext[4];
	DDRIVE *pdrive;
	UTINY attr;


	/* Get out the filename and d:parent */
	if (!pc_parsepath(mompath,filename,fileext,path))
		return (NO);

	/* Find the parent */
	pmom = NULL;
	pobj = NULL;

	ret_val = YES;
	
	if (!(pmom =  pc_fndnode(mompath) ) )
		return (NULL);
	else if (! (pdrive = pmom->pdrive) )
		ret_val = NO;
	/* And make sure she is a directory */
	else if ( !pc_isadir(pmom))
		ret_val = NO;
	else if (attributes & ADIRENT)
		{
		/*Grab a cluster for a new dir and cleaer it */
		 if (!(cluster = pc_clalloc(pdrive) ) )
			ret_val = NO;
		else if (!pc_clzero( pdrive , cluster ) )
			{
			pc_clrelease(pdrive , cluster);
			ret_val = NO;
			}
		}
	else
		cluster = 0;
	
	if (!ret_val)
		{
		pc_freeobj(pmom);
		return (NULL);
		}

	/* For a subdirectory. First make it a simple file. We will change the
	   attribute after all is clean */
	if ( (attr = attributes) & ADIRENT)
		attr = ANORMAL;

	/* Allocate an empty DROBJ and FINODE to hold the new file */
	if (!(pobj = pc_allocobj()))
		{
		pc_freeobj(pmom);
		return (NULL);
		}

	/* Load the inode copy name,ext,attr,cluster, size,datetime*/
	pc_init_inode( pobj->finode, filename, fileext, 
					attr, cluster, /*size*/ 0L ,pc_getsysdate(&crdate) );

	/* Convert pobj to native and stitch it in to mom */
	if (!pc_insert_inode(pobj , pmom))
		{
		pc_freeobj(pobj);
		pc_freeobj(pmom);
		pc_clrelease(pdrive , cluster);
		return (NULL);
		}
	else
		pc_freeobj(pmom);	/* Free mom, were all done with her */
		

	/* Now if we are creating subdirectory we have to make the DOT and DOT DOT
	  inodes and then change pobj's attribute to ADIRENT
	  The DOT and DOTDOT are not buffered inodes. We are simply putting
	  the to disk  */
	if ( attributes & ADIRENT)
		{
		/* Set up a buffer to do surgery */
		if (!(pbuff = pc_init_blk( pdrive , pc_cl2sector(pdrive , cluster))))
			{
			pc_clrelease(pdrive , cluster);
			pc_freeobj(pobj);
			return (NULL);
			}
		pdinodes = (DOSINODE *) pbuff->pdata;
		/* Load DOT and DOTDOT in native form */
		/* DOT first. It points to the begining of this sector */
		pc_init_inode( &lfinode, ".", "", ADIRENT, 
					       cluster, /*size*/ 0L ,&crdate);
		/* And to the buffer in intel form */
		pc_ino2dos (pdinodes, &lfinode);

		/* Now DOTDOT points to mom's cluster */
		pc_init_inode( &lfinode, "..", "", ADIRENT, 
					  pc_sec2cluster( pdrive , pobj->blkinfo.my_frstblock),
					 /*size*/ 0L ,&crdate);
		/* And to the buffer in intel form */
		pc_ino2dos ( ++pdinodes, &lfinode );
		
		/* Write the cluster out */
		if ( !pc_write_blk ( pbuff ) )
			{
			pc_freeobj(pobj);
			pc_free_buf(pbuff,YES);			/* Error. Chuck the buffer */
			pc_clrelease(pdrive , cluster);
			return (NULL);
			}
		else
			pc_free_buf(pbuff,NO);

		/* And write the node out with the original attributes */
		pobj->finode->fattribute = attributes;

		/* Convert to native and overwrite the existing inode*/
		if (!pc_update_inode(pobj))
			{
			pc_freeobj(pobj);
			pc_clrelease(pdrive , cluster);
			return (NULL);
			}
		}

	if (pc_flushfat(pdrive->driveno))
		return (pobj);
	else
		{
		pc_freeobj(pobj);
		return (NULL);
		}
	}
