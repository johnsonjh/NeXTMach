/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 * 15-Apr-90  Avadis Tevanian (avie) at NeXT
 *	Remove special case code for pid 0 being the kernel task.
 *
 * 25-Sep-89  Morris Meyer (mmeyer) at NeXT, Inc.
 *	NFS 4.0 changes.
 *
 * 05-Dec-88  Avadis Tevanian (avie) at NeXT, Inc.
 *	Removed TBL_INCLUDE_VERSION.
 *	Removed setaid(), getaid(), setmodes(), getmodes().
 *
 * 25-Oct-88  Gregg Kellogg (gk) at NeXT, Inc.
 *	Added	TBL_MACHFACTOR
 *		TBL_DISKINFO
 *		TBL_CPUINFO
 *		TBL_IOINFO
 *		TBL_DISKINFO
 *		TBL_NETINFO
 *
 * 15-Mar-88  David Golub (dbg) at Carnegie-Mellon University
 *	MACH:  added TBL_PROCINFO to avoid having to read proc table
 *	from /dev/kmem.  Fixed table call to not return EINVAL if at
 *	least one element has been read.
 *
 *  3-Mar-88  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Removed more lint.
 *
 * 19-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminate MACH conditionals.
 *
 *  3-Oct-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 29-Sep-87  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Replaced uses of sizeof(sigcode) with SIGCODE_SIZE.
 *
 * 19-Sep-87  Peter King (king) at NeXT
 *	SUN_VFS: Add support for vnodes.
 *
 * 18-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 17-Aug-87  David Golub (dbg) at Carnegie-Mellon University
 *	Use kernel_pageable_map instead of user_pt_map in table() to
 *	avoid deadlocks.
 *
 * 10-Jul-87  David Golub (dbg) at Carnegie-Mellon University
 *	Use reference count on task (MACH_TT) or map (not TT) to keep a
 *	task's map from vanishing during the TBL_UAREA and TBL_ARGUMENTS
 *	calls.
 *
 * 30-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Split out ttyloc and fsparam portions of table call to simplify
 *	it (the vax compiler really has problem groking this file,
 *	hopefully this will put an end to the problem).  While I was at
 *	it, I reformatted the whole file.
 *
 * 26-May-87  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	MACH_TT: Fixed the TBL_UAREA call to allocate a struct user from
 *	kernel data space rather than on the kernel stack.  Also, adjust
 *	the pointers in the TBL_ARGUMENTS call down over the sigcode (which
 *	now resides above the user stack) to keep ps and friends happy.
 *	I may have even got it right this time.
 *
 *  3-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Deleted some dead code.
 *
 * 15-Apr-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Add TBL_LOADAVG to retrieve load average vector and scale
 *	factor.
 *	[ V5.1(F8) ]
 *
 *  8-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Use new fake_u call for MACH_TT.
 *
 *  2-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Removed dead SPIN_LOCK code, simplified table() a bit to prevent
 *	compiler schain botch.
 *
 *  1-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Updated for latest u-area hacks.
 *
 * 27-Jan-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Implemented rpause() system call;  revised table() call: to
 *	permit set as well as get operation and slightly optimized
 *	vm_map_remove() exit path, to support new TBL_AID, TBL_MODES
 *	(to eventually replace [gs]etaid() and [gs]etmodes()) and
 *	TBL_MAXUPRC options, and to fix minor bug which neglected to
 *	check element length in single element only tables; simplified
 *	setmodes() call (in anticipation of its eventual removal).
 *	[ V5.1(F1) ]
 *
 *  5-Jan-87  David Golub (dbg) at Carnegie-Mellon University
 *	Fixed the hack in TBL_UAREA to copy wired-down U areas, now that
 *	the vm_map code is fixed (if !SPIN_LOCKS).  De-linted.
 *
 *  7-Nov-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added TBL_ARGUMENTS, also for MACH_VM only.
 *	Fixed TBL_UAREA call to give back the U-area itself, not the
 *	block of pages with the U-area and kernel-stack somewhere in it.
 *	This hides the difference between the romp (which puts its U-area
 *	above the kernel stack) and the other machines (which put it
 *	below the kernel stack).
 *	This gets lots easier when MACH_TT is in use.
 *
 * 16-Aug-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Fixed test condition in the "copy u area" loop to
 *	use byte units (i.e. removed "atop" usage).
 *
 *  7-Aug-86  David Golub (dbg) at Carnegie-Mellon University
 *	Merge with official 4.3 release.
 *
 * 29-Jul-86  David Golub (dbg) at Carnegie-Mellon University
 *	vm_deallocate doesn't work on kernel maps... use vm_map_remove
 *	instead.  XXX
 *
 * 28-Jul-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added TBL_UAREA to table() call to get U area for process.  For
 *	now, it only works under the MACH virtual memory.  May be
 *	slightly flaky if thread is unwired while looking at U area.
 *
 * 25-Jul-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added TBL_INCLUDE_VERSION and TBL_FSPARAM.
 *
 * 14-Jul-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	CS_AID: conditionalized setaid() and getaid().  Added include of
 *	cs_aid.h
 *
 * 25-Apr-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_GENERIC:  Added PRIVMODES variable which contains the
 *	privileged UMODE_* modes which only the super-user may set
 *	and otherwise may not be cleared.
 *
 * 23-Mar-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	Removed includes of ../vax/mtpr.h and ../vax/reg.h.
 *
 * 03-Mar-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Change privilege restrictions in setmodes() to prevent some
 *	modes from ever being disabled once set (UMODE_NEWDIR in
 *	particular).
 *
 * 30-Aug-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added U_TTYD table.
 *	[V1(1)]
 *
 * 30-Mar-83  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added table() system call.  We only need this to retrieve the
 *	terminal location information at the moment but the interface
 *	has been designed with a general purpose mechanism in mind.
 *	It may still need some work (V3.06h).
 *
 * 22-Jun-82  Mike Accetta (mja) at Carnegie-Mellon University
 *	Created (V3.05a).
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/vnode.h>
#import <sys/file.h>
#import <sys/proc.h>
#import <sys/conf.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/vfs.h>
#import <ufs/mount.h>
#import <ufs/fs.h>
#import <sys/kernel.h>
#import <sys/table.h>

