/* 
 * HISTORY
 * 25-Apr-90  Morris Meyer (mmeyer) at NeXT
 * 	Revert getdirentries() uio_offset check for whether or not to
 *	use fake dir entries or not.
 * 
 * 29-Mar-90  Mike DeMoney (mike) at NeXT
 *	copen must call vn_open with address of fp->f_data rather than
 *	address of a local to deal with interupted opens.
 *
 * 16-Feb-90  Gregg Kellogg (gk) at NeXT
 *	uio_seg -> uio_segflg.
 *
 *  9-May-89  Peter King (king) at NeXT
 *	Back out NFS 4.0 directory changes.  Who needs'em?  They slow
 *	things down.  VOP_READDIR routines are optimized for
 *	getdirentries.  getdents() rewritten to be compatable with old
 *	VOP_READDIR routines.
 *
 *	Sun Bugfixes: 1006269 & 1008383 - Allow writes to special files
 *				in a read-only filesystem.
 *		      1014308 - truncate and ftruncate check for negative
 *				off_t.
 *
 * 23-Jan-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes:  Added getdents() system call.  Added
 *			old_getfakedirents() to support the old directory
 *			format.
 *
 * 17-Aug-87  Peter King (king) at NeXT
 *	Original Sun source, upgraded to Mach
 */ 

#import <mach_nbc.h>

/*	@(#)vfs_syscalls.c	2.4 88/06/20 4.0NFSSRC SMI;  from SMI 2.17 87/01/17	*/

#import <sys/param.h>
#import <sys/user.h>
#import <sys/file.h>
#import <sys/conf.h>
#import <sys/stat.h>
#import <sys/uio.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/vfs.h>
#import <sys/dirent.h>
#import <sys/pathname.h>
#import <sys/vnode.h>
#import <ufs/inode.h>
#import <ufs/fsdir.h>

extern	struct fileops vnodefops;

/*
 * System call routines for operations on files other
 * than read, write and ioctl.  These calls manipulate
 * the per-process file table which references the
 * networkable version of normal UNIX inodes, called vnodes.
 *
 * Many operations take a pathname, which is read
 * into a kernel buffer by pn_get (see vfs_pathname.c).
 * After preparing arguments for an operation, a simple
 * operation proceeds:
 *
 *	error = lookupname(pname, seg, followlink, &dvp, &vp, &vattr)
 *
 * where pname is the pathname operated on, seg is the segment that the
 * pathname is in (UIO_USERSPACE or UIO_SYSSPACE), followlink specifies
 * whether to follow symbolic links, dvp is a pointer to the vnode that
 * represents the parent directory of vp, the pointer to the vnode
 * referenced by the pathname. vattr is a vattr structure which hold the
 * attributes of the final component. The lookupname routine fetches the
 * pathname string into an internal buffer using pn_get (vfs_pathname.c),
 * and iteratively running down each component of the path until the
 * the final vnode and/or it's parent are found. If either of the addresses
 * for dvp or vp are NULL, then it assumes that the caller is not interested
 * in that vnode. If the pointer to the vattr structure is NULL then attributes
 * are not returned. Once the vnode or its parent is found, then a vnode
 * operation (e.g. VOP_OPEN) may be applied to it.
 *
 * One important point is that the operations on vnode's are atomic, so that
 * vnode's are never locked at this level.  Vnode locking occurs
 * at lower levels either on this or a remote machine. Also permission
 * checking is generally done by the specific filesystem. The only
 * checks done by the vnode layer is checks involving file types
 * (e.g. VREG, VDIR etc.), since this is static over the life of the vnode.
 *
 */

/*
 * Change current working directory (".").
 */
chdir()
{
	register struct a {
		char *dirnamep;
	} *uap = (struct a *)u.u_ap;
	struct vnode *vp;

	u.u_error = chdirec(uap->dirnamep, &vp);
	if (u.u_error == 0) {
		VN_RELE(u.u_cdir);
		u.u_cdir = vp;
	}
}

/*
 * Change notion of root ("/") directory.
 */
