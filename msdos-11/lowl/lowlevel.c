/*
 * merged version of EBS lowl/*.c
 *
*****************************************************************************
	PC_CL2SECTOR - Convert a cluster number to block number representation.

   
Summary
	#include <pcdisk.h>

	BLOCKT pc_cl2sector(pdrive, cluster)
		DDRIVE *pdrive;
		UCOUNT cluster;

 Description
 	Convert cluster number to a blocknumber.

 Returns
 	Returns 0 if the cluster is out of range. else returns the
    block number of the beginning of the cluster.

Example:
	#include <pcdisk.h>

	Get the next block in a chain.

	cluster = pc_sec2cluster(pdrive,curblock);
	Consult the fat for the next cluster 
	if ( !(cluster = pc_clnext(pdrive, cluster)) )
		return (0);	End of chain
	else
		return (pc_cl2sector(pdrive, cluster));

				
*****************************************************************************
*/
#include <pcdisk.h>
#import <nextdos/dosdbg.h>

/* #define CLUSTER_DEBUG	1	/* */

/* Convert cluster. to sector */
#ifndef ANSIFND
BLOCKT pc_cl2sector(pdrive, cluster)
	DDRIVE *pdrive;
	UCOUNT cluster;
#else
BLOCKT pc_cl2sector(DDRIVE *pdrive, UCOUNT cluster) /* _fn_ */
#endif
	{
	BLOCKT blockno;

	if (cluster < 2)
		return (BLOCKEQ0);
	else 
		blockno = pdrive->firstclblock +
				 (BLOCKT) ((cluster - 2) * pdrive->secpalloc);

	if (blockno >= pdrive->numsecs)
		return (BLOCKEQ0);
	else 
		return (blockno);
	}
/*
*****************************************************************************
	PC_CLALLOC - Reserve and return the next free cluster on a drive

  
Summary
	#include <pcdisk.h>

	UCOUNT pc_clalloc(pdr)
		DDRIVE	*pdr;


 Description
 	Given a DDRIVE, mark the next available cluster in the file allocation 
 	table as used and return the associated cluster number.


 Returns
	 Return a new cluster number or 0 if the disk is full.

Example:
	#include <pcdisk.h>

	Get a new cluster

	if (!(cluster = pc_clalloc(pdr) ))
		printf("Disk full\n");

*****************************************************************************
*/

#ifndef ANSIFND
UCOUNT pc_clalloc(pdr)  /* Return the next free clno from fat or null */
	FAST DDRIVE	*pdr;
#else
UCOUNT pc_clalloc(FAST DDRIVE	*pdr) /* _fn_ */
#endif
	{
	FAST COUNT i;
	UCOUNT nxt;
	UCOUNT startpt;

	/* Play a game here. If we are fat buffering we do not want to thrash by 
	   going back to the beginning of the fat for storage. So we look forward
	   from the current index. (pc_pfgcind()).
	   If that fails we want to go back to the beginning (index == 2) and
	   search the whole fat. This should eliminate thrashing in most cases.
	   I guess that in some wierd situations it could lead to extra 
	  fragmentation. (If you don't need FAT buffering don't use it !) */

	startpt = 2;

#ifdef USEFATBUF
	startpt = pc_pfgcind(pdr);
#endif
	
	while (YES)
		{
		if ( (startpt < 2) || (startpt > pdr->maxfindex) )
			startpt = 2;
		for (i = startpt ; i <= pdr->maxfindex; i++)
			{
			/* Get the value from the FAT, if FAT buffering is enabled it
			   is possible for io errors to occur while swapping. Return
			   0 if an error occurred */
			if ( !pc_faxx(pdr , i, &nxt) ) 
				return((UCOUNT) 0);
			if (nxt == 0)
				{
				if (!pc_pfaxx(pdr, i,0xffff))/* Mark it as last in the file */
					return ( (UCOUNT) 0);	 /* Can possibly fail if FAT 
												buffering is truned on */
				return((UCOUNT) i);         /* and return */
				}
			}
		if (startpt == 2)	/* We looked from beginning and could not find */
			break;
		else
			startpt = 2;    /* Start back at the beginning */
		}
	return((UCOUNT) 0);
	}

/*
*****************************************************************************
	PC_CLGROW - Extend a cluster chain and return the next free cluster

  
Summary
	#include <pcdisk.h>

	UCOUNT pc_clgrow(pdr , clno)
		DDRIVE	*pdr;
		UCOUNT clno;

 Description
 	Given a DDRIVE and a cluster, extend the chain containing the cluster
	by allocating a new cluster and linking clno to it. If clno is zero
	assume it is the start of a new file and allocate a new cluster.

 Returns
	 Return a new cluster number or 0 if the disk is full.

Example:
	#include <pcdisk.h>

	Extend the file

	if (!(cluster = pc_clgrow(pdr,cluster) ))
		printf("Disk full\n");

*****************************************************************************
*/

#ifndef ANSIFND
UCOUNT  pc_clgrow(pdr , clno) /* Return the next free clno from fat or null */
	FAST DDRIVE	*pdr;
	UCOUNT  clno; 
#else
UCOUNT  pc_clgrow(FAST DDRIVE	*pdr, UCOUNT  clno) /* _fn_ */
#endif
	{
     UCOUNT nxt;
#ifdef	CLUSTER_DEBUG
	dbg_api(("pc_clgrow: clno %d\n", clno));
#endif	CLUSTER_DEBUG
	/* Get a cluster */
	if (!(nxt = pc_clalloc(pdr)))
		return((UCOUNT) 0);
	/* Attach it to the current cluster if not at the begining of the chain */
	if (clno)
		if (!pc_pfaxx(pdr, clno, nxt))
			return((UCOUNT) 0);

#ifdef	CLUSTER_DEBUG
	dbg_api(("pc_clgrow: RETURNING nxt %d\n", nxt));
#endif	CLUSTER_DEBUG
	return(nxt);
	}

