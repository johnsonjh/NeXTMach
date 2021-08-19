/*
 * History
 * -------
 * 10-Oct-90 	Written by John Anderson at NeXT
 *		po_seteof has been added to fix the ftruncate with a negative
 *		argument failing feature.
 *
*****************************************************************************
	PO_SETEOF	-  Sets the end of file to a given size.

Summary
	
	#include <posix.h>

	INT po_seteof(fd, newsize)
		PCFD 	fd;
		ULONG	newsize;

 Description
	Attempt to set the end of file to newsize bytes from the beginning of
	the file. Extends the file if newsize is greater than the current end 
	of file, filling the file with zeros.  Files may never have "holes" in 
	them.  Shortens the file if newsize is less than the current end of 
	file.  If file pointer is set to the new end of file if it would be 
	beyond the new end of file.
	
	Added 8 October 90 -- DJA


 Returns
	Returns 0 if successful and -1 on error.
	If the return value is -1 p_errno will be set with one of the 
	following:

	PENBADF		- File descriptor invalid or open read only
	PENOSPC		- Write failed. Presumably because of no space
			  or write protection.

Example:
	#include <posix.h>

	PCFD fd;
	IMPORT INT p_errno;
	if (po_seteof(fd, 512)  < 0)
		printf("Cant write to file error:%i\n",p_errno)
		

*****************************************************************************
*/

#include <posix.h>
#import <nextdos/dosdbg.h>

#ifdef	DEBUG
#define CLUSTER_DEBUG	1
#endif	DEBUG
/* #define FLUSH_FILE	1		/* */
/* #define FLUSH_FAT	1		/* */

IMPORT INT p_errno;