chroot()
{
	register struct a {
		char *dirnamep;
	} *uap = (struct a *)u.u_ap;
	struct vnode *vp;

	if (!suser())
		return;

	u.u_error = chdirec(uap->dirnamep, &vp);
	if (u.u_error == 0) {
		if (u.u_rdir != (struct vnode *)0)
			VN_RELE(u.u_rdir);
		u.u_rdir = vp;
	}
}

/*
 * Common code for chdir and chroot.
 * Translate the pathname and insist that it
 * is a directory to which we have execute access.
 * If it is replace u.u_[cr]dir with new vnode.
 */
chdirec(dirnamep, vpp)
	char *dirnamep;
	struct vnode **vpp;
{
	struct vnode *vp;		/* new directory vnode */
	register int error;

	error =
	    lookupname(dirnamep, UIO_USERSPACE, FOLLOW_LINK,
		(struct vnode **)0, &vp);
	if (error)
		return (error);
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
	} else {
		error = VOP_ACCESS(vp, VEXEC, u.u_cred);
	}
	if (error) {
		VN_RELE(vp);
	} else {
		*vpp = vp;
	}
	return (error);
}

/*
 * Open system call.
 */
open()
{
	register struct a {
		char *fnamep;
		int fmode;
		int cmode;
	} *uap = (struct a *)u.u_ap;

	u.u_error = copen(uap->fnamep, uap->fmode - FOPEN, uap->cmode);
}

/*
 * Creat system call.
 */
creat()
{
	register struct a {
		char *fnamep;
		int cmode;
	} *uap = (struct a *)u.u_ap;

	u.u_error = copen(uap->fnamep, FWRITE|FCREAT|FTRUNC, uap->cmode);
}

/*
 * Common code for open, creat.
 */
copen(pnamep, filemode, createmode)
	char *pnamep;
	int filemode;
	int createmode;
{
	register struct file *fp;
	struct vnode *vp;
	register int error;
	register int i;

	/*
	 * allocate a user file descriptor and file table entry.
	 */
	fp = falloc();
	if (fp == NULL)
		return(u.u_error);
	i = u.u_r.r_val1;		/* this is bullshit */
	/*
	 * open the vnode.
	 */
	error =
	    vn_open(pnamep, UIO_USERSPACE,
		filemode, ((createmode & 07777) & ~u.u_cmask), &vp);

	/*
	 * If there was an error, deallocate the file descriptor.
	 * Otherwise fill in the file table entry to point to the vnode.
	 */
	if (error) {
		u.u_ofile[i] = NULL;
		crfree(fp->f_cred);
		fp->f_count = 0;
#if	NeXT
		free_file(fp);
#endif	NeXT
	} else {
#if	NeXT
		fp->f_flag = (filemode & FMASK) |
			(filemode & (O_POPUP|O_ALERT));
#else	NeXT
		fp->f_flag = filemode & FMASK;
#endif	NeXT
		fp->f_type = DTYPE_VNODE;
		fp->f_data = (caddr_t)vp;
		fp->f_ops = &vnodefops;

		/*
		 * For named pipes, the FNDELAY flag must propagate to the
		 * rdwr layer, for backward compatibility.
		 */
		if (vp->v_type == VFIFO)
			fp->f_flag |= (filemode & FNDELAY);
	}
	return(error);
}

/*
 * Create a special (or regular) file.
 */
mknod()
{
	register struct a {
		char		*pnamep;
		int		fmode;
		int		dev;
	} *uap = (struct a *)u.u_ap;
	struct vnode *vp;
	struct vattr vattr;

	/* map 0 type into regular file, as other versions of UNIX do */
	if ((uap->fmode & IFMT) == 0)
		uap->fmode |= IFREG;

	/* Must be super-user unless making a FIFO node */
	if (((uap->fmode & IFMT) != IFIFO) && !suser())
		return;

	/*
	 * Setup desired attributes and vn_create the file.
	 */
	vattr_null(&vattr);
	vattr.va_type = MFTOVT(uap->fmode);
	vattr.va_mode = (uap->fmode & 07777) & ~u.u_cmask;

	switch (vattr.va_type) {
	case VDIR:
		u.u_error = EISDIR;	/* Can't mknod directories: use mkdir */
		return;

	case VBAD:
	case VCHR:
	case VSTR:
	case VBLK:
		vattr.va_rdev = uap->dev;
		break;

	case VNON:
		u.u_error = EINVAL;
		return;

	default:
		break;
	}

	u.u_error = vn_create(uap->pnamep, UIO_USERSPACE, &vattr, EXCL, 0, &vp);
	if (u.u_error == 0)
		VN_RELE(vp);
}

