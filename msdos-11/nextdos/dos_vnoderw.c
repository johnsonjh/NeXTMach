/*	@(#)dos_vnoderw.c	2.0	17/07/90	(c) 1990 NeXT	*/

/* 
 * dos_vnoderw.c -- vnode r/w functions for loadable DOS file system
 *
 * HISTORY
 * 17-Jul-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <nextdev/ldd.h>
#import <sys/param.h>
#import <sys/vnode.h>
#import <sys/ucred.h>
#import <next/vm_types.h>
#import <vm/vm_pager.h>
#import <vm/vm_page.h>
#import <kern/mfs.h>
#import <sys/errno.h>
#import <nextdos/msdos.h>
#import <nextdos/next_proto.h>
#import <nextdos/dosdbg.h>
#import <header/posix.h>

/* #define ALWAYS_SEEK	1	/* po_seek before every read/write */
#define USE_PAGEIO_BUF	1

extern void copy_to_phys(vm_offset_t virt, vm_offset_t phys,
	vm_size_t size);
extern void copy_from_phys(vm_offset_t phys, vm_offset_t virt,
	vm_size_t size);

int dos_rdwr(struct vnode *vp, 			/* vnode to r/w */	
	struct uio *uiop, 			/* from/to */
	enum uio_rw rw, 
	int ioflag, 				/* IO_APPEND, IO_SYNC */
	struct ucred *cred)
{
	msdnode_t mnp;
	
	mnp = VTOM(vp);
	dbg_rw(("dos_rdwr: msdnode 0x%x path %s\n", mnp, mnp->m_path));
	return(EINVAL);
}


pager_return_t
dos_pagein(struct vnode *vp,
	vm_page_t	m,
	vm_offset_t	f_offset)	/* byte offset within file block */
{
	msdnode_t mnp = VTOM(vp);
	char *buf;			/* read data goes here initially */
	int bytes_read;
	pager_return_t prtn = PAGER_SUCCESS;
	int rtn;
	int new_fp;
	
	dbg_rw(("dos_pagein: path <%s> f_offset %d\n",
		mnp->m_path, f_offset));
	mn_lock(mnp);
	if(mnp->m_fd < 0) {
		dbg_err(("dos_pagein: INVALID FD\n"));
		mn_unlock(mnp);
		return(EBADF);
	}
#ifdef	USE_PAGEIO_BUF
	lock_write(pageio_buf_lockp);
	buf = pageio_buf;
#else	USE_PAGEIO_BUF
	buf = kalloc(PAGE_SIZE);
	if(buf == NULL) {
		dbg_err(("dos_pagein: COULDN'T KALLOC BUF SPACE\n"));
		mn_unlock(mnp);
		return(PAGER_ERROR);
	}
#endif	USE_PAGEIO_BUF
#ifndef	ALWAYS_SEEK
	if(mnp->m_fp != (LONG)f_offset) {
#endif	ALWAYS_SEEK
		new_fp = po_lseek(mnp->m_fd, (LONG)f_offset, PSEEK_SET);
		if(new_fp != (LONG)f_offset) {
			dbg_rw(("dos_pagein: LSEEK FAILED (offset %d, "
				"new_fp %d)\n", f_offset, new_fp));
			prtn = PAGER_ABSENT;
			goto out;
		}
		mnp->m_fp = (LONG)f_offset;
#ifndef	ALWAYS_SEEK
	}
	else {
		dbg_api(("dos_pagein: no seek needed\n"));
	}
#endif	ALWAYS_SEEK
	bytes_read = po_read(mnp->m_fd, (UTINY *)buf, PAGE_SIZE);
	switch(bytes_read) {
	    case -1:
		rtn = msd_errno();
		dbg_err(("dos_pagein: po_read() returned %d\n", rtn));
		prtn = PAGER_ABSENT;	/* ERROR??? */
		goto out;
	    case 0:
		prtn = PAGER_ABSENT;
		goto out;
	    default:
	    	break;
	}
	mnp->m_fp += bytes_read;
	
	/*
	 * Be sure that data not in the file is zero filled, then copy a
	 * whole page worth of data over to vm_page.
	 */
	if(bytes_read < PAGE_SIZE) 
		bzero(buf + bytes_read, (PAGE_SIZE-bytes_read));
	copy_to_phys((vm_offset_t)buf,
		      VM_PAGE_TO_PHYS(m),
		      PAGE_SIZE);
out:
	mn_unlock(mnp);
#ifdef	USE_PAGEIO_BUF
	lock_done(pageio_buf_lockp);
#else	USE_PAGEIO_BUF
	kfree(buf, PAGE_SIZE);
#endif	USE_PAGEIO_BUF
	dbg_rw(("dos_pagein: returning %d, %d bytes read\n", prtn, 
		bytes_read));
	return(prtn);
	
}

