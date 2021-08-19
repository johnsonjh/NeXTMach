/*
 * HISTORY
 *  4-Apr-90  Simson Garfinkel (simsong) in Cambridge.
 *	Took source-code from nfs_vnodeops and:
 *		1. Replaced all functions that involve writing with
 *		   return(EROFS).  I left them seperate functions because
 *		   I may want to use the stubs to implement write-once.
 *
 * 28-Oct-87  Peter King (king) at NeXT, Inc.
 *	Original Sun source, ported to Mach.
 *
 **********************************************************************
 */ 

/*#define  DEBUG*/
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/dir.h"
#include "sys/proc.h"
#include "sys/user.h"
#include "sys/vnode.h"
#include "sys/vfs.h"
#include "sys/file.h"
#include "sys/uio.h"
#include "sys/buf.h"
#include "sys/kernel.h"
#include "sys/proc.h"
#include "sys/mount.h"
#include "kern/kalloc.h"
#include "machine/spl.h"
#include "kern/xpr.h"
#include "vm/vm_page.h"

#include "cdrom.h"				
#include "cfs.h"



/*
 * These are the vnode ops routines which implement the vnode interface to
 * the networked file system.  These routines just take their parameters,
 * make them look networkish by putting the right info into interface structs,
 * and then calling the appropriate remote routine(s) to do the work.
 *
 */

int cfs_open();
int cfs_close();
int cfs_rdwr();
int cfs_ioctl();
int cfs_select();
int cfs_getattr();
int cfs_setattr();
int cfs_access();
int cfs_lookup();
int cfs_create();
int cfs_remove();
int cfs_link();
int cfs_rename();
int cfs_mkdir();
int cfs_rmdir();
int cfs_readdir();
int cfs_symlink();
int cfs_readlink();
int cfs_fsync();
int cfs_inactive();
int cfs_bmap();
int cfs_strategy();
int cfs_badop();
int 	cfs_lockctl();
int	cfs_realvp();
int 	cfs_fid();
int 	cfs_noop();
int	cfs_cmp();

#if	MACH
pager_return_t cfs_pagein();
pager_return_t cfs_pageout();
int cfs_nlinks();
#endif	MACH

struct vnodeops cfs_vnodeops = {
	cfs_open,
	cfs_close,
	cfs_rdwr,
	cfs_ioctl,
	cfs_select,
	cfs_getattr,
	cfs_setattr,
	cfs_access,
	cfs_lookup,
	cfs_create,
	cfs_remove,
	cfs_link,
	cfs_rename,
	cfs_mkdir,
	cfs_rmdir,
	cfs_readdir,
	cfs_symlink,
	cfs_readlink,
	cfs_fsync,
	cfs_inactive,
	cfs_bmap,
	cfs_strategy,
	cfs_badop,	/* bread */
	cfs_badop,	/* brelse */
	cfs_lockctl,
	cfs_fid,
	cfs_badop,	/* dump */
	cfs_cmp,
	cfs_realvp,
	cfs_pagein,
	cfs_pageout,
	cfs_nlinks,
};

int 	cfs_open(vpp, flag, cred)
	register struct vnode **vpp;
	int flag;
	struct ucred *cred;
{
	/* No special open code is required, because "read" is stateless.
	 */
	return(0);		
}

int	cfs_close(vp, flag, cred)
	struct vnode *vp;
	int flag;
	struct ucred *cred;
{
	/* Likewise, no special close is needed.
	 */
	return(0);		
}

int	cfs_rdwr(vp, uiop, rw, ioflag, cred)
	struct vnode 	*vp;
	struct uio 	*uiop;
	enum uio_rw 	rw;
	int 		ioflag;
	struct ucred 	*cred;
{
	struct	cnode	*cp = vtoc(vp);
	CDFILE		cdf;
	int		error;
	u_long	 	actual;

	dprint3("cfs_rdwr: %x offset %x len %d\n",
	    vp, uiop->uio_offset, uiop->uio_iov->iov_len);

	if (rw == UIO_WRITE) {			/* simplifies things */
		return(EROFS);
	}

	if (vp->v_type != VREG) {		/* Can only read files */
		return (EISDIR);
	}

