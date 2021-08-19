/*
 * cfs_clnt.h:
 *
 * 15-Apr-90 Simson L. Garfinkel (simsong), in Cambridge for NeXT.
 *	Took code from nfs_clnt.h
 *	This file defines the private information for each mounted
 *	file system.  We use it to describe all of the information
 *	about the currently mounted CDROM.
 *
 *	We use the same name, struct mntinfo, for the information.
 */

/*
 * vfs pointer to mount info
 */
#define	vftomi(vfsp)	((struct mntinfo *)((vfsp)->vfs_data))

/*
 * vnode pointer to mount info
 */
#define	vtomi(vp)	((struct mntinfo *)(((vp)->v_vfsp)->vfs_data))

/*
 * NFS private data per mounted file system
 */
struct mntinfo {				/* Mount information 	*/
	struct	vnode *mi_devvp;		/* vnode for block I/O	*/
	fs_typ	fstype;				/* CDROM file system type */
	extent	root_dir;			/* root directory	*/
	int	mi_refct;			/* active vnodes for vfs*/
	int	mountslot;			/* slot in cfs_mounts	*/

#ifdef FULL_ISO
	/* Volume information */
	char	system_id[36];
	char	volume_id[36];
	char	volset_id[132];
	char	publisher_id[132];
	char	dataprep_id[132];
	char	application_id[132];
	char	copyright_file[40];
	char	abstract_file[40];
	char	bibliographic_file[40];
#endif
};


/* cnode.h:
 *
 * Taken from nfs/rnode.h.
 *
 * The cnode is the "inode" for files on the cdrom.  It contains
 * all the information necessary to handle remote file on the
 * client side.
 */
struct cnode {
	struct vnode	c_vnode;		/* vnode for CDROM file */
	struct vnode	*c_devvp;		/* vnode for block I/O */
	struct cnode	*c_next;		/* active cnode list */
	extent		c_ext;			/* extent for the file */
	u_short		c_flags;		/* flags, see below */
	off_t		endofdir;		/* to keep track of end */
	off_t		nextdirread;		/* performance hack 	*/
};

/* Shortcuts for things stored in the extent */
#define c_size	c_ext.e_bytes			/* size stored in extent  */

/*
 * CNODE Flags
 */
#define	REOD		0x01		/* EOD We know where end of dir is   */

/*
 * Convert between vnode and cnode
 */
#define	ctov(cp)	(&(cp)->c_vnode)
#define	vtoc(vp)	((struct cnode *)((vp)->v_data))
#define	vtoext(vp)	(&(vtoc(vp)->c_ext))
#define	ctoext(cp)	(&(cp)->c_ext)

/* returns */
int	cdrom_extovnode(struct vfs *,extent *,int,struct vnode **);
caddr_t	vnode_allocate(void);
void	vnode_free(caddr_t vn);

/* cfs_subr.c */
struct vnode *makecfsnode(extent *ex, struct vfs *vfsp);
void	cfs_cfree(struct cnode *cp);
void	c_inval(struct vfs *vp);
void	csave(struct cnode *cp);
void	clist_init(void);


/* cfs_vfsops.c */

extern	struct	vfsops 	cfs_vfsops;

int	cfs_mount(struct vfs *vfsp, char *path, caddr_t data);
int	cfs_umount(struct vfs *vfsp);
int	cfs_root(struct vfs *vfsp,struct vnode **vpp);
int	cfs_statfs(struct vfs *vfsp,struct statfs *sdp);
int	cfs_sync(struct vfs *vfsp);
void	cfs_init(void);

/* cfs_vnodeops.c */

extern	struct vnodeops cfs_vnodeops;

int	cfs_getattr(struct vnode *vp, struct vattr *vap, struct ucred *cred);
int	cfs_lookup(struct vnode *dvp,char *nm,
		   struct vnode **vpp,struct ucred *cred);
int	cfs_readdir(struct vnode *vp,struct uio *uiop,struct ucred *cred);
int	cfs_pagein(struct vnode *vp,vm_page_t page,vm_offset_t offset);

/* cdrom.c */
#define	OFFSET_IS_COUNT	0x80000000
#define OFFSET_MASK	0x7fffffff

int	cdrom_mount(struct vnode *devvp,struct mntinfo *mi);
int	cdrom_readdir(struct cnode *cp,off_t *offset,u_long *bytes,char *buf);
int	cdrom_lookup(struct cnode *cp,char *name,extent *ex);
void	cdrom_init(void);

/* define functions that aren't defined in include files */
int	printf(const char *format, ...);
int	uiomove(caddr_t,int,enum uio_rw,struct uio *);
int	bcmp(void *,void *,int);
void	binvalfree(struct vnode *);
int	strncmp(const char *,const char *,int);
void 	strcpy(char *s1, char *s2);
int	strlen(const char *);
int	strcmp(const char *,const char *);
void	copy_to_phys(char *,long,int);
int	copyin(void *p1, void *p2, int size);
void 	bcopy(void *src, void *dest, int length);
void 	bzero(void *bp, int size);
void	panic(const char *);
