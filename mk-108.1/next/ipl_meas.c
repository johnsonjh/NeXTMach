/*	@(#)ipl_meas.c	1.0	2/1/88		(c) 1988 NeXT	*/

/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 *  19-Jun-89 Mike DeMoney (mike) at NeXT
 *	Modified to provide xpr's if spl()/splx() relationships violated
 *
 *  22-May-89 Mike DeMoney (mike) at NeXT
 * 	Started major mods
 *  1-Feb-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 
#import <iplmeas.h>
#if NIPLMEAS

#import <sys/types.h>
#import <sys/ioctl.h>
#import <sys/time.h>
#import <sys/errno.h>
#import <sys/uio.h>
#import <kern/xpr.h>
#import <kern/assert.h>
#import <next/cframe.h>
#import <next/machparam.h>
#import <next/cframe.h>
#import <next/ipl_meas.h>
#import <next/psl.h>
#import <next/eventc.h>
#import <next/vm_types.h>

#define meas_debug(f) { XPR(XPR_MEAS, f); }
#define meas_debug1(f) { if (ipl_debug & 1) { XPR(XPR_MEAS, f); } }

static inline int Rspl7(void)
{
	register int old_sr;

	asm volatile ("movw	sr,%0" : "=dm"((short)old_sr));
	asm volatile ("movw	%1,sr" : "=m" (*(char *)0): "Jdm" (0x2700));
	return old_sr;
}

static inline int Rsplx(int new_sr)
{
	register int old_sr;

	asm volatile ("movw	sr,%0" : "=dm" ((short)old_sr));
	asm volatile ("movw	%1,sr" : "=m" (*(char *)0): "Jdm" ((short)new_sr));
	return old_sr;
}

static struct ipl_data ipl_data[8];
#define	NINTRS	256
static struct intr_times intr_times[NINTRS];
static int last_ipl;
static int last_ipl_valid;
int ipl_meas = 0;
int ipl_debug = 0;

extern struct frame *getfp();
static void ipl_traceback();

static void
ipl_change(old_ipl, new_ipl, pc)
int old_ipl, new_ipl;
char *pc;
{
	register struct ipl_data *idp;
	register u_int usecs, cur_usecs;

	if (ipl_meas == 0) {
		last_ipl_valid = 0;
		return;
	}
	if (ipl_debug)
		xprflags |= XPR_MEAS;

	if (last_ipl_valid && old_ipl != last_ipl)
		meas_debug(("MISSED IPL CHANGE: last 0x%x old 0x%x new 0x%x\n",
		    last_ipl, old_ipl, new_ipl));
	last_ipl = new_ipl;
	last_ipl_valid = 1;

	if (new_ipl > old_ipl) {
		/* entering a new level */
		usecs = event_get();
		for (idp = &ipl_data[old_ipl+1];
		     idp <= &ipl_data[new_ipl]; idp++) {
			idp->pc_enter = pc;
			idp->us_enter = usecs;
		}
	} else if (new_ipl < old_ipl) {
		/* leaving a level */
		cur_usecs = event_get();
		for (idp = &ipl_data[new_ipl+1];
		    idp <= &ipl_data[old_ipl]; idp++) {
			if (idp->pc_enter == 0)
				continue;
			usecs = cur_usecs - idp->us_enter;
			ASSERT((int)usecs >= 0)
			idp->us_total += usecs;
			idp->n_exits++;
			if (usecs > idp->us_max) {
				idp->us_max = usecs;
				idp->pc_enter_max = idp->pc_enter;
				idp->pc_exit_max = pc;
			}
		}
	}
}

int
ipl_splu(pc, new_sr)
char *pc;
int new_sr;
{
	register old_ipl, new_ipl, old_sr;

	old_sr = Rspl7();
	old_ipl = srtoipl(old_sr);
	new_ipl = srtoipl(new_sr);
	meas_debug1(("splu %d %d 0x%x\n", old_ipl, new_ipl, pc));
#ifdef DEBUG
	if (new_ipl < old_ipl)
		ipl_traceback("UP", getfp(), old_ipl, new_ipl, pc);
#endif DEBUG
	ipl_change(old_ipl, new_ipl, pc);
	/* caller will change ipl to new_sr, so we don't have to */
	return(old_sr);
}

int
ipl_spld(pc, new_sr)
char *pc;
int new_sr;
{
	register old_ipl, new_ipl, old_sr;

	old_sr = Rspl7();
	old_ipl = srtoipl(old_sr);
	new_ipl = srtoipl(new_sr);
	meas_debug1(("spld %x %x %x\n", old_sr, new_sr, pc));
#ifdef DEBUG
	if (new_ipl > old_ipl)
		ipl_traceback("DOWN", getfp(), old_ipl, new_ipl, pc);
#endif DEBUG
	ipl_change(old_ipl, new_ipl, pc);
	/* caller will change ipl to new_sr, so we don't have to */
	return(old_sr);
}

