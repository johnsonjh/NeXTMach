/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 07-Apr-90	Doug Mitchell at NeXT
 *	Added fd driver support.
 *
 * 16-Feb-88  John Seamons (jks) at NeXT
 *	Updated to Mach release 2.
 *
 * 29-Oct-87  Robert V. Baron (rvb) at Carnegie-Mellon University
 *	Allow root to be an arbitrary slice on the drive.
 *
 * 14-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)swapgeneric.c	7.1 (Berkeley) 6/6/86
 */

#import <sys/param.h>
#import <sys/conf.h>
#import <sys/buf.h>
#import <sys/time.h>
#import <sys/vm.h>
#import <sys/systm.h>
#import <sys/mount.h>
#import <sys/reboot.h>
#import <sys/vfs.h>
#import <sys/vnode.h>
#import <nextdev/busvar.h>
#import <sys/socket.h>
#import <sys/errno.h>
#import <sys/ioctl.h>
#import <sys/bootconf.h>
#import <rpc/types.h>
#import <net/if.h>
#import <netinet/in_systm.h>
#import <netinet/in.h>
#import <netinet/in_var.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/if_ether.h>


extern char hostname[], boot_info[], boot_dev[];
extern char domainname[];
extern struct xdr_ops xdrmbuf_ops;
extern int hostnamelen;
char rootdevice[8];

/*
 * Generic configuration;  all in one
 */
long	dumplo;
int	dmmin, dmmax, dmtext;

#import <en.h>
#if	NEN > 0
extern struct bus_driver endriver;
#endif	NEN

#import <rd.h>
#if	NRD > 0
extern struct bus_driver rddriver;
#endif	NRD

#import <sd.h>
#if	NSD > 0
extern struct bus_driver scdriver;
#endif	NSD

#import <od.h>
#if	NOD > 0
extern struct bus_driver ocdriver;
#endif	NOD

#import <fd.h>
#if	NFD > 0
extern struct bus_driver fcdriver;
#endif	NFD

struct	genericconf {
	struct bus_driver *gc_driver;
	char	*gc_name;
	dev_t	gc_root;	/* NODEV means remote root */
} genericconf[] = {
#if	NSD > 0
	{ &scdriver, "sd", makedev(6, 0) },
#endif	NSD
#if	NRD > 0
	{ &rddriver, "rd", makedev(5, 0) },
#endif	NRD
#if	NOD > 0
	{ &ocdriver, "od", makedev(2, 0) },
#endif	NOD
#if	NFD > 0
	{ &fcdriver, "fd", makedev(1, 0) },
#endif	NFD
#if	NEN > 0
 	{ &endriver, "en", NODEV },
#endif	NEN
	{ 0 },
};

static struct genericconf *gc;

setconf()
{
	register struct bus_device *bd;
	int unit, swaponroot = 0;
	int remoteroot = 0;
	int slice = 0;
	char *name, root_name[128];
	extern int nfs_mountroot();
 
	if ((boothowto & RB_ASKNAME) || rootdevice[0]) {
retry:
		if (boothowto & RB_ASKNAME) {
			printf("root device? ");
			name = root_name;
			gets(name, name);
		} else {
			name = rootdevice;
			printf ("root on %s\n", name);
		}
		for (gc = genericconf; gc->gc_driver; gc++)
			if (gc->gc_name[0] == name[0] &&
			    gc->gc_name[1] == name[1])
				goto gotit;
		goto bad;
gotit:
		if (gc->gc_root == NODEV) {
			goto found;
		}
		if (name[3] == '*') {
			name[3] = name[4];
			swaponroot++;
		}
		if (name[2] >= '0' && name[2] <= '7') {
			if (name[3] >= 'a' && name[3] <= 'h') {
				slice = name[3] - 'a';
			} else if (name[3]) {
				printf("bad partition number\n");
				goto bad;
			}
			unit = name[2] - '0';
			goto found;
		}
		printf("bad/missing unit number\n");
bad:
		for (gc = genericconf; gc->gc_driver; gc++)
			printf("%s%s%%d",
			       (gc == genericconf)?"use ":
				    (((gc+1)->gc_driver)?", ":" or "),
			       gc->gc_name);
		printf("\n");
		boothowto |= RB_ASKNAME;
		goto retry;
	}
	unit = 0;

	if (boot_dev[0]){
		if (boot_info[0])
			unit = boot_info[1] - '0';
		for (gc = genericconf; gc->gc_driver; gc++) {
			if (strcmp (boot_dev, gc->gc_name))
				continue;
			for (bd = bus_dinit; bd->bd_driver; bd++) {
				if (bd->bd_alive == 0)
					continue;
				if (bd->bd_unit == unit &&
				    bd->bd_driver == gc->gc_driver &&
				    strcmp(bd->bd_name, gc->gc_name)==0) {
					printf("root on %s%d\n",
						bd->bd_name, unit);
					goto found;
				}
			}
		}
		printf ("root device %s%d not configured\n", boot_dev, unit);
	}

	for (gc = genericconf; gc->gc_driver; gc++) {
		for (bd = bus_dinit; bd->bd_driver; bd++) {
			if (bd->bd_alive == 0)
				continue;
			if (bd->bd_unit == 0 &&
			    bd->bd_driver == gc->gc_driver &&
			    strcmp(bd->bd_name, gc->gc_name)==0) {
				printf("root on %s0\n", bd->bd_name);
				goto found;
			}
		}
	}
	printf("no suitable root\n");
	mon_boot ("-h");
found:
	if (gc->gc_root == NODEV) {
		strcpy(rootfs.bo_fstype,"nfs");
	} else {
		strcpy(rootfs.bo_fstype,"4.3");
		gc->gc_root = makedev(major(gc->gc_root), unit*8+slice);
		rootdev = gc->gc_root;
	}
}

