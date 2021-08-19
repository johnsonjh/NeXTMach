/*	@(#)dos_vnodeops.c	2.0	25/06/90	(c) 1990 NeXT	*/

/* 
 * dos_vnodeops.c -- vnode layer for loadable DOS file system
 *
 * HISTORY
 * 25-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <nextdev/ldd.h>
#import <sys/param.h>
#import <sys/buf.h>
#import <sys/vnode.h>
#import <sys/uio.h>
#import <sys/ucred.h>
#import <sys/pathname.h>
#import <sys/vfs.h>
#import <sys/stat.h>
#import <sys/file.h>
#import <sys/dnlc.h>
#ifdef	MACH_ASSERT
#undef	MACH_ASSERT
#endif	MACH_ASSERT
#define MACH_ASSERT 1
#import <kern/assert.h>
#ifdef	NeXT
#import <sys/dir.h>
#else	NeXT
#import <sys/dirent.h>
#endif	NeXT
#import <kern/mfs.h>
#import <next/vm_types.h>
#import <vm/vm_pager.h>
#import <vm/vm_page.h>
#import <sys/errno.h>
#import <header/posix.h>
#import <nextdos/msdos.h>
#import <nextdos/next_proto.h>
#import <nextdos/dosdbg.h>

#define TRANSLATE_ALWAYS	1	/* filename translation on lookup, 
					 * mv, etc. */
					 
extern void strcpy(void *s1, void *s2);
extern int strlen(void *s1);
extern int strcmp(void *s1, void *s2);
extern int strncmp(void *s1, void *s2, int cnt);
extern int uiomove(caddr_t cp, int n, enum uio_rw rw, struct uio *uio);
extern void dnlc_remove(struct vnode *dp, char *name);
extern void microtime(struct timeval *tvp);

/*
 * Private functions accessed via dos_vnodeops
 */
static int dos_open(struct vnode **vpp, int flag, struct ucred *cred);
static int dos_close(struct vnode *vp, int flag, int count, 
	struct ucred *cred);
extern int dos_rdwr(struct vnode *vp, struct uio *uiop, enum uio_rw rw, 
	int ioflag, struct ucred *cred);
static int dos_ioctl(struct vnode *vp, int com, caddr_t data, int flag, 
	struct ucred *cred);
static int dos_select(struct vnode *vp, int which, struct ucred *cred);
static int dos_getattr(struct vnode *vp, struct vattr *vap,
	struct ucred *cred);
static int dos_setattr( struct vnode *vp, struct vattr *vap,
	struct ucred *cred);
static int dos_access(struct vnode *vp, int mode, struct ucred *cred);
static int dos_readlink(struct vnode *vp, struct uio *uiop, 
	struct ucred *cred);
static int dos_fsync(struct vnode *vp, struct ucred *cred);
static int dos_inactive(struct vnode *vp, struct ucred *cred);
static int dos_lookup(struct vnode *dvp, char *nm, struct vnode **vpp,
	struct ucred *cred, struct pathname *pnp, int flags);
static int dos_create(struct vnode *dvp, char *nm, struct vattr *vap,
	enum vcexcl exclusive, int mode, struct vnode **vpp,
	struct ucred *cred);
static int dos_remove(struct vnode *vp, char *nm, struct ucred *cred);
static int dos_link(struct vnode *vp, struct vnode *tdvp, char *tnm,
	struct ucred *cred);
static int dos_rename(struct vnode *sdvp, char *snm, struct vnode *tdvp,	
	char *tnm, struct ucred *cred);
static int dos_mkdir(struct vnode *dvp, char *nm, struct vattr *vap,
	struct vnode **vpp, struct ucred *cred);
static int dos_rmdir(struct vnode *vp, char *nm, struct ucred *cred);
static int dos_readdir(struct vnode *vp, struct uio *uiop, struct ucred *cred);
static int dos_symlink(struct vnode *dvp, char *lnm, struct vattr *vap,
	char *tnm, struct ucred *cred);
static int dos_bmap(struct vnode *vp,daddr_t lbn, struct vnode **vpp,
	daddr_t *bnp);
static int dos_bread(struct vnode *vp, daddr_t lbn, struct buf **bpp,
	long *sizep);
static int dos_brelse(struct vnode *vp,	struct buf *bp);
static int dos_cmp(struct vnode *vp1, struct vnode *vp2);
static int dos_realvp(struct vnode *vp, struct vnode **vpp);
static int dos_badop();
static int dos_lockctl();
static int dos_fid(struct vnode *vp, struct fid **fidpp);
static int dos_strategy(struct buf *bp);
extern pager_return_t dos_pagein(struct vnode *vp, vm_page_t m, 
	vm_offset_t f_offset);
extern pager_return_t dos_pageout(struct vnode *vp,vm_offset_t addr, 
	vm_size_t csize, vm_offset_t f_offset);
static int dos_nlinks(struct vnode *vp,	int *l);
	
struct vnodeops dos_vnodeops = {
	(PFI)dos_open,
	(PFI)dos_close,
	(PFI)dos_rdwr,
	(PFI)dos_ioctl,
	(PFI)dos_select,
	(PFI)dos_getattr,
	(PFI)dos_setattr,
	(PFI)dos_access,
	(PFI)dos_lookup,
	(PFI)dos_create,
	(PFI)dos_remove,
	(PFI)dos_link,
	(PFI)dos_rename,
	(PFI)dos_mkdir,
	(PFI)dos_rmdir,
	(PFI)dos_readdir,
	(PFI)dos_symlink,
	(PFI)dos_readlink,
	(PFI)dos_fsync,
	(PFI)dos_inactive,
	(PFI)dos_bmap,
	(PFI)dos_strategy,
	(PFI)dos_bread,
	(PFI)dos_brelse,
	(PFI)dos_lockctl,
	(PFI)dos_fid,
	(PFI)dos_badop,		/* dump */
	(PFI)dos_cmp,
	(PFI)dos_realvp,
	(PFI)dos_pagein,
	(PFI)dos_pageout,
	(PFI)dos_nlinks,
};

static int
dos_open(struct vnode **vpp, 			/* ptr**2 to vnode to open */
	int flag, 				/* FWRITE, FREAD */
	struct ucred *cred)
{
	/*
	 * We only have to do an actual DOS open call if we haven't obtained
	 * a file descriptor for this msdnode yet. This happens when we
	 * create an msdnode in dos_lookup(). Note that there is no "opendir"
	 * call in EBS DOS.
	 */
	msdnode_t mnp;
	UCOUNT flags=0;				/* arg to po_open() */
	int rtn;
	PCFD fd;
	char filename[EMAXPATH];
	
