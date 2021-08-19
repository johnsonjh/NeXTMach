/*
 * merged version of EBS block/*.c
 *
*****************************************************************************
	PC_ALLOC_BLK - Find existing or create an empty block in the buffer pool.

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_alloc_blk( ppblk, pdrive , blockno )
		BLKBUFF **ppblk;
		DDRIVE *pdrive;
		UCOUNT blockno;


Description
	Use pdrive and blockno to search for a buffer in the buffer pool.
	If not found create a new buffer entry by discarding the Least recently
	used buffer in the buffer pool. The buffer is locked. A pointer to
	the buffer is returned in ppblk.

	Note: If all buffers are locked, the function will sleep until
	a buffer is available. In a non multitasking system this will cause
	a panic. It is a sure sign that some code is not freeing a buffer
	when it is done with it.

 Returns
 	Returns YES if the buffer was found in the pool or NO if a new
 	buffer was assigned.

Example:

	#include <pcdisk.h>

	BLKBUFF *pblk;

	Find it in the buffer pool or read it.

	if (!pc_alloc_blk(&pblk, pdrive , blockno) )
		{
		Not found. Read it in 
		if (!gblock(pdrive->driveno, blockno, pblk->pdata, (COUNT) 1))
			{
			pc_free_buf(pblk, YES);
			pblk = NULL;
			}
		}

*****************************************************************************
*/

#include <pcdisk.h>
#ifdef	NeXT
#import <kern/lock.h>
#import <nextdos/msdos.h>
#import <nextdos/dosdbg.h>
void thread_wakeup(void *event);
void thread_block();
void assert_wait(int event, boolean_t interruptible);
#endif	NeXT

#ifndef ANSIFND
BOOL pc_alloc_blk( ppblk, pdrive , blockno )
	BLKBUFF **ppblk;
	DDRIVE *pdrive;
	BLOCKT blockno;
#else
BOOL pc_alloc_blk(BLKBUFF **ppblk, DDRIVE *pdrive, BLOCKT blockno) /* _fn_ */
#endif
	{
	FAST BLKBUFF *pblk;
	FAST BLKBUFF *oldest = NULL;
	ULONG lru = (ULONG) ~0L;
	LOCAL ULONG  useindex = 0L;
	PROCL pl;
	BOOL rtn;
	
#ifdef	NeXT
	dbg_cache(("pc_alloc_blk: blockno %d\n", blockno));
	lock_write(buff_lockp);		/* must release before returning */
#else	NeXT
	pl = pc_inhibit();
#endif	NeXT
	/* Get or init the block pool */
	pblk = pc_blkpool(pdrive); 

	useindex += 1;
	while (pblk)
		{
		if (!pblk->pdrive)
			{
			break;		/* This buffer's free */
			}
		else
			{
			if ( (pblk->pdrive == pdrive) && (pblk->blockno == blockno)  )
				{
				/* Found it */
				if(pblk->locked)
					{
					while(pblk->locked)
						{
						/* Sleep til unlocked and re-find, just in 
						 * case someone munges it before we wake up 
						 */
#ifdef	NeXT
						dbg_cache(("pc_alloc_blk: releasing "
							"buff_lock before sleeping\n"));
						lock_done(buff_lockp);
#endif	NeXT
						pc_sleep(pblk);
						rtn = pc_alloc_blk(ppblk, pdrive, blockno);
						dbg_cache(("pc_alloc_blk: rtn 0x%x\n", 
								*ppblk));
						return(rtn);
						}
					}
				else
					{
					/* Found it , not locked. report the good news */
					*ppblk = pblk;
					/* Update the last recently used stuf */
					pblk->lru = useindex;
					pblk->locked = YES;
#ifdef	NeXT
					lock_done(buff_lockp);
					dbg_cache(("pc_alloc_blk: returning 0x%x\n", pblk));
#else	NeXT
					pc_allow(pl);
#endif	NeXT
					return(YES);
					}
				}
			else
				{
				/* No match. see if its a candidate for swapping if we run out of
				   pointers */
				if (!pblk->locked)
					{
					if (pblk->lru < lru)
						{
						lru = pblk->lru;
						oldest = pblk;
						}
					}
				}
			}
		pblk = pblk->pnext;
		}

	/* If off the end of the list we have to bump somebody */
	if (!pblk)
		pblk = oldest;

	if (!pblk)
		pc_buffpanic("CLAIM"); 

	pblk->lru = useindex;
	pblk->pdrive = pdrive;
	pblk->blockno = blockno;
	pblk->locked = YES;
	*ppblk = pblk;
	
	/* Return NO since we didn't find it in the buffer pool */
#ifdef	NeXT
	lock_done(buff_lockp);
	dbg_cache(("pc_alloc_blk: returning 0x%x (invalid)\n", pblk));
#else	NeXT
	pc_allow(pl);
#endif	NeXT
	return (NO);
	}


