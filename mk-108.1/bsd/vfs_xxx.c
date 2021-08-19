/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 **********************************************************************
 * HISTORY
 * 23-Jan-89  Peter King (king) at NeXT
 *	Original Sun source (NFS 4.0).  Ported to Mach.  Removed
 *	backwards compatability stuff.
 *
 **********************************************************************
 */

/*	@(#)vfs_xxx.c	2.1 88/05/18 4.0NFSSRC SMI;  from SMI 2.9 86/11/11	*/

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/vnode.h>
#import <sys/file.h>
#import <sys/vfs.h>


/*
 * System V Interface Definition-compatible "ustat" call.
 */
struct ustat {
	daddr_t	f_tfree;	/* Total free blocks */
	ino_t	f_tinode;	/* Number of free inodes */
	char	f_fname[6];	/* null */
	char	f_fpack[6];	/* null */
};

ustat()
{
	register struct a {
		int	dev;
		struct ustat *buf;
	} *uap = (struct a *)u.u_ap;
	struct vfs *vfs;
	struct statfs sb;
	struct ustat usb;

	/*
	 * The most likely source of the "dev" here is a "stat" structure.
	 * It's a "short" there.  Unfortunately, it's a "long" before
	 * it gets there, and has its sign bit set when it's an NFS
	 * file system.  Turning it into a "short" and back to a "int"/"long"
	 * smears the sign bit through the upper 16 bits; we have to get
	 * rid of the sign bit.  Yuk.
	 */
	u.u_error = vafsidtovfs((long)(uap->dev & 0xffff), &vfs);
	if (u.u_error)
		return;
	u.u_error = VFS_STATFS(vfs, &sb);
	if (u.u_error)
		return;
	bzero((caddr_t)&usb, sizeof(usb));
	/*
	 * We define a "block" as being the unit defined by DEV_BSIZE.
	 * System V doesn't define it at all, except operationally, and
	 * even there it's self-contradictory; sometimes it's a hardcoded
	 * 512 bytes, sometimes it's whatever the "physical block size"
	 * of your filesystem is.
	 */
	usb.f_tfree = howmany(sb.f_bavail * sb.f_bsize, DEV_BSIZE);
	usb.f_tinode = sb.f_ffree;
	u.u_error = copyout((caddr_t)&usb, (caddr_t)uap->buf, sizeof(usb));
}