	mnp = VTOM(*vpp);
	dbg_vop(("dos_open: msdnode 0x%x path %s: ", mnp, mnp->m_path));
	if(mnp->m_fd >= 0) {
		dbg_vop(("already open\n"));
		return(0);
	}
	if(mnp->m_vnode.v_flag & VROOT) {
		dbg_vop(("ROOT\n"));
		return(0);
	}
	dbg_vop(("new open\n"));
	if(flag & FREAD) {
		if(flag & FWRITE) {
			flags = PO_RDWR;
		}
		else {
			flags = PO_RDONLY;
		}
	}
	else if(flag & FWRITE) {
		flags = PO_WRONLY;
	}
	else {
		printf("dos_open: No Read or Write access\n");
		return(EINVAL);
	}
	if(pc_isdir(mnp->m_path) == NO) {
		/*
		 * No open for directory.
		 */
		fd = dos_do_open(mnp->m_path, flags, 0);  /* mode is don't care
							   */
		if(fd < 0) {
			dbg_err(("dos_open: po_open returned %d\n", 
				msd_errno()));
			return(msd_errno());		
		}
		/*
		 * Success.
		 */
		mnp->m_fd = fd;
	}
	return(0);
}

static int
dos_close(struct vnode *vp, 			/* vnode to close */
	int flag, 				/* ? */
	int count, 				/* ? */
	struct ucred *cred)
{
	msdnode_t mnp;
	
	mnp = VTOM(vp);
	dbg_vop(("dos_close: msdnode 0x%x path %s\n", mnp, mnp->m_path));
	/*
	 * Keep this node around (and its DOS file open) until vnode is 
	 * inactivated.
	 */
	return(0);
}

static int
dos_ioctl(struct vnode *vp, 
	int com, 
	caddr_t data, 
	int flag, 
	struct ucred *cred)
{	
	msdnode_t mnp;
	
	mnp = VTOM(vp);
	dbg_vop(("dos_ioctl: msdnode 0x%x path %s\n", mnp, mnp->m_path));
	return(EINVAL);
}

static int
dos_select(struct vnode *vp, 
	int which, 
	struct ucred *cred)
{
	msdnode_t mnp;
	
	mnp = VTOM(vp);
	dbg_vop(("dos_select: msdnode 0x%x path %s\n", mnp, mnp->m_path));
	return(EINVAL);
}

int fake_root=1;
					
static int
dos_getattr(struct vnode *vp,
	struct vattr *vap,			/* attributes RETURNED here */
	struct ucred *cred)
{
	msdnode_t mnp, pmnp;
	DSTAT *dstat_p;
	int rtn=0;
	char pathname[EMAXPATH];
	char *name;
	ddi_t ddip;
	DDRIVE *ddp;
	
	mnp = VTOM(vp);
	mn_lock(mnp);
	dbg_vop(("dos_getattr: msdnode 0x%x path %s\n", mnp, mnp->m_path));
	ddip = (ddi_t)(vp->v_vfsp->vfs_data);
	ddp = msd_getdrive(ddip - dos_drive_info);
	if(fake_root) {
	if(vp->v_flag & VROOT) {
		/*
		 * Special case for stat'ing the root directory of a DOS
		 * disk...Fake it.
		 */
		vap->va_type = VDIR;
		vap->va_mode = S_IFDIR | 
			(S_IEXEC | (S_IEXEC >> 3) | (S_IEXEC >> 6));
		vap->va_mode |= (S_IREAD  | (S_IREAD >> 3)  | (S_IREAD >> 6));
		vap->va_mode |= (S_IWRITE | (S_IWRITE >> 3) | (S_IWRITE >> 6));
		/*
		 * owner, group are owner, group of caller (?)
		 */
		vap->va_uid = cred->cr_uid;
		vap->va_gid = cred->cr_gid;
		vap->va_fsid = ddip->dev;   	/* dev for ufs  */
		vap->va_nodeid = ROOT_CLUSTER;
		vap->va_nlink = 1;	/* number of references to file */
		/* vap->va_size = ddp->secproot * DOS_SECTOR_SIZE; */
		vap->va_size = 0;
		vap->va_blocksize = ddip->bytes_per_cluster;	
		/*
		 * DOS only has one time for the file; we'll return that as
		 * creation, access, and modify times.
		 */
		vap->va_mtime = vap->va_ctime = vap->va_atime = 
			mnp->m_msddp->mtime;
		vap->va_rdev = -1;	/* device the file represents */
		vap->va_blocks = howmany(vap->va_size, STAT_BSIZE);
		rtn = 0;
		goto out;
	} /* faking root */
	
	pmnp = mnp->m_parent;
	strcpy(pathname, mnp->m_path);
	} else /* !fake_root */ {
	if(vp->v_flag & VROOT) {
		/*
		 * Root - lookup something like "A:".
		 */
		pmnp = mnp;
		name = "";
		msd_genpath(pmnp, name, pathname, FALSE);
	}
	else {
		pmnp = mnp->m_parent;
		strcpy(pathname, mnp->m_path);
	}	
	} /* !fake_root */
	mn_lock(pmnp);
	dstat_p = &pmnp->m_msddp->dstat;
	rtn = msd_lookup(pmnp, pathname);
	mn_unlock(pmnp);
	if(rtn) {
		dbg_vop(("dos_getattr: msd_lookup returned %d\n", rtn));
		goto out;
	}
	/*
	 * Always readable by everyone; DOS "read only" bit determines
	 * writability by everyone. DOS directories are executable by 
	 * everyone.
	 */
	if(dstat_p->fattribute & ADIRENT) {
		vap->va_type = VDIR;
		vap->va_mode = S_IFDIR | 
			(S_IEXEC | (S_IEXEC >> 3) | (S_IEXEC >> 6));
	}
	else {
		vap->va_type = VREG;
		vap->va_mode = S_IFREG;
	}
	vap->va_mode |= (S_IREAD | (S_IREAD >> 3) | (S_IREAD >> 6));
	if(!(dstat_p->fattribute & ARDONLY))
		vap->va_mode |= (S_IWRITE | (S_IWRITE >> 3) | (S_IWRITE >> 6));
	/*
	 * owner, group are owner, group of caller (?)
	 */
	vap->va_uid = cred->cr_uid;
	vap->va_gid = cred->cr_gid;
	vap->va_fsid = ddip->dev;	/* dev for ufs */
	vap->va_nodeid = dstat_p->pobj->finode->fcluster;
	/*
	 * File of 0 size needs a non-zero nodeid.
	 */
	if(vap->va_nodeid == 0)
		vap->va_nodeid = ZERO_SIZE_CLUSTER;
	vap->va_nlink = 1;		/* number of references to file */
	/*
	 * MFS maintains this once we create the vnode.
	 */
	vap->va_size = vm_get_vnode_size(vp);
	dbg_vop(("dos_getattr: fsize %d va_size %d\n",
		dstat_p->fsize, vap->va_size));
	vap->va_blocksize = ddip->bytes_per_cluster;
					/* blocksize preferred for i/o */
	/*
	 * DOS only has one time for the file; we'll return that as
	 * creation, access, and modify times. For directories, we use
	 * the mtime stored in the msd_dir struct, since we update that
	 * modifying the directory.
	 */
	msd_time_to_timeval(dstat_p->ftime, dstat_p->fdate, &vap->va_atime);
	if(mnp->m_msddp && mnp->m_msddp->mtime.tv_sec)
		vap->va_mtime = mnp->m_msddp->mtime;	/* modified dir */
	else
		vap->va_mtime = vap->va_atime;		/* all others */
	vap->va_ctime = vap->va_atime;
	vap->va_rdev = -1;		/* device the file represents */
	vap->va_blocks = howmany(dstat_p->fsize, STAT_BSIZE);
	/*
	 * Leave directory in clean state.
	 */
	pc_gdone(&pmnp->m_msddp->dstat);
	pmnp->m_msddp->offset = 0;
out:
	mn_unlock(mnp);
	return(rtn);
} /* dos_getattr() */

