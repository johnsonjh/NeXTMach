/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */ 

/*
 * HISTORY
 * 26-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: Removed dir.h.  Cleaned up casting of uid_t and
 *			 gid_t.  Changed crfree to run at splhigh.
 *
 * 19-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Changed inodes to vnodes.  Add support for credentials
 *		 record - crget(), crfree(), crcopy(), and crdup().
 *
 * 25-Jun-87  William Bolosky (bolosky) at Carnegie-Mellon University
 *	Made QUOTA a #if-type option.
 *
 */
#import <quota.h>
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_prot.c	7.1 (Berkeley) 6/5/86
 */

/*
 * System calls related to processes and protection
 */

#import <machine/reg.h>
#import <machine/spl.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <sys/proc.h>
#import <sys/timeb.h>
#import <sys/times.h>
#import <sys/reboot.h>
#import <sys/buf.h>
#import <sys/acct.h>

#import <kern/kalloc.h>


void crfree();
getpid()
{
	register struct uthread *uth = current_thread()->u_address.uthread;
	register struct proc *p = u.u_procp;

	uth->uu_r.r_val1 = p->p_pid;
	uth->uu_r.r_val2 = p->p_ppid;
}

getpgrp()
{
	register struct a {
		int	pid;
	} *uap = (struct a *)u.u_ap;
	register struct proc *p;

	if (uap->pid == 0)
		uap->pid = u.u_procp->p_pid;
	p = pfind(uap->pid);
	if (p == 0) {
		u.u_error = ESRCH;
		return;
	}
	u.u_r.r_val1 = p->p_pgrp;
}

getuid()
{

	u.u_r.r_val1 = (int) u.u_ruid;
	u.u_r.r_val2 = (int) u.u_uid;
}

getgid()
{

	u.u_r.r_val1 = (int) u.u_rgid;
	u.u_r.r_val2 = (int) u.u_gid;
}

getgroups()
{
	register struct	a {
		u_int	gidsetsize;
		int	*gidset;
	} *uap = (struct a *)u.u_ap;
	register gid_t *gp;
	register int *lp;
	int groups[NGROUPS];

	for (gp = &u.u_groups[NGROUPS]; gp > u.u_groups; gp--)
		if (gp[-1] != NOGROUP)
			break;
	if (uap->gidsetsize < gp - u.u_groups) {
		u.u_error = EINVAL;
		return;
	}
	uap->gidsetsize = gp - u.u_groups;
	for (lp = groups, gp = u.u_groups; lp < &groups[uap->gidsetsize]; )
		*lp++ = (int) *gp++;
	u.u_error = copyout((caddr_t)groups, (caddr_t)uap->gidset,
	    uap->gidsetsize * sizeof (groups[0]));
	if (u.u_error)
		return;
	u.u_r.r_val1 = uap->gidsetsize;
}

setpgrp()
{
	register struct proc *p;
	register struct a {
		int	pid;
		int	pgrp;
	} *uap = (struct a *)u.u_ap;

	if (uap->pid == 0)
		uap->pid = u.u_procp->p_pid;
	p = pfind(uap->pid);
	if (p == 0) {
		u.u_error = ESRCH;
		return;
	}
/* need better control mechanisms for process groups */
	if (p->p_uid != u.u_uid && u.u_uid && !inferior(p)) {
		u.u_error = EPERM;
		return;
	}
	p->p_pgrp = uap->pgrp;
}

setreuid()
{
	struct a {
		int	ruid;
		int	euid;
	} *uap;
	register uid_t ruid,euid;

	uap = (struct a *)u.u_ap;
	if (uap->ruid == -1)
		ruid = u.u_ruid;
	else
		ruid = (uid_t)uap->ruid;
	if (u.u_ruid != ruid && u.u_uid != ruid && !suser())
		return;
	if (uap->euid == -1)
		euid = u.u_uid;
	else
		euid = (uid_t)uap->euid;
	if (u.u_ruid != euid && u.u_uid != euid && !suser())
		return;
	/*
	 * Everything's okay, do it.
	 */
#if	NeXT
	u_cred_lock();
#endif	NeXT
	u.u_cred = crcopy(u.u_cred);
	u.u_procp->p_uid = euid;
	u.u_ruid = ruid;
	u.u_uid = euid;
#if	NeXT
	u_cred_unlock();
#endif	NeXT
}

