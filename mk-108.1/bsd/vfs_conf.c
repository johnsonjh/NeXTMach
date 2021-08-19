/* 
 * HISTORY
 * 02-May-90  Morris Meyer (mmeyer) at NeXT
 *	Reserved space for loadable CD-ROM filesystem.
 *
 * 26-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	Sun 4.0 Changes: Support for bootconf stuff.
 *
 * 26-Oct-87  Peter King (king) at NeXT
 *	Original Sun source.  Changed NFS to SUN_NFS.
 */ 

#import <nfs_client.h>

/*	@(#)vfs_conf.c	2.2 88/06/13 4.0NFSSRC SMI;  from SMI 1.6 87/01/27	*/

#import <sys/param.h>
#import <sys/vfs.h>
#import <sys/bootconf.h>

extern	struct vfsops ufs_vfsops;	/* XXX Should be ifdefed */

#if	NFS_CLIENT
extern	struct vfsops nfs_vfsops;
#endif	NFS_CLIENT

#ifdef PCFS
extern	struct vfsops pcfs_vfsops;
#endif

#ifdef LOFS
extern	struct vfsops lo_vfsops;
#endif

#ifdef RFS
extern	struct vfsops rfs_vfsops;
#endif

extern	struct vfsops spec_vfsops;

/* 
 * WARNING: THE POSITIONS OF FILESYSTEM TYPES IN THIS TABLE SHOULD NOT
 * BE CHANGED. These positions are used in generating fsids and fhandles.
 * Thus, changing positions will cause a server to change the fhandle it
 * gives out for a file.
 */

struct vfssw vfssw[] = {
#ifndef NeXT
	"spec", &spec_vfsops,		/* SPEC */
#endif  NeXT
	"4.3", &ufs_vfsops,		/* 0 - UFS */
#ifdef NFS_CLIENT
	"nfs", &nfs_vfsops,		/* 1 - NFS */
#else	NFS_CLIENT
	(char *)0, (struct vfsops *)0,
#endif	NFS_CLIENT
#ifdef PCFS
	"pc", &pcfs_vfsops,		/* 2 - PC */
#else
	(char *)0, (struct vfsops *)0,	/* NeXT - MS-DOS */
#endif
#ifdef LOFS
	"lo", &lo_vfsops,		/* 3 - Loopback */
#else
	(char *)0, (struct vfsops *)0,
#endif
#ifdef NeXT
	"spec", &spec_vfsops,		/* 4 - SPEC */
	(char *)0, (struct vfsops *)0,  /* 5 = CD-ROM File System */
#endif NeXT
	(char *)0, (struct vfsops *)0,  /* 6 = runtime expansion */
	(char *)0, (struct vfsops *)0,  /* 7 = runtime expansion */
	(char *)0, (struct vfsops *)0,  /* 8 = runtime expansion */
	(char *)0, (struct vfsops *)0,  /* 9 = runtime expansion */
};

#define	NVFS	(sizeof vfssw / sizeof vfssw[0])

struct vfssw *vfsNVFS = &vfssw[NVFS];

struct bootobj rootfs;
#if	! MACH
struct bootobj rootfs = {
	{ "4.3",           "",     0, 0, (struct vnode *)0 },
};
struct bootobj dumpfile = {
	{ "",           "",     0, 0, (struct vnode *)0 },
};
#endif	! MACH