/*
 * Make a directory.
 */
mkdir()
{
	struct a {
		char	*dirnamep;
		int	dmode;
	} *uap = (struct a *)u.u_ap;
	struct vnode *vp;
	struct vattr vattr;

	vattr_null(&vattr);
	vattr.va_type = VDIR;
	vattr.va_mode = (uap->dmode & 0777) & ~u.u_cmask;

	u.u_error = vn_create(uap->dirnamep, UIO_USERSPACE, &vattr, EXCL, 0, &vp);
	if (u.u_error == 0)
		VN_RELE(vp);
}

/*
 * make a hard link
 */
link()
{
	register struct a {
		char	*from;
		char	*to;
	} *uap = (struct a *)u.u_ap;

	u.u_error = vn_link(uap->from, uap->to, UIO_USERSPACE);
}

/*
 * rename or move an existing file
 */
rename()
{
	register struct a {
		char	*from;
		char	*to;
	} *uap = (struct a *)u.u_ap;

	u.u_error = vn_rename(uap->from, uap->to, UIO_USERSPACE);
}

/*
 * Create a symbolic link.
 * Similar to link or rename except target
 * name is passed as string argument, not
 * converted to vnode reference.
 */
symlink()
{
	register struct a {
		char	*target;
		char	*linkname;
	} *uap = (struct a *)u.u_ap;
	struct vnode *dvp;
	struct vattr vattr;
	struct pathname tpn;
	struct pathname lpn;

	u.u_error = pn_get(uap->linkname, UIO_USERSPACE, &lpn);
	if (u.u_error)
		return;
	u.u_error = lookuppn(&lpn, NO_FOLLOW, &dvp, (struct vnode **)0);
	if (u.u_error) {
		pn_free(&lpn);
		return;
	}
	if (dvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		u.u_error = EROFS;
		goto out;
	}
	u.u_error = pn_get(uap->target, UIO_USERSPACE, &tpn);
	vattr_null(&vattr);
	vattr.va_mode = 0777;
	if (u.u_error == 0) {
		u.u_error =
		   VOP_SYMLINK(dvp, lpn.pn_path, &vattr, tpn.pn_path, u.u_cred);
		pn_free(&tpn);
	}
out:
	pn_free(&lpn);
	VN_RELE(dvp);
}

/*
 * Unlink (i.e. delete) a file.
 */
unlink()
{
	struct a {
		char	*pnamep;
	} *uap = (struct a *)u.u_ap;

	u.u_error = vn_remove(uap->pnamep, UIO_USERSPACE, FILE);
}

/*
 * Remove a directory.
 */
rmdir()
{
	struct a {
		char	*dnamep;
	} *uap = (struct a *)u.u_ap;

	u.u_error = vn_remove(uap->dnamep, UIO_USERSPACE, DIRECTORY);
}

/*
 * get directory entries in a file system independent format
 * This is the old system call.  It remains in 4.0 for binary compatibility.
 * It will disappear in 5-?.0. It returns directory entries in the
 * old file-system independent directory entry format. This structure was
 * formerly defined in /usr/include/sys/dir.h. It is now no longer available to
 * user source files. It is defined only by struct direct below.
 *	This system call is superseded by the new getdents() call, which
 * returns directory entries in the new filesystem-independent format
 * given in /usr/include/sys/dirent.h and /usr/include/sys/dir.h. The
 * vn_readdir interface has also been modified to return entries in this
 * format.
 */

