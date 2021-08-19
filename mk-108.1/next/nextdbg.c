/*
 * Copyright (c) 1990, NeXT Inc.
 *
 * HISTORY
 *  2-Mar-90  Gregg Kellogg (gk) at NeXTE
 *	Clean up #import syntax.
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	General cleanup of Ethernet-based debugging routines.
 */
 
#pragma CC_OPT_OFF

#ifdef gcc
#define	void	int
#endif gcc

#import <sys/types.h>
#import <sys/param.h>
#import <sys/reboot.h>
#import <sys/systm.h>
#import <sys/conf.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <next/pcb.h>
#import <next/psl.h>
#import <next/trap.h>
#import <next/cons.h>
#import <next/mmu.h>
#import <next/cpu.h>
#import <next/eventc.h>
#import <vm/vm_param.h>
#import <vm/vm_map.h>
#import <sys/dir.h>		/* for user.h (!) */
#import <sys/user.h>		/* for u.u_procp  */
#import <sys/proc.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <sys/signal.h>
#import <sys/errno.h>
#import <next/nextdbg.h>

#define NO_IPLMEAS	1	/* Don't measure, we're recursive */
#import <machine/spl.h>

char *kern_stext, *kern_etext, *kern_sdata, *kern_edata;

#define	isxdigit(c)	(((c) >= '0' && (c) <= '9') \
			 || ((c) >= 'A' && (c) <='F') \
			 || ((c) >= 'a' && (c) <= 'f'))

#define isdigit(c)	((c) >= '0' && (c) <= '9')

#define	isupper(c)	((c) >= 'A' && (c) <= 'Z')

#define	tolower(c)	((c) & ~0x20)

#define	VEC_BUSERR	2
#define	VEC_ADDRERR	3
#define	VEC_ILLINST	4
#define	VEC_TRACE	9
#define	VEC_BRKPT	46	/* trap #14 */
#define ERROR(s) panic(s)

#define	R_D0		0
#define	R_D1		1
#define	R_D2		2
#define	R_D3		3
#define	R_D4		4
#define	R_D5		5
#define	R_D6		6
#define	R_D7		7

#define	R_A0		8
#define	R_A1		9
#define	R_A2		10
#define	R_A3		11
#define	R_A4		12
#define	R_A5		13
#define	R_A6		14
#define	R_A7		15
#undef	R_SP
#define	R_SP		R_A7

#define	NREGS		16

#define	SR_T		0x8000

/*
 * 0 - 15 regs, 16 pc
 */
typedef int jmpbuf[17];
static jmpbuf dbgjb;
typedef	unsigned unlong;
int rmtdebug;

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
};

#define	sf_sr		sf_4wd.sf4_sr
#define	sf_pc		sf_4wd.sf4_pc
#define	sf_ftype	sf_4wd.sf4_ftype
#define	sf_vecoff	sf_4wd.sf4_vecoff

struct nextdbgpcb {
	unsigned usp;
	unsigned regs[16];
	short zero;
	union stk_frms sf;
};

struct nextdbgpcb client_pcb;
static int *nofault;
int client_running;
#define DBG_BUSERR	2
#define DBG_ADRERR	3
#define DBG_TRACE	9
#define DBG_BKPT	46	/* trap #14 */

static void hookbuserr(), unhookbuserr();

static int *oldbuserr, *oldadrerr, *oldtrace, *oldbkpt;
static int trace_expected;
#if	0
extern int gdb;
#endif	0

nextdbginit()
{
	int *vbp;
	int _dbg_trap();

	/* 
	 * Grab the vectors which cause us to enter the debugger:
	 *	Trace trap, breakpoint, bus error, address error.
	 *
	 * Bus error and address error are held only while under debugger
	 * control.
	 *
	 * Next initialize monitor IO.
	 */
	vbp = (int *)get_vbr();		/* Get the vector base register */
	oldbkpt = (int *)vbp[DBG_BKPT];
	vbp[DBG_BKPT] = (int)_dbg_trap;
	dbug_tp = &dbug;
	dbug.t_dev = makedev (11, 0);		/* zs */
	kdbg();
}

dbg_exit(code)
{
	printf("exit(%d) called\n", code);
	while (1)
		;
}

