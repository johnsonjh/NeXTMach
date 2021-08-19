/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 **********************************************************************
 * HISTORY
 * Revision 2.5  89/01/30  22:03:09  rpd
 * 	Added declarations of rablock, rasize.
 * 	The declarations in sys/systm.h are "extern" now.
 * 	[89/01/25  14:53:12  rpd]
 * 
 * 28-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes.
 *
 * 28-Aug-87  Peter King (king) at NeXT
 *	Sun VFS port
 *
 * 22-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	MACH_NBC:  Be sure to flush any pages potentially cached in the
 *	VM system when allocating new blocks to a file.  Actually,
 *	this is probably only necessary when a frag is extended, this
 *	can be verified later.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 **********************************************************************
 */

#import <mach_nbc.h>
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
/* static char sccsid[] = 	"@(#)ufs_bmap.c	2.2 88/06/09 4.0NFSSRC SMI"; */
	/* from UCB 7.1 6/5/86 */
	/* @(#) from SUN 2.33	*/
#endif

#import <sys/param.h>
#import <sys/systm.h>
#import <ufs/fsdir.h>
#import <sys/user.h>
#import <sys/vnode.h>
#import <sys/buf.h>
#import <sys/proc.h>
#import <ufs/inode.h>
#import <ufs/fs.h>

daddr_t	rablock;		/* block to be read ahead */
int	rasize;			/* size of block in rablock */

/*
 * Bmap defines the structure of file system storage
 * by returning the physical block number on a device given the
 * inode and the logical block number in a file.
 * When convenient, it also leaves the physical
 * block number of the next block of the file in rablock
 * for use in read-ahead.
 */