/*
*****************************************************************************
	PC_BLKBOOL - Return the first element in a drives buffer pool
					   
Summary
	
	#include <pcdisk.h>

	BLKBUFF *pc_blkpool(pdrive)
		DDRIVE *pdrive;


Description
	Return the beginning of the buffer pool for a drive. If the pool
	is uninitialized, initialize it.
	Panics if not enough core to init the buffer pool.

 Returns
 	Returns the beginning of the buffer pool.

Example:
	#include <pcdisk.h>
	BLKBUFF *pbuff;

	pbuff = pc_blkpool( pobj->pdrive);

*****************************************************************************
*/

/* Return the beginning of the buffer pool . Initialize the buffer bool
  if you have to */
#ifndef ANSIFND
BLKBUFF *pc_blkpool(pdrive)
	DDRIVE *pdrive;
#else
BLKBUFF *pc_blkpool(DDRIVE *pdrive) /* _fn_ */
#endif
	{
	IMPORT BLKBUFF *broot;
	FAST COUNT i;
	FAST BLKBUFF *p;
	FAST BLKBUFF *pprev;
	UTINY *pbuff;

	/* Note we do not touch the drive struct here but we could easilly
	   change this to a per drive buffer pool and enhance performance at 
	   the cost of memory */
	pdrive;

	if (!broot)
		{
		/* Initialize the buffer pool */
		for (i = 0; i < NBLKBUFFS; i++)
			{
			if ( (p =     (BLKBUFF *) pc_malloc(sizeof (BLKBUFF)) )&&
				 (pbuff = (UTINY   *) pc_malloc(512             ) ) )
					{
					p->pdrive = NULL;
					p->blockno = BLOCKEQ0;
					p->pdata = pbuff;
					p->lru = 0L;
					p->locked = NO;
					}
			else
				pc_buffpanic("ALLOC");

			/* Stitch it in to the linked list */
			if (!broot)
				broot = p;
			else
				pprev->pnext = p;

			pprev = p;
			}
		/* We're at the end. terminate it */
		pprev->pnext = NULL;
		}
	return (broot);
	}
/*
*****************************************************************************
	PC_FREE_ALL_BLK - Release all buffers associated with a drive

					   
Summary
	
	#include <pcdisk.h>

  
	VOID pc_free_all_blk(pdrive)
		DDRIVE *pdrive;


Description
	Use pdrive to find all buffers in the buffer pool associated with the
	drive. Mark them as unused, called by dsk_close.
	If any are locked, print a debug message in debug mode to warn the
	programmer.

 Returns
 	Nothing


Example:

	#include <pcdisk.h>

	DDRIVE *pdrive;


	pc_free_all_blk(pdrive);


*****************************************************************************
*/
  
#ifndef ANSIFND
VOID pc_free_all_blk(pdrive)
	DDRIVE *pdrive;
#else
VOID pc_free_all_blk(DDRIVE *pdrive) /* _fn_ */
#endif
	{
	FAST BLKBUFF *pblk;
	PROCL pl;

#ifdef	NeXT
	dbg_cache(("pc_free_all_blk: pdrive %d\n", pdrive));
	lock_write(buff_lockp);
	lock_set_recursive(buff_lockp);
#else	NeXT
	pl = pc_inhibit();
#endif	NeXT
	/* Get or init the block pool */
	pblk = pc_blkpool(pdrive); 
	while (pblk)
		{
		if (pblk->pdrive == pdrive)
			{
			if (pblk->locked)
				pr_db_str("Warning: ",
					"freeing a locked buffer\n");
			/*
			 * locks support recursion; no deadlock 
			 * on this call...
			 */
			pc_free_buf(pblk, YES);
			}
		pblk = pblk->pnext;
		}
#ifdef	NeXT
	lock_clear_recursive(buff_lockp);
	lock_done(buff_lockp);
	dbg_cache(("pc_free_all_blk: done\n")); 
#else	NeXT
	pc_allow(pl);
#endif	NeXT
	}
/*
*****************************************************************************
	PC_FREE_BUF - Unlock a block buffer and wake up anybody waiting for it

					   
Summary
	
	#include <pcdisk.h>

	VOID pc_free_buf( pblk, error )
		BLKBUFF *pblk;
		BOOL error;
		{


Description
	Clear the lock for block and call pc_wakeup to wake anyone who wants
	it. If error is YES, discard the buffer from the buffer pool so 
	future buffer access will attempt to read the data from disk.
	Failing to call freeblock when you are done with a block will cause
	gridlock. Hogging a buffer by not calling freeblock does not give you 
	the performance advantage you might expect anyway since the buffer pool
	uses an LRU algorithm to replace buffers and a frequently used buffer
	will be in the pool.

	Warning: As of 7/89 the multitasking facilities are unproven, but must
	call free_buf at appropriate times or you will run out of buffers.
	
 Returns
 	Nothing

Example:

	#include <pcdisk.h>
	
	Were done , release the buffer.
	pc_free_buf(pobj->pblkbuff))

*****************************************************************************
*/

