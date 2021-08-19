/* 
 * HISTORY
 * 26-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: Removed dir.h.  Cleaned up credentials handling
 *
 * 26-Oct-87  Peter King (king) at NeXT
 *	Original Sun source.  Upgraded to Mach.
 */ 

/*	@(#)vfs_dnlc.c	2.2 88/05/24 4.0NFSSRC SMI;  from SMI 2.15 86/08/11	*/

#import <sys/param.h>
#import <sys/user.h>
#import <sys/systm.h>
#import <sys/vnode.h>
#import <sys/pathname.h>
#import <sys/dnlc.h>

/*
 * Directory name lookup cache.
 * Based on code originally done by Robert Els at Melbourne.
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (vp, name) where the vp refers to the
 * directory containing the name.
 *
 * For simplicity (and economy of storage), names longer than
 * some (small) maximum length are not cached, they occur
 * infrequently in any case, and are almost never of interest.
 */

int	ncsize;				/* static size of the cache */
struct	ncache *ncache;			/* storage for the cache entries */
struct	nc_hash nc_hash[NC_HASH_SIZE];	/* headers for hash chains */
struct	nc_lru nc_lru;			/* header for lru chain */
struct	ncstats ncstats;		/* cache effectiveness statistics */

int	doingcache = 1;

/*
 * Initialize the directory cache.
 * Put all the entries on the LRU chain and clear out the hash links.
 */
dnlc_init()
{
	register struct ncache *ncp;
	register int i;

	nc_lru.lru_next = (struct ncache *) &nc_lru;
	nc_lru.lru_prev = (struct ncache *) &nc_lru;
	for (i = 0; i < ncsize; i++) {
		ncp = &ncache[i];
		INS_LRU(ncp, &nc_lru);
		NULL_HASH(ncp);
		ncp->dp = ncp->vp = (struct vnode *) 0;
#ifdef	NeXT
		ncp->symLinkValid = FALSE;
#endif	NeXT
	}
	for (i = 0; i < NC_HASH_SIZE; i++) {
		ncp = (struct ncache *) &nc_hash[i];
		NULL_HASH(ncp);
	}
}

/*
 * Add a name to the directory cahce.
 */
dnlc_enter(dp, name, vp, cred)
	register struct vnode *dp;
	register char *name;
	struct vnode *vp;
	struct ucred *cred;
{
	register int namlen;
	register struct ncache *ncp;
	register int hash;

	if (!doingcache) {
		return;
	}
	namlen = strlen(name);
	if (namlen > NC_NAMLEN) {
		ncstats.long_enter++;
		return;
	}
	hash = NC_HASH(name, namlen, dp);
	ncp = dnlc_search(dp, name, namlen, hash, cred);
	if (ncp != (struct ncache *) 0) {
		ncstats.dbl_enters++;
		return;
	}
	/*
	 * Take least recently used cache struct.
	 */
	ncp = nc_lru.lru_next;
	if (ncp == (struct ncache *) &nc_lru) {	/* LRU queue empty */
		ncstats.lru_empty++;
		return;
	}
	/*
	 * Remove from LRU, hash chains.
	 */
	RM_LRU(ncp);
	RM_HASH(ncp);

#if	NeXT	
	/*
	 *  If this was a valid cache entry, decrement the count of valid entries
	 */
	if (ncp->dp != (struct vnode *) 0 && ncp->vp != (struct vnode *) 0) {
		ncstats.valid_entries--;
		assert(ncstats.valid_entries >= 0);
	}
#endif	NeXT	

	/*
	 * Drop hold on vnodes (if we had any).
	 */
	if (ncp->dp != (struct vnode *) 0) {
		VN_RELE(ncp->dp);
	}
	if (ncp->vp != (struct vnode *) 0) {
		VN_RELE(ncp->vp);
	}
	if (ncp->cred != (struct ucred *) 0) {
		crfree(ncp->cred);
	}
#if	NeXT
	if (ncp->symLinkValid) {
		assert(ncp->symLink != (char *) 0);
		kfree((void *) ncp->symLink, (long) ncp->symLinkLength);
	}
#endif	NeXT
		
