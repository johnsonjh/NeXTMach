/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 * 21-Feb-90  Morris Meyer (mmeyer) at NeXT
 *	Backed out NFS 4.0 IMARK() macro.
 *
 * 28-Sep-89  Morris Meyer (mmeyer) at NeXT
 *      NFS 4.0 Changes.
 *
 * 20-Sep-88  Avadis Tevanian (avie) at NeXT
 *	Support fast symbolic links for NeXT too.
 *
 * 18-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminate MACH_VM conditional.
 *
 * 27-Oct-87  Peter King (king) at NeXT
 *	SUN_VFS: Added NFS support.
 *
 * 25-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Use vm_info field regardless of which options are enabled.
 *
 * 22-Jan-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	MACH_XP: Correct type for inode.pager field.
 *
 * 22-Dec-86  David L. Black (dlb) at Carnegie-Mellon University
 *	MULTIMAX: support Encore's fast symbolic links.
 *
 * 21-Oct-86  Jonathan J. Chew at Carnegie-Mellon University
 *	Use romp definition of "i_size" and "di_size" fields in inode
 *	structure with 68000.
 *
 * 14-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added changes for ns32000 byte ordering (from dlb).
 *
 * 31-May-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Set the pager_id field in the inode structure.  This is a back
 *	pointer to the real pager id which will allow for pager id's to
 *	be different than inode numbers.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 18-Feb-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Added different definitions of i_size and di_size for Sailboat
 *	from IBM code under switch ROMP.  These differences are
 *	presumably because of byte ordering differences.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)inode.h	7.1 (Berkeley) 6/4/86
 */

/*	@(#)inode.h	2.2 88/06/20 4.0NFSSRC SMI;	from UCB 7.1 6/4/86	*/

/*
 * The I node is the focus of all local file activity in UNIX.
 * There is a unique inode allocated for each active file,
 * each current directory, each mounted-on file, text file, and the root.
 * An inode is 'named' by its dev/inumber pair. (iget/iget.c)
 * Data in icommon is read in from permanent inode on volume.
 */

#define	NDADDR	12		/* direct addresses in inode */
#define	NIADDR	3		/* indirect addresses in inode */
#if	defined(NeXT) || defined(MULTIMAX)
#define MAX_FASTLINK_SIZE	((NDADDR + NIADDR) * sizeof (daddr_t))
#endif	defined(NeXT) || defined(MULTIMAX)

struct inode {
	struct	inode *i_chain[2];	/* must be first */
	struct	inode *inode_list;	/* list of all inodes --- could search
					   inode hash table to traverse
					   instead though */
	struct	vnode i_vnode;	/* vnode associated with this inode */
	struct	vnode *i_devvp;	/* vnode for block I/O */
	u_short	i_flag;
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* i number, 1-to-1 with device address */
	off_t	i_diroff;	/* offset in dir, where we found last entry */
	struct	fs *i_fs;	/* file sys associated with this inode */
	struct	dquot *i_dquot;	/* quota structure controlling this file */
	union {
		daddr_t	if_lastr;	/* last read (read-ahead) */
		struct	socket *is_socket;
	} i_un;
	struct	{
		struct inode  *if_freef;	/* free list forward */
		struct inode **if_freeb;	/* free list back */
	} i_fr;
	struct 	icommon
	{
		u_short	ic_mode;	/*  0: mode and type of file */
		short	ic_nlink;	/*  2: number of links to file */
		uid_t	ic_uid;		/*  4: owner's user id */
		gid_t	ic_gid;		/*  6: owner's group id */
		quad	ic_size;	/*  8: number of bytes in file */
		time_t	ic_atime;	/* 16: time last accessed */
		long	ic_atspare;
		time_t	ic_mtime;	/* 24: time last modified */
		long	ic_mtspare;
		time_t	ic_ctime;	/* 32: last time inode changed */
		long	ic_ctspare;
#if	defined(NeXT) || defined(MULTIMAX)
		union {
		    struct { 
			daddr_t	Mb_db[NDADDR]; /* 40: disk block addresses*/
			daddr_t	Mb_ib[NIADDR]; /* 88: indirect blocks */
		    } ic_Mb;
#define	ic_db	ic_Mun.ic_Mb.Mb_db
#define	ic_ib	ic_Mun.ic_Mb.Mb_ib
		    char	ic_Msymlink[MAX_FASTLINK_SIZE];
						/* 40: symbolic link name */
		} ic_Mun;
#define ic_symlink	ic_Mun.ic_Msymlink
#else	defined(NeXT) || defined(MULTIMAX)
		daddr_t	ic_db[NDADDR];	/* 40: disk block addresses */
		daddr_t	ic_ib[NIADDR];	/* 88: indirect blocks */
#endif	defined(NeXT) || defined(MULTIMAX)
		long	ic_flags;	/* 100: status, currently unused */
#if	defined(NeXT) || defined(MULTIMAX)
#define	IC_FASTLINK	0x0001		/* Symbolic link in inode */
#endif	defined(NeXT) || defined(MULTIMAX)
		long	ic_blocks;	/* 104: blocks actually held */
		long	ic_gen;		/* 108: generation number */
		long	ic_spare[4];	/* 112: reserved, currently unused */
	} i_ic;
	uid_t	i_ruid;			/* cached real uid for user mounts */
	gid_t	i_rgid;			/* cached real gid for user mounts */
};

