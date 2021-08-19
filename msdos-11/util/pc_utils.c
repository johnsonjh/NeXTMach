/*
 * merged version of EBS util/*.c
 *
*****************************************************************************
	PC_ALLSPACE - Test if size characters in a string are spaces

Summary
	
	#include <pcdisk.h>
	BOOL pc_allspace(string, size)
		TEXT *string;
		INTsize;
		{

Description
	Test if the first size spaces of string are ' ' characters.
Returns
	YES if all spaces.

Example:

	#include <pcdisk.h>
	
	if ( (fname[0] == '.') &&  pc_allspace(&fname[1], 7) )
		printf("File name is DOT\n");

*****************************************************************************
*/

#include <pcdisk.h>

/* Return YES if n bytes of p are spaces  */
#ifndef ANSIFND
BOOL pc_allspace(p, i)
	FAST UTINY *p;
	FAST INT i;
#else
BOOL pc_allspace(FAST UTINY *p, FAST INT i) /* _fn_ */
#endif
	{
	while (i--)
		if (*p++ != ' ')
			return (NO);
	return (YES);
	}


/*
*****************************************************************************
	COPYBUF  - Copy one buffer to another

Summary
	
	#include <pcdisk.h>

	VOID copybuff(vto, vfrom, size)
		VOID *vto;
		VOID *vfrom;
		INT size;

 Description
 	Essentially strncpy. Copy size BYTES from from to to.
 Returns
 	Nothing

Example:
	
	#include <pcdisk.h>

	copybuf(buffer, initdata, initdatalength);

*****************************************************************************
*/
		
/* Copy data */
#ifndef ANSIFND
VOID copybuff(vto, vfrom, size)
	VOID *vto;
	VOID *vfrom;
	INT size;
#else
VOID copybuff(VOID *vto, VOID *vfrom, INT size) /* _fn_ */
#endif
	{
	FAST UTINY *to = (UTINY *) vto;
	FAST UTINY *from = (UTINY *) vfrom;

	while (size--)
		*to++ = *from++;
	}


/*
*****************************************************************************
	PC_CPPAD  - Copy one buffer to another and right fill with spaces

Summary
	
	#include <pcdisk.h>

	VOID pc_pad(to, from, size)
		UTINY *to;
		UTINY *from;
		INT size;

Description
 	Copy up to size characters from "from" to "to". If less than size
 	characters are transferred before reaching \0 fill "to" with ' '
 	characters until its length reaches size. 

 	Note: "to" is NOT ! Null terminated.

Returns
 	Nothing

Example:
	
	#include <pcdisk.h>
	TEXT fname[9];
	pc_cppad(fname, "cc", 8);
	fname[8] = '\0';

	creates
	{'c','c',' ',' ',' ',' ',' ',' ','\0'}


*****************************************************************************
*/

/* Copy data  pad past end of string with ' ' */
#ifndef ANSIFND
VOID pc_cppad(to, from, size)
	FAST UTINY *to;
	FAST UTINY *from;
	FAST INT size;
#else
VOID pc_cppad(FAST UTINY *to, FAST UTINY *from, FAST INT size) /* _fn_ */
#endif
	{
	while (size--)
		{
		if (*from)
			*to++ = *from++;
		else
			*to++ = ' ';
		}
	}

/*
*****************************************************************************
	PC_ISDOT  - Test if a filename is exactly '.'

Summary
	
	#include <pcdisk.h>
	BOOL pc_isdot(fname, fext)
		UTINY *fname;
		UTINY *fext;
		{

Description
	Test to see if fname is exactly '.' followed by seven spaces and fext is
	exactly three spaces.
Returns
	YES if file:ext == '.'

Example:

	#include <pcdisk.h>
	(Change directories)
	. is a no-op.
	if (pc_isdot( filename, fileext ))
		;
	 if ".." we need to shift up a level
	if (pc_isdotdot( filename, fileext ))
		pobj = pc_get_mom(pchild)


*****************************************************************************
*/

/* Return YES if File is exactly '.'  */
#ifndef ANSIFND
BOOL pc_isdot(fname, fext)
	FAST UTINY *fname;
	FAST UTINY *fext;
#else
BOOL pc_isdot(FAST UTINY *fname, FAST UTINY *fext) /* _fn_ */
#endif
	{
	return ((*fname == '.') &&
			  pc_allspace(fname+1,7) && pc_allspace(fext,3) );
	}