/* #define NO_TRUNCATE	1	/* disallow truncate to non-zero size */
 
static int
dos_setattr(struct vnode *vp,
	struct vattr *vap,
	struct ucred *cred)
{
	msdnode_t mnp;
	int rtn = 0;
		
	mnp = VTOM(vp);
	dbg_vop(("dos_setattr: msdnode 0x%x path %s\n", mnp, mnp->m_path));
	/*
	 * The only thing we can do is truncate to 0 length or beyond EOF.
	 */
	if ((vap->va_nlink != -1)        || (vap->va_blocksize != -1)     ||
	    (vap->va_rdev != -1)         || (vap->va_blocks != -1)        ||
	    (vap->va_fsid != -1)         || (vap->va_nodeid != -1)        ||
	    ((int)vap->va_type != -1)    || 
	    (vap->va_uid != -1)          || (vap->va_gid != -1)) {
	    	dbg_err(("dos_setattr: INVALID OP\n"));
		return (EINVAL);
	}
	if(vap->va_mode != (u_short)-1) {
		dbg_vop(("dos_setattr: attamepting to change mode "
			"(ignored)\n"));
		return(0);
	}
	if((vap->va_atime.tv_sec != -1) || (vap->va_mtime.tv_sec != -1) ||
	    (vap->va_ctime.tv_sec != -1)) {
	 	dbg_vop(("dos_setattr: attempting to change time"
			" (ignored)\n"));   
		return(0);
	}
	if(vap->va_size != -1) {
		PC_FILE *pfile;
		int fsize;
		int new_fp;
		
		pfile = pc_fd2file(mnp->m_fd, YES);
		if(pfile == NULL) {
			dbg_err(("dos_setattr: COULD NOT GET FILE POINTER\n"));
			return(-1);
		}
		fsize = pfile->pobj->finode->fsize;
#ifdef	NO_TRUNCATE
		if((vap->va_size > 0) && (vap->va_size < fsize)) {
			dbg_err(("dos_setattr: NONZERO TRUNCATE\n"));
			return(EINVAL);
		}
#endif	NO_TRUNCATE
		mfs_trunc(vp, vap->va_size);
		/*
		 * mfs_trunc can change finode->fsize...
		 */
		fsize = pfile->pobj->finode->fsize;
		if(vap->va_size == fsize) {
			dbg_vop(("dos_setattr: TRIVIAL TRUNCATE\n"));
			return(0);
		}
		mn_lock(mnp);
		dbg_vop(("dos_setattr: TRUNCATING from %d to %d\n", 
			mnp->m_fp, vap->va_size));
		if(vap->va_size == 0) {
		
			UCOUNT po_flags;
			BOOL ertn;
			
			/*
			 * Truncate to 0 by closing and re-creating. We have to
			 * stat the file before we close it to get the current
			 * "read-only" state.
			 */
			if(pfile->pobj->finode->fattribute & ARDONLY)
				po_flags = PO_BINARY | PO_RDONLY | 
					   PO_CREAT | PO_TRUNC;
			else 
				po_flags = PO_BINARY | PO_RDWR | 
					   PO_CREAT | PO_TRUNC;
			po_close(mnp->m_fd);
			mnp->m_fd = dos_do_open(mnp->m_path, po_flags, 
				PS_IWRITE | PS_IREAD);
			if(mnp->m_fd < 0) {
				dbg_err(("dos_setattr: COULD NOT REOPEN FILE "
					"%s\n",	mnp->m_path));
				mn_unlock(mnp);
				return(EIO);
			}
			new_fp = 0;
		}
		else if(vap->va_size > fsize) {
			/*
			 * Extend by writing 0's from eof to desired offset.
			 */
			new_fp = msd_extend(mnp, vap->va_size);
			if(new_fp != vap->va_size) 
				rtn = EIO;
		} 
		else {
			/*
			 * For saftey's sake we'll only use seteof if 
			 * vap->va_size < fsize. However, after 2.0, when we 
			 * have time to test more carefully we should probably 
			 * use po_seteof when vap->va_size > fsize as well. 
			 * -- DJA
			 */
			if (po_seteof(mnp->m_fd, vap->va_size) != -1)
				new_fp = vap->va_size;
			else	
				rtn = EIO;
		}
		if(rtn == 0)
			mnp->m_fp = new_fp;
		mn_unlock(mnp);
	}
	return(rtn);
}

static int
dos_access(struct vnode *vp,
	int mode,				/* check access permissions for
						 * this mode */
	struct ucred *cred)
{
	DSTAT *dstat_p;
	msdnode_t mnp, pmnp;
	int rtn = 0;
	
	mnp = VTOM(vp);
	dbg_vop(("dos_access: msdnode 0x%x path %s\n", mnp, mnp->m_path));
	mn_lock(mnp);
	if(vp->v_flag & VROOT) {
		/*
		 * Special case for root directory of a DOS
		 * disk...always r/w by everyone.
		 */
		goto out;
	}
	/*
	 * DOS files are always readable...
	 */
	if((mode & S_IWRITE) == 0)
		goto out;
	pmnp = mnp->m_parent;
	mn_lock(pmnp);
	dstat_p = &pmnp->m_msddp->dstat;
	rtn = msd_lookup(pmnp, mnp->m_path);
	mn_unlock(pmnp);
	if(rtn)
		goto out;
	if(dstat_p->fattribute & ARDONLY)
		rtn = EACCES;
	/*
	 * Leave directory in clean state.
	 */
	pc_gdone(&pmnp->m_msddp->dstat);
	pmnp->m_msddp->offset = 0;
out:
	mn_unlock(mnp);
	return(rtn);
}

