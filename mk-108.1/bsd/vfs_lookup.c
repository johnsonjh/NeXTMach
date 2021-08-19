/* 
 * HISTORY
 *  4-Apr-90  Brian Pinkerton at NeXT
 *	Added ability to cache symbolic link contents in dnlc.  
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: Removed dir.h.  Other misc. changes.
 *
 *  2-Aug-88  Peter King (king) at NeXT
 *	Merged in D/NFS release.
 *
 * 27-Aug-87  Peter King (king) at NeXT
 *	Original Sun source, upgraded to Mach.
 */ 

/* @(#)vfs_lookup.c	1.4 87/09/30 D/NFS */
/* @(#)vfs_lookup.c    1.3 87/09/30 3.2/4.3NFSSRC */
/*	@(#)vfs_lookup.c 1.1 86/09/25 SMI	*/

#import <sys/param.h>
#import <sys/user.h>
#import <sys/uio.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/dirent.h>
#import <sys/pathname.h>
#if	NeXT
#import <sys/dnlc.h>
#endif	NeXT
#import <sys/metalink.h>
#import <kern/assert.h>

static int getsymlink();

/*
 * lookup the user file name,
 * Handle allocation and freeing of pathname buffer, return error.
 */
lookupname(fnamep, seg, followlink, dirvpp, compvpp)
	char *fnamep;			/* user pathname */
	int seg;			/* addr space that name is in */
	enum symfollow followlink;	/* follow sym links */
	struct vnode **dirvpp;		/* ret for ptr to parent dir vnode */
	struct vnode **compvpp;		/* ret for ptr to component vnode */
{
	struct pathname lookpn;
	register int error;

	error = pn_get(fnamep, seg, &lookpn);
	if (error)
		return (error);
	error = lookuppn(&lookpn, followlink, dirvpp, compvpp);
	pn_free(&lookpn);
	return (error);
}

/*
 * Starting at current directory, translate pathname pnp to end.
 * Leave pathname of final component in pnp, return the vnode
 * for the final component in *compvpp, and return the vnode
 * for the parent of the final component in dirvpp.
 * If au_path is set, then return the resolved full pathname for the
 * pathname specified.
 *
 * This is the central routine in pathname translation and handles
 * multiple components in pathnames, separating them at /'s.  It also
 * implements mounted file systems and processes symbolic links.
 */
lookuppn(pnp, followlink, dirvpp, compvpp)
	register struct pathname *pnp;		/* pathaname to lookup */
	enum symfollow followlink;		/* (don't) follow sym links */
	struct vnode **dirvpp;			/* ptr for parent vnode */
	struct vnode **compvpp;			/* ptr for entry vnode */
{
	register struct vnode *vp;		/* current directory vp */
	register struct vnode *cvp;		/* current component vp */
	struct vnode *tvp;			/* non-reg temp ptr */
	register struct vfs *vfsp;		/* ptr to vfs for mount indir */
	char component[MAXNAMLEN+1];		/* buffer for component */
	register int error;
	register int nlink;
	int lookup_flags;

	nlink = 0;
	cvp = (struct vnode *)0;
	lookup_flags = 0;

	/*
	 * start at current directory.
	 */
	vp = u.u_cdir;
	ASSERT(vp != NULL);
	VN_HOLD(vp);

begin:
	/*
	 * Each time we begin a new name interpretation (e.g.
	 * when first called and after each symbolic link is
	 * substituted), we allow the search to start at the
	 * root directory if the name starts with a '/', otherwise
	 * continuing from the current directory.
	 */
	component[0] = 0;
	if (pn_peekchar(pnp) == '/') {
		VN_RELE(vp);
		pn_skipslash(pnp);
		if (u.u_rdir)
			vp = u.u_rdir;
		else
			vp = rootdir;
		VN_HOLD(vp);
	}

next:
	/*
	 * Make sure we have a directory.
	 */
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto bad;
	}
	/*
	 * Process the next component of the pathname.
	 */
	error = pn_stripcomponent(pnp, component);
	if (error)
		goto bad;

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. "/." or ".".
	 */
	if (component[0] == 0) {
		/*
		 * If the caller was interested in the parent then
		 * return an error since we don't have the real parent
		 * Return EEXIST in this case, to disambiguate EINVAL.
		 */
		if (dirvpp != (struct vnode **)0) {
			VN_RELE(vp);
			return(EEXIST);
		}
		(void) pn_set(pnp, ".");
		if (compvpp != (struct vnode **)0) {
			*compvpp = vp;
			ASSERT(   *compvpp == (struct vnode *)0
			       || *compvpp > (struct vnode *)0x400000);
		} else {
			VN_RELE(vp);
		}
		return(0);
	}

	/*
	 * Handle "..": two special cases.
	 * 1. If at root directory (e.g. after chroot)
	 *    then ignore it so can't get out.
	 * 2. If this vnode is the root of a mounted
	 *    file system, then replace it with the
	 *    vnode which was mounted on so we take the
	 *    .. in the other file system.
#if	NeXT
	 * 3. If this vnode is a fake entry, then make
	 *    the parent the cvp and skip to next lookup.
#endif	NeXT
	 */
	if (strcmp(component, "..") == 0) {
checkforroot:
		if (VN_CMP(vp, u.u_rdir) || VN_CMP(vp, rootdir)) {
			cvp = vp;
			VN_HOLD(cvp);
			goto skip;
		}
		if (vp->v_flag & VROOT) {
			cvp = vp;
			vp = vp->v_vfsp->vfs_vnodecovered;
			VN_HOLD(vp);
			VN_RELE(cvp);
#if	NeXT
			if (vp->v_vfsmountedhere == (struct vfs *)0) {
				/*
				 * We must be in the fake entry list,
				 * make the component an alias for the
				 * directory and skip to next lookup.
				 */
				cvp = vp;
				VN_HOLD(cvp);
				goto skip;
			}
#endif	NeXT
			cvp = (struct vnode *)0;
			goto checkforroot;
		}
	}