/*
*****************************************************************************
	PC_CLNEXT - Return the next cluster in a cluster chain

				   
Summary
	#include <pcdisk.h>

	UCOUNT pc_clnext(pdr , clno)
		DDRIVE	*pdr;
		UCOUNT  clno;
		{

 Description
 	Given a DDRIVE and a cluster number, return the next cluster in the 
 	chain containing clno. Return 0 on end of chain.

 Returns
	 Return a new cluster number or 0 on end of chain.

Example:
	#include <pcdisk.h>

	Find the end of a file.

	eof = cluster = pobj->finode->fcluster;

	while(cluster)
		{
		if (cluster = pc_clnext(pdr, cluster) )
			eof = cluster;
		}

*****************************************************************************
*/

/* Return the next cluster in a chain or ZERO */
#ifndef ANSIFND
UCOUNT pc_clnext(pdr , clno)
	FAST DDRIVE	*pdr;
	UCOUNT  clno;
#else
UCOUNT pc_clnext(FAST DDRIVE	*pdr, UCOUNT  clno) /* _fn_ */
#endif
	{
	UCOUNT nxt;

	/* Get the value at clno. return 0 on any io errors */
	if (! pc_faxx(pdr,clno,&nxt) )
		return ( (UCOUNT) 0 );

	if (pdr->fasize == 3)		/* 3 nibble ? */
		{
	 	if ( (0xff7 < nxt) && (nxt <= 0xfff) )
     	 	nxt = 0;							/* end of chain */
		}
	else
		{
		if ( (0xfff7 < nxt) && (nxt <= 0xffff) )
			nxt = 0;							/* end of chain */
		}
	return(nxt);
	}

/*
*****************************************************************************
	PC_CLRELEASE - Return a cluster to the pool of free space on a disk

  
Summary
	#include <pcdisk.h>


	VOID pc_clrelease(pdr , clno)
		 DDRIVE	*pdr;
		 UCOUNT clno;

 Description
 	Given a DDRIVE and a cluster, mark the cluster in the file allocation
	table as free. It will be used again by calls to pc_clalloc().

 Returns
	 Nothing

Example:
	#include <pcdisk.h>

	Release a chain (delete contents of a file)

	cluster = pobj->finode->fcluster;
	nextcluster = pc_clnext(pdrive , cluster);

	while (cluster)
		{
		pc_clrelease(pdrive , cluster);
		cluster = nextcluster;
		nextcluster = pc_clnext(pdrive , nextcluster);
		}

*****************************************************************************
*/

/* Return a cluster to the free list */
#ifndef ANSIFND
VOID pc_clrelease(pdr , clno)
	FAST DDRIVE	*pdr;
	UCOUNT  clno;
#else
VOID pc_clrelease(FAST DDRIVE	*pdr, UCOUNT  clno) /* _fn_ */
#endif
	{
	/* Don't catch any lower level errors here. Youl catch them soon enough */
    pc_pfaxx(pdr, clno, 0x0000);		/* Mark it as free */
    }


/*
*****************************************************************************
	PC_CLZERO -  Fill a disk cluster with zeroes

	   
Summary
	
	#include <pcdisk.h>

	BOOL pc_clzero(pdrive , clusterno )
		DDRIVE *pdrive;
		UCOUNT clusterno;
		{

 Description
	Write zeroes into the cluster at clusterno on the drive pointed to by 
	pdrive. Used to zero out directory and data file clusters to eliminate
	any residual data.

 Returns
 	Returns NO on a write erro.

 Example:

	#include <pcdisk.h>
	DROBJ *pobj;

	Zero out the first cluster in the data file.
	if (pc_clzero(pobj->pdrive , pobj->finode->fcluster))
		printf("Write error\n);


*****************************************************************************
*/

/* Write zeros to all blocks in a cluster */
#ifndef ANSIFND
BOOL pc_clzero(pdrive , cluster )
	DDRIVE *pdrive;
	UCOUNT cluster;
#else
BOOL pc_clzero(DDRIVE *pdrive, UCOUNT cluster) /* _fn_ */
#endif
	{
	BLKBUFF *pbuff;
	FAST UCOUNT i;
	FAST BLOCKT currbl;

	if (!(currbl = pc_cl2sector(pdrive , cluster)) )
		return (NO);
	/*Init and write a block for each block in cl. Note: init clears the core*/
	for (i = 0; i < pdrive->secpalloc; i++,	currbl++ )
		{
		if ( !(pbuff = pc_init_blk( pdrive , currbl)) )
			{
			return (NO);
			}
		if ( !pc_write_blk ( pbuff ) )
			{
			pc_free_buf(pbuff,YES);
			return (NO);
			}
		else
			pc_free_buf(pbuff,NO);

		}

	return (YES);
	}
/*
*****************************************************************************
	PC_DRNO2DR -  Convert a drive number to a pointer to DDRIVE

				   
Summary
	#include <pcdisk.h>

	DDRIVE	*pc_drno2dr(driveno) 
		COUNT driveno;


 Description
 	Given a drive number look up the DDRIVE structure associated with it.

 Returns
 	Returns NULL if driveno is not an open drive.

Example:
	#include <pcdisk.h>

	DDRIVE *pd0;

	pd0 = pc_drno2dr(0) ;

*****************************************************************************
*/