getdirentries()
{
	register struct a {
		int	fd;
		char	*buf;
		unsigned count;
		long	*basep;
	} *uap = (struct a *)u.u_ap;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;

	u.u_error = getvnodefp(uap->fd, &fp);
	if (u.u_error)
		return;
	if ((fp->f_flag & FREAD) == 0) {
		u.u_error = EBADF;
		return;
	}
#if	NeXT
tryagain:
#endif	NeXT
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = fp->f_offset;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = uap->count;
#if	NeXT
	/*
	 * We use negative offsets to mark entries that we dynamically build
	 * out of the fake entry list.
	 */
	if ((long) auio.uio_offset < 0) {
		u.u_error = getfakedirentries((struct vnode *)fp->f_data,
					      &auio, fp->f_cred);
	} else {
		u.u_error = VOP_READDIR((struct vnode *)fp->f_data,
					    &auio, fp->f_cred);
		if (uap->count - auio.uio_resid <= 0) {
			/*
			 * Nothing there, try the fake entries
			 */
			fp->f_offset = (long) -DIRBLKSIZ;
			goto tryagain;
		}
	}
#else	NeXT
	u.u_error = VOP_READDIR((struct vnode *)fp->f_data, &auio, fp->f_cred);
#endif	NeXT
	if (u.u_error)
		return;
	u.u_error =
	    copyout((caddr_t)&fp->f_offset, (caddr_t)uap->basep, sizeof(long));
	u.u_r.r_val1 = uap->count - auio.uio_resid;
	fp->f_offset = auio.uio_offset;
}

#if	NeXT
/*
 * get the fake direntries of a vnode as an extension of the old-style
 * getdirentries.
 */
getfakedirentries(vp, uiop, cred)
	struct vnode		*vp;
	register struct uio	*uiop;
	struct ucred		*cred;
{
	register struct iovec	*iovp;
	off_t			offset;
	char			*working_buf;
	int			bufsize;
	int			resid;
	struct direct		fakedir;
	int			fakedirsize;
	struct direct		*dp;
	off_t			n;
	register struct vfs	*vfsp;
	int			error;

	if (vp->v_vfsentries == (struct vfs *)0) {
		return(0);
	}
	
	resid = uiop->uio_resid;
	offset = (off_t)((long)uiop->uio_offset * -1) - DIRBLKSIZ;
	if ((offset + resid) < 0)
		return(EINVAL);
	if (resid == 0)
		return(0);

	/*
	 * Scan through fake entries to find the starting point for this
	 * offset
	 */
	n = 0;
	vfsp = vp->v_vfsentries;
	while(n < offset) {
		if (vfsp == (struct vfs *)0) {
			/*
			 * No more entries
			 */
			return(0);
		}
		fakedir.d_namlen = strlen(vfsp->vfs_name);
		fakedirsize = DIRSIZ(&fakedir);
		if (((n & ~(DIRBLKSIZ - 1)) + fakedirsize) > DIRBLKSIZ) {
			/*
			 * We don't have enough room for this entry in
			 * this block, skip to the next one
			 */
			n = (n & ~(DIRBLKSIZ - 1)) + DIRBLKSIZ;
		} else {
			n += fakedirsize;
		}
		vfsp = vfsp->vfs_nextentry;
	}
	if (vfsp == (struct vfs *)0) {
		return(0);
	}

	/*
	 * Get some vm to work with.
	 */
	bufsize = resid;
	working_buf = (char *)kalloc(bufsize);
	dp = (struct direct *)working_buf;
	n = 0;
	while (resid) {
		dp->d_ino = -1; /* bogus fileno */
		dp->d_namlen = strlen(vfsp->vfs_name);
		strcpy(dp->d_name, vfsp->vfs_name);

		/*
		 * Check to see if we have room in the DIRBLK for the
		 * next entry.  If not then jump to the next DIRBLK
		 */
		vfsp = vfsp->vfs_nextentry;
		if (vfsp) {
			fakedir.d_namlen = strlen(vfsp->vfs_name);
			fakedirsize = DIRSIZ(&fakedir);
			if ((n + fakedirsize + DIRSIZ(dp)) > DIRBLKSIZ) {
				/*
				 * We don't have enough room for this entry in
				 * this block, skip to the next one
				 */
				dp->d_reclen = DIRBLKSIZ - n;
				n = 0;
			} else {
				dp->d_reclen = DIRSIZ(dp);
				n += DIRSIZ(dp);
			}
			resid -= dp->d_reclen;
			offset += dp->d_reclen;
		} else {
			dp->d_reclen = DIRBLKSIZ - n;
			resid -= dp->d_reclen;
			offset += dp->d_reclen;
			break;
		}
		dp = (struct direct *)((int)dp + dp->d_reclen);
	}
	error = uiomove(working_buf, uiop->uio_resid - resid, UIO_READ, uiop);
	kfree(working_buf, bufsize);
	if (error)
		return(error);
	uiop->uio_offset = (off_t)(((long)offset + DIRBLKSIZ) * -1 );
	return(0);
}
#endif	NeXT

