/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 *  1-May-90  Morris Meyer (mmeyer) at NeXT
 *	Correct fcntl() to work for the socketop case of F_GETOWN and
 *	F_SETOWN.
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: File locking cleanup.
 *
 * 18-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Merge VFS changes:
 *			Support credentials record user in fd's.
 *	SUN_LOCK:	Release record locks in dup2() and close().
 *			Add F_GETLK(et.al.) to fcntl().
 */
 
#import <sun_lock.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_descrip.c	7.1 (Berkeley) 6/5/86
 */

/* @(#)kern_descrip.c	2.3 88/06/17 4.0NFSSRC SMI;	from UCB 7.1 6/5/86 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/proc.h>
#import <sys/file.h>
#import <sys/stat.h>

#import <sys/ioctl.h>

/*
 * Descriptor management.
 */

/*
 * TODO:
 *	eliminate u.u_error side effects
 */

/*
 * System calls on descriptors.
 */
getdtablesize()
{

	u.u_r.r_val1 = NOFILE;
}

getdopt()
{

}

setdopt()
{

}

dup()
{
	register struct a {
		int	i;
	} *uap = (struct a *) u.u_ap;
	struct file *fp;
	int j;

	if (uap->i &~ 077) { uap->i &= 077; dup2(); return; }	/* XXX */

	GETF(fp, uap->i);
	j = ufalloc(0);
	if (j < 0)
		return;
	dupit(j, fp, u.u_pofile[uap->i]);
}

dup2()
{
	register struct a {
		int	i, j;
	} *uap = (struct a *) u.u_ap;
	register struct file *fp;

	GETF(fp, uap->i);
	if (uap->j < 0 || uap->j >= NOFILE) {
		u.u_error = EBADF;
		return;
	}
	u.u_r.r_val1 = uap->j;
	if (uap->i == uap->j)
		return;
#if	NeXT
	expand_fdlist(current_task()->u_address, uap->j);
#endif	NeXT
	if (u.u_ofile[uap->j]) {
#if	SUN_LOCK
		/* Release all System-V style record locks, if any */
		(void) vno_lockrelease(u.u_ofile[uap->j]);	/* errors? */
#endif	SUN_LOCK
		if (u.u_pofile[uap->j] & UF_MAPPED)
			munmapfd(uap->j);
		closef(u.u_ofile[uap->j]);
		/*
		 * Even if an error occurred when calling the close routine
		 * for the vnode or the device, the file table entry has
		 * had its reference count decremented anyway.  As such,
		 * the descriptor is closed, so there's not much point
		 * in worrying about errors; we might as well pretend
		 * the "close" succeeded.
		 */
		u.u_error = 0;
	}
	dupit(uap->j, fp, u.u_pofile[uap->i]);
}

dupit(fd, fp, flags)
	int fd;
	register struct file *fp;
	register int flags;
{

#if	NeXT
	expand_fdlist(current_task()->u_address, fd);
#endif	NeXT
	u.u_ofile[fd] = fp;
	u.u_pofile[fd] = flags &~ UF_EXCLOSE;
	fp->f_count++;
	if (fd > u.u_lastfile)
		u.u_lastfile = fd;
}

/*
 * The file control system call.
 */