	if (uiop->uio_resid == 0) {		/* no bytes to read */
		return (0);
	}

	if ((int) uiop->uio_offset < 0 ||	/* illegal address */
	    (int) (uiop->uio_offset + uiop->uio_resid) < 0) {
		return (EINVAL);
	}

	error	= cdf_open(ctoext(cp),&cdf,uiop->uio_offset,CDF_UIO );
	if(error){
		return(error);
	}

	return(cdf_read(&cdf,uiop->uio_resid,uiop,&actual));
}

int	cfs_ioctl(vp, com, data, flag, cred)
struct 	vnode *vp;
int 	com;
caddr_t data;
int 	flag;
	struct ucred *cred;
{

	return (EOPNOTSUPP);	/* We don't support IOCTLS */
}


int	cfs_select(vp, which, cred)
struct 	vnode *vp;
int 	which;
struct 	ucred *cred;
{

	return (EOPNOTSUPP);
}

/* Get attributes for a CDROM VNODE.
 * Note that most of these credentials have no meaning in the ISO 9660
 * standard.
 * We fake them as best as possible.
 */
int	cfs_getattr(struct vnode *vp, struct vattr *vap, struct ucred *cred)
{
	struct cnode *cp;

	dprint1("cfs_getattr(vp=%x)\n",vp);

	cp	= vtoc(vp);				

	vap->va_type		= cp->c_ext.e_type; 
	vap->va_mode		= cp->c_ext.e_mode;

	/*
	 * owner is caller for autodiskmount...
	 */
	vap->va_uid		= cred->cr_uid;
	vap->va_gid		= cred->cr_gid;
	vap->va_fsid		= 1;		/* NFS does it wrong also. */
	vap->va_nodeid		= cp->c_ext.e_start;	
	vap->va_nlink		= cp->c_ext.e_nlink;
	vap->va_size		= cp->c_ext.e_bytes;
	vap->va_blocksize	= CDROM_BLOCK_SIZE;	/* by definition */

	vap->va_atime		= cp->c_ext.e_atime; 	/* only have 1 time */
	vap->va_mtime		= cp->c_ext.e_mtime;
	vap->va_ctime		= cp->c_ext.e_ctime;
	vap->va_rdev		= cp->c_ext.e_rdev;
	vap->va_blocks		= (vap->va_size + vap->va_blocksize - 1) /
	  				vap->va_blocksize;

	return(0);	/* if vnode was good, this is good */
}


int
cfs_setattr(vp, vap, cred)
	register struct vnode *vp;
	register struct vattr *vap;
	struct ucred *cred;
{
	return(EROFS);
}

int
cfs_access(vp, mode, cred)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
{
	return(0);	/* all of CDROM is readable */
}


int	cfs_readlink(vp, uiop, cred)
struct 	vnode *vp;
struct 	uio *uiop;
struct 	ucred *cred;
{
#ifdef POSIX_EXTENSIONS
	char	*linkname;
	int	ret;
#endif
	
	dprint1("cfs_readlink(vp=%x)\n",vp);
	
	if(vp->v_type != VLNK){
		dprint("     not a link\n");
		return (ENXIO);
	}
#ifdef POSIX_EXTENSIONS
	linkname	= vtoext(vp)->e_symlink;
	
	ret	= uiomove(linkname,strlen(linkname)+1,UIO_READ,uiop);
	printf("   linkname=%s  ret=%d\n",linkname,ret);
	return(ret);
#else
	printf("cfs:  Link on disk?\n");
	return(EOPNOTSUPP);		/* no links on CDROM */
#endif
}

/*ARGSUSED*/
int	cfs_fsync(vp, cred)
struct 	vnode *vp;
struct 	ucred *cred;
{
	return(0);				/* don't need to sink CDROM */
}

/* Release a vnode we aren't using any more */
/*ARGSUSED*/
int	cfs_inactive(vp, cred)
struct 	vnode *vp;
struct 	ucred *cred;
{
	dprint1("cfs_inactive(vp=%x)\n", vp);

	/* Decrement the number of references to the vnode
	 * on which we are mounted.
	 */
	
	((struct mntinfo *)vp->v_vfsp->vfs_data)->mi_refct--;


