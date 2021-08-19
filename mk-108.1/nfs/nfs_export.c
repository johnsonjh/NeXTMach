/* 
 **********************************************************************
 * Mach Operating System
 * Copyright (c) 1988 Carnegie-Mellon University
 *  
 * Some software in these files are derived from sources other
 * than CMU.  Previous copyright and other source notices are
 * preserved below and permission to use such software is
 * dependent on licenses from those institutions.
 * 
 * Permission to use the CMU portion of this software for 
 * any non-commercial research and development purpose is
 * granted with the understanding that appropriate credit
 * will be given to CMU, the Mach project and its authors.
 * The Mach project would appreciate being notified of any
 * modifications and of redistribution of this software so that
 * bug fixes and enhancements may be distributed to users.
 *
 * All other rights are reserved to Carnegie-Mellon University.
 **********************************************************************
 * HISTORY
 * 16-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Don't allocate (or free) zero bytes.
 *
 * 19-Dec-88  Peter King (king) at NeXT
 *	NFS 4.0 Changes:  Added support for AUTH_DES.
 *
 *  2-Aug-88  Peter King (king) at NeXT, Inc.
 *	Ported from Sun D/NFS release.
 *
 **********************************************************************
 */ 

/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

#import <nfs_server.h>
#import <secure_nfs.h>

#import <sys/types.h>
#import <sys/param.h>
#import <sys/time.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/socket.h>
#import <sys/errno.h>
#import <sys/uio.h>
#import <sys/user.h>
#import <sys/file.h>
#import <sys/pathname.h>
#import <netinet/in.h>
#import <rpc/types.h>
#import <rpc/auth.h>
#import <rpc/auth_unix.h>
#if	SECURE_NFS
#import <rpc/auth_des.h>
#endif	SECURE_NFS
#import <rpc/svc.h>
#import <nfs/nfs.h>
#import <nfs/export.h>

#define eqfsid(fsid1, fsid2)	\
	(bcmp((char *)fsid1, (char *)fsid2, (int)sizeof(fsid_t)) == 0)

#define eqfid(fid1, fid2) \
	((fid1)->fid_len == (fid2)->fid_len && \
	bcmp((char *)(fid1)->fid_data, (char *)(fid2)->fid_data,  \
	(int)(fid1)->fid_len) == 0)

#define exportmatch(exi, fsid, fid) \
	(eqfsid(&(exi)->exi_fsid, fsid) && eqfid((exi)->exi_fid, fid))

#ifdef NFSDEBUG 
extern int nfsdebug; 
#endif

struct exportinfo *exported;	/* the list of exported filesystems */


/*
 * Exportfs system call
 */
