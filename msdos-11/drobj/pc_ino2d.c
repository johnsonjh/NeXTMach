/*
*****************************************************************************
	PC_INO2DOS - Convert  an in memory inode to a dos disk entry.

					   
Summary
	
	#include <pcdisk.h>

	VOID pc_ino2dos (pbuff,pdir)
		DOSINODE *pbuff;
		FINODE *pdir;


 Description
 	Take in memory native format inode information and copy it to a
 	buffer. Translate the inode to INTEL byte ordering during the transfer.

 Returns
	Nothing

Example:
	#include <pcdisk.h>
	FINODE *pdir;
	UTINY *dbuff;
	DOSINODE *pbuff;

	Copy and convert an in memory inode to the 4th entry in a disk buffer.
	pc_ino2dos (&((DOSINODE *) buff)[3]),pdir)

*****************************************************************************
*/
#include <pcdisk.h>

/* Un-Make a disk directory entry */
/* Convert an inmem inode to dos  form.*/
#ifndef ANSIFND
VOID pc_ino2dos (pbuff,pdir)
	DOSINODE *pbuff;	/* Dos result */
	FINODE *pdir;    /* Orig */
	{	
#else
VOID pc_ino2dos (DOSINODE *pbuff, FINODE *pdir) /* _fn_ */
	{
#endif
	copybuff(&pbuff->fname[0],&pdir->fname[0],8);
	if (pbuff->fname[0] == (UTINY) 0xE5)
		pbuff->fname[0] = PCDELETE;
	copybuff(&pbuff->fext[0],&pdir->fext[0],3);
	pbuff->fattribute = pdir->fattribute;
	copybuff(&pbuff->resarea[0],&pdir->resarea[0],0x16-0xc);
	fr_WORD((UTINY *) &pbuff->ftime,pdir->ftime);
	fr_WORD((UTINY *) &pbuff->fdate,pdir->fdate);
	fr_WORD((UTINY *) &pbuff->fcluster,pdir->fcluster);
	fr_DWORD((UTINY *) &pbuff->fsize,pdir->fsize);
	}