pager_return_t
dos_pageout(struct vnode *vp,
	vm_offset_t addr,		/* physical address - source */
	vm_size_t csize,		/* bytes to write */
	vm_offset_t f_offset)		/* byte offset in file */
{
	msdnode_t mnp = VTOM(vp);
	char *buf;			/* I/O goes here */
	int bytes_written;
	pager_return_t prtn = PAGER_SUCCESS;
	int rtn;
	int new_fp;
	PC_FILE *pfile;
	
	dbg_rw(("dos_pageout: path <%s> csize %d f_offset %d fp %d\n",
		mnp->m_path, csize, f_offset, mnp->m_fp));
	mn_lock(mnp);
	if(mnp->m_fd < 0) {
		dbg_err(("dos_pageout: INVALID FD\n"));
		mn_unlock(mnp);
		return(EBADF);
	}
#ifdef	USE_PAGEIO_BUF
	lock_write(pageio_buf_lockp);
	buf = pageio_buf;
#else	USE_PAGEIO_BUF
	buf = kalloc(PAGE_SIZE);
	if(buf == NULL) {
		dbg_err(("dos_pageout: COULDN'T KALLOC BUF SPACE\n"));
		mn_unlock(mnp);
		return(PAGER_ERROR);
	}
#endif	USE_PAGEIO_BUF
	/*
	 * Seek as necessary. This includes writing pad bytes if
	 * we're seeking beyond EOF.
	 */
	pfile = pc_fd2file(mnp->m_fd, YES);
#ifdef	CLUSTER_DEBUG
	check_cluster(pfile, "dos_pageout - before lseek");
#endif	CLUSTER_DEBUG
#ifndef	ALWAYS_SEEK
	if(mnp->m_fp != (LONG)f_offset) {
#endif	ALWAYS_SEEK
		if((LONG)f_offset > pfile->pobj->finode->fsize) 
		    /* extend file */
		    new_fp = msd_extend(mnp, f_offset);
		else
		    /* easy case */
		    new_fp = po_lseek(mnp->m_fd, (LONG)f_offset, PSEEK_SET);
		if(new_fp != f_offset) {
			dbg_err(("dos_pageout: po_lseek returned %d, expected"
				" %d\n", new_fp, f_offset));
			prtn = PAGER_ERROR;
			goto out;
		}
		mnp->m_fp = (LONG)f_offset;
#ifndef	ALWAYS_SEEK
	}
	else {
		dbg_api(("dos_pageout: no seek needed\n"));
	}
#endif	ALWAYS_SEEK

	/*
	 * copy data to local buffer. Optimize this later.
	 */
	copy_from_phys(addr, (vm_offset_t)buf, csize);
#ifdef	CLUSTER_DEBUG
	check_cluster(pfile, "dos_pageout - before po_write");
#endif	CLUSTER_DEBUG
	bytes_written = po_write(mnp->m_fd, (UTINY *)buf, csize);
	if(bytes_written != csize) {
		rtn = msd_errno();
		dbg_err(("dos_pageout: po_write() returned %d\n", rtn));
		prtn = PAGER_ERROR;
		goto out;
	}	
	mnp->m_fp += bytes_written;
out:
#ifdef	CLUSTER_DEBUG
	check_cluster(pfile, "dos_pageout - out");
#endif	CLUSTER_DEBUG
	/*
	 * On first write to a file, its cluster number becomes valid...
	 */
	if(mnp->m_fcluster == 0)
		mnp->m_fcluster = msd_fd_to_cluster(mnp->m_fd);
	mn_unlock(mnp);
#ifdef	USE_PAGEIO_BUF
	lock_done(pageio_buf_lockp);
#else	USE_PAGEIO_BUF
	kfree(buf, PAGE_SIZE);
#endif	USE_PAGEIO_BUF
	dbg_rw(("dos_pageout: returning %d; m_fp %d fsize %d vnode_size %d\n", 
		prtn, mnp->m_fp, pfile->pobj->finode->fsize, 
		vm_get_vnode_size(vp)));
	return(prtn);
	
}

/* end of dos_vnoderw.c */
