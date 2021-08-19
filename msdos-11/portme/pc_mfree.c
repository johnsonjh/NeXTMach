/*
*****************************************************************************
	PC_MFREE - System storage de-allocator

					   
Summary
	
	#include <posix.h>

	VOID pc_mfree(ptr)
		VOID *ptr;

 Description
 	Return bytes allocated by pc_alloc to the heap. If your compiler
 	libraries supply malloc and free then you should un-comment the 
	constant "USEEBMALLOC" in the disk header and pc_malloc and pc_mfree 
	will call malloc and free. Otherwise a local malloc and free capability
	is provided. Don't confuse pc_mfree() with pc_free() they are very 
	different functions.

	Note: You should look at this code to be sure that the allignment
	mechanism is correct for your CPU.
	This code is not necessarilly portable !!!.


Example:
	#include <pcdisk.h>

	TEXT *mybuff;

	if (! (mybuff = (TEXT *) pc_malloc(1024) )
		printf("Out of core\n");
	else
		{
		do_something(mybuff);
		pc_free(mybuff);
		}

*****************************************************************************
*/
#include <pcdisk.h>

#ifdef MSDOS
#include <stdlib.h>
#include <malloc.h>
#endif

#ifdef	NeXT
#import <sys/types.h>
#import <nextdos/msdos.h>
#import <nextdos/dosdbg.h>
#import <portme/pc_malloc.h>
extern void kfree(void *p, int size);
#endif	NeXT

#ifdef DBALLOC
/* To check for matched alloc free */
IMPORT VOID *allo_db[];
#endif

#ifndef ANSIFND
VOID pc_mfree( ptr )
	VOID *ptr;
	{
#else
VOID pc_mfree(VOID *ptr) /* _fn_ */
	{
#endif

#ifdef	KERNEL
	/*
	 * In the kernel, we have to keep track of the malloc's size for use
	 * with kalloc()/kfree(). Also for debugging, we flag the region
	 * malloc'd with a magic number at malloc and verify (then clear) the
	 * magic number at free time. 
	 */
	u_int nbytes;
	u_int *ip;
	
	ip = (u_int *)(ptr - MALLOC_OFFSET);
	nbytes = ip[SIZE_OFFSET];
	if(ip[MAGIC_OFFSET] != MALLOC_MAGIC) 
		panic("MSDOS pc_mfree: Duplicate pc_mfree");
	ip[MAGIC_OFFSET] = 0;
	dbg_malloc(("pc_mfree: 0x%x bytes @ 0x%x\n", nbytes, ip));
	kfree(ip, nbytes);
	return;
	
#else	KERNEL

#ifdef 	USEEBMALLOC

	pc_i_free(ptr);
#else	USEEBMALLOC
	free(ptr);
#endif	USEEBMALLOC

#ifdef 	DBALLOC
{
INT i;
for (i = 0; i < 500; i++)
	if (allo_db[i] == ptr)
		{
		allo_db[i] = NULL;
		break;
		}
}
#endif	DBALLOC
#endif	KERNEL

	}

#ifdef DBALLOC
VOID pc_memdisp()
	{
	IMPORT BLKBUFF *broot;
	FAST COUNT i;
	FAST BLKBUFF *p;
	IMPORT COUNT allo_db_si[];
	BOOL foundit;

	for (i = 0; i < 500 ; i++)
		{
		if (allo_db[i])
			{
			foundit = NO;
			if (broot)
				{
				p = broot;
				while (p)
					{
					if ( (p == (BLKBUFF *) allo_db[i]) ||
						 (p->pdata == (UTINY *) allo_db[i]) )
						 {
						 foundit = YES;
						 break;
						 }
					p = p->pnext;
					}
				}
			if (!foundit)
				printf("Core unaccounted for %i\n", allo_db_si[i]);
			}
		}
	}

#endif
