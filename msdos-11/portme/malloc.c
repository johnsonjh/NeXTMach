/*
*****************************************************************************
	malloc - low level system storage allocator/deallocator

					   
Summary
	
	#include <pcdisk.h>

	VOID pc_i_free(ptr)
		VOID *ptr;
	
	TEXT *pc_i_malloc(nbytes)
		INT nbytes;

	TEXT *pc_sbrk(nbytes)
		INT nbytes;

	
 Description
	These are the free and malloc routines from k & r volume 1 pgs 173-176
	with a few minor changes. They are provided just in case your environment
	does not have free and malloc. 

	If USEEBMALLOC is defined in the pcdisk.h file then this code will be used
	otherwise malloc and free will be called.

	If you use this code keep these things in mind:

		1. PCHEAPSIZE is the maximum heapsize - you may change it
		2. pc_sbrk()  will probably need to be re-written for your 
					  environment.
		

 	Return bytes allocated by pc_alloc to the heap. If your compiler
 	libraries supply malloc and free then you should un-comment the constant
 	"USEEBMALLOC" in the disk header and pc_malloc and pc_mfree will call
 	malloc and free. Otherwise a local malloc and free capability is provided.
 	Don't confuse pc_mfree() with pc_free() they are very different functions.

	Note: You should look at this code to be sure that the allignment mechanism
	is correct for your CPU.
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

#ifdef USEEBMALLOC

/* malloc code lifted directly from K & R. (page 173) Some modifications
   have been made to include sbrk and some functions were renamed so they
   do not clash with the same fn's in other libraries */

#define ALIGN	int
#define HEADER	union header
#define NALLOC	128
#define NULL	0

/* PORT-ME - Static heap. You may not implement things this way at all */
/* Start with a 32k heap. (this may be a little small on a large winch 
   in release 1.00 */

#define PCHEAPSIZE 0x8000

HEADER
{
	struct
	{
    HEADER *	ptr;
    unsigned	size;
	} s;
	ALIGN	x;
};

static HEADER	base;
static HEADER *allocp = NULL;
static HEADER *morecore(unsigned nu);

/*Core: */
char   pc_core[PCHEAPSIZE] = {0};

/*This is pointer to memory. At starup it will be NULL. That means we will 
   point it at pc_core and zero it. For the test system I point pc_break
   at pc_core. On a rom based system you should get rid of pc_core and point
   pc_break into free ram. */
static char	  *pc_break = NULL;

#ifndef ANSIFND
void pc_i_free(ap)
	char *ap;
	{
#else
void pc_i_free(char *ap) /* _fn_ */
	{
#endif
	register HEADER *p, *q;

	p = (HEADER *) ap - 1;		/*point to header */

	for (q = allocp ; !(p > q && p < q->s.ptr) ; q = q->s.ptr)
	    if (q >= q->s.ptr && (p > q || p < q->s.ptr))
			break;	/*at one end or other */

	if (p + p->s.size == q->s.ptr)	/*join to upper nbr */
		{
	    p->s.size += q->s.ptr->s.size;
	    p->s.ptr   = q->s.ptr->s.ptr;
		}
	else
	    p->s.ptr = q->s.ptr;

	if (q + q->s.size == p) 	/*join to lower nbr */
		{
	    q->s.size += p->s.size;
	    q->s.ptr   = p->s.ptr;
		}
	else
	    q->s.ptr = p;

	allocp = q;
	}

#ifndef ANSIFND
static HEADER *morecore(nu)
	unsigned nu;
	{
#else
static HEADER *morecore(unsigned nu) /* _fn_ */
	{
#endif
	register char *    cp;
	register HEADER *  up;
	register int	    rnu;

	rnu = NALLOC *(( nu + NALLOC - 1) / NALLOC);
	cp  = pc_sbrk(rnu *sizeof(HEADER));
	
	/* EBS Mod from K & R. if sbrk returns NULL there is no more core. */
	if (!cp)	    /*no space at all */
	    return NULL;

	up = (HEADER *) cp;	    /*fixup header info */
	up->s.size = rnu;

	pc_i_free((char *) (up + 1));    /*insert new space onto free list */

	return allocp;
}

#ifndef ANSIFND
char *pc_i_malloc(nbytes)
	UCOUNT nbytes;
	{
#else
char *pc_i_malloc(UCOUNT nbytes) /* _fn_ */
	{
#endif
	FAST HEADER *p, *q;
	FAST COUNT 	nunits;

	nunits = 1 + (nbytes + sizeof(HEADER) - 1) / sizeof(HEADER);

	if ((q = allocp) == NULL)   /*no free list yet */
		{
	    base.s.ptr	= allocp = q = &base;
	    base.s.size = 0;
		}

	for (p = q->s.ptr ; ; q = p,p = p->s.ptr)
		{
		if (p->s.size >= nunits)	    /*big enough */
	    	{
			if (p->s.size == nunits)    /*exactly */
			    q->s.ptr = p->s.ptr;
			else			    /*allocate tail end */
				{
			    p->s.size -= nunits;
		    	p += p->s.size;
			    p->s.size  = nunits;
				}
			allocp = q;
			return ((char *) (p + 1));
	    	}
		 if (p == allocp)		    /*wrapped around free list */
			if ((p = morecore((unsigned) nunits)) == NULL)
			    return NULL;
		}
	}

/* EBS specific code: Set this up for your systems. */
#ifndef ANSIFND
char *pc_sbrk(nbytes)
	int nbytes;
	{
#else
char *pc_sbrk(int nbytes) /* _fn_ */
	{
#endif
	FAST UCOUNT i;
	FAST char *p;

	/* Zero out core if needed */
	if (!pc_break)
		{
		p = &(pc_core[0]);
		for (i = 0; i < PCHEAPSIZE; i++)
			*p++ = '\0';
		pc_break = &(pc_core[0]);
		}

	/* Return the current break */
	p = pc_break;
	pc_break += nbytes;

	/* Not enough core ? */
	if ( (UCOUNT) (pc_break - &(pc_core[0]) ) >= PCHEAPSIZE )
		{
		pc_break -= nbytes;
		return (NULL);
		}
	else
		return(p);
	}

#endif