static int
dos_readlink(struct vnode *vp,
	struct uio *uiop,
	struct ucred *cred)
{
	dbg_vop(("dos_readlink\n"));
	return(EINVAL);				/* no links on DOS disks */
}

static int
dos_fsync(struct vnode *vp,
	struct ucred *cred)
{
	dbg_vop(("dos_fsync\n"));
	return(0);				/* this call unnecessary */
}

/* 
 * Release a vnode (and its msdnode) we aren't using any more. Called by VFS
 * when vnode's v_count decrements to 0.
 */
static int
dos_inactive(struct vnode *vp,
	struct ucred *cred)
{
	msdnode_t mnp = VTOM(vp);
	
	dbg_vop(("dos_inactive: msdnode 0x%x path %s ", mnp, mnp->m_path));
	if(vp->v_flag & VROOT) {
		dbg_err(("( ROOT; ignoring)\n"));
		return(0);
	}
	if(mnp->m_fd >= 0) {
		/*
		 * Regular file.
		 */
		dbg_vop(("(open file)\n"));
		po_close(mnp->m_fd);
		mnp->m_fd = -1;
	}
	else {
		if(mnp->m_msddp) {
			/*
			 * Special case for directories - we need to keep these
			 * around so we can report accurate modify time.
			 */
			dbg_vop(("(directory; retaining)\n"));
			mfs_uncache(vp);
			return(0);
		}
		else			
			dbg_vop(("(unopened file)\n"));
	}
	msdnode_free(mnp);
	return(0);
}

/*
 * Lookup *nm in directory *dvp, return it in *vpp. **vpp and dvp are held on 
 * exit. We create an msdnode for the file, but we do NOT open the file here.
 */
static int
dos_lookup(struct vnode *pvp,		/* parent vnode */
	char *nm,			/* file to look for */
	struct vnode **vpp,		/* vnode of found file (RETURNED) */
	struct ucred *cred,
	struct pathname *pnp,		/* ? */
	int flags)
{
	msdnode_t pmnp;			/* parent */
	msdnode_t tmnp;			/* target */
	struct msd_dir *msddp;
	BOOL ertn;
	char pathname[EMAXPATH];
	int rtn;
	boolean_t is_dir = FALSE;
	UCOUNT fcluster;
	FINODE *finodep;
	vm_size_t vnode_size;
	
	pmnp = VTOM(pvp);
	if(pmnp->m_msddp == NULL)
		return(EINVAL);
	dbg_vop(("dos_lookup: parent 0x%x (%s) nm %s\n",
		pmnp, pmnp->m_path, nm));
	/*
	 * Trivial cases for current and parent directories
	 */
	if(nm[0] == '.') {
		if(nm[1] == '\0') {			/* self */
			*vpp = pvp;
			VN_HOLD(pvp);
			return(0);
		}
		if((nm[1] == '.') && (nm[2] == '\0')) {	/* parent */
			if(pvp->v_flag & VROOT) {
				/*
				 * Huh? This shouldn't happen. Return self.
				 */
				printf("dos_lookup: request for root's "
					"parent\n");
				*vpp = pvp;
			}
			else
				*vpp = MTOV(pmnp->m_parent);
			VN_HOLD(*vpp);
			/*
			 * FIXME - should we hold a parent vnode here? 
			 */
			return(0);
		}
	}
	msd_genpath(pmnp, nm, pathname, TRANSLATE_ALWAYS);	
						/* get full pathname */
	mn_lock(pmnp);
	msddp = pmnp->m_msddp;
	ASSERT(msddp != NULL);
	tmnp = msdnode_search(pathname);	/* existing reference to 
						 * this file? */
	if(tmnp) { 
		*vpp = MTOV(tmnp);
		mn_unlock(pmnp);
		return(0);
	}
	/*
	 * We don't know about this file. See if it exists. If so, make a
	 * vnode for it, but don't do an open...
	 */ 
	rtn = msd_lookup(pmnp, pathname);
	if(rtn) {
		dbg_vop(("dos_lookup: msd_lookup returned %d\n", rtn));
		mn_unlock(pmnp);
		/*
		 * EBS returns PEINVAL on "not found" error. 
		 */
		return(ENOENT);
	}
	finodep = pmnp->m_msddp->dstat.pobj->finode;
	fcluster = finodep->fcluster;
	vnode_size = (vm_size_t)finodep->fsize;
	if(pc_isdir(pathname) == YES)
		is_dir = TRUE;
	tmnp = msdnode_new(pmnp, fcluster, vnode_size, is_dir);
	/*
	 * Leave directory in clean state.
	 */
	pc_gdone(&pmnp->m_msddp->dstat);
	pmnp->m_msddp->offset = 0;
	if(tmnp == NULL) {
		printf("dos_lookup: NO KERNEL SPACE FOR MSDNODE\n");
		mn_unlock(pmnp);
		return(ENOSPC);
	}
	strcpy(tmnp->m_path, pathname);
	*vpp = MTOV(tmnp);
	mn_unlock(tmnp);
	mn_unlock(pmnp);
	return(0);
}

static int
dos_create(struct vnode *pvp,		/* parent directory's vnode */
	char *nm,			/* name of new file */
	struct vattr *vap,		/* VREAD, VWRITE, etc. RETURNED as
					 * well. */
	enum vcexcl exclusive,
	int mode,			/* for access checking in case file
					 * exists */
	struct vnode **vpp,		/* vnode of new file (RETURNED) */
	struct ucred *cred)		/* cred of caller */
{
	msdnode_t pmnp;
	msdnode_t tmnp = NULL;
	char pathname[EMAXPATH];
	int rtn;
	UCOUNT po_flags;		/* args to po_open() */
	UCOUNT po_mode = 0;
	PCFD fd;
	UCOUNT fcluster;
	vm_size_t vnode_size;
	FINODE *finodep;
	
