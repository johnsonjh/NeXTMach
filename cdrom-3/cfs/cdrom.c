/* cdrom.c:
 *
 * Actual cfs routines to decode information on the CDROM and change
 * them into vnodes.
 *
 * This module supports both ISO 9660 and High Sierra file systems
 * (C) 1990, Simson L. Garfinkel
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/dir.h>
#include <sys/mount.h>

#include <kernserv/kern_server_types.h>

#include <errno.h>
#include <tzfile.h>

#include "vm/vm_page.h"
#include "kern/kalloc.h"
#include "kern/lock.h"


char 	*asctime();

#include "cdrom.h"
#include "cfs.h"

void bcopy1(char *x,char *y,int z){bcopy(x,y,z);}
void bcopy2(char *x,char *y,int z){bcopy(x,y,z);}


kern_server_t  cfs_ks_var;			/* needed by kern server */
char	namemap[255];				/* filename mapping	 */


/* ISO9660 and High Sierra Standard */
enum {
	VOLUME_DESCRIPTOR_SET_START	= 16
  };

typedef long std[3];				

/*      VOLUME DESCRIPTOR FIELD NAME 	   ISO,  HS, LEN
 */
std	system_identifier 		= {  8,  16,  32};
std	volume_identifier 		= { 40,  48,  32};
std	volume_set_identifier		= {190, 214, 128};
std	publisher_identifier		= {318, 342, 128};
std	data_preparer_identifier	= {446, 470, 128};
std	application_identifier		= {574, 598, 128};
std	copyright_file_identifier	= {702, 726,  36};
std	abstract_file_identifier	= {739, 758,  36};
std	bibliographic_file_identifier	= {776,   0,  36};
std	root_dir_loc			= {156, 180,  34};

/* 	DIRECTORY RECORD FIELD NAME	   ISO,  HS, LEN
 */
std	LEN_DR				= {  0,   0,   1};
std	extended_attribute_rec		= {  1,   1,   1};
std	location			= {  2,   2,   4}; /* lsb first */
std	byte_count			= { 10,  10,   4}; /* lsb first */
std	cd_year				= { 18,  18,   1}; /* since 1900 */
std	cd_month			= { 19,  19,   1}; /* 1..12 */
std	cd_day				= { 20,  20,   1}; /* 1..31 */
std	cd_hour				= { 21,  21,   1}; /* 0..23 */
std	cd_minute			= { 22,  22,   1}; /* 0..59 */
std	cd_second			= { 23,  23,   1}; /* 0..59 */
std	gmt_offset			= { 24,   0,   1}; /* in 15 minutes */
std	file_flags			= { 25,  24,   1};
std	LEN_FI				= { 32,  32,   1};
std	file_identifier			= { 33,  33,   1};

/* blong():
 *
 * Convert a 4-byte sequence stored on a CDROM to a long variable.
 */

static	long	blong(unsigned char *x)
{
	return(*x | (*(x+1))<<8 | (*(x+2))<<16 | (*(x+3))<<24);
}


/****************************************************************
			     EXTENT CACHE
 ****************************************************************/

/*
 * The extent cache is a list of CFS_CACHESLOTS records, each one with
 * a buffer that is CFS_CACHESIZE in length.  When cdf_bufread() is called,
 * it actually calls cdf_cachefill to get the bytes that it needs.
 */

struct cache {
	lock_t	c_lock;			/* lock on this cache slot	*/
	long	c_flags;		/* flags --- see below		*/
	extent	c_ext;			/* extent 			*/
	u_long	c_offset;		/* byte offset in cache	 	*/
	u_long	c_bytes;		/* bytes in cache 		*/
	int	c_refcount;		/* number of people who have ref*/
	long	c_time;			/* time when last used 		*/
	char	c_data[CFS_CACHESIZE];	/* the actual cache		*/
};
  
/* Flags */
#define	C_ERROR 	0x0001			/* error on read */

int	cache_time	= 0;			/* incremental time */
struct	cache *cache;

/* cache_init()
 *
 * Allocate the memory for the cace and initialize the locks.
 */
void	cache_init(void)
{
	int	i;
	
	cache	= kalloc(sizeof(struct cache) * CFS_CACHESLOTS);
	bzero(cache,sizeof(cache));
	for(i=0;i<CFS_CACHESLOTS;i++){
		cache[i].c_lock	= lock_alloc();
		lock_init(cache[i].c_lock,TRUE);
	}
}

/* cache_free()
 *
 * Free the space used by the cache.
 */

