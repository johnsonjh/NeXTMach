/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 * 28-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes.
 *
 */
#import <quota.h>

#if	QUOTA
#ifndef lint
/* static char sccsid[] = "@(#)quota_syscalls.c	2.2 88/05/31 4.0NFSSRC Copyr 1988 Sun Micro"; */
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.16
 */

/*
 * Quota system calls.
 */
#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/kernel.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/uio.h>
#import <ufs/quotas.h>
#import <ufs/inode.h>
#import <ufs/mount.h>
#import <ufs/fs.h>

/*
 * Sys call to allow users to find out
 * their current position wrt quota's
 * and to allow super users to alter it.
 */
quotactl(uap)
	register struct a {
		int	cmd;
		caddr_t	fdev;
		int	uid;
		caddr_t	addr;
	} *uap;
{
	struct mount *mp;

	if (uap->uid < 0)
		uap->uid = u.u_ruid;
	if (uap->cmd == Q_SYNC && uap->fdev == NULL) {
		mp = NULL;
	} else {
		u.u_error = fdevtomp(uap->fdev, &mp);
		if (u.u_error)
			return;
	}
	switch (uap->cmd) {

	case Q_QUOTAON:
		u.u_error = opendq(mp, uap->addr);
		break;

	case Q_QUOTAOFF:
		u.u_error = closedq(mp);
		break;

	case Q_SETQUOTA:
	case Q_SETQLIM:
		u.u_error = setquota(uap->cmd, uap->uid, mp, uap->addr);
		break;

	case Q_GETQUOTA:
		u.u_error = getquota(uap->uid, mp, uap->addr);
		break;

	case Q_SYNC:
		u.u_error = qsync(mp);
		break;

	default:
		u.u_error = EINVAL;
		break;
	}
}

/* XXX */
oldquota()
{
	printf("oldquota\n");
}

/*
 * Set the quota file up for a particular file system.
 * Called as the result of a setquota system call.
 */
int
opendq(mp, addr)
	register struct mount *mp;
	caddr_t addr;			/* quota file */
{
	struct vnode *vp;
	struct dquot *dqp;
	int error;

	if (!suser())
		return (EPERM);
	error =
	    lookupname(addr, UIO_USERSPACE, FOLLOW_LINK,
		(struct vnode **)0, &vp);
	if (error)
		return (error);
	if (VFSTOM(vp->v_vfsp) != mp || vp->v_type != VREG) {
		VN_RELE(vp);
		return (EACCES);
	}
	if (mp->m_qflags & MQ_ENABLED)
		(void) closedq(mp);
	if (mp->m_qinod != NULL) {	/* open/close in progress */
		VN_RELE(vp);
		return (EBUSY);
	}
	mp->m_qinod = VTOI(vp);
	/*
	 * The file system time limits are in the super user dquot.
	 * The time limits set the relative time the other users
	 * can be over quota for this file system.
	 * If it is zero a default is used (see quota.h).
	 */
	error = getdiskquota(0, mp, 1, &dqp);
	if (error == 0) {
		mp->m_btimelimit =
		    (dqp->dq_btimelimit? dqp->dq_btimelimit: DQ_BTIMELIMIT);
		mp->m_ftimelimit =
		    (dqp->dq_ftimelimit? dqp->dq_ftimelimit: DQ_FTIMELIMIT);
		dqput(dqp);
		mp->m_qflags = MQ_ENABLED;	/* enable quotas */
	} else {
		/*
		 * Some sort of I/O error on the quota file.
		 */
		irele(mp->m_qinod);
		mp->m_qinod = NULL;
	}
	return (error);
}

/*
 * Close off disk quotas for a file system.
 */