exportfs()
{
	register struct a {
		char *dname;
		struct export *uex;
	} *uap = (struct a *)u.u_ap;
	struct vnode *vp;
	struct export *kex;
	struct exportinfo **tail;
	struct exportinfo *exi;
	struct exportinfo *tmp;
	struct fid *fid;
	struct vfs *vfs;

	if (! suser()) {
		u.u_error = EPERM;
		return;
	}

	/*
	 * Get the vfs id
	 */
	u.u_error = lookupname(uap->dname, UIO_USERSPACE, FOLLOW_LINK, 
		(struct vnode **) NULL, &vp);
	if (u.u_error) {
		return;	
	}
	u.u_error = VOP_FID(vp, &fid);
	vfs = vp->v_vfsp;
	VN_RELE(vp);
	if (u.u_error) {
		return;	
	}

	if (uap->uex == NULL) {
		u.u_error = unexport(&vfs->vfs_fsid, fid);
		freefid(fid);
		return;
	}
	exi = (struct exportinfo *) mem_alloc(sizeof(struct exportinfo));
	exi->exi_fsid  = vfs->vfs_fsid;
	exi->exi_fid = fid;
	kex = &exi->exi_export;

	/*
	 * Load in everything, and do sanity checking
	 */	
	u.u_error = copyin((caddr_t) uap->uex, (caddr_t) kex, 
		(u_int) sizeof(struct export));
	if (u.u_error) {
		goto error_return;
	}
	if (kex->ex_flags & ~(EX_RDONLY | EX_RDMOSTLY)) {
		u.u_error = EINVAL;
		goto error_return;
	}
	if (kex->ex_flags & EX_RDMOSTLY) {
		u.u_error = loadaddrs(&kex->ex_writeaddrs);
		if (u.u_error) {
			goto error_return;
		}
	}
	switch (kex->ex_auth) {
	case AUTH_UNIX:
		u.u_error = loadaddrs(&kex->ex_unix.rootaddrs);
		break;
#if	SECURE_NFS
	case AUTH_DES:
		u.u_error = loadrootnames(kex);
		break;
#endif	SECURE_NFS
	default:
		u.u_error = EINVAL;
	}
	if (u.u_error) {	
		goto error_return;
	}

	/*
	 * Commit the new information to the export list, making
	 * sure to delete the old entry for the fs, if one exists.
	 */
	tail = &exported;
	while (*tail != NULL) {
		if (exportmatch(*tail, &exi->exi_fsid, exi->exi_fid)) {
			tmp = *tail;
			*tail = (*tail)->exi_next;
			exportfree(tmp);
		} else {
			tail = &(*tail)->exi_next;
		}
	}
	exi->exi_next = NULL;
	*tail = exi;
	return;

error_return:	
	freefid(exi->exi_fid);
	mem_free((char *) exi, sizeof(struct exportinfo));
}


/*
 * Remove the exported directory from the export list
 */
unexport(fsid, fid)
	fsid_t *fsid;
	struct fid *fid;
{
	struct exportinfo **tail;	
	struct exportinfo *exi;

	tail = &exported;
	while (*tail != NULL) {
		if (exportmatch(*tail, fsid, fid)) {
			exi = *tail;
			*tail = (*tail)->exi_next;
			exportfree(exi);
			return (0);
		} else {
			tail = &(*tail)->exi_next;
		}
	}
	return (EINVAL);
}

/*
 * Get file handle system call.
 * Takes file name and returns a file handle for it.
 * Also recognizes the old getfh() which takes a file 
 * descriptor instead of a file name, and does the 
 * right thing. This compatibility will go away in 5.0.
 * It goes away because if a file descriptor refers to
 * a file, there is no simple way to find its parent 
 * directory.  
 */
nfs_getfh()
{
	register struct a {
		char *fname;
		fhandle_t   *fhp;
	} *uap = (struct a *) u.u_ap;
	register struct file *fp;
	fhandle_t fh;
	struct vnode *vp;
	struct vnode *dvp;
	struct exportinfo *exi;	
	int error;
	int oldgetfh = 0;

	if (!suser()) {
		u.u_error = EPERM;
		return;
	}
	if ((u_int)uap->fname < NOFILE) {
		/*
		 * old getfh()
		 */
		extern struct fileops vnodefops;

		oldgetfh = 1;
		fp = getf((int)uap->fname);
		if (fp == NULL || fp->f_ops != &vnodefops) {
			u.u_error = EINVAL;   
			return;
		}
		vp = (struct vnode *)fp->f_data;
		dvp = NULL;
	} else {
		/*
		 * new getfh()
		 */
		u.u_error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW_LINK, 
				       &dvp, &vp);
		if (u.u_error == EEXIST) {
			/*
			 * if fname resolves to / we get EEXIST error
			 * since we wanted the parent vnode. Try again
		 	 * with NULL dvp.
			 */
			u.u_error = lookupname(uap->fname, UIO_USERSPACE,
			       FOLLOW_LINK, (struct vnode **) NULL, &vp);
			dvp = NULL;
		}
		if (u.u_error == 0 && vp == NULL) {
			/*
			 *  Last component of fname not found
			 */
			if (dvp) {
				VN_RELE(dvp);
			}
			u.u_error = ENOENT;
		}
		if (u.u_error) {
			return;
		}
	}
	error = findexivp(&exi, dvp, vp);
	if (!error) {
		error = makefh(&fh, vp, exi);
		if (!error) {
			error =	copyout((caddr_t)&fh, (caddr_t)uap->fhp, 
					sizeof(fh));
		}
	}
	if (!oldgetfh) {
		/* 
		 * new getfh(): release vnodes
		 */
		VN_RELE(vp);
		if (dvp != NULL) {
			VN_RELE(dvp);
		}
	}
	u.u_error = error;
}