/* Free a buffer by unlocking it. If waserr is true, zero out the  
 	drive number so the erroneous data is not cached. */
#ifndef ANSIFND
VOID pc_free_buf(pblk, waserr)
	BLKBUFF *pblk;
	BOOL	waserr;
#else
VOID pc_free_buf(BLKBUFF *pblk, BOOL	waserr) /* _fn_ */
#endif
	{
	PROCL pl;

#ifdef	NeXT
	dbg_cache(("pc_free_buf: pblk 0x%x  blockno %d\n", pblk, pblk->blockno));
	lock_write(buff_lockp);
#else	NeXT
	pl = pc_inhibit();
#endif	NeXT
	if (pblk)
		{
		pblk->locked = NO;
		if (waserr)
			pblk->pdrive = NULL;
		/* Wake up any body waiting for this buffer */
		pc_wakeup(pblk);
		}
#ifdef	NeXT
	dbg_cache(("pc_free_buf: done\n"));
	lock_done(buff_lockp);
#else	NeXT
	pc_allow(pl);
#endif	NeXT
	}
/*
*****************************************************************************
	PC_INIT_BLK - Zero a BLKBUFF and add it to the buffer pool
					   
Summary
	
	#include <pcdisk.h>


	BLKBUFF *pc_init_blk( pdrive , blockno )
		DRIVE *pdrive;	
		BLOCKT blockno;


Description
	Allocate and zero a BLKBUFF and add it to the to the buffer pool.
	You should be sure the block is not already in the buffer pool before
	calling this function.

	Note: After initializing you "own" the buffer. You must release it by
	calling pc_free_buff() before others can use it. In a multitasking
	system pc_inhibit, pc_allow, pc_sleep and pc_wakeup will keep thing
	synchronized.

 Returns
 	Returns a valid pointer or NULL if no core.

Example:
	#include <pcdisk.h>
	BLKBUFF *pbuff;

	if (!(pbuff = pc_init_blk( pobj->pdrive , pd->my_block)))
		{
		pc_clrelease(pobj->pdrive , cluster);
		return (NO);
		}
	pc_ino2dos (  (DOSINODE *) pbuff->pdata ,  pobj->finode ) ;
	if ( !pc_write_blk ( pbuff ) )
		{
		pc_free_buf(pbuff,YES);
		pc_clrelease(pobj->pdrive , cluster);
		return (NO);
		}


*****************************************************************************
*/

#ifndef ANSIFND
BLKBUFF *pc_init_blk( pdrive , blockno )
	DDRIVE *pdrive;
	BLOCKT blockno;
#else
BLKBUFF *pc_init_blk(DDRIVE *pdrive, BLOCKT blockno) /* _fn_ */
#endif
	{
	BLKBUFF *pblk;

	if ( !pdrive || (blockno >= pdrive->numsecs) )
		return(NULL);
	else
		{
		pc_alloc_blk(&pblk, pdrive , blockno );
		pc_memfill(pblk->pdata, 512, '\0');
		return (pblk);
		}
	}


/*
*****************************************************************************
	PC_MISC - Miscelaneous block and process control functions
					   
Summary
	
	#include <pcdisk.h>

	VOID pc_buffpanic(message)  Call this if the buffer pool is locked up.
		TEXT *message;			probably someone isn't freeing buffers

	PROCL pc_inhibit()			Entering sensitive area, raise processor level
								(mutixing support not ready (8/8/89)
	
	VOID pc_allow(pl)			Leaving sensitive area, lower processor level
		PROCL pl;				(mutixing support not ready (8/8/89)

	VOID pc_sleep(channel)		Sleep til channel is ready, usually wait
		VOID *channel;          for a free buffer. Currently panics since
								single tasking assumed.
	
	VOID pc_wakeup(channel)		A resourse is available, wake anybody waiting
		VOID *channel;

	VOID pc_errout(m)			Print a string to error port. 
		TEXT *m;				(same as pr_er_putsr)


Description

 Returns

Example:

*****************************************************************************
*/

#ifndef ANSIFND
VOID pc_buffpanic(message)
	TEXT *message;
#else
VOID pc_buffpanic(TEXT *message) /* _fn_ */
#endif
	{
#ifdef	NeXT
	printf("DOS File System Panic\n");
	panic(message);
#endif	NeXT
	message;
	pc_errout(message);
	while (1);
	}

