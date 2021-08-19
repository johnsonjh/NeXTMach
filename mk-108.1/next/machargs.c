/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 *  2-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Removed sched_debug.
 *	Added mach_ldebug.
 */

#import <show_space.h>
#import <mach_ldebug.h>
#import <gdb.h>

#import <sys/types.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/errno.h>
#import <sys/reboot.h>
#import <sys/kernel.h>
#import <kern/xpr.h>
#import <next/event_meter.h>
#import <next/cpu.h>

extern int cache, hz, phz, nbuf, bufpages,
	nmi_cont, nmi_gdb, nmi_mon, nmi_help, nmi_halt, nmi_msg,
	nmi_stay, nmi_reboot, nmi_big, ncallout, pagesize, mem, rootrw,
	astune_rate, astune_calls;
extern char init_program_name[], rootname[], rootdevice[];

int	debug;		/* general purpose kernel debug flag */
int	maxphys = 8192;	/* max physio size due to DMA chip bug */

#if	DEBUG
extern int endbug, timer_debug, dmadbug, show_info, timer_dbug,
	traptrace, signaltrace, syscalltrace;

#import <sound.h>
#if	NSOUND > 0
extern int sdbgflag;
extern int dspdbgflag;
#endif
#endif	DEBUG

#if	FP_DEBUG
extern int fp;
#endif	FP_DEBUG

#if	XPR_DEBUG
extern int nmi_xpr, nmi_xprclear, nxprbufs;
#endif	XPR_DEBUG

#if	SHOW_SPACE
extern int show_space;
#endif	SHOW_SPACE

#if	MACH_LDEBUG
extern int mach_ldebug;
#endif	MACH_LDEBUG

#if	GDB
extern int breakpoint;
extern int rmtdebug;
#endif	GDB

#import <od.h>
#if	NOD
extern int od_frmr, od_land, od_spin, od_noverify, od_maxecc,
	mi_rv_delay, mi_expire, mi_expire_sh, mi_expire_jmp, mi_expire_max,
	mi_expire_sh2, od_errmsg_filter, od_id_r, od_id_v, od_id_o,
	od_maxecc_l1, od_maxecc_h1, od_maxecc_l2, od_maxecc_h2, od_write_retry,
	od_requested, od_dbug;
extern u_char od_flgstr[];
#if	DEBUG
extern int od_dbug1;
#endif	DEBUG
#endif	NOD

#import <iplmeas.h>
#if NIPLMEAS
extern int ipl_debug;
extern int ipl_meas;
#endif NIPLMEAS

#import <mallocdebug.h>
#if	MALLOCDEBUG
extern int mallocdebug;
#endif	MALLOCDEBUG