#import <vm/vm_user.h>
#import <vm/vm_map.h>
#import <vm/vm_kern.h>
#import <vm/vm_param.h>
#import <kern/task.h>
#import <machine/vmparam.h>	/* only way to find the user stack (argblock) */

#import <sys/vmmeter.h>
#import <sys/socket.h>
#import <net/if.h>

#import <sys/version.h>

/*
 *  rpause - resource pause control
 *
 *  errno  = error number of interest
 *  etype  = type of error (used to distinguish between different
 *	     cases of resource exhaustion which all map to the
 *	     same error number)
 *  action = {SAME, ENABLE, DISABLE}
 *
 *  Return: the error type(s) currently enabled for the specified number.
 *
 *  N.B.  This routine is overly general for the current implementation
 *  which supports only ENOSPC and fails to distinguish among the different
 *  error types.
 */

rpause()
{
	register struct a {
		int errno;	/* error number */
		int etype;	/* type of error */
		int action;	/* action to take */
	} *uap;
	int bits, oldbits;

	uap = (struct a *)u.u_ap;
	switch (uap->errno) {
	case ENOSPC:
		if (uap->etype == RPAUSE_ALL) {
			bits = URPS_NOSPC;
			break;
		}
		/* fall through */
	default:
		u.u_error = EINVAL;
		return;
	}

	oldbits = (u.u_rpause&bits);
	switch (uap->action) {
	case RPAUSE_ENABLE:
		u.u_rpause |= bits;
		break;
	case RPAUSE_DISABLE:
		u.u_rpause &= ~bits;
		break;
	case RPAUSE_SAME:
		break;
	default:
		u.u_error = EINVAL;
		return;
	}
	u.u_r.r_val1 = oldbits?RPAUSE_ALL:0;
}

/*
 *  table - get/set element(s) from system table
 *
 *  This call is intended as a general purpose mechanism for retrieving or
 *  updating individual or sequential elements of various system tables and
 *  data structures.
 *
 *  One potential future use might be to make most of the standard system
 *  tables available via this mechanism so as to permit non-privileged programs
 *  to access these common SYSTAT types of data.
 *
 *  Parameters:
 *
 *  id		= an identifer indicating the table in question
 *  index	= an index into this table specifying the starting
 *		  position at which to begin the data copy
 *  addr	= address in user space to receive/supply the data
 *  nel		= number of table elements to retrieve/update
 *  lel		= expected size of a single element of the table.  If this
 *		  is smaller than the actual size, extra data will be
 *		  truncated from the end.  If it is larger, holes will be
 *		  left between elements copied to/from the user address space.
 *
 *		  The intent of separately distinguishing these final two
 *		  arguments is to insulate user programs as much as possible
 *		  from the common change in the size of system data structures
 *		  when a new field is added.  This works so long as new fields
 *		  are added only to the end, none are removed, and all fields
 *		  remain a fixed size.
 *
 *  Returns:
 *
 *  val1	= number of elements retrieved/updated (this may be fewer than
 *		  requested if more elements are requested than exist in
 *		  the table from the specified index).
 *
 *  Note:
 *
 *  A call with lel == 0 and nel == MAXSHORT can be used to determine the
 *  length of a table (in elements) before actually requesting any of the
 *  data.
 */

