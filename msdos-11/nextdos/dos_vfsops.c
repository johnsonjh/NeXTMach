/*	@(#)dos_vfsops.c	2.0	26/06/90	(c) 1990 NeXT	*/

/* 
 * dos_vfsops.c -- VFS layer for loadable DOS file system
 *
 * HISTORY
 * 26-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <nextdev/ldd.h>
#import <sys/param.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/printf.h>
#import <sys/errno.h>
#import <sys/mount.h>
#import <sys/user.h>
#ifdef	MACH_ASSERT
#undef	MACH_ASSERT
#endif	MACH_ASSERT
#define MACH_ASSERT 1
#import <kern/assert.h>
#import <nextdev/voldev.h>
#import <nextdos/msdos.h>
#import <nextdos/next_proto.h>
#import <header/posix.h>
#import <nextdos/dosdbg.h>

extern void strcpy(char *s1, char *s2);
extern void microtime(struct timeval *tvp);

/*
 * EBS functions used in dos_statfs()
 */
char *pc_parsedrive(COUNT *driveno, char *path);
DDRIVE	*pc_drno2dr(COUNT driveno);

static int get_fsmdev(char *fspec, dev_t *pdev, enum vtype dev_type);
	
/*
 * misc. kernel prototypes
 */
int lookupname(char *fnamep,		/* user pathname */
	int seg,			/* addr space that name is in */
	enum symfollow followlink,	/* follow sym links */
	struct vnode **dirvpp,		/* ret for ptr to parent dir vnode */
	struct vnode **compvpp);

/*
 * local functions accessed via dos_vfsops
 */
static int dos_mount(struct vfs *vfsp, char *path, caddr_t data);
static int dos_mountroot(struct vfs *vfsp, struct vnode **vpp, char *name);
static int dos_unmount(struct vfs *vfsp);
static int dos_root(struct vfs *vfsp, struct vnode **vpp);
static int dos_statfs(struct vfs *vfsp, struct statfs *sbp);
static int dos_sync(struct vfs *vfsp);
static int dos_vget(struct vfs *vfsp, struct vnode **vpp, struct fid *fidp);
static int dos_badvfsop();

struct vfsops dos_vfsops = {
	(PFI)dos_mount,
	(PFI)dos_unmount,
	(PFI)dos_root,
	(PFI)dos_statfs,
	(PFI)dos_sync,
	(PFI)dos_vget,
	(PFI)dos_mountroot,
#if	!MACH
	(PFI)dos_swapvp,		/* XXX - swapvp */
#endif	!MACH
};

/*
 * dos_mount system call
 */
