/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 *  3-Jan-90  Gregg Kellogg at NeXT
 *	Removed calloc for NeXT.
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Removed dir.h
 *
 * 12-Jan-88  Mike DeMoney at NeXT
 *	NeXT: Commented out ffs, bcmp, and strlen, now in assembler.
 *
 * 30-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 * 07-Oct-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Change to panic on strlen(NULL) until this is determined to be
 *	a feature instead of a bug.
 *	[ V5.1(XF18) ]
 *
 * 20-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Changed inodes to vnodes.  Added nullsys() and errsys().
 *
 * 05-May-87  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Fixed strlen() so it won't memory fault if it is passed
 *	a null pointer.
 *
 * 18-Mar-87  John Seamons (jks) at NeXT
 *	Added NeXT to list of machines that need definition of imin(),
 *	min(), etc..
 *
 * 08-Jan-87  Robert Beck (beck) at Sequent Computer Systems, Inc.
 *	Include cputypes.h.
 *	Don't compile calloc() if BALANCE -- this is used differently.
 *	Don't compile ffs(), bcmp(), and strlen() for BALANCE.
 *
 * 23-Oct-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Added Sun to list of machines that need definition of imin(),
 *	min(), etc..
 *
 *  7-Jul-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	MACH_VM: (negatively) Conditionalized include of pte.h
 *
 * 25-Feb-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	New calloc routine using VM routines.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 16-Nov-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	CS_BUGFIX:  Fixed off by one error in ffs.
 */

#import <cputypes.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)subr_xxx.c	7.1 (Berkeley) 6/5/86
 */

/* @(#)subr_xxx.c	1.2 87/05/01 3.2/4.3NFSSRC */

#import <vm/vm_map.h>
#import <vm/vm_kern.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/conf.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/proc.h>
#import <sys/vnode.h>
#import <sys/uio.h>

/*
 * Routine placed in illegal entries in the bdevsw and cdevsw tables.
 */
nodev()
{

	return (ENODEV);
}

/*
 * Null routine; placed in insignificant entries
 * in the bdevsw and cdevsw tables.
 */
nulldev()
{

	return (0);
}

/*
 * Null system calls. Not invalid, just not configured.
 */
errsys()
{
	u.u_error = EINVAL;
}

nullsys()
{
}

#if defined(romp) || defined(ns32000) || defined(sun) || NeXT
#define	notdef
#endif	romp || ns32000 || sun || NeXT
#ifdef notdef
imin(a, b)
{

	return (a < b ? a : b);
}

imax(a, b)
{

	return (a > b ? a : b);
}

unsigned
min(a, b)
	u_int a, b;
{

	return (a < b ? a : b);
}

unsigned
max(a, b)
	u_int a, b;
{

	return (a > b ? a : b);
}
#endif notdef
#if defined(romp) || defined(ns32000) || defined(sun) || NeXT
#undef	notdef
#endif	romp || ns32000 || sun || NeXT

#ifndef NeXT
caddr_t calloc(size)
	int size;
{
	return((caddr_t)kmem_alloc(kernel_map, (vm_offset_t)size));
}
#endif	NeXT

#ifdef GPROF
/*
 * Stub routine in case it is ever possible to free space.
 */
cfreemem(cp, size)
	caddr_t cp;
	int size;
{
	printf("freeing %x, size %d\n", cp, size);
}
#endif

#if	BALANCE || NeXT || vax
#else	BALANCE || NeXT || vax
ffs(mask)
	register long mask;
{
	register int i;

	for(i = 1; i <= NSIG; i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}
	return (0);
}

bcmp(s1, s2, len)
	register char *s1, *s2;
	register int len;
{

	while (len--)
		if (*s1++ != *s2++)
			return (1);
	return (0);
}

strlen(s1)
	register char *s1;
{
	register int len;

	if (s1 == 0)
		panic("strlen(NULL)");
	for (len = 0; *s1++ != '\0'; len++)
		/* void */;
	return (len);
}
#endif	BALANCE || NeXT || vax