struct dinode {
	union {
		struct	icommon di_icom;
		char	di_size[128];
	} di_un;
};

#define	i_mode		i_ic.ic_mode
#define	i_nlink		i_ic.ic_nlink
#define	i_uid		i_ic.ic_uid
#define	i_gid		i_ic.ic_gid
/* ugh! -- must be fixed */
#if	defined(vax) || defined(ns32000)
#define	i_size		i_ic.ic_size.val[0]
#endif	defined(vax) || defined(ns32000)
#if	defined(romp) || defined(mc68000)
#define i_size		i_ic.ic_size.val[1]
#endif	defined(romp) || defined(mc68000)
#define	i_db		i_ic.ic_db
#define	i_ib		i_ic.ic_ib
#if	defined(NeXT) || defined(MULTIMAX)
#define	i_symlink	i_ic.ic_symlink
#define i_icflags	i_ic.ic_flags
#endif	defined(NeXT) || defined(MULTIMAX)
#define	i_atime		i_ic.ic_atime
#define	i_mtime		i_ic.ic_mtime
#define	i_ctime		i_ic.ic_ctime
#define i_blocks	i_ic.ic_blocks
#define	i_rdev		i_ic.ic_db[0]
#define	i_gen		i_ic.ic_gen
#define	i_lastr		i_un.if_lastr
#define	i_socket	i_un.is_socket
#define	i_forw		i_chain[0]
#define	i_back		i_chain[1]
#define	i_freef		i_fr.if_freef
#define	i_freeb		i_fr.if_freeb

#define di_ic		di_un.di_icom
#define	di_mode		di_ic.ic_mode
#define	di_nlink	di_ic.ic_nlink
#define	di_uid		di_ic.ic_uid
#define	di_gid		di_ic.ic_gid
#if	defined(vax) || defined(ns32000)
#define	di_size		di_ic.ic_size.val[0]
#endif	defined(vax) || defined(ns32000)
#if	defined(romp) || defined(mc68000)
#define di_size		di_ic.ic_size.val[1]
#endif	defined(romp) || defined(mc68000)
#define	di_db		di_ic.ic_db
#define	di_ib		di_ic.ic_ib
#if	defined(NeXT) || defined(MULTIMAX)
#define	di_symlink	di_ic.ic_symlink
#define	di_icflags	di_ic.ic_flags
#endif	defined(NeXT) || defined(MULTIMAX)
#define	di_atime	di_ic.ic_atime
#define	di_mtime	di_ic.ic_mtime
#define	di_ctime	di_ic.ic_ctime
#define	di_rdev		di_ic.ic_db[0]
#define	di_blocks	di_ic.ic_blocks
#define	di_gen		di_ic.ic_gen

#ifdef KERNEL
struct inode *inode_list;	/* list of all inodes */

extern struct vnodeops ufs_vnodeops;	/* vnode operations for ufs */

struct inode	*ialloc();
struct inode	*iget();
ino_t		dirpref();
#endif

/* flags */
#define	ILOCKED		0x001		/* inode is locked */
#define	IUPD		0x002		/* file has been modified */
#define	IACC		0x004		/* inode access time to be updated */
#define IMOD		0x008		/* inode has been modified */
#define	IWANT		0x010		/* some process waiting on lock */
#define	ICHG		0x040		/* inode has been changed */
#define	ILWAIT		0x080		/* someone waiting on file lock */
#define IREF		0x100		/* inode is being referenced */
#if	NeXT
#define	IFREE		0x200		/* inode has been free'd */
#endif	NeXT

/* modes */
#define	IFMT		0170000		/* type of file */
#define	IFIFO		0010000		/* named pipe (fifo) */
#define	IFCHR		0020000		/* character special */
#define	IFDIR		0040000		/* directory */
#define	IFBLK		0060000		/* block special */
#define	IFREG		0100000		/* regular */
#define	IFLNK		0120000		/* symbolic link */
#define	IFSOCK		0140000		/* socket */

