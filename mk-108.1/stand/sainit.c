/*
 * Copyright (c) 1990, NeXT Inc.
 *
 * HISTORY
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Variable p_eprom is now set with the base address of the EPROM because
 *	it moved in the 040 implementation.
 */
 
#import <sys/signal.h>
#import <sys/errno.h>
#import <next/trap.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/bmap.h>
#import <mon/global.h>
#import <ctype.h>
#import <stand/trap.h>

/* #define	KDBG_DEBUG	1	/* */

int debugging = 0;
struct pcb client_pcb;
int *nofault, *probe_nofault;
int client_running;

extern int reg[];

extern int _trap();

static int scb[256];
static int *vbrsave = 0;
static jmpbuf dbgjb;

static void dbg_trap();
static char *btoh();
static char *htobs();
static char *htob();
static int rdmem();
static int wrmem();
static void kquit();
static void putnib();
static void putpkt();
static int ack();
static int getnib();
static int getpkt();
static int gfunc();
static int kresume();
static void pfunc();

#define	ERROR(x)	printf(x)

typedef	unsigned unlong;

struct sf_4wd {
	short sf4_sr;
	unlong sf4_pc;
	short sf4_ftype:4,		/* can be 0 or 1 */
	      sf4_vecoff:12;
};

struct sf_6wd {
	short sf6_sr;
	unlong sf6_pc;
	short sf6_ftype:4,		/* must be 2 */
	      sf6_vecoff:12;
	unlong sf6_instadr;
};

struct sf_float {
	short sf_sr;
	unlong sf_pc;
	short sf_ftype:4,		/* must be 3 */
	      sf_vecoff:12;
	unlong sf_effadr;
};

struct sf_access {
	short sf_sr;
	unlong sf_pc;
	short sf_ftype:4,		/* must be 7 */
	      sf_vecoff:12;
	unlong sf_junk[13];
};

struct sf_10wd {
	short sf10_sr;
	unlong sf10_pc;
	short sf10_ftype:4,		/* must be 9 */
	      sf10_vecoff:12;
	unlong sf10_instadr;
	short sf10_junk[4];
};

struct sf_16wd {
	short sf16_sr;
	unlong sf16_pc;
	short sf16_ftype:4,		/* must be 10 */
	      sf16_vecoff:12;
	short sf16_junk[12];
};

struct sf_46wd {
	short sf46_sr;
	unlong sf46_pc;
	short sf46_ftype:4,		/* must be 11 */
	      sf46_vecoff:12;
	short sf46_junk[42];
};

union stk_frms {
	struct sf_4wd sf_4wd;
	struct sf_6wd sf_6wd;
	struct sf_10wd sf_10wd;
	struct sf_16wd sf_16wd;
	struct sf_46wd sf_46wd;
	struct sf_float sf_float;
	struct sf_access sf_access;
};

#define	sf_sr		sf_4wd.sf4_sr
#define	sf_pc		sf_4wd.sf4_pc
#define	sf_ftype	sf_4wd.sf4_ftype
#define	sf_vecoff	sf_4wd.sf4_vecoff

struct pcb {
	unsigned usp;
	unsigned regs[16];
	short zero;
	union stk_frms sf;
};

int p_eprom;

sainit(edata, end, vbr)
int *edata, *end, *vbr;
{
	register int i, t;
	extern int _monsp;
	struct mon_global *mg = restore_mg();

	while (edata < end)
		*edata++ = 0;
	/*
	 * NOTE: vbrsave MUST be in the
	 * data segment, NOT BSS, lest they get trashed by the bss zero
	 * above!
	 */
	vbrsave = vbr;
	for (i = 0, t = 0; i < 256; i++, t += 4) {

		/*
		 *  Don't touch trap13's (exit to monitor),
		 *  trap 0 & 1 (monitor global pointer)
		 *  or any interrupt vectors.
		 */
		if (t == T_STRAY || t == T_BADTRAP || t == T_MON_BOOT ||
		    (t >= T_LEVEL1 && t <= T_LEVEL7))
			scb[i] = vbr[i];
		else
			scb[i] = (int)_trap;
	}
#if 0
	printf("saved vbr 0x%x, new vbr 0x%x\n", vbrsave, scb);
	printf("monsp = 0x%x\n", _monsp);
#endif notdef
	slot_id_bmap = slot_id = mg->mg_sid;
	p_eprom = P_EPROM;
	bmap_chip = 0;
	scr1 = (struct scr1*) P_SCR1;
	switch (MACHINE_TYPE(scr1->s_cpu_rev)) {

	case NeXT_WARP9:
	case NeXT_X15:
	case NeXT_WARP9C:
		slot_id_bmap += 0x00100000;
		p_eprom = P_EPROM_BMAP;
		bmap_chip = (struct bmap_chip*) P_BMAP;
		break;
	}
	return((int)scb);
}