	/*
	 * Hold the vnodes we are entering and
	 * fill in cache info.
	 */
	ncp->dp = dp;
	VN_HOLD(dp);
	ncp->vp = vp;
	VN_HOLD(vp);
	ncp->namlen = namlen;
	bcopy(name, ncp->name, (unsigned)namlen);
#if	NeXT
	ncp->symLinkValid = FALSE;
	ncp->symLinkLength = 0;
	ncp->symLink = (char *) 0;
#endif	NeXT
	ncp->cred = cred;
	if (cred) {
		crhold(cred);
	}
	/*
	 * Insert in LRU, hash chains.
	 */
	INS_LRU(ncp, nc_lru.lru_prev);
	INS_HASH(ncp, &nc_hash[hash]);
	
#ifdef	NeXT
	ncstats.valid_entries++;
#endif	NeXT
	ncstats.enters++;
}


#if	NeXT
/*
 *  Lookup a symbolic link name/vnode pair in the cache.
 */
struct ncache *
dnlc_lookupSymLink(linkName, dvp)
	char *linkName;			/* name of link */
	struct vnode *dvp;		/* vp of directory in which link lives */
{
	int nameLength = strlen(linkName);
	int hashValue;
	
	if (nameLength <= NC_NAMLEN) {
		hashValue = NC_HASH(linkName, nameLength, dvp);
		return dnlc_search(dvp, linkName, nameLength, hashValue, ANYCRED);
	} else
		return (struct ncache *) 0;
}


/*
 *  Enter a symbolic link into the cache.
 */
void
dnlc_enterSymLink(linkName, dvp, pnp)
	char *linkName;			/* name of link */
	struct vnode *dvp;		/* vp of directory in which link lives */
	struct pathname *pnp;		/* contents of link */
{
	int nameLength = pnp->pn_pathlen;
	struct ncache *ncp;
	
	ncp = dnlc_lookupSymLink(linkName, dvp);
	
	if (ncp == (struct ncache *) 0)
		return;
	
	if (ncp->symLinkValid) {	/* someone could have beaten us here */
	
		if (ncp->symLinkLength != pnp->pn_pathlen ||
		    bcmp(pnp->pn_buf, ncp->symLink, pnp->pn_pathlen) != 0)
		    	kfree(ncp->symLink);
		else
			return;		/* if it's the same, we're done */
	}
		 
	
	ncp->symLink = (char *) kalloc(nameLength);
	
	if (ncp->symLink != (char *) 0) {
		ncp->symLinkValid = TRUE;
		ncp->symLinkLength = nameLength;
		bcopy(pnp->pn_buf, ncp->symLink, nameLength);
		/*
		 * Move this slot to the end of LRU chain.
		 */
		RM_LRU(ncp);
		INS_LRU(ncp, nc_lru.lru_prev);
	}
}
#endif	NeXT


/*
 * Look up a name in the directory name cache.
 */
struct vnode *
dnlc_lookup(dp, name, cred)
	struct vnode *dp;
	register char *name;
	struct ucred *cred;
{
	register int namlen;
	register int hash;
	register struct ncache *ncp;

	if (!doingcache) {
		return ((struct vnode *) 0);
	}
	namlen = strlen(name);
	if (namlen > NC_NAMLEN) {
		ncstats.long_look++;
		return ((struct vnode *) 0);
	}
	hash = NC_HASH(name, namlen, dp);
	ncp = dnlc_search(dp, name, namlen, hash, cred);
	if (ncp == (struct ncache *) 0) {
		ncstats.misses++;
		return ((struct vnode *) 0);
	}
	ncstats.hits++;
	/*
	 * Move this slot to the end of LRU
	 * chain.
	 */
	RM_LRU(ncp);
	INS_LRU(ncp, nc_lru.lru_prev);
	/*
	 * If not at the head of the hash chain,
	 * move forward so will be found
	 * earlier if looked up again.
	 */
	if (ncp->hash_prev != (struct ncache *) &nc_hash[hash]) {
		RM_HASH(ncp);
		INS_HASH(ncp, ncp->hash_prev->hash_prev);
	}
	return (ncp->vp);
}

/*
 * Remove an entry in the directory name cache.
 */
dnlc_remove(dp, name)
	struct vnode *dp;
	register char *name;
{
	register int namlen;
	register struct ncache *ncp;
	int hash;

	namlen = strlen(name);
	if (namlen > NC_NAMLEN) {
		return;
	}
	hash = NC_HASH(name, namlen, dp);
	while (ncp = dnlc_search(dp, name, namlen, hash, ANYCRED)) {
		dnlc_rm(ncp);
	}
}

/*
 * Purge the entire cache.
 */