/*
 * Seek on file.  Only hard operation
 * is seek relative to end which must
 * apply to vnode for current file size.
 * 
 * Note: lseek(0, 0, L_XTND) costs much more than it did before.
 */
lseek()
{
	register struct a {
		int	fd;
		off_t	off;
		int	sbase;
	} *uap = (struct a *)u.u_ap;
	struct file *fp;

	u.u_error = getvnodefp(uap->fd, &fp);
	if (u.u_error) {
		if (u.u_error == EINVAL)
			u.u_error = ESPIPE;	/* be compatible */
		return;
	}

	if (((struct vnode *)fp->f_data)->v_type == VFIFO) {
		u.u_error = ESPIPE;
		return;
	}

	switch (uap->sbase) {

	case L_INCR:
		fp->f_offset += uap->off;
		break;

	case L_XTND: {
		struct vattr vattr;

		u.u_error =
		    VOP_GETATTR((struct vnode *)fp->f_data, &vattr, u.u_cred);
		if (u.u_error)
			return;
		fp->f_offset = uap->off + vattr.va_size;
		break;
	}

	case L_SET:
		fp->f_offset = uap->off;
		break;

	default:
		u.u_error = EINVAL;
	}
	u.u_r.r_off = fp->f_offset;
}

/*
 * Determine accessibility of file, by
 * reading its attributes and then checking
 * against our protection policy.
 */
access()
{
	register struct a {
		char	*fname;
		int	fmode;
	} *uap = (struct a *)u.u_ap;
	struct vnode *vp;
	register u_short mode;
	register int svuid;
	register int svgid;

	/*
	 * Lookup file
	 */
	u.u_error =
	    lookupname(uap->fname, UIO_USERSPACE, FOLLOW_LINK,
		(struct vnode **)0, &vp);
	if (u.u_error)
		return;

	/*
	 * Use the real uid and gid and check access
	 */
	svuid = u.u_uid;
	svgid = u.u_gid;
	u.u_uid = u.u_ruid;
	u.u_gid = u.u_rgid;

	mode = 0;
	/*
	 * fmode == 0 means only check for exist
	 */
	if (uap->fmode) {
		if (uap->fmode & R_OK)
			mode |= VREAD;
		if (uap->fmode & W_OK) {
			if (isrofile(vp)) {
				u.u_error = EROFS;
				goto out;
			}
			mode |= VWRITE;
		}
		if (uap->fmode & X_OK)
			mode |= VEXEC;
		u.u_error = VOP_ACCESS(vp, mode, u.u_cred);
	}

	/*
	 * release the vnode and restore the uid and gid
	 */
out:
	VN_RELE(vp);
	u.u_uid = svuid;
	u.u_gid = svgid;
}

/*
 * Get attributes from file or file descriptor.
 * Argument says whether to follow links, and is
 * passed through in flags.
 */
stat()
{
	caddr_t uap = (caddr_t)u.u_ap;

	u.u_error = stat1(uap, FOLLOW_LINK);
}

lstat()
{
	caddr_t uap = (caddr_t)u.u_ap;

	u.u_error = stat1(uap, NO_FOLLOW);
}

stat1(uap0, follow)
	caddr_t uap0;
	enum symfollow follow;
{
	struct vnode *vp;
	struct stat sb;
	register int error;
	register struct a {
		char	*fname;
		struct	stat *ub;
	} *uap = (struct a *)uap0;

	error =
	    lookupname(uap->fname, UIO_USERSPACE, follow,
		(struct vnode **)0, &vp);
	if (error)
		return (error);
	error = vno_stat(vp, &sb);
	VN_RELE(vp);
	if (error)
		return (error);
	return (copyout((caddr_t)&sb, (caddr_t)uap->ub, sizeof (sb)));
}

