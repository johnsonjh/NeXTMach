#if	CMU
/* 
 **********************************************************************
 * Mach Operating System
 * Copyright (c) 1986 Carnegie-Mellon University
 *  
 * This software was developed by the Mach operating system
 * project at Carnegie-Mellon University's Department of Computer
 * Science. Software contributors as of May 1986 include Mike Accetta, 
 * Robert Baron, William Bolosky, Jonathan Chew, David Golub, 
 * Glenn Marcy, Richard Rashid, Avie Tevanian and Michael Young. 
 * 
 * Some software in these files are derived from sources other
 * than CMU.  Previous copyright and other source notices are
 * preserved below and permission to use such software is
 * dependent on licenses from those institutions.
 * 
 * Permission to use the CMU portion of this software for 
 * any non-commercial research and development purpose is
 * granted with the understanding that appropriate credit
 * will be given to CMU, the Mach project and its authors.
 * The Mach project would appreciate being notified of any
 * modifications and of redistribution of this software so that
 * bug fixes and enhancements may be distributed to users.
 *
 * All other rights are reserved to Carnegie-Mellon University.
 **********************************************************************
 * HISTORY
 * 12-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Boot MACH, not UNIX.
 *
 * 01-Jul-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_BOOT:  picked up the ULTRIX-32 boot options to pass in
 *	the boot unit in the second byte of R10 and the partition in
 *	the high 4 bits of R11 so so that these may be passed through
 *	to UNIX for use in determining the root device;
 *	CS_KDB:  changed copyunix() to save the symbol table after _end
 *	if bit 2 (04) was supplied in the boot flags and pass its
 *	top through to UNIX in R9.
 *
 * 20-Aug-85  Robert V Baron (rvb) at Carnegie-Mellon University
 *	Allow code to be loaded directly into shared memory, by letting
 *	the high 16 bits of "howto" (r11) specify a 64k boundary foR
 *	loading to.
 *	Pass the correct major/minor as r10 to the program that was
 *	started.
 *
 **************************************************************************
 */

#endif	CMU
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)boot.c	7.1 (Berkeley) 6/5/86
 */

#define	A_OUT_COMPAT	1

#import <sys/param.h>
#ifdef SUN_NFS
#import <sys/time.h>
#import <sys/vnode.h>
#import <ufs/inode.h>
#import <ufs/fs.h>
#else !SUN_NFS
#import <sys/inode.h>
#import <sys/fs.h>
#endif !SUN_NFS
#import <sys/vm.h>
#import <sys/loader.h>
#if	A_OUT_COMPAT
#import <stand/exec.h>
#endif	A_OUT_COMPAT
#import <nextdev/dma.h>
#import <mon/global.h>			/* 0.9 ROM compat */
#import <stand/saio.h>

char line[100];

int	retry = 0;

/*
 *  The original contents of these registers upon entry to the program 
 *  are made available as parameters here by the run-time startup code.
 */
main(bootfile)
char *bootfile;
{
	int io;
	int error;
	struct mon_global *mg = (struct mon_global*) restore_mg();

	error = 0;
	for (;;) {
	
		/* don't ask if animating, just return */
		if (error && (mg->km.flags & KMF_SEE_MSGS) == 0) {
			runit (0);
			next_monstart();
		}
		if (error || bootfile == 0 || *bootfile == '\0' ||
		    (int)bootfile == (int)restore_mg() /* 0.9 ROM compat */ ) {
			printf("\nblk0 boot: ");
			gets(line);
			if (*line == '\0')
				return;
		} else {
			strcpy(line, bootfile);
			printf("blk0 boot: %s\n", line);
		}
		io = open(line, 0);
		if (io >= 0) {
			copyunix(io, bootfile);
			close(io);
		}

		/* if we get here, something went wrong */
		printf ("load failed\n");
		error = 1;
	}
}

/* #define BOOT_DEBUG	1	/* */

/*ARGSUSED*/
copyunix(io, bootfile)
register io;
{
#define	SLOP	512
	char x[1024+SLOP];
	int i, size;
	int (*entry)();
	char *addr;
	struct mon_global *mg = (struct mon_global*) restore_mg();
	struct nvram_info *ni = &mg->mg_nvram;
 
#ifdef	BOOT_DEBUG
	printf("copyunix\n");	
#endif	BOOT_DEBUG
	if ((i = read(io, &x, sizeof x - SLOP)) <= 0)
		goto readerr;
	size = loader (x, i, &entry, &addr);
	if (read(io, addr, size) <= 0)
		goto readerr;
	
#ifdef	BOOT_DEBUG
	printf("copyunix: transferring to 0x%x\n", entry);	
#endif	BOOT_DEBUG
	/* 0.9 ROM compat */
	if (bootfile == (int)mg) {
		(*entry) (mg, 0, 0, mg->mg_boot_dev, mg->mg_boot_arg,
		mg->mg_boot_info, mg->mg_sid, mg->mg_pagesize, N_SIMM,
		mg->mg_region, 0x8, mg->mg_boot_file);
	}
#ifdef	BOOT_DEBUG
	printf("copyunix: returning from *entry\n");
#endif	BOOT_DEBUG
	/*
	 * runit returns to prom monitor with entry point address in
	 * d0.  Prom monitor does actual transfer to loaded image.
	 */
	runit(entry);
	next_monstart();
readerr:
	_stop("read error\n");
}

/* process a.out and mach.o object file headers */
loader (bp, len, entry, loadp)
	char *bp;
	char **entry, **loadp;
{
#if	A_OUT_COMPAT
	struct exec *ap = (struct exec*) bp;
#endif	A_OUT_COMPAT
	struct mach_header *mh = (struct mach_header*) bp;
	struct segment_command *sc;
	int size, cmd, first_seg = 0;

#if	A_OUT_COMPAT
	if (ap->a_magic == OMAGIC) {
		size = ap->a_text + ap->a_data;
		*loadp = *entry = (char*) ap->a_entry;
		bp += sizeof (struct exec);
		len -= sizeof (struct exec);
	} else
#endif	A_OUT_COMPAT
	if (mh->magic == MH_MAGIC && mh->filetype == MH_PRELOAD) {
		sc = (struct segment_command*)
			(bp + sizeof (struct mach_header));
		first_seg = 1;
		size = 0;

		/*
		 *  Only look at the first two segments (assumes that text and data
		 *  sections will reside there).  Can't look at all of them because
		 *  the header probably exceeds the buffer size.
		 */
		for (cmd = 0; cmd < 2; cmd++) {
			switch (sc->cmd) {
			
			case LC_SEGMENT:
				if (first_seg) {
					*entry = *loadp = (char*) sc->vmaddr;
					bp += sc->fileoff;
					len -= sc->fileoff;
					first_seg = 0;
				}
				size += sc->filesize;
				break;
			}
			sc = (struct segment_command*)
				((int)sc + sc->cmdsize);
		}
	} else
		goto bad;
	if (len)
		bcopy (bp, *loadp, len);
	*loadp += len;
	size -= len;
	return (size);
bad:
	_stop ("unknown binary format\n");
}




