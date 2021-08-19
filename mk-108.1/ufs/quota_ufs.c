/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 **********************************************************************
 * HISTORY
 * 25-Jan-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes.
 **********************************************************************
 */
#import <quota.h>

#if	QUOTA
#ifndef lint
/* static char sccsid[] = 	"@(#)quota_ufs.c	2.1 88/05/20 4.0NFSSRC Copyr 1988 Sun Micro"; */
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 2.12
 */

/*
 * SUN QUOTAS
 *
 * Routines used in checking limits on file system usage.
 */
#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/kernel.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/buf.h>
#import <sys/uio.h>
#import <ufs/quotas.h>
#import <ufs/inode.h>
#import <ufs/mount.h>
#import <ufs/fs.h>

/*
 * Find the dquot structure that should
 * be used in checking i/o on inode ip.
 */
struct dquot *
getinoquota(ip)
	register struct inode *ip;
{
	register struct dquot *dqp;
	register struct mount *mp;
	struct dquot *xdqp;

	mp = VFSTOM(ip->i_vnode.v_vfsp);
	/*
	 * Check for quotas enabled.
	 */
	if ((mp->m_qflags & MQ_ENABLED) == 0)
		return (NULL);
	/*
	 * Check for someone doing I/O to quota file.
	 */
	if (ip == mp->m_qinod)
		return (NULL);
	if (getdiskquota(ip->i_uid, mp, 0, &xdqp))
		return (NULL);
	dqp = xdqp;
	if (dqp->dq_fhardlimit == 0 && dqp->dq_fsoftlimit == 0 &&
	    dqp->dq_bhardlimit == 0 && dqp->dq_bsoftlimit == 0) {
		dqput(dqp);
		dqp = NULL;
	} else {
		DQUNLOCK(dqp);
	}
	return (dqp);
}

/*
 * Update disk usage, and take corrective action.
 */
int
chkdq(ip, change, force)
	struct inode *ip;
	long change;
	int force;
{
	register struct dquot *dqp;
	register u_long ncurblocks;
	int error = 0;

	if (change == 0)
		return (0);
	dqp = ip->i_dquot;
	if (dqp == NULL)
		return (0);
	dqp->dq_flags |= DQ_MOD;
	if (change < 0) {
		if ((int)dqp->dq_curblocks + change >= 0)
			dqp->dq_curblocks += change;
		else
			dqp->dq_curblocks = 0;
		if (dqp->dq_curblocks < dqp->dq_bsoftlimit)
			dqp->dq_btimelimit = 0;
		dqp->dq_flags &= ~DQ_BLKS;
		return (0);
	}

	ncurblocks = dqp->dq_curblocks + change;
	/*
	 * Allocation. Check hard and soft limits.
	 * Skip checks for super user.
	 */
	if (u.u_uid == 0)
		goto out;
	/*
	 * Dissallow allocation if it would bring the current usage over
	 * the hard limit or if the user is over his soft limit and his time
	 * has run out.
	 */
	if (ncurblocks >= dqp->dq_bhardlimit && dqp->dq_bhardlimit && !force) {
		if ((dqp->dq_flags & DQ_BLKS) == 0 && ip->i_uid == u.u_ruid) {
			uprintf("\nDISK LIMIT REACHED (%s) - WRITE FAILED\n",
			   ip->i_fs->fs_fsmnt);
			dqp->dq_flags |= DQ_BLKS;
		}
		error = EDQUOT;
	}
	if (ncurblocks >= dqp->dq_bsoftlimit && dqp->dq_bsoftlimit) {
		if (dqp->dq_curblocks < dqp->dq_bsoftlimit ||
		    dqp->dq_btimelimit == 0) {
			dqp->dq_btimelimit =
			    time.tv_sec +
			    VFSTOM(ip->i_vnode.v_vfsp)->m_btimelimit;
			if (ip->i_uid == u.u_ruid)
				uprintf("\nWARNING: disk quota (%s) exceeded\n",
				   ip->i_fs->fs_fsmnt);
		} else if (time.tv_sec > dqp->dq_btimelimit && !force) {
			if ((dqp->dq_flags & DQ_BLKS) == 0 &&
			    ip->i_uid == u.u_ruid) {
				uprintf(
				"\nOVER DISK QUOTA: (%s) NO MORE DISK SPACE\n",
				ip->i_fs->fs_fsmnt);
				dqp->dq_flags |= DQ_BLKS;
			}
			error = EDQUOT;
		}
	}
out:
	if (error == 0)
		dqp->dq_curblocks = ncurblocks;
	return (error);
}

