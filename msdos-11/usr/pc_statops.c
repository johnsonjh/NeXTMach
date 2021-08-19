/*
 * merged version of EBS usr/[pc_gfirs,pc_gnrxt,pc_gdone].c
 *
*****************************************************************************
	PC_GFIRST - Get first entry in a directory to match a pattern.

Summary
	
	#include <pcdisk.h>

	BOOL pc_gfirst( statobj, pattern)
		DSTAT *statobj;
		TEXT  *pattern;

 Description
	Given a pattern which contains both a path specifier and a search pattern
	fill in the structure at statobj with information about the file and set
	up internal parts of statobj to supply appropriate information for calls
	to pc_gnext.

	Examples of patterns are:
		"D:\USR\RELEASE\NETWORK\*.C"
		"D:\USR\BIN\UU*.*"
		"D:MEMO_?.*"
		"D:*.*"

 Returns
 	Returns YES if a match was found otherwise NO. (see also the pcls.c
 	utility.)


Example:
	#include <pcdisk.h>

	DSTAT *statobj;
	A simple directory list subroutine.
	if (pc_gfirst(&statobj, path))
		{
		while (YES)
			{
			if (statobj.fattribute & AVOLUME)
				 dirstr = "<VOL>";
			else if (statobj.fattribute & ADIRENT)
				 dirstr = "<DIR>";
			else
				 dirstr = "     ";
			printf("%-8s.%-3s %7ld %5s \n",	
			statobj.fname, statobj.fext,statobj.fsize,dirstr);

	         if (!pc_gnext(&statobj))
	         	break;
		     }
		}
	Cal gdone to free up internal resources used by statobj
	pc_gdone(&statobj);

*****************************************************************************
*/

#include <pcdisk.h>
#import <nextdos/dosdbg.h>

#ifndef ANSIFND
BOOL pc_gfirst( statobj, name)
	DSTAT *statobj;
	TEXT 	*name;
#else
BOOL pc_gfirst(DSTAT *statobj, TEXT 	*name) /* _fn_ */
#endif
	{
	TEXT mompath[EMAXPATH];
	TEXT filename[9];
	TEXT fileext[4];

	dbg_stat(("pc_gfirst: statp 0x%x <%s>\n", statobj, name));
	statobj->pobj = NULL;
	statobj->pmom = NULL;

	/* Get out the filename and d:parent */
	if (!pc_parsepath(mompath,filename,fileext,name))
		return (NO);

	/* Save the pattern. we'll need it in pc_gnext */
	copybuff(statobj->pname, filename, 9);
	copybuff(statobj->pext, fileext, 4);
	/* Copy over the path. we will need it later */
	copybuff(statobj->path, mompath, EMAXPATH);

	/* Find the file and init the structure */
	if (statobj->pmom = pc_fndnode(mompath) )
		/* Found it. Check access permissions */
		{
		if(pc_isadir(statobj->pmom)) 
			{
			/* Now find pattern in the directory */
				
			if (statobj->pobj = pc_get_inode(NULL, statobj->pmom, filename, fileext))
				{
				/* And update the stat structure */
				pc_upstat(statobj);
				return (YES);
				}
			}
		}
	 /* If it gets here we had a probblem */
	 if (statobj->pmom)
		{
	 	pc_freeobj(statobj->pmom);
	 	statobj->pmom = NULL;
	 	}
	return(NO);
	}
/*
*****************************************************************************
	PC_GNEXT - Get next entry in a directory that matches a pattern.

Summary
	
	#include <pcdisk.h>

	BOOL pc_gnext(statobj)
		DSTAT *statobj;

 Description
	Given a pointer to a DSTAT structure that has been set up by a call to
	pc_gfirst(), search for the next match of the original pattern in the
	original path. Return yes if found and update statobj for subsequent
	calls to pc_gnext.

 Returns
 	Returns YES if a match was found otherwise NO.


Example:
	#include <pcdisk.h>

	DSTAT *statobj;
	A simple directory list subroutine.
	if (pc_gfirst(&statobj, path))
		{
		while (YES)
			{
			if (statobj.fattribute & AVOLUME)
				 dirstr = "<VOL>";
			else if (statobj.fattribute & ADIRENT)
				 dirstr = "<DIR>";
			else
				 dirstr = "     ";
			printf("%-8s.%-3s %7ld %5s \n",	
			statobj.fname, statobj.fext,statobj.fsize,dirstr);

	         if (!pc_gnext(&statobj))
	         	break;
		     }
		}
	Cal gdone to free up internal resources used by statobj
	pc_gdone(&statobj);

*****************************************************************************
*/

#ifndef ANSIFND
BOOL pc_gnext(statobj)
	DSTAT *statobj;
#else
BOOL pc_gnext(DSTAT *statobj) /* _fn_ */
#endif
	{
	FAST DROBJ *nextobj;

	dbg_stat(("pc_gnext: statp 0x%x <%s>\n", statobj, statobj->path));
	/* Now find the next instance of pattern in the directory */
	if (nextobj = pc_get_inode(statobj->pobj, statobj->pmom, statobj->pname,
							   statobj->pext))
		{
		statobj->pobj = nextobj;
		/* And update the stat structure */
		pc_upstat(statobj);
		return(YES);
		}
	else
		{
		return(NO);
		}
	}

/*
*****************************************************************************
	PC_GDONE - Free internal resources used by pc_gnext and pc_gfirst.

Summary
	
	#include <pcdisk.h>

	VOID pc_done(statobj)
		DSTAT *statobj;

 Description
	Given a pointer to a DSTAT structure that has been set up by a call to
	pc_gfirst() free internal elements used by the statobj.

	NOTE: You MUST call this function when done searching through a 
	directory.

 Returns
 	Nothing

Example:
	#include <pcdisk.h>

	DSTAT *statobj;
	A simple directory list subroutine.
	if (pc_gfirst(&statobj, path))
		{
		while (YES)
			{
			if (statobj.fattribute & AVOLUME)
				 dirstr = "<VOL>";
			else if (statobj.fattribute & ADIRENT)
				 dirstr = "<DIR>";
			else
				 dirstr = "     ";
			printf("%-8s.%-3s %7ld %5s \n",	
			statobj.fname, statobj.fext,statobj.fsize,dirstr);

	         if (!pc_gnext(&statobj))
	         	break;
		     }
		}
	Cal gdone to free up internal resources used by statobj
	pc_gdone(&statobj);

*****************************************************************************
*/

#ifndef ANSIFND
VOID pc_gdone( statobj )
	DSTAT *statobj;
#else
VOID pc_gdone(DSTAT *statobj) /* _fn_ */
#endif
	{
	FAST DROBJ *pobj;
	FAST DROBJ *pmom;

	dbg_stat(("pc_gdone: statp 0x%x <%s>\n", statobj, statobj->path));
	if (pobj = statobj->pobj) {
		pc_freeobj(pobj);
		statobj->pobj = NULL;	/* dmitch */
	}
	if (pmom = statobj->pmom) {
		pc_freeobj(pmom);
		statobj->pmom = NULL;	/* dmitch */
	}
	}