	pmnp = VTOM(pvp);
	dbg_vop(("dos_create parent 0x%x (%s) nm %s\n",
		pmnp, pmnp->m_path, nm));
	dbg_vop(("    mode 0x%x  va_type %d va_mode 0x%x va_size %d\n",
		mode, vap->va_type, vap->va_mode, vap->va_size));
	ASSERT(pmnp->m_msddp);
	if(vap == NULL) 
		printf("dos_create: NULL vattr\n");	/* ??? */
	switch(vap->va_type) {
	    case VDIR:
		return (EISDIR);		/* use dos_mkdir instead */
	    case VREG:
	    	break;
	    default:
	    	dbg_err(("dos_create: invalid va_type (%d)\n", vap->va_type));
	    	return(EINVAL);
	}
	if(vap->va_mode & (VSUID | VSGID | VSVTX)) {
	    	dbg_err(("dos_create: invalid va_mode (%o)\n", vap->va_mode));
		return(EINVAL);			/* DOS can't do these */
	}
	mn_lock(pmnp);
	/*
	 * Convert to a DOS-compatible filename and generate a full
	 * path.
	 */
	if(rtn = msd_genpath(pmnp, nm, pathname, TRUE)) {
		mn_unlock(pmnp);
		return(rtn);			/* path too long */
	}
	tmnp = msdnode_search(pathname);
	if(tmnp) {
		/* 
		 * we already have an msdnode for this. Close the file if
		 * it's open.
		 */
		mn_lock(tmnp);
		if(tmnp->m_fd >= 0) {
			po_close(tmnp->m_fd);
			tmnp->m_fd = -1;
		}
	}

	/*
	 * generate file mode (written to disk). Executable bit is don't care.
	 */
	if(vap->va_mode & VWRITE) 
		po_mode |= PS_IWRITE;
	if(vap->va_mode & VREAD) 
		po_mode |= PS_IREAD;
	/*
	 * Generate access flags.
	 */
	if(mode & VWRITE)
		po_flags = PO_BINARY | PO_RDWR | PO_CREAT;
	else 
		po_flags = PO_BINARY | PO_RDONLY | PO_CREAT;
	if(exclusive == EXCL)
		po_flags |= PO_EXCL;
	if(vap->va_size == 0)			/* truncate */
		po_flags |= PO_TRUNC;
	dbg_vop(("dos_create: pathname <%s> flags 0x%x mode 0x%x\n",
		pathname, po_flags, po_mode));
	fd = dos_do_open(pathname, po_flags, po_mode);
	if(fd < 0) {
		dbg_err(("po_open returned %d\n", msd_errno));
		mn_unlock(pmnp);
		if(tmnp)
			mn_unlock(tmnp);
		return(msd_errno());		
	}
	/*
	 * New entry: generate a new msdnode. We have to stat the file to get
	 * its cluster number and size.
	 */
	if(msd_lookup(pmnp, pathname)) {
		printf("dos_create: COULD NOT STAT NEWLY CREATED FILE\n");
		mn_unlock(pmnp);
		if(tmnp)
			mn_unlock(tmnp);
		return(EIO);
	}
	finodep = pmnp->m_msddp->dstat.pobj->finode;
	fcluster = finodep->fcluster;
	vnode_size = (vm_size_t)finodep->fsize;
	if(tmnp == NULL)
		tmnp = msdnode_new(pmnp, fcluster, vnode_size, FALSE);
	else if(vnode_size == 0) {
		/*
		 * We already had this msdnode. Let mfs know we're changing the
		 * size.
		 */
		mfs_trunc(MTOV(tmnp), vnode_size);	
	}
	/*
	 * Leave directory in clean state.
	 */
	pc_gdone(&pmnp->m_msddp->dstat);
	pmnp->m_msddp->offset = 0;
	if(tmnp == NULL) {
		printf("dos_create: NO KERNEL SPACE FOR MSDNODE\n");
		mn_unlock(pmnp);
		return(ENOSPC);
	}
	strcpy(tmnp->m_path, pathname);
	tmnp->m_parent = pmnp;
	tmnp->m_fd = fd;
	/*
	 * Update the parent directory's modify time.
	 */
	microtime(&pmnp->m_msddp->mtime);
	mn_unlock(tmnp);
	mn_unlock(pmnp);
	*vpp = MTOV(tmnp);
	if (vap != (struct vattr *)0) {
		(void) VOP_GETATTR(*vpp, vap, cred);
	}
	return(0);
}

static int
dos_remove(struct vnode *vp,		/* parent's vnode  */
	char *nm,			/* name of deleted file */
	struct ucred *cred)
{
	msdnode_t pmnp = VTOM(vp);
	msdnode_t tmnp;
	BOOL ertn;
	char pathname[EMAXPATH];
	int rtn;
	
	dbg_vop(("dos_remove: parent 0x%x (%s) nm %s\n", 
		pmnp, pmnp->m_path, nm));
	mn_lock(pmnp);
	if(dos_isdot(nm)) {
		dbg_err(("dos_remove: attempt to remove %s\n", nm));
		mn_unlock(pmnp);
		return(EINVAL);
	}
	if(rtn = msd_genpath(pmnp, nm, pathname, TRANSLATE_ALWAYS)) {
		dbg_vop(("dos_remove: ILLEGAL PATHNAME\n"));
		mn_unlock(pmnp);
		return(rtn);
	}
	if(tmnp = msdnode_search(pathname)) {
		/*
		 * we have an open reference to the file being deleted.
		 */
		ASSERT((MTOV(tmnp))->v_count != 0);
		dbg_vop(("dos_remove: FILE OPEN\n"));
		VN_RELE(MTOV(tmnp));
		mn_unlock(pmnp);
		return(EBUSY);
	}
	dnlc_remove(vp, nm);
	ertn = pc_unlink(pathname);
	/*
	 * Update the parent directory's modify time.
	 */
	microtime(&pmnp->m_msddp->mtime);
	mn_unlock(pmnp);
	if(ertn == YES) 
		return(0);
	else
		return(msd_errno());
}

static int
dos_link(struct vnode *vp,		/* vnode of source file */
	struct vnode *tdvp,		/* target's parent */
	char *tnm,			/* target name */
	struct ucred *cred)
{
	dbg_vop(("dos_link\n"));
	return(EINVAL);
}

/* #define ALLOW_CROSS_MV	1	/* allow move across directories */

static int
dos_rename(struct vnode *sdvp,		/* old (source) parent vnode */
	char *snm,			/* old (source) entry name */
	struct vnode *tdvp,		/* new (target) parent vnode */
	char *tnm,			/* new (target) entry name */
	struct ucred *cred)
{
	msdnode_t smnp, tmnp, moved_mnp;
	char spathname[EMAXPATH], tpathname[EMAXPATH];
	BOOL ertn;
	int rtn;
	