#ifndef ANSIFND
DDRIVE	*pc_drno2dr(driveno) 
	COUNT driveno;
#else
DDRIVE	*pc_drno2dr(COUNT driveno) /* _fn_ */
#endif
	{
	IMPORT DDRIVE *drv_array[];

	/* Check drive number */
	if ((driveno < 0) || (driveno > HIGHESTDRIVE))
		{
		return(NULL);
		}
	else
		return (drv_array[driveno]);

	}

/*
*****************************************************************************
	PC_DSKFREE -  Deallocate all core associated with a disk structure

				   
Summary
	#include <pcdisk.h>

	BOOL pc_dskfree(driveno, unconditional)
		COUNT driveno;
		BOOL  unconditional;

 Description
 	Given a valid drive number. If the drive open count goes to zero, free the
	file allocation table and the block zero information associated with the
	drive. If unconditional is true, ignore the open count and release the 
	drive. 
	If open count reaches zero or unconditional, all future accesses to
	driveno will fail until re-opened.

 Returns
 	Returns NO if driveno is not an open drive.

Example:
	#include <pcdisk.h>

	pc_dskfree(0,YES);

*****************************************************************************
*/

/* free up all core associated with the drive 
   called by close. A drive restart would consist of 
   	pc_dskfree(driveno, YES), pc_dskopen() */ 
#ifndef ANSIFND
BOOL pc_dskfree(driveno, unconditional)
	COUNT driveno;
	BOOL  unconditional;
#else
BOOL pc_dskfree(COUNT driveno, BOOL  unconditional) /* _fn_ */
#endif
	{
	FAST DDRIVE	*pdr;
	IMPORT DROBJ *lcwd[];
	DROBJ *pcwd;

	IMPORT DDRIVE *drv_array[];

	if ( !(pdr = pc_drno2dr(driveno) ) )
		{
		return(NO);
		}

	/* Don't free if open count > 0 */
	if (!unconditional)
		if (--pdr->opencount)
			return(YES);

	/* Free the current working directory */
	if ( (pcwd = lcwd[driveno]) != NULL)
		pc_freeobj(pcwd);
	lcwd[driveno] = NULL;


	/* Free all files, finodes & blocks associated with the drive */
	pc_free_all_fil(pdr);
	pc_free_all_i(pdr);
	pc_free_all_blk(pdr);

#ifdef USEFATBUF
	/* If fat buffering is enabled, free the core. */
	pc_pfclose(pdr);
#else
	pc_mfree(pdr->pdirty);
	pc_mfree(pdr->pf);
#endif

	pc_mfree(pdr);
	drv_array[driveno] = NULL;

	return (YES);
	}

/*
*****************************************************************************
	PC_FATSW - FAT Access code when fat buffering is enabled.

Summary

Allocate a fat buffer and load the first page into core. (Called by pc_dskinit)

	BOOL pc_pfinit( DDRIVE *pdr);

Free all core associated with the drive's fat

	VOID pc_pfclose( DDRIVE *pdr);

Flush the current page (if needed. And read in a new page such that "index" is
contained therein. (Called by pc_pfpbyte() and pc_pfgbyte())

	BOOL pc_pfswap( DDRIVE *pdr, COUNT index);

Put a byte (value) into the FAT at index. Return YES if everything worked. 
Note: FAT page swaps will occur if they are needed. Called by pc_pfaxx().

	BOOL pc_pfpbyte( DDRIVE *pdr, COUNT index, UTINY value);

Get a byte from the FAT at index and put the value at *value. Return YES if
everything worked.
Note: FAT page swaps will occur if they are needed. Called by pc_faxx().

	BOOL pc_pfgbyte( DDRIVE *pdr, COUNT index, UTINY *value);

Flush any dirty blocks from the current FAT page. Called by pc_flushfat()

	BOOL pc_pfflush( DDRIVE *pdr);

Description
	Maintains FAT buffering. To enable FAT buffering un-comment USEFATBUF in
	pcdisk.h and recompile all modules. 
	Note: Use of FAT buffering is not recomended if you have enough core to
	hold your FATS in memory.

Returns

Example
*****************************************************************************
*/

#ifdef USEFATBUF
			

BOOL pc_pfinit( DDRIVE *pdr);
VOID pc_pfclose( DDRIVE *pdr);
BOOL pc_pfswap( DDRIVE *pdr, COUNT index);
BOOL pc_pfpbyte( DDRIVE *pdr, COUNT index, UTINY value);
BOOL pc_pfgbyte( DDRIVE *pdr, COUNT index, UTINY *value);
BOOL pc_pfflush( DDRIVE *pdr);

/* Allocate core needed for FAT swapping and read in the first page */			
#ifndef ANSIFND
BOOL pc_pfinit(pdr)
	FAST DDRIVE *pdr;
#else
BOOL pc_pfinit(FAST DDRIVE *pdr) /* _fn_ */
#endif
	{
	FAST FATSWAP *pfs;
	UTINY c;

	/* Alloc a buffer */
	if (!(pfs = pdr->pfs = (FATSWAP *) pc_malloc( sizeof(FATSWAP))) )
		return (NO);
	
	/* Decide the maximum fat swap page. - don't use more then we have to  */
	pfs->pf_pagesize = ( (MAXFPAGE > pdr->secpfat) ? pdr->secpfat : MAXFPAGE );

	/* Allocate a buffer and a "dirty list" */
	if (!(pfs->pf = (UTINY *) pc_malloc((pfs->pf_pagesize << 9)) ))
		{
		pc_mfree(pfs);
		return(NO);
		}
	if (!(pfs->pdirty = (UTINY *) pc_malloc(pfs->pf_pagesize)) )
		{
		pc_mfree(pfs->pf);
		pc_mfree(pfs);
		return(NO);
		}

	/* Clear the dirty list */
	pc_memfill(pdr->pfs->pdirty, pfs->pf_pagesize, (UTINY) 0);

	/* Swap in item 0. (ie read the first page of the FAT */
	if (!pc_pfswap(pdr, (COUNT) 0))
		{
		/* call "close" to free the core */
		pc_pfclose(pdr);
		return(NO);
		}
	else
		return (YES);
	}