int *
dbg_trap(pcb)
register struct nextdbgpcb *pcb;
{
	int *jbp;
	int sig;
	int *vbp;
	int _dbg_trap();

	vbp = (int *)get_vbr();		/* Get the vector base register */
	if (rmtdebug)
		printf("dbg_trap:  client_running=0x%x, sr=0x%x, pc=0x%x, vecoff=%d, nofault=%d\n",
			client_running, pcb->sf.sf_sr, pcb->sf.sf_pc,
			pcb->sf.sf_vecoff>>2, nofault);
	if (client_running) {
		/*
		 * Decide if this was for the debugger, if so, snag
		 * buserr and adrerr.  Otherwise, return the old 
		 * exception vector and let _dbg_trap reroute the
		 * exception.
		 */
		if ((pcb->sf.sf_sr & 0x2000) == 0) {
			/* Return old exception pointer */
printf("dbg_trap from user mode\n");
			switch(pcb->sf.sf_vecoff>>2) {
			case VEC_BRKPT:
				return(oldbkpt);
				break;
			case VEC_TRACE:
				return(oldtrace);
				break;
			default:
				printf("dbg_trap returning, vecoff = %d\n",
					pcb->sf.sf_vecoff);
				panic("dbg_trap: bad user vecoff");
			}
		}

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

		switch (client_pcb.sf.sf_vecoff>>2) {
		case VEC_BUSERR:
		case VEC_ADDRERR:
			sig = SIGBUS;
			break;
		case VEC_ILLINST:
			sig = SIGILL;
			break;
		case VEC_TRACE:
			if (nofault && trace_expected) {
				int *vbp;

				vbp = (int *)get_vbr();
				vbp[DBG_TRACE] = (int)oldtrace;
				client_pcb.sf.sf_sr &= ~SR_T;
				trace_expected = 0;
			}
			/* fall through */
		case VEC_BRKPT:
			sig = SIGTRAP;
			break;
		default:
			sig = SIGIOT;
			break;
		}
	} else
		sig = SIGIOT;
	if (nofault) {
		if (rmtdebug)
			printf("assigning nofault=0x%x to jbp\n", nofault);
		jbp = nofault;
		if (rmtdebug)
			printf("assigning jbp=0x%x\n", jbp);
		nofault = 0;
		spl7();
		if (rmtdebug) {
			printf("dbg_trap:  nofault dbg_longjmp, SIGIOT, jbp=0x%x.\n", jbp);
			printf("pc=0x%x, a7=0x%x\n", jbp[16], jbp[15]);
		}
		dbg_longjmp(jbp, sig);

	}
	printf("debugger screw-up\n");
}

extern int gfunc();
extern void pfunc();
extern int getpkt();
extern void putpkt();

char *
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