#if	NeXT
	/*
	 * Check the virtual mount list first.  If we succeed, we
	 * transparently indirect to the vnode which is the root of
	 * the mounted filesystem.  Before we do this we must check
	 * that an unmount is not in progress on this vnode.  this
	 * maintains the fs status quo while a possibly lengthy
	 * unmount is going on.
	 */
nmloop:
	if (vp->v_vfsentries) {
		/*
		 * Make sure we can look in this directory.
		 */
		if (error = VOP_ACCESS(vp, VEXEC, u.u_cred))
			goto bad;

		/*
		 * Now scan the fake entry list
		 */
		for(vfsp = vp->v_vfsentries; vfsp; vfsp = vfsp->vfs_nextentry) {
			if (strncmp(vfsp->vfs_name, component, MAXNAMLEN) == 0) {
				while(vfsp->vfs_flag & VFS_MLOCK) {
					vfsp->vfs_flag |= VFS_MWAIT;
					(void) sleep((caddr_t)vfsp, PVFS);
					goto nmloop;
				}
				error = VFS_ROOT(vfsp, &tvp);
				if (error)
					goto bad;
				cvp = tvp;
				goto skip;
			}
		}
	}
#endif	NeXT		

	/*
	 * Perform a lookup in the current directory.
	 */
	error = VOP_LOOKUP(vp, component, &tvp, u.u_cred, pnp, lookup_flags);
	cvp = tvp;
	if (error) {
		cvp = (struct vnode *)0;
		/*
		 * On error, if more pathname or if caller was not interested
		 * in the parent directory then hard error.
		 * If the path is unreadable, fail now with the right error.
		 */
		if (pn_pathleft(pnp) || dirvpp == (struct vnode **)0 ||
		    (error == EACCES))
			goto bad;
		(void) pn_set(pnp, component);
		*dirvpp = vp;
		if (compvpp != (struct vnode **)0) {
			*compvpp = (struct vnode *)0;
			ASSERT(   *compvpp == (struct vnode *)0
			       || *compvpp > (struct vnode *)0x400000);
		}
		return (0);
	}
	/*
	 * If we hit a symbolic link and there is more path to be
	 * translated or this operation does not wish to apply
	 * to a link, then place the contents of the link at the
	 * front of the remaining pathname.
	 */
	if (cvp->v_type == VLNK &&
	    ((followlink == FOLLOW_LINK) || pn_pathleft(pnp))) {
		struct pathname linkpath;

		nlink++;
		if (nlink > MAXSYMLINKS) {
			error = ELOOP;
			goto bad;
		}
		error = getsymlink(cvp, component, vp, &linkpath);
		if (error)
			goto bad;
		if (pn_pathleft(&linkpath) == 0)
			(void) pn_set(&linkpath, ".");
		error = pn_combine(pnp, &linkpath);	/* linkpath before pn */
		pn_free(&linkpath);
		if (error)
			goto bad;
		VN_RELE(cvp);
		cvp = (struct vnode *)0;
		goto begin;
	}

	/*
	 * If this vnode is mounted on, then we
	 * transparently indirect to the vnode which 
	 * is the root of the mounted file system.
	 * Before we do this we must check that an unmount is not
	 * in progress on this vnode. This maintains the fs status
	 * quo while a possibly lengthy unmount is going on.
	 */
mloop:
	while (vfsp = cvp->v_vfsmountedhere) {
		while (vfsp->vfs_flag & VFS_MLOCK) {
			vfsp->vfs_flag |= VFS_MWAIT;
			(void) sleep((caddr_t)vfsp, PVFS);
			goto mloop;
		}
		error = VFS_ROOT(cvp->v_vfsmountedhere, &tvp);
		if (error)
			goto bad;
		VN_RELE(cvp);
		cvp = tvp;
	}