fcntl()
{
	register struct file *fp;
	register struct a {
		int	fdes;
		int	cmd;
		int	arg;
	} *uap;
	register i;
	register char *pop;
#if	SUN_LOCK
	struct flock ld;
	register int oldwhence;
#endif	SUN_LOCK
	register int newflag;
	int fioarg;

	uap = (struct a *)u.u_ap;
	GETF(fp, uap->fdes);
	pop = &u.u_pofile[uap->fdes];
	switch(uap->cmd) {
	case F_DUPFD:
		i = uap->arg;
		if (i < 0 || i >= NOFILE) {
			u.u_error = EINVAL;
			return;
		}
		if ((i = ufalloc(i)) < 0)
			return;
		dupit(i, fp, *pop &~ UF_EXCLOSE);
		break;

	case F_GETFD:
		u.u_r.r_val1 = *pop & 1;
		break;

	case F_SETFD:
		*pop = (*pop &~ 1) | (uap->arg & 1);
		break;

	case F_GETFL:
		u.u_r.r_val1 = fp->f_flag+FOPEN;
		break;

	case F_SETFL:
		/*
		 * XXX Actually, there should not be any connection
		 * between the "ioctl"-settable per-object FIONBIO
		 * and FASYNC flags and any per-file-descriptor flags,
		 * so this call should simply fiddle fp->f_flag.
		 * Unfortunately, 4.2BSD has such a connection, so we
		 * must support that.  Thus, we must pass the new
		 * values of the FNDELAY and FASYNC flags down to the
		 * object by doing the appropriate "ioctl"s.
		 */
		newflag = fp->f_flag;
		newflag &= FCNTLCANT;
		newflag |= (uap->arg-FOPEN) &~ FCNTLCANT;
		fioarg = (newflag & FNDELAY) != 0;
		u.u_error = fioctl(fp, FIONBIO, (caddr_t) &fioarg);
		if (u.u_error)
			break;
		fioarg = (newflag & FASYNC) != 0;
		u.u_error = fioctl(fp, FIOASYNC, (caddr_t) &fioarg);
		if (u.u_error) {
			fioarg = (fp->f_flag & FNDELAY) != 0;
			(void) fioctl(fp, FIONBIO, (caddr_t) &fioarg);
			break;
		}
		fp->f_flag = newflag;
		break;

	case F_GETOWN:
#if	NeXT
		u.u_error = fgetown(fp, &u.u_r.r_val1);
#else
		u.u_error = fioctl(fp, FIOGETOWN, (caddr_t) &u.u_r.r_val1);
#endif	NeXT
		break;

	case F_SETOWN:
#if	NeXT
		u.u_error = fsetown(fp, uap->arg);
#else
		u.u_error = fioctl(fp, FIOSETOWN, (caddr_t) &uap->arg);
#endif	NeXT
		break;

#if	SUN_LOCK
       /* System-V Record-locking (lockf() maps to fcntl()) */
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		/* First off, allow only vnodes here */
		if (fp->f_type != DTYPE_VNODE) {
			u.u_error = EBADF;
			return;
		}
		/* get flock structure from user-land */
		if (u.u_error =
		    copyin((caddr_t)uap->arg, (caddr_t)&ld, sizeof (ld))) {
			return;
		}

		/*
		 * *** NOTE ***
		 * The SVID does not say what to return on file access errors!
		 * Here, EBADF is returned, which is compatible with S5R3
		 * and is less confusing than EACCES
		 */
		switch (ld.l_type) {
		case F_RDLCK:
			if ((uap->cmd != F_GETLK) && !(fp->f_flag & FREAD)) {
				u.u_error = EBADF;
				return;
			}
			break;

		case F_WRLCK:
			if ((uap->cmd != F_GETLK) && !(fp->f_flag & FWRITE)) {
				u.u_error = EBADF;
				return;
			}
			break;

		case F_UNLCK:
			break;

		default:
			u.u_error = EINVAL;
			return;
		}
		
		/* convert offset to start of file */
		oldwhence = ld.l_whence;	/* save to renormalize later */

		if (u.u_error = rewhence(&ld, fp, 0))
			return;

		/* convert negative lengths to positive */
		if (ld.l_len < 0) {
			ld.l_start += ld.l_len;		/* adjust start point */
			ld.l_len = -(ld.l_len);		/* absolute value */
		}
	 
		/* check for validity */
		if (ld.l_start < 0) {
			u.u_error = EINVAL;
			return;
		}

		if ((uap->cmd != F_GETLK) && (ld.l_type != F_UNLCK)) {
			/*
			 * If any locking is attempted, mark file locked
			 * to force unlock on close.
			 * Also, since the SVID specifies that the FIRST
			 * close releases all locks, mark process to
			 * reduce the search overhead in vno_lockrelease().
			 */
			*pop |= UF_FDLOCK;
			u.u_procp->p_flag |= SLKDONE;
		}

		/*
		 * Dispatch out to vnode layer to do the actual locking.
		 * Then, translate error codes for SVID compatibility
		 */
		switch (u.u_error = VOP_LOCKCTL((struct vnode *)fp->f_data,
		    &ld, uap->cmd, fp->f_cred,u.u_procp->p_pid)) {
		case 0:
			break;		/* continue, if successful */
		case EWOULDBLOCK:
			u.u_error = EACCES;	/* EAGAIN ??? */
			return;
		default:
			return;		/* some other error code */
		}

		/* if F_GETLK, return flock structure to user-land */
		if (uap->cmd == F_GETLK) {
			/* per SVID, change only 'l_type' field if unlocked */
			if (ld.l_type == F_UNLCK) {
				if (u.u_error = copyout((caddr_t)&ld.l_type,
				    (caddr_t)&((struct flock*)uap->arg)->l_type,
				    sizeof (ld.l_type))) {
					return;
				}
			} else {
				if (u.u_error = rewhence(&ld, fp, oldwhence))
					return;
				if (u.u_error = copyout((caddr_t)&ld,
				    (caddr_t)uap->arg, sizeof (ld))) {
					return;
				}
			}
		}
		break;
#endif	SUN_LOCK
	default:
		u.u_error = EINVAL;
	}
}