INT po_seteof(PCFD fd, ULONG newsize) /* _fn_ */
{
	UCOUNT cluster;
	ULONG fatindex;
	UCOUNT lastcluster;
	ULONG neweof;
	ULONG newmaxfatindex;
	UCOUNT nextcluster;
	ULONG oldeof;
	ULONG oldmaxfatindex;
	ULONG oldsize;
	DDRIVE *pdrive;
	FAST PC_FILE *pfile;
	LONG result;

	p_errno = 0;
	/* 
	 * Get the FILE. We don't want it if an error has occured.
	 */	
	if ( (pfile = pc_fd2file(fd, NO)) == NULL) {
		dbg_api(("po_seteof: returning PEBADF\n"));
		p_errno = PEBADF;
		return(-1);
	}
#ifdef	FLUSH_FILE
	pc_fileflush (pfile);
#endif	FLUSH_FILE

	oldsize = pfile->pobj->finode->fsize;
	dbg_api(("po_seteof: fd %d newsize %u oldsize %u\n", 
		fd, newsize, oldsize));
		
	/* 
	 * If we are setting it to the same size as the current size then we're 
	 * done.
	 */	
	if (oldsize == newsize)
		return (0);	
	
	neweof = newsize;
	if (neweof != 0)
		neweof --;

	oldeof = oldsize;
	if (oldeof != 0)
		oldeof --;

	/* 
	 * Exit with an error if we don't have write permission.
	 */	
	if (!((pfile->flag & PO_WRONLY) || (pfile->flag & PO_RDWR))) {
		p_errno = PEBADF;
		dbg_api(("po_seteof: returning PEBADF\n"));
		return(-1);
	}
#ifdef	CLUSTER_DEBUG
	check_cluster(pfile, "begin po_seteof");
#endif	CLUSTER_DEBUG

	/* 
	 * Zoom through the FAT looking for the last cluster or the FAT 
	 * newmaxfatindex clusters down the chain, which ever comes first. 
	 *
	 * {old,new}maxfatindex are "# of clusters - 1" for current/desired
	 * files.
	 */

	fatindex = 0;
	newmaxfatindex = neweof / pfile->pobj->pdrive->bytespcluster;
	oldmaxfatindex = oldeof / pfile->pobj->pdrive->bytespcluster;
	lastcluster = 0;
	cluster = pfile->pobj->finode->fcluster;
	pdrive = pfile->pobj->pdrive;
	while ((cluster != 0) && (fatindex <= newmaxfatindex)) {
		lastcluster = cluster;
		cluster = pc_clnext(pdrive, cluster);
		fatindex++;
	}
	
	/*
	 * Append case:
	 *	cluster  = 0
	 *      lastcluster = cluster # at end of chain
	 * Truncate case:
	 * 	cluster = # of first cluster to nuke.
	 *	lastcluster = new end of chain.
	 *
	 * See if we need to add or remove clusters.
	 */
	dbg_api(("po_seteof: fatindex %d newmaxfatindex %d "
		"oldmaxfatindex %d\n",
		fatindex, newmaxfatindex, oldmaxfatindex));
	if (newmaxfatindex != oldmaxfatindex) {
		if (newmaxfatindex < oldmaxfatindex) {
			/* 
			 * remove clusters.
			 */
			dbg_api(("po_seteof - REMOVING: ccl: %d  bccl: %d"
				" fsize: %d fptr: %d\n", pfile->ccl,
				pfile->bccl, pfile->pobj->finode->fsize, 
				pfile->fptr));
			while (cluster) {
			    nextcluster = pc_clnext(pdrive, cluster);
			    dbg_api(("po_seteof: remove cluster number"
				    " %d\n", cluster));
			    pc_clrelease(pdrive, cluster);
			    /* 
			     * If buffer cluster is removed then 
			     * update bdirty and bccl.
			     */
			    if (cluster == pfile->bccl) {
				    dbg_api(("po_seteof: removing buffer "
					    "cluster\n"));
				    pfile->bdirty = 0;
				    pfile->bccl = 0;
			    }
			    cluster = nextcluster;
			}
			dbg_api(("po_seteof - done  ccl %d bccl: %d fsize %d "
				"fptr: %d\n", pfile->ccl, pfile->bccl,
				pfile->pobj->finode->fsize, pfile->fptr));
		}
		else { /* add clusters */
			do { /* Adding clusters has never been tested! */
				dbg_api(("adding cluster number: %d\n", 
					cluster));
				cluster = pc_clm_next(pfile->pobj,
					lastcluster);
				if (cluster == 0) {
					p_errno = PENOSPC;
					dbg_api(("seteof returning "
						"PENOSPC\n"));
					return (-1);
				}
				/* 
				 * Zero out contents of cluster.
				 */
				pc_clzero(pdrive, cluster); 
				lastcluster = cluster;
				oldmaxfatindex ++;
			} while (oldmaxfatindex < newmaxfatindex);
		} /* adding clusters */
		/*
		 * After either adding or removing clusters we need
		 * to mark the last cluster in the chain with end of
		 * file.  We then flush the fat.
		 */
		if (lastcluster != 0) {
			if (!pc_pfaxx (pdrive, lastcluster,0xffff)) {
				p_errno = PENOSPC;
				dbg_api(("seteof returning "
					"PENOSPC\n"));
				return(-1);
			}
		}
#ifdef	DEBUG
		else
			printf("po_seteof: BOGUS LASTCLUSTER\n");
#endif	DEBUG
#ifdef	FLUSH_FAT
		if(pc_flushfat(pfile->pobj->pdrive->driveno) == NO) {
			dbg_api(("seteof returning flushfat "
				"failure\n"));
			return(-1);
		}
#endif	FLUSH_FAT

	} /* # of clusters changed */
		
	pfile->pobj->finode->fsize = newsize;
	
	/* 
	 * set file pointer to the new size 
	 */	
	if (po_lseek(fd, newsize, PSEEK_SET) == -1) {
		dbg_api(("po_seteof: returning bad seek\n"));
		return (-1); /* po_lseek sets p_errno */
	}

#ifdef	CLUSTER_DEBUG
	check_cluster(pfile, "end po_seteof");
#endif	CLUSTER_DEBUG
	dbg_api(("po_seteof OUT:  ccl: %d  bccl: %d  fsize: %d  fptr: %d\n",
		pfile->ccl, pfile->bccl, pfile->pobj->finode->fsize, 
		pfile->fptr));
	return(0);
}