skip:
	/*
	 * Skip to next component of the pathname.
	 * If no more components, return last directory (if wanted)  and
	 * last component (if wanted).
	 */
	if (pn_pathleft(pnp) == 0) {
		(void) pn_set(pnp, component);
		if (dirvpp != (struct vnode **)0) {
			/*
			 * Check that we have the real parent and not
			 * an alias of the last component.
			 * Return EEXIST in this case, to disambiguate EINVAL.
			 */
			if (VN_CMP(vp, cvp)) {
				VN_RELE(vp);
				VN_RELE(cvp);
				return(EEXIST);
			}
			*dirvpp = vp;
		} else {
			VN_RELE(vp);
		}
		if (compvpp != (struct vnode **)0) {
			*compvpp = cvp;
			ASSERT(   *compvpp == (struct vnode *)0
			       || *compvpp > (struct vnode *)0x400000);
		} else {
			VN_RELE(cvp);
		}
		return (0);
	}
	/*
	 * skip over slashes from end of last component
	 */
	pn_skipslash(pnp);

	/*
	 * Searched through another level of directory:
	 * release previous directory handle and save new (result
	 * of lookup) as current directory.
	 */
	VN_RELE(vp);
	vp = cvp;
	cvp = (struct vnode *)0;
	goto next;

bad:
	/*
	 * Error. Release vnodes and return.
	 */
	if (cvp)
		VN_RELE(cvp);
	VN_RELE(vp);
	return (error);
}

/*
 * Gets symbolic link into pathname.
 */
int
getsymlink(vp, linkName, dvp, pnp)
	struct vnode *vp;
	char *linkName;
	struct vnode *dvp;
	struct pathname *pnp;
{
	struct iovec aiov;
	struct uio auio;
	register int error = 0;
	char *cp;			/* METALINK */
	extern char *index();		/* METALINK */
	struct ncache *ncp;

	pn_alloc(pnp);

#if	NeXT
	/*
	 *  Check the contents of the dnlc for the value of this symbolic
	 *  link.  If we succeed, just return the contents.  Otherwise we
	 */	 
	ncp = dnlc_lookupSymLink(linkName, dvp);
	
	if (ncp != (struct ncache *) 0 && ncp->symLinkValid) {	/* cache hit */
	
		bcopy(ncp->symLink, pnp->pn_buf, ncp->symLinkLength);
		pnp->pn_pathlen = ncp->symLinkLength;
		
	} else {						/* cache miss */
	
		aiov.iov_base = pnp->pn_buf;		/* fill in UIO struct */
		aiov.iov_len = MAXPATHLEN;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_resid = MAXPATHLEN;
		
		error = VOP_READLINK(vp, &auio, u.u_cred);	/* do the work */
		
		pnp->pn_pathlen = MAXPATHLEN - auio.uio_resid;
			
		if (error) {
			pn_free(pnp);
			return(error);
		}
		
		dnlc_enterSymLink(linkName, dvp, pnp);/* put result in the cache */
	}
	
#else	NeXT

	aiov.iov_base = pnp->pn_buf;		/* fill in UIO struct */
	aiov.iov_len = MAXPATHLEN;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = MAXPATHLEN;
	
	error = VOP_READLINK(vp, &auio, u.u_cred);	/* do the work */
	
	pnp->pn_pathlen = MAXPATHLEN - auio.uio_resid;
		
	if (error) {
		pn_free(pnp);
		return(error);
	}
#endif	NeXT
	
/* BEGIN NEXT_METALINK */
	pnp->pn_buf[pnp->pn_pathlen] = '\0'; /* So index won't fail */
	/*
	 * Check to see if there are any metalink expansions
	 */
	cp = pnp->pn_buf;
	while (cp = index(cp, ML_ESCCHAR)) {
		if ( (cp == pnp->pn_buf) || (*(cp-1) == '/') )
			break;
		else
			cp++;	/* Move past the ML_ESCCHAR */
	}
	if (cp) {
		struct pathname metapath;
		char metacomponent[MAXNAMLEN+1];

		pn_alloc(&metapath);
	        while (pn_pathleft(pnp)) {
			/*
			 * Check first for an initial slash and move it to the
			 * metapath if need be.
			 */
			if (pn_peekchar(pnp) == '/') {
				error = pn_append(&metapath, "/");
				if (error)
					break;
				pn_skipslash(pnp);
			}
			error = pn_getcomponent(pnp, metacomponent);
			if (error)
				break;
			if (metacomponent[0] == ML_ESCCHAR) {
				struct metalink *ml;

				for (ml = metalinks; ml->ml_token; ml++) {
					if (strcmp(&metacomponent[1],
						   ml->ml_token) == 0)
						break;
				}
				if ( !ml->ml_token ) {
					error = ENOENT;
					break;
				}
				if ( ml->ml_variable[0] == '\0' ) {
					/*
					 * Variable has not been set yet.
					 * We should try the default
					 */
					if (ml->ml_default) {
						error = pn_append(&metapath,
								  ml->ml_default);
					} else {
						error = ENOENT;
					}
				} else {
					error = pn_append(&metapath,
							  ml->ml_variable);
				}
			} else {
				error = pn_append(&metapath, metacomponent);
			}
			if (error)
				break;
		}
		if (!error) {
			error = pn_set(pnp, metapath.pn_buf);
		}
		pn_free(&metapath);
	}
/* END NEXT_METALINK */
	if (error)
		pn_free(pnp);
	return (error);
}










