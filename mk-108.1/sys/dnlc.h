/* @(#)dnlc.h	1.1 87/08/26 3.2/4.3NFSSRC */
/*	@(#)dnlc.h 1.1 86/09/25 SMI	*/

/*
 * Copyright (c) 1984 Sun Microsystems Inc.
 */
#import <sys/pathname.h>

/*
 * This structure describes the elements in the cache of recent
 * names looked up.
 */

#define	NC_NAMLEN	15	/* maximum name segment length we bother with*/

struct	ncache {
	struct	ncache	*hash_next, *hash_prev;	/* hash chain, MUST BE FIRST */
	struct 	ncache	*lru_next, *lru_prev;	/* LRU chain */
	struct	vnode	*vp;			/* vnode the name refers to */
	struct	vnode	*dp;			/* vno of parent of name */
	char		namlen;			/* length of name */
	char		name[NC_NAMLEN];	/* segment name */
	struct	ucred	*cred;			/* credentials */
#if	NeXT
	char		*symLink;		/* contents if a symbolic link */
	char		symLinkValid;		/* TRUE if symLinkValue is valid */
	short		symLinkLength;		/* length (excluding null) */
#endif	NeXT
};

#define	ANYCRED	((struct ucred *) -1)
#define	NOCRED	((struct ucred *) 0)

#define	NC_HASH_SIZE		64	/* size of hash table */

#define	NC_HASH(namep, namlen, vp)	\
	((namep[0] + namep[namlen-1] + namlen + (int) vp) & (NC_HASH_SIZE-1))

/*
 * Macros to insert, remove cache entries from hash, LRU lists.
 */
#define	INS_HASH(ncp,nch)	insque(ncp, nch)
#define	RM_HASH(ncp)		remque(ncp)

#define	INS_LRU(ncp1,ncp2)	insque2((struct ncache *) ncp1, (struct ncache *) ncp2)
#define	RM_LRU(ncp)		remque2((struct ncache *) ncp)

#define	NULL_HASH(ncp)		(ncp)->hash_next = (ncp)->hash_prev = (ncp)

/*
 * Stats on usefulness of name cache.
 */
struct	ncstats {
	int	hits;		/* hits that we can really use */
	int	misses;		/* cache misses */
	int	enters;		/* number of enters done */
	int	dbl_enters;	/* number of enters tried when already cached */
	int	long_enter;	/* long names tried to enter */
	int	long_look;	/* long names tried to look up */
	int	lru_empty;	/* LRU list empty */
	int	purges;		/* number of purges of cache */
#if	NeXT
	int	valid_entries;	/* current number of valid entries */
#endif	NeXT
};

/*
 * Hash list of name cache entries for fast lookup.
 */
struct	nc_hash	{
	struct	ncache	*hash_next, *hash_prev;
};

/*
 * LRU list of cache entries for aging.
 */
struct	nc_lru	{
	struct	ncache	*hash_next, *hash_prev;	/* hash chain, unused */
	struct 	ncache	*lru_next, *lru_prev;	/* LRU chain */
};


/*
 *  Globals
 *
 *  ncsize is configuration dependent, and is set in conf/param.c
 *  ncstats is used in ufs_inode.c to determine when to steal inodes from the cache
 */
extern int	ncsize;			/* static size of the cache */
extern struct	ncache *ncache;		/* storage for all the cache entries */
extern struct	ncstats ncstats;	/* cache effectiveness statistics */
extern struct	nc_hash nc_hash[NC_HASH_SIZE];	/* headers for hash chains */
extern struct	nc_lru nc_lru;			/* header for lru chain */

/*
 *  Functions
 */
struct ncache *dnlc_search();
int dnlc_rm(), insque2(), remque2();
#if	NeXT
struct ncache *dnlc_lookupSymLink(char *linkName, struct vnode *dvp);
void dnlc_enterSymLink(char *linkName, struct vnode *dvp, struct pathname *pnp);
#endif	NeXT