#define	PBUFSIZ		300

kdbg(initpc, initsp)
{
	char gbuf[PBUFSIZ];
	char pbuf[PBUFSIZ];
	char *gbp;
	char *pbp;
	int error;
	int i, j, k;

	if (!debugging)
		return;
	printf("Ready for remote debugging, initpc=0x%x, initsp=0x%x, cursp=0x%x\n", 
		initpc, initsp, getsp());
	client_pcb.sf.sf_pc = initpc;
	client_pcb.regs[R_SP] = initsp;
	client_pcb.sf.sf_ftype = 0;
	client_pcb.sf.sf_sr = 0x2700;
	for (;;) {
		i = getpkt(gbuf, gfunc, pfunc);
		if (i == 0)
			continue;
		gbuf[i] = 0;

#ifdef KDBG_DEBUG
		printf("kdbg recvd \"%s\"\n", gbuf);
#endif KDBG_DEBUG

		pbp = pbuf;
		gbp = gbuf;

		switch (*gbp++) {

		default:
			ERROR("unknown req\n");
			break;

		case 'r':	/* read register rNN */
			if (*htob(gbp, &i))
				ERROR("mangled r req\n");
			if (i >= NREGS+2) {
				error = EINVAL;
				goto error_reply;
			}
			switch (i) {
			default:
				pbp = btoh(client_pcb.regs[i], pbp);
				break;
			case NREGS:
				pbp = btoh(client_pcb.sf.sf_sr, pbp);
				break;
			case NREGS+1:
				pbp = btoh(client_pcb.sf.sf_pc, pbp);
				break;
			}
			break;
			
		case 'R':	/* write register RNN:XX..XX */
			if (*(gbp = htob(gbp, &i)) != ':')
				ERROR("mangled R req\n");
			if (i >= NREGS+2) {
				error = EINVAL;
				goto error_reply;
			}
			if (*htob(++gbp, &j))
				ERROR("mangled2 R req\n");
			switch (i) {
			default:
				client_pcb.regs[i] = j;
				break;
			case NREGS:
				client_pcb.sf.sf_sr = j;
				break;
			case NREGS+1:
				client_pcb.sf.sf_pc = j;
				break;
			}
			*pbp++ = '0';
			break;

		case 'm':	/* peek memory mAA..AA */
			if (*htob(gbp, &i))
				ERROR("mangled m req\n");
			error = rdmem(i, sizeof(int), &j);
			if (error)
				goto error_reply;
			pbp = btoh(j, pbp);
			break;

		case 'M':	/* poke memory MAA..AA:XX..XX */
			if (*(gbp = htob(gbp, &i)) != ':')
				ERROR("mangled M req\n");
			if (*htob(++gbp, &j))
				ERROR("mangled2 M req\n");
			error = wrmem(i, sizeof(int), j);
			if (error)
				goto error_reply;
			*pbp++ = '0';
			break;
			
		case 'c':	/* continue cAA..AA */
			if (*htob(gbp, &i))
				ERROR("mangled c req\n");
			if (i != 1)
				client_pcb.sf.sf_pc = i;
#ifdef KDBG_DEBUG
			printf("kdbg sending \"0\"\n");
#endif KDBG_DEBUG
			*pbuf = '0';
			putpkt(pbuf, 1, gfunc, pfunc, 1);
			i = kresume();
			*pbp++ = 'S';
			pbp = btoh(i, pbp);
			break;

		case 's':	/* step sAA..AA */
			if (*htob(gbp, &i))
				ERROR("mangled s req\n");
			if (i != 1)
				client_pcb.sf.sf_pc = i;
#ifdef KDBG_DEBUG
			printf("kdbg sending \"0\"\n");
#endif KDBG_DEBUG
			*pbuf = '0';
			putpkt(pbuf, 1, gfunc, pfunc, 1);
#ifdef KDBG_DEBUG
			printf("step, sr before=0x%x\n", client_pcb.sf.sf_sr);
#endif
			client_pcb.sf.sf_sr |= SR_T;
#ifdef KDBG_DEBUG
			printf("step, sr after=0x%x\n", client_pcb.sf.sf_sr);
#endif
			i = kresume();
			client_pcb.sf.sf_sr &= ~SR_T;
			*pbp++ = 'S';
			pbp = btoh(i, pbp);
			break;

		case 'k':	/* kill k */
			if (*gbp)
				ERROR("mangled k req\n");
			kquit();
			/* doesn't return */

		case 'g':	/* get registers g */
			if (*gbp)
				ERROR("mangled g req\n");
			for (i = 0; i < NREGS; i++) {
				pbp = btoh(client_pcb.regs[i], pbp);
				*pbp++ = ':';
			}
			pbp = btoh(client_pcb.sf.sf_sr, pbp);
			*pbp++ = ':';
			pbp = btoh(client_pcb.sf.sf_pc, pbp);
			break;

		case 'G':	/* set registers GR0:..:RN */
			for (i = 0; i < NREGS+2; i++) {
				switch (i) {
				default:
					gbp = htob(gbp, &client_pcb.regs[i]);
					break;
				case NREGS:
					gbp = htobs(gbp, &client_pcb.sf.sf_sr);
					break;
				case NREGS+1:
					gbp = htob(gbp, &client_pcb.sf.sf_pc);
					break;
				}
				if (*gbp != ':' && i != NREGS+2-1)
					ERROR("mangled G req\n");
				if (*gbp == ':')
					gbp++;
			}
			*pbp++ = '0';
			break;

		case 'b':	/* read memory bNN:AA..AA */
			if (*(gbp = htob(gbp, &i)) != ':' || i > 128)
				ERROR("mangled b req\n");
			if (*htob(++gbp, &j))
				ERROR("mangled2 b req\n");
			while (i-- > 0) {
				error = rdmem(j++, sizeof(char), &k);
				if (error)
					goto error_reply;
				pbp = btoh(k, pbp);
				*pbp++ = ':';
			}
			pbp--;
			break;

		case 'B':	/* write memory BAA..AA:X1:..:XN */
			if (*(gbp = htob(gbp, &i)) != ':')
				ERROR("mangled B req\n");
			error = 0;
			do {
				gbp = htob(++gbp, &j);
				error = wrmem(i++, sizeof(char), j);
				if (*gbp != ':' && *gbp)
					ERROR("mangled2 B req\n");
			} while (*gbp && !error);
			if (error)
				goto error_reply;
			*pbp++ = '0';
			break;

error_reply:
			/*
			 * error should be set to appropriate errno
			 * before jumping here
			 */
			*pbp++ = 'E';
			pbp = btoh(error, pbp);

		}
#ifdef KDBG_DEBUG
		pbuf[pbp-pbuf] = '\0';
		printf("kdbg sending \"%s\"\n", pbuf);
#endif KDBG_DEBUG
		putpkt(pbuf, pbp-pbuf, gfunc, pfunc, 1);
	}
}