dnlc_purge()
{
	register struct nc_hash *nch;
	register struct ncache *ncp;

	ncstats.purges++;
start:
	for (nch = nc_hash; nch < &nc_hash[NC_HASH_SIZE]; nch++) {
		ncp = nch->hash_next;
		while (ncp != (struct ncache *) nch) {
			if (ncp->dp == 0 || ncp->vp == 0) {
				panic("dnlc_purge: zero vp");
			}
			dnlc_rm(ncp);
			goto start;
		}
	}
}

/*
 * Purge any cache entries referencing a vnode.
 */
dnlc_purge_vp(vp)
	register struct vnode *vp;
{
	register int moretodo;
	register struct ncache *ncp;

	do {
		moretodo = 0;
		for (ncp = nc_lru.lru_next; ncp != (struct ncache *) &nc_lru;
		    ncp = ncp->lru_next) {
			if (ncp->dp == vp || ncp->vp == vp) {
				dnlc_rm(ncp);
				moretodo = 1;
				break;
			}
		}
	} while (moretodo);
}

/*
 * Purge any cache entry.
 * Called by iget when inode freelist is empty.
 */
dnlc_purge1()
{
	register struct ncache *ncp;

	for (ncp = nc_lru.lru_next; ncp != (struct ncache *) &nc_lru;
	    ncp = ncp->lru_next) {
		if (ncp->dp) {
			dnlc_rm(ncp);
			return (1);
		}
	}
	return (0);
}

/*
 * Obliterate a cache entry.
 */
static
dnlc_rm(ncp)
	register struct ncache *ncp;
{
	/*
	 * Remove from LRU, hash chains.
	 */
	RM_LRU(ncp);
	RM_HASH(ncp);
	/*
	 * Release ref on vnodes.
	 */
	VN_RELE(ncp->dp);
	ncp->dp = (struct vnode *) 0;
	VN_RELE(ncp->vp);
	ncp->vp = (struct vnode *) 0;
	if (ncp->cred != NOCRED) {
		crfree(ncp->cred);
		ncp->cred = NOCRED;
	}
#if	NeXT
	if (ncp->symLinkValid) {
		assert(ncp->symLink != (char *) 0);
		kfree((void *) ncp->symLink, (long) ncp->symLinkLength);
		ncp->symLinkValid = FALSE;
		ncp->symLinkLength = 0;
	}
#endif	NeXT
	
	/*
	 * Insert at head of LRU list (first to grab).
	 */
	INS_LRU(ncp, &nc_lru);
	/*
	 * And make a dummy hash chain.
	 */
	NULL_HASH(ncp);

#if	NeXT	
	ncstats.valid_entries--;
	assert(ncstats.valid_entries >= 0);
#endif	NeXT	
}

/*
 * Utility routine to search for a cache entry.
 */
struct ncache *
dnlc_search(dp, name, namlen, hash, cred)
	register struct vnode *dp;
	register char *name;
	register int namlen;
	int hash;
	struct ucred *cred;
{
	register struct nc_hash *nhp;
	register struct ncache *ncp;

	nhp = &nc_hash[hash];
	for (ncp = nhp->hash_next; ncp != (struct ncache *) nhp;
	    ncp = ncp->hash_next) {
		if (ncp->dp == dp && ncp->namlen == namlen &&
		    *ncp->name == *name &&	/* fast chk 1st chr */
		    bcmp(ncp->name, name, namlen) == 0 &&
		    (cred == ANYCRED || ncp->cred == cred ||
		     (cred->cr_uid == ncp->cred->cr_uid &&
		      cred->cr_gid == ncp->cred->cr_gid &&
		      bcmp((caddr_t)cred->cr_groups, 
			   (caddr_t)ncp->cred->cr_groups,
			   NGROUPS * sizeof(cred->cr_groups[0])) == 0))) {
			return (ncp);
		}
	}
	return ((struct ncache *) 0);
}

/*
 * Insert into queue, where the queue pointers are
 * in the second two longwords.
 * Should be in assembler like insque.
 */
insque2(ncp2, ncp1)
	register struct ncache *ncp2, *ncp1;
{
	register struct ncache *ncp3;

	ncp3 = ncp1->lru_next;
	ncp1->lru_next = ncp2;
	ncp2->lru_next = ncp3;
	ncp3->lru_prev = ncp2;
	ncp2->lru_prev = ncp1;
}

/*
 * Remove from queue, like insque2.
 */
remque2(ncp)
	register struct ncache *ncp;
{
	ncp->lru_prev->lru_next = ncp->lru_next;
	ncp->lru_next->lru_prev = ncp->lru_prev;
}