/*
*****************************************************************************
	PC_ISDOTDOT  - Test if a filename is exactly {'.','.'};

Summary
	
	#include <pcdisk.h>
	BOOL pc_isdotdot(fname, fext)
		UTINY *fname;
		UTINY *fext;
		{

Description
	Test to see if fname is exactly '..' followed by six spaces and fext is
	exactly three spaces.
Returns
	YES if file:ext == {'.','.'}

Example:

	#include <pcdisk.h>
	(Change directories)
	. is a no-op.
	if (pc_isdot( filename, fileext ))
		;
	 if ".." we need to shift up a level
	if (pc_isdotdot( filename, fileext ))
		pobj = pc_get_mom(pchild)


*****************************************************************************
*/

/* Return YES if File is exactly '..'  */
#ifndef ANSIFND
BOOL pc_isdotdot(fname, fext)
	FAST UTINY *fname;
	FAST UTINY *fext;
#else
BOOL pc_isdotdot(FAST UTINY *fname, FAST UTINY *fext) /* _fn_ */
#endif
	{
	return ( (*fname == '.') && (*(fname+1) == '.') && 
			  pc_allspace(fname+2,6) && pc_allspace(fext,3) );
	}


/*
*****************************************************************************
	PC_MEMFILL  - Fill a buffer with a character

Summary
	
	#include <pcdisk.h>

	VOID pc_memfill(vto, size , c)
		VOID *vto;
		INT size;
		UTINY c;
	

 Description
 	Fill to with size instances of c


 Returns
 	Nothing

Example:
	
	#include <pcdisk.h>
	
	Null out a block buffer
	pc_fillmem(pbuf, 512, (UTINY) 0 );

*****************************************************************************
*/
		
/* Fill a string */
#ifndef ANSIFND
VOID pc_memfill(vto, size , c)
	VOID *vto;
	INT size;
	UTINY c;
#else
VOID pc_memfill(VOID *vto, INT size, UTINY c) /* _fn_ */
#endif
	{
	FAST UTINY *to = (UTINY *) vto;;
	
	while (size--)
		*to++ = c;
	}

/*
*****************************************************************************
	PC_MFILE  - Build a file spec (xxx.yyy) from a file name and extension


Summary
	
	#include <pcdisk.h>

	TEXT *pc_mfile(to, file, ext)
		TEXT *to;
		TEXT *file;
		TEXT *ext;
	

 Description
 	Fill in to with a concatenation of file and ext. File and ext are
	not assumed to be null terminated but must be blank filled to [8,3]
	chars respectively. 'to' will be a null terminated string file.ext.

 Returns
 	A pointer to to.

Example:
	#include <pcdisk.h>
	TEXT newfile[13];

	pc_mfile(newfile, "CC      ", "EXE")	


*****************************************************************************
*/

/* Take blank filled file and ext and make file.ext */
#ifndef ANSIFND
TEXT *pc_mfile(to, filename, ext)
	FAST TEXT *to;
	TEXT *filename;
	TEXT *ext;
#else
TEXT *pc_mfile(FAST TEXT *to, TEXT *filename, TEXT *ext) /* _fn_ */
#endif
	{
	FAST TEXT *p;
	COUNT i;
	TEXT *retval = to;

	p = filename;
	i = 0;
	while(*p)
		{
		if (*p == ' ')
			break;
		else
			{
			*to++ = *p++;
			i++;
			}
		if (i == 8)
			break;
		}
	if (p != filename)
		{
		*to++ = '.';
		p = ext;
		i = 0;
		while(*p)
			{
			if (*p == ' ')
				break;
			else
				{
				*to++ = *p++;
				i++;
				}
			if (i == 3)
				break;
			}
		}
	/* Get rid of trailing '.' s */
	if ( (to > retval) && *(to-1) == '.')
		to--;
	*to = '\0';
	return (retval);
	}

/*
*****************************************************************************
	PC_MPATH  - Build a path sppec from a filename and pathname

Summary
	
	#include <pcdisk.h>

	TEXT *pc_mpath(to, path, filename)
		TEXT *to;
		TEXT *path;
		TEXT *filename;

	

 Description
 	Fill in "to" with a concatenation of path and filename. If path 
	does not end with a path separator, one will be placed between
 	path and filename.

 	"TO" will be null terminated.

 Returns
 	A pointer to to.

Example:
	#include <pcdisk.h>
	TEXT newpath[EMAXPATH];

	pc_mpath(newpath, "\USR\TEXT\", "FILE.NAM")	

*****************************************************************************
*/