/*
 * Read contents of symbolic link.
 */
readlink()
{
	register struct a {
		char	*name;
		char	*buf;
		int	count;
	} *uap = (struct a *)u.u_ap;
	struct vnode *vp;
	struct iovec aiov;
	struct uio auio;

	u.u_error =
	    lookupname(uap->name, UIO_USERSPACE, NO_FOLLOW,
		(struct vnode **)0, &vp);
	if (u.u_error)
		return;
	if (vp->v_type != VLNK) {
		u.u_error = EINVAL;
		goto out;
	}
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = uap->count;
	u.u_error = VOP_READLINK(vp, &auio, u.u_cred);
out:
	VN_RELE(vp);
	u.u_r.r_val1 = uap->count - auio.uio_resid;
}

/*
 * Change mode of file given path name.
 */
chmod()
{
	register struct a {
		char	*fname;
		int	fmode;
	} *uap = (struct a *)u.u_ap;
	struct vattr vattr;

	vattr_null(&vattr);
	vattr.va_mode = uap->fmode & 07777;
	u.u_error = namesetattr(uap->fname, FOLLOW_LINK, &vattr);
}

/*
 * Change mode of file given file descriptor.
 */
fchmod()
{
	register struct a {
		int	fd;
		int	fmode;
	} *uap = (struct a *)u.u_ap;
	struct vattr vattr;

	vattr_null(&vattr);
	vattr.va_mode = uap->fmode & 07777;
	u.u_error = fdsetattr(uap->fd, &vattr);
}

/*
 * Change ownership of file given file name.
 */
chown()
{
	register struct a {
		char	*fname;
		int	uid;
		int	gid;
	} *uap = (struct a *)u.u_ap;
	struct vattr vattr;

	vattr_null(&vattr);
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	u.u_error = namesetattr(uap->fname, NO_FOLLOW,  &vattr);
}

/*
 * Change ownership of file given file descriptor.
 */
fchown()
{
	register struct a {
		int	fd;
		int	uid;
		int	gid;
	} *uap = (struct a *)u.u_ap;
	struct vattr vattr;

	vattr_null(&vattr);
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	u.u_error = fdsetattr(uap->fd, &vattr);
}

/*
 * Set access/modify times on named file.
 */
utimes()
{
	register struct a {
		char	*fname;
		struct	timeval *tptr;
	} *uap = (struct a *)u.u_ap;
	struct timeval tv[2];
	struct vattr vattr;

	u.u_error = copyin((caddr_t)uap->tptr, (caddr_t)tv, sizeof (tv));
	if (u.u_error)
		return;
	vattr_null(&vattr);
	vattr.va_atime = tv[0];
	vattr.va_mtime = tv[1];
	u.u_error = namesetattr(uap->fname, FOLLOW_LINK, &vattr);
}

/*
 * Truncate a file given its path name.
 */
truncate()
{
	register struct a {
		char	*fname;
		int	length;
	} *uap = (struct a *)u.u_ap;
	struct vattr vattr;

	if (uap->length < 0) {
		u.u_error = EINVAL;
		return;
	}
	vattr_null(&vattr);
	vattr.va_size = uap->length;
	u.u_error = namesetattr(uap->fname, FOLLOW_LINK, &vattr);
}

/*
 * Truncate a file given a file descriptor.
 */
ftruncate()
{
	register struct a {
		int	fd;
		int	length;
	} *uap = (struct a *)u.u_ap;
	register struct vnode *vp;
	struct file *fp;

	if (uap->length < 0) {
		u.u_error = EINVAL;
		return;
	}
	u.u_error = getvnodefp(uap->fd, &fp);
	if (u.u_error)
		return;
	vp = (struct vnode *)fp->f_data;
	if ((fp->f_flag & FWRITE) == 0) {
		u.u_error = EINVAL;
	} else if (vp->v_vfsp->vfs_flag & VFS_RDONLY) {
		u.u_error = EROFS;
	} else {
		struct vattr vattr;

		vattr_null(&vattr);
		vattr.va_size = uap->length;
		u.u_error = VOP_SETATTR(vp, &vattr, fp->f_cred);
	}
}

