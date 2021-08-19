/*	@(#)dkbad.c	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 **********************************************************************
 * HISTORY
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 **********************************************************************
 */ 

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)dkbad.c	7.1 (Berkeley) 6/5/86
 */

#ifndef NOBADSECT
#import <sys/param.h>
#import <sys/buf.h>
#import <sys/dkbad.h>

/*
 * Search the bad sector table looking for
 * the specified sector.  Return index if found.
 * Return -1 if not found.
 */

isbad(bt, cyl, trk, sec)
	register struct dkbad *bt;
{
	register int i;
	register long blk, bblk;

	blk = ((long)cyl << 16) + (trk << 8) + sec;
	for (i = 0; i < 126; i++) {
		bblk = ((long)bt->bt_bad[i].bt_cyl << 16) + bt->bt_bad[i].bt_trksec;
		if (blk == bblk)
			return (i);
		if (blk < bblk || bblk < 0)
			break;
	}
	return (-1);
}
#endif
