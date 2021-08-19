/*	@(#)unix_rw.c	2.0	27/06/90	(c) 1990 NeXT	*/

/* 
 * unix_rw.c - Basic Unix disk I/O functions - LOADABLE KERNEL VERSION
 *
 * HISTORY
 * 27-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <sys/param.h>
#import <nextdev/scsireg.h>
#import <nextdev/fd_extern.h>
#import <sys/file.h>
#import <sys/uio.h>
#import <sys/user.h>
#import <sys/vnode.h>
#import <sys/vfs.h>
#ifdef	MACH_ASSERT
#undef	MACH_ASSERT
#endif	MACH_ASSERT
#define MACH_ASSERT 1
#import <kern/assert.h>
#import <header/pcdisk.h>
#import <nextdev/ldd.h>
#import "msdos.h"
#import <sys/printf.h>
#import <nextdos/dosdbg.h>

extern int binval(struct vnode *vp);

static int unix_rw(ddi_t ddip, 
	int block, 
	int block_count, 
	char *addrs, 
	int read_flag);
/*
 * Open a unix special device. ddip->ddivp must be valid.
 */
int unix_open(ddi_t ddip) 
{	
	int error;
	
	dbg_io(("unix_open\n"));
	ASSERT(ddip->ddivp != NULL);
	error = VOP_OPEN(&ddip->ddivp,
	    (ddip->vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE,
	    u.u_cred);
	if (error) {
		dbg_err(("unix_io: open failed; errno = %d\n", u.u_error));
		return(NO);
	}
	return(YES);
}

/*
 * Close a unix special device.
 */
void unix_close(ddi_t ddip)
{
	dbg_io(("unix_close\n"));
	ASSERT(ddip->ddivp != NULL);
	VOP_CLOSE(ddip->ddivp, 
		 (ddip->vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE, 
		  1, 
		  u.u_cred);
	binval(ddip->ddivp);
}

int unix_read(ddi_t ddip, 
	int block, 
	int block_count, 
	char *addrs)
{
	int rtn;
	
	dbg_io(("unix_read: block 0x%x block_count 0x%x\n",
		block, block_count));
	rtn = unix_rw(ddip, block, block_count, addrs, TRUE);
	return(rtn);
}

int unix_write(ddi_t ddip, 
	int block, 
	int block_count, 
	char *addrs)
{
	int rtn;
	
	dbg_io(("unix_write: block 0x%x block_count 0x%x\n",
		block, block_count));
	rtn = unix_rw(ddip, block, block_count, addrs, FALSE);
	return(rtn);
}

/*
 * Read from an open unix disk. 'live partition" I/O is assumed.
 */
static int unix_rw(ddi_t ddip, 
	int block, 
	int block_count, 
	char *addrs, 
	int read_flag)
{
	struct iovec iov;
	struct uio uio;
	int rtn;
	
	iov.iov_base   = addrs;
	iov.iov_len    = block_count * DOS_SECTOR_SIZE;
	uio.uio_iov    = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = block * DOS_SECTOR_SIZE;
	uio.uio_segflg = UIO_SYSSPACE;	/* phys I/O to kernel memory */
	uio.uio_fmode  = 0;		/* what's this? */
	uio.uio_resid  = iov.iov_len;
	rtn = VOP_RDWR(ddip->ddivp,
		&uio,
		read_flag ? UIO_READ : UIO_WRITE,
		IO_SYNC,
		u.u_cred);
	if(rtn) {
		dbg_err(("MSDOS unix_rw: %s returned %d\n",
			read_flag ? "read" : "write"));
		return(NO);
	}
	if(uio.uio_resid != 0) {
		dbg_err(("MSDOS unix_rw: incomplete transfer on %s\n",
			read_flag ? "read" : "write"));
		return(NO);
	}
	return(YES);
} /* unix_rw() */