/*
 * Common routine for modifying attributes
 * of named files.
 */
namesetattr(fnamep, followlink, vap)
	char *fnamep;
	enum symfollow followlink;
	struct vattr *vap;
{
	struct vnode *vp;
	register int error;

	error =
	    lookupname(fnamep, UIO_USERSPACE, followlink,
		 (struct vnode **)0, &vp);
	if (error)
		return(error);	
	if(vp->v_vfsp->vfs_flag & VFS_RDONLY)
		error = EROFS;
	else
		error = VOP_SETATTR(vp, vap, u.u_cred);
	VN_RELE(vp);
	return(error);
}

/*
 * Common routine for modifying attributes
 * of file referenced by descriptor.
 */
fdsetattr(fd, vap)
	int fd;
	struct vattr *vap;
{
	struct file *fp;
	register struct vnode *vp;
	register int error;

	error = getvnodefp(fd, &fp);
	if (error == 0) {
		vp = (struct vnode *)fp->f_data;
		if(vp->v_vfsp->vfs_flag & VFS_RDONLY)
			return(EROFS);
		error = VOP_SETATTR(vp, vap, fp->f_cred);
	}
	return(error);
}

/*
 * Flush output pending for file.
 */
fsync()
{
	struct a {
		int	fd;
	} *uap = (struct a *)u.u_ap;
	struct file *fp;
	int	error;

	error = getvnodefp(uap->fd, &fp);
#if	MACH_NBC
	if (error == 0)
		error = mfs_fsync((struct vnode *)fp->f_data);
#endif	MACH_NBC
	if (error == 0)
		error = VOP_FSYNC((struct vnode *)fp->f_data, fp->f_cred);
	u.u_error = error;
}

/*
 * Set file creation mask.
 */
umask()
{
	register struct a {
		int mask;
	} *uap = (struct a *)u.u_ap;
	u.u_r.r_val1 = u.u_cmask;
	u.u_cmask = uap->mask & 07777;
}

/*
 * Revoke access the current tty by all processes.
 * Used only by the super-user in init
 * to give ``clean'' terminals at login.
 */
vhangup()
{

	if (!suser())
		return;
	if (u.u_ttyp == NULL)
		return;
	forceclose(u.u_ttyd);
	gsignal(*u.u_ttyp, SIGHUP);
}