#ifndef ANSIFND
TEXT *pc_mpath(to, path, filename)
	FAST TEXT *to;
	TEXT *path;
	TEXT *filename;
#else
TEXT *pc_mpath(FAST TEXT *to, TEXT *path, TEXT *filename) /* _fn_ */
#endif
	{
	TEXT *retval = to;
	FAST TEXT *p;
	FAST TEXT c = '\0';

	p = path;
	while(*p)
		if (*p == ' ')
			break;
		else
			*to++ = (c =  *p++);
			
	if (c != BACKSLASH)
		*to++ = BACKSLASH;

	p = filename;
	while(*p)
		*to++ = *p++;

	*to = '\0';
	
	return (retval);
	}

/*
*****************************************************************************
	PC_PARSEDRIVE -  Get a drive number from a path specifier

					   
Summary
	
	#include <pcdisk.h>

	TEXT *pc_parsedrive( driveno, path )
		COUNT *driveno;
		TEXT  *path;	

Description
	Take a path specifier in path and extract the drive number from it.
	If the second character in path is ':' then the first char is assumed
	to be a drive specifier and 'A' is subtracted from it to give the
	drive number. If the drive number is valid, driveno is updated and 
	a pointer to the text just beyond ':' is returned. Otherwise null
	is returned.
	If the second character in path is not ':' then the default drive number
	is put in driveno and path is returned.


Returns
 	Returns NULL on a bad drive number otherwise a pointer to the first
 	character in the rest of the path specifier.

Example:
	#include <pcdisk.h>

	TEXT *path;
	COUNT driveno;
	TEXT topath[EMAXPATH], filename[9], fileext[4];

	Get the drive no and return a pointer to the path part
	path = pc_parsedrive( &driveno,"D:\USR\BIN\GREP.EXE" );

	Parse the path into its components.
	pc_parsepath(topath, filename, fileext, path);

	Results in,

	driveno = 3
	topath =  "\USR\BIN";
	filename = "GREP";
	fileext  = "EXE";

*****************************************************************************
*/

/* Extract drive no from D: or use defualt. return the rest of the string
   or NULL if a bad drive no is requested */
#ifndef ANSIFND
TEXT *pc_parsedrive( driveno, path )
	COUNT *driveno;
	TEXT  *path;	
#else
TEXT *pc_parsedrive(COUNT *driveno, TEXT  *path) /* _fn_ */
#endif
	{
	TEXT *p = path;
	COUNT dno;

	/* get drive no */
	if ( *p && (*(p+1) == ':'))
		{
		dno = *p - 'A';
		p += 2;
		}
	else
		dno = pc_getdfltdrvno();

	if ( (dno < 0) || (dno > HIGHESTDRIVE) )
		return (NULL);
	else
		{
		*driveno = dno;
		return (p);
		}
	}

/*
*****************************************************************************
	PC_FILEPARSE -  Parse a file xxx.yyy into filename/pathname

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_fileparse(filename, fileext, name)
		TEXT *filename;
		TEXT *fileext;
		TEXT *name;

 Description
	Take a file named "XXX.YY" and return SPACE padded NULL terminated 
	filename "XXX     " and fileext "YY " components. If the name or ext are
	less than [8,3] characters the name/ext is space filled and null termed.
	If the name/ext is greater	than [8,3] the name/ext is truncated. '.'
	is used to seperate file from ext, the special cases of "." and ".." are
	also handled.



 Returns
 	Returns YES


Example:
	#include <pcdisk.h>

	TEXT *path;
	COUNT driveno;
	TEXT filename[9], fileext[4];

	pc_fileparse(filename, fileext, "GREP.C")

	Results in,

	filename = "GREP    ";
	fileext  = "C  ";

*****************************************************************************
*/

/* Take a string "xxx[.yy]" and put it into filename and fileext */
/* Note: add a check legal later */
#ifndef ANSIFND
BOOL pc_fileparse(filename, fileext, p)
	TEXT *filename;
	TEXT *fileext;
	TEXT *p;
