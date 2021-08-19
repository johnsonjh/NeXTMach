/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 *  1-Feb-90  Brian Pinkerton at NeXT
 *	Removed umount (again).
 *
 *  1-Feb-90  John Seamons (jks) at NeXT
 *	Added second agrument to reboot syscall.  Interpreted as the reboot 
 *	command if RB_COMMAND flag is set in first argument.
 *
 *  9-May-89  Peter King (king) at NeXT
 *	Sun Bugfixes: 1010524 - If NFSCLIENT is turned off, do an errsys
 *				rather than a nosys (which causes core to
 *				get dumped).
 *
 * 18-Jan-89  Peter King (king) at NeXT
 *	NFS 4.0 Changes: Replaced old wait (7) with wait4.  Removed umount
 *	(22).  Added getdents() and ustat().  
 *	Cleaned up SUN_VFS switches.
 */

#import <cputypes.h>

#import <nfs_client.h>
#import <nfs_server.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * System call switch table.
 */

#import <sys/param.h>
#import <sys/systm.h>

	/* serial or parallel system call */
#define syss(fn,no) {no, 0, fn}
#define sysp(fn,no) {no, 1, fn}

int	nosys();
int	nullsys();
int	errsys();

/* 1.1 processes and protection */
int	sethostid(),gethostid(),sethostname(),gethostname(),getpid();
int	setdomainname(), getdomainname();
int	fork(),rexit(),execv(),execve(),wait(),wait3();
int	getuid(),setreuid(),getgid(),getgroups(),setregid(),setgroups();
int	getpgrp(),setpgrp();

/* 1.2 memory management */
int	sbrk(),sstk();
int	getpagesize(),smmap(),mremap(),munmap(),mprotect(),madvise(),mincore();

/* 1.3 signals */
int	sigvec(),sigblock(),sigsetmask(),sigpause(),sigstack(),sigreturn();
int	kill(), killpg();
#if     CS_SYSCALL 
int	osigcleanup();		/* XXX 4.2 sigcleanup call, used by longjmp */
#endif  CS_SYSCALL

/* 1.4 timing and statistics */
int	gettimeofday(),settimeofday();
int	getitimer(),setitimer();
int 	adjtime();

/* 1.5 descriptors */
int	getdtablesize(),dup(),dup2(),close();
int	select(),getdopt(),setdopt(),fcntl(),flock();

/* 1.6 resource controls */
int	getpriority(),setpriority(),getrusage(),getrlimit(),setrlimit();
#ifdef QUOTA
int	oldquota(), quotactl();
#else
#define oldquota nullsys	/* for backward compatability with old login */
#endif QUOTA

/* 1.7 system operation support */
#if	NeXT
int	smount(),swapon();
#else	NeXT
int	umount(),smount(),swapon();
#endif	NeXT
int	unmount();
int	sync(),reboot(),sysacct();

/* 2.1 generic operations */
int	read(),write(),readv(),writev(),ioctl();

/* 2.2 file system */
int	chdir(),chroot(),wait4();
int	mkdir(),rmdir();
int	getdirentries(), getdents();
int	creat(),open(),mknod(),unlink(),stat(),fstat(),lstat();
int	chown(),fchown(),chmod(),fchmod(),utimes();
int	link(),symlink(),readlink(),rename();
int	lseek(),truncate(),ftruncate(),access(),fsync();
int	ustat(),statfs(),fstatfs();

/* 2.3 communications */
int	socket(),bind(),listen(),accept(),connect();
int	socketpair(),sendto(),send(),recvfrom(),recv();
int	sendmsg(),recvmsg(),shutdown(),setsockopt(),getsockopt();
int	getsockname(),getpeername(),pipe();

int	umask();		/* XXX */

/* 2.4 processes */
int	ptrace();

/* 2.5 terminals */

#define	syss_getdirentries()	syss(nosys,0)
#define	syss_getdomainname()	syss(nosys,0)	
#define	syss_setdomainname()	syss(nosys,0)	

#define	compat(n, name)		{0, 0, nosys}

/* BEGIN JUNK */
int	profil();		/* 'cuz sys calls are interruptible */
int	vhangup();		/* should just do in exit() */
int	vfork();		/* awaiting fork w/ copy on write */
int	obreak();		/* awaiting new sbrk */
int	ovadvise();		/* awaiting new madvise */
/* END JUNK */

#if	NFS_CLIENT
/* nfs */
int	async_daemon();		/* client async daemon */

#define	syss_async_daemon()	syss(async_daemon,0)
#else	NFS_CLIENT
#define	syss_async_daemon()	syss(errsys,0)
#endif	NFS_CLIENT