gets(cp, lp)
	char *cp, *lp;
{
	register c;

	for (;;) {
		c = cngetc() & 0177;
		switch (c) {
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\177':
			if (lp == cp) {
				cnputc('\b');
				continue;
			}
			cnputc('\b');
			cnputc('\b');
			/* fall into ... */
		case '\b':
			if (lp == cp) {
				cnputc('\b');
				continue;
			}
			cnputc(' ');
			cnputc('\b');
			lp--;
			continue;
		case '@':
		case 'u'&037:
			lp = cp;
			cnputc('\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}

/*
 * If booted with ASKNAME, prompt on the console for a filesystem
 * name and return it.
 */
getfsname(askfor, name)
	char *askfor;
	char *name;
{

	if (boothowto & RB_ASKNAME) {
		printf("%s key [%s]: ", askfor, askfor);
		gets(name, name);
	}
}

/*
 * Routine: initrootnet
 * Function:
 *	This routine starts up the primary network interface and autoaddr's
 *	it.
 * Returns:
 *	0  success
 *	otherwise failure
 */
int
initrootnet()
{
	struct socket	*so = NULL;
	struct ifreq	ifr;
	int		error;
	register struct genericconf *lgc;
	register struct bus_device *bd;
	extern struct in_ifaddr *in_ifaddr;
	int 		message = 0;

	/* Is there already a primary network? */
	if (in_ifaddr && !(in_ifaddr->ia_ifp->if_flags & IFF_LOOPBACK))
		return(0);

	/*
	 * Decide which interface is our primary one
	 */
	if (gc->gc_root != NODEV) {
		/* We booted locally, find first congifured net */
		for (lgc = genericconf; lgc->gc_driver; lgc++) {
			if (lgc->gc_root == NODEV)
				continue;
			for (bd = bus_dinit; bd->bd_driver; bd++) {
				if (bd->bd_alive == 0)
					continue;
				if (bd->bd_unit == 0 &&
				    bd->bd_driver == gc->gc_driver)
					break;
			}
		}
	} else {
		lgc = gc;
	}
	if (!lgc->gc_driver) {
		return(ENODEV);
	}
	ifr.ifr_name[0] = lgc->gc_name[0];
	ifr.ifr_name[1] = lgc->gc_name[1];
	ifr.ifr_name[2] = '0';
	ifr.ifr_name[3] = '\0';
	error = socreate(AF_INET, &so, SOCK_DGRAM, 0);
	if (error) {
		printf("initrootnet: socreate failed\n");
		goto errout;
	}
	ifr.ifr_addr.sa_family = AF_INET;
	while (error = ifioctl(so, SIOCAUTOADDR, (caddr_t)&ifr)) {
		if (error == ETIMEDOUT) {
			if (!message) {
				printf("initrootnet: BOOTP timed out, still trying...\n");
				message++;
			}
		} else {
			printf("initrootnet: autoaddr failed\n");
			goto errout;
		}
	}
	if (message) {
		printf("initrootnet: BOOTP [OK].\n");
	}
	printf("primary network interface: %s [%s]\n", ifr.ifr_name,
	       inet_ntoa(&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
errout:	
	if (so) {
		soclose(so);
	}
	return(error);
}