void	cache_free(void)
{
	int	i;

	for(i=0;i<CFS_CACHESLOTS;i++){
		lock_free(cache[i].c_lock);
	}
	kfree(cache,sizeof(struct cache) * CFS_CACHESLOTS);
}

/* cache_clear()
 *
 * Called when a new CDROM is mounted; this function clears
 * all of a cache entries associated with the physical device.
 */
void	cache_clear(struct vnode *vp)
{
	int	i;

	for(i=0;i<CFS_CACHESLOTS;i++){
		lock_write(cache[i].c_lock);
		if(cache[i].c_ext.e_vp == vp){

			/* Nobody should be using this slot */
			assert(cache[i].c_refcount==0);

			/* And make sure we won't accidently match */
			cache[i].c_bytes	= 0;
			cache[i].c_time		= 0;
			cache[i].c_refcount	= 0;
		}
		lock_done(cache[i].c_lock);
	}
}

/* cache_find_oldest()
 *
 *	Find the oldest unused element in the cache to discard it.
 *	If we cannot find anything in the cache, block, until there
 *	is something that is oldest.
 *
 *	Return with the oldest one locked.
 */

static	int	cache_find_oldest()
{
	long	oldtime=0;
	int	oldest = -1;
	
	while(oldest==-1){
		int	i;

		/* Lock the entire cache */
		for(i=0;i<CFS_CACHESLOTS;i++){
			lock_write(cache[i].c_lock);
		}

		for(i=0;i<CFS_CACHESLOTS;i++){
			if(cache[i].c_refcount==0
			   && ((cache[i].c_time < oldtime) || (oldest==-1))){
				
				oldest	= i;
				oldtime	= cache[i].c_time;
			}
		}

		/* Unlock all of the ones except the one we need... */
		for(i=0;i<CFS_CACHESLOTS;i++){
			if(i!=oldest) lock_done(cache[i].c_lock);
		}

		if(oldest==-1){			/* wait for an empty slot */
			assert_wait(cache,0);
			thread_block();
		}
	}
	return(oldest);
}



/* cache_read():
 *
 * Return a pointer to the cache element that contains the requested
 * extent.  If the extent is not in a cache, flush the cache and
 * find it.
 */

static	struct	cache	*cache_read(extent *ext,u_long offset,u_long bytes)
{
	int	i,oldest;
	int	blockoff;
	int	blocks;

	/* Determine the maximum number of bytes we can read.
	 */
	bytes	= MIN(CFS_CACHESIZE,MIN(bytes,ext->e_bytes)); 


	/* See if we can find a matching extent
	 * already in the cache.
	 */

	for(i=0;i<CFS_CACHESLOTS;i++){

		lock_read(cache[i].c_lock);
		
		if(bcmp(&cache[i].c_ext,ext,sizeof(*ext))==0
		   && cache[i].c_offset <= offset
		   && cache[i].c_offset + cache[i].c_bytes  >= offset + bytes){

			/* Increment the reference count on this slot.
			 */

			cache[i].c_refcount++;	

			lock_done(cache[i].c_lock);
			return(&cache[i]);
		}
		lock_done(cache[i].c_lock);
	}

	/* No matching cache.
	 * Find the oldest and get a lock.  This function blocks
	 * if there is no oldest...
	 */
	oldest	= cache_find_oldest();

	/* Slot oldest can be used for the cache.
	 * The flags variable is already blocked.
	 */

	blockoff		= offset / CDROM_BLOCK_SIZE;

	/* do not alter lock variable */
	cache[oldest].c_flags	= 0;
	cache[oldest].c_ext	= *ext;
	cache[oldest].c_offset	= blockoff * CDROM_BLOCK_SIZE;
	cache[oldest].c_bytes	= MIN(CFS_CACHESIZE,ext->e_bytes - offset);
	cache[oldest].c_refcount++;

	/* Now read the data into the buffer.
	 * Round up the number of bytes in the cache to the nearest block.
	 */
	blocks			= (cache[oldest].c_bytes +
				   CDROM_BLOCK_SIZE - 1) / CDROM_BLOCK_SIZE;
	cache[oldest].c_bytes	= blocks * CDROM_BLOCK_SIZE;

	if(unix_read(ext->e_vp,ext->e_start + blockoff,blocks,
		     cache[oldest].c_data)){
		cache[oldest].c_flags	|= C_ERROR;
	}
	lock_done(cache[oldest].c_lock);
	return(&cache[oldest]);
}

/* cache_release()
 * Release a cache entry.  If this is the last reference to this
 * cache, update its time and wakeup anybody who may be sleeping for a
 * cache slot...
 */
