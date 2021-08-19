/*
 * @(#)spec_clone.c	2.2 88/05/24 4.0NFSSRC Copyr 1988 Sun Micro
 */

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.5
 */

/*
 * HISTORY
 *  4-Jan-88  Peter King (king) at NeXT
 *	Original Sun source, ported to Mach.
 */

/*
 * Clone device driver.  Forces a clone open of some other
 * character device.  Since its purpose in life is to force
 * some other device to clone itself, there's no need for
 * anything other than the open routine here.
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>

/*
 * Do a clone open.  The (major number of the) device to be cloned
 * is specified by minor(dev).  We tell spec_open to do the work
 * by returning EEXIST after naming the device to clone.
 */
/* ARGSUSED */
cloneopen(dev, flag, newdevp)
	dev_t	dev;
	int	flag;
	dev_t	*newdevp;
{
	/* Convert to the device to be cloned. */
	*newdevp = makedev(minor(dev), 0);

	return (EEXIST);
}