struct kernargs {
	char *name;
	int *i_ptr;
	u_char *c_ptr;		/* for devices mapped by BMAP only! */
} kernargs[] = {

	/* nmi mini-monitor commands -- should be first */
	"?", &nmi_help, 0,
	"help", &nmi_help, 0,
	"continue", &nmi_cont, 0,
	"gdb", &nmi_gdb, 0,
	"monitor", &nmi_mon, 0,
	"reboot", &nmi_reboot, 0,
	"halt", &nmi_halt, 0,
	"msg", &nmi_msg, 0,
	"stay", &nmi_stay, 0,
	"big", &nmi_big, 0,
	
#if	XPR_DEBUG
	"xpr", &nmi_xpr, 0,
	"xprclear", &nmi_xprclear, 0,
#endif	XPR_DEBUG
	"cache", &cache, 0,
	"hz", &hz, 0,
	"phz", &phz, 0,
#if	EVENTMETER
	"em_chirp", &em_chirp, 0,
	"p0", &em.pid[0], 0,
	"p1", &em.pid[1], 0,
	"p2", &em.pid[2], 0,
#endif	EVENTMETER
	"nbuf", &nbuf, 0,
	"bufpages", &bufpages, 0,
	"pagesize", &pagesize, 0,
	"mem", &mem, 0,
#ifdef	SLOT_ID
#undef	SLOT_ID
#endif	SLOT_ID
#define SLOT_ID 0
#ifdef	SLOT_ID_BMAP
#undef	SLOT_ID_BMAP
#endif	SLOT_ID_BMAP
#define	SLOT_ID_BMAP 0
	"dspicr", 0, (u_char *)(P_DSP+0),
	"dspcvr", 0, (u_char *)(P_DSP+1),
	"dspisr", 0, (u_char *)(P_DSP+2),
	"dspivr", 0, (u_char *)(P_DSP+3),
	"dsph", 0, (u_char *)(P_DSP+5),
	"dspm", 0, (u_char *)(P_DSP+6),
	"dspl", 0, (u_char *)(P_DSP+7),
	"debug", &debug, 0,
	"maxphys", &maxphys, 0,
	"ncallout", &ncallout, 0,
#if	DEBUG
	"endbug", &endbug, 0,
	"signaltrace", &signaltrace, 0,
	"traptrace", &traptrace, 0,
	"timer_debug", &timer_debug, 0,
	"show_info", &show_info, 0,
#if	NSOUND > 0
	"sdbgflag", &sdbgflag, 0,
	"sounddbg", &sdbgflag, 0,
	"dspdbgflag", &dspdbgflag, 0,
#endif
	"dmadbug", &dmadbug, 0,
	"syscalltrace", &syscalltrace, 0,
	"hostid", (int*)&hostid, 0,
#endif	DEBUG
#if	FP_DEBUG
	"fp", &fp, 0,
#endif	FP_DEBUG
#if	MACH_LDEBUG
	"mach_ldebug", &mach_ldebug, 0,
#endif	MACH_LDEBUG
#if	XPR_DEBUG
	"nxprbufs", &nxprbufs, 0,
	"xprflags", (int *)&xprflags, 0,
	"xprwrap", (int *)&xprwrap, 0,
#endif	XPR_DEBUG
#if	SHOW_SPACE
	"show_space", &show_space, 0,
#endif	SHOW_SPACE
#if	GDB
	"breakpoint", &breakpoint, 0,
	"rmtdebug", &rmtdebug, 0,
#endif	GDB
#if	NOD
	"od_frmr", &od_frmr, 0,
	"od_land", &od_land, 0,
	"od_spin", &od_spin, 0,
	"od_noverify", &od_noverify, 0,
	"od_maxecc", &od_maxecc, 0,
	"od_maxecc_l1", &od_maxecc_l1, 0,
	"od_maxecc_h1", &od_maxecc_h1, 0,
	"od_maxecc_l2", &od_maxecc_l2, 0,
	"od_maxecc_h2", &od_maxecc_h2, 0,
	"od_write_retry", &od_write_retry, 0,
	"od_flgstr1", (int*)&od_flgstr[0], 0,
	"od_flgstr2", (int*)&od_flgstr[4], 0,
	"mi_rv_delay", &mi_rv_delay, 0,
	"mi_expire", &mi_expire, 0,
	"mi_expire_sh", &mi_expire_sh, 0,
	"mi_expire_sh2", &mi_expire_sh2, 0,
	"mi_expire_jmp", &mi_expire_jmp, 0,
	"mi_expire_max", &mi_expire_max, 0,
	"od_errmsg_filter", &od_errmsg_filter, 0,
	"od_id_r", &od_id_r, 0,
	"od_id_v", &od_id_v, 0,
	"od_id_o", &od_id_o, 0,
	"od_requested", &od_requested, 0,
	"od_dbug", &od_dbug, 0,
#if	DEBUG
	"od_dbug1", &od_dbug1, 0,
#endif	DEBUG
#endif	NOD
#if	MALLOCDEBUG
	"mallocdebug", &mallocdebug, 0,
#endif	MALLOCDEBUG
#if	NIPLMEAS
	"ipl_debug", &ipl_debug, 0,
	"ipl_meas", &ipl_meas, 0,
#endif	NIPLMEAS
	"init", (int*) init_program_name, 0,
	"rootdir", (int*) rootname, 0,
	"rootdev", (int*) rootdevice, 0,
	"rootrw", &rootrw, 0,
	"astune_rate", &astune_rate, 0,
	"astune_calls", &astune_calls, 0,
	0,0,0,
};


/*
 * getargs()
 *
 * 1. loop through the arg list:
 *
 * 1a. if switch is "-x", copy the switch from the boot stack to kernel
 *  space so it can be passed to init process.
 *
 * 1a. if switch is "sw=n", set kernel flag sw to number n; if "=n" is
 *  omitted set flag to 1.
 *
 * 1c. if switch is "sw=str", set kernel flag sw to string str.
 *
 * 1d. if switch is "sw=", printf the current value of the switch.
 */

#define	NUM	0
#define	STR	1

