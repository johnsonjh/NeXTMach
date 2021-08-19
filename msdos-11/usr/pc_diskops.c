/*
 * merged version of EBS 
 *        usr/pc_{dskcl,dskin,dskop,free,gcwd,scwd, gtdfd,stdfd,idskc}.c 
 */
/*
*****************************************************************************
	PC_DSKINIT -  Open a disk for business. (internal)

					   
Summary
	#include <pcdisk.h>

	BOOL pc_dskinit(driveno)
		COUNT driveno;
		{

 Description
 	Given a drive number, open the disk by reading all of the block zero
	information and the file allocation table. (called by pc_dskopen())
 	

 Returns
 	Returns YES if the disk was successfully initialized.
.
Example:
	#include <pcdisk.h>

	pc_dskinit(0);

*****************************************************************************
*/
#include <pcdisk.h>
#import <nextdos/dosdbg.h>

#ifndef ANSIFND
BOOL pc_dskinit(driveno)
	COUNT driveno;
#else
BOOL pc_dskinit(COUNT driveno) /* _fn_ */
#endif
	{
	FAST DDRIVE	*pdr;
	IMPORT DDRIVE *drv_array[];
	struct pcblk0 bl0;
	FAST	UTINY *p;
	FAST 	COUNT i;
	IMPORT DROBJ *lcwd[];
	DROBJ 	*pwd;

	if ( (pwd = lcwd[driveno]) != NULL) {
		pc_freeobj(pwd);
		lcwd[driveno] = NULL;		/* dmitch */
	}
	/* Check drive number */
	if ((driveno < 0) || (driveno > HIGHESTDRIVE))
		{
		pr_er_putstr("Invalid driveno to pc_dskinit\n");
		return(NO);
		}
	else
		{
		/* Don't do anything on reopens */
		if ( (pdr = drv_array[driveno]) != NULL)
			{
			pdr->opencount += 1;
			return(YES);
			}
		else if (!(pdr = (DDRIVE *) pc_malloc( sizeof(DDRIVE) ) ) )
			{
			pr_er_putstr("Out of core:pc_dskinit\n");
			return (NO);
			}
		}

	/* Clear the current working directory */
	lcwd[driveno] = NULL;
		
	/* Read block 0 */
	if (!gblk0((UCOUNT) driveno, &bl0 ))
		{
		pr_er_putstr("Can't read block 0:pc_dskinit\n");
		pc_mfree(pdr);
		return(NO);
		}

	/* Verify that we have a good dos formatted disk */
	if ( (bl0.jump != (UTINY) 0xE9) && (bl0.jump !=(UTINY) 0xEB) )
		{
		pr_er_putstr("Not a DOS disk:pc_dskinit\n");
		pc_mfree(pdr);
		return(NO);
		}

	/* set up the drive structur from block 0 */
	pdr->secpalloc = bl0.secpalloc;	/* sectors / cluster */
	pdr->secpfat = bl0.secpfat;		/* sectors / fat */
	pdr->numfats = bl0.numfats;		/* Number of fat copies */
	pdr->numroot = bl0.numroot;		/* Maximum number of root entries */
	pdr->numsecs = (BLOCKT) bl0.numsecs;	/* Total sectors on the disk */
	pdr->mediadesc =	bl0.mediadesc;	/* Media descriptor byte */
	pdr->secreserved = bl0.secreserved;	/* sectors reserved */
	pdr->secptrk	= bl0.secptrk;		/* sectors per track */
	pdr->numhead	= bl0.numhead;		/* number of heads */
	pdr->numhide	=bl0.numhide;		/* # hidden sectors */

/* Check if running on a DOS (4.0) huge partition */
#ifdef SUPPORTDOS4
#if 0
	if ((pdr->numsecs + (ULONG) pdr->numhide) > (ULONG) 0xffff ) /* (4.0) */
#endif
	/* If traditional total # sectors is zero, use value in extended BPB */
	if (pdr->numsecs == 0L)
		pdr->numsecs = bl0.numsecs2;							 /* (4.0) */
#endif

	/* derive some things */
			/* Nibbles/fat entry if < 4087 clusters then 12 bit else 16 */
	pdr->fasize = ( (pdr->numsecs/pdr->secpalloc) < 4087) ? 3 : 4;
			/* beginning of fat is just past reserved sectors */
	pdr->fatblock = (BLOCKT) bl0.secreserved;
		/* The first block of the root is just past the fat copies */
	pdr->rootblock = pdr->fatblock + pdr->secpfat * pdr->numfats;
		/* The first block of the cluster area is just past the root */
		/* Round up if we have to */
	pdr->firstclblock = pdr->rootblock +
						(pdr->numroot + INOPBLOCK - 1)/INOPBLOCK;
	pdr->bytespcluster = 512 * pdr->secpalloc;
	
	
	/*	Calculate the largest index in the file allocation table.
		Total # block in the cluster area)/Blockpercluster =='s total
		Number of clusters. Entries 0 & 1 are reserved so the highest
		valid fat index is 1 + total # clusters.
	*/
	 pdr->maxfindex = (UCOUNT)
	 		(1 + (pdr->numsecs - pdr->firstclblock)/pdr->secpalloc);

	/* Finish filling in info. We do it here just in case the fat buffering 
	code needs to use it. We do not want to cause any hard to find bugs
	somewhere down the road by not filling in all fields before calling the
	fat code. */

	pdr->driveno = driveno;	
	drv_array[driveno] = pdr;
	pdr->opencount = 1;

#ifdef USEFATBUF
	/* If FAT buffering is enabled, alloc the needed space and read the first
	   page into core */
	if (!pc_pfinit( pdr ) )
		{
		drv_array[driveno] = NULL;
		pc_mfree(pdr);
		return(NO);
		}
#else
	/* No fat buferring.. Alloc space for the fat and the dirty list
	   and read in th whole thing */
	/* Now: make room for the fat */
	if (!(pdr->pf = (UTINY *) pc_malloc ( (INT) (pdr->secpfat * 512) ) ) )
		{
		drv_array[driveno] = NULL;
		pc_mfree(pdr);
		return(NO);
		}

	/* And a dirty table */
	if (!(pdr->pdirty = (UTINY *) pc_malloc (pdr->secpfat) ) )
		{
		drv_array[driveno] = NULL;
		pc_mfree(pdr->pf);
		pc_mfree(pdr);
		return(NO);
		}
	else
		{
		p = pdr->pdirty;
		for (i = 0; i < pdr->secpfat; i++)
			*p++ = (UTINY) 0;
		}

	/* Now read the fat */
    if (!gblock(driveno,pdr->fatblock,&(pdr->pf[0]),pdr->secpfat ) )
		{
		drv_array[driveno] = NULL;
		pc_mfree(pdr->pf);
		pc_mfree(pdr->pdirty);
		pc_mfree(pdr);
		return(NO);
		}
#endif

	return(YES);
	}

