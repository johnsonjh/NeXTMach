/*
*****************************************************************************
	PO_LSEEK	-  Move file pointer

Summary

	#include <posix.h>

	LONG po_lseek(fd, offset, origin)
		PCFD 	fd;
		LONG 	offset;
		COUNT	origin;

 Description
	Move the file pointer offset bytes from the origin described by 
	origin. The file pointer is set according to the following rules.


	Origin				Rule
	PSEEK_SET			offset from begining of file
	PSEEK_CUR			offset from current file pointer
	PSEEK_END			offset from end of file

	Attempting to seek beyond end of file puts the file pointer one
	byte past eof. 

 Returns
	Returns the	new offset or -1 on error.

	If the return value is -1 p_errno will be set with one of the following:

	PENBADF		- File descriptor invalid
	PEINVAL		- Seek to negative file pointer attempted.

Example:
	#include <posix.h>

	PCFD fd;

	if ( (fd = pc_open("FROM.FIL",PO_RDWR,0)) >= 0)
	 if (po_lseek(fd, (recsize * recno) , PSEEK_SET) != (recsize * recno) )
	 	printf("Cant find record %i\n",recno);
	

*****************************************************************************
*/
/*
MRU:
	PVO 8-4-89 - When seeking to end of file check if past end of cluster
				 before requesting a new cluster 
*/

#include <posix.h>
#import <nextdos/dosdbg.h>

IMPORT INT p_errno;

#ifndef ANSIFND
LONG po_lseek(fd, offset, origin)
	PCFD 	fd;
	LONG 	offset;
	COUNT	origin;
	{
#else
LONG po_lseek(PCFD 	fd, LONG 	offset, COUNT	origin) /* _fn_ */
	{
#endif
	FAST PC_FILE *pfile;
	LONG destptr;
	LONG startptr;
	UCOUNT	startcl;
	UCOUNT  destcl;
	UCOUNT curcl;

	p_errno = 0;	
	dbg_api(("po_lseek: fd %d offset %d origin %d\n", fd, offset, origin));
	
	/* Get the FILE. We don't want it if an error has occured */	
	if ( (pfile = pc_fd2file(fd, NO)) == NULL)
		{
		p_errno = PEBADF;
		return(-1);
		}

	/* If file is zero sized. were there */
	if (!(pfile->pobj->finode->fsize))
		return (0L);

	if (origin == PSEEK_SET)			/*	offset from begining of file */
		{
		if (offset < 0L)
			{
			p_errno = PEINVAL;
			return (-1L);
			}
		else
			{
			curcl = pfile->pobj->finode->fcluster;
			startptr = 0L;
			}
		}
	/* Fall through here on purpose */
	else if (origin == PSEEK_CUR)	/* offset from current file pointer */
		{
		if ( ( (startptr = pfile->fptr) + offset) < 0)
			{
			p_errno = PEINVAL;
			return (-1L);
			}
		else
			curcl = pfile->ccl;
		}
	else if (origin == PSEEK_END)	/* 	offset from end of file */
		{
		if ( ( (startptr = pfile->pobj->finode->fsize) + offset) < 0)
			{
			p_errno = PEINVAL;
			return (-1L);
			}
		}
	else	/* Illegal origin */
		{
		p_errno = PEINVAL;
		return (-1L);
		}


	if (offset < 0L)		/* If seeking back in the file we have to
							   switch to seeking forward since fat is
							   singly linked */
		{
		curcl = pfile->pobj->finode->fcluster;
		startptr = 0L;
		offset = pfile->fptr + offset;
		}

	/* If seeking beyond EOF. we simply place the fptr at the end
	   check if the new file pointer is on a cluster boundary
	   		if it it on the boundary set curcl to 0 so write will
	   		grab a cluster.
	   		if it is not on a boundary, make sure curcl is set
	   	  	to the last in chain.
	*/

	if ( (destptr = (startptr + offset)) >= pfile->pobj->finode->fsize)
		{
		destptr = pfile->pobj->finode->fsize;
		/* Grab a new cluster if on a cluster boundary */
		if ( !(destptr % pfile->pobj->pdrive->bytespcluster ) )
			{
			curcl = 0;
			}
		else
			{
			/* Go to the end of file */
			if (pfile->pobj->finode->fcluster)
				curcl = pc_lastinchain(pfile->pobj,
									   pfile->pobj->finode->fcluster);
			else
				curcl = 0;

			}

		}
	else
		{
		/* Scan the chain until we are in the right cluster */
		/*	Note: startcl and destcl are just counters. They DO NOT map
			to anything on the disk */
#ifdef	NeXT
		startcl = (UCOUNT) (startptr / pfile->pobj->pdrive->bytespcluster);
		destcl =  (UCOUNT) (destptr / pfile->pobj->pdrive->bytespcluster);
#else	NeXT
		/* 
		 * note (type) has higher precedence then '/'!
		 */
		startcl = (UCOUNT) startptr / pfile->pobj->pdrive->bytespcluster;
		destcl =  (UCOUNT) destptr / pfile->pobj->pdrive->bytespcluster;
#endif	NeXT
		while ( startcl < destcl )
			{
			if (! (curcl = pc_clnext(pfile->pobj->pdrive, curcl) ) )
				{
				return (-1L);
				}
			startcl++;
			}
		}

		/* Set up the file - Note: we DO NOT change the buffer cluster here
	   we didn't touch it. This way read/write close will be able to flush
	   it */
	pfile->ccl = curcl;
	pfile->fptr = destptr;
	return (pfile->fptr);
	}

#ifndef ANSIFND
UCOUNT pc_lastinchain(pobj , cluster)
	FAST DROBJ *pobj;
	UCOUNT cluster;
	{
#else
UCOUNT pc_lastinchain(FAST DROBJ *pobj, UCOUNT cluster) /* _fn_ */
	{
#endif
	UCOUNT nextcluster;
	DDRIVE *pdrive = pobj->pdrive;

	if (!cluster)	
		cluster = pobj->finode->fcluster;
	nextcluster = pc_clnext(pdrive , cluster);

	while (nextcluster)
		{
		cluster = nextcluster;
		nextcluster = pc_clnext(pdrive , nextcluster);
		}
	return (cluster);
	}