	cfs_cfree(vtoc(vp));	/* Free this cnode */

	return (0);
}

/*
 * CDROM file system operations having to do with directory manipulation.
 */

int	cfs_lookup(dvp, nm, vpp, cred)
struct 	vnode *dvp;
char 	*nm;
struct 	vnode **vpp;
struct 	ucred *cred;
{
	int	error = 0;
	struct	cnode	*cp;
	extent	ex;

	dprint2("cfs_lookup(dvp=%x,nm=%s)\n",dvp,nm);

	cp	= vtoc(dvp);
	error	= cdrom_lookup(cp,nm,&ex);
	if(error){
		*vpp	= (struct vnode *)0;
		return(error);
	}

	/* Make a vnode for this entry */
	*vpp	= makecfsnode(&ex,dvp->v_vfsp);

	dprint2("cfs_lookup(%s) returning vp = %x\n", nm,*vpp);
	return(0);
}



int	cfs_create()
{
	return(EROFS);
}

int	cfs_remove()
{
	return(EROFS);
}


int	cfs_link()
{
	return(EROFS);
}


/*ARGSUSED*/
int	cfs_rename()
{
	return(EROFS);
}


/*ARGSUSED*/
int	cfs_mkdir(dvp, nm, va, vpp, cred)
struct 	vnode *dvp;
char 	*nm;
struct	vattr *va;
struct	vnode **vpp;
struct 	ucred *cred;
{
	return(EROFS);
}

/*ARGSUSED*/
int	cfs_rmdir(dvp, nm, cred)
struct 	vnode *dvp;
char 	*nm;
struct 	ucred *cred;
{
	return(EROFS);
}

/*ARGSUSED*/
int	cfs_symlink(dvp, lnm, tva, tnm, cred)
struct 	vnode *dvp;
char 	*lnm;
struct 	vattr *tva;
char 	*tnm;
struct 	ucred *cred;
{
	return(EROFS);
}

/*
 * Read directory entries.
 * There are some weird things to look out for here.  The uio_offset
 * field is either 0 or it is the offset returned from a previous
 * readdir.  It is an opaque value used by the server to find the
 * correct directory block to read.  The byte count must be at least
 * vtoblksz(vp) bytes.  The count field is the number of blocks to
 * read on the server.  This is advisory only, the server may return
 * only one block's worth of entries.  Entries may be compressed on
 * the server.
 */
/*ARGSUSED*/
int	cfs_readdir(vp, uiop, cred)
struct 	vnode *vp;
struct 	uio *uiop;
struct 	ucred *cred;
{
	int 		error = 0;
	struct iovec 	*iovp;
	u_long	 	bytes_wanted;		/* bytes wanted */
	u_long		bytes_read;		/* bytes read */
	struct cnode 	*cp;
	char		*buf;
	int		eof;
	off_t		offset;

	cp 		= vtoc(vp);
	if ((cp->c_flags & REOD)
	    && (cp->endofdir == (u_long)uiop->uio_offset)) {
		return (0);
	}
	iovp 		= uiop->uio_iov;
	bytes_wanted 	= iovp->iov_len;
	offset		= uiop->uio_offset;

	dprint3("cfs_readdir(vp=%x) bytes_wanted %d offset %d\n",
	    vp, bytes_wanted, offset);

	/*
	 * XXX We should do some kind of test for bytes_wanted >= DEV_BSIZE
	 */
	if (uiop->uio_iovcnt != 1) {
		return (EINVAL);
	}
	buf	= kalloc(bytes_wanted);		/* get buffer for user data */
	bzero(buf,bytes_wanted);		/* clear it */
	
	bytes_read	= bytes_wanted;

	eof = cdrom_readdir(cp,&offset,&bytes_read,buf);

	dprint3("   eof=%d bytes read=%d  newoffset=%d\n",
		eof,bytes_read,&offset);

	if(eof>0){				/* some sort of error */
		kfree(buf,bytes_wanted);	/* free the space */
		return(error);
	}