/*
 *****************************************************************************
	PC_DSKCLOSE -  Flush all buffers for a disk and free all core.

					   
Summary
	#include <pcdisk.h>

	BOOL pc_dskclose(path)
		TEXT *path;
		{

 Description
 	Given a path name containing a valid drive specifier. Flush the
 	file allocation table and purge any buffers or objects associated
 	with the drive.

 Returns
 	Returns YES if all went well.

Example:
	#include <pcdisk.h>

	pc_dskclose("A:");

*****************************************************************************
*/

/* Flush a disk's fat and free up all core associated with the drive */ 
#ifndef ANSIFND
BOOL pc_dskclose(path)
	TEXT *path;
#else
BOOL pc_dskclose(TEXT *path) /* _fn_ */
#endif
	{
	COUNT driveno;

	dbg_api(("pc_dskclose: path <%s>\n", path));
	if (!pc_parsedrive( &driveno, path ))
		return (NO);
	else
		return (pc_idskclose(driveno));
	}

/*
*****************************************************************************
	PC_DSKOPEN -  Open a disk for business.

					   
Summary
	#include <pcdisk.h>

	BOOL pc_dskopen(path)
		TEXT *path;
		{

 Description
 	Given a path spec containing a valid drive specifier open the disk by
 	reading all of the block zero information and converting it to native
 	byte order, and then reading the file allocation table

 	
	THIS ROUTINE MUST BE CALLED BEFORE ANY OTHERS.

 Returns
 	Returns YES if the disk was successfully initialized.
.
Example:
	#include <pcdisk.h>

	pc_dskopen("A:");

	- or - 

	pc_dskopen("A:\USR\FOO");

*****************************************************************************
*/

