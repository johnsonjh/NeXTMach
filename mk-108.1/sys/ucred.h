/*	@(#)ucred.h	2.1 88/05/18 4.0NFSSRC SMI; from SMI 1.3 87/09/16	*/

/*
 * HISTORY
 * 19-Dec-88  Peter King (king) at NeXT
 *	Original NFS 4.0 source.
 */

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#ifndef _sys_ucred_h
#define _sys_ucred_h

/*
 * User credential structure
 */
struct ucred {
 	u_short		cr_ref;			/* reference count */
 	uid_t 	 	cr_uid;			/* effective user id */
 	gid_t  		cr_gid;			/* effective group id */
 	uid_t  		cr_ruid;		/* real user id */
 	gid_t		cr_rgid;		/* real group id */
 	gid_t  		cr_groups[NGROUPS];	/* groups, 0 terminated */
};

#ifdef KERNEL
#define	crhold(cr)	(cr)->cr_ref++
void crfree();
struct ucred *crget();
struct ucred *crcopy();
struct ucred *crdup();
struct ucred *crgetcred();
#endif KERNEL

#endif !_sys_ucred_h