#define	ISUID		04000		/* set user id on execution */
#define	ISGID		02000		/* set group id on execution */
#define	ISVTX		01000		/* save swapped text even after use */
#define	IREAD		0400		/* read, write, execute permissions */
#define	IWRITE		0200
#define	IEXEC		0100

#ifdef	KERNEL
/*
 * Convert between inode pointers and vnode pointers
 */
#define VTOI(VP)	((struct inode *)(VP)->v_data)
#define ITOV(IP)	((struct vnode *)&(IP)->i_vnode)

/*
 * Convert between vnode types and inode formats
 */
extern enum vtype	iftovt_tab[];
extern int		vttoif_tab[];
#define IFTOVT(M)	(iftovt_tab[((M) & IFMT) >> 13])
#define VTTOIF(T)	(vttoif_tab[(int)(T)])

#define MAKEIMODE(T, M)	(VTTOIF(T) | (M))

/*
 * Lock and unlock inodes.
 */

#define	ILOCK(ip) { \
	while ((ip)->i_flag & ILOCKED) { \
		(ip)->i_flag |= IWANT; \
		sleep((caddr_t)(ip), PINOD); \
	} \
	(ip)->i_flag |= ILOCKED; \
}

#define	IUNLOCK(ip) { \
	(ip)->i_flag &= ~ILOCKED; \
	if ((ip)->i_flag&IWANT) { \
		(ip)->i_flag &= ~IWANT; \
		wakeup((caddr_t)(ip)); \
	} \
}

#define IUPDAT(ip, waitfor) { \
	if (ip->i_flag & (IUPD|IACC|ICHG|IMOD)) \
		iupdat(ip, waitfor); \
}

/*
 * Mark an inode with the current (unique) timestamp.
 */
struct timeval iuniqtime;

#ifdef NeXT
#define IMARK(ip) { \
	microtime(&iuniqtime); \
	if ((ip)->i_flag & IACC) { \
		(ip)->i_atime = iuniqtime.tv_sec; \
	} \
	if ((ip)->i_flag & IUPD) { \
		(ip)->i_mtime = iuniqtime.tv_sec; \
	} \
	if ((ip)->i_flag & ICHG) { \
		ip->i_diroff = 0; \
		(ip)->i_ctime = iuniqtime.tv_sec; \
	} \
}
#else
#define IMARK(ip) { \
	if (time.tv_sec > iuniqtime.tv_sec || \
	    time.tv_usec > iuniqtime.tv_usec) { \
		iuniqtime.tv_sec = time.tv_sec; \
		iuniqtime.tv_usec = time.tv_usec; \
	} else { \
		iuniqtime.tv_usec++; \
	} \
	if ((ip)->i_flag & IACC) { \
		(ip)->i_atime = iuniqtime.tv_sec; \
	} \
	if ((ip)->i_flag & IUPD) { \
		(ip)->i_mtime = iuniqtime.tv_sec; \
	} \
	if ((ip)->i_flag & ICHG) { \
		ip->i_diroff = 0; \
		(ip)->i_ctime = iuniqtime.tv_sec; \
	} \
}
#endif NeXT

#define ITIMES(ip) { \
	if ((ip)->i_flag & (IUPD|IACC|ICHG)) { \
		(ip)->i_flag |= IMOD; \
		IMARK(ip); \
		(ip)->i_flag &= ~(IACC|IUPD|ICHG); \
	} \
}

#define ESAME	(-1)		/* trying to rename linked files (special) */

/*
 *  Macros for modifying inode reference counts - used to check for consistency
 *  at each reference.
 *
 *  The idecr_chk() macro is defined such that it doesn't decrement the
 *  reference count when already zero.  This is in order to avoid the nested
 *  iincr panic which would otherwise occur when update() tries to sync the
 *  inode before halting and increments the count back to zero.
 *
 *  The panic message strings are initialized variables rather than constant
 *  strings since they are potentially used in many places.
 */
extern char *PANICMSG_IINCR;
extern char *PANICMSG_IDECR;

#define	iincr_chk(ip)	{ if (++((ip)->i_count) == 0) panic(PANICMSG_IINCR); }
#define	idecr_chk(ip)	{ if (((ip)->i_count) != 0) ((ip)->i_count--); else panic(PANICMSG_IDECR); }

/*
 * Check that file is owned by current user or user is su.
 */
#define OWNER(CR, IP)	(((CR)->cr_uid == (IP)->i_uid)? 0: (suser()? 0: u.u_error))
/*
 * enums
 */
enum de_op	{ DE_CREATE, DE_LINK, DE_RENAME };	/* direnter ops */

/*
 * This overlays the fid sturcture (see vfs.h)
 */
struct ufid {
	u_short	ufid_len;
	ino_t	ufid_ino;
	long	ufid_gen;
};
#endif	KERNEL