/* Make sure the drive in a path specifier is initialized */ 
#ifndef ANSIFND
BOOL pc_dskopen(path)
	TEXT *path;
#else
BOOL pc_dskopen(TEXT *path) /* _fn_ */
#endif
	{
	COUNT driveno;

	dbg_api(("pc_dskopen: path <%s>\n", path));
	if (!pc_parsedrive( &driveno, path ))
		return (NO);
	return (pc_dskinit(driveno));
	}		

/*
*****************************************************************************
	PC_FREE - Count the number of free bytes remaining on a disk

  
Summary
	#include <pcdisk.h>

	LONG pc_free(path)
		TEXT *path;

 Description
 	Given a path containing a valid drive specifier count the number 
 	of free bytes on the drive.

 Returns
	 The number of free bytes or zero if the drive is full,not open,
	 or out of range.

Example:
	#include <pcdisk.h>
	printf("%l bytes remaining \n",pc_free("A:") );

*****************************************************************************
*/

/* Return # free bytes on a drive */
#ifndef ANSIFND
LONG pc_free(path)
	TEXT *path;
#else
LONG pc_free(TEXT *path) /* _fn_ */
#endif
	{
	COUNT driveno;

	dbg_api(("pc_free: path <%s>\n", path));
	if (!pc_parsedrive( &driveno, path ))
		return(0L);
	else
		return (pc_ifree(driveno));
	}


/*
*****************************************************************************
	PC_CWD -  Get the current working directory for a drive,

					   
Summary
	
	#include <pcdisk.h>

	DROBJ *pc_get_cwd(pdrive)
		DDRIVE *pdrive;

 Description
 	Return the current directory inode for the drive represented by ddrive. 


Example:
	#include <pcdisk.h>

	 Get the top of the current path 
	if ( *path == BACKSLASH )
		{
		if (!(pobj = pc_get_root(pdrive)) )
			return (NULL);
		path++;
		}
	else
	Get the current directory if path does not start with '\'
		if ( !(pobj = pc_get_cwd(pdrive)) )
			return (NULL);


*****************************************************************************
*/

/* The cwd structure */
IMPORT DROBJ *lcwd[];

/* Still need a setpwd function */

/*  Get the current working directory and copy it into pobj */
#ifndef ANSIFND
DROBJ *pc_get_cwd(pdrive)
	DDRIVE *pdrive;
#else
DROBJ *pc_get_cwd(DDRIVE *pdrive) /* _fn_ */
#endif
	{
	DROBJ *pcwd;
	DROBJ *pobj;

	pobj = NULL;

	if ( (pcwd = lcwd[pdrive->driveno]) != NULL)
		{
		if (!(pobj = pc_allocobj()) )
			return (NULL);
		/* Free the inode that comes with allocobj */
		pc_freei(pobj->finode);
		copybuff(pobj, pcwd, sizeof(DROBJ));
		pobj->finode->opencount += 1;
		}
	else 	/* If no cwd is set .. return the root */
		pobj = pc_get_root(pdrive);

	return (pobj);
	}

/*
*****************************************************************************
	PC_GETDFLTDRVNO - Return the current default drive.

				   
Summary
	#include <pcdisk.h>

	COUNT	pc_getdfltdrvno()


 Description
 	Use this function to get the current default drive when a path specifier
 	does not contain a drive specifier.

	see also pc_setdfltdrvno()

 Returns
	 Return the current default drive.

Example:
	#include <pcdisk.h>

	if (path[1] != ':')
		driveno = pc_getdfltdrvno();
	else
		driveno = path[0] - 'A';

*****************************************************************************
*/

