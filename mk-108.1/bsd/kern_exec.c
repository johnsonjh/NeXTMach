/*
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes:  Changed SPAGI to SPAGV.  Removed #include dir.h.
 *
 * 24-Oct-88  Steve Stone (steve) at NeXT
 *	Added Attach trace support.
 *
 * 22-Jul-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Support for Mach-O files.
 *
 * 19-Apr-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Removed old history, purged lots of dead code.
 */
 
#import <cputypes.h>

#import <sun_lock.h>

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_exec.c	7.1 (Berkeley) 6/5/86
 */

#import <machine/reg.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/kernel.h>
#import <sys/proc.h>
#import <sys/buf.h>
#import <sys/socketvar.h>	/* SUN_VFS */
#import <sys/vnode.h>		/* SUN_VFS */
#import <sys/pathname.h>	/* SUN_VFS */
#import <sys/file.h>
#import <sys/uio.h>
#import <sys/acct.h>
#import <sys/vfs.h>		/* SUN_VFS */
#import <sys/exception.h>

#import <sys/signal.h>
#import <kern/task.h>
#import <kern/thread.h>

#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <vm/vm_object.h>
#import <vm/vnode_pager.h>
#import <vm/vm_kern.h>
#import <vm/vm_user.h>
#import <kern/zalloc.h>

#import <kern/parallel.h>

#import <sys/loader.h>

#import <machine/vmparam.h>

/*
 * exec system call, with and without environments.
 */
struct execa {
	char	*fname;
	char	**argp;
	char	**envp;
};

execv()
{
	((struct execa *)u.u_ap)->envp = NULL;
	u.u_error = execve();
}