#else
BOOL pc_fileparse(TEXT *filename, TEXT *fileext, TEXT *p) /* _fn_ */
#endif
	{
	COUNT i = 0;

	/* Defaults */
	pc_memfill(filename, 8, ' ');
	filename[8] = '\0';
	pc_memfill(fileext, 3, ' ');
	fileext[3] = '\0';

	/* Special cases of . and .. */
	if (*p == '.')
		{
		*filename = '.';
		if (*(p+1) == '.')
			{
			*(++filename) = '.';
			return (YES);
			}
		else if (*(p + 1) == '\0')
			return (YES);
		else
			return (NO);
		}
			
			
	i = 0;
	while (*p) 
		{
		if (*p == '.')
			{
			p++;
			break;
			}
		else
			if (i < 8)
				*filename++ = *p;
		p++;
		}

	i = 0;
	while (*p)
		{
		if (i < 3)
			*fileext++ = *p;
		p++;
		}
	return (YES);
	}


/*
*****************************************************************************
	PC_NIBBLEPARSE -  Nibble off the left most part of a pathspec

					   
Summary
	
	#include <pcdisk.h>

	TEXT *pc_nibbleparse(filename, fileext, path)
		TEXT *filename;
		TEXT *fileext;
		TEXT *path;

 Description
 	Take a pathspec (no leading D:). and parse the left most element into
 	filename and files ext. (SPACE right filled.).

 Returns
 	Returns a pointer to the rest of the path specifier beyond file.ext
Example:
	#include <pcdisk.h>

	TEXT *path;
	COUNT driveno;
	TEXT filename[9], fileext[4];

	path = "D:..\USR\TEXT.PRS\LETTER";

	path = pc_parsedrive( &driveno, path );

	while (path = pc_nibbleparse(filename, fileext,path))
		printf("|%-20.20s|%-8.8s|%-3.3s|\n",path, filename, fileext);
	printf("|DONE       |%-8.8s|%-3.3s|\n" filename, fileext);

	produces:
			remaining
			path				file   ext
		|\USR\TEXT.PRS\LETTER|..      |   |
		|\TEXT.PRS\LETTER    |USR     |   |
		|\LETTER             |TEXT    |PRS|
		|DONE                |LETTER  |   |


*****************************************************************************
*/

/* Parse a path. Return NULL if problems or a pointer to the "next" */
#ifndef ANSIFND
TEXT *pc_nibbleparse( filename, fileext, path )
	TEXT *filename;
	TEXT *fileext;
	TEXT *path;
#else
TEXT *pc_nibbleparse(TEXT *filename, TEXT *fileext, TEXT *path) /* _fn_ */
#endif
	{
	FAST TEXT *p;
	TEXT tbuf[EMAXPATH];
	TEXT *t = &tbuf[0];

	if ( !(p = path) )	/* Path must exist */
		return (NULL);
	while (*p) 
		{
		if (*p == BACKSLASH)
			{
			p++;
			break;
			}
		else
			*t++ = *p++;
		}
	*t = '\0';

	if (pc_fileparse(filename, fileext, &tbuf[0]))
		return (p);
	else
		return (NULL);
	}

/*
*****************************************************************************
	PC_PARSEPATH -  Parse a path specifier into path,file,ext

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_parsepath(topath, filename, fileext, path)
		TEXT *topath;
		TEXT *filename;
		TEXT *fileext;
		TEXT *path;

 Description
	Take a path specifier in path and break it into three null terminated
	strings topath,filename and file ext.
	The result pointers must contain enough storage to hold the results.
	Filename and fileext are BLANK filled to [8,3] spaces.


 Returns
 	Returns YES.

Example:
	#include <pcdisk.h>

	TEXT *path;
	COUNT driveno;
	TEXT topath[EMAXPATH], filename[9], fileext[4];

	Get the drive no and return a pointer to the path part
	path = pc_parsedrive( &driveno,"D:\USR\BIN\GREP.EXE" );

	Parse the path into its components.
	pc_parsepath(topath, filename, fileext, path);

	Results in,

	driveno = 3
	topath =  "\USR\BIN";
	filename = "GREP    ";
	fileext  = "EXE";

*****************************************************************************
*/

/* Take a path spec and chop it into PATH FILE and EXT. Note The storage
   should already alloced. Drive specifiers are thrown away. */
#ifndef ANSIFND
BOOL pc_parsepath(topath, filename, fileext, path)
	TEXT *topath;
	TEXT *filename;
	TEXT *fileext;
	TEXT *path;
