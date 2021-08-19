/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.12 88/02/08 
 **********************************************************************
 * HISTORY
 *  9-May-89  Peter King (king) at NeXT
 *	Sun Bugfixes: 1010338 - Replace printf's with syslog's
 *
 **********************************************************************
 */


/*
 * xdr_reference.c, Generic XDR routines impelmentation.
 *
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * "pointers".  See xdr.h for more info on the interface to xdr.
 */

#ifdef	KERNEL
#import <sys/param.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#else	KERNEL
#import <sys/syslog.h>
#import <stdio.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#endif	KERNEL

#define LASTUNSIGNED	((u_int)0-1)

/*
 * XDR an indirect pointer
 * xdr_reference is for recursively translating a structure that is
 * referenced by a pointer inside the structure that is currently being
 * translated.  pp references a pointer to storage. If *pp is null
 * the  necessary storage is allocated.
 * size is the sizeof the referneced structure.
 * proc is the routine to handle the referenced structure.
 */
bool_t
xdr_reference(xdrs, pp, size, proc)
	register XDR *xdrs;
	caddr_t *pp;		/* the pointer to work on */
	u_int size;		/* size of the object pointed to */
	xdrproc_t proc;		/* xdr routine to handle the object */
{
	register caddr_t loc = *pp;
	register bool_t stat;

	if (loc == NULL)
		switch (xdrs->x_op) {
		case XDR_FREE:
			return (TRUE);

		case XDR_DECODE:
			*pp = loc = (caddr_t) mem_alloc(size);
#ifndef	KERNEL
			if (loc == NULL) {
				(void) syslog(LOG_ERR,
				    "xdr_reference: out of memory");
				return (FALSE);
			}
			bzero(loc, (int)size);
#else
			bzero(loc, size);
#endif	KERNEL
			break;
	}

	stat = (*proc)(xdrs, loc, LASTUNSIGNED);

	if (xdrs->x_op == XDR_FREE) {
		mem_free(loc, size);
		*pp = NULL;
	}
	return (stat);
}

#ifndef	KERNEL
/*
 * xdr_pointer():
 *
 * XDR a pointer to a possibly recursive data structure. This
 * differs with xdr_reference in that it can serialize/deserialiaze
 * trees correctly.
 *
 *  What's sent is actually a union:
 *
 *  union object_pointer switch (boolean b) {
 *  case TRUE: object_data data;
 *  case FALSE: void nothing;
 *  }
 *
 * > objpp: Pointer to the pointer to the object.
 * > obj_size: size of the object.
 * > xdr_obj: routine to XDR an object.
 *
 */
bool_t
xdr_pointer(xdrs,objpp,obj_size,xdr_obj)
	register XDR *xdrs;
	char **objpp;
	u_int obj_size;
	xdrproc_t xdr_obj;
{

	bool_t more_data;

	more_data = (*objpp != NULL);
	if (! xdr_bool(xdrs,&more_data)) {
		return (FALSE);
	}
	if (! more_data) {
		*objpp = NULL;
		return (TRUE);
	}
	return (xdr_reference(xdrs,objpp,obj_size,xdr_obj));
}
#endif	/* ! KERNEL */
