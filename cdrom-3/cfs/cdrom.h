/* Configuration information for CFS filesystem */

#define	CDROM_BLOCK_SIZE	2048

#define	CFS_DOWNCASE			/* make all filenames to lowercase */
#define CFS_MAPREV			/* map ';' to '%' in file names	   */
#define CFS_NOREV1			/* drop ;1 at end of file names	   */
#define CFS_CACHESIZE	8192		/* size of cache entries 	   */
#define	CFS_CACHESLOTS	4		/* slots in the cache.  Make small */
#define CFS_MAXMOUNTS	8		/* maximum number of mounted fs    */

/* following for POSIX extensions */
#ifdef POSIX_EXTENSIONS
#define	CFS_MAXSYMSIZE	128		/* maximum symbolic link length	   */
#endif

/* The CDROM package implements a streams abstraction on the CDROM.
 * We call them CDFILEs.
 *
 */

/* File System types */
typedef enum {
	ISO_9660	= 0,
	High_Sierra	= 1,
	LEN		= 2,			/* length of fields in array*/
	Unknown		= -1000
} fs_typ;


/* an extent is a starting location on a disk and a length
 * in bytes.  ISO 9660 specifies that all extents begin on block boundries.
 *
 * Additional fields are provided for POSIX extentions
 */
typedef struct {
	struct vnode 	*e_vp;			/* device vnode		*/
	enum vtype	e_type;			/* vnode type of extent	*/
	long 		e_start;		/* start of extent 	*/
	long 		e_bytes;		/* number of bytes in extent*/
	struct timeval	e_ctime;		/* time extent was created */

	/* The following are for POSIX extensions */
	struct timeval	e_atime;
	struct timeval	e_mtime;
	int		e_uid;
	int		e_gid;
	int		e_mode;
	int		e_nlink;
	int		e_rdev;			/* major & minor device */

	/* The follow are only used for POSIX, and they are so big
	 * that we normally won't compile them in.
	 */
#ifdef POSIX_EXTENSIONS
	char		e_symlink[CFS_MAXSYMSIZE];
#endif
} extent;


typedef struct {
	int		flags;
	extent		ex;			/* extent we are reading*/
	long		pos;			/* next byte to read 	*/
} CDFILE;



/* Flags for cdf_open */
#define	CDF_KERNEL	0x0001			/* xfer to Kernel address */
#define	CDF_PAGER	0x0002			/* xfer with Mach pager */
#define CDF_UIO		0x0004			/* xfer with uiomove 	*/
#define	CDF_NOCACHE	0x0008			/* don't age cache on hit*/


/* cdrom.c */
void	cache_free(void);
int	cdf_open(extent *ex,CDFILE *cdf,long offset,int flags);
int	cdf_read(CDFILE *cdf,unsigned bytes,void *buf,u_long *actual);
int	cdf_release(CDFILE *cdf);


/* unix_rw.c */
int	unix_open(struct vnode *vp);
void	unix_close(struct vnode *vp);
int	unix_read(struct vnode *vp,int block,int block_count,char *addrs);


/* Simson's Debugging macros */

#ifdef 	DEBUG
#define	dprint(x) printf(x)
#define dprint1(x,y) printf(x,y)
#define dprint2(x,y,z) printf(x,y,z)
#define dprint3(x,y,z,a) printf(x,y,z,a)
#define dprint4(x,y,z,a,b) printf(x,y,z,a,b)

#else
#define dprint(x) {}
#define dprint1(x,y) {}
#define dprint2(x,y,z) {}
#define dprint3(x,y,z,a) {}
#define dprint4(x,y,z,a,b) {}
#endif

