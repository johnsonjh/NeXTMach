/*	@(#)msdos.h	2.0	26/06/90	(c) 1990 NeXT	*/

/* 
 * msdos.h
 *
 * HISTORY
 * 26-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <sys/uio.h>
#import <sys/printf.h>
#import <sys/vnode.h>
#import <kern/queue.h>
#import <kern/lock.h>
#import <header/pcdisk.h>

typedef int (*rw_func_t)(dev_t dev, struct uio *uiop);
typedef struct msdnode *msdnode_t;

/*
 * drive info. One entry per dos 'driveno'. One-to-one with struct vfs.
 */

struct dos_drive_info {
	dev_t 		dev;		/* live partition where I/O occurs. 
					 * NULL indicates not mounted. */
	struct vnode 	*ddivp;		/* vnode of snode representing dev */
	struct vfs	*vfsp;		/* vfs mounted on this device */
	UCOUNT		bytes_per_cluster;
					/* native FS block size */
	/*
	 * A list of active msdnodes is maintained per open drive. The first
	 * msdnode in the queue is always the msdnode of the filesystem's
	 * root.
	 */
	queue_head_t 	msdnode_valid_list;	
};
typedef struct dos_drive_info *ddi_t;

/*
 * DOS directory info.
 */
struct msd_dir {
	DSTAT		dstat;		/* if we've done pc_gfirst(), 
					 * msdnode.mflags & MF_STAT_VALID is
					 * true */
	int 		offset;		/* current entry number. 0 indicates
					 * no pc_gfirst yet; (u_int)(-1) 
					 * indicates that the last pc_gnext() 
					 * failed. This is the uio_offset used 
					 * in the getdirentries syscall. The 
					 * only vnode op which leaves this 
					 * nonzero is dos_readdir(). */
	int		last_offset;	/* offset value when pc_gnext failed */
	struct timeval	mtime;		/* time last modified. DOS does not
					 * support "modidy" times for 
					 * directories (only for files), so we
					 * keep track of this as long as we 
					 * have a directory open. 0 means we
					 * haven't gotten the time from disk. 
					 */
};
typedef	struct msd_dir *msd_dir_t;

/*
 * Another node - the msdnode. An msdnode exists as long as its associated
 * vnode's v_count is non-zero. We keep two linked lists of these around -
 * msdnode_valid and msdnode_free. Whenever we do a lookup of a new filename,
 * we search through msdnode_valid for an existing msdnode for the desired
 * file; the EBS file system provides no multi-user file opens.
 */
#define MSD_NAME_LEN	8		/* fname length, without null */
#define MSD_EXT_LEN	3		/* fext length, without null */
#define FNAME_LEN	(MSD_NAME_LEN+MSD_EXT_LEN+2)
					/* DOS filename length, with '.' and
					 * extension and trailing NULL */
struct msdnode {
	struct vnode 	m_vnode;	/* vnode associated with this 
					 * msdnode */
	struct msdnode	*m_parent;	/* parent dirdectory's msdnode */
	char		m_path[EMAXPATH];	
					/* path to this node, including 
					 * drive letter. */
	msd_dir_t	m_msddp;	/* if VDIR, ptr to directory info */
	lock_t		m_lockp;	/* alloc dynamically */
	u_int		m_flags;
	PCFD		m_fd;		/* EBS file descriptor. -1 indicates
					 * a file not open or directory 
					 * node. */
	LONG		m_fp;		/* file pointer; non-zero after 
					 * reading or writing */
	queue_chain_t	m_link;		/* for linking on msdnode_valid and
					 * msdnode_free */
	UCOUNT		m_fcluster;	/* DOS cluster number. Used for
					 * d_fileno in readdir and for
					 * va_nodeid in getattr */
};

/*
 * m_flags
 */
#define MF_LOCKED	0x00000001		/* debug only */

#define MF_STAT_VALID	0x00000002		/* pc_gfirst called for 
						 * m_msddp->dstat. The only
						 * vnode op which leaves this
						 * true is dos_readdir(). */
/*
 * Conversion between vnode and msdnode pointers.
 */
#define VTOM(VP)	((struct msdnode *)(VP)->v_data)
#define MTOV(MP)	(&(MP)->m_vnode)

/*
 * msdnode locking and unlocking.
 */
#ifdef	DEBUG
#define mn_lock(mnp)	{					\
	(lock_write((mnp)->m_lockp));				\
	(mnp)->m_flags |= MF_LOCKED;				\
}
#define mn_unlock(mnp)	{					\
	if(((mnp)->m_flags & MF_LOCKED) == 0) {			\
		panic("mn_unlock - msdnode not locked");	\
	}							\
	(lock_done((mnp)->m_lockp));				\
	(mnp)->m_flags &= ~MF_LOCKED;				\
}
#else	DEBUG
#define mn_lock(mnp)	(lock_write((mnp)->m_lockp))
#define mn_unlock(mnp)	(lock_done((mnp)->m_lockp))
#endif	DEBUG

/*
 * character mapping struct. An array of these is found in msd_char_map.
 */
struct msd_char_mapping {
	char in_char;
	char out_char;
};

/*
 * Globals.
 */
extern struct vfsops dos_vfsops;
extern struct vnodeops dos_vnodeops;
extern struct dos_drive_info dos_drive_info[];
extern int msdnode_free_cnt;	
extern queue_head_t msdnode_free_list;
extern struct msd_char_mapping msd_char_map[];
extern lock_t buff_lockp;
extern char *pageio_buf;
extern lock_t pageio_buf_lockp;

/*
 * Misc. constants
 */
#ifndef	NULL
#define NULL	0
#endif	NULL
#define DOS_SECTOR_SIZE		512		/* hard coded for this fs type 
						 */
#define MN_FREE_MAX		16		/* max number of msdnodes in
						 * msdnode_free */
#define ROOT_CLUSTER		((UCOUNT)(-1))	/* cluster # for root */
#define ZERO_SIZE_CLUSTER	((UCOUNT)(-2))	/* cluster # for files with */
						/* size of 0 */

/* end of msdos.h */