/* Deallocate core for a drive's FAT buffering, no flushing to disk */			
#ifndef ANSIFND
VOID pc_pfclose(pdr)
	FAST DDRIVE *pdr;
#else
VOID pc_pfclose(FAST DDRIVE *pdr) /* _fn_ */
#endif
	{
	FAST FATSWAP *pfs;

	/* And free all core */
	pfs = pdr->pfs;
	pc_mfree(pfs->pdirty);
	pc_mfree(pfs->pf);
	pc_mfree(pfs);
	pdr->pfs = (FATSWAP *) NULL;
	}

	
/* Flush buffers and then swap in the page containing index */			
#ifndef ANSIFND
BOOL pc_pfswap(pdr,index)
	FAST DDRIVE *pdr;
	COUNT index;
#else
BOOL pc_pfswap(FAST DDRIVE *pdr, COUNT index) /* _fn_ */
#endif
	{
	FAST FATSWAP *pfs = pdr->pfs;
	BLOCKT newblock;

	/* Flush the buffer */
	pc_pfflush(pdr);

	/* Convert the index to block values */
	newblock = (index >> 9);

	if (newblock >= pdr->secpfat) /* Check range */
		return (NO);

	/* Use the maximum page size available */
	if ((newblock + pfs->pf_pagesize) >= pdr->secpfat)
		newblock = pdr->secpfat - pfs->pf_pagesize;

	/* Calculate the highest and lowest indeces in this page */
	pfs->pf_indlow = (COUNT) (newblock << 9);
	pfs->pf_indhigh =(COUNT) ( ((pfs->pf_pagesize + newblock) << 9) - 1);

	/* Calculate the blocknumber */
	pfs->pf_blocklow = newblock + pdr->fatblock;

	/* Now read the page into the buffer at pfs->pf */
    return(gblock(pdr->driveno,pfs->pf_blocklow,pfs->pf,pfs->pf_pagesize));
	}
		
/* Put a BYTE value into the fat at index  */			
#ifndef ANSIFND
BOOL pc_pfpbyte(pdr,index,value)
	FAST DDRIVE *pdr;
	COUNT index;
	UTINY value;
#else
BOOL pc_pfpbyte(FAST DDRIVE *pdr, COUNT index, UTINY value) /* _fn_ */
#endif
	{
	FAST FATSWAP *pfs = pdr->pfs;
	COUNT newindex;

	/* Swap pages if we have to - Note: pfswap does range checks */
	if ( (index < pfs->pf_indlow) || (index > pfs->pf_indhigh) )
		if (!pc_pfswap(pdr,index))
			return (NO);

	/* Convert index from Byte offset in FAT to byte offset in page */
	newindex = index - pfs->pf_indlow;

	/* Put the byte in the buffer - and mark its block dirty. */
	pfs->pf[newindex] = value;
	pfs->pdirty[(newindex >> 9)] = (UTINY) YES;	

	return (YES);
	}

/* Get a BYTE value from the fat at index  */			
#ifndef ANSIFND
BOOL pc_pfgbyte(pdr,index,value)
	FAST DDRIVE *pdr;
	COUNT index;
	UTINY *value;
#else
BOOL pc_pfgbyte(FAST DDRIVE *pdr, COUNT index, UTINY *value) /* _fn_ */
#endif
	{
	FAST FATSWAP *pfs = pdr->pfs;
	COUNT newindex;

	/* Swap pages if we have to - Note: pfswap does range checks */
	if ( (index < pfs->pf_indlow) || (index > pfs->pf_indhigh) )
		if (!pc_pfswap(pdr,index))
			return (NO);
	/* Convert index from Byte offset in FAT to byte offset in page */
	newindex = index - pfs->pf_indlow;

	/* Get the byte */
	*value = pfs->pf[newindex];
	return (YES);
	}

/* Consult the dirty fat block list and write any. write all copies
   of the fat */
#ifndef ANSIFND
BOOL pc_pfflush(pdr)
	FAST DDRIVE *pdr;
#else
BOOL pc_pfflush(FAST DDRIVE *pdr) /* _fn_ */
#endif
	{
	FAST FATSWAP *pfs = pdr->pfs;

	FAST UTINY *pf;
	FAST UTINY *pd;
	FAST COUNT i;
	COUNT j;
	BLOCKT baseblock;


	for (j = 0; j < pdr->numfats;j++)
		{
		pf = pfs->pf;
		baseblock = pfs->pf_blocklow + (BLOCKT) (j * pdr->secpfat);
		for (i = 0,pd = pfs->pdirty; i < pfs->pf_pagesize; i++,pd++,baseblock++)
			{
			if (*pd != (UTINY) 0)
				if(!pblock(pdr->driveno,baseblock,pf,1))
					{
				 	pr_er_putstr("Cant flush F.A.T\n");
				 	return(NO);
				 	}
			pf += 512;
			}
		}

	/* Clear the dirty list */
	pc_memfill(pfs->pdirty, pfs->pf_pagesize, (UTINY) 0);

	return (YES);
	}