/*
 * Common code for both old getfh() and new getfh(). 
 * If old getfh(), then dvp is NULL.
 * Strategy: if vp is in the export list, then
 * return the associated file handle. Otherwise, ".."
 * once up the vp and try again, until the root of the
 * filesystem is reached.
 */
findexivp(exip, dvp, vp)
	struct exportinfo **exip;
	struct vnode *dvp;  /* parent of vnode want fhandle of */
	struct vnode *vp;   /* vnode we want fhandle of */
{
	struct fid *fid;
	int error;

	VN_HOLD(vp);
	if (dvp != NULL) {
		VN_HOLD(dvp);
	}
	for (;;) {
		error = VOP_FID(vp, &fid);
		if (error) {
			break;
		}
		*exip = findexport(&vp->v_vfsp->vfs_fsid, fid); 
		freefid(fid);
		if (*exip != NULL) {
			/*
			 * Found the export info
			 */
			error = 0;
			break;
		}

		/*
		 * We have just failed finding a matching export.
		 * If we're at the root of this filesystem, then
		 * it's time to stop (with failure).
		 */
		if (vp->v_flag & VROOT) {
			error = EINVAL;
			break;	
		}

		/*
		 * Now, do a ".." up vp. If dvp is supplied, use it,
	 	 * otherwise, look it up.
		 */
		if (dvp == NULL) {
			error = VOP_LOOKUP(vp, "..", &dvp, u.u_cred,
					   (struct pathname *)NULL, 0);
			if (error) {
				break;
			}
		}
		VN_RELE(vp);
		vp = dvp;
		dvp = NULL;
	}
	VN_RELE(vp);
	if (dvp != NULL) {
		VN_RELE(dvp);
	}
	return (error);
}

/*
 * Make an fhandle from a vnode
 */
makefh(fh, vp, exi)
	register fhandle_t *fh;
	struct vnode *vp;
	struct exportinfo *exi;
{
	struct fid *fidp;
	int error;

	error = VOP_FID(vp, &fidp);
	if (error || fidp == NULL) {
		/*
		 * Should be something other than EREMOTE
		 */
		return (EREMOTE);
	}
	if (fidp->fid_len + exi->exi_fid->fid_len + sizeof(fsid_t) 
		> NFS_FHSIZE) 
	{
		freefid(fidp);
		return (EREMOTE);
	}
	bzero((caddr_t) fh, sizeof(*fh));
	fh->fh_fsid.val[0] = vp->v_vfsp->vfs_fsid.val[0];
	fh->fh_fsid.val[1] = vp->v_vfsp->vfs_fsid.val[1];
	fh->fh_len = fidp->fid_len;
	bcopy(fidp->fid_data, fh->fh_data, fidp->fid_len);
	fh->fh_xlen = exi->exi_fid->fid_len;
	bcopy(exi->exi_fid->fid_data, fh->fh_xdata, fh->fh_xlen);
#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "makefh: vp %x fsid %x %x len %d data %d %d\n",
		vp, fh->fh_fsid.val[0], fh->fh_fsid.val[1], fh->fh_len,
		*(int *)fh->fh_data, *(int *)&fh->fh_data[sizeof(int)]);
#endif
	freefid(fidp);
	return (0);
}

/*
 * Find the export structure associated with the given filesystem
 */
struct exportinfo *
findexport(fsid, fid)
	fsid_t *fsid;	
	struct fid *fid;
{
	struct exportinfo *exi;

	for (exi = exported; exi != NULL; exi = exi->exi_next) {
		if (exportmatch(exi, fsid, fid)) {
			return (exi);
		}
	}
	return (NULL);
}

/*
 * Load from user space, a list of internet addresses into kernel space
 */