void
ipl_rte(new_sr)
int new_sr;
{
	register old_sr = Rspl7();

	meas_debug1(("rte %x %x\n", old_sr, new_sr));
	/* nothing significant about the 3rd arg -- it's just non-zero */
	ipl_change(srtoipl(old_sr), srtoipl(new_sr), 2);
	/* rte will change ipl to new_sr, so we don't have to */
}

void
ipl_urte(new_sr)
int new_sr;
{
	register old_sr = Rspl7();
	extern int lastsyscall;

	/*
	 * Break out rte's to user mode, because if we want we could,
	 * ASSERT(srtoipl(old_sr) == 0 && srtoipl(new_sr) == 0);
	 * for some added ipl debugging.
	 */
#ifdef notdef
	ASSERT(srtoipl(old_sr) == 0 && srtoipl(new_sr) == 0);
#endif notdef
#ifdef DEBUG
	if (srtoipl(old_sr) != 0 || srtoipl(new_sr) != 0)
		meas_debug(("BAD URTE: old_sr 0x%x new_sr 0x%x"
		    " lastsyscall 0x%x pc 0x%x\n",
		    old_sr, new_sr, lastsyscall, callsite()));
#endif DEBUG
	meas_debug1(("urte %x %x\n", old_sr, new_sr));
	/* nothing significant about the 3rd arg -- it's just non-zero */
	ipl_change(srtoipl(old_sr), srtoipl(new_sr), 2);
	/* rte will change ipl to new_sr, so we don't have to */
}

void
ipl_intr(old_sr)
int old_sr;
{
	register new_sr = Rspl7();

	meas_debug1(("intr %x %x\n", old_sr, new_sr));
	/* nothing significant about the 3rd arg -- it's just non-zero */
	ipl_change(srtoipl(old_sr), srtoipl(new_sr), 1);
	Rsplx(new_sr);
}

static inline struct intr_times *intr_find(int (*func)(), int arg, int ipl)
{
	register i;
	register struct intr_times *itp;
	int s;

	itp = &intr_times[((int)func ^ arg ^ ipl) & (NINTRS-1)];
	i = 0;
	for (;;) {
		if (itp->func == func && itp->arg == arg && itp->ipl == ipl)
			break;
		if (itp->func == 0) {
			s = Rspl7();
			if (((volatile struct intr_times *)itp)->func) {
				Rsplx(s);
				continue;
			}
			itp->func = func;
			Rsplx(s);
			itp->arg = arg;
			itp->ipl = ipl;
			break;
		}
		if (i++ >= NINTRS)
			panic("out of intr_times slots");
		if (++itp >= &intr_times[NINTRS])
			itp = intr_times;
	}
	return(itp);
}

intr_call(func, arg, pc, ps)
int (*func)();
int arg;
{
	register struct intr_times *itp;
	register u_int usecs, usecs2;
	register result, ipl, s;

	if (!ipl_meas)
		return ((*func)(arg, pc, ps));

	ipl = curipl();
	/* softint_run is called at multiple ipl levels */
	itp = intr_find(func, arg, ipl);
	s = Rspl7();
	usecs2 = ipl_data[ipl+1].us_total;
	usecs = event_get();
	Rsplx(s);
	result = (*func)(arg, pc, ps);
	s = Rspl7();
	usecs = event_delta(usecs);
	usecs -= ipl_data[ipl+1].us_total - usecs2;
	Rsplx(s);
	if (usecs > itp->us_max)
		itp->us_max = usecs;
	itp->us_total += usecs;
	itp->n_exits++;
	if (itp->ipl != curipl())
		meas_debug(("BAD INTR IPL: func 0x%x called ipl %d exit ipl %d\n",
		    func, itp->ipl, curipl()));
	return (result);
}

int
ipl_open(dev)
	dev_t dev;
{
	if (minor(dev) != 0)
		return(EINVAL);

	return(0);
}

int
ipl_read(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register int error = 0;
	register int s, i;
	struct ipl_data id[8], *idp;
	struct intr_times it, *itp;

	s = Rspl7();
	bcopy(ipl_data, id, sizeof(id));
	Rsplx(s);
	error = uiomove((caddr_t)id, sizeof(id), UIO_READ, uio);
	for (itp = intr_times;
	    itp < &intr_times[NINTRS] && uio->uio_resid && !error;
	    itp++) {
		if (itp->func) {
			s = Rspl7();
			bcopy(itp, &it, sizeof(it));
			Rsplx(s);
			error = uiomove((caddr_t)&it,sizeof(it),UIO_READ,uio);
		}
	}
	return(error);
}