forceclose(dev)
	dev_t dev;
{
	register struct file *fp;
	register struct vnode *vp;

#if	NeXT
	for (fp = (struct file *) queue_first(&file_list);
	     !queue_end(&file_list, (queue_entry_t) fp);
	     fp = (struct file *) queue_next(&fp->links)) {
#else	NeXT
	for (fp = file; fp < fileNFILE; fp++) {
#endif	NeXT
		if (fp->f_count == 0)
			continue;
		if (fp->f_type != DTYPE_VNODE)
			continue;
		vp = (struct vnode *)fp->f_data;
		if (vp == 0)
			continue;
		if (vp->v_type != VCHR && vp->v_type != VSTR)
			continue;
		if (vp->v_rdev != dev)
			continue;
		/*
		 * Note that while this prohibits further I/O on the
		 * descriptor, it does not prohibit closing the
		 * descriptor.
		 */
		fp->f_flag &= ~(FREAD|FWRITE);
	}
}

/*
 * Get the file structure entry for the file descrpitor, but make sure
 * its a vnode.
 */
int
getvnodefp(fd, fpp)
	int fd;
	struct file **fpp;
{
	register struct file *fp;

	fp = getf(fd);
	if (fp == (struct file *)0)
		return(EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return(EINVAL);
	*fpp = fp;
	return(0);
}

#if	NeXT
getdents()
{
	register struct a {
		int	fd;
		char	*buf;
		unsigned count;
	} *uap = (struct a *)u.u_ap;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct vnode *vp;
	register struct direct *idp;
	register struct dirent *odp;
	register int incount;
	register int outcount = 0;
	off_t offset;
	u_int count, bytes_read;
	register caddr_t ibuf, obuf;
	int bufsize;
	int firsttime = 1;

	if (uap->count < sizeof(struct dirent)) {
		u.u_error = EINVAL;
		return;
	}
	u.u_error = getvnodefp(uap->fd, &fp);
	if (u.u_error)
		return;
	vp = (struct vnode *)fp->f_data;
	if ((fp->f_flag & FREAD) == 0) {
		u.u_error = EBADF;
		return;
	}
	if (vp->v_type != VDIR) {
	    u.u_error = ENOTDIR;
	    return;
	}

tryagain:
	count = uap->count;
	offset = fp->f_offset;

	/* Allocate temporary space for format conversion */
	if (firsttime) {
		firsttime = 0;
		bufsize = count + sizeof (struct dirent);
		obuf = (caddr_t)kalloc(bufsize * 2);
		odp = (struct dirent *) obuf;
		ibuf = (caddr_t)((u_int)obuf + bufsize);
	}

	aiov.iov_base = ibuf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = offset;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = count;

	/*
	 * We use negative offsets to mark entries that we dynamically build
	 * out of the fake entry list.
	 */
	if ((long) auio.uio_offset <= -DIRBLKSIZ) {
		u.u_error = getfakedirentries(vp, &auio, fp->f_cred);
	} else {
		u.u_error = VOP_READDIR(vp, &auio, fp->f_cred);
		if (count - auio.uio_resid <= 0) {
			/*
			 * Nothing there, try the fake entries
			 */
			fp->f_offset = (long) -DIRBLKSIZ;
			goto tryagain;
		}
	}
	if (u.u_error)
		goto out;
	bytes_read = count - auio.uio_resid;

	incount = 0;
	idp = (struct direct *)ibuf;

	/* Transform to new format */
	while (incount < bytes_read) {
	    extern char *strcpy();

	    /* skip empty entries */
	    if (idp->d_ino != 0 && offset >= fp->f_offset) {
		odp->d_fileno = idp->d_ino;
		odp->d_namlen = idp->d_namlen;
		(void) strcpy(odp->d_name, idp->d_name);
		odp->d_reclen = DIRENTSIZ(odp);
		odp->d_off = (offset >= 0 ? offset + idp->d_reclen :
			      offset - idp->d_reclen);
		outcount += odp->d_reclen;
		/* Got as many bytes as requested, quit */
		if (outcount > count) {
		    outcount -= odp->d_reclen;
		    break;
		}
		odp = (struct dirent *)((int)odp + odp->d_reclen);
	    }
	    incount += idp->d_reclen;
	    if (offset >= 0) {
		offset += idp->d_reclen;
	    } else {
		offset -= idp->d_reclen;
	    }
	    idp = (struct direct *)((int)idp + idp->d_reclen);
	}

	aiov.iov_base = uap->buf;
	aiov.iov_len = outcount;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = fp->f_offset;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = outcount;

	if (u.u_error = uiomove(obuf, outcount, UIO_READ, &auio))
		goto out;

	u.u_r.r_val1 = outcount - auio.uio_resid;
	fp->f_offset = offset;
out:
	kfree(obuf, bufsize * 2);
}
#else	NeXT
/* #ifdef UFS */
/*
 * get directory entries in a file system independent format.
 * This call returns directory entries in the new format specified
 * in sys/dirent.h which is also the format returned by the vn_readdir
 * interface. This call supersedes getdirentries(), which will disappear
 * in 5-?.0.
 */
getdents()
{
	register struct a {
		int	fd;
		char	*buf;
		unsigned count;
	} *uap = (struct a *)u.u_ap;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct vnode *vp;

	if (uap->count < sizeof (struct dirent)) {
		u.u_error = EINVAL;
		return;
	}
	u.u_error = getvnodefp(uap->fd, &fp);
	if (u.u_error)
		return;
	vp = (struct vnode *)fp->f_data;
	if ((fp->f_flag & FREAD) == 0) {
		u.u_error = EBADF;
		return;
	}
	if (vp->v_type != VDIR) {
		u.u_error = ENOTDIR;
		return;
	}
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = fp->f_offset;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = uap->count;
	u.u_error = VOP_READDIR(vp, &auio, fp->f_cred);
	if (u.u_error)
		return;
	u.u_r.r_val1 = uap->count - auio.uio_resid;
	fp->f_offset = auio.uio_offset;
}
#endif	NeXT