/* Return the currently stored default drive */	
#ifndef ANSIFND
COUNT	pc_getdfltdrvno()
#else
COUNT	pc_getdfltdrvno() /* _fn_ */
#endif
	{
	IMPORT COUNT dfltdrv;
	return (dfltdrv);
	}

/*
*****************************************************************************
	PC_IDSKCLOSE -  Flush all buffers for a disk and free all core. (internal)

					   
Summary
	#include <pcdisk.h>

	BOOL pc_idskclose(driveno)
		COUNT driveno;
		{

 Description
 	Given a valid drive number. Flush the file allocation table and purge any
	buffers or objects associated with the drive. (called by pc_dskclose)

 Returns
 	Returns YES if all went well.

Example:
	#include <pcdisk.h>

	pc_idskclose(0);

*****************************************************************************
*/

#ifndef ANSIFND
BOOL pc_idskclose(driveno)
	COUNT driveno;
#else
BOOL pc_idskclose(COUNT driveno) /* _fn_ */
#endif
	{
	FAST DDRIVE	*pdr;
	BOOL retval = NO;

	/* Check drive number */
	if ( !(pdr = pc_drno2dr(driveno) ) )
		{
		return(NO);
		}

	if (retval = pc_flushfat(driveno))
		{
		/* Release the drive if opencount == 0 */
		pc_dskfree(driveno,NO);
		}

	return (retval);
	}

/*
*****************************************************************************
	PC_SET_CWD -  Set the current working directory for a drive.

					   
Summary
	#include <pcdisk.h>


	BOOL pc_set_cwd(path)
		TEXT *path;
		{

 Description
 	Find path. If it is a subdirectory make it the current working 
 	directory for the drive.


 Returns
 	Returns yes if the current working directory was changed.

Example:
	if (!pc_set_cwd("D:\USR\DATA\FINANCE"))
		printf("Can't change working directory\n");

*****************************************************************************
*/

IMPORT DROBJ *lcwd[];

#ifndef ANSIFND
BOOL pc_set_cwd(path)
	TEXT *path;
#else
BOOL pc_set_cwd(TEXT *path) /* _fn_ */
#endif
	{
	DROBJ *pobj;
	COUNT driveno;

	dbg_api(("pc_set_cwd: path <%s>\n", path));
	if ( (pobj = pc_fndnode(path)) == NULL)
		return (NO);
	else
		{
		if (pc_isadir(pobj))
			{
			driveno = pobj->pdrive->driveno;
			if (lcwd[driveno] != NULL)
				pc_freeobj(lcwd[driveno]);
			lcwd[driveno] = pobj;
			return (YES);
			}
		else
			{
			pc_freeobj(pobj);
			return (NO);
			}
		}
	}

/*
*****************************************************************************
	PC_SETDFLTDRVNO - Set the current default drive.

				   
Summary
	#include <pcdisk.h>

	BOOL	pc_setdfltdrvno(driveno)
		COUNT driveno;


 Description
 	Use this function to set the current default drive that will be used 
	when a path specifier does not contain a drive specifier.
	Note: The default default is zero (drive A:)

	see also pc_getdfltdrvno()

 Returns
	 Return NO if the drive is out of range.

Example:
	#include <pcdisk.h>

	in a shell program respond to:
	SETDFLT D:

	if (command == SETDFLT)  
		if (cmd[2] == '\0')
			if (cmd[1] != ':')
				pc_setdfltdrvno( cmd[0] - 'A');

*****************************************************************************
*/

/* Set the currently stored default drive */	
#ifndef ANSIFND
BOOL	pc_setdfltdrvno(driveno)
	COUNT driveno;
#else
BOOL	pc_setdfltdrvno(COUNT driveno) /* _fn_ */
#endif
	{
	IMPORT COUNT dfltdrv;
	/* Check drive number */
	if ((driveno < 0) || (driveno > HIGHESTDRIVE))
		{
		return(NO);
		}
	else
		dfltdrv = driveno;

	return (YES);
	}