/* Set the processor interupt level so the pc package won't be interupted 
	In a multitasking system you tie this to you spl() or other function
*/
#ifndef ANSIFND
PROCL pc_inhibit()
#else
PROCL pc_inhibit() /* _fn_ */
#endif
	{
#ifdef	NeXT
	panic("DOS File System: pc_inhibit");
#endif	NeXT
	return ( /* spl(diskdrivelevel) */ (PROCL) 0);
	}
#ifndef ANSIFND
VOID pc_allow(pl)
	PROCL pl;
#else
VOID pc_allow(PROCL pl) /* _fn_ */
#endif
	{
	pl;
#ifdef	NeXT
	panic("DOS File System: pc_allow");
#endif	NeXT
	/* spl(pl) */
	}

/* Sleep and wakeup. In a multi tasking environment
   these should be linked to the hosts sleep/wakeup
   Note:
   	when the host wakes up he should be at the pc_inhibit 
   	level
 */
#ifndef ANSIFND
VOID pc_sleep(channel)
	VOID *channel;
#else
VOID pc_sleep(VOID *channel) /* _fn_ */
#endif
	{
	channel;
#ifdef	NeXT
	dbg_cache(("pc_sleep: channel 0x%x\n", channel));
	assert_wait((int)channel, FALSE);
	thread_block();
	return;
#else	NeXT
	/* sleep(channel, pcdiskdrivelevel) */
	pc_errout((TEXT *)"Sleep called, Uh Oh don't know what to do with it \n");
#endif	NeXT
	}

#ifndef ANSIFND
VOID pc_wakeup(channel)
	VOID *channel;
#else
VOID pc_wakeup(VOID *channel) /* _fn_ */
#endif
	{
#ifdef	NeXT
	dbg_cache(("pc_wakeup: channel 0x%x\n", channel));
	thread_wakeup(channel);
	return;
#else	NeXT
	channel;
#endif	NeXT
	}

#ifndef ANSIFND
VOID pc_errout(m)
	TEXT *m;
#else
VOID pc_errout(TEXT *m) /* _fn_ */
#endif
	{
	pr_er_str((TEXT *)"E: ",m);
	}
/*
*****************************************************************************
	PC_READ_BLK - Allocate and read a BLKBUFF, or get it from the buffer pool.

					   
Summary
	
	#include <pcdisk.h>


	BLKBUFF *pc_read_blk( pdrive , blockno )
		DDRIVE *pdrive;
		BLOCKT blockno;
		{


Description
	Use pdrive and blockno to determine what block to read. Read the block
	or get it from the buffer pool and return the buffer.

	Note: After reading, you "own" the buffer. You must release it by
	calling pc_free_buff() before others can use it.


 Returns
 	Returns a valid pointer or NULL if block not found and not readable.

Example:

	#include <pcdisk.h>

	return(pobj->pblkbuff = pc_read_blk(pobj->pdrive, pobj->blkinfo.my_block));

*****************************************************************************
*/

#ifndef ANSIFND
BLKBUFF *pc_read_blk( pdrive , blockno )
	DDRIVE *pdrive;
	BLOCKT blockno;
#else
BLKBUFF *pc_read_blk(DDRIVE *pdrive, BLOCKT blockno) /* _fn_ */
#endif
	{
	BLKBUFF *pblk;

	if ( !pdrive || (blockno >= pdrive->numsecs) )
		return(NULL);
	else
		{
		/* Find the existing block. or grab an empty slot */
		if (!pc_alloc_blk(&pblk, pdrive , blockno) )
			{
			/* Not found. Read it in */
			if (!gblock(pdrive->driveno, blockno, pblk->pdata, (COUNT) 1))
				{
				/* OOPS. Free it , and mark it bad, */
				pc_free_buf(pblk, YES);
				pblk = NULL;
				}
			}
		}
	return (pblk);
	}

/*
*****************************************************************************
	PC_WRITE_BLK - Flush a BLKBUFF to disk.

					   
Summary
	
	#include <pcdisk.h>

	BOOL pc_write_blk( pblk )
		BLKBUFF *pblk;
		{


Description
	Use pdrive and blockno information in pblk to flush it's pdata buffer
	to disk.

 Returns
 	Returns YES if the write succeeded.

Example:

	#include <pcdisk.h>

	if (!pc_write_blk(pobj->pblkbuff))
		printf("Could not write.\n);

*****************************************************************************
*/


/* Write */
#ifndef ANSIFND
BOOL pc_write_blk( pblk )
	BLKBUFF *pblk;
#else
BOOL pc_write_blk(BLKBUFF *pblk) /* _fn_ */
#endif
	{
	return (pblock(pblk->pdrive->driveno,pblk->blockno,pblk->pdata, (COUNT) 1));
	}