	dbg_vop(("dos_rename\n"));
#ifndef	ALLOW_CROSS_MV
	if(sdvp != tdvp) {
		dbg_err(("dos_rename: PARENT DIRECTORIES DIFFER; REJECTED\n"));
		return(EINVAL);
	}
#endif	ALLOW_CROSS_MV
	smnp = VTOM(sdvp);
	tmnp = VTOM(tdvp);
	mn_lock(smnp);
	if(smnp != tmnp) 		/* may be same directory! */
		mn_lock(tmnp);
	ASSERT((sdvp->v_type == VDIR) && (tdvp->v_type == VDIR));
	/*
	 * Convert both names to DOS_Compatible names. 	 
	 */
	if((rtn = msd_genpath(smnp, snm, spathname, TRANSLATE_ALWAYS)) || 
	   (rtn = msd_genpath(tmnp, tnm, tpathname, TRUE))) {
	   	/* 
		 * pathname too long 
		 */
	   	mn_unlock(smnp);
		if(smnp != tmnp) 
			mn_unlock(tmnp);
	   	return(rtn);
	}
	dnlc_remove(sdvp, snm);
	ertn = pc_mv(spathname, tpathname);
	if(ertn == YES) {
		/*
		 * If we have an active msdnode for this file, rename it too.
		 * lose a reference to old parent; gain one to new parent.
		 */
		moved_mnp = msdnode_search(spathname);
		if(moved_mnp) {
		    struct vnode *moved_vp;
		    
		    if(tmnp != smnp) {
			    VN_HOLD(tdvp);
			    VN_RELE(sdvp);
			    moved_mnp->m_parent = tmnp;
		    }
		    strcpy(moved_mnp->m_path, tpathname);
		    VN_RELE(MTOV(moved_mnp));
		    
		    /*
		     * If this is a directory, we have to rename the m_path 
		     * field for each cached msdnode below this directory.
		     */
		    moved_vp = MTOV(moved_mnp);
		    if(moved_vp->v_type == VDIR) {
		    	int pathlen ;
			msdnode_t rmnp;
			ddi_t ddip;
			queue_head_t *qhp;
			char oldpath[EMAXPATH];
			
			/*
			 * search thru msdnode_valid_list anything starting
			 * with the old pathname...
			 */
			ddip = (ddi_t)(moved_vp->v_vfsp->vfs_data);
			qhp = &ddip->msdnode_valid_list;
			rmnp = (msdnode_t)queue_first(qhp);
			strcpy(oldpath, spathname);
			strcat(oldpath, "/");
			pathlen = strlen(oldpath);
			while(!queue_end(qhp, (queue_entry_t)rmnp)) {
			    
			    if(strncmp(oldpath, rmnp->m_path, pathlen)==0) {
			    
			        char newpath[EMAXPATH];
				char *sp, *dp;
				
				/*
				 * We can't use msd_genpath because this
				 * node might be more than one level below
				 * the directory which got moved. 
				 * Start with the new directory path.
				 */
				strcpy(newpath, tpathname);
				
				/*
				 * Start copying over from the "/" after
				 * the old directory name.
				 */
				dp = &newpath[strlen(newpath)];
				sp = &rmnp->m_path[pathlen-1];
				while(*sp) {
				    *dp++ = *sp++;
				}
				*dp = '\0';
				
				/*
				 * Copy the new name back in to the msdnode.
				 */
				dbg_vop(("dos_rename: changed node from %s",
					rmnp->m_path));
				strcpy(rmnp->m_path, newpath);
				dbg_vop((" to %s\n", rmnp->m_path));
			    }
			    
			    /*
			     * Next msdnode in valid list.
			     */
			    rmnp = (msdnode_t)rmnp->m_link.next;
			}
		    }
		}
	}
	else
		dbg_err(("pc_mv returned %d\n", ertn));
		
	/*
	 * Update both parent directories' modify times.
	 */
	microtime(&smnp->m_msddp->mtime);
	mn_unlock(smnp);
	if(smnp != tmnp) {
		microtime(&tmnp->m_msddp->mtime);
		mn_unlock(tmnp);
	}
	if(ertn == YES) 
		return(0);
	else
		return(msd_errno());
}

static int
dos_mkdir(struct vnode *dvp,		/* parent vnode */
	char *nm,			/* new directory's name */
	struct vattr *vap,		/* attr of new directory */
	struct vnode **vpp,		/* vnode of new directory (RETURNED) */
	struct ucred *cred)
{
	msdnode_t pmnp, tmnp;
	char pathname[EMAXPATH];
	BOOL ertn;
	int rtn;
	UCOUNT fcluster;
	
	pmnp = VTOM(dvp);
	dbg_vop(("dos_mkdir: parent 0x%x (%s)  nm %s\n",
		pmnp, pmnp->m_path, nm));
	ASSERT(pmnp->m_msddp);
	mn_lock(pmnp);
	/*
	 * Generate DOS_Compatible component, the generate full path.
	 */
	if(rtn = msd_genpath(pmnp, nm, pathname, TRUE)) {
		mn_unlock(pmnp);
		return(rtn);
	}
	/*
	 * We don't have to bother checking for an existing node with this 
	 * name; EBS will do that for us.
	 */
	ertn = pc_mkdir(pathname);
	if(ertn == NO) {
#ifdef	DEBUG
		dbg_err(("pc_mkdir(%s) returned %d\n", pathname, ertn));
#endif	DEBUG
		mn_unlock(pmnp);
		return(msd_errno());
	}
	/*
	 * New entry: generate a new msdnode. We have to stat the file to get
	 * its cluster number.
	 */
	if(msd_lookup(pmnp, pathname)) {
		printf("dos_mkdir; COULD NOT STAT NEWLY CREATED FILE\n");
		mn_unlock(pmnp);
		return(EIO);
	}
	fcluster = pmnp->m_msddp->dstat.pobj->finode->fcluster;
	/*
	 * Leave directory in clean state.
	 */
	pc_gdone(&pmnp->m_msddp->dstat);
	pmnp->m_msddp->offset = 0;
	tmnp = msdnode_new(pmnp, fcluster, (vm_size_t)0, TRUE);
	if(tmnp == NULL) {
		printf("dos_mkdir: NO KERNEL SPACE FOR MSDNODE\n");
		mn_unlock(pmnp);
		return(ENOSPC);
	}
	/*
	 * place full pathname of new directory in m_msddp->path.
	 */
	strcpy(tmnp->m_path, pathname);
	/*
	 * Update the parent directory's modify time.
	 */
	microtime(&pmnp->m_msddp->mtime);
	mn_unlock(tmnp);
	mn_unlock(pmnp);
	*vpp = MTOV(tmnp);
	return(0);
}

static int
dos_rmdir(struct vnode *vp,		/* parent vnode */
	char *nm,			/* name to dir to delete */
	struct ucred *cred)
{
	msdnode_t pmnp = VTOM(vp);
	msdnode_t tmnp;
	BOOL ertn;
	char pathname[EMAXPATH];
	int rtn;
	