int
ipl_ioctl(dev, cmd, arg)
	dev_t dev;
	int cmd;
	caddr_t arg;
{
	register int i = 0;
	register int s, error;
	struct ipl_data id;
	struct intr_ioctl *iip;
	struct intr_times it, *itp, *uitp;

	switch(cmd) {

	case IPLIOCCTRL:
		s = Rspl7();
		ipl_meas = *(int *)arg;
		for (i = 0; i <= 7; i++)
			ipl_data[i].pc_enter = 0;
		Rsplx(s);
		break;
 
	case IPLIOCCLR:
		s = Rspl7();
		for (itp = intr_times; itp < &intr_times[NINTRS]; itp++) {
			itp->func = 0;
			itp->us_max = 0;
			itp->us_total = 0;
			itp->n_exits = 0;
		}
		Rsplx(s);
		s = Rspl7();
		for (i = 0; i <= 7; i++) {
			ipl_data[i].pc_enter = 0;
			ipl_data[i].us_total = 0;
			ipl_data[i].us_max = 0;
			ipl_data[i].n_exits = 0;
		}
		Rsplx(s);
		break;

	case IPLIOCGET7: i++;
	case IPLIOCGET6: i++;
	case IPLIOCGET5: i++;
	case IPLIOCGET4: i++;
	case IPLIOCGET3: i++;
	case IPLIOCGET2: i++;
	case IPLIOCGET1: i++;
		s = Rspl7();
		id = ipl_data[i];
		Rsplx(s);
		*(struct ipl_data *)arg = id;
		break;

	case IPLIOCGETINT:
		iip = (struct intr_ioctl *)arg;
		iip->used = 0;
		i = iip->entries;
		uitp = iip->itp;
		error = 0;
		for (itp = intr_times;
		    itp < &intr_times[NINTRS] && ! error;
		    itp++) {
			if (itp->func) {
				iip->used++;
				if (i > 0) {
					i--;
					s = Rspl7();
					bcopy(itp, &it, sizeof(it));
					Rsplx(s);
					error = copyout(&it, uitp, sizeof(it));
					uitp++;
				}
			}
		}
		return(error);

	default:
		return(EINVAL);
	}

	return(0);
}

static caddr_t bad_ipls[256 /* size MUST BE power of 2 */];

#ifndef NELEM
#define	NELEM(x)	(sizeof(x)/sizeof(x[0]))
#endif NELEM

static inline ipl_seenit(caddr_t pc)
{
	register caddr_t *bip;
	register int j, s;

	/* cheap hash */
	bip = &bad_ipls[(int)pc & (NELEM(bad_ipls) - 1)];
	j = 0;
	for (;;) {
		if (*bip == pc)
			break;
		if (*bip == 0) {
			s = Rspl7();
			if (*(volatile caddr_t *)bip) {
				Rsplx(s);
				continue;
			}
			*bip = pc;
			Rsplx(s);
			return(0);
		}
		if (j++ >= NELEM(bad_ipls))
			break;		/* we've seen enough for now */
		if (++bip >= &bad_ipls[NELEM(bad_ipls)])
			bip = bad_ipls;
	}
	return(1);
}

static void
ipl_traceback(direction, fp, old_ipl, new_ipl, pc)
char *direction;
register struct frame *fp;
int old_ipl;
int new_ipl;
caddr_t pc;
{
	extern int intstack, eintstack;
	int first_time;

	for (first_time = 1;
	     ((u_int)fp & 1) == 0
	        && (  ( (u_int)fp >= (u_int)&intstack
		        && (u_int)fp <= (u_int)&eintstack)
	           || ( (u_int)fp >= VM_MIN_KERNEL_ADDRESS
			&& (u_int)fp <= VM_MAX_KERNEL_ADDRESS)
	           );
	      fp = fp->f_fp
	
	) {
		if (first_time) {
			if (ipl_seenit((caddr_t)fp->f_pc))
				return;
			first_time = 0;
			meas_debug(("BAD SPL %s: old %d new %d pc 0x%x\n",
			    direction, old_ipl, new_ipl, pc));
			continue;
		}
		meas_debug(("called from 0x%x (0x%x)\n", fp->f_pc, fp->f_arg));
		if (fp == fp->f_fp) {
			meas_debug(("LOOPING fp\n"));
			break;
		}
	}
	meas_debug(("last fp 0x%x\n", fp));
}
#endif	NIPLMEAS