	/* Move the entries to user land */
	error	= uiomove((caddr_t)buf,bytes_read,UIO_READ,uiop);
	uiop->uio_offset	= offset;	/* put it back */
	if(eof){
		/* We reached the end of the directory, so we should
		 * remember how long it is so that we don't read past it.
		 */
		
		cp->c_flags	|= REOD;
		cp->endofdir	=  uiop->uio_offset;

		/* set size of directory */
	}
	kfree(buf,bytes_wanted);

	dprint3("cfs_readdir: returning %d resid %d, offset %d\n",
	    error, uiop->uio_resid, uiop->uio_offset);

	return (error);
}

/*
 * Convert from file system blocks to device blocks
 */
int	cfs_bmap(vp, bn, vpp, bnp)
struct 	vnode *vp;	/* file's vnode */
daddr_t bn;		/* fs block number */
struct 	vnode **vpp;	/* RETURN vp of device */
daddr_t *bnp;		/* RETURN device block number */
{
	int bsize;		/* server's block size in bytes */

	dprint2("cfs_bmap %x blk %d\n", vp, bn);

	if (vpp){
		*vpp 	= vp;
	}
	if (bnp) {
		bsize 	= CDROM_BLOCK_SIZE;
		*bnp 	= bn * (bsize / DEV_BSIZE);
	}
	return (0);
}

struct buf *async_bufhead;

int	cfs_strategy(bp)
struct 	buf *bp;
{
	return(0);
}


int 	cfs_badop()
{
	panic("cfs_badop");
	return(0);
}

int	cfs_noop()
{
	return(EREMOTE);
}

int
cfs_lockctl()
{
	return(0);			/* Don't error on lock */
}

/* cfs_pagein()
 *
 * 	This is the main function that MACH uses to
 *	read bytes from a file.  MACH actually takes
 *	page faults on all the blocks to read.  cfs_pagein()
 *	is called by the MACH pager to copy the bytes from
 * 	the device into a block of memory pointed to by
 *	"m".
 */

pager_return_t
cfs_pagein(vp, m, f_offset)
	struct vnode	*vp;
	vm_page_t	m;
	vm_offset_t	f_offset; 	/* byte offset within file block */
{
	struct cnode 	*cp = vtoc(vp);		/* get cnode  */
	CDFILE		cdf;			/* CDROM stream  */
	int		csize	= PAGE_SIZE;	/* bytes to read */
	long		bytes_at_end;		/* bytes in file after read */
	u_long		actual;			/* actual # of bytes read  */


	bytes_at_end	= cp->c_ext.e_bytes - (f_offset + csize); 

	/* Zero the page if we are not going to fill it completely with the 
	 * contents of the file.
	 */

	if (bytes_at_end < 0){
		vm_page_zero_fill(m);
		csize	+= bytes_at_end;	/* only read this much then */
	}

	if(cdf_open(ctoext(cp),&cdf,f_offset,CDF_PAGER)){
		return(PAGER_ERROR);
	}

	if(cdf_read(&cdf,csize,m,&actual)){
		return(PAGER_ERROR);
	}

	return(PAGER_SUCCESS);
}



pager_return_t
cfs_pageout(vp, addr, csize, f_offset)
	struct vnode	*vp;
	vm_offset_t	addr;
	vm_size_t	csize;
	vm_offset_t	f_offset;	/* byte offset within file block */
{
	return(PAGER_ERROR);		/* can't write to CDROM */
}

/* Every CDROM file has exactly one link.
 * Nevertheless, we'll be ethical and use the cfs_getattr() function.
 */
int 	cfs_nlinks(vp, l)
	struct vnode	*vp;
	int 		*l;
{
	struct vattr 	vattr;
	int 		error;

	error = VOP_GETATTR(vp, &vattr, u.u_cred); /* u.u_cred XXX */
	if (error) {
		return (error);
	}
	*l = vattr.va_nlink;
	return (0);
}

int	cfs_fid(struct vnode *vp, struct fid **fidpp)
{
	return(0);
}


int	cfs_cmp(struct vnode *vp1, struct vnode *vp2)
{
	dprint2("cfs_cmp  <%x> <%x>\n", vp1,vp2);
	return (vp1 == vp2);
}

int	cfs_realvp(struct vnode *vp, struct vnode **vpp)
{
	dprint("cfs_realvp\n");
	return (EINVAL);		/* ? */
}