/* Convert the current page base in bytes to FAT index representation.
   Used by pc_clalloc(). to find an inteligient place to start looking for
   free FAT entries and avoid thrashing by going back to the beginning of
   the FAT */

#ifndef ANSIFND
UCOUNT pc_pfgcind(pdr)
	FAST DDRIVE *pdr;
#else
UCOUNT pc_pfgcind(FAST DDRIVE *pdr) /* _fn_ */
#endif
	{
	FAST FATSWAP *pfs = pdr->pfs;
	ULONG t;

	/* twelve bit fat entries. 
		Conversion == s
		first entry in page == s
		firstbytenoinpage BYTES * (2/3) ENTRIES/BYTE + 1 
		== first x 8/12 + 1
	*/
	if (pdr->fasize == 3)		/* 3 nibble ? */
		{
		t = (ULONG) pfs->pf_indlow;
		t  = (t << 3); /* times 8 */
		return ( (UCOUNT)(t/12) + 1);
		}
	else
		{
		/* 16 bit FAT */
		/* Index == byte / 2 */
		return ( (UCOUNT) (pfs->pf_indlow >> 1) );
		}
	}

#endif
/*
*****************************************************************************
	PC_FAXX - Get the value store in the FAT at clno.

  
Summary
	#include <pcdisk.h>

	BOOL pc_faxx(pdr, clno, pvalue)
		 DDRIVE	*pdr;
		 UCOUNT clno;
		 UCOUNT *pvalue;

 Description
 	Given a DDRIVE and a cluster number. Get the value in the fat at clusterno
 	(the next cluster in a chain.) Handle 16 and 12 bit fats correctly.

 Returns
 	Returns the the value at clno. In pvalue. 
	If any error occured while FAT swapping return NO else return YES.
	Note: If fat buffering is disabled, always returns yes
Example:
	#include <pcdisk.h>

	Count # of free clusters on the disk

	for (i = 2 ; i <= pdr->maxfindex; i++)
		if ( (nxt = pc_faxx(pdr , i)) == 0)
          	freecount++;

*****************************************************************************
*/

/* Retrieve a value from the fat */
#ifndef ANSIFND
BOOL pc_faxx(pdr, clno, pvalue)
	FAST DDRIVE *pdr;
	UCOUNT clno;
	UCOUNT *pvalue;
#else
BOOL pc_faxx(FAST DDRIVE *pdr, UCOUNT clno, UCOUNT *pvalue) /* _fn_ */
#endif
	{
	UCOUNT	try;
	UCOUNT	result;
	UTINY	wrdbuf[2];			/* Temp storage area */

	wrdbuf;	/* So the compiler won't complain if we do not use it */

	if (pdr->fasize == 3)		/* 3 nibble ? */
		{
		try = clno + (clno >> 1);			/* multiply by 1.5 */
#ifdef USEFATBUF
		if ( pc_pfgbyte( pdr, try, &wrdbuf[0] ) &&
			 pc_pfgbyte( pdr, try+1, &wrdbuf[1] ) )
			result = to_WORD(&wrdbuf[0]); 	/* And use the product as index */
		else
			{
			return (NO);	
			}
#else
		result = to_WORD(&pdr->pf[try]); 	/* And use the product as index */
#endif

		if ( ((clno << 1) + clno) == (try << 1) )	/* If whole number */
			{
			result &= 0xfff;			/* Return it */
			}
		else
			{
			result = ( (result >> 4) & 0xfff ); /* shift right 4 */
			}
		}
	else	/* 16 BIT fat. ret the value at 2 * clno */
		{
		try = clno << 1;
#ifdef USEFATBUF
		if ( pc_pfgbyte( pdr, try, &wrdbuf[0] ) &&
			 pc_pfgbyte( pdr, try+1, &wrdbuf[1] ) )
			result = to_WORD(&wrdbuf[0]); 	/* And use the product as index */
		else
			{
			return (NO);
			}
#else
		result = to_WORD(&pdr->pf[try]);
#endif
		}

	*pvalue = result;
	return (YES);
	}

/*
*****************************************************************************
	PC_FLUSHFAT -  Write any dirty FAT blocks to disk

				   
Summary
	#include <pcdisk.h>

	BOOL pc_flushfat(driveno)
		COUNT driveno;


 Description
 	Given a valid drive number. Write any fat blocks to disk that
 	have been modified. Updates all copies of the fat.

 Returns
 	Returns NO if driveno is not an open drive. Or a write failed.

Example:
	#include <pcdisk.h>

	pc_flushfat(0);

*****************************************************************************
*/

#ifdef USEFATBUF

/* Get the drive structure and call the FATSWAP code's flush routine. */
#ifndef ANSIFND
BOOL pc_flushfat(driveno)
	COUNT driveno;
#else
BOOL pc_flushfat(COUNT driveno) /* _fn_ */
#endif
	{
	FAST DDRIVE	*pdr;

	if ( !(pdr = pc_drno2dr(driveno) ) )
		{
		return(NO);
		}
	else
		return ( pc_pfflush(pdr) );
	}

#else

/* Consult the dirty fat block list and write any. write all copies
   of the fat */
#ifndef ANSIFND
BOOL pc_flushfat(driveno)
	COUNT driveno;