/*
 * Check the inode limit, applying corrective action.
 */
int
chkiq(mp, ip, uid, force)
	struct mount *mp;
	struct inode *ip;
	int uid;
	int force;
{
	register struct dquot *dqp;
	register u_long ncurfiles;
	struct dquot *xdqp;
	int error = 0;

	/*
	 * Free.
	 */
	if (ip != NULL) {
		dqp = ip->i_dquot;
		if (dqp == NULL)
			return (0);
		dqp->dq_flags |= DQ_MOD;
		if (dqp->dq_curfiles)
			dqp->dq_curfiles--;
		if (dqp->dq_curfiles < dqp->dq_fsoftlimit)
			dqp->dq_ftimelimit = 0;
		dqp->dq_flags &= ~DQ_FILES;
		return (0);
	}

	/*
	 * Allocation. Get dquot for for uid, fs.
	 * Check for quotas enabled.
	 */
	if ((mp->m_qflags & MQ_ENABLED) == 0)
		return (0);
	if (getdiskquota(uid, mp, 0, &xdqp))
		return (0);
	dqp = xdqp;
	if (dqp->dq_fsoftlimit == 0 && dqp->dq_fhardlimit == 0) {
		dqput(dqp);
		return (0);
	}
	dqp->dq_flags |= DQ_MOD;
	/*
	 * Skip checks for super user.
	 */
	if (u.u_uid == 0)
		goto out;
	ncurfiles = dqp->dq_curfiles + 1;
	/*
	 * Dissallow allocation if it would bring the current usage over
	 * the hard limit or if the user is over his soft limit and his time
	 * has run out.
	 */
	if (ncurfiles >= dqp->dq_fhardlimit && dqp->dq_fhardlimit && !force) {
		if ((dqp->dq_flags & DQ_FILES) == 0 && uid == u.u_ruid) {
			uprintf("\nFILE LIMIT REACHED - CREATE FAILED (%s)\n",
			    mp->m_bufp->b_un.b_fs->fs_fsmnt);
			dqp->dq_flags |= DQ_FILES;
		}
		error = EDQUOT;
	} else if (ncurfiles >= dqp->dq_fsoftlimit && dqp->dq_fsoftlimit) {
		if (ncurfiles == dqp->dq_fsoftlimit || dqp->dq_ftimelimit==0) {
			dqp->dq_ftimelimit = time.tv_sec + mp->m_ftimelimit;
			if (uid == u.u_ruid)
				uprintf("\nWARNING - too many files (%s)\n",
				    mp->m_bufp->b_un.b_fs->fs_fsmnt);
		} else if (time.tv_sec > dqp->dq_ftimelimit && !force) {
			if ((dqp->dq_flags&DQ_FILES) == 0 && uid == u.u_ruid) {
				uprintf(
				    "\nOVER FILE QUOTA - NO MORE FILES (%s)\n",
				    mp->m_bufp->b_un.b_fs->fs_fsmnt);
				dqp->dq_flags |= DQ_FILES;
			}
			error = EDQUOT;
		}
	}
out:
	if (error == 0)
		dqp->dq_curfiles++;
	dqput(dqp);
	return (error);
}

/*
 * Release a dquot.
 */
void
dqrele(dqp)
	register struct dquot *dqp;
{

	if (dqp != NULL) {
		DQLOCK(dqp);
		if (dqp->dq_cnt == 1 && dqp->dq_flags & DQ_MOD)
			dqupdate(dqp);
		dqput(dqp);
	}
}
#endif	QUOTA