	dbg_vop(("dos_rmdir: parent 0x%x (%s)  nm %s\n",
		pmnp, pmnp->m_path, nm));
	mn_lock(pmnp);
	if(rtn = msd_genpath(pmnp, nm, pathname, TRANSLATE_ALWAYS)) {
		dbg_vop(("dos_rmdir: ILLEGAL PATHNAME\n"));
		mn_unlock(pmnp);
		return(rtn);
	}
	if(tmnp = msdnode_search(pathname)) {
		VN_RELE(MTOV(tmnp));		/* lose the reference from
						 * msdnode_search() */
		if(MTOV(tmnp)->v_count) {
			/*
			 * we have an open reference to the directory being 
			 * deleted.
			 */
			ASSERT((MTOV(tmnp))->v_count != 0);
			dbg_vop(("dos_rmdir: DIRECTORY OPEN\n"));
			mn_unlock(pmnp);
			return(EBUSY);
		}
		else {
			/*
			 * We've kept this msdnode for stat'ing purposes,
			 * but now it's really time to get rid of it.
			 */
			dnlc_remove(vp, nm);
			msdnode_free(tmnp);
		}
	}
	else
		dnlc_remove(vp, nm);	/* in case it's cached at higher 
					 * levels (should this ever happen?) */
	ertn = pc_rmdir(pathname);
	/*
	 * Update the parent directory's modify time.
	 */
	microtime(&pmnp->m_msddp->mtime);
	mn_unlock(pmnp);
	if(ertn == YES) {
		return(0);
	}
	else  {
		rtn = msd_errno();
		dbg_err(("dos_rmdir: pc_rmdir(%s) returned %d\n",
			pathname, rtn));
		return(rtn);
	}
}

static int
dos_readdir(struct vnode *vp,		/* vnode of dir to read */
	struct uio *uiop,		/* bytes to read, where they go */
	struct ucred *cred)
{
	struct iovec *iovp;		/* for uiomove on return */
	struct dirent *dirent_p;	/* formatted output */
	struct direct *direct_p;
	int bytes_generated = 0;
	int bytes_to_move;
	int rtn;
	BOOL ertn;
	DSTAT *dstatp, *dstat_convert;
	DSTAT dstat_local;
	msdnode_t mnp;
	char search_path[EMAXPATH];
	void *outbuf = NULL;
	int offset;
	
	mnp = VTOM(vp);
	dbg_vop(("dos_readdir: msdnode 0x%x path %s offset %d\n", 
		mnp, mnp->m_path, uiop->uio_offset));

	if(mnp->m_msddp == NULL) {
		dbg_vop(("dos_readdir: m_msddp INVALID\n"));
		return(EINVAL);
	}
	mn_lock(mnp);
	iovp = uiop->uio_iov;
	bytes_to_move = iovp->iov_len;
	/*
	 * The local variable 'offset' and  uiop->uio_offset are used like
	 * this (values are on entry):
	 *
	 * offset	uiop->uio_offset	  dir entry
	 * ------	----------------	-------------
	 *   -2			0		.  (concocted)
	 *   -1			1		.. (concocted)
	 *    0			2		first entry from disk
	 *    1			3		second entry
	 *
	 *	etc. -  entries from disk always count towards incrementing
	 *		the offset values, but "." and ".." are discarded
	 *		when encountered. We always force the first two
	 *		entries to be these; on DOS disks, they can be 
	 *		anywhere.
	 *
	 * On entry, the only reason mnp->m_msddp->offset should be -1 
	 * is if we hit end of directory the last time thru. We return
	 * error immediately in this case if we're trying to read at or 
	 * greater than mnp->m_msddp->last_offset.
	 */
	offset = (int)uiop->uio_offset - 2;
	dstatp = &mnp->m_msddp->dstat;
	if((offset >= 0) && 
	   (mnp->m_msddp->offset < 0) && 
	   (offset >= mnp->m_msddp->last_offset)) {
	   	dbg_vop(("dos_readdir: EOD (no I/O)\n"));
	   	goto out;		/* nothing to read; done */
	}
		
	
	/*
	 * Get space to change directory entries into fs independent format.
	 * Leave space for one extra dirent.
	 */
#ifdef	NeXT
	direct_p = outbuf = (struct direct *)kalloc(bytes_to_move + 
						sizeof(struct direct));
#else	NeXT
	dirent_p = outbuf = (struct dirent *)kalloc(bytes_to_move + 
						sizeof(struct dirent));
#endif	NeXT
	/*
	 * Generate a wildcard search path for this directory.
	 */
	if(rtn = msd_genpath(mnp, "*.*", search_path, FALSE)) 
		goto out;

	/*
	 * 'lseek' to desired offset if necessary.
	 */
	if((offset < 0) && mnp->m_msddp->offset) {
		/*
		 * VFS wants to start at the beginning, and we're not there.
		 * Offset -2 and -1 ("." and "..") are concocted by us without
		 * any EBS I/O.
		 */
		pc_gdone(dstatp);
		mnp->m_msddp->offset = 0;
	}
	else if((offset > 0) && (mnp->m_msddp->offset != offset)) {
		/*
		 * VFS wants a real entry, other than the one we're at.
		 */
		if(mnp->m_msddp->offset > offset) {
			/*
			 * go back to beginning of directory before starting
			 * the search.
			 */
			pc_gdone(dstatp);
			mnp->m_msddp->offset = 0;
		}
		/*
		 * Scan thru directory entries until we get to the
		 * offset we want. These entries are thrown on the
		 * floor.
		 */
		while(mnp->m_msddp->offset < offset) {
			if(mnp->m_msddp->offset == 0)
				ertn = pc_gfirst(dstatp, search_path);
			else
				ertn = pc_gnext(dstatp);
			if(ertn == NO) {
			    	dbg_vop(("dos_readdir: LSEEK FAILED\n"));
				mnp->m_msddp->last_offset = 
					mnp->m_msddp->offset;
				mnp->m_msddp->offset = -1;
			    	goto done;
			}
			else {
				dbg_vop(("dos_readdir: <%s> <%s> DISCARDED\n", 		
					dstatp->fname, dstatp->fext));
				mnp->m_msddp->offset++;
			}
		}
	} /* lseek required */
	
