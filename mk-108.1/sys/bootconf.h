/*	@(#)bootconf.h	2.1 88/05/18 NFSSRC4.0 from 1.1 87/01/12 SMI	*/

/*
 * HISTORY
 *
 * 19-Dec-88  Peter King (king) at NeXT
 *	From NFSSRC4.0.  Ported to Mach
 */

/*
 * Boot time configuration information objects
 */

#define	MAXFSNAME	16
#define	MAXOBJNAME	128
/*
 * Boot configuration information
 */
struct bootobj {
	char	bo_fstype[MAXFSNAME];	/* filesystem type name (e.g. nfs) */
	char	bo_name[MAXOBJNAME];	/* name of object */
	int	bo_flags;		/* flags, see below */
	int	bo_size;		/* number of blocks */
	struct vnode *bo_vp;		/* vnode of object */
};

/*
 * flags
 */
#define	BO_VALID	0x01		/* all information in object is valid */
#define	BO_BUSY		0x02		/* object is busy */

extern struct bootobj rootfs;
#if	MACH
extern struct bootobj privatefs;
#else	MACH
extern struct bootobj dumpfile;
extern struct bootobj argsfile;
extern struct bootobj swaptab[];
extern int Nswaptab;
#endif	MACH
