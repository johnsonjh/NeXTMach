/*
 * merged version of EBS usr/{po_open, po_close}.c
 *
*****************************************************************************
	PO_OPEN -  Open a file.

					   
Summary
	
	#include <posix.h>

	PCFD po_open(name, flag, mode)
		TEXT 	*name;
		UCOUNT 	flag;
		UCOUNT	mode;

 Description
 	Open the file for access as specified in flag. If creating use mode to
 	set the access permissions on the file.

	Flag values are

	PO_APPEND	- Seek to end of file before all writes.
	PO_BINARY	- Ignored. All file access is binary
	PO_TEXT		- Ignored
	PO_RDONLY	- Open for read only
	PO_RDWR		- Read/write access allowed.
	PO_WRONLY	- Open for write only

	PO_CREAT	- Create the file if it does not exist. Use mode to
				specify the permission on the file.
	PO_EXCL		- If flag contains (PO_CREAT | PO_EXCL) and the file 
				already exists fail and set p_errno to EEXIST
	PO_TRUNC	- Truncate the file if it already exists

	Mode values are

	PS_IWRITE	- Write permitted 	
	PS_IREAD	- Read permitted. (Always true anyway)

 Returns
 	Returns a non-negative integer to be used as a file descriptor for
 	calling read/write/seek/close otherwise it returns -1 and p_errno is 
	set to one of these values

	PENOENT		- File not found or path to file not found
	PEMFILE		- No file descriptors available (too many files open)
	PEEXIST		- Exclusive access requested but file already exists.
	PEACCESS	- Attempt to open a read only file or a special
			 (directory) file.
Example:
	#include <posix.h>

	PCFD fd;
	IMPORT INT p_errno;

	if (pcfd = po_open("\USR\MYFILE",(PO_CREAT|PO_EXCL|PO_WRONLY),PS_IWRITE)<0))
		printf("Cant create file error:%i\n",p_errno)

*****************************************************************************
*/
#include <posix.h>
#import <nextdos/dosdbg.h>

IMPORT INT p_errno;


#ifndef ANSIFND
PCFD po_open(name, flag, mode)
	TEXT 	*name;
	UCOUNT 	flag;
	UCOUNT	mode;
#else
PCFD po_open(TEXT 	*name, UCOUNT 	flag, UCOUNT	mode) /* _fn_ */
#endif
	{
	PCFD fd;
	PC_FILE *pfile;
	UCOUNT cluster;
	FAST DROBJ *pobj;

	p_errno = 0;

	dbg_api(("po_open: name <%s>\n", name));
	if ( (fd = pc_allocfile()) < 0 ) 	/* Grab a file */
		{
		p_errno = PEMFILE;
		return (fd);
		}
	
	/* Get the FILE. This will never fail */
	if ( (pfile = pc_fd2file(fd, NO)) == NULL)
		{
		p_errno = PEMFILE;
		return (fd);
		}


	if ( (pobj = pc_fndnode( name) ))
		{
		pfile->pobj = pobj;			/* Link the file to the object */
		/* If we goto exit: we want them linked so we can clean up */

		if(pc_isroot(pobj)
			|| (pobj->finode->fattribute & (AVOLUME | ADIRENT)) )
			{
			p_errno = PEACCES;		/* t is a directory */
			goto errex;
			}
		if ( (flag & (PO_EXCL|PO_CREAT)) == (PO_EXCL|PO_CREAT) )
			{
			p_errno = PEEXIST;		/* Exclusive fail */
			goto errex;
			}
		if( (flag & (PO_WRONLY|PO_RDWR)) &&
			(pobj->finode->fattribute & ARDONLY) )
			{
			p_errno = PEACCES;		/* read only file */
			goto errex;
			}
		if (flag & PO_TRUNC )
			{
			cluster = pobj->finode->fcluster;
			pobj->finode->fcluster = 0;
			pobj->finode->fsize = 0L;
			if (pc_update_inode(pobj))
				{
				/* And clear up the space */
				pobj->finode->fcluster = cluster;
				pc_freechain(pobj);
				pobj->finode->fcluster = 0;
				pc_flushfat(pobj->pdrive->driveno);
				}
			else
				{
				p_errno = PEACCES;		/* read only file */
				goto errex;
				}
			}
		}
	else	/* File not found */
		{

		if (!(flag & PO_CREAT))
			{
			p_errno = PENOENT;		/* File does not exist */
			goto errex;
			}
			/* Do not allow create if write bits not set */
		if( !(flag & (PO_WRONLY|PO_RDWR)) )
				{
				p_errno = PEACCES;		/* read only file */
				goto errex;
				}
			/* Create for read only if Read perm not allowed */			
		if ( !(pobj = pc_mknode( name , (mode & PS_IREAD) ? (UTINY) 0 : ARDONLY) ) )
			{
			goto errex;
			}
		else
			pfile->pobj = pobj;			/* Link the file to the object */
		}


	pfile->flag = flag;			/* Access flags */
	pfile->fptr = 0L;			/* File pointer */
	pfile->ccl = pobj->finode->fcluster; /* Current cluster - note on a new
											file this will be zero */
	pfile->bccl = 0;				 /* Currently nothing buffered */
	/* Set up the local read/write buffer for the file. */
	if (! (pfile->bbase = (UTINY *) pc_malloc(pobj->pdrive->bytespcluster)) )
		{
		p_errno = PEMFILE;
		goto errex;
		}
	pfile->bdirty = NO;			
	pfile->error = NO;			
	/* Set up and end of buffer address. Makes finding EOB easier. */
	pfile->bend = pfile->bbase + (pobj->pdrive->bytespcluster - 1);
	p_errno = 0;
	return (fd);
errex:
	pc_freefile(fd);
	return (-1);
	}
/*
*****************************************************************************
	PO_CLOSE  -  Close a file.

					   
Summary
	
	#include <posix.h>

	INT po_close(fd)
		PCFD fd;

 Description
 	Close the file updating the disk and freeing all core associated with FD.

 Returns
 	Returns 0 if all went well otherwise it returns -1 and p_errno is set to
	one of these values

	PENBADF		- Invalid file descriptor

Example:
	#include <posix.h>

	PCFD fd;
	IMPORT INT p_errno;

	if (pc_close(fd) < 0)
		printf("Error closing file:%i\n",p_errno)

*****************************************************************************
*/

#ifndef ANSIFND
INT po_close(fd)
	PCFD fd;
#else
INT po_close(PCFD fd) /* _fn_ */
#endif
	{
	FAST PC_FILE *pfile;
	INT retval = 0;

	p_errno = 0;	

	dbg_api(("po_close: fd %d\n", fd));
	/* Get the FILE. Take it even if an error has occured */	
	if ( (pfile = pc_fd2file(fd, YES)) == NULL)
		{
		p_errno = PEBADF;
		return(-1);
		}


	if (pfile->flag & (	PO_RDWR | PO_WRONLY ) )
		{
		/* Update the disk if the current cluster different from the buffer */
		if (!pfile->error) /* Just in case the file is trashed */
			if (pfile->bdirty)
				if (!pc_wr_cluster(pfile->pobj, pfile->bbase, pfile->bccl) )
					retval = -1;
		/* Convert to native and overwrite the existing inode*/
		if (!pc_update_inode(pfile->pobj))
			retval = -1;
		/* Flush the file allocation table */
		if (!pc_flushfat(pfile->pobj->pdrive->driveno))
			retval = -1;
		}
	/* Release the FD and its core */
	pc_freefile(fd);
	return (retval);
	}
