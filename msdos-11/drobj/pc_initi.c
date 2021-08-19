
/*
*****************************************************************************
	PC_INIT_INODE -  Load an in memory inode up with user supplied values.

					   
Summary
	
	#include <pcdisk.h>

	VOID pc_init_inode( pdir, filename, fileext, attr, cluster,size,crdate) 
		FINODE *pdir;
		TEXT *filename;
		TEXT *fileext;
		UTINY attr;
		UCOUNT cluster;
		ULONG size;
		DATESTR *crdate;

 Description
 	Take an uninitialized inode (pdir) and fill in some fields. No other
 	processing is done. This routine simply copies the arguments into the
 	FINODE structure. 

	Note: filename & fileext do not need null termination.

 Returns
	Nothing

Example:
	#include <pcdisk.h>
	FINODE lfinode;
	DOSINODE *pdinodes;

	Create the "." and ".." entries for a subdirectory.
	pdinodes = (DOSINODE *) pbuff->pdata;
	
	pc_init_inode( &lfinode, ".", "", ADIRENT,cluster, 0L ,&crdate);
	pc_ino2dos (pdinodes, &lfinode);
	pc_init_inode( &lfinode, "..", "", ADIRENT, 
					  pc_sec2cluster( pdrive , pobj->blkinfo.my_frstblock),
					  0L ,&crdate);
	pc_ino2dos ( ++pdinodes, &lfinode );
	pc_write_blk ( pbuff );

*****************************************************************************
*/
#include <pcdisk.h>

	/* Load an in memory inode up with user supplied values*/
#ifndef ANSIFND
VOID pc_init_inode( pdir, filename, fileext, attr, cluster,size,crdate) 
	FINODE *pdir;
	TEXT *filename;
	TEXT *fileext;
	UTINY attr;
	UCOUNT cluster;
	ULONG size;
	DATESTR *crdate;
	{
#else
VOID pc_init_inode(FINODE *pdir, TEXT *filename, TEXT *fileext, UTINY attr, UCOUNT cluster, ULONG size, DATESTR *crdate) /* _fn_ */
	{
#endif
	/* Copy the file names and pad with ' ''s */
	pc_cppad(&pdir->fname[0],filename,8);
	pc_cppad(&pdir->fext[0],fileext,3);
	pdir->fattribute = attr;
	pc_memfill(&pdir->resarea[0],10, '\0');

	pdir->ftime = crdate->time;
	pdir->fdate = crdate->date;
	pdir->fcluster = cluster;
	pdir->fsize = size;
	}