#ifdef	NFS_SERVER
int	nfs_svc();		/* run nfs server */
int	nfs_getfh();		/* get file handle */
int	exportfs();		/* export file systems */

#define	syss_nfs_svc()		syss(nfs_svc,1)
#define	syss_nfs_getfh()	syss(nfs_getfh,2)
#define	syss_exportfs()		syss(exportfs,2)
#else	NFS_SERVER
#define	syss_nfs_svc()		syss(nosys,0)
#define	syss_nfs_getfh()	syss(nosys,0)
#define	syss_exportfs()		syss(errsys,0)
#endif	NFS_SERVER

#if	NeXT
extern int machparam();
#endif	NeXT
extern int rpause();
extern int table();

struct sysent sysent[] = {
	syss(nosys,0),			/*   0 = indir */
	syss(rexit,1),			/*   1 = exit */
	syss(fork,0),			/*   2 = fork */
	sysp(read,3),			/*   3 = read */
	sysp(write,3),			/*   4 = write */
	syss(open,3),			/*   5 = open */
	syss(close,1),			/*   6 = close */
	syss(wait4, 4),			/*   7 = wait4 */
	syss(creat,2),			/*   8 = creat */
	syss(link,2),			/*   9 = link */
	syss(unlink,1),			/*  10 = unlink */
	syss(execv,2),			/*  11 = execv */
	syss(chdir,1),			/*  12 = chdir */
		compat(otime,0),	/*  13 = old time */
	syss(mknod,3),			/*  14 = mknod */
	syss(chmod,2),			/*  15 = chmod */
	syss(chown,3),			/*  16 = chown; now 3 args */
	syss(obreak,1),			/*  17 = old break */
#if	NeXT
	syss(nosys, 0),			/*  18 = was old stat */
#else
		compat(ostat,2),	/*  18 = old stat */
#endif	NeXT
	syss(lseek,3),			/*  19 = lseek */
	sysp(getpid,0),			/*  20 = getpid */
	syss(nosys, 0),			/*  21 = old mount */
#if	NeXT
	syss(nosys, 0),			/*  22 = was old umount */
#else	NeXT
	syss(umount, 1),		/*  22 = old umount */
#endif	NeXT
		compat(osetuid,1),	/*  23 = old setuid */
	sysp(getuid,0),			/*  24 = getuid */
		compat(ostime,1),	/*  25 = old stime */
	syss(ptrace,4),			/*  26 = ptrace */
		compat(oalarm,1),	/*  27 = old alarm */
#if	NeXT
	syss(nosys,0),			/*  28 = was old fstat */
	syss(nosys,0),			/*  29 = was opause */
	syss(nosys,0),			/*  30 = was old utime */
#else	NeXT
		compat(ofstat,2),	/*  28 = old fstat */
		compat(opause,0),	/*  29 = opause */
		compat(outime,2),	/*  30 = old utime */
#endif	NeXT
	syss(nosys,0),			/*  31 = was stty */
	syss(nosys,0),			/*  32 = was gtty */
	syss(access,2),			/*  33 = access */
		compat(onice,1),	/*  34 = old nice */
		compat(oftime,1),	/*  35 = old ftime */
	syss(sync,0),			/*  36 = sync */
	syss(kill,2),			/*  37 = kill */
	syss(stat,2),			/*  38 = stat */
		compat(osetpgrp,2),	/*  39 = old setpgrp */
	syss(lstat,2),			/*  40 = lstat */
	syss(dup,2),			/*  41 = dup */
	syss(pipe,0),			/*  42 = pipe */
		compat(otimes,1),	/*  43 = old times */
	syss(profil,4),			/*  44 = profil */
	syss(nosys,0),			/*  45 = nosys */
		compat(osetgid,1),	/*  46 = old setgid */
	sysp(getgid,0),			/*  47 = getgid */
		compat(ossig,2),	/*  48 = old sig */
	syss(nosys,0),			/*  49 = reserved for USG */
	syss(nosys,0),			/*  50 = reserved for USG */
	syss(sysacct,1),		/*  51 = turn acct off/on */
	syss(nosys,0),			/*  52 = old set phys addr */
	syss(nosys,0),			/*  53 = old lock in core */
	syss(ioctl,3),			/*  54 = ioctl */
	syss(reboot,2),			/*  55 = reboot */
	syss(nosys,0),			/*  56 = old mpxchan */
	syss(symlink,2),		/*  57 = symlink */
	syss(readlink,3),		/*  58 = readlink */
	syss(execve,3),			/*  59 = execve */
	syss(umask,1),			/*  60 = umask */
	syss(chroot,1),			/*  61 = chroot */
	syss(fstat,2),			/*  62 = fstat */
	syss(nosys,0),			/*  63 = used internally */
	sysp(getpagesize,0),		/*  64 = getpagesize */
	syss(mremap,5),			/*  65 = mremap */
	syss(vfork,0),			/*  66 = vfork */
	syss(read,3),			/*  67 = old vread */
	syss(write,3),			/*  68 = old vwrite */
	syss(sbrk,1),			/*  69 = sbrk */
	syss(sstk,1),			/*  70 = sstk */
	syss(smmap,6),			/*  71 = mmap */
	syss(ovadvise,1),		/*  72 = old vadvise */
	syss(munmap,2),			/*  73 = munmap */
	syss(mprotect,3),		/*  74 = mprotect */
	syss(madvise,3),		/*  75 = madvise */
	syss(vhangup,1),		/*  76 = vhangup */
		compat(ovlimit,2),	/*  77 = old vlimit */
	syss(mincore,3),		/*  78 = mincore */
	sysp(getgroups,2),		/*  79 = getgroups */
	sysp(setgroups,2),		/*  80 = setgroups */
	sysp(getpgrp,1),		/*  81 = getpgrp */
	sysp(setpgrp,2),		/*  82 = setpgrp */
	syss(setitimer,3),		/*  83 = setitimer */
	syss(wait,0),			/*  84 = wait */
	syss(swapon,1),			/*  85 = swapon */
	syss(getitimer,2),		/*  86 = getitimer */
	sysp(gethostname,2),		/*  87 = gethostname */
	sysp(sethostname,2),		/*  88 = sethostname */
	sysp(getdtablesize,0),		/*  89 = getdtablesize */
	syss(dup2,2),			/*  90 = dup2 */
	sysp(getdopt,2),		/*  91 = getdopt */
	syss(fcntl,3),			/*  92 = fcntl */
	syss(select,5),			/*  93 = select */
	syss(setdopt,2),		/*  94 = setdopt */
	syss(fsync,1),			/*  95 = fsync */
	sysp(setpriority,3),		/*  96 = setpriority */
	syss(socket,3),			/*  97 = socket */
	syss(connect,3),		/*  98 = connect */
	syss(accept,3),			/*  99 = accept */
	sysp(getpriority,2),		/* 100 = getpriority */
	syss(send,4),			/* 101 = send */
	syss(recv,4),			/* 102 = recv */
	syss(sigreturn,1),		/* 103 = sigreturn */
	syss(bind,3),			/* 104 = bind */
	syss(setsockopt,5),		/* 105 = setsockopt */
	syss(listen,2),			/* 106 = listen */
		compat(ovtimes,2),	/* 107 = old vtimes */
	syss(sigvec,3),			/* 108 = sigvec */
	syss(sigblock,1),		/* 109 = sigblock */
	syss(sigsetmask,1),		/* 110 = sigsetmask */
	syss(sigpause,1),		/* 111 = sigpause */
	syss(sigstack,2),		/* 112 = sigstack */
	syss(recvmsg,3),		/* 113 = recvmsg */
	syss(sendmsg,3),		/* 114 = sendmsg */
	syss(nosys,0),			/* 115 = nosys */
	syss(gettimeofday,2),		/* 116 = gettimeofday */
	sysp(getrusage,2),		/* 117 = getrusage */
	syss(getsockopt,5),		/* 118 = getsockopt */
	syss(nosys,0),			/* 119 = nosys */
	sysp(readv,3),			/* 120 = readv */
	sysp(writev,3),			/* 121 = writev */
	syss(settimeofday,2),		/* 122 = settimeofday */
	syss(fchown,3),			/* 123 = fchown */
	syss(fchmod,2),			/* 124 = fchmod */
	syss(recvfrom,6),		/* 125 = recvfrom */
	sysp(setreuid,2),		/* 126 = setreuid */
	sysp(setregid,2),		/* 127 = setregid */
	syss(rename,2),			/* 128 = rename */
	syss(truncate,2),		/* 129 = truncate */
	syss(ftruncate,2),		/* 130 = ftruncate */
	syss(flock,2),			/* 131 = flock */
	syss(nosys,0),			/* 132 = nosys */
	syss(sendto,6),			/* 133 = sendto */
	syss(shutdown,2),		/* 134 = shutdown */
	syss(socketpair,5),		/* 135 = socketpair */
	syss(mkdir,2),			/* 136 = mkdir */
	syss(rmdir,1),			/* 137 = rmdir */
	syss(utimes,2),			/* 138 = utimes */
#if	NeXT
	/* SUN binary compatibility */
	syss(sigreturn,0),		/* 139 = 4.2 signal cleanup */
#else	NeXT
	syss(nosys,0),			/* 139 = used internally */
#endif	NeXT
	syss(adjtime,2),		/* 140 = adjtime */
	syss(getpeername,3),		/* 141 = getpeername */
	sysp(gethostid,0),		/* 142 = gethostid */
	sysp(errsys,0),			/* 143 = old sethostid */
	sysp(getrlimit,2),		/* 144 = getrlimit */
	sysp(setrlimit,2),		/* 145 = setrlimit */
	syss(killpg,2),			/* 146 = killpg */
	syss(nosys,0),			/* 147 = nosys */
	syss(oldquota, 0),/* XXX */	/* 148 = old quota */
	syss(oldquota, 0),/* XXX */	/* 149 = old qquota */
	syss(getsockname,3),		/* 150 = getsockname */
	/*
	 * Syscalls 151-180 inclusive are reserved for vendor-specific
	 * system calls.  (This includes various calls added for compatibity
	 * with other Unix variants.)
	 */
#if	NeXT
	syss(machparam,2),		/* 151 = machparam */
	syss(nosys,0),			/* 152 */
	syss(nosys,0),			/* 153 */
	syss(nosys,0),			/* 154 */
	syss_nfs_svc(),			/* 155 = nfs_svc */
	syss(getdirentries,4),		/* 156 = getdirentries */
	syss(statfs, 2),		/* 157 = statfs */
	syss(fstatfs, 2),		/* 158 = fstatfs */
	syss(unmount, 1),		/* 159 = unmount */
	syss_async_daemon(),		/* 160 = async_daemon */
	syss_nfs_getfh(),		/* 161 = get file handle */
	syss(getdomainname,2),		/* 162 = getdomainname */
	syss(setdomainname,2),		/* 163 = setdomainname */
	syss(nosys,0),			/* 164 */
#if	QUOTA
	syss(quotactl, 4),		/* 165 = quotactl */
#else	QUOTA
	syss(errsys, 0),		/* 165 = not configured */
#endif	QUOTA
	syss_exportfs(),		/* 166 = exportfs */
	syss(smount, 4),		/* 167 = mount */
	syss(ustat,2),			/* 168 = ustat */
	syss(nosys,0),			/* 169 */
	syss(table, 5),			/* 170 = table lookup (for ps) */
	syss(wait3, 3),			/* 171 = wait3 */
	syss(rpause, 3),		/* 172 = rpause (resource pausing) */
	syss(nosys,0),			/* 173 */
	syss(getdents,3),		/* 174 = getdents */
	syss(nosys,0),			/* 175 */
	syss(nosys,0),			/* 176 */
	syss(nosys,0),			/* 177 */
	syss(nosys,0),			/* 178 */
	syss(nosys,0),			/* 179 */
	syss(nosys,0),			/* 180 */
#endif	NeXT
#if	!defined(vax) && !defined(romp) && !defined(sun3) && !(BALANCE) && !(NeXT)
	syss(nosys,0),			/* 151 */
	syss(nosys,0),			/* 152 */
	syss(nosys,0),			/* 153 */
	syss(nosys,0),			/* 154 */
	syss(nosys,0),			/* 155 */
	syss(nosys,0),			/* 156 */
	syss(nosys,0),			/* 157 */
	syss(nosys,0),			/* 158 */
	syss(nosys,0),			/* 159 */
	syss(nosys,0),			/* 160 */
	syss(nosys,0),			/* 161 */
	syss(nosys,0),			/* 162 */
	syss(nosys,0),			/* 163 */
	syss(nosys,0),			/* 164 */
	syss(nosys,0),			/* 165 */
	syss(nosys,0),			/* 166 */
	syss(nosys,0),			/* 167 */
	syss(nosys,0),			/* 168 */
	syss(nosys,0),			/* 169 */
	syss(nosys,0),			/* 170 */
	syss(nosys,0),			/* 171 */
	syss(nosys,0),			/* 172 */
	syss(nosys,0),			/* 173 */
	syss(nosys,0),			/* 174 */
	syss(nosys,0),			/* 175 */
	syss(nosys,0),			/* 176 */
	syss(nosys,0),			/* 177 */
	syss(nosys,0),			/* 178 */
	syss(nosys,0),			/* 179 */
	syss(nosys,0),			/* 180 */
#endif	!defined(vax) && !defined(romp) && !defined(sun3) && !(BALANCE) && !(NeXT)
	syss(nosys,0),			/* 181 */
};
int	nsysent = sizeof (sysent) / sizeof (sysent[0]);