#define	MAXLEL	(sizeof(long))	/* maximum element length (for set) */

table()
{
	register struct a {
		int id;
		int index;
		caddr_t addr;
		int nel;	/* >0 ==> get, <0 ==> set */
		u_int lel;
	} *uap = (struct a *)u.u_ap;
	caddr_t data;
	unsigned size;
	int error = 0;
	int set;
	vm_offset_t	arg_addr;
	vm_size_t	arg_size;
	int		*ip;
	struct proc	*p;
	vm_offset_t	copy_start, copy_end;
	vm_map_t	proc_map;
	vm_offset_t	dealloc_start;	/* area to remove from kernel map */
	vm_size_t	dealloc_end;

	/*
	 *  Verify that any set request is appropriate.
	 */
	set = 0;
	if (uap->nel < 0) {
		/*
		 * Give the machine dependent code a crack at this first
		 */
		switch (machine_table_setokay(uap->id)) {
		case TBL_MACHDEP_OKAY:
			goto okay;
		case TBL_MACHDEP_BAD:
			u.u_error = EINVAL;
			return;
		case TBL_MACHDEP_NONE:
		default:
			break;
		}
		switch (uap->id) {
		default:
			u.u_error = EINVAL;
			return;
		}
	    okay:
		set++;
		uap->nel = -(uap->nel);
	}

	u.u_r.r_val1 = 0;

	/*
	 *  Verify common case of only current process index.
	 */
	switch (uap->id) {
	case TBL_U_TTYD:
		if ((uap->index != u.u_procp->p_pid && uap->index != 0) ||
				(uap->nel != 1))
			goto bad;
		break;
	}

	/*
	 *  Main loop for each element.
	 */

	while (uap->nel > 0) {
		dev_t nottyd;
		int iv;
		struct tbl_fsparam	tf;
		struct tbl_loadavg	tl;
		struct tbl_procinfo	tp;
		struct tbl_cpuinfo	tc;
		struct tbl_ioinfo	ti;
		struct tbl_netinfo	tn;

		dealloc_start = (vm_offset_t) 0;

		/*
		 * Give machine dependent code a chance
		 */
		switch (machine_table(uap->id, uap->index, uap->addr,
				      uap->nel, uap->lel, set))
		{
		case TBL_MACHDEP_OKAY:
			uap->addr += uap->lel;
			uap->nel -= 1;
			uap->index += 1;
			u.u_r.r_val1 += 1;
			continue;
		case TBL_MACHDEP_NONE:
			break;
		case TBL_MACHDEP_BAD:
		default:
			goto bad;
		}

		switch (uap->id) {
		case TBL_U_TTYD:
			if (u.u_ttyp)
				data = (caddr_t)&u.u_ttyd;
			else {
				nottyd = -1;
				data = (caddr_t)&nottyd;
			}
			size = sizeof (u.u_ttyd);
			break;
		case TBL_LOADAVG:
			if (uap->index != 0 || uap->nel != 1)
				goto bad;
			bcopy((caddr_t)&avenrun[0], (caddr_t)&tl.tl_avenrun[0],
					sizeof(tl.tl_avenrun));
			tl.tl_lscale = LSCALE;
			data = (caddr_t)&tl;
			size = sizeof (tl);
			break;
		case TBL_FSPARAM: {
		        boolean_t table_fsparam();

			if (table_fsparam(uap->index, &tf)) {
				data = (caddr_t)&tf;
				size = sizeof(tf);
				break;
			}
			goto bad;
		}
		case TBL_UAREA:
		   {
			struct user 	*fake;
			task_t		task;

			/*
			 *	Lookup process by pid
			 */
			p = pfind(uap->index);
			if (p == (struct proc *)0) {
				/*
				 *	No such process
				 */
				u.u_error = ESRCH;
				return;
			}

			/*
			 *	Before we can block (any VM code), make
			 *	another reference to the task to keep it
			 *	alive.
			 */

			task = p->task;
			task_reference(task);

			fake = (struct user *)
				kmem_alloc_wait(kernel_pageable_map,
					round_page(sizeof(struct user)));

			task_lock(task);
			fake_u(fake, (thread_t) task->thread_list.next);
			task_unlock(task);
			task_deallocate(task);

			data = (caddr_t) fake;
			size = (vm_size_t) sizeof(struct user);
			dealloc_start = (vm_offset_t) fake;
			dealloc_end = dealloc_start +
				round_page(sizeof(struct user));
			break;
		   }

		case TBL_ARGUMENTS:
			/*
			 *	Returns the top N bytes of the user stack, with
			 *	everything below the first argument character
			 *	zeroed for security reasons.
			 *	Odd data structure is for compatibility.
			 */
			/*
			 *	Lookup process by pid
			 */
			p = pfind(uap->index);
			if (p == (struct proc *)0) {
				/*
				 *	No such process
				 */
				u.u_error = ESRCH;
				return;
			 }
			/*
			 *	Get map for process
			 */
			proc_map = p->task->map;

			/*
			 *	Copy the top N bytes of the stack.
			 *	On all machines we have so far, the stack grows
			 *	downwards.
			 *
			 *	If the user expects no more than N bytes of
			 *	argument list, use that as a guess for the
			 *	size.
			 */
			if ((arg_size = uap->lel) == 0) {
				error = EINVAL;
				goto bad;
			}

			arg_addr = (vm_offset_t)USRSTACK - arg_size;

			/*
			 *	Before we can block (any VM code), make another
			 *	reference to the map to keep it alive.
			 */
			vm_map_reference(proc_map);

			copy_start = kmem_alloc_wait(kernel_pageable_map,
						round_page(arg_size));

			copy_end = round_page(copy_start + arg_size);

			if (vm_map_copy(kernel_pageable_map, proc_map, copy_start,
			    round_page(arg_size), trunc_page(arg_addr),
			    FALSE, FALSE) != KERN_SUCCESS) {
				kmem_free_wakeup(kernel_pageable_map, copy_start,
					round_page(arg_size));
				vm_map_deallocate(proc_map);
				goto bad;
			}

			/*
			 *	Now that we've done the copy, we can release
			 *	the process' map.
			 */
			vm_map_deallocate(proc_map);

#if	( defined(vax) || defined(romp) )
			data = (caddr_t) (copy_end-arg_size-SIGCODE_SIZE);
			ip = (int *) (copy_end - SIGCODE_SIZE);
#else	( defined(vax) || defined(romp) )		
			data = (caddr_t) (copy_end - arg_size);
			ip = (int *) copy_end;		
#endif	( defined(vax) || defined(romp) )		
			size = arg_size;

			/*
			 *	Now look down the stack for the bottom of the
			 *	argument list.  Since this call is otherwise
			 *	unprotected, we can't let the nosy user see
			 *	anything else on the stack.
			 *
			 *	The arguments are pushed on the stack by
			 *	execve() as:
			 *
			 *		.long	0
			 *		arg 0	(null-terminated)
			 *		arg 1
			 *		...
			 *		arg N
			 *		.long	0
			 *
			 */

			ip -= 2; /*skip trailing 0 word and assume at least one
				  argument.  The last word of argN may be just
				  the trailing 0, in which case we'd stop
				  there */
			while (*--ip)
				if (ip == (int *)data)
					break;			
			bzero(data, (unsigned) ((int)ip - (int)data));

			dealloc_start = copy_start;
			dealloc_end = copy_end;
			break;

		case TBL_PROCINFO:
		    {
			register struct proc	*p;
			register struct utask	*utaskp;

			/*
			 *	Index is entry number in proc table.
			 */
#if	NeXT
			/* transition, take negative numbers as pid for now */
			if (uap->index < 0)
				uap->index = -uap->index;	/* compatibility */
			p = pfind(uap->index);
			if (p == (struct proc *)0) {
				/*
				 *	No such process
				 */
					u.u_error = ESRCH;
					return;
			}
#else	NeXT
			if (uap->index >= nproc || uap->index < 0)
			    goto bad;

			p = &proc[uap->index];
#endif	NeXT
			if (p->p_stat == 0) {
			    bzero((caddr_t)&tp, sizeof(tp));
			    tp.pi_status = PI_EMPTY;
			}
			else {
			    tp.pi_uid	= p->p_uid;
			    tp.pi_pid	= p->p_pid;
			    tp.pi_ppid	= p->p_ppid;
			    tp.pi_pgrp	= p->p_pgrp;
			    tp.pi_flag	= p->p_flag;

			    if (p->task == TASK_NULL) {
				tp.pi_status = PI_ZOMBIE;
			    }
			    else {
				utaskp = p->task->u_address;
				if (utaskp->uu_ttyp == 0)
				    tp.pi_ttyd = -1;
				else
				    tp.pi_ttyd = utaskp->uu_ttyd;
				bcopy(utaskp->uu_comm, tp.pi_comm,
				      MAXCOMLEN);
				tp.pi_comm[MAXCOMLEN] = '\0';

				if (p->p_flag & SWEXIT)
				    tp.pi_status = PI_EXITING;
				else
				    tp.pi_status = PI_ACTIVE;
			    }
			}

			data = (caddr_t)&tp;
			size = sizeof(tp);
			break;
		    }

		case TBL_MACHFACTOR:
			if (uap->index != 0 || uap->nel != 1)
				goto bad;
			bcopy((caddr_t)&mach_factor[0],
					(caddr_t)&tl.tl_avenrun[0],
					sizeof(tl.tl_avenrun));
			tl.tl_lscale = LSCALE;
			data = (caddr_t)&tl;
			size = sizeof (tl);
			break;

		case TBL_CPUINFO:
			if (uap->index != 0 || uap->nel != 1)
				goto bad;
			tc.ci_swtch = cnt.v_swtch;
			tc.ci_intr = cnt.v_intr;
			tc.ci_syscall = cnt.v_syscall;
			tc.ci_traps = cnt.v_trap;
			tc.ci_hz = hz;
			tc.ci_phz = phz;
			bcopy(cp_time, tc.ci_cptime, sizeof(cp_time));
			data = (caddr_t)&tc;
			size = sizeof (tc);
			break;

		case TBL_IOINFO: {
			register int i;
			register struct ifnet *ifp;
			if (uap->index != 0 || uap->nel != 1)
				goto bad;
			ti.io_ttin = tk_nin;
			ti.io_ttout = tk_nout;
			ti.io_dkbusy = dk_busy;
			ti.io_ndrive = dk_ndrive;	// don't know any better

			for (i = 0, ifp = ifnet; ifp; ifp = ifp->if_next)
				i++;
			ti.io_nif = i;
			data = (caddr_t)&ti;
			size = sizeof (ti);
			break;
		}

		case TBL_NETINFO: {
			register int i, j;
			register struct ifnet *ifp;
			
			i = uap->index;
			for (ifp = ifnet; ifp && i; ifp = ifp->if_next, i--)
				;
			if (ifp == NULL)
				goto bad;
			tn.ni_ipackets = ifp->if_ipackets;
			tn.ni_ierrors = ifp->if_ierrors;
			tn.ni_opackets = ifp->if_opackets;
			tn.ni_oerrors = ifp->if_oerrors;
			tn.ni_collisions = ifp->if_collisions;
			strncpy(tn.ni_name, ifp->if_name,
				sizeof(tn.ni_name)-2);
			tn.ni_name[j = strlen(tn.ni_name)] =
				'0'+ ifp->if_unit;
			tn.ni_name[j+1] = '\0';
			data = (caddr_t)&tn;
			size = sizeof (tn);
			break;
		}

		default:
		bad:
			/*
			 *	Return error only if all indices
			 *	are invalid.
			 */
			if (u.u_r.r_val1 == 0)
				u.u_error = EINVAL;
			return;
		}
		/*
		 * This code should be generalized if/when other tables
		 * are added to handle single element copies where the
		 * actual and expected sizes differ or the table entries
		 * are not contiguous in kernel memory (as with TTYLOC)
		 * and also efficiently copy multiple element
		 * tables when contiguous and the sizes match.
		 */
		size = MIN(size, uap->lel);
		if (size) {
			if (set) {
				char buff[MAXLEL];

			        error = copyin(uap->addr, buff, size);
				if (error == 0)
					bcopy(buff, data, size);
			}
			else {
				error = copyout(data, uap->addr, size);
			}
		}
		if (dealloc_start != (vm_offset_t) 0) {
			kmem_free_wakeup(kernel_pageable_map, dealloc_start,
				dealloc_end - dealloc_start);
		}
		if (error) {
			u.u_error = error;
			return;
		}
		uap->addr += uap->lel;
		uap->nel -= 1;
		uap->index += 1;
		u.u_r.r_val1 += 1;
	}
}

boolean_t table_fsparam(index, tf)
	int				index;
	register struct tbl_fsparam	*tf;
{
	register struct mount	*mp;
	register struct fs	*fs;

	for (mp = mounttab; mp != NULL; mp = mp->m_nxt)
		if (mp->m_bufp != 0 && mp->m_dev == index)
			continue;
	if (mp == NULL)
		return(FALSE);

	fs = mp->m_bufp->b_un.b_fs;
	tf->tf_used = freefrags(fs);
	tf->tf_size = fs->fs_dsize;
	tf->tf_iused = freeinodes(fs);
	tf->tf_isize = fs->fs_ncg*fs->fs_ipg;
	return(TRUE);
}