/*VARARGS3*/
daddr_t
bmap(ip, bn, rwflg, size, sync)
	register struct inode *ip;
	daddr_t bn;
	int rwflg;
	u_long size;	/* supplied only when rwflg == B_WRITE */
	int *sync;	/* supplied only when rwflg == B_WRITE */
{
	register int i;
	u_long osize, nsize;
	struct buf *bp, *nbp;
	struct fs *fs;
	int j, sh;
	daddr_t nb, lbn, *bap, pref, blkpref();
#if	MACH_NBC
	int	flush;
#endif	MACH_NBC

	if (bn < 0) {
		u.u_error = EFBIG;
		return ((daddr_t)0);
	}
	fs = ip->i_fs;
	rablock = 0;
	rasize = 0;		/* conservative */

#if	MACH_NBC
	flush = TRUE;
	if (rwflg & B_XXX) {	/* hack hack hack */
		flush = FALSE;
		rwflg &= ~B_XXX;
	}
#endif	MACH_NBC
	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */
	nb = lblkno(fs, ip->i_size);
	if (rwflg == B_WRITE && nb < NDADDR && nb < bn && ip->i_db[nb] != 0) {
		osize = blksize(fs, ip, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			bp = realloccg(ip, ip->i_db[nb],
				blkpref(ip, nb, (int)nb, &ip->i_db[0]),
				osize, (int)fs->fs_bsize);
			if (bp == NULL)
				return ((daddr_t)-1);
			ip->i_size = (nb + 1) * fs->fs_bsize;
			ip->i_db[nb] = dbtofsb(fs, bp->b_blkno);
			ip->i_flag |= IUPD|ICHG;
			/*
			 * if syncronous operation is specified, then
			 * write out the new block synchronously, then
			 * update the inode to make sure it points to it
			 */
			if (sync) {
				bwrite(bp);
				iupdat(ip, 1);
			} else {
				bdwrite(bp);
			}
#if	MACH_NBC && !NeXT
			if (flush)
				vno_flush(ITOV(ip), nb * fs->fs_bsize,
					fs->fs_bsize);
#endif	MACH_NBC
		}
	}
	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (bn < NDADDR) {
		nb = ip->i_db[bn];
		if (rwflg == B_READ) {
			if (nb == 0)
				return ((daddr_t)-1);
			goto gotit;
		}
		if (nb == 0 || ip->i_size < (bn + 1) * fs->fs_bsize) {
			if (nb != 0) {
				/* consider need to reallocate a frag */
				osize = fragroundup(fs, blkoff(fs, ip->i_size));
				nsize = fragroundup(fs, size);
				if (nsize <= osize)
					goto gotit;
				bp = realloccg(ip, nb,
					blkpref(ip, bn, (int)bn, &ip->i_db[0]),
					osize, nsize);
			} else {
				if (ip->i_size < (bn + 1) * fs->fs_bsize)
					nsize = fragroundup(fs, size);
				else
					nsize = fs->fs_bsize;
				bp = alloc(ip,
					blkpref(ip, bn, (int)bn, &ip->i_db[0]),
					nsize);
			}
			if (bp == NULL)
				return ((daddr_t)-1);
			nb = dbtofsb(fs, bp->b_blkno);
			if (sync) {
				*sync = 1;
			}
			if ((ip->i_mode&IFMT) == IFDIR)
				/*
				 * Write directory blocks synchronously
				 * so they never appear with garbage in
				 * them on the disk.
				 */
				bwrite(bp);
			else
				bdwrite(bp);
#if	MACH_NBC && !NeXT
			if (flush)
				vno_flush(ITOV(ip), bn*fs->fs_bsize, fs->fs_bsize);
#endif	MACH_NBC
			ip->i_db[bn] = nb;
			ip->i_flag |= IUPD|ICHG;
		}
gotit:
		if (bn < NDADDR - 1) {
			rablock = fsbtodb(fs, ip->i_db[bn + 1]);
			rasize = blksize(fs, ip, bn + 1);
		}
		return (nb);
	}

	/*
	 * Determine how many levels of indirection.
	 */
	pref = 0;
	sh = 1;
	lbn = bn;
	bn -= NDADDR;
	for (j = NIADDR; j>0; j--) {
		sh *= NINDIR(fs);
		if (bn < sh)
			break;
		bn -= sh;
	}
	if (j == 0) {
		u.u_error = EFBIG;
		return ((daddr_t)0);
	}

	/*
	 * fetch the first indirect block
	 */
	nb = ip->i_ib[NIADDR - j];
	if (nb == 0) {
		if (rwflg == B_READ)
			return ((daddr_t)-1);
		pref = blkpref(ip, lbn, 0, (daddr_t *)0);
	        bp = alloc(ip, pref, (int)fs->fs_bsize);
		if (bp == NULL)
			return ((daddr_t)-1);
		nb = dbtofsb(fs, bp->b_blkno);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		bwrite(bp);
#if	MACH_NBC
		/* no need to VM flush indirect blocks */
#endif	MACH_NBC
		ip->i_ib[NIADDR - j] = nb;
		ip->i_flag |= IUPD|ICHG;
		if (sync) {
			*sync = 1;
		}
	}

	/*
	 * fetch through the indirect blocks
	 */
	for (; j <= NIADDR; j++) {
		bp = bread(ip->i_devvp, fsbtodb(fs, nb), (int)fs->fs_bsize);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			return ((daddr_t)0);
		}
		bap = bp->b_un.b_daddr;
		sh /= NINDIR(fs);
		i = (bn / sh) % NINDIR(fs);
		nb = bap[i];
		if (nb == 0) {
			if (rwflg==B_READ) {
				brelse(bp);
				return ((daddr_t)-1);
			}
			if (pref == 0)
				if (j < NIADDR)
					pref = blkpref(ip, lbn, 0,
						(daddr_t *)0);
				else
					pref = blkpref(ip, lbn, i, &bap[0]);
		        nbp = alloc(ip, pref, (int)fs->fs_bsize);
			if (nbp == NULL) {
				brelse(bp);
				return ((daddr_t)-1);
			}
			nb = dbtofsb(fs, nbp->b_blkno);
			if (j < NIADDR || (ip->i_mode&IFMT) == IFDIR || sync)
				/*
				 * Write synchronously so indirect blocks
				 * never point at garbage and blocks
				 * in directories never contain garbage.
				 */
				bwrite(nbp);
			else
				bdwrite(nbp);
			bap[i] = nb;
			if (sync)
				bwrite(bp);
			else
				bdwrite(bp);
				
#if	MACH_NBC && !NeXT
			if (flush)
				vno_flush(ITOV(ip), lbn*fs->fs_bsize, fs->fs_bsize);
#endif	MACH_NBC
		} else
			brelse(bp);
	}

	/*
	 * calculate read-ahead.
	 */
	if (i < NINDIR(fs) - 1) {
		rablock = fsbtodb(fs, bap[i+1]);
		rasize = fs->fs_bsize;
	}
	return (nb);
}


