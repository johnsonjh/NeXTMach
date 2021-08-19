/*
 * merged version of po_{read,write}.c
 *
 * History
 * -------
 * 10-Oct-90 	Doug Mitchell at NeXT
 *		po_read() and po_write() now take and return LONGs as byte 
 *			counts
 *
*****************************************************************************
	PO_READ	-  Read from a file.

Summary
	
	#include <posix.h>

	LONG po_read(fd, buf, count)
		PCFD 	fd;
		UTINY 	*buf;
		LONG	count;

 Description
	Attempt to read count bytes from the current file pointer of file at fd
	and put them in buf. The file pointer is updated.


 Returns
	Returns the	number of bytes read or -1 on error.
	If the return value is -1 p_errno will be set with one of the following:

	PENBADF		- File descriptor invalid

Example:
	#include <posix.h>

	PCFD fd;
	PCFD fd2;
	IMPORT INT p_errno;

	if ( (fd = pc_open("FROM.FIL",PO_RDONLY,0)) >= 0)
		if ( (fd2 = pc_open("TO.FIL",PO_CREAT|PO_WRONLY,PS_IWRITE)) >= 0)
			while ( po_read(fd, buff, 512) > 0)
				 po_write(fd2, buff, 512);
	

*****************************************************************************
*/
#include <posix.h>
#import <nextdos/dosdbg.h>

#ifdef	DEBUG
/* #define CLUSTER_DEBUG	1		/* */
#endif	DEBUG

static UCOUNT pc_file_extend(PC_FILE *pfile);

IMPORT INT p_errno;