fset(fp, bit, value)
	struct file *fp;
	int bit, value;
{

	if (value)
		fp->f_flag |= bit;
	else
		fp->f_flag &= ~bit;
	return (fioctl(fp, (int)(bit == FNDELAY ? FIONBIO : FIOASYNC),
	    (caddr_t)&value));
}

fgetown(fp, valuep)
	struct file *fp;
	int *valuep;
{
	int error;

	switch (fp->f_type) {

	case DTYPE_SOCKET:
		*valuep = ((struct socket *)fp->f_data)->so_pgrp;
		return (0);

	default:
		error = fioctl(fp, (int)TIOCGPGRP, (caddr_t)valuep);
		*valuep = -*valuep;
		return (error);
	}
}

fsetown(fp, value)
	struct file *fp;
	int value;
{

	if (fp->f_type == DTYPE_SOCKET) {
		((struct socket *)fp->f_data)->so_pgrp = value;
		return (0);
	}
	if (value > 0) {
		struct proc *p = pfind(value);
		if (p == 0)
			return (ESRCH);
		value = p->p_pgrp;
	} else
		value = -value;
	return (fioctl(fp, (int)TIOCSPGRP, (caddr_t)&value));
}

fioctl(fp, cmd, value)
	struct file *fp;
	int cmd;
	caddr_t value;
{

	return ((*fp->f_ops->fo_ioctl)(fp, cmd, value));
}

close()
{
	struct a {
		int	i;
	} *uap = (struct a *)u.u_ap;
	register int i = uap->i;
	register struct file *fp;
	register u_char *pf;

	GETF(fp, i);
#if	SUN_LOCK
	/* Release all System-V style record locks, if any */
	(void) vno_lockrelease(fp);	/* WHAT IF error returned? */
#endif	SUN_LOCK
	pf = (u_char *)&u.u_pofile[i];
	if (*pf & UF_MAPPED)
		munmapfd(i);
	u.u_ofile[i] = NULL;
	while (u.u_lastfile >= 0 && u.u_ofile[u.u_lastfile] == NULL)
		u.u_lastfile--;
	*pf = 0;
	closef(fp);
	/* WHAT IF u.u_error ? */
}

fstat()
{
	register struct file *fp;
	register struct a {
		int	fdes;
		struct	stat *sb;
	} *uap;
	struct stat ub;

	uap = (struct a *)u.u_ap;
	GETF(fp, uap->fdes);
	switch (fp->f_type) {

	case DTYPE_VNODE:
		u.u_error = vno_stat((struct vnode *)fp->f_data, &ub);
		break;

	case DTYPE_SOCKET:
		u.u_error = soo_stat((struct socket *)fp->f_data, &ub);
		break;

	default:
		panic("fstat");
		/*NOTREACHED*/
	}
	if (u.u_error == 0)
		u.u_error = copyout((caddr_t)&ub, (caddr_t)uap->sb,
		    sizeof (ub));
}

/*
 * Allocate a user file descriptor.
 */
ufalloc(i)
	register int i;
{

	for (; i < NOFILE; i++)
#if	NeXT
	{
		expand_fdlist(current_task()->u_address, i);
#endif	NeXT
		if (u.u_ofile[i] == NULL) {
			u.u_r.r_val1 = i;
			u.u_pofile[i] = 0;
			if (i > u.u_lastfile)
				u.u_lastfile = i;
			return (i);
		}
#if	NeXT
	}
#endif	NeXT
	u.u_error = EMFILE;
	return (-1);
}

ufavail()
{
	register int i, avail = 0;

	for (i = 0; i < NOFILE; i++)
#if	NeXT
		if (i < u.u_ofile_cnt && u.u_ofile[i] == NULL)
#else	NeXT
		if (u.u_ofile[i] == NULL)
#endif	NeXT
			avail++;
	return (avail);
}

struct zone	*file_zone;
int		max_file = 10000;	/* XXX */

file_init()
{
	vm_size_t	size;

	queue_init(&file_list);
	size = sizeof(struct file);
	file_zone = zinit(size, size*max_file, 0, FALSE,
		"file structs");
}

/*
 * Allocate a user file descriptor
 * and a file structure.
 * Initialize the descriptor
 * to point at the file structure.
 */