/* #define EXCEP_DEBUG	1	/* */

trap(pcb)
register struct pcb *pcb;
{
	register int i;
	int *jbp;

	if (probe_nofault) {
		jbp = probe_nofault;
		probe_nofault = 0;
		longjmp(jbp, 1);
	}
	if (debugging)
		dbg_trap(pcb);
	printf("trap %d at pc 0x%x, sr: 0x%x\n", pcb->sf.sf_vecoff/4,
	    pcb->sf.sf_pc, pcb->sf.sf_sr);
	for (i = 0; i < 8; i++)
		printf("d%d:\t0x%x\ta%d:\t0x%x\n",
		    i, pcb->regs[i], i, pcb->regs[i+8]);
#ifdef	EXCEP_DEBUG
	while(1)
		;
#endif	EXCEP_DEBUG
	exit(1);
}

_exit()
{
#ifdef	EXCEP_DEBUG
	while(1)
		;
#endif	EXCEP_DEBUG
	setvbr(vbrsave);
	next_monstart();
	for (;;);
}

runit(pc)
{
	/* printf("transfering to 0x%x, vbrsave 0x%x\n", pc, vbrsave); */
	setvbr(vbrsave);
	_runit(pc);
}

static void
dbg_trap(pcb)
register struct pcb *pcb;
{
	int *jbp;
	int sig;
	int vec;

#ifdef KDBG_DEBUG
	printf("trap: running=%d, ftape=%d, vec=%d, sr=0x%x\n", client_running,
		pcb->sf.sf_ftype, pcb->sf.sf_vecoff/4, pcb->sf.sf_sr);
#endif
	if (client_running) {
		client_running = 0;
		client_pcb = *pcb;

		switch (pcb->sf.sf_ftype) {
		case 0:
		case 1:
			client_pcb.regs[R_SP] += sizeof(struct sf_4wd);
			break;
		case 2:
			client_pcb.regs[R_SP] += sizeof(struct sf_6wd);
			break;
		case 3:
			client_pcb.regs[R_SP] += sizeof(struct sf_float);
			break;
		case 7:
			client_pcb.regs[R_SP] += sizeof(struct sf_access);
			break;
		case 9:
			client_pcb.regs[R_SP] += sizeof(struct sf_10wd);
			break;
		case 10:
			client_pcb.regs[R_SP] += sizeof(struct sf_16wd);
			break;
		case 11:
			client_pcb.regs[R_SP] += sizeof(struct sf_46wd);
			break;
		default:
			printf("stack frame screw-up\n");
			exit(1);
		}

		/* SUN compiler bug */
		vec = client_pcb.sf.sf_vecoff;
		switch (vec/4) {
		case VEC_BUSERR:
		case VEC_ADDRERR:
			sig = SIGBUS;
			break;
		case VEC_ILLINST:
			sig = SIGILL;
			break;
		case VEC_BRKPT:
		case VEC_TRACE:
			sig = SIGTRAP;
			break;
		default:
			sig = SIGIOT;
			break;
		}
	} else
		sig = SIGIOT;
	if (nofault) {
		jbp = nofault;
		nofault = 0;
		longjmp(jbp, sig);
	}
	printf("debugger screw-up\n");
}