#else
BOOL pc_flushfat(COUNT driveno) /* _fn_ */
#endif
	{
	FAST UTINY *pf;
	FAST UTINY *pd;
	FAST COUNT i;
	COUNT j;
	BLOCKT baseblock;
	FAST DDRIVE	*pdr;

	if ( !(pdr = pc_drno2dr(driveno) ) )
		{
		return(NO);
		}

	pf = pdr->pf;
	pd = pdr->pdirty;
	baseblock = pdr->fatblock;


	if (!(pd && pf) )
		{
	 	pr_db_putstr("flushfat called with null pointer\n");
		return (NO);
		}

	for (j = 0; j < pdr->numfats;j++)
		{
		pf = pdr->pf;
		baseblock = pdr->fatblock + (BLOCKT) (j * pdr->secpfat);
		for (i = 0,pd = pdr->pdirty; i <  pdr->secpfat; i++,pd++,baseblock++)
			{
			if (*pd != (UTINY) 0)
				if(!pblock(pdr->driveno,baseblock,pf,1))
					{
				 	pr_er_putstr("Cant flush F.A.T\n");
				 	return(NO);
				 	}
			pf += 512;
			}
		}
	for (i = 0,pd = pdr->pdirty;i <  pdr->secpfat; i++)
		*pd++ = (UTINY) 0;
	return (YES);
	}

#endif
/*
*****************************************************************************
	PC_FREECHAIN - Free a cluster chain associated with an inode.

					   
Summary
	
	#include <pcdisk.h>

	VOID pc_freechain(pobj)
		DROBJ *pobj;

 Description
	Trace the cluster chain for an inode and return all the clusters to the   
	free state for re-use. The FAT is not flushed.

 Returns
 	Nothing.


Example
	#include <pcdisk.h>

	pc_freechain(pobj)
	pobj->finode->fcluster = 0;
	pc_flushfat();

*****************************************************************************
*/

#ifndef ANSIFND
VOID	pc_freechain(pobj)
	FAST DROBJ *pobj;
#else
VOID	pc_freechain(FAST DROBJ *pobj) /* _fn_ */
#endif
	{
	UCOUNT cluster;
	UCOUNT nextcluster;
	DDRIVE *pdrive = pobj->pdrive;
	
	cluster = pobj->finode->fcluster;
	nextcluster = pc_clnext(pdrive , cluster);

	while (cluster)
		{
		pc_clrelease(pdrive , cluster);
		cluster = nextcluster;
		nextcluster = pc_clnext(pdrive , nextcluster);
		}
	}
/*
*****************************************************************************
	GBLK0 -  Read block 0 and load values into a a structure

					   
Summary
	#include <pcdisk.h>

	BOOL gblk0( driveno, pbl0 )
		UCOUNT driveno;
		struct pcblk0 *pbl0;
		{

 Description
	Given a path name a valid drive number, read block zero and convert
	its contents from intel to native byte order.

 Returns
 	Returns YES if all went well.

Example:
	#include <pcdisk.h>

	if (!gblk0((UCOUNT) driveno, &bl0 ))
		{
		pr_er_putstr("Can't read block 0:pc_dskinit\n");
		pc_mfree(pdr);
		return(NO);
		}

*****************************************************************************
*/

/* read block zero */
#ifndef ANSIFND
BOOL gblk0( driveno, pbl0 )
	UCOUNT driveno;
	struct pcblk0 *pbl0;
#else
BOOL gblk0(UCOUNT driveno, struct pcblk0 *pbl0) /* _fn_ */
#endif
	{
	UTINY b[512];

	/* get 1 block starting at 0 from driveno */
    if (!gblock(driveno,BLOCKEQ0,&b[0],1))
		{
	  	return(NO);
	  	}

	/* Now load the structure from the buffer */
	pbl0->jump = b[0];
	copybuff( &pbl0->oemname[0],&b[3],8);
	pbl0->oemname[8] = '\0';
    pbl0->bytspsector = to_WORD(&b[0xb]);
	pbl0->secpalloc = b[0xd];
	pbl0->secreserved = to_WORD(&b[0xe]);
	pbl0->numfats = b[0x10];
	pbl0->numroot = to_WORD(&b[0x11]);
	pbl0->numsecs = to_WORD(&b[0x13]);
	pbl0->mediadesc = b[0x15];
	pbl0->secpfat = to_WORD(&b[0x16]);
	pbl0->secptrk = to_WORD(&b[0x18]);
	pbl0->numhead = to_WORD(&b[0x1a]);
	pbl0->numhide = to_WORD(&b[0x1c]);

#ifdef SUPPORTDOS4
	pbl0->numhide2 = to_WORD(&b[0x1e]);
	pbl0->numsecs2 = to_DWORD(&b[0x20]); /* # secs if > 32M (4.0) */
	pbl0->physdrv = b[24];			/* Physical Drive No. (4.0) */
	pbl0->filler = b[25];				/* Reserved (4.0) */
	pbl0->xtbootsig = b[26];		/* Extended signt 29H if 4.0 stuf valid */
	pbl0->volid = to_DWORD(&b[0x27]);	/* Unique number per volume (4.0) */
	copybuff( &pbl0->vollabel[0],&b[0x2b],11); /* Volume label (4.0) */
#endif

#if 0
		pr_db_int ("Jump 	  ",pbl0->jump);
		pr_db_str ("Oem NAME  ",pbl0->oemname);
		pr_db_int ("Bytspsec  ",pbl0->bytspsector);
		pr_db_int ("secpallc  ",pbl0->secpalloc);
		pr_db_int ("secres    ",pbl0->secreserved);
		pr_db_int ("numfat    ",pbl0->numfats);
		pr_db_int ("numrot    ",pbl0->numroot);
		pr_db_int ("numsec    ",pbl0->numsecs);
		pr_db_int ("mediac    ",pbl0->mediadesc);
		pr_db_int ("secfat    ",pbl0->secpfat);
		pr_db_int ("sectrk    ",pbl0->secptrk);
		pr_db_int ("numhed    ",pbl0->numhead);
		pr_db_int ("numhide   ",pbl0->numhide);
#endif

 	return(YES);
}