execve()
{
	register nc;
	register char *cp;
	register struct execa *uap;
	int na, ne, ucp, ap, cc;
	unsigned len;
	int indir, uid, gid;
	char *sharg;
	char *execnamep;
	struct vnode *vp;
	struct vattr vattr;
	vm_offset_t exec_args;
	struct pathname pn;
#define	SHSIZE	32
	char cfarg[SHSIZE];
	struct mach_header	*header;
	kern_return_t		ret;
	boolean_t		needargs;
	struct utask		*utask;
	union {
		char	ex_shell[SHSIZE];  /* #! and name of interpreter */
		struct mach_header	header;
	} exdata;

	int resid, error;

	uap = (struct execa *)u.u_ap;
	utask = current_task()->u_address;
	error = pn_get(uap->fname, UIO_USERSPACE, &pn);
	if (error) {
		u.u_error = error;	/* XXX */
  		return(error);
	}
	error = lookuppn(&pn, FOLLOW_LINK, (struct vnode **)0, &vp);
	if (error || vp == 0) {
		pn_free(&pn);
		u.u_error = error;	/* XXX */
		return(error);
	}
	exec_args = 0;
	indir = 0;
	uid = utask->uu_uid;
	gid = utask->uu_gid;
	if (error = VOP_GETATTR(vp, &vattr, utask->uu_cred))
		goto bad;
	if ((vp->v_vfsp->vfs_flag & VFS_NOSUID) == 0) {
		/*
		 *	Can't let setuid things get abused by others
		 *	through the IPC interface
		 */

		if (vattr.va_mode & (VSUID | VSGID)) {
			if (task_secure(current_task())) {
				if (vattr.va_mode & VSUID)
					uid = vattr.va_uid;
				if (vattr.va_mode & VSGID)
					gid = vattr.va_gid;
			}
			else {
				uprintf("%s: privileges disabled because of outstanding IPC access to task\n",
					utask->uu_comm);
			}
		}
	} else if ((vattr.va_mode & VSUID) || (vattr.va_mode & VSGID)) {
		struct pathname tmppn;

		error = pn_get(uap->fname, UIO_USERSPACE, &tmppn);
		if (error)
			goto bad; /* this used to be return; this causes a
					pn_free when pn_get fails  */
		uprintf("%s: Setuid execution not allowed\n", tmppn.pn_buf);
		pn_free(&tmppn);
	}

  again:
	error = check_exec_access(vp);
	if (error)
		goto bad;


	/*
	 * Read in first few bytes of file for segment sizes, magic number:
	 *	407 = plain executable
	 *	410 = RO text
	 *	413 = demand paged RO text
	 * Also an ASCII line beginning with #! is
	 * the file name of a ``shell'' and arguments may be prepended
	 * to the argument list if given here.
	 *
	 * SHELL NAMES ARE LIMITED IN LENGTH.
	 *
	 * ONLY ONE ARGUMENT MAY BE PASSED TO THE SHELL FROM
	 * THE ASCII LINE.
	 */
	exdata.ex_shell[0] = '\0';	/* for zero length files */
	error =
	    vn_rdwr(UIO_READ, vp, (caddr_t)&exdata, sizeof (exdata),
		0, UIO_SYSSPACE, IO_UNIT, &resid);
	if (error)
		goto bad;
#ifndef lint
	if (resid > sizeof(exdata) - sizeof(exdata.header) &&
	    exdata.ex_shell[0] != '#') {
		error = ENOEXEC;
		goto bad;
	}
#endif lint
	header = (struct mach_header *) &exdata;
	if (header->magic != MH_MAGIC) {
		if (exdata.ex_shell[0] != '#' ||
		    exdata.ex_shell[1] != '!' ||
		    indir) {
			error = ENOEXEC;
			goto bad;
		}
		cp = &exdata.ex_shell[2];		/* skip "#!" */
		while (cp < &exdata.ex_shell[SHSIZE]) {
			if (*cp == '\t')
				*cp = ' ';
			else if (*cp == '\n') {
				*cp = '\0';
				break;
			}
			cp++;
		}
		if (*cp != '\0') {
			error = ENOEXEC;
			goto bad;
		}
		cp = &exdata.ex_shell[2];
		while (*cp == ' ')
			cp++;
		execnamep = cp;
		while (*cp && *cp != ' ')
			cp++;
		cfarg[0] = '\0';
		if (*cp) {
			*cp++ = '\0';
			while (*cp == ' ')
				cp++;
			if (*cp)
				bcopy((caddr_t)cp, (caddr_t)cfarg, SHSIZE);
		}
		indir = 1;
		VN_RELE(vp);
		vp = (struct vnode *)0;
		if (error = pn_set(&pn, execnamep))
			goto bad;
		error = lookuppn(&pn, FOLLOW_LINK, (struct vnode **)0, &vp);
		if (error)
			goto bad;
		if (error = VOP_GETATTR(vp, &vattr, u.u_cred))
			goto bad;
		goto again;
	}

	/*
	 * Collect arguments on "file" in swap space.
	 */
	na = 0;
	ne = 0;
	nc = 0;
	cc = 0;
	exec_args = kmem_alloc_wait(kernel_pageable_map, NCARGS);
	cp = (char *) exec_args;	/* running pointer for copy */
	cc = NCARGS;			/* size of exec_args */
	/*
	 * Copy arguments into file in argdev area.
	 */
	if (uap->argp) for (;;) {
		ap = NULL;
		sharg = NULL;
		if (indir && na == 0) {
			sharg = pn.pn_buf;
			ap = (int)sharg;
			uap->argp++;		/* ignore argv[0] */
		} else if (indir && (na == 1 && cfarg[0])) {
			sharg = cfarg;
			ap = (int)sharg;
		} else if (indir && (na == 1 || na == 2 && cfarg[0]))
			ap = (int)uap->fname;
		else if (uap->argp) {
			ap = fuword((caddr_t)uap->argp);
			uap->argp++;
		}
		if (ap == NULL && uap->envp) {
			uap->argp = NULL;
			if ((ap = fuword((caddr_t)uap->envp)) != NULL)
				uap->envp++, ne++;
		}
		if (ap == NULL)
			break;
		na++;
		if (ap == -1) {
			error = EFAULT;
			break;
		}
		do {
			if (nc >= NCARGS-1) {
				error = E2BIG;
				break;
			}
			if (sharg) {
				error = copystr(sharg, cp, (unsigned)cc, &len);
				sharg += len;
			} else {
				error = copyinstr((caddr_t)ap, cp, (unsigned)cc,
				    &len);
				ap += len;
			}
			cp += len;
			nc += len;
			cc -= len;
		} while (error == ENOENT);
		if (error) {
			goto badarg;
		}
	}
	nc = (nc + NBPW-1) & ~(NBPW-1);

	ret = load_machfile(vp, header, &needargs);
	if (ret != KERN_SUCCESS) {
		error = EBADEXEC;
		goto bad;
	}
	/*
	 * set SUID/SGID protections, if no tracing
	 */
	if ((utask->uu_procp->p_flag&STRC)==0) {
		if (uid != utask->uu_uid || gid != utask->uu_gid)
			utask->uu_cred = crcopy(utask->uu_cred);
		utask->uu_uid = uid;
		utask->uu_procp->p_uid = uid;
		utask->uu_gid = gid;
	} else
		thread_doexception(current_thread(), EXC_BREAKPOINT, 0, 0);

	/*
	 *	Temp hack until ld changes... allocate page 0 if its not
	 *	already allocated.
	 */
	{
		vm_offset_t	addr = 0;
		kern_return_t	ret;

		ret = vm_allocate(current_task()->map, &addr, PAGE_SIZE,
				  FALSE);
		if (ret == KERN_SUCCESS) {
			(void)vm_protect(current_task()->map, 0, PAGE_SIZE,
				FALSE, VM_PROT_NONE);
		}
	}
	if (error) {
badarg:
	/*
	 *	NOTE: to prevent a race condition, getxfile had
	 *	to temporarily unlock the inode.  If new code needs to
	 *	be inserted here before the iput below, and it needs
	 *	to deal with the inode, keep this in mind.
	 */
		goto bad;
	}
	VN_RELE(vp);
	vp = NULL;

	/*
	 * Copy back arglist if necessary.
	 */
	ucp = USRSTACK;
	if (needargs) {
		ucp = ucp - nc - NBPW;
		ap = ucp - na*NBPW - 3*NBPW;
		u.u_ar0[SP] = ap;
		(void) suword((caddr_t)ap, na-ne);
		nc = 0;
		cc = 0;
		cp = (char *) exec_args;
		cc = NCARGS;
		for (;;) {
			ap += NBPW;
			if (na == ne) {
				(void) suword((caddr_t)ap, 0);
				ap += NBPW;
			}
			if (--na < 0)
				break;
			(void) suword((caddr_t)ap, ucp);
			do {
				error = copyoutstr(cp, (caddr_t)ucp,
						   (unsigned)cc, &len);
				ucp += len;
				cp += len;
				nc += len;
				cc -= len;
			} while (error == ENOENT);
			if (error == EFAULT)
	/*			panic("exec: EFAULT");*/
				break;	/* bad stack - user's problem */
		}
		(void) suword((caddr_t)ap, 0);
	}

	/*
	 * Reset caught signals.  Held signals
	 * remain held through p_sigmask.
	 */
	while (utask->uu_procp->p_sigcatch) {
		nc = ffs((long)utask->uu_procp->p_sigcatch);
		utask->uu_procp->p_sigcatch &= ~sigmask(nc);
		utask->uu_signal[nc] = SIG_DFL;
	}
	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	utask->uu_onstack = 0;
	utask->uu_sigsp = 0;
	utask->uu_sigonstack = 0;

	for (nc = utask->uu_lastfile; nc >= 0; --nc) {
		if (utask->uu_pofile[nc] & UF_EXCLOSE) {
#if	SUN_LOCK
			register struct file *f;

			f = utask->uu_ofile[nc];
			/* Release all System-V style record locks, if any */
			(void) vno_lockrelease(f);
			closef(f);
#else	SUN_LOCK
			closef(utask->uu_ofile[nc]);
#endif	SUN_LOCK
			utask->uu_ofile[nc] = NULL;
			utask->uu_pofile[nc] = 0;
		}
		utask->uu_pofile[nc] &= ~UF_MAPPED;
	}
	while (utask->uu_lastfile >= 0 && utask->uu_ofile[utask->uu_lastfile] == NULL)
		utask->uu_lastfile--;

	u.u_eosys = JUSTRETURN;	/* XXX */

	/*
	 * Remember file name for accounting.
	 */
	utask->uu_acflag &= ~AFORK;
	if (pn.pn_pathlen > MAXCOMLEN)
		pn.pn_pathlen = MAXCOMLEN;
	bcopy((caddr_t)pn.pn_buf, (caddr_t)utask->uu_comm,
	      (unsigned)(pn.pn_pathlen + 1));
bad:
	pn_free(&pn);
	if (exec_args != 0)
		kmem_free_wakeup(kernel_pageable_map, exec_args, NCARGS);
	if (vp)
		VN_RELE(vp);
	u.u_error = error;	/* XXX */
	return(error);
}