char *
htobs(cp, sp)
char *cp;
short *sp;
{
	int val;
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

char *
htob(cp, ip)
char *cp;
int *ip;
{
	int val;
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

#define	GETPKT(x, y)	getpkt((x), gfunc, pfunc, (y))
#define	PUTPKT(x, y, z)	putpkt((x), (y), gfunc, pfunc, (z))

#define	CUR_SEQ		-1	/* use current sequence number */
#define	NO_TIMEOUT	0
#define	ACK_TIMEOUT	50000
#define	NO_BLOCK	0
#define	BLOCK		1

struct kdb_net kdb_net;

kdbg()
{
	char gbuf[PBUFSIZ];
	char pbuf[PBUFSIZ];
	char *gbp;
	char *pbp;
	int *vbp;
	int error;
	int i, j, k;
	int *dbg_trap();
	int _dbg_trap();
	struct kdb_net *n = &kdb_net;

	printf("Remote debugging enabled\n");
	i = dbg_kresume();
	if (rmtdebug) printf("connect to debugger\n");
	pbp = pbuf;
	*pbp++ = 'S';
	pbp = btoh(i, pbp);
	pbuf[pbp-pbuf] = '\0';
	if (rmtdebug) printf("kdbg sending \"%s\"\n", pbuf);
	PUTPKT(pbuf, pbp-pbuf, 1);
	for (;;) {
		i = GETPKT(gbuf, BLOCK);
		if (i == 0)
			continue;
		gbuf[i] = 0;

		if (rmtdebug) printf("kdbg recvd \"%s\"\n", gbuf);

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
			if (rmtdebug)
				printf("kdbg sending \"0\"\n");
			*pbuf = '0';
			PUTPKT(pbuf, 1, 1);
			i = dbg_kresume();
			*pbp++ = 'S';
			pbp = btoh(i, pbp);
			break;

		case 's':	/* step sAA..AA */
			if (*htob(gbp, &i))
				ERROR("mangled s req\n");
			if (i != 1)
				client_pcb.sf.sf_pc = i;
			if (rmtdebug)
				printf("kdbg sending \"0\"\n");
			*pbuf = '0';
			PUTPKT(pbuf, 1, 1);
			client_pcb.sf.sf_sr |= SR_T;
			vbp = (int *)get_vbr();
			oldtrace = (int *)vbp[DBG_TRACE];
			vbp[DBG_TRACE] = (int)_dbg_trap;
			trace_expected = 1;
			i = dbg_kresume();
#ifdef notdef
			/* Now handled in dbg_trap */
			vbp[DBG_TRACE] = (int)oldtrace;
			client_pcb.sf.sf_sr &= ~SR_T;
#endif
			*pbp++ = 'S';
			pbp = btoh(i, pbp);
			break;

		case 'k':	/* kill k */
			if (*gbp)
				ERROR("mangled k req\n");
			dbg_exit();
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

		case 'z':
			if (rmtdebug)
				rmtdebug = 0;
			else
				rmtdebug = 1;
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
		pbuf[pbp-pbuf] = '\0';
		if (rmtdebug)
			printf("kdbg sending \"%s\"\n", pbuf);
		if (pbp != pbuf)
			PUTPKT(pbuf, pbp-pbuf, 1);
	}
}

rdmem(addr, size, valp)
unsigned addr;
int size, *valp;
{
	int error;

	if (error = dbg_setjmp(dbgjb)) {
		if (rmtdebug)
			printf("rdmem: error.\n");
		unhookbuserr();
		return(error);
	}
	nofault = dbgjb;
	hookbuserr();
	switch (size) {
	case sizeof(char):
		*valp = *(char *)addr;
		*valp &= 0xff;
		break;
	case sizeof(short):
		*valp = *(short *)addr;
		*valp &= 0xffff;
		break;
	case sizeof(int):
		*valp = *(int *)addr;
		break;
	default:
		nofault = 0;
		printf("rdmem: bad size: 0x%x\n", size);
		exit(1);
	}
	unhookbuserr();
	nofault = 0;
	return(0);
}

wrmem(addr, size, val)
unsigned addr;
int size, val;
{
	int error;

	if (error = dbg_setjmp(dbgjb)) {
		if (rmtdebug)
			printf("wrmem: error.\n");
		unhookbuserr();
		return(error);
	}
	nofault = dbgjb;
	hookbuserr();

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
	cache_push_page(addr);		/* invalidate instruction cache on 040 */
	nofault = 0;
	unhookbuserr();
	return(0);
}

static void
hookbuserr()
{
	int *vbp;
	int _dbg_trap();

	vbp = (int *)get_vbr();		/* Get the vector base register */
	oldbuserr = (int *)vbp[DBG_BUSERR];
	oldadrerr = (int *)vbp[DBG_ADRERR];
	vbp[DBG_BUSERR] = (int) _dbg_trap;
	vbp[DBG_ADRERR] = (int) _dbg_trap;
}

static void
unhookbuserr()
{
	int *vbp;

	vbp = (int *)get_vbr();		/* Get the vector base register */
	vbp[DBG_BUSERR] = (int)oldbuserr;
	vbp[DBG_ADRERR] = (int)oldadrerr;
}


dbg_kresume()
{
	int sig;
	struct nextdbgpcb *pcb;
	struct sf_4wd *sfp;

	if (sig = dbg_setjmp(dbgjb))
		return(sig);
	client_pcb.regs[R_SP] -= sizeof(struct sf_4wd);
	sfp = (struct sf_4wd *)client_pcb.regs[R_SP];
	sfp->sf4_sr = client_pcb.sf.sf_sr;
	sfp->sf4_pc = client_pcb.sf.sf_pc;
	sfp->sf4_ftype = 0;
	nofault = dbgjb;
	_dbg_kresume();
	/* doesn't return */
}

kdbg_connect()
{
	struct kdb_net *n = &kdb_net;
	char buf[PBUFSIZ], *cp;

	if (n->locked)
		return(0);
	n->rp = &n->rpkt;
	n->tp = &n->tpkt;
	n->dev = NET;
	if (GETPKT(buf, NO_BLOCK) && buf[0] == 'S') {
		cp = (char*) index(buf, ':');
		*cp = 0;
		printf("\ndebugging connection from %s on %s\n",
			&buf[1], cp+1);
		n->locked = 1;
		asm ("trap #14");
		return (1);
	}
	n->dev = SERIAL;
	return (0);
}

void
putnib(putf, nib)
void (*putf)();
int nib;
{
	if (nib < 10)
		(*putf)('0'+nib);
	else
		(*putf)('a'+nib-10);
}

void
putpkt(buf, cnt, getf, putf, waitack)
char *buf;
int cnt;
int (*getf)();
void (*putf)();
{
	int i;
	char csum;
	struct kdb_net *n = &kdb_net;

	do {
		(*putf)('$');
		csum = 0;
		for (i = 0; i < cnt; i++) {
			csum += buf[i];
			(*putf)(buf[i]);
		}
		(*putf)('#');
		putnib(putf, (csum >> 4) & 0xf);
		putnib(putf, csum & 0xf);
		kdebug_send(CUR_SEQ);
	} while (waitack && !ack(getf));
	n->seq++;
}

int
ack(getf)
int (*getf)();
{
	char c;
	struct kdb_net *n = &kdb_net;

	do {
		c = (*getf)(ACK_TIMEOUT, BLOCK);
		if (c != '+' && c != '-')
			n->rlen = 0;
	} while (c != '+' && c != '-');
	n->rlen = 0;
	return(c == '+');
}

int
getnib(getf)
int (*getf)();
{
	char c;

	c = (*getf)(NO_TIMEOUT, BLOCK);
	if (!isxdigit(c))
		return(-1);
	if (isdigit(c))
		return(c-'0');
	return(c-'a'+10);
}

int
getpkt(buf, getf, putf, block)
char *buf;
int (*getf)();
void (*putf)();
{
	char *bp;
	char csum;
	char sentsum;
	int c;
	struct kdb_net *n = &kdb_net;

	for (;;) {
		do {
			c = (*getf)(NO_TIMEOUT, block);
			if (!block && c == -1)
				return (0);
			if (c != '$')
				n->rlen = 0;
		} while (c != '$');
		bp = buf;
		csum = 0;
		while ((c = (*getf)(NO_TIMEOUT, 1)) != '#') {
			*bp++ = c;
			csum += c;
		}
		sentsum = getnib(getf) << 4;
		sentsum |= getnib(getf);
		if (sentsum == csum)
			break;
		printf("bad csum\n");
		(*putf)('-');
		kdebug_send(CUR_SEQ);
	}
	(*putf)('+');
	kdebug_send(CUR_SEQ);
	n->seq++;
	return(bp-buf);
}

kdebug_send(seq)
{
	struct kdb_net *n = &kdb_net;
	int len;

	if (n->dev != NET || n->ti == 0)
		return;
		
	n->tp->seq = seq == -1? n->seq : seq;
	en_send(n->tp, n->ti + HDRSIZE);
	n->ti = 0;
	n->rlen = 0;
}

int
gfunc(timeout, block)
{
	dev_t device = dbug_tp->t_dev;
	int c, time;
	struct kdb_net *n = &kdb_net;

	switch (n->dev) {
	
	case SERIAL:
		return ((*cdevsw[major(device)].d_getc)(device));
		
	case NET:
		if (n->rlen == 0) {
		
			/*
			 *  Can't use event_get() to measure timeout interval
			 *  because it requires that interrupts are enabled.
			 */
			time = 0;
bad_seq:		do {
				n->rlen = en_recv(n->rp, sizeof (struct rpkt),
					n->locked);
				if (!block && n->rlen == 0)
					return (-1);
			} while (n->rlen == 0 &&
			    (!timeout || time++ < timeout));
			if (n->rlen == 0)
				return ('-');

			/* positive ack delayed duplicates */
			if (n->rp->seq != n->seq) {
				if (n->rp->buf[0] != '+' && n->rp->buf[0] != '-' &&
				    n->rp->seq < n->seq) {
					pfunc('+');
					kdebug_send(n->rp->seq);
				}
				goto bad_seq;
			}
			n->rlen -= HDRSIZE;
			n->ri = 0;
		}
		if (n->ri >= n->rlen)
			c = '#';
		else
			c = n->rp->buf[n->ri++];
		return (c);
	}
}

void
pfunc(c)
{
	dev_t device = dbug_tp->t_dev;
	struct kdb_net *n = &kdb_net;

	switch (n->dev) {
	
	case SERIAL:
		(*cdevsw[major(device)].d_putc)(device, c & 0x7f);
		break;

	case NET:
		n->tp->buf[n->ti++] = c;
		break;
	}
}


