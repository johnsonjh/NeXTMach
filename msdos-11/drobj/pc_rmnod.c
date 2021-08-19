/*
*****************************************************************************
	PC_RMNODE - Delete an inode unconditionally.

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_rmnode(pobj)
		DROBJ *pobj;

 Description
	Delete the inode at pobj and flush the file allocation table. Does not
	check file permissions or if the file is already open. (see also pc_unlink
	and pc_rmdir). The inode is marked deleted on the disk and the cluster
	chain associated with the inode is freed. (Un-delete won't work)

 Returns
 	Returns YES if it successfully deleted the inode an flushed the fat.


Example
	#include <pcdisk.h>

	Check to see if a simple file, then delete
	 if(!(pobj->finode->fattribute & ( ARDONLY | AVOLUME | ADIRENT)) )
			ret_val = pc_rmnode(pobj);
		pc_freeobj(pobj);

*****************************************************************************
*/
#include <pcdisk.h>
#ifdef	DEBUG
#import <sys/printf.h>
#endif	DEBUG

/* Delete a file / dir or volume. Dont chech for write access et al */
#ifndef ANSIFND
BOOL pc_rmnode(pobj)
	FAST DROBJ *pobj;
	{
#else
BOOL pc_rmnode(FAST DROBJ *pobj) /* _fn_ */
	{
#endif
	UCOUNT cluster;
	VOID pc_freechain();
	char fname[9];
	char fext[4];

	/* Don't delete anything that has multiple links */
	if (pobj->finode->opencount > 1)
		{
		pr_db_putstr("Trying to remove inode with open > 1\n");
#ifdef	DEBUG
		bcopy(pobj->finode->fname, fname, 8);
		fname[8] = '\0';
		bcopy(pobj->finode->fext, fext, 3);
		fext[3] = '\0';
		printf("fname <%s> fext <%s>\n", fname, fext);
#endif	DEBUG
		return (NO);
		}

	/* Mark it deleted and unlink the cluster chain */
	pobj->finode->fname[0] = PCDELETE;
	cluster = pobj->finode->fcluster;
	/* We free up store right away. Don't leave cluster pointer 
    hanging around to cause problems. */
	pobj->finode->fcluster = 0;
	if (pc_update_inode(pobj))
		{
		/* And clear up the space */
		pobj->finode->fcluster = cluster;
		pc_freechain(pobj);
		return (pc_flushfat(pobj->pdrive->driveno));
		}
	 /* If it gets here we had a probblem */
	return(NO);
	}

