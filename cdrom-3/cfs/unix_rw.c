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
#import <nextdev/ldd.h>

#import <sys/printf.h>

#include "cdrom.h"

extern int binval(struct vnode *vp);

/*
 * Open a unix special device. ddip->ddivp must be valid.
 */
int 	unix_open(struct vnode *vp) 
{	
	int error;
	
	dprint(("unix_open\n"));
	ASSERT(vp != NULL);
	error = VOP_OPEN(&vp,FREAD,u.u_cred);
	if (error) {
		dprint1("unix_io: open failed; errno = %d\n", u.u_error);
		return(error);
	}
	return(0);
}

/*
 * Close a unix special device.
 */
void 	unix_close(struct vnode *vp)
{
	dprint(("unix_close\n"));
	ASSERT(vp != NULL);
	VOP_CLOSE(vp, FREAD, 1, u.u_cred);
	binval(vp);
}

/*
 * Read from an open unix disk. 'live partition" I/O is assumed.
 */
int 	unix_read(struct vnode *vp, 
	int block, 
	int block_count, 
	char *addrs) 
{
	struct iovec iov;
	struct uio uio;
	int rtn;
	
	dprint2("unix_read: block=%d block_count=%d \n",block,block_count);
	

	iov.iov_base   = addrs;
	iov.iov_len    = block_count * CDROM_BLOCK_SIZE;
	uio.uio_iov    = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = block * CDROM_BLOCK_SIZE;
	uio.uio_segflg = UIO_SYSSPACE;	/* phys I/O to kernel memory */
	uio.uio_fmode  = 0;		/* what's this? */
	uio.uio_resid  = iov.iov_len;
	rtn = VOP_RDWR(vp, &uio, UIO_READ, IO_SYNC, u.u_cred);
	if(rtn) {
		dprint1("CFS unix_read returned %d\n",rtn);
		return(-1);
	}
	if(uio.uio_resid != 0) {
		dprint1("CFS: unix_read incomplete transfer %d\n",
			uio.uio_resid);
		return(-1);
	}
	return(0);
} 