struct file *
falloc()
{
	register struct file *fp;
	register i;

	i = ufalloc(0);
	if (i < 0)
		return (NULL);
	fp = (struct file *) zalloc(file_zone);
	queue_enter(&file_list, fp, struct file *, links);
	u.u_ofile[i] = fp;
	fp->f_count = 1;
	fp->f_data = 0;
	fp->f_offset = 0;
#if	NeXT
	fp->f_ops = NULL;
#endif	NeXT
	crhold(u.u_cred);
	fp->f_cred = u.u_cred;
	return (fp);
}

/*
 * Convert a user supplied file descriptor into a pointer
 * to a file structure.  Only task is to check range of the descriptor.
 * Critical paths should use the GETF macro.
 */
struct file *
getf(f)
	register int f;
{
	register struct file *fp;

	if ((unsigned)f >= NOFILE || (fp = u.u_ofile[f]) == NULL) {
		u.u_error = EBADF;
		return (NULL);
	}
	return (fp);
}

/*
 * Internal form of close.
 * Decrement reference count on file structure.
 */
closef(fp)
	register struct file *fp;
{

	if (fp == NULL)
		return;
#ifdef	notdef
	/*
	 * Unlock the file even if other references exist.
	 */
	if (fp->f_flag & (FSHLOCK | FEXLOCK)) 
	{
		ASSERT(fp->f_data != NULL);
		vno_bsd_unlock(fp, (FSHLOCK | FEXLOCK));
	}
#endif	notdef
	if (fp->f_count > 1) {
		fp->f_count--;
		return;
	}
	if (fp->f_ops != NULL)
		(*fp->f_ops->fo_close)(fp);
	crfree(fp->f_cred);
	fp->f_count = 0;
	free_file(fp);
}

free_file(fp)
	struct file	*fp;
{
	queue_remove(&file_list, fp, struct file *, links);
	zfree(file_zone, fp);
}

#if	SUN_LOCK
/*
 * Normalize SystemV-style record locks
 */
rewhence(ld, fp, newwhence)
	struct flock *ld;
	struct file *fp;
	int newwhence;
{
	struct vattr va;
	register int error;

	/* if reference to end-of-file, must get current attributes */
	if ((ld->l_whence == 2) || (newwhence == 2)) {
		if (error = VOP_GETATTR((struct vnode *)fp->f_data, &va,
		    u.u_cred))
			return(error);
	}

	/* normalize to start of file */
	switch (ld->l_whence) {
	case 0:
		break;
	case 1:
		ld->l_start += fp->f_offset;
		break;
	case 2:
		ld->l_start += va.va_size;
		break;
	default:
		return(EINVAL);
	}

	/* renormalize to given start point */
	switch (ld->l_whence = newwhence) {
	case 1:
		ld->l_start -= fp->f_offset;
		break;
	case 2:
		ld->l_start -= va.va_size;
		break;
	}
	return(0);
}
#endif	SUN_LOCK

/*
 * Apply an advisory lock on a file descriptor.
 */
flock()
{
	register struct a {
		int	fd;
		int	how;
	} *uap = (struct a *)u.u_ap;
	register struct file *fp;

	GETF(fp, uap->fd);
	if (fp->f_type != DTYPE_VNODE) {
		u.u_error = EOPNOTSUPP;
		return;
	}
	if (uap->how & LOCK_UN) {
		vno_bsd_unlock(fp, FSHLOCK|FEXLOCK);
		return;
	}
	
	if (uap->how & LOCK_EX)
		uap->how &= ~LOCK_SH;	/* can't have both types */
	else if (!(uap->how & LOCK_SH)) {
		u.u_error = EINVAL;	/* but must have one */
		return;
	}
	u.u_error = vno_bsd_lock(fp, uap->how);
}

#if	NeXT
expand_fdlist(utask, n)
	struct utask	*utask;
	int		n;
{
	struct file	**fpp;
	char		*flagsp;
	int		old_cnt, new_cnt;

	old_cnt = utask->uu_ofile_cnt;
	if (n < old_cnt)
		return;

	new_cnt = n + 1;
	fpp = (struct file **) kalloc(new_cnt * sizeof(struct file *));
	flagsp = (char *) kalloc(new_cnt * sizeof(char));
	bzero(fpp, new_cnt * sizeof(struct file *));
	bzero(flagsp, new_cnt * sizeof(char));
	if (old_cnt) {
		bcopy(utask->uu_ofile, fpp, old_cnt * sizeof(struct file *));
		bcopy(utask->uu_pofile, flagsp, old_cnt * sizeof(char));
		kfree(utask->uu_ofile, old_cnt * sizeof(struct file *));
		kfree(utask->uu_pofile, old_cnt * sizeof(char));
	}
	utask->uu_ofile = fpp;
	utask->uu_pofile = flagsp;
	utask->uu_ofile_cnt = new_cnt;
}
#endif	NeXT