/*
*****************************************************************************
	Globals -  Important globals.

					   
Summary
	GLOBAL FINODE *inoroot = NULL;		 Begining of inode pool 
	GLOBAL BLKBUFF *broot = NULL;		 Beginning of block buff pool 
	GLOBAL DROBJ *lcwd[HIGHESTDRIVE+1] = {NULL};	 current working dirs 
	GLOBAL PC_FILE *filearray[MAXUFILES] = {NULL};   Open files 


Description

Returns

Example:

*****************************************************************************
*/

#include <posix.h>

GLOBAL FINODE *inoroot = NULL;		/* Begining of inode pool */
GLOBAL BLKBUFF *broot = NULL;		/* Beginning of block buff pool */
GLOBAL DROBJ *lcwd[HIGHESTDRIVE+1] = {NULL};	/* current working dirs */
GLOBAL PC_FILE *filearray[MAXUFILES] = {NULL};  /* Open files */
GLOBAL DDRIVE *drv_array[HIGHESTDRIVE+1] = {NULL}; /* Array of open drives */
GLOBAL COUNT dfltdrv = 0; /* Default drive to use if no drive specified */
/*
*****************************************************************************
	PC_IFREE - Count the number of free bytes remaining on a disk (internal)

  
Summary
	#include <pcdisk.h>

	LONG pc_ifree(driveno)
		COUNT driveno;

 Description
 	Given a drive number count the number of free bytes on the drive. (called
 	by pc_free).

 Returns
	 The number of free bytes or zero if the drive is full or 
	 it it not open or out of range.

Example:
	#include <pcdisk.h>
	printf("%l bytes remaining \n",pc_ifree(0) );

*****************************************************************************
*/

#ifndef ANSIFND
LONG pc_ifree(driveno)
	COUNT driveno;
#else
LONG pc_ifree(COUNT driveno) /* _fn_ */
#endif
	{
	FAST DDRIVE	*pdr;
	FAST UCOUNT i;
	UCOUNT nxt;
	LONG freecount = 0L;

	if ( !(pdr = pc_drno2dr(driveno) ) )
		{
		return(0L);
		}

	for (i = 2 ; i <= pdr->maxfindex; i++)
		{
		/* If an io error occurs during FAT swapping, just return 0 bytes free
		*/
		if (!pc_faxx(pdr, i, &nxt))
			{
			return(0L);
			}
		if (nxt == 0)
          	freecount++;
		}
	return (freecount * pdr->bytespcluster);
	}


/*
*****************************************************************************
	PC_PFAXX - Write a value to the FAT at clno.

  
Summary
	#include <pcdisk.h>

	BOOL pc_pfaxx(pdr, clno, value)
		DDRIVE	*pdr;
		UCOUNT  clno;
		UCOUNT  value;


 Description
 	Given a DDRIVE,cluster number and value. Write the value in the fat
	at clusterno. Handle 16 and 12 bit fats correctly.

 Returns
	No if an io error occurred during fat swapping, else YES.

Example:
	#include <pcdisk.h>
	
	Alloc a cluster and attach it to the current cluster if not at the
	begining of the chain

	if ((nxt = pc_clalloc(pdr)))
		if (clno)
			if (!pc_pfaxx(pdr, clno, nxt))
				return (0);
	return(nxt);

*****************************************************************************
*/

#ifdef USEFATBUF

/* Given a clno & fatval Put the value in the table at the index (clno)  */

/* Using the FAT Buffering code */
#ifndef ANSIFND
BOOL pc_pfaxx(pdr, clno, value)
	FAST DDRIVE	*pdr;
	UCOUNT  clno;
	UCOUNT  value;
#else
BOOL pc_pfaxx(FAST DDRIVE	*pdr, UCOUNT  clno, UCOUNT  value) /* _fn_ */
#endif
	{
	FAST UCOUNT	try;
	UTINY wrdbuf[2];			/* Temp storage area */

#ifdef	CLUSTER_DEBUG
	dbg_api(("pc_pfaxx: writing %d to fat entry %d\n", value, clno));
#endif	CLUSTER_DEBUG
	if (pdr->fasize == 3)		/* 3 nibble ? */
		{
		value &= 0x0fff;        /* 3 nibble clusters */
		try = clno + (clno >> 1);			/* multiply by 1.5 */

		/* We have to get the current values at the locations in the FAT
		   and modify them , so first we read them in */
		/* See the in-memory version below for a cleaner view */

		if (! ( pc_pfgbyte( pdr, try, &wrdbuf[0] ) &&
			 pc_pfgbyte( pdr, try+1, &wrdbuf[1] ) ) )
			{
			return (NO);
			}

		if ( ((clno << 1) + clno) == (try << 1) )	/* If whole number */
			{
			wrdbuf[0] = ((UTINY)value & 0xff); /* Low Byte to NIBBLE 1 & 2 */
			wrdbuf[1] &= 0xf0; /* clr low NIBBLE of next byte */
			/* Put the high nibble of value in the low nibble ofnext byte */
			wrdbuf[1] |= ( ((UTINY)(value>>8)) & 0x0f );
            }
		 else
			{
			wrdbuf[1]=((UTINY)(value>>4) & 0xff); /* high to NIB 2 & 3*/
			wrdbuf[0] &= 0x0f; /* clr high NIBBLE of byte */
			/* Put the low nibble of value in the high nibble of byte */
			wrdbuf[0] |= ( ((UTINY)(value & 0xf) << 4) & 0xf0 );
            }
		}
	else		/* 16 BIT entries */
		{
	    try = (clno << 1);
	    fr_WORD(&wrdbuf[0],value);
	    }

	/* Now put the values back into the FAT */
	if (! ( pc_pfpbyte( pdr, try, wrdbuf[0] ) &&
		 pc_pfpbyte( pdr, try+1, wrdbuf[1] ) ) )
		{
		return (NO);
		}

	return (YES);
	}