#define	unix_stack_size	(u.u_rlimit[RLIMIT_STACK].rlim_cur)

kern_return_t create_unix_stack(map)
	vm_map_t	map;
{
	vm_size_t	size;
	vm_offset_t	addr;

	size = round_page(unix_stack_size);
	addr = trunc_page((vm_offset_t)USRSTACK - size);
/*	(void) vm_map_remove(map, addr, addr + size);*/
	return(vm_map_find(map, VM_OBJECT_NULL, (vm_offset_t) 0,
			&addr, size, FALSE));
}

#import <sys/reboot.h>

char		init_program_name[128] =
			"/etc/mach_init\0";

char		init_args[128] = "-xx\0";
struct execa	init_exec_args;
int		init_attempts = 0;

void load_init_program()
{
	vm_offset_t	init_addr;
	int		*old_ap;
	char		*argv[3];
	int		error;

	unix_master();

	error = 0;

	/* init_args are copied in string form directly from bootstrap */
	
	do {
		if (boothowto & RB_INITNAME) {
			printf("init program? ");
			gets(init_program_name, init_program_name);
		}

		if (error && ((boothowto & RB_INITNAME) == 0) &&
					(init_attempts == 1)) {
			static char other_init[] = "/etc/init";
			printf("Load of %s, errno %d, trying %s\n",
				init_program_name, error, other_init);
			error = 0;
			bcopy(other_init, init_program_name,
							sizeof(other_init));
		}

		init_attempts++;

		if (error) {
			printf("Load of %s failed, errno %d\n",
					init_program_name, error);
			error = 0;
			boothowto |= RB_INITNAME;
			continue;
		}

		/*
		 *	Copy out program name.
		 */

		init_addr = VM_MIN_ADDRESS;
		(void) vm_allocate(current_task()->map, &init_addr,
				PAGE_SIZE, TRUE);
		if (init_addr == 0)
			init_addr++;
		(void) copyout((caddr_t) init_program_name,
				(caddr_t) (init_addr),
				(unsigned) sizeof(init_program_name)+1);

		argv[0] = (char *) init_addr;
		init_addr += ((sizeof(init_program_name)+1)*sizeof(long))/
				sizeof(long);

		/*
		 *	Put out first (and only) argument, similarly.
		 *	Assumes everything fits in a page as allocated
		 *	above.
		 */

		(void) copyout((caddr_t) init_args,
				(caddr_t) (init_addr),
				(unsigned) sizeof(init_args));

		argv[1] = (char *) init_addr;
		init_addr += ((sizeof(init_args)+1)*sizeof(long))/
				sizeof(long);

		/*
		 *	Null-end the argument list
		 */

		argv[2] = (char *) 0;
		
		/*
		 *	Copy out the argument list.
		 */
		
		(void) copyout((caddr_t) argv,
				(caddr_t) (init_addr),
				(unsigned) sizeof(argv));

		/*
		 *	Set up argument block for fake call to execve.
		 */

		init_exec_args.fname = argv[0];
		init_exec_args.argp = (char **) init_addr;
		init_exec_args.envp = 0;

		old_ap = u.u_ap;
		u.u_ap = (int *) &init_exec_args;
		error = execve();
		u.u_ap = old_ap;
	} while (error);

	unix_release();
}

check_exec_access(vp)
	struct vnode	*vp;
{
	struct vattr	vattr;
	int		error;
	struct ucred	*cred;

	cred = u.u_cred;
	if (error = VOP_GETATTR(vp, &vattr, cred))
		return(error);

	/*
	 * XXX should change VOP_ACCESS to not let super user always have it
	 * for exec permission on regular files.
	 */
	if (error = VOP_ACCESS(vp, VEXEC, cred))
		return(error);
	if ((u.u_procp->p_flag & STRC)
	    && (error = VOP_ACCESS(vp, VREAD, cred)))
		return(error);
	if (vp->v_type != VREG ||
	   (vattr.va_mode & (VEXEC|(VEXEC>>3)|(VEXEC>>6))) == 0) {
		return(EACCES);
	}
	return(0);
}