static char *
btoh(val, bp)
unsigned int val;
char *bp;
{
	char hbuf[10];
	int i;
	int nib;

	if (val == 0) {
		*bp++ = '0';
		return(bp);
	}
	for (i = 0; val; i++) {
		nib = val & 0xf;
		hbuf[i] = (nib < 10) ? (nib + '0') : (nib - 10 + 'a');
		val >>= 4;
	}
	for (i--; i >= 0; i--)
		*bp++ = hbuf[i];
	return(bp);
}

static char *
htobs(cp, sp)
char *cp;
unsigned short *sp;
{
	unsigned int val;
	int nib;

	val = 0;
	while (isxdigit(*cp)) {
		val <<= 4;
		nib = *cp++;
		if (isdigit(nib))
			val += nib - '0';
		else {
			if (isupper(nib))
				nib = tolower(nib);
			val += nib - 'a' + 10;
		}
	}
	*sp = val;
	return(cp);
}

static char *
htob(cp, ip)
char *cp;
unsigned int *ip;
{
	unsigned int val;
	int nib;

	val = 0;
	while (isxdigit(*cp)) {
		val <<= 4;
		nib = *cp++;
		if (isdigit(nib))
			val += nib - '0';
		else {
			if (isupper(nib))
				nib = tolower(nib);
			val += nib - 'a' + 10;
		}
	}
	*ip = val;
	return(cp);
}