#ifndef ANSIFND
LONG po_read(fd, buf, count)
	PCFD 	fd;
	UTINY 	*buf;
	LONG	count;
	{
#else
LONG po_read(PCFD 	fd, UTINY 	*buf, LONG	count) /* _fn_ */
	{
#endif
	PC_FILE *pfile;
	UTINY *pfbuf;		/* Fast access to the file buffer */
	LONG i;
	LONG nx = 0;

	p_errno = 0;	

	dbg_api(("po_read: fd %d count %d\n", fd, count));
	/* Get the FILE. We don't want it if an error has occured */	
	if ( (pfile = pc_fd2file(fd, NO)) == NULL)
		{
		p_errno = PEBADF;
		return(-1);
		}

	if (!count || !buf)
		return (0);	

	if (pfile->fptr >= pfile->pobj->finode->fsize)	/* Dont read if done */
		return(0);

	/* This shouldnt happen in a size file. But just in case */
	if (!pfile->ccl)	
		return (0);

	/* Read the current cluster into our buffer if not already there */
	if (pfile->bccl != pfile->ccl)
		{
		/* Update the disk if the cluster is dirty */
		if (!pc_fileflush (pfile))
			{
			p_errno = PENOSPC;
			return (-1);
			}
		/* And read the cluster */
		else if (!(pc_rd_cluster(pfile->bbase,
				   pfile->pobj->pdrive,pfile->ccl)))
			{
			return (-1);
			}
		else
			{
			pfile->bdirty = NO;
			pfile->bccl = pfile->ccl;
			}
		}

	/* Divide the filepointer by the cluster size and take the remainder to
	   get the offset into the buffer */
	pfbuf = pfile->bbase + (pfile->fptr % pfile->pobj->pdrive->bytespcluster);

	nx = 0;		/* Bytes xfered */

	for (i = 0;i < count;i++)
		{
		if (pfile->fptr >= pfile->pobj->finode->fsize)	/* Dont read if done */
			break;
		if (pfbuf > pfile->bend)
			{
			/* Flush the buffer if it is dirty */
			if (!pc_fileflush (pfile))
				{
				p_errno = PENOSPC;
				return (-1);
				}
			if (!(pfile->ccl =
				 pc_rd_next( pfile->pobj, pfile->bbase , pfile->ccl)) )
			 	{
			 	pfile->error = YES;
				return(-1);			/* Unexp End of file */
				}
			pfile->bdirty = NO;
			pfile->bccl = pfile->ccl;
			pfbuf = pfile->bbase;
			}
		nx++;
		*buf++ = *pfbuf++;
		pfile->fptr++;
		}

	/* Clean up at the end. If we finished a cluster */
	if (pfbuf > pfile->bend)		/* Buffer full */
		{
		if (pfile->fptr >= pfile->pobj->finode->fsize)
			pfile->ccl = 0;		/* Write will claim a new cluster next time*/
		else					/* Or get next in chain */
			pfile->ccl = pc_clnext(pfile->pobj->pdrive, pfile->bccl);
		}

	return(nx);
	}

/* Get the next cluster from a chain. If cluster is zero read the first */
/* Return the cluster number if successful, else zero */
#ifndef ANSIFND
UCOUNT pc_rd_next( pobj, ddest, cluster)
	FAST DROBJ *pobj;
	UTINY *ddest;
	UCOUNT cluster;
	{
#else
UCOUNT pc_rd_next(FAST DROBJ *pobj, UTINY *ddest, UCOUNT cluster) /* _fn_ */
	{
#endif
	DDRIVE *pdrive =   pobj->pdrive;

	if (!cluster)
		cluster = pobj->finode->fcluster;
	else
		cluster = pc_clnext(pdrive, cluster);

#ifdef	CLUSTER_DEBUG
	dbg_api(("pc_rd_next: READING cluster %d\n", cluster));
#endif	CLUSTER_DEBUG
	if (cluster && pc_rd_cluster( ddest, pdrive, cluster ) )
		return (cluster);
	else
		return (0);
	}

/* Read a cluster from the disk. Return YES if successful */
#ifndef ANSIFND
BOOL pc_rd_cluster(ddest, pdrive, cluster)
	UTINY *ddest;
	DDRIVE *pdrive;
	UCOUNT cluster;
	{
#else
BOOL pc_rd_cluster(UTINY *ddest, DDRIVE *pdrive, UCOUNT cluster) /* _fn_ */
	{
#endif
	BLOCKT frstblock;
	COUNT  blkcount;

	blkcount = pdrive->secpalloc;
	if ( (frstblock = pc_cl2sector(pdrive , cluster)) )
		return (gblock (pdrive->driveno, frstblock, ddest, blkcount) );
	else
		return (NO);
	}
/*
*****************************************************************************
	PO_WRITE	-  Write to a file.

Summary
	
	#include <posix.h>

	LONG po_write(fd, buf, count)
		PCFD 	fd;
		UTINY 	*buf;
		LONG	count;

 Description
	Attempt to write count bytes from buf to the current file pointer of file
	at fd. The file pointer is updated.


 Returns
	Returns the	number of bytes written or -1 on error.
	If the return value is -1 p_errno will be set with one of the following:

	PENBADF		- File descriptor invalid or open read only
	PENOSPC		- Write failed. Presumably because of no space
				  file.
Example:
	#include <posix.h>

	PCFD fd;
	IMPORT INT p_errno;
	if (po_write(fd, buff, 512)  < 0)
		printf("Cant write to file error:%i\n",p_errno)
		

*****************************************************************************
*/

#ifndef ANSIFND
LONG po_write(fd, buf, count)
	PCFD 	fd;
	UTINY 	*buf;
	LONG	count;
	{
#else
LONG po_write(PCFD 	fd, UTINY 	*buf, LONG	count) /* _fn_ */
	{
#endif
	PC_FILE *pfile;
	UTINY *pfbuf;		/* Fast access to the file buffer */
	LONG i;
	LONG nx;
	UCOUNT curcl;

	dbg_api(("po_write: fd %d count %d\n", fd, count));
	p_errno = 0;

	/* Get the FILE. We don't want it if an error has occured */	
	if ( (pfile = pc_fd2file(fd, NO)) == NULL)
		{
		p_errno = PEBADF;
		return(-1);
		}

#ifdef	CLUSTER_DEBUG
	check_cluster(pfile, "po_write - TOP");
#endif	CLUSTER_DEBUG

	if (!((pfile->flag & PO_WRONLY) || (pfile->flag & PO_RDWR)))
		{
		p_errno = PEBADF;
		return(-1);
		}

	if (!count || !buf)
		return (0);	

	/* First handle the append always case */
	if (pfile->flag & PO_APPEND)
		if (po_lseek(fd, 0L, PSEEK_END) < 0L)
			return (-1);

	/* See if we need to claim a new cluster */
	if (!pfile->ccl)
		{
		/* Update the disk if the cluster is dirty */
		if (!pc_fileflush (pfile))
			{
			p_errno = PENOSPC;
			return (-1);
			}
		/*
		 * Now get a new cluster for the file. Don't
		 * assume that pfile->ccl is associated with
		 * last cluster in the file; we might have just 
		 * flushed a lower cluster number. In that case, 
		 * ccl is the cluster we just flushed, not the 
		 * end of the chain.
		 */
#ifdef	bogus_extend
		if (!(curcl = pc_clm_next(pfile->pobj , pfile->bccl)))
#else	bogus_extend		/* new way - dmitch 18-Oct-90 */
		curcl = pc_file_extend(pfile);
	    	if (curcl == 0) 
#endif	bogus_extend
			{
			p_errno = PENOSPC;
			return (-1);
			}
		else
			{
			pc_memfill(pfile->bbase,pfile->pobj->pdrive->bytespcluster,'\0');
			pfile->bdirty = YES;		
    	    		pfile->bccl = pfile->ccl = curcl;

			/* Up date the inode if just starting */
			if (!pfile->pobj->finode->fcluster)
				pfile->pobj->finode->fcluster = curcl;
			}
		}
		/* See if we have to flush the buffer and read a new cluster */
	else if (pfile->bccl != pfile->ccl)
		{
		/* Update the disk if the cluster is dirty */
		if (!pc_fileflush (pfile))
			{
			p_errno = PENOSPC;
			return (-1);
			}
		/* And read the cluster */
		else if (!(pc_rd_cluster(pfile->bbase,
				   pfile->pobj->pdrive,pfile->ccl)))
			{
			return (-1);
			}
		else
			{
			pfile->bccl = pfile->ccl;
			pfile->bdirty = 0;
			}
		}

	/* Now do the transfer */
	
	/* Divide the filepointer by the cluster size and take the remainder to
	   get the offset into the buffer */
	pfbuf = pfile->bbase + (pfile->fptr % pfile->pobj->pdrive->bytespcluster);

	nx = 0;		/* Number xferred */
	for (i = 0; i < count; i++)
		{
		if (pfbuf > pfile->bend)		/* Buffer full */
			{

			/* Write it */
            		if (!pc_wr_cluster(pfile->pobj, pfile->bbase , pfile->bccl))
				  {
				  pr_er_putstr("Failed writing to disk\n");
				  break;
		  		  }
			pfile->bdirty = NO;			

			/* Get a new cluster if extending the file */
			if (pfile->fptr >= pfile->pobj->finode->fsize)
				{
 		       		/* 
				 * claim a cluster for the next block.
				 */
#ifdef	bogus_extend	
	    	    		if ( !(pfile->bccl = pfile->ccl = 
	        		     pc_clm_next(pfile->pobj , pfile->ccl)) )
					{
					pfile->error = YES;
					pr_er_putstr("No room on disk\n");
					break;
	  				}
#else	bogus_extend		/* new way - dmitch 18-Oct-90 */
				pfile->bccl = pfile->ccl = 
					pc_file_extend(pfile);
	    	    		if (pfile->bccl == 0) {
					pfile->error = YES;
					pr_er_putstr("No room on disk\n");
					break;
	  			}
#endif	bogus_extend
				/* Zero it. */
				pc_memfill(pfile->bbase, 
					pfile->pobj->pdrive->bytespcluster, 
					'\0');
	  			}
		  	else 		/* Random access. Read the next cluster */
		  		{
				if (!(pfile->bccl = pfile->ccl = pc_rd_next( pfile->pobj,
					 pfile->bbase , pfile->ccl)) )
					break;
				}
 			pfbuf = pfile->bbase;
			}
		*pfbuf++ = *buf++;
	  	pfile->fptr++;
	  	nx++;
		/* DEBUG only */
		if(nx > count) {
			printf("po_write screwup\n");
			panic("po_write");
		}
		pfile->bdirty = YES;			
	   }

	/* Now clean up at the end.. If we read beyond the buffer we have
	   to set the ccl index to the next cluster so it jives with the 
	   file pointer */
	if (pfbuf > pfile->bend)		/* Buffer full */
		{
		if (pfile->fptr >= pfile->pobj->finode->fsize)
			pfile->ccl = 0;		/* Claim a new cluster next time in */
		else					/* Or get next in chain */
			pfile->ccl = pc_clnext(pfile->pobj->pdrive, pfile->bccl);
		}

	/* Update the size if the file pointer is beyond the file */
	if (pfile->fptr > pfile->pobj->finode->fsize)
		pfile->pobj->finode->fsize = pfile->fptr;
#ifdef	CLUSTER_DEBUG
	check_cluster(pfile, "po_write - OUT");
#endif	CLUSTER_DEBUG

	return(nx);
	}


/* Add a cluster to a chain */
#ifndef ANSIFND
UCOUNT pc_clm_next( pobj, cluster)
	FAST DROBJ *pobj;
	UCOUNT cluster;
	{
#else
UCOUNT pc_clm_next(FAST DROBJ *pobj, UCOUNT cluster) /* _fn_ */
	{
#endif
	DDRIVE *pdrive =   pobj->pdrive;
	return (pc_clgrow( pdrive, cluster ) );
	}

/* Write a cluster to the disk. Return YES if successful */
#ifndef ANSIFND
BOOL pc_wr_cluster(pobj, pdata, cluster)
	FAST DROBJ *pobj;
	UTINY  *pdata;
	UCOUNT cluster;
	{
#else
BOOL pc_wr_cluster(FAST DROBJ *pobj, UTINY  *pdata, UCOUNT cluster) /* _fn_ */
	{
#endif
	DDRIVE *pdrive =   pobj->pdrive;
	BLOCKT frstblock;
	COUNT  blkcount;
	
#ifdef  CLUSTER_DEBUG
      	dbg_api(("pc_wr_cluster: cluster %d\n", cluster));
#endif  CLUSTER_DEBUG
	blkcount = pdrive->secpalloc;
	if ( (frstblock = pc_cl2sector(pdrive , cluster)) )
		return (pblock (pdrive->driveno, frstblock, pdata, blkcount) );
	else
		return (NO);
	}

/* Update the disk if the dirty flag is set */
#ifndef ANSIFND
BOOL pc_fileflush (pfile)
	FAST PC_FILE *pfile;
	{
#else
BOOL pc_fileflush (FAST PC_FILE *pfile) /* _fn_ */
	{
#endif
	/* See if already up to date. */
	if (!pfile->bdirty)
		return (YES);
	else
		{
		pfile->bdirty = NO;
		return (pc_wr_cluster(pfile->pobj, pfile->bbase, pfile->bccl));
		}
	}

/*
 * Extend file. This does not assume that pfile->ccl points to the last 
 * cluster in the file. Returns number of new cluster, or 0 if
 * no space.
 */
static UCOUNT pc_file_extend(PC_FILE *pfile)
{
	UCOUNT end_cluster;
	UCOUNT new_cluster;
				
	/* 
	 * Go to the end of file.
	 */
	if(pfile->pobj->finode->fcluster)
		end_cluster = pc_lastinchain(pfile->pobj,
			    pfile->pobj->finode->fcluster);
	else
		end_cluster = 0;
	new_cluster = pc_clm_next(pfile->pobj, end_cluster);
	dbg_api(("pc_file_extend: old end %d, new end %d\n",
		end_cluster, new_cluster));
	return(new_cluster);     		     

}