static	void	cache_release(CDFILE *cdf,struct cache *cp)
{
	lock_write(cp->c_lock);
	if(--cp->c_refcount==0){

		/* Update the use time if we should cache this one */
		if((cdf->flags & CDF_NOCACHE)==0){
			cp->c_time	= cache_time++;
		}
		thread_wakeup(cache);
	}

	lock_done(cp->c_lock);
}




/****************************************************************
  DISK DECODING FUNCTIONS
 ****************************************************************/
  


/* cdrom_mktime()
 *	Because the libc mktime is not available inside the kernel.
 *	Also save ourself the offtime() translation
 */
static int	mon_lengths[2][MONS_PER_YEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int	year_lengths[2] = {
	DAYS_PER_NYEAR, DAYS_PER_LYEAR
};

/*
 * mktime converts a localtime back to a single
 * calendar time value, allowing alterations to
 * the tm_year, tm_mon, tm_mday, tm_hour, tm_min and tm_sec fields.
 * The values in these fields can be out of range, so long
 * as the resulting calendar time fits in a positive time_t
 *
 * NeXT, Inc, 6-Dec-88 CCH
 */
 
static	time_t	cdrom_mktime(struct tm *tmp)
{
	time_t 	stamp;
	long 	days;
	int 	month, year, y;
	int 	*ip;

	year 	= tmp->tm_year + TM_YEAR_BASE;
	month 	= tmp->tm_mon;
	if (month < 0) {
	    year -= (11-month) / 12;
	    month = 11 - ((11-month) % 12);
	} else {
            year += month / 12;
	    month = month % 12;
	}
	days = (long) tmp->tm_mday - 1;
	ip = mon_lengths[isleap(year)];
	while (month>0) {
	    month--;
	    days += ip[month];
	}
	y = EPOCH_YEAR;
	while (y < year) {
	      days += (long) year_lengths[isleap(y)];
	      y++;
	}
	while (y > year) {
	      y--;
	      days -= (long) year_lengths[isleap(y)];
	}
        stamp = tmp->tm_sec
	      + SECS_PER_MIN*tmp->tm_min
	      + SECS_PER_HOUR*tmp->tm_hour
	      + SECS_PER_DAY*days
	      /* - tmp->tm_gmtoff */;

	return(stamp);
}

/* dir_2_extent:
 * The function changes a CDROM directory type into an "extent,"
 * which are the CFS analogs of inodes.  Extents are kept in the cnodes.
 *
 * Note that it is necessary to know the filesystem type.
 * Conversion alg. is based on date.c.  
 */
static	void	dir_2_extent(unsigned char *dir,
			     struct mntinfo *mi,extent *ext)
{
	struct	tm tm;
	int	idx;
	
	idx		= mi->fstype;			/* shorthand index */

	ext->e_vp	= mi->mi_devvp;		/* get device number */
	ext->e_start 	= blong(dir+location[idx]);
	ext->e_bytes 	= blong(dir+byte_count[idx]);
	ext->e_type	= (dir[file_flags[idx]] & 0x02) ? VDIR : VREG;

	/* Determine time of entry */

	bzero(&tm,sizeof(tm));
	tm.tm_sec	= dir[cd_second[idx]];
	tm.tm_min	= dir[cd_minute[idx]];
	tm.tm_hour	= dir[cd_hour[idx]];
	tm.tm_mday	= dir[cd_day[idx]];
	tm.tm_mon	= dir[cd_month[idx]];
	tm.tm_year	= dir[cd_year[idx]];

	ext->e_ctime.tv_sec	= cdrom_mktime(&tm);
	ext->e_ctime.tv_usec	= 0;

	/* Set default values for additional fields used by POSIX extensions */
	ext->e_mode	= ext->e_type == VDIR ? 0040555 : 0100444;
	ext->e_uid	= ext->e_vp->v_vfsp->vfs_uid;  
	ext->e_gid	= -2;
	ext->e_nlink	= 1;
	ext->e_rdev	= ext->e_vp->v_rdev;
	ext->e_mtime	= ext->e_ctime;
	ext->e_atime	= ext->e_ctime;
}


#ifdef FULL_ISO
/* Function to copy out the field from the CDROM header,
 * hacking off the spaces.
 */

static	void	iso_cpy(char *dest,char *source,int len)
{
	strncpy(dest,source,len);		/* copy the data */
	dest[len] = 0;				/* null terminate it */
	while(len>0 && dest[len-1]==' '){	/* while we have white space*/
		dest[--len] = 0;		/* eat the blanks behind us */
	}

}
#endif


/* cdrom_mount:
 *
 * Try to mount the CDROM.
 * See what kind of CDROM it is an set up global variables.
 *
 * Return 0 if successful, errno if not.
 */

int	cdrom_mount(struct vnode *devvp,struct mntinfo *mi)
{
	struct	volume_descriptor *vd;
	long	block 	= VOLUME_DESCRIPTOR_SET_START;
	int	idx;
	char	*data;
	
	dprint("   cdrom_mount()\n");

	/* allocate memory for a block */

	data	= kalloc(CDROM_BLOCK_SIZE);

	cache_clear(devvp);	/* Invalidate the cache for this vnode */

	/* Try to read the first block */
	
	if(unix_read(devvp,block,1,data)){
		kfree(data,CDROM_BLOCK_SIZE);
		return(EIO);
	}


	/* Set up the mount info a little */

	mi->mi_devvp	= devvp;
	mi->fstype	= Unknown;		/* unknown file system */
	mi->mi_refct	= 0;			/* nobody has it */

	
	/* Check to see if this is a High Sierra disk */
	if(!strncmp(data+9,"CDROM",5)){
		mi->fstype	= High_Sierra;
		dprint("   Disk is High Sierra\n");
	}
	
	/* Check to see if this is an ISO disk */
	if(!strncmp(data+1,"CD001",5)){

		/* Now scan for the Primary Volume Descriptor.  If the current
		 * block isn't one, scan until we find one or until we run out
		 * of volume descriptors.  If we run out,
		 * we can't mount the disk.
		 */
		while(!strncmp(data+1,"CD001",5)
		      && data[6]!='\001'){

			if(unix_read(devvp,++block,1,data)){
				kfree(data,CDROM_BLOCK_SIZE);
				return(EIO);
			}

			vd	= (struct volume_descriptor *)data;
		}
		mi->fstype	= ISO_9660;
		dprint1("   Disk is ISO 9660.  PVD at %d\n",block);
	}

	if(mi->fstype == Unknown){
		printf("   cdrom: Unknown disc type.  Mount aborted.\n");
		kfree(data,CDROM_BLOCK_SIZE);
		return(EINVAL);
	}

	idx		= mi->fstype;

#ifdef FULL_ISO
	/* Copy out the fields that we want for mount info */
#define copy(miname,stdname) if(stdname[idx]!=0) \
	iso_cpy(mi->miname,data+stdname[idx],stdname[LEN]);

	copy(system_id,system_identifier);
	copy(volume_id,volume_identifier);
	copy(volset_id,volume_set_identifier);
	copy(publisher_id,publisher_identifier);
	copy(dataprep_id,data_preparer_identifier);
	copy(application_id,application_identifier);
	copy(copyright_file,copyright_file_identifier);
	copy(abstract_file,abstract_file_identifier);
	copy(bibliographic_file,bibliographic_file_identifier);
#endif
	
	/* Make up the root directory extent */
	dir_2_extent((unsigned char *)data+root_dir_loc[idx],mi,&mi->root_dir);

	kfree(data,CDROM_BLOCK_SIZE);
	return(0);
}


/****************************************************************
			     CDF ROUTINES
 ****************************************************************/

/* CDF is a block-to-bytestream translation protocol for the cache.
 */

/* cdf_open:
 *	Set up a CDFILE to the a place in an extent that is pointed to by a
 *	cnode pointer.
 *
 *	Flags should only specify Kernel, Pager or UIO 
 */

int 	cdf_open(extent *ex, CDFILE *cdf,long offset,int flags)
{
	bzero(cdf,sizeof(*cdf));		/* clear out state */

	cdf->ex		= *ex;			/* copy extent */
	cdf->pos	= offset;		/* Requested start */
	cdf->flags	= flags;
	return(0);				/* successful open */
}

/* cdf_read:
 * 	Read a number of bytes from the CDF to a given place in memory.
 *	Advance the pointer.  If necessary, free the current block and read
 *	the next block.
 *	Returns 0 if successful, -1 if failure.
 */

int	cdf_read(CDFILE *cdf,unsigned bytes,void *buf,u_long *actual)
{
	char		*kmem 	= 0;	/* possible transfer addresses */
	vm_page_t	vm 	= 0;
	struct uio	*uio 	= 0;

	assert(bytes<=65536);			/* some error checking	*/

	if(cdf->flags & CDF_PAGER){
		vm	= (vm_page_t)buf;	/* convert pointer type */
	}
	if(cdf->flags & CDF_KERNEL){
		kmem	= (char *)buf;
	}
	if(cdf->flags & CDF_UIO){
		uio	= (struct uio *)buf;
	}

	*actual	= 0;
	while(bytes > 0 && cdf->ex.e_bytes > cdf->pos){
		/* Determine how many bytes we can transfer
		 * Do not transfer more bytes then are in the block
		 * or the extent.
		 */

		int 	xfer	= bytes;
		struct	cache	*cp;
		char	*cdata;

		/* Compute how many bytes left in extent */

		if(xfer > cdf->ex.e_bytes-cdf->pos){
			xfer = cdf->ex.e_bytes - cdf->pos; 
		}


		/* Now get the cache block */
		cp	= cache_read(&cdf->ex,cdf->pos,xfer);
		

		if(cp->c_flags & C_ERROR){
			cache_release(cdf,cp);
			return(EIO);
		}

		/* Determine how many bytes in cache we can read */
		if(xfer + cdf->pos > cp->c_offset + cp->c_bytes){
			xfer = cp->c_offset + cp->c_bytes - cdf->pos;
		}

		/* Find where our data starts in the cache */
		cdata	= cp->c_data + (cdf->pos - cp->c_offset);

		/* Transfer the bytes and update our pointers.
		 *
		 */

		if(cdf->flags & CDF_KERNEL){
			bcopy1(cdata,kmem + *actual, xfer);

		}
		if(cdf->flags & CDF_PAGER){
			copy_to_phys(cdata,
				     VM_PAGE_TO_PHYS(vm) + *actual,
				     xfer);
		}
		if(cdf->flags & CDF_UIO){
			int	error;

			error	= uiomove(cdata,
					  xfer, UIO_READ, uio);
			if(error){
				return(error);
			}
		}

		cache_release(cdf,cp);	/* don't need any more */

		*actual		+= xfer;/* increment count transfered 	*/
		cdf->pos	+= xfer;/* increment position in extent */
		bytes		-= xfer;/* decrement bytes to transfer */
	}
	return(0);		/* successful */
}


/****************************************************************
			  DIRECTORY ROUTINES
 ****************************************************************/

/* Partially interperted High Sierra / ISO9660 directory record */
struct dir_rec {
	int		len_dr;		/* length of directory record */
	extent		ext;		/* extent for directory */
	int		len_fi;		/* length of the file identifier */
	char		file_identifier[222];	/* maximum file size */
};

#ifdef POSIX_EXTENSIONS
/* POSIX EXTENSIONS, by Sun Microsystems and modified by Young Minds,
 *	are implemented by a series of "decode" functions that each
 *	know how to decode a particular kind of field.  If the function
 *	finds its field, it decodes it, and increments the buffer pointer
 *	to point to the beginning of the next field.  If the function
 *	does not know how to decode the field, it simply does nothing.
 */

/* Unix Extensions */

static	ux_decode(unsigned char **buf,extent *ext)
{
	if((*buf)[0]=='U' && (*buf)[1]=='X'
	   && (*buf)[2]==36 && (*buf)[3]==1){ /* UX version 1 */

		printf("ux decocode\n");

		ext->e_mode	= blong((*buf)+4);
		ext->e_nlink	= blong((*buf)+12);
		ext->e_uid	= blong((*buf)+20);
		ext->e_gid	= blong((*buf)+28);
		*buf		+= 36;	/* go to next field */
		return(1);
	}
	return(0);
}

/* Symbolic Link */

static	sl_decode(unsigned char **buf,extent *ext)
{
	int	i;

	if((*buf)[0]=='S' && (*buf)[1]=='L' && (*buf)[3]==1){
		printf("sl decode\n");

		for(i=0;i<(*buf)[2]-4;i++){
			ext->e_symlink[i]	= (*buf)[i+4];
		}
		ext->e_symlink[i]	= '\000';
		ext->e_type		= VLNK;
		*buf	+= (*buf)[2];
		return(1);
	}
	return(0);
}

/* Unix Device */

static	ud_decode(unsigned char **buf,extent *ext)
{
	if((*buf)[0]=='U' && (*buf)[1]=='D'
	   && (*buf)[2]==20 && (*buf)[3]==1){	/* UD version 1 */
		printf("ud decode\n");

		ext->e_rdev	= (  (blong((*buf)+4) << 2)
				   | (blong((*buf)+12)));
		*buf	+= 20;
		return(1);
	}
	return(0);
}

/* Time */

static	ti_decode(unsigned char **buf,extent *ext)
{
	if((*buf)[0]=='T' && (*buf)[1]=='I'
	   && (*buf)[2]==39 && (*buf)[4]==1){	/* TI version 1 */

		printf("ti decode\n");

		ext->e_ctime.tv_sec	= blong((*buf)+4);
		ext->e_ctime.tv_usec	= 0;

		ext->e_mtime.tv_sec	= blong((*buf)+11);
		ext->e_mtime.tv_usec	= 0;


		ext->e_atime.tv_sec	= blong((*buf)+32);
		ext->e_mtime.tv_usec	= 0;

		*buf	+= 39;
		return(1);
	}
	return(0);
}

/* Name identifier */

static	nm_decode(unsigned char **buf,struct dir_rec *dr)
{
	int	i;

	if((*buf)[0]=='N' && (*buf)[1]=='M' && (*buf)[3]==1){

		printf("nm decode\n");
		
		for(i=0;i<(*buf)[2]-4;i++){
			dr->file_identifier[i]	= (*buf)[i+4];
		}
		dr->file_identifier[i]	= '\000';
		*buf	+= (*buf)[2];
		return(1);
	}
	return(0);
}


#endif


/* cdrom_readdir_elem():
 *
 * Read an element from the directory.
 * Uses a CDFILE, and reads at that location.
 * Returns the offset of the next element in the directory.
 *
 * Returns ERRNO if there is an error, -1 if end of dir, 0 if no error.
 *
 */

static	int	cdrom_readdir_elem(struct mntinfo *mi,
				   CDFILE *cdf,struct dir_rec *dr,
				   off_t *offset)
{
	u_long	bytes;
	int	idx 	= mi->fstype;
	int	i;
	char	*fid 	= dr->file_identifier;	/* shorthand */
	unsigned char	*buf;			/* entry off disk */
	int	ret	= 0;			/* return code */

	dprint1("   cdrom_readdir_elem(cdf->pos=%d)  ",cdf->pos);
	buf	= kalloc(256);
	do{
		if(cdf_read(cdf,1,buf,&bytes)){	/* read length of the entry */
			ret	= EIO;
			goto	done;
		}
		
		if(bytes==0){
			ret	= -1;		/* end of extent */
			goto	done;
		}

		if(buf[0]==0){
			/* End of directory entries in this block (see 6.8.1.1)
			 * Advance "offset" and the CDFILE to the beginning
			 * of the next block
			 * of the CDROM.
			 * Then loop, trying to read the beginning of the
			 * next block.
			 * If we have gone too far, that will return -1.
			 */
			int	blkoff;		/* new block offset  */

			dprint("   cdrom_readdir_elem: block skip code\n");

			blkoff		= ((cdf->pos + CDROM_BLOCK_SIZE) /
					   CDROM_BLOCK_SIZE);
			cdf->pos	= blkoff * CDROM_BLOCK_SIZE;
			*offset		= blkoff * CDROM_BLOCK_SIZE;
		}
	}while(buf[0]==0);		/* repeat till we have a valid entry */

	/* read rest of dir entry */
	if(cdf_read(cdf,((unsigned)buf[0])-1,buf+1,&bytes)){ 
		ret	= EIO;
		goto	done;
	}

	dr->len_dr	= buf[LEN_DR[idx]];
	if(dr->len_dr==0){		/* 0 length.  must be end of dir */

		ret	= -1;
		goto	done;
	}
	dr->len_fi	= buf[LEN_FI[idx]];
	dir_2_extent(buf,mi,&dr->ext);
	

	/* If this is the first or second entry in the directory,
	 * fake the name to be "." and ".." respectively.
	 *
	 * We can do this because the ISO standard (and High Sierra)
	 * fortunately put "." and ".." in all subdirectories
	 *	--- even the root directory.
	 *
	 *
	 * Otherwise, just take the real name
	 */
	switch(*offset){
	      case 0:
		strcpy(fid,".");
		dr->len_fi	= 1;
		break;

	      case 34:				/* high sierra */
	      case 42:				/* ISO 9660 */
		strcpy(fid,"..");
		dr->len_fi	= 2;
		break;

	      default:

		/* Translate and map the file name */
		for(i=0;i<dr->len_fi;i++){
			fid[i]	=
			  namemap[buf[i+file_identifier[idx]]];
		}
#ifdef CFS_NOREV1
		/* Change "filename.bat;1" into "filename.bat" */
		   
		if(dr->len_fi>2
		   && fid[dr->len_fi-1]=='1'
		   && fid[dr->len_fi-2]==namemap[';']){

			dr->len_fi -= 2;	/* drop ;1 */
		}
#endif

		fid[dr->len_fi]	= 0;
		break;
	}

	*offset += dr->len_dr;			/* add to offset	*/

	dprint3("   len_dr=%d  len_fi=%d fn=%s\n",
		dr->len_dr,dr->len_fi,dr->file_identifier);

	/* If any of these fields are out of range, return an error */
	if(dr->len_dr < 34){
		dprint("      >>> invalid directory read\n");
		ret	= EIO;
		goto	done;
	}

#ifdef POSIX_EXTENSIONS
	if(idx==ISO_9660){
		unsigned char *sua;
		int	i;

		/* Check end of directory record for POSIX_EXTENSIONS
		 * System Use Area and decode it if possible
		 * Decode until we have no more fields left.
		 *
		 * A bug with the young minds disks is that dr->len_fi
		 * is not right.  We avoid this by starting at the
		 * file_identifier, scanning for a 0 byte, then scanning
		 * for a non-zero byte (if we don't reach the end of
		 * the directory record.)
		 */
		sua	= buf+file_identifier[idx]+dr->len_fi;

		while(*sua && (*sua==';' || (*sua>='0' && *sua<='9'))
			       && (sua < buf+dr->len_dr)){
			sua++;			/* skip version number */
		}
		while(*sua==0 && sua < buf+dr->len_dr){
			sua++;			/* skip past padding */
		}
		while(*sua && sua < buf+(unsigned)(buf[0])){
			if(ux_decode(&sua,&dr->ext)==0
			   && sl_decode(&sua,&dr->ext)==0
			   && ud_decode(&sua,&dr->ext)==0
			   && ti_decode(&sua,&dr->ext)==0
			   && nm_decode(&sua,dr)==0){
				goto	done;	/* can't decode this*/
			}
		}
	}

#endif POSIX_EXTENSIONS
     done:
	kfree(buf,256);
	return(ret);				/* okay */
}



/*
 * cdrom_readdir:
 *
 * Process a read directory request.  Offset is the location to start reading.
 * Return 0 for success, -1 for end of directory, and errno for error
 * 
 *
 * If offset is negative, it is the directory entry number.  You must
 * rewind to the beginning and start reading.  Sigh.  This is here for NFS.
 */

int	cdrom_readdir(struct cnode *cp,
		      off_t *offset,	/* offset --- updated 		*/
		      u_long *bytes,	/* # wanted --- updated to # gotten */
		      char *buf)	/* the buffer 			*/
{
	CDFILE	cdf;			/* buffer structure 		*/
	struct	dir_rec *dr	 = 0;	/* returned from cdrom_readdir_elem */
	struct	direct	*dp;		/* pointer into buf[] 		*/
	int	max		= *bytes;/* get maximum number of bytes */
	struct 	mntinfo *mi 	= vtomi(ctov(cp)); /* get mount info */
	int	ocounting	= 0;	/* offset was a count		*/
	int	ocount		= 0;	/* the actual count		*/
	int	ret		= 0;	/* return code */


	if(*offset & OFFSET_IS_COUNT){
		ocounting	= 1;
		ocount		= *offset & OFFSET_MASK;
		*offset		= 0;		/* start at 0 */
	}

	*bytes	= 0;				/* bytes we have put in buf */
	
	if(ctov(cp)->v_type!=VDIR){		

		/* Node must be a directory */
		return(ENOTDIR);
	}

	/* Open the directory extent and start reading from it */

	if(cdf_open(ctoext(cp),&cdf,*offset,CDF_KERNEL)){
		return(EIO);
	}

	dr = kalloc(sizeof(struct dir_rec));
	if(ocounting){				/* if we are counting */
		int	i;

		for(i=0;i<ocount;i++){
			
			if((ret=cdrom_readdir_elem(mi,&cdf,dr,offset))!=0){
				goto	error;
			}
		}
	}

	while(1){
		if((ret=cdrom_readdir_elem(mi,&cdf,dr,offset))!=0){
			break;
		}

		ocount++;		/* increment to next count */

		/* Check to see if there is room for this entry in
		 * the buffer */

		dp		= (struct direct *) (buf + (*bytes));
		dp->d_fileno	= dr->ext.e_start;
		dp->d_namlen	= strlen(dr->file_identifier);
		dp->d_reclen	= DIRSIZ(dp);	

		/* If we do not have room for the name,
		 * then we ran out of room */

		if(DIRSIZ(dp) + *bytes >= max){
			dp->d_reclen	= 0;	/* zero record */
			break;
		}
		
		/* Copy in the name now */
		bcopy2(dr->file_identifier,dp->d_name,dp->d_namlen+1);

		*bytes	+= dp->d_reclen;/* increment record length */
		if(*bytes + 8 >= max){	/* not enough room for next header? */
			break;
		}
	}
      error:;
	if(ocounting){
		*offset	= ocount;
	}
	kfree(dr,sizeof(struct dir_rec));
	return(ret);
}




/* cdrom_lookup()
 *
 * Lookup a name in a directory.  Return the extent when you find it.
 * Return ERRNO if error.  To avoid having to go to the beginning of the
 * directory, first try to read at "cp->nextdirread" If this fails, go to
 * the beginning.  If not, we win.  Doing this makes stating every file
 * in a directory an O(n) proposition, instead of O(n*n).
 *
 * There is a possibility that cp->nextdirread should be "locked" while
 * it is being accessed or changed.  On the other hand, I'm not sure if
 * that is necessarily a problem.  As long as the changes are monotomic.
 * You will either get the old value or the new value, and this is fine.
 */

int	cdrom_lookup(struct cnode *cp,char *name,extent *ex)
{
	CDFILE	cdf;
	int	ret;
	off_t	offset 		= 0;
	struct 	mntinfo *mi 	= vtomi(ctov(cp));	/* get mount info */
	struct	dir_rec *dr 	= 0;

	dprint2("   cdrom_lookup(cp=%x,name=%s)\n",cp,name);

	if(ctov(cp)->v_type!=VDIR){	/* check to make sure it is a dir */
		dprint("   return(ENOTDIR)\n");
		return(ENOTDIR);
	}

	/* First try to read at the next position */
	dprint2("        cp->nextdirread=%d  cp->c_ext.e_bytes=%d\n",
		cp->nextdirread,cp->c_ext.e_bytes);
	
	dr	= kalloc(sizeof(struct dir_rec));

	if(cp->nextdirread > 0 && cp->nextdirread < cp->c_ext.e_bytes){
		off_t	toffset = cp->nextdirread;

		dprint("   checking next entry\n");

		if(cdf_open(ctoext(cp),&cdf,toffset,CDF_KERNEL)){

			ret	= EIO;
			goto	finished;
		}
		if(cdrom_readdir_elem(mi,&cdf,dr,&toffset)==0){
			dprint2("   got %s  wanted %s\n",
				dr->file_identifier,name);
				
			if(!strcmp(dr->file_identifier,name)){

				/* Found entry with quick lookup */

				*ex = dr->ext;
				cp->nextdirread	= toffset;
				dprint1("   found1.  ex->e_start=%d\n",
					ex->e_start);

				ret	= 0;
				goto	finished;
			}
		}
		/* Short check failed
		 * Have to read from the beginning of the directory.
		 */
	}

	if(cdf_open(ctoext(cp),&cdf,offset,CDF_KERNEL)){ /* open extent */
		dprint2("   cdrom_lookup(cp=%x,name=%d) ",cp,name);

		ret	= EIO;
		goto	finished;
	}

	while((ret=cdrom_readdir_elem(mi,&cdf,dr,&offset))==0){
		if(!strcmp(dr->file_identifier,name)){

						/* Found the entry.  	*/
			*ex	= dr->ext;
			cp->nextdirread=offset;	/* next time, start here for*/
						/* increased gas milage */

			dprint1("   found2.  ex->e_start=%d\n",ex->e_start);

			ret	= 0;		/* success */
			goto	finished;
		}
	}

	cp->nextdirread=0;			/* start at 0 next time */
	if(ret==-1){				/* reached end of directory */
		dprint("   return(ENOENT)\n");

		ret	= ENOENT;
		goto	finished;
	}

      finished:;

	kfree(dr,sizeof(struct dir_rec));
	return(ret);				/* not found */
}



/****************************************************************
				 INIT
 ****************************************************************/

void	cdrom_init()
{
	int	i;


	dprint("   cdrom_init()\n");
	
	cache_init();

	/* Establish name translation */
	for(i=0;i<255;i++){
		namemap[i]	= i;
	}

#ifdef CFS_MAPREV
	namemap[';']		= '%';		/* handle specially */
#endif

#ifdef CFS_DOWNCASE
	/* Now establish case translation */
	for(i='A';i<='Z';i++){
		namemap[i]	= i + ('a' - 'A');
	}
#endif
}