static int
rdmem(addr, size, valp)
unsigned addr;
int size, *valp;
{
	jmpbuf jb;
	int error;

	if (error = setjmp(jb))
		return(error);
	nofault = jb;
	switch (size) {
	case sizeof(char):
		*valp = *(char *)addr;
		break;
	case sizeof(short):
		*valp = *(short *)addr;
		break;
	case sizeof(int):
		*valp = *(int *)addr;
		break;
	default:
		nofault = 0;
		printf("rdmem: bad size: 0x%x\n", size);
		exit(1);
	}
	nofault = 0;
	return(0);
}

static int
wrmem(addr, size, val)
unsigned addr;
int size, val;
{
	jmpbuf jb;
	int error;

	if (error = setjmp(jb))
		return(error);
	nofault = jb;
	switch (size) {
	case sizeof(char):
		*(char *)addr = val;
		break;
	case sizeof(short):
		*(short *)addr = val;
		break;
	case sizeof(int):
		*(int *)addr = val;
		break;
	default:
		printf("wrmem: bad size: 0x%x\n", size);
		nofault = 0;
		exit(1);
	}
	nofault = 0;
	return(0);
}

static int
kresume()
{
	int sig;
	struct pcb *pcb;
	struct sf_4wd *sfp;

	if (sig = setjmp(dbgjb))
		return(sig);
	client_pcb.regs[R_SP] -= sizeof(struct sf_4wd);
	sfp = (struct sf_4wd *)client_pcb.regs[R_SP];
	sfp->sf4_sr = client_pcb.sf.sf_sr;
	sfp->sf4_pc = client_pcb.sf.sf_pc;
	sfp->sf4_ftype = 0;
	nofault = dbgjb;
#ifdef KDBG_DEBUG
	printf("kresume: pc=0x%x, sr=0x%x\n", sfp->sf4_pc, sfp->sf4_sr);
#endif
	_kresume();
	/* doesn't return */
}

static void
kquit()
{
	exit(0);
}

static void
putnib(putf, nib)
void (*putf)();
int nib;
{
	if (nib < 10)
		(*putf)('0'+nib);
	else
		(*putf)('a'+nib-10);
}

static void
putpkt(buf, cnt, getf, putf, waitack)
char *buf;
int cnt;
int (*getf)();
void (*putf)();
{
	int i;
	char csum = 0;

	do {
		(*putf)('$');
		for (i = 0; i < cnt; i++) {
			csum += buf[i];
			(*putf)(buf[i]);
		}
		(*putf)('#');
		putnib(putf, (csum >> 4) & 0xf);
		putnib(putf, csum & 0xf);
	} while (waitack && !ack(getf));
}

static int
ack(getf)
int (*getf)();
{
	char c;

	do {
		c = (*getf)();
	} while (c != '+' && c != '-');
	return(c == '+');
}

static int
getnib(getf)
int (*getf)();
{
	char c;

	c = (*getf)();
	if (!isxdigit(c))
		return(-1);
	if (isdigit(c))
		return(c-'0');
	return(c-'a'+10);
}

static int
getpkt(buf, getf, putf)
char *buf;
int (*getf)();
void (*putf)();
{
	char *bp;
	char csum = 0;
	char sentsum;
	char c;

	for (;;) {
		while ((*getf)() != '$')
			continue;
		bp = buf;
		while ((c = (*getf)()) != '#') {
			*bp++ = c;
			csum += c;
		}
		sentsum = getnib(getf) << 4;
		sentsum |= getnib(getf);
		if (sentsum == csum)
			break;
		printf("bad csum\n");
		(*putf)('-');
	}
	(*putf)('+');
	return(bp-buf);
}

/*
 * Routines which implement debugger i/o
 */
static int
gfunc()
{
	return(scc_getc(0));
}

static void
pfunc(c)
{
	scc_putc(0, c);
}

probe_rb(addr)
volatile char *addr;
{
	jmpbuf jb;
	int error;
	int junk;

	if (setjmp(jb))
		return(0);
	probe_nofault = jb;
	junk = *addr;
	probe_nofault = 0;
	return(1);
}