setregid()
{
	register struct a {
		int	rgid;
		int	egid;
	} *uap;
	register gid_t rgid, egid;

	uap = (struct a *)u.u_ap;
	if (uap->rgid == -1)
		rgid = u.u_rgid;
	else
		rgid = (gid_t)uap->rgid;
	if (u.u_rgid != rgid && u.u_gid != rgid && !suser())
		return;
	if (uap->egid == -1)
		egid = u.u_gid;
	else
		egid = (gid_t)uap->egid;
	if (u.u_rgid != egid && u.u_gid != egid && !suser())
		return;
#if	NeXT
	u_cred_lock();
#endif	NeXT
	u.u_cred = crcopy(u.u_cred);
	if (u.u_rgid != rgid) {
		leavegroup(u.u_rgid);
		(void) entergroup(rgid);
		u.u_rgid = rgid;
	}
	u.u_gid = egid;
#if	NeXT
	u_cred_unlock();
#endif	NeXT
}

setgroups()
{
	register struct	a {
		u_int	gidsetsize;
		int	*gidset;
	} *uap = (struct a *)u.u_ap;
	register gid_t *gp;
	register int *lp;
	int groups[NGROUPS];
	struct ucred *newcr, *tmpcr;

	if (!suser())
		return;
	if (uap->gidsetsize > sizeof (u.u_groups) / sizeof (u.u_groups[0])) {
		u.u_error = EINVAL;
		return;
	}
	newcr = crdup(u.u_cred);
	/*
	 *	Can't copy gid array directly, we still have to cast
	 *	them to a gid_t !
	 */
	u.u_error = copyin((caddr_t)uap->gidset, (caddr_t)groups,
	    uap->gidsetsize * sizeof (uap->gidset[0]));
	if (u.u_error) {
		crfree(newcr);
		return;
	}
	for(lp = groups,gp = newcr->cr_groups;
		lp < &groups[uap->gidsetsize];lp++,gp++) 
		*gp = (gid_t) *lp;
	tmpcr = u.u_cred;
	u.u_cred = newcr;
	crfree(tmpcr);
	for (gp = &u.u_groups[uap->gidsetsize]; gp < &u.u_groups[NGROUPS]; gp++)
		*gp = NOGROUP;
}

/*
 * Group utility functions.
 */

/*
 * Delete gid from the group set and compress.
 */
leavegroup(gid)
	gid_t gid;
{
	register gid_t *gp;

	for (gp = u.u_groups; gp < &u.u_groups[NGROUPS]; gp++)
		if (*gp == gid)
			goto found;
	return;
found:
	for (; gp < &u.u_groups[NGROUPS-1]; gp++)
		*gp = *(gp+1);
	*gp = NOGROUP;
}

/*
 * Add gid to the group set.
 */
entergroup(gid)
	gid_t gid;
{
	register gid_t *gp;

	for (gp = u.u_groups; gp < &u.u_groups[NGROUPS]; gp++) {
		if (*gp == gid)
			return (0);
		if (*gp == NOGROUP) {
			*gp = gid;
			return (0);
		}
	}
	return (-1);
}

/*
 * Check if gid is a member of the group set.
 */
groupmember(gid)
	gid_t gid;
{
	register gid_t *gp;

	if (u.u_gid == gid)
		return (1);
	for (gp = u.u_groups; gp < &u.u_groups[NGROUPS] && *gp != NOGROUP; gp++)
		if (*gp == gid)
			return (1);
	return (0);
}

/*
 * Test if the current user is the super user.
 */
suser()
{
	if (u.u_uid == 0) {
		u.u_acflag |= ASU;
		return (1);
	}
	u.u_error = EPERM;
	return (0);
}

/*
 * Routines to allocate and free credentials structures
 */

int cractive = 0;

struct ucred *rootcred;

/*
 * Allocate a zeroed cred structure and crhold it.
 */
struct ucred *
crget()
{
	register struct ucred *cr;

	cr = (struct ucred *) kalloc(sizeof(*cr));
	bzero((caddr_t)cr, sizeof(*cr));
	crhold(cr);
	cractive++;
	return(cr);
}

/*
 * Free a cred structure.
 * Throws away space when ref count gets to 0.
 */
void
crfree(cr)
	struct ucred *cr;
{
	int	s = splhigh();

	if (--cr->cr_ref != 0) {
		(void) splx(s);
		return;
	}
	kfree(cr, sizeof(*cr));
	cractive--;
	(void) splx(s);
}

/*
 * Copy cred structure to a new one and free the old one.
 */
struct ucred *
crcopy(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	newcr = crget();
	*newcr = *cr;
	crfree(cr);
	newcr->cr_ref = 1;
	return(newcr);
}

/*
 * Dup cred struct to a new held one.
 */
struct ucred *
crdup(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	newcr = crget();
	*newcr = *cr;
	newcr->cr_ref = 1;
 	return(newcr);
}

