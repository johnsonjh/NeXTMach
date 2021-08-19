/*
 * HISTORY
 * 15-Apr-90  Simson Garfinkel (simsong) in Cambridge.
 *	Adopted for use with CFS from nfs_subr.c
 *
 * 28-Oct-87  Peter King (king) at NeXT, Inc.
 *	Original Sun source, ported to Mach.
 */ 

#include "sys/param.h"
#include "sys/systm.h"
#include "sys/dir.h"
#include "sys/user.h"
#include "sys/vnode.h"
#include "sys/vfs.h"
#include "sys/file.h"
#include "sys/uio.h"
#include "sys/mount.h"
#include "kern/mfs.h"
#include "vm/vm_page.h"
#include "kern/zalloc.h"
#include "kern/lock.h"

#include "cdrom.h"
#include "cfs.h"

#define	CTABLESIZE	16
#define	ctablehash(ex) ((ex->e_start ^ ex->e_bytes) & (CTABLESIZE-1))

struct cnode *ctable[CTABLESIZE];


/* The free list */
static	zone_t	cnode_zone 		= ZONE_NULL;
static	struct 	cnode *cpfreelist 	= NULL;
static	lock_t	freelist_lock;

/* Stats */

int 	creuse	= 0;
int	cnew	= 0;
int	cactive	= 0;


/*
 * Macro to say if to extents are the same
 */

#define same_ext(x,y) ((((x)->e_vp==(y)->e_vp)) \
		       && (((x)->e_start == (y)->e_start)) \
		       && (((x)->e_bytes == (y)->e_bytes)))
  


/*
 * Lookup a cnode by extent
 */
static struct cnode *c_find(extent *ex)
{
	struct cnode *rt;
	 
	dprint4("   c_find: dev: %x estart:%d bytes:%d  hash:%d\n",
	       ex->e_vp,ex->e_start,ex->e_bytes,ctablehash(ex));

	rt = ctable[ctablehash(ex)]; 
	while (rt != NULL) { 

		dprint3("    checking %x/%d/%d\n",
		       ctoext(rt)->e_vp,ctoext(rt)->e_start,
			ctoext(rt)->e_bytes);

		if (same_ext(ctoext(rt),ex)){

			dprint("   match found!\n");
			ctov(rt)->v_count++;	/* increment use count */
			return(rt); 
		}	

		rt = rt->c_next;
	}	
	dprint("   not found\n");
	return (NULL);
}

/* makecfsnode:
 * Returns a vnode for a given extent.  We are also given a flag.
 * If no cnode exists for this extent create one and put it in
 * a table hashed by fh_fsid and fs_fid.  If the cnode for
 * this fhandle is already in the table return it (ref count is
 * incremented by c_find.)  The cnode will be flushed from the
 * table when cfs_inactive calls cunsave.
 */

struct vnode *makecfsnode(extent *ex, struct vfs *vfsp)
{
	struct 	cnode *cp;

	dprint("makecfsnode...\n");

	if ((cp = c_find(ex)) == NULL) {

		struct	mntinfo	*vfs_mi = (struct mntinfo *)vfsp->vfs_data;

		/* A vnode for the extent is not in the cache
		 *
		 * First find a node that we can use; try to take one
		 * off the freelist first.
		 */
		   
		
		lock_write(freelist_lock);
		if (cpfreelist) {
			/* SLG: There is a freelist,
			 * so grab the next node from there.
			 */

			cp 		= cpfreelist;
			cpfreelist 	= cp->c_next;
			creuse++;

			dprint1("   from FREELIST: cp=%x\n",cp);

		} else {
			/* SLG: Looks like there is no freelist (nothing in it)
			 * so create it from scratch.
			 */
			cp = (struct cnode *)zalloc(cnode_zone);
			cnew++;

			dprint("    CREATED NEW cnode\n"); 
		}

		lock_done(freelist_lock);

		/* Now initialize the cnode / vnode */

		bzero(cp,sizeof(*cp));

		cp->c_vnode.v_data 	= (caddr_t)cp;
		cp->c_vnode.v_op 	= &cfs_vnodeops;
		cp->c_vnode.vm_info 	= VM_INFO_NULL;

		vm_info_init(&cp->c_vnode);
		vm_set_vnode_size(&cp->c_vnode,ex->e_bytes);
		vm_set_close_flush(&cp->c_vnode,0);

		/* Use "approved macro" to set:
		 * 	flag, count, shlock, vfsp, type, rdev,
		 *	socket, vfstime
		 */
		VN_INIT(&cp->c_vnode,vfsp,ex->e_type,0);

		cp->c_ext 		= *ex;	/* remember extent! */

		/* check to see if we are root */
		if(same_ext(&vfs_mi->root_dir,ex)){
			
			dprint("THIS IS A ROOT VNODE\n");
			cp->c_vnode.v_flag	|= VROOT;
		}


		/* save the cnode in cache and increment the reference
		 * counters with the number of active cnodes.
		 */

		csave(cp);			
		vfs_mi->mi_refct++;
		cactive++;
	}

	dprint4("makecfsnode stats:  creuse=%d cnew=%d active=%d cp=%x\n",
	       creuse,cnew,cactive,cp);
	return (&cp->c_vnode);
}

/*
 * Cnode lookup stuff.
 * These routines maintain a table of cnodes hashed by extents so
 * that the cnode for an extent can be found if it already exists.
 * NOTE: CTABLESIZE must be a power of 2 for ctablehash to work!
 */

/*
 * Put a cnode in the hash table
 */
void	csave(struct cnode *cp)
{
	dprint2("    csave(cp=%x)  hash=%d\n",cp,ctablehash(ctoext(cp)));
	dprint2("    saved estart: %d bytes: %d\n",
	       ctoext(cp)->e_start,ctoext(cp)->e_bytes);
	cp->c_next = ctable[ctablehash(ctoext(cp))];
	ctable[ctablehash(ctoext(cp))] = cp;
}

/*
 * Remove a cnode from the hash table
 */
static	void	cunsave(struct cnode *cp)
{
	struct cnode *rt;
	struct cnode *rtprev = NULL;
	 
	rt = ctable[ctablehash(ctoext(cp))]; 
	while (rt != NULL) { 
		if (rt == cp) { 
			if (rtprev == NULL) {
				ctable[ctablehash(ctoext(cp))] = rt->c_next;
			} else {
				rtprev->c_next = rt->c_next;
			}
			return; 
		}	
		rtprev = rt;
		rt = rt->c_next;
	}	
}

/* Put a cnode on the free list and take it out of
 * the hash table.
 */

/*
 * Put a cnode on the free list and take it out if
 * the hash table.
 *
 * Also free its vm_info struct.
 */
void	cfs_cfree(struct cnode *cp)
{
	dprint1("   cfs_cfree(cp=%x)\n",cp);
	cunsave(cp);				/* take off hash 	*/
	vm_info_free(ctov(cp));			/* free vm info struct 	*/

	lock_write(freelist_lock);
	cp->c_next = cpfreelist;		/* add to free list 	*/
	cpfreelist = cp;
	cactive--;
	lock_done(freelist_lock);
}


/* Create the cnode_zone and the freelist_lock */

void	clist_init()
{
	cnode_zone 	= zinit(sizeof(struct cnode ),
				10000*sizeof(struct cnode),
				0, 0, "cnode structures");

	freelist_lock	= lock_alloc();
	lock_init(freelist_lock,TRUE);
}