#else
BOOL pc_parsepath(TEXT *topath, TEXT *filename, TEXT *fileext, TEXT *path) /* _fn_ */
#endif
	{
	FAST TEXT *p,*t,*l;
	COUNT i = 0;
	l = topath;
	t = NULL;
	
/*   If we have a D:, we want to be sure that it is retained as the path is 
  parsed. By seeting t just beyond it we guarantee it will be kept */
	if (*path && (*(path + 1) == ':'))
		t = path + 2;

	p = path;
	while( *p )
		{
		if ( *p == BACKSLASH )		/* Find last back slah */
			t = p;
		i++;
		p++;
		}
	if (i >= EMAXPATH)
		return (0);

	p = path;
	if ( t )						/* copy the path over up to \ */
		{
		while( p < t )
			*topath++ = *p++;
		}
	*topath = '\0';

	/* Since : and \ are handled differently. Adjust here */
	if (*p == BACKSLASH)
		p++;

	/* And make whats left into file */
	if (pc_fileparse(filename, fileext, p))
		return (YES);
	else
		return (NO);
	}
/*
*****************************************************************************
	PC_PATCMP  - Compare a pattern with a string

					   
Summary
	
	#include <pcdisk.h>
	BOOL pc_patcmp(p, pattern, size)
		TEXT *p;
		TEXT *pattern;
		INT size;


 Description
 	Compare size bytes of p against pattern. Applying the following rules.
 		If size == 8. 
 			(To handle the way dos handles deleted files)
			if p[0] = DELETED, never match
			if pattern[0] == DELETED, match with 0x5 

		'?' in pattern always matches the current char in p.
		'*' in pattern always matches the rest of p.
 Returns
 	Returns YES if they match

Example:
	
	#include <pcdisk.h>

	pc_patcmp("JOE", "J?E", 3)
	pc_patcmp("JOE", "J*", 3)
	pc_patcmp("JOE", "JOE", 3)
	pc_patcmp("JOE", "*", 3)

	All return YES.


*****************************************************************************
*/

/* Compare return yes if they match. ? in pattern always matches */
#ifndef ANSIFND
BOOL pc_patcmp(p, pattern, size)
	FAST TEXT *p;
	FAST TEXT *pattern;
	FAST INT size;
#else
BOOL pc_patcmp(FAST TEXT *p, FAST TEXT *pattern, FAST INT size) /* _fn_ */
#endif
	{
	/* Kludge. never match a deleted file */
	if (size == 8)
		{
		if (*p == PCDELETE)
			return (NO);
		else if (*pattern == PCDELETE)	/* But E5 in the Pattern matches 0x5 */
			{
			if ( (*p == 0x5) || (*p == '?') )
				{
				size -= 1;
				*p++;
				*pattern++;
				}
			else
				return (NO);
			}
		}

	while (size--)
		{
		if (*pattern == '*')	/* '*' matches the rest of the name */
			return (YES);
		if (*pattern != *p)
			if (*pattern != '?')
				return (NO);
		pattern++;
		p++;
		}
	return (YES);
	}
/*
*****************************************************************************
	pc_strcat - strcat

Summary
	
	#include <pcdisk.h>

	VOID pc_strcat(to, from)
		TEXT *to;
		TEXT *from;

 Description

 Returns
 	Nothing

Example:
	
	#include <pcdisk.h>

	pc_strcat (string, " MORE");

*****************************************************************************
*/
		
/* Copy data */
#ifndef ANSIFND
VOID pc_strcat(to, from)
	TEXT *to;
	TEXT *from;
#else
VOID pc_strcat(TEXT *to, TEXT *from) /* _fn_ */
#endif
	{
	while (*to)
		to++;
	while (*from)
		*to++ = *from++;
	*to = '\0';
	}


/*
*****************************************************************************
	PC_STR2UPPER  - Copy a string and make sure the dest is in Upper case


Summary
	
	#include <pcdisk.h>

	VOID pc_str2upper(to, from)
		UTINY *to;
		UTINY *from;

 Description
 	Copy a null termed string. Change all lower case chars to upper case

 Returns
 	Nothing

Example:
	#include <pcdisk.h>

	TEXT fname[9];

	pc_str2upper(fname,"uppernow")

	gives "UPPERNOW"


*****************************************************************************
*/
			
/* 2 upper */
#ifndef ANSIFND
VOID pc_str2upper(to, from)
	FAST UTINY *to;
	FAST UTINY *from;
#else
VOID pc_str2upper(FAST UTINY *to, FAST UTINY *from) /* _fn_ */
#endif
	{
	FAST TEXT c;
	
	while(c = *from++)
		{
		if  ((c >= 'a') && (c <= 'z'))
			c = 'A' + c - 'a';
		*to++ = c;		
		}
	*to = '\0';
	}
