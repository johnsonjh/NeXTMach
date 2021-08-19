/*
 * HISTORY
 * 15-Apr-90  Simson L. Garfinkel (simsong) in Cambridge, for Next.
 *	Adopted from nfs_vsops.c
 *
 * 28-Oct-87  Peter King (king) at NeXT, Inc.
 *	Original Sun source, ported to  Mach.
 */ 

#include "sys/param.h"
#include "sys/systm.h"
#include "sys/dir.h"
#include "sys/user.h"
#include "sys/vfs.h"
#include "sys/vnode.h"
#include "sys/uio.h"
#include "sys/socket.h"
#include "sys/time.h"
#include "sys/mount.h"
#include "sys/ioctl.h"
#include "kern/zalloc.h"
#include "vm/vm_page.h"

#include "kern/kalloc.h"
#include "cdrom.h"				/* cdrom hardware defines */
#include "cfs.h"

extern struct zone *vfs_zone;

/*
 * cfs vfs operations.
 */
int 		cfs_mount();
int 		cfs_unmount();
int 		cfs_root();
int 		cfs_statfs();
int 		cfs_sync();
extern int 	cfs_badop();

struct vfsops cfs_vfsops = {
	cfs_mount,
	cfs_unmount,
	cfs_root,
	cfs_statfs,
	cfs_sync,
	cfs_badop
};

dev_t	cfs_mounts[CFS_MAXMOUNTS];


/*
 * Convert "/dev/whatever" to a dev_t. Maybe this should be in the kernel; it's
 * a copy of getmdev() in ufs/ufs_vfsops with the addition of passing the 
 * expected device type.
 */
static int fs_getmdev(char *fspec, dev_t *pdev, enum vtype dev_type)
{
	register int error;
	struct vnode *vp;
	int	lookupname(char *,int,int,struct vnode **,struct vnode **);

#ifndef VTEST

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
#else
	*pdev	= 1;				/* for testing */
	return(0);
#endif
}


/* this function allocates the mntinfo (vfs->vfs_data) and links it into
 * the vfs that we are handed.
 */

/*
 * cfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
int	cfs_mount(struct vfs *vfsp, char *path, caddr_t data)
{
	int 	error 		= 0;	/* errors that develop 		*/
	struct 	mntinfo 	*mnt;	/* mount info returned by cdrom */
	struct 	ufs_args 	args;	/* cfs mount arguments 		*/
	struct	vnode		*devvp;	/* device vnode 		*/
	dev_t	dev;			/* device to mount onto 	*/
	int	freeslot;		/* cfs_mounts slot		*/
	int	i;

	dprint2("cfs_mount(vfs=%x,path=%s)\n",vfsp,path);

	/*
	 * get arguments
	 */
	error = copyin(data, (caddr_t)&args, sizeof (args));
	if (error) {
		goto errout;
	}


	/* Get a filespec for the mount.
	 * CDROM Style we get a TID.
	 */

	if((error = fs_getmdev(args.fspec, &dev, VCHR)) != 0){
		return(error);
	}

	/* See if we already have something mounted on this device */
	for(i=0;i<CFS_MAXMOUNTS;i++){
		if(cfs_mounts[i]==dev){
			return(EBUSY);
		}
	}

	/* See if we have room in the table */
	for(freeslot = -1,i = 0;i<CFS_MAXMOUNTS;i++){
		if(cfs_mounts[i]==0){
			freeslot	= i;
			break;
		}
	}

	if(freeslot==-1){
		return(ENOMEM);
	}

	/*
	 * Make a special (device) vnode for the filesystem.
	 */
	
	devvp		= bdevvp(dev);
	devvp->v_type	= VCHR;		/* Be sure we are a character device */

	/*
	 * See if we can open the device.
	 */
	if(unix_open(devvp) ) {
		dprint("dos_open: unix_open failed\n");
		return(ENODEV);
	}

	/* Allocate space for mount info */

	mnt 		= (struct mntinfo *)kalloc(sizeof(struct mntinfo ));


	/* And mount the file system */

	error	= cdrom_mount(devvp,mnt);

	if(error) {
		unix_close(devvp);
		return(error);
	}
	
	/*
	 * Patch the mount info into the VFS-pointer
	 */

	vfsp->vfs_data 		= (caddr_t)mnt;
	vfsp->vfs_bsize		= CDROM_BLOCK_SIZE;
	vfsp->vfs_fsid.val[0] 	= dev;
	vfsp->vfs_fsid.val[1] 	= MOUNT_CFS;
	vfsp->vfs_flag		|= VFS_RDONLY;		/* read only fs */

	/*
	 * Remember we have mounted this file system.
	 */
	mnt->mountslot		= freeslot;
	cfs_mounts[freeslot]	= dev;

	dprint("cfs_mount: successful\n");

errout:
	return (error);
}


/*
 * unmount a file system.
 * Since we are stateless, we merely need to check to make sure that we
 * have no vnodes in use --- that the refct is 0.
 */

int	cfs_unmount(struct vfs *vfsp)
{
	struct mntinfo *mi = (struct mntinfo *)vfsp->vfs_data;


        dprint2("cfs_unmount(vfs=%x) mi = %x\n", vfsp, mi);

	if (mi->mi_refct > 0) {		/* active vnodes on cfs */
		printf("cfs: cannot unmount; refcnt=%d\n",mi->mi_refct);
		return (EBUSY);
	}

	cfs_mounts[mi->mountslot]	= 0;	/* we have unmounted it */

	unix_close(mi->mi_devvp);		/* close the device 	*/

	kfree((caddr_t)mi, (u_int)sizeof(*mi));	/* free our personal data */
	return(0);				/* success! 		*/
}

/*
 * find root of cfs and allocate a vnode for it.
 */
int	cfs_root(struct vfs *vfsp, struct vnode **vpp)
{
	struct	mntinfo *mi;

	dprint("cfs_root()\n");

	mi	= vftomi(vfsp);
	*vpp	= makecfsnode(&mi->root_dir,vfsp);

        dprint2("cfs_root(0x%x) = %x\n", vfsp, *vpp);

	return(0);
}

/*
 * Get file system statistics.
 * 	Unfortunately, there is no easy way to determine the amount of space
 *	used on an ISO 9660 CDROM, so we just tell the operating system that
 * 	it is all in use.
 */
int	cfs_statfs(struct vfs *vfsp, struct statfs *sbp)
{
	dprint1("cfs_statfs(vfs=%x)\n",vfsp);
	
	sbp->f_type	= 0;
	sbp->f_bsize	= CDROM_BLOCK_SIZE;
	sbp->f_blocks	= 70 * 60 * 75;	/* theoretical max */
	sbp->f_bfree	= 0;		/* no blocks free */
	sbp->f_bavail	= 0;		/* no blocks available */
	sbp->f_files	= 0;		/* system doesn't use file nodes*/
	sbp->f_ffree	= 0;		/* so none of them are free */
	sbp->f_fsid	= vfsp->vfs_fsid;
#ifdef Q
	bcopy((caddr_t)&vfsp->vfs_fsid,
	      (caddr_t)&sbp->f_fsid, sizeof (fsid_t));	/* grab vnode fs id */
#endif
	return(0);					/* no sweat! */
}


/*
 * Flush any pending I/O.
 */
int	cfs_sync(struct vfs *vfsp)
{
	return(0);
}


void	cfs_init()
{
	bzero(cfs_mounts,sizeof(cfs_mounts));
	cdrom_init();
	clist_init();
}