	while(bytes_generated < bytes_to_move) {
		/* 
		 * loop until end of dir or all bytes xferred 
		 */
		 
		switch(offset) {
		    case -2:					/* "." */
			strcpy(dstat_local.fname, ".       ");
			goto concoct;
			
		    case -1:					/* ".." */
		    	strcpy(dstat_local.fname, "..      ");
concoct:
			strcpy(dstat_local.fext, "   ");
		    	ertn = YES;
			dstat_convert = &dstat_local;
			break;
			
		    case  0:
			ertn = pc_gfirst(dstatp, search_path);
			goto getdir;

		    default:
		    	ertn = pc_gnext(dstatp);
getdir:
			if(ertn == YES)
				mnp->m_msddp->offset++;
			else {
			    	dbg_vop(("dos_readdir: END OF DIR\n"));
				mnp->m_msddp->last_offset = 
					mnp->m_msddp->offset;
				mnp->m_msddp->offset = -1;
			}
			dstat_convert = dstatp;
			break;
		}
		offset++;
		if(ertn == NO)
			break;		/* done - no more entries */
		/* 
		 * Entries actually read from disk which are "." and ".."
		 * are ignored (although they DO count towards 
		 * mnp->m_msddp->offset). Also ignore DOS "Volume" entries.
		 */
		if(mnp->m_msddp->offset) {
			if(dos_isdot(dstat_convert->fname)) {
				dbg_vop(("dos_readdir: IGNORING %s\n",
					 dstat_convert->fname));
				continue;
			}
			if(dstat_convert->fattribute & AVOLUME)
				continue;	/* root directory 'pointer
						 * to self' - skip */
		}
						 
		/*
		 * We got a valid entry. Convert this DSTAT ufs direct format
		 * (NeXT) or file-system independent format (others). 
		 */
#ifdef	NeXT
		/*
		 * The NeXT way - Old UFS directory structure.
		 */
		msd_dosfname_to_unix(dstat_convert, direct_p->d_name);
		direct_p->d_namlen = strlen(direct_p->d_name);
		direct_p->d_reclen = DIRSIZ(direct_p);
		/*
		 * File's First cluster number, like inode number. 
		 */
		if(dstat_convert->fname[0] == '.') {
			if(dstat_convert->fname[1] == '.') {
				/*
				 * cluster number of the parent.
				 */
				direct_p->d_fileno = mnp->m_parent->m_fcluster;
			}
			else 
				/*
				 * our own cluster.
				 */
				direct_p->d_fileno = mnp->m_fcluster;
		}
		else if(dstat_convert->pobj->finode->fcluster == 0) {
			/*
			 * File with 0 size - fake a non-zero fileno 
			 * so caller will see SOMETHING
			 */
			direct_p->d_fileno = ZERO_SIZE_CLUSTER;
		}
		else
			direct_p->d_fileno = 
				dstat_convert->pobj->finode->fcluster;		
		bytes_generated += direct_p->d_reclen;
		if (bytes_generated > bytes_to_move) {
			/* 
			 * Got as many bytes as requested; back up and quit.
			 * Note we don't re-adjust offset; we can't back
			 * up a pc_gnext() call.
			 */
			bytes_generated -= direct_p->d_reclen;
			break;
		}
		direct_p = (struct direct *)((int)direct_p + 
			    direct_p->d_reclen);
#else	NeXT
		/*
		 * "Real" vfs way.
		 */
		msd_dosfname_to_unix(dstat_convert, dirent_p->d_name);
		dirent_p->d_namlen = strlen(dirent_p->d_name);
		dirent_p->d_reclen = DIRENTSIZ(dirent_p);
		dirent_p->d_off = bytes_generated + dirent_p->d_reclen;
		/*
		 * File's First cluster number, like inode number. 
		 */
		if(dstat_convert->fname[0] == '.') {
			if(dstat_convert->fname[1] == '.') {
				/*
				 * cluster number of the parent.
				 */
				direct_p->d_fileno = mnp->m_parent->m_fcluster;
			}
			else {
				direct_p->d_fileno = mnp->m_fcluster;
			}
		}
		else
			direct_p->d_fileno = 
				dstat_convert->pobj->finode->fcluster;		
		bytes_generated += dirent_p->d_reclen;
		if (bytes_generated > bytes_to_move) {
			/* 
			 * Got as many bytes as requested; back up and quit.
			 */
			bytes_generated -= dirent_p->d_reclen;
			break;
		}
		dirent_p->d_fileno = -1;		/* meaningless */
		dirent_p = (struct dirent *)((int)dirent_p + 
			    dirent_p->d_reclen);
#endif	NeXT
	} /* until end of dir or all bytes xferred */ 
 
	/*
	 * note we leave the directory 'open' (no pc_gdone()) for next
	 * dos_readdir().
	 * 
	 * Copy out the entry data.
	 */
done:
	if(bytes_generated) 
		rtn = uiomove(outbuf, bytes_generated, UIO_READ, uiop);
	uiop->uio_offset = offset + 2;
out:
	dbg_vop(("dos_readdir: EXIT offset %d\n", uiop->uio_offset));
	if(outbuf)
#ifdef	NeXT
		kfree(outbuf, bytes_to_move + sizeof(struct direct));
#else	NeXT
		kfree(outbuf, bytes_to_move + sizeof(struct dirent));
#endif	NeXT
	mn_unlock(mnp);
	return (rtn);
}

static int
dos_symlink(struct vnode *dvp,		/* new link's parent's vnode */
	char *lnm,			/* new link's name */
	struct vattr *vap,		/* attr of new link */
	char *tnm,			/* source's name */
	struct ucred *cred)
{
	dbg_vop(("dos_symlink\n"));
	return(EINVAL);
}

static int
dos_bmap(struct vnode *vp,		/* vnode to check out */
	daddr_t lbn,			/* DOS FS block # */
	struct vnode **vpp,		/* vnode of device vp resides on
					 * (RETURNED if vpp != NULL) */
	daddr_t *bnp)			/* device block containing lbn
					 * (RETURNED if bnp != 0) */
{
	dbg_vop(("dos_bmap\n"));
}

static int
dos_bread(struct vnode *vp,		/* desired file's vnode */
	daddr_t lbn,			/* DOS FS block to read */
	struct buf **bpp,		/* RETURNED */
	long *sizep)			/* ? */
{
	dbg_vop(("dos_bread\n"));
}

static int
dos_brelse(struct vnode *vp,
	struct buf *bp)
{
	dbg_vop(("dos_brelse\n"));
}

static int
dos_cmp(struct vnode *vp1, 
	struct vnode *vp2)
{
	dbg_vop(("dos_cmp <%s> <%s>\n", VTOM(vp1)->m_path, VTOM(vp2)->m_path));
	return (vp1 == vp2);
}

static int
dos_realvp(struct vnode *vp,
	struct vnode **vpp)
{
	dbg_vop(("dos_realvp\n"));
	return (EINVAL);		/* ? */
}

static int
dos_badop()
{
	dbg_vop(("dos_badop\n"));
	panic("dos_badop");
}

static int
dos_lockctl()
{
	dbg_vop(("dos_lockctl\n"));
    	return(EINVAL);
}

static int
dos_fid(struct vnode *vp,
	struct fid **fidpp)
{
	dbg_vop(("dos_fid\n"));
}

static int
dos_strategy(struct buf *bp)
{
	dbg_vop(("dos_strategy\n"));
}

static int
dos_nlinks(struct vnode	*vp,
    	int	*l)
{
	dbg_vop(("dos_nlinks\n"));
	*l = 1;
	return(0);			/* links not supported */
}

/* end of dos_vnodeops.c */