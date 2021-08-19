/* 
 **********************************************************************
 * HISTORY
 * 26-Jan-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes.
 *
 *  3-Nov-87  Peter King (king) at NeXT, Inc.
 *	Changed struct pmap to struct portmap.
 *
 **********************************************************************
 */ 

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.18 88/02/08 
 */


/*
 * pmap_prot.c
 * Protocol for the local binder service, or pmap.
 */

#ifdef	KERNEL
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/pmap_prot.h>
#else
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/pmap_prot.h>
#endif

bool_t
xdr_pmap(xdrs, regs)
	XDR *xdrs;
	struct portmap *regs;
{

	if (xdr_u_long(xdrs, &regs->pm_prog) && 
		xdr_u_long(xdrs, &regs->pm_vers) && 
		xdr_u_long(xdrs, &regs->pm_prot))
		return (xdr_u_long(xdrs, &regs->pm_port));
	return (FALSE);
}

