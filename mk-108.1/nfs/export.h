/*      @(#)export.h	2.1 88/05/20 NFSSRC4.0 from 1.4 88/02/08 SMI      */

/*
 * HISTORY
 *  Peter King (king) at NeXT
 *	Original Sun NFS 4.0 source
 */

/*	Copyright (C) 1988 Sun Microsystems Inc.	*/
/*
 * exported vfs flags.
 */
#define EX_RDONLY     0x01		/* exported read only */
#define EX_RDMOSTLY   0x02              /* exported read mostly */

#if	NeXT
/*
 * NeXT: Make upper-bound a more reasonable number
 */
#define EXMAXADDRS 1024			/* max number in address list */
#else	NeXT
#define EXMAXADDRS 10			/* max number in address list */
#endif	NeXT
struct exaddrlist {
	unsigned naddrs;		/* number of addresses */
	struct sockaddr *addrvec;	/* pointer to array of addresses */
};

/*
 * Associated with AUTH_UNIX is an array of internet addresses
 * to check root permission.
 */
#if	NeXT
/*
 * NeXT: Make upper-bound a more reasonable number
 */
#define EXMAXROOTADDRS	1024		/* should be config option */
#else	NeXT
#define EXMAXROOTADDRS	10		/* should be config option */
#endif	NeXT
struct unixexport {
	struct exaddrlist rootaddrs;
};

/*
 * Associated with AUTH_DES is a list of network names to check
 * root permission, plus a time window to check for expired
 * credentials.
 */
#if	NeXT
/*
 * NeXT: Make upper-bound a more reasonable number
 */
#define EXMAXROOTNAMES 1024	   	/* should be config option */
#else	NeXT
#define EXMAXROOTNAMES 10	   	/* should be config option */
#endif	NeXT
struct desexport {
	unsigned nnames;
	char **rootnames;
	int window;
};


/*
 * The export information passed to exportfs()
 */
struct export {
	int ex_flags;	/* flags */
	int ex_anon;	/* uid for unauthenticated requests */
	int ex_auth;	/* switch */
	union {
		struct unixexport exunix;	/* case AUTH_UNIX */
		struct desexport exdes;		/* case AUTH_DES */
	} ex_u;
	struct exaddrlist ex_writeaddrs;
};
#define ex_des ex_u.exdes
#define ex_unix ex_u.exunix

#ifdef KERNEL
/*
 * A node associated with an export entry on the list of exported
 * filesystems.
 */
struct exportinfo {
	struct export exi_export;
	fsid_t exi_fsid;
	struct fid *exi_fid;
	struct exportinfo *exi_next;
};
extern struct exportinfo *findexport();
#endif
