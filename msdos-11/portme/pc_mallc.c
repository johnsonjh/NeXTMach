/*
*****************************************************************************
	PC_MALLOC - System storage allocator

					   
Summary
	
	#include <posix.h>

	VOID *pc_malloc(size )
		int size

 Description
 	Allocate "size" bytes off the heap and return them. If your compiler
 	libraries supply malloc and free then you should un-comment the
 	constant "USEEBMALLOC" in the disk header and pc_malloc and pc_mfree
	will call malloc and free. Otherwise a local malloc and free capability
	is provided.

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
		pc_mfree(mybuff);
		}

*****************************************************************************
*/

/* MSC uses preprocessor tricks to remap malloc to _fmalloc or _nmalloc
   depending on model. */
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
extern void *kalloc(int size);
#endif	NeXT

#ifdef DBALLOC
VOID *allo_db[500] = { NULL };
COUNT allo_db_si[500] = {0};
#endif
#ifndef ANSIFND
VOID *pc_malloc( nbytes )
	int nbytes;
	{
#else
VOID *pc_malloc( int nbytes ) /* _fn_ */
	{
#endif
	
#ifdef	KERNEL
	/*
	 * In the kernel, we have to keep track of the malloc's size for use
	 * with kalloc()/kfree(). Also for debugging, we flag the region
	 * malloc'd with a magic number at malloc and verify (then clear) the
	 * magic number at free time. 
	 */
	u_char *cp;
	u_int *ip;
	u_int size = nbytes + MALLOC_OFFSET;
	
	ip = kalloc(size);
	dbg_malloc(("pc_malloc: 0x%x bytes @ 0x%x\n", size, ip));
	if(!ip) {
		dbg_err(("pc_malloc: KALLOC RETURNED 0\n"));
		return(0);
	}
	else {
		ip[SIZE_OFFSET] = size;
  		ip[MAGIC_OFFSET] = MALLOC_MAGIC;
		cp = (u_char *)ip;
		return(cp + MALLOC_OFFSET);
	}
#else	KERNEL

	VOID *p;

#ifdef 	USEEBMALLOC
	if ( !(p = pc_i_malloc(nbytes)) )
#else	USEEBMALLOC
	if ( !(p = malloc(nbytes)) )
#endif	USEEBMALLOC
		pr_er_putstr("Malloc error\n");
#ifdef 	DBALLOC
{
INT i;
for (i = 0; i < 500; i++)
	if (!allo_db[i])
		{
		allo_db[i] = p;
		allo_db_si[i] = nbytes;
		break;
		}
}
#endif	DBALLOC
	return (p);
#endif	KERNEL
	}