static int
dos_mount(struct vfs *vfsp,
	char *path,
	caddr_t data)
{
	int error;
	dev_t ldev;			/* live device for this mount */
	struct vnode *devvp;		/* vnode of ldev */
	struct pc_args args;		/* "/dev/xxx", copyin'd from *data */
	ddi_t ddip;			/* dos drive */
	int drivenum;
	msdnode_t rootnode;
	DDRIVE *pdr;
	
	dbg_vfs(("dos_mount\n"));
	/*
	 * Get device on which to mount. We expect the live partition here,
	 * but we don't check it.
	 */
	error = copyin(data, (caddr_t)&args, sizeof (struct pc_args));
	if (error) {
		return (error);
	}
	if ((error = get_fsmdev(args.fspec, &ldev, VCHR)) != 0)
		return (error);
	/*
	 * Get next available dos driveno. Also, check for other DOS 
	 * filesystems mounted on this device.
	 
	 */
	ddip = dos_drive_info;
	for(drivenum=0; drivenum<HIGHESTDRIVE; drivenum++, ddip++) {
		if(ddip->dev == ldev) {
			dbg_vfs(("File system already mounted on dev 0x%x\n",
				ldev));
			return(EBUSY);
		}
	}
	ddip = dos_drive_info;
	for(drivenum=0; drivenum<HIGHESTDRIVE; drivenum++, ddip++) {
		if(ddip->dev == NULL)
			break;
	}
	if(drivenum == HIGHESTDRIVE) {
		dbg_err(("dos_mount: NO DOS DRIVES AVAILABLE\n"));
		return(ENODEV);
	}
	/*
	 * Init root msdnode and dos_drive_info.
	 */
	rootnode = msdnode_new(NULL, (UCOUNT)ROOT_CLUSTER, (vm_size_t)0, TRUE);	
						/* root, is a directory */
	rootnode->m_vnode.v_flag |= VROOT;
	rootnode->m_vnode.v_vfsp = ddip->vfsp = vfsp;
	ddip->dev = ldev;

	/*
	 * Init *vfsp.
	 * FIXME: Allow read-only mounts? 
	 */
	vfsp->vfs_data = (caddr_t)ddip;
	vfsp->vfs_bsize = DOS_SECTOR_SIZE;	/* FIXME: cluster size?? */
	/*
	 * vfs_fsid set up like ufs code...probably meaningless..
	 */
	vfsp->vfs_fsid.val[0] = (long)drivenum;
	vfsp->vfs_fsid.val[1] = MOUNT_PC;
	/*
	 * First msdnode for this drive's valid_list is the root.
	 */
	queue_init(&ddip->msdnode_valid_list);
	queue_enter(&ddip->msdnode_valid_list, 
			rootnode, 
			msdnode_t, 
			m_link);

	/*
	 * Name of the root directory is something like "A:".
	 */
	rootnode->m_path[0] = 'A' + drivenum;
	rootnode->m_path[1] = ':';
	rootnode->m_path[2] = '\0';
	dbg_vfs(("dos_mount: mounting drive %s from dev_t 0x%x\n", 
		rootnode->m_path, ldev));
	/*
	 * Get an snode for the device we'll mount on. We keep this snode's 
	 * vnode in ddip->ddivp; all I/O will henceforth go though ddivp.
	 */
	ddip->ddivp = bdevvp(ldev);
	if(ddip->ddivp == NULL) {
		printf("dos_mount: Could Not Allocate snode\n");
		return(ENOSPC);
	}
	/*
	 * FIXME: is this legal? bdevvp() / specvp() set this to VBLK, which
	 * the live device is NOT...
	 */
	ddip->ddivp->v_type = VCHR;

	/*
	 * See if we can open the device.
	 */
	if(unix_open(ddip) == NO) {
		dbg_err(("dos_open: unix_open failed\n"));
		mn_unlock(rootnode);
		ddip->dev = 0;		/* drive not in use */
		return(ENODEV);
	}
	if(pc_dskopen(rootnode->m_path) == NO) {
		dbg_err(("dos_open: pc_dskopen returned %d\n", msd_errno));
		unix_close(ddip);
		mn_unlock(rootnode);
		ddip->dev = 0;		/* drive not in use */
		return(msd_errno());
	}
	microtime(&rootnode->m_msddp->mtime);
	/*
	 * Get cluster size from EBS private info.
	 */
	pdr = msd_getdrive(drivenum);
	ddip->bytes_per_cluster = pdr->bytespcluster;

	/* VN_HOLD(MTOV(rootnode)); */
	mn_unlock(rootnode);
	vol_notify_cancel(ldev);
	return(0);
}

/*
 * Convert "/dev/whatever" to a dev_t. Maybe this should be in the kernel; it's
 * a copy of getmdev() in ufs/ufs_vfsops with the addition of passing the 
 * expected device type.
 */
static int get_fsmdev(char *fspec,
	dev_t *pdev,
	enum vtype dev_type)
{
	register int error;
	struct vnode *vp;

	/*
	 * Get the device to be mounted
	 */
	error = lookupname(fspec, UIO_USERSPACE, FOLLOW_LINK,
	    (struct vnode **)0, &vp);
	if (error) {
		if (u.u_error == ENOENT)
			return (ENODEV);	/* needs translation */
		return (error);
	}
	if (vp->v_type != dev_type) {
		VN_RELE(vp);
		return (EINVAL);
	}
	*pdev = vp->v_rdev;
	VN_RELE(vp);
	return (0);
}


/*
 * Called by vfs_mountroot when dos is going to be mounted as root
 */
static int
dos_mountroot(struct vfs *vfsp,
	struct vnode **vpp,
	char *name)
{
	dbg_vfs(("dos_mountroot\n"));
	return(EINVAL);
}