#else

/* Given a clno & fatval Put the value in the table at the index (clno)  */

/* Using the in-memory FAT Buffers */
#ifndef ANSIFND
BOOL pc_pfaxx(pdr, clno, value)
	FAST DDRIVE	*pdr;
	UCOUNT  clno;
	UCOUNT  value;
#else
BOOL pc_pfaxx(FAST DDRIVE	*pdr, UCOUNT  clno, UCOUNT  value) /* _fn_ */
#endif
	{
	FAST UCOUNT	try;

#ifdef	CLUSTER_DEBUG
	dbg_api(("pc_pfaxx: writing %d to fat entry %d\n", value, clno));
#endif	CLUSTER_DEBUG
	if (pdr->fasize == 3)		/* 3 nibble ? */
		{
		value &= 0x0fff;        /* 3 nibble clusters */
		try = clno + (clno >> 1);			/* multiply by 1.5 */
		if ( ((clno << 1) + clno) == (try << 1) )	/* If whole number */
			{
			pdr->pf[try] = ((UTINY)value & 0xff); /* Low Byte to NIBBLE 1 & 2 */
			pdr->pf[try+1] &= 0xf0; /* clr low NIBBLE of next byte */
			/* Put the high nibble of value in the low nibble ofnext byte */
			pdr->pf[try+1] |= ( ((UTINY)(value>>8)) & 0x0f );
            }
		 else
			{
			pdr->pf[try+1]=((UTINY)(value>>4) & 0xff); /* high to NIB 2 & 3*/
			pdr->pf[try] &= 0x0f; /* clr high NIBBLE of byte */
			/* Put the low nibble of value in the high nibble of byte */
			pdr->pf[try] |= ( ((UTINY)(value & 0xf) << 4) & 0xf0 );
            }
		}
	else		/* 16 BIT entries */
		{
	    try = (clno << 1);
	    fr_WORD(&pdr->pf[try],value);
	    }
    /* Now mark the dirty flags block == index/512 */
    /* Note try>>9 and try+1 >> 9 are usually the same */
    pdr->pdirty[(try>>9)] = (UTINY) 1;
    pdr->pdirty[((try+1)>>9)] = (UTINY) 1; 
	return (YES);
	}

#endif
/*
*****************************************************************************
	PC_SEC2CLUSTER - Convert a block number to its cluster representation.

   
Summary
	#include <pcdisk.h>

	UCOUNT pc_sec2cluster(pdrive, blockno)
		DRIVE *pdrive;
		BLOCKT blockno;


 Description
 	Convert blockno to its cluster representation if it is in cluster space.

 Returns
 	Returns 0 if the block is not in cluster space, else returns the
 	cluster number associated with block.

Example:
	#include <pcdisk.h>

	cluster = pc_sec2cluster(pdrive,curblock);
	Consult the fat for the next cluster 
	if ( !(cluster = pc_clnext(pdrive, cluster)) )
		return (0);	 End of chain
	else
		return (pc_cl2sector(pdrive, cluster));

				
*****************************************************************************
*/

/* Cluster<->sector conversion routines */

/* Convert sector to cluster. 0 == s error */
#ifndef ANSIFND
UCOUNT pc_sec2cluster(pdrive, blockno)
	DDRIVE *pdrive;
	BLOCKT blockno;
#else
UCOUNT pc_sec2cluster(DDRIVE *pdrive, BLOCKT blockno) /* _fn_ */
#endif
	{
	if (blockno >= pdrive->numsecs)
		 return (0);
	else if ( pdrive->firstclblock > blockno)
		return (0);
	else
		return (
				(UCOUNT)
				(2 + (blockno - pdrive->firstclblock)/pdrive->secpalloc)
			   );
	}

/*
*****************************************************************************
	PC_SEC2INDEX - Calculate the offset into a cluster for a block.
   
Summary
	#include <pcdisk.h>

	UCOUNT pc_sec2index(pdrive, blockno)
		DRIVE *pdrive;
		BLOCKT blockno;


 Description
	Given a block number offset from the beginning of the drive, calculate
	which block number within a cluster it will be. If the block number
	coincides with a cluster boundary, the return value will be zero. If it
	coincides with a cluster boundary + 1 block, the value will be 1, etc.

 	
 Returns
 	0,1,2 upto blockspcluster -1.

Example:
	#include <pcdisk.h>

	If the next block is not on a cluster edge then it must be
	in the same cluster as the current. - otherwise we have to
	get the firt block from the next cluster in the chain
	
	if (pc_sec2index(pdrive, curblock))
		return (curblock);

*****************************************************************************
*/

/* Convert sector to index into a cluster . No error detection */
#ifndef ANSIFND
UCOUNT pc_sec2index(pdrive, blockno)
	DDRIVE *pdrive;
	BLOCKT blockno;
#else
UCOUNT pc_sec2index(DDRIVE *pdrive, BLOCKT blockno) /* _fn_ */
#endif
	{
	return ( (UCOUNT)
			((blockno - pdrive->firstclblock) % pdrive->secpalloc) );
	}