int
closedq(mp)
	register struct mount *mp;
{
	register struct dquot *dqp;
	register struct inode *ip;
	register struct inode *qip;

	if (!suser())
		return (EPERM);
	if ((mp->m_qflags & MQ_ENABLED) == 0)
		return (0);
	qip = mp->m_qinod;
	if (qip == NULL)
		panic("closedq");
	mp->m_qflags = 0;	/* disable quotas */
loop:
	/*
	 * Run down the inode table and release all dquots assciated with
	 * inodes on this filesystem.
	 */
#if	MACH
	for (ip = inode_list; ip != NULL; ip = ip->inode_list) {
#else	MACH
	for (ip = inode; ip < inodeNINODE; ip++) {
#endif	MACH
		dqp = ip->i_dquot;
		if (dqp != NULL && dqp->dq_mp == mp) {
			if (dqp->dq_flags & DQ_LOCKED) {
				dqp->dq_flags |= DQ_WANT;
				(void) sleep((caddr_t)dqp, PINOD+2);
				goto loop;
			}
			dqp->dq_flags |= DQ_LOCKED;
			dqput(dqp);
			ip->i_dquot = NULL;
		}
	}
	/*
	 * Run down the dquot table and clean and invalidate the
	 * dquots for this file system.
	 */
	for (dqp = dquot; dqp < dquotNDQUOT; dqp++) {
		if (dqp->dq_mp == mp) {
			if (dqp->dq_flags & DQ_LOCKED) {
				dqp->dq_flags |= DQ_WANT;
				(void) sleep((caddr_t)dqp, PINOD+2);
				goto loop;
			}
			dqp->dq_flags |= DQ_LOCKED;
			if (dqp->dq_flags & DQ_MOD)
				dqupdate(dqp);
			dqinval(dqp);
		}
	}
	/*
	 * Sync and release the quota file inode.
	 */
	ilock(qip);
	(void) syncip(qip);
	iput(qip);
	mp->m_qinod = NULL;
	return (0);
}

/*
 * Set various feilds of the dqblk according to the command.
 * Q_SETQUOTA - assign an entire dqblk structure.
 * Q_SETQLIM - assign a dqblk structure except for the usage.
 */
int
setquota(cmd, uid, mp, addr)
	int cmd;
	short uid;
	struct mount *mp;
	caddr_t addr;
{
	register struct dquot *dqp;
	struct dquot *xdqp;
	struct dqblk newlim;
	int error;

	if (!suser())
		return (EPERM);
	if ((mp->m_qflags & MQ_ENABLED) == 0)
		return (ESRCH);
	error = copyin(addr, (caddr_t)&newlim, sizeof (struct dqblk));
	if (error)
		return (error);
	error = getdiskquota(uid, mp, 0, &xdqp);
	if (error)
		return (error);
	dqp = xdqp;
	/*
	 * Don't change disk usage on Q_SETQLIM
	 */
	if (cmd == Q_SETQLIM) {
		newlim.dqb_curblocks = dqp->dq_curblocks;
		newlim.dqb_curfiles = dqp->dq_curfiles;
	}
	dqp->dq_dqb = newlim;
	if (uid == 0) {
		/*
		 * Timelimits for the super user set the relative time
		 * the other users can be over quota for this file system.
		 * If it is zero a default is used (see quota.h).
		 */
		mp->m_btimelimit =
		    newlim.dqb_btimelimit? newlim.dqb_btimelimit: DQ_BTIMELIMIT;
		mp->m_ftimelimit =
		    newlim.dqb_ftimelimit? newlim.dqb_ftimelimit: DQ_FTIMELIMIT;
	} else {
		/*
		 * If the user is now over quota, start the timelimit.
		 * The user will not be warned.
		 */
		if (dqp->dq_curblocks >= dqp->dq_bsoftlimit &&
		    dqp->dq_bsoftlimit && dqp->dq_btimelimit == 0)
			dqp->dq_btimelimit = time.tv_sec + mp->m_btimelimit;
		else
			dqp->dq_btimelimit = 0;
		if (dqp->dq_curfiles >= dqp->dq_fsoftlimit &&
		    dqp->dq_fsoftlimit && dqp->dq_ftimelimit == 0)
			dqp->dq_ftimelimit = time.tv_sec + mp->m_ftimelimit;
		else
			dqp->dq_ftimelimit = 0;
		dqp->dq_flags &= ~(DQ_BLKS|DQ_FILES);
	}
	dqp->dq_flags |= DQ_MOD;
	dqupdate(dqp);
	dqput(dqp);
	return (0);
}

/*
 * Q_GETQUOTA - return current values in a dqblk structure.
 */
int
getquota(uid, mp, addr)
	short uid;
	struct mount *mp;
	caddr_t addr;
{
	register struct dquot *dqp;
	struct dquot *xdqp;
	int error;

	if (uid != u.u_ruid && !suser())
		return (EPERM);
	if ((mp->m_qflags & MQ_ENABLED) == 0)
		return (ESRCH);
	error = getdiskquota(uid, mp, 0, &xdqp);
	if (error)
		return (error);
	dqp = xdqp;
	if (dqp->dq_fhardlimit == 0 && dqp->dq_fsoftlimit == 0 &&
	    dqp->dq_bhardlimit == 0 && dqp->dq_bsoftlimit == 0) {
		error = ESRCH;
	} else {
		error =
		    copyout((caddr_t)&dqp->dq_dqb, addr, sizeof (struct dqblk));
	}
	dqput(dqp);
	return (error);
}

/*
 * Q_SYNC - sync quota files to disk.
 */
int
qsync(mp)
	register struct mount *mp;
{
	register struct dquot *dqp;

	if (mp != NULL && (mp->m_qflags & MQ_ENABLED) == 0)
		return (ESRCH);
	for (dqp = dquot; dqp < dquotNDQUOT; dqp++) {
		if ((dqp->dq_flags & DQ_MOD) &&
		    (mp == NULL || dqp->dq_mp == mp) &&
		    (dqp->dq_mp->m_qflags & MQ_ENABLED) &&
		    (dqp->dq_flags & DQ_LOCKED) == 0) {
			dqp->dq_flags |= DQ_LOCKED;
			dqupdate(dqp);
			DQUNLOCK(dqp);
		}
	}
	return (0);
}

int
fdevtomp(fdev, mpp)
	char *fdev;
	struct mount **mpp;
{
	struct vnode *vp;
	dev_t dev;
	int error;

	error =
	    lookupname(fdev, UIO_USERSPACE, FOLLOW_LINK,
		(struct vnode **)0, &vp);
	if (error)
		return (error);
	if (vp->v_type != VBLK) {
		VN_RELE(vp);
		return (ENOTBLK);
	}
	dev = vp->v_rdev;
	VN_RELE(vp);
	*mpp = getmp(dev);
	if (*mpp == NULL)
		return (ENODEV);
	else
		return (0);
}
#endif	QUOTA