loadaddrs(addrs)
	struct exaddrlist *addrs;
{
	int error;
	int allocsize;
	struct sockaddr *uaddrs;

	if (addrs->naddrs > EXMAXADDRS) {
		return (EINVAL);
	}
	allocsize = addrs->naddrs * sizeof(struct sockaddr);
	uaddrs = addrs->addrvec;

#if	NeXT
	/*
	 * Don't allocate zero bytes.
	 */
	if (allocsize == 0) {
		addrs->addrvec = 0;
		return 0;
	}
#endif	NeXT
	addrs->addrvec = (struct sockaddr *)mem_alloc(allocsize);
	error = copyin((caddr_t)uaddrs, (caddr_t)addrs->addrvec,
		       (u_int)allocsize);	
	if (error) {
		mem_free((char *)addrs->addrvec, allocsize);
	}
	return (error);
}


#if	SECURE_NFS
/*
 * Load from user space the root user names into kernel space
 * (AUTH_DES only)
 */
loadrootnames(kex)
	struct export *kex;
{
	int error;
	char *exnames[EXMAXROOTNAMES];
	int i;
	u_int len;
	char netname[MAXNETNAMELEN+1];
	u_int allocsize;

	if (kex->ex_des.nnames > EXMAXROOTNAMES) {
		return (EINVAL);
	}

	/*
	 * Get list of names from user space
	 */
	allocsize =  kex->ex_des.nnames * sizeof(char *);
	error = copyin((char *)kex->ex_des.rootnames, (char *)exnames,
		allocsize);
	if (error) {
		return (error);
	}
	kex->ex_des.rootnames = (char **) mem_alloc(allocsize);
	bzero((char *) kex->ex_des.rootnames, allocsize);

	/*
	 * And now copy each individual name
	 */
	for (i = 0; i < kex->ex_des.nnames; i++) {
		error = copyinstr(exnames[i], netname, sizeof(netname), &len);
		if (error) {
			goto freeup;
		}
		kex->ex_des.rootnames[i] = mem_alloc(len + 1);
		bcopy(netname, kex->ex_des.rootnames[i], len);
		kex->ex_des.rootnames[i][len] = 0;
	}
	return (0);

freeup:
	freenames(kex);
	return (error);
}

/*
 * Figure out everything we allocated in a root user name list in
 * order to free it up. (AUTH_DES only)
 */
freenames(ex)
	struct export *ex;
{
	int i;

	for (i = 0; i < ex->ex_des.nnames; i++) {
		if (ex->ex_des.rootnames[i] != NULL) {
			mem_free((char *) ex->ex_des.rootnames[i],
				strlen(ex->ex_des.rootnames[i]) + 1);
		}
	}	
	mem_free((char *) ex->ex_des.rootnames, ex->ex_des.nnames * sizeof(char *));
}
#endif	SECURE_NFS

/*
 * Free an entire export list node
 */
exportfree(exi)
	struct exportinfo *exi;
{
	struct export *ex;

	ex = &exi->exi_export;
	switch (ex->ex_auth) {
	case AUTH_UNIX:
#if	NeXT
		/*
		 * Don't deallocate zero bytes.
		 */
		if (ex->ex_unix.rootaddrs.naddrs == 0)
			break;
#endif	NeXT
		mem_free((char *)ex->ex_unix.rootaddrs.addrvec, 
			 (ex->ex_unix.rootaddrs.naddrs * 
			  sizeof(struct sockaddr)));
		break;
#if	SECURE_NFS
	case AUTH_DES:
		freenames(ex);
		break;
#endif	SECURE_NFS
	}
	if (ex->ex_flags & EX_RDMOSTLY) {
#if	NeXT
	    /*
	     * Don't deallocate zero bytes.
	     */
	    if (ex->ex_writeaddrs.naddrs)
#endif	NeXT
		mem_free((char *)ex->ex_writeaddrs.addrvec,
			 ex->ex_writeaddrs.naddrs * sizeof(struct sockaddr));
	}
	freefid(exi->exi_fid);
	mem_free(exi, sizeof(struct exportinfo));
}