getargs(args, interactive)
register char *args;
{
	register char *cp, c;
	struct kernargs *kp;
	register i, ret = 0;
	int val;
	
	/* Find arg on command line */
#if	DEBUG
	boothowto = RB_DEBUG;
#endif	DEBUG
	if (*args == 0)
		return (1);
	while(isargsep(*args))
		args++;
	while(*args) {
		if (*args == '-') {
			extern char init_args[];
			register char *ia = init_args;

			argstrcpy(args, init_args);
			printf("init arg: %s\n", init_args);
			do {
				switch (*ia) {
				case 'a':
					boothowto |= RB_ASKNAME;
					break;
				case 'b':
					boothowto |= RB_NOBOOTRC;
					break;
				case 's':
					boothowto |= RB_SINGLE;
					break;
				case 'i':
					boothowto |= RB_INITNAME;
					break;
				case 'p':
					boothowto |= RB_DEBUG;
					break;
				}
			} while (*ia && !isargsep(*ia++));
		} else {
			cp = args;
			while (!isargsep (*cp) && *cp != '=')
				cp++;
			c = *cp;
			*cp = 0;
			for(kp=kernargs;kp->name;kp++) {
				i = strlen (args);
				if (strncmp(args, kp->name, i))
					continue;
				*cp = c;
				if (interactive && *cp == '=' &&
				    isargsep(*(cp+1))) {
				    	if (kp->i_ptr)
					    	printf ("kernel flag %s = 0x%x\n",
						kp->name, *kp->i_ptr);
					else
					    	printf ("kernel flag %s = 0x%x\n",
						kp->name, *(kp->c_ptr +
						slot_id_bmap));
					goto gotit;
				}
				if (!interactive)
					printf ("kernel flag %s = ", kp->name);
				switch (getval(cp, &val)) {
				case NUM:
					if (kp->i_ptr)
						*kp->i_ptr = val;
					else
						*(kp->c_ptr + slot_id_bmap) =
							val;
					if (interactive)
						break;
					if (kp->i_ptr)
						printf("0x%x\n", *kp->i_ptr);
					else
						printf("0x%x\n", *(kp->c_ptr +
							slot_id_bmap));
					break;
				case STR:
					argstrcpy(++cp, kp->i_ptr);
					if (interactive)
						break;
					printf ("\"%s\"\n", kp->i_ptr);
					break;
				}
				goto gotit;
			}
			printf("kernel flag %s unknown\n",args);
			ret = 1;
		}
gotit:
		/* Skip over current arg */
		while(!isargsep(*args))
			args++;
		/* Skip leading white space (catch end of args) */
		while(isargsep(*args) && *args != '\0')
			args++;
	}
	return (ret);
}

isargsep(c)
char c;
{
	if (c == ' ' || c == '\0' || c == '\t' || c == ',')
		return(1);
	else
		return(0);
}

argstrcpy(from, to)
char *from, *to;
{
	int i = 0;

	while (!isargsep(*from)) {
		i++;
		*to++ = *from++;
	}
	return(i);
}

getval(s, val)
register char *s;
int *val;
{
	register unsigned radix, intval;
	register unsigned char c;
	int sign = 1;

	if (*s == '=') {
		s++;
		if (*s == '-')
			sign = -1, s++;
		intval = *s++-'0';
		radix = 10;
		if (intval == 0)
			switch(*s) {

			case 'x':
				radix = 16;
				s++;
				break;

			case 'b':
				radix = 2;
				s++;
				break;

			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				intval = *s-'0';
				s++;
				radix = 8;
				break;

			default:
				if (!isargsep(*s))
					return (STR);
			}
		for(;;) {
			if (((c = *s++) >= '0') && (c <= '9'))
				c -= '0';
			else if ((c >= 'a') && (c <= 'f'))
				c -= 'a' - 10;
			else if ((c >= 'A') && (c <= 'F'))
				c -= 'A' - 10;
			else if (isargsep(c))
				break;
			else
				return (STR);
			if (c >= radix)
				return (STR);
			intval *= radix;
			intval += c;
		}
		*val = intval * sign;
		return (NUM);
	}
	*val = 1;
	return (NUM);
}

/* system call for setting parameters */
machparam()
{
	register struct a {
		char *argname;
		int value;
	} *uap;
	char nbuf[32];
	int n;
	register struct kernargs *kp;

	uap = (struct a *)u.u_ap;
	if (!suser())
		return;
	u.u_error = copyinstr(uap->argname, &nbuf, sizeof (nbuf), &n);
	if (u.u_error)
		return;
	for (kp = kernargs; *(kp->name); kp++)
		if (strcmp(kp->name, nbuf) == 0) {
			if (kp->i_ptr)
				*(kp->i_ptr) += uap->value;
			else
				*(kp->c_ptr + slot_id_bmap) += uap->value;
			return;
		}
	u.u_error = EINVAL;
}









