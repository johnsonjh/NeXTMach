/*
*****************************************************************************
	PC_DOS2INODE - Convert a dos disk entry to an in memory inode.

					   
Summary
	
	#include <pcdisk.h>

	VOID pc_dos2inode (pdir,pbuff)
		FINODE *pdir;
		DOSINODE *pbuff;


 Description
 	Take the data from pbuff which is a raw disk directory entry and copy
 	it to the inode at pdir. The data goes from INTEL byte ordering to 
 	native during the transfer.

 Returns
	Nothing

Example:
	#include <pcdisk.h>
	FINODE *pdir;
	UTINY *dbuff;
	DOSINODE *pbuff;

	Convert the 4th entry in a disk buffer to internal form.
	pc_dos2inode (pdir,(&((DOSINODE *) buff)[3]) )

*****************************************************************************
*/

#include <pcdisk.h>

/* Convert a dos inode to in mem form.*/
#ifndef ANSIFND
VOID pc_dos2inode (pdir,pbuff)
	FINODE *pdir;    /* put result here */
	DOSINODE *pbuff;	/* POINTER TO 32 UTINY ENTRY */
	{	
#else
VOID pc_dos2inode (FINODE *pdir, DOSINODE *pbuff) /* _fn_ */
	{
#endif
	copybuff(&pdir->fname[0],&pbuff->fname[0],8);
	copybuff(&pdir->fext[0],&pbuff->fext[0],3);
	pdir->fattribute = pbuff->fattribute;
	copybuff(&pdir->resarea[0],&pbuff->resarea[0],0x16-0xc);
	pdir->ftime = to_WORD((UTINY *) &pbuff->ftime);
	pdir->fdate = to_WORD((UTINY *) &pbuff->fdate);
	pdir->fcluster = to_WORD((UTINY *) &pbuff->fcluster);
	pdir->fsize = to_DWORD((UTINY *) &pbuff->fsize);
	}