static int
dos_unmount(struct vfs *vfsp)
{
	msdnode_t rootnode, mnp, mnp_prev;
	ddi_t ddip;
	BOOL ertn;
	int rtn = 0;
	queue_head_t *qhp;
	
	dbg_vfs(("dos_unmount\n"));
	ddip = (ddi_t)(vfsp->vfs_data);
	/*
	 * First free all msdnodes with count of 0 (these should only be
	 * directories). What is left should only be the root. Note we start
	 * from the end of the queue to ensure that parents are freed after
	 * their children.
	 */
	qhp = &ddip->msdnode_valid_list;
	mnp = (msdnode_t)qhp->prev;
	rootnode = (msdnode_t)qhp->next;
	while(mnp != rootnode) {
		mnp_prev = (msdnode_t)mnp->m_link.prev;
		if(mnp->m_vnode.v_count == 0) {
			ASSERT(mnp->m_msddp);
			queue_remove(qhp, mnp, struct msdnode *,
				    m_link);
			msdnode_free(mnp);
		}
		mnp = mnp_prev;
	}
	/*
	 * sanity check - make sure the only active msdnode is the root.
	 */
	if(ddip->msdnode_valid_list.next != ddip->msdnode_valid_list.prev) {
		printf("dos_unmount: msdnode references still exist\n");
		return(EBUSY);
	}
	if(rootnode->m_vnode.v_count != 1) {
		dbg_vfs(("dos_unmount: root msdnode count = %d\n",
			rootnode->m_vnode.v_count));
		return(EBUSY);
	}
	/*
	 * flush all data out to disk. 
	 */
	msd_free_dir(rootnode);	
	ertn = pc_dskclose(rootnode->m_path);
	if(ertn == NO) {
		printf("dos_unmount: pc_dskclose returned %d\n", msd_errno);
		rtn = EIO;
	}
	/*
	 * can't VOP_INACTIVE; we special case the root msdnode there...
	 */
	rootnode->m_vnode.v_count = 0;
	msdnode_free(rootnode);
	unix_close(ddip);	
	ddip->dev = NULL;			/* mark unmounted */
	return(0);
}

/*
 * find root vnode of dos. It's in the msdnode at the head of the associated
 * dos_drive_info's msdnode_active_list.
 */
static int
dos_root(struct vfs *vfsp,
	struct vnode **vpp)			/* vnode of root (RETURNED) */
{
	ddi_t ddip;
	msdnode_t rootnode;
	
	dbg_vfs(("dos_root\n"));
	ddip = (ddi_t)vfsp->vfs_data;
	rootnode = (msdnode_t)ddip->msdnode_valid_list.next;
	VN_HOLD(MTOV(rootnode));
	ASSERT(rootnode != (msdnode_t)&ddip->msdnode_valid_list);
	*vpp = MTOV(rootnode);
	return(0);
}

/*
 * Get file system statistics.
 */
static int
dos_statfs(struct vfs *vfsp,
	struct statfs *sbp)
{
	ddi_t ddip = (ddi_t)vfsp->vfs_data;
	DDRIVE *pdr;
	char path[3];
	int drivenum = ddip - dos_drive_info;
	
	dbg_vfs(("dos_statfs\n"));
	if((ddip < dos_drive_info) || 
	   (ddip > &dos_drive_info[HIGHESTDRIVE-1])) {
	   	dbg_err(("dos_statfs: INVALID vfsp\n"));
	   	return(EINVAL);
	}
	sbp->f_bsize = DOS_SECTOR_SIZE;
	pdr = msd_getdrive(drivenum);
	if(!pdr)
		return(EINVAL);
	sbp->f_blocks = pdr->maxfindex * ddip->bytes_per_cluster /
		DOS_SECTOR_SIZE;
	strcpy(path, "A:");
	path[0] += drivenum;
	sbp->f_bfree = sbp->f_bavail = pc_free(path) / DOS_SECTOR_SIZE;
	/*
	 * FIXME
	 */
	sbp->f_files =  1000;
	sbp->f_ffree = 1000;
	return (0);
}

/*
 * Flush any pending I/O to file system vfsp.
 */
static int
dos_sync(struct vfs *vfsp)
{
#ifdef	SHOW_SYNC
	dbg_vfs(("dos_sync\n"));
#endif	SHOW_SYNC
	return(0);
}

static int
dos_vget(struct vfs *vfsp,
	struct vnode **vpp,
	struct fid *fidp)
{
	/* 
	 * DOS filesystem does not provide any fid mechanism (except for 
	 * currently opened files).
	 */
	dbg_vfs(("dos_vget\n"));
	return(EINVAL);
}
#if	!MACH
static int
dos_swapvp()
{

	dbg_vfs(("dos_swapvp\n"));
	return (EINVAL);
}
#endif	MACH
/* end of dos_nfsops.c */