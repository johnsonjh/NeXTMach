/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 02-May-90  Morris Meyer (mmeyer) at NeXT
 *	Added CD-ROM file system mount type.
 *
 * 27-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: Additional mount options, hooks for other filesystems
 *
 * 11-Aug-87  Peter King (king) at NeXT
 *	Add SUN_VFS support.  Also, added NFS mount structures.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)mount.h	7.1 (Berkeley) 6/4/86
 */

/*	@(#)mount.h	1.3 88/05/18 4.0NFSSRC SMI	*/

/*
 * mount options
 */
#define M_RDONLY	0x01	/* mount fs read only */
#define M_NOSUID	0x02	/* mount fs with setuid not allowed */
#define	M_NEWTYPE	0x04	/* use type string instead of int */
#define	M_GRPID		0x08	/* Old BSD group-id on create */
#define M_REMOUNT	0x10	/* change options on an existing mount */
#define M_NOSUB		0x20	/* Disallow mounts beneath this mount */
#define M_MULTI		0x40	/* Do multi-component lookup on files */

#if	NeXT
#define	M_VIRTUAL	0x8000	/* Internal kernel flag for virtual mount */
#endif	NeXT

/*
 * File system types, these corespond to entries in fsconf
 */
#ifdef NeXT
#define	MOUNT_UFS	0
#define	MOUNT_NFS	1
#define	MOUNT_PC	2
#define	MOUNT_LO	3
#define MOUNT_SPECFS	4
#define MOUNT_CFS	5		/* CD-ROM File system          */
#define	MOUNT_MAXTYPE	6		/* allow for runtime expansion */
#else NeXT
#define	MOUNT_UFS	1
#define	MOUNT_NFS	2
#define	MOUNT_PC	3
#define	MOUNT_LO	4
#define	MOUNT_MAXTYPE	5		/* allow for runtime expansion */
#endif NeXT

struct ufs_args {
	char	*fspec;
};

#ifdef NeXT
struct pc_args {
	char	*fspec;
};

struct cfs_args {
	char 	*fspec;
};
#endif NeXT

#ifdef LOFS
struct lo_args {
	char    *fsdir;
};
#endif LOFS

#ifdef RFS
struct token {
	int	t_id;	 /* token id for differentiating multiple ckts	*/
	char	t_uname[64]; /* full domain name of machine, 64 = MAXDNAME */
};

struct rfs_args {
	char    *rmtfs;		/* name of service (fs) */
	struct token *token;	/* identifier of remote mach */
};

/*
 * RFS mount option flags
 */
#define RFS_RDONLY	0x001	/* read-only: passed with remote mount request */
#endif	RFS

