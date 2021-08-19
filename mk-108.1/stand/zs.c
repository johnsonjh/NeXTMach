#import <next/cpu.h>
#import <stand/zsreg.h>

#define	ZSADDR_A	((struct zsdevice *)P_SCC+1)
#define	ZSADDR_B	((struct zsdevice *)P_SCC)

#define	ZS_CONSOLE	((struct zsdevice *)ZSADDR_A)
#define	ZS_DEBUG	((struct zsdevice *)ZSADDR_B)

#ifndef CTRL
#define	CTRL(s)		((s)&0x1f)
#endif

static int zs_stop;
static int zs_initialized;

static char parity[] = {
	0000,0200,0200,0000,0200,0000,0000,0200,	/* 000 - 007 */
	0200,0000,0000,0200,0000,0200,0200,0000,	/* 010 - 017 */
	0200,0000,0000,0200,0000,0200,0200,0000,	/* 020 - 027 */
	0000,0200,0200,0000,0200,0000,0000,0200,	/* 030 - 037 */
	0200,0000,0000,0200,0000,0200,0200,0000,	/* 040 - 047 */
	0000,0200,0200,0000,0200,0000,0000,0200,	/* 050 - 057 */
	0000,0200,0200,0000,0200,0000,0000,0200,	/* 060 - 067 */
	0200,0000,0000,0200,0000,0200,0200,0000,	/* 070 - 077 */
	0200,0000,0000,0200,0000,0200,0200,0000,	/* 100 - 107 */
	0000,0200,0200,0000,0200,0000,0000,0200,	/* 110 - 117 */
	0000,0200,0200,0000,0200,0000,0000,0200,	/* 120 - 127 */
	0200,0000,0000,0200,0000,0200,0200,0000,	/* 130 - 137 */
	0000,0200,0200,0000,0200,0000,0000,0200,	/* 140 - 147 */
	0200,0000,0000,0200,0000,0200,0200,0000,	/* 150 - 157 */
	0200,0000,0000,0200,0000,0200,0200,0000,	/* 160 - 167 */
	0000,0200,0200,0000,0200,0000,0000,0200		/* 170 - 177 */
};

static void zs_init();
static void zschan_init();
static int zs_tc();
static int zs_getc();
static void zs_putc();
static int zs_ischar();
static void zs_scan();

static void
zs_init()
{
	ZSWRITE_A(9, WR9_RESETHARD);
	DELAY(10);
	zschan_init(ZSADDR_A, 9600);
	zschan_init(ZSADDR_B, 9600);
	zs_stop = 0;
}

static void
zschan_init(zsaddr, speed)
register struct zsdevice *zsaddr;
{
	int tmp;

	ZSWRITE(zsaddr, 9, WR9_NV);
	ZSWRITE(zsaddr, 11, WR11_TXCLKBRGEN|WR11_RXCLKBRGEN);
	zsaddr->zs_ctrl = WR0_RESET;
	zsaddr->zs_ctrl = WR0_RESET_STAT;
	ZSWRITE(zsaddr, 4, WR4_X16CLOCK|WR4_STOP1);
	ZSWRITE(zsaddr, 3, WR3_RX8);
	ZSWRITE(zsaddr, 5, WR5_TX8);
	tmp = zs_tc(speed, 16);
	ZSWRITE(zsaddr, 12, tmp);
	ZSWRITE(zsaddr, 13, tmp>>8);
	ZSWRITE(zsaddr, 14, tmp>>16);
	DELAY(10);
	ZSWRITE(zsaddr, 14, WR14_BRENABLE|(tmp>>16));
	ZSWRITE(zsaddr, 3, WR3_RX8|WR3_RXENABLE);
	ZSWRITE(zsaddr, 5, WR5_TX8|WR5_RTS|WR5_DTR|WR5_TXENABLE);
}

static int zsfreq[] = { PCLK_HZ, RTXC_HZ };

/*
 * NOTE: FIXSCALE * max_clk_hz must be < 2^31
 */
#define	FIXSCALE	256

static int
zs_tc(baudrate, clkx)
{
	int i, br, f;
	int rem[2], tc[2];

	/*
	 * This routine takes the requested baudrate and calculates
	 * the proper clock input and time constant which will program
	 * the baudrate generator with the minimum error
	 *
	 * BUG: doesn't handle 134.5 -- so what!
	 */
	for (i = 0; i < 2; i++) {
		tc[i] = ((zsfreq[i]+(baudrate*clkx))/(2*baudrate*clkx)) - 2;
		/* kinda fixpt calculation */
		br = (FIXSCALE*zsfreq[i]) / (2*(tc[i]+2)*clkx);
		rem[i] = abs(br - (FIXSCALE*baudrate));
	}
#if defined(NeXT)
	if (rem[0] < rem[1])	/* PCLK is better clock choice */
		return((tc[0] & 0xffff) | (WR14_BRPCLK<<16));
#endif
	return(tc[1] & 0xffff);	/* RTxC is better clock choice */
}

static int
zs_getc(zsaddr)
register struct zsdevice *zsaddr;
{
	while ((zsaddr->zs_ctrl & RR0_RXAVAIL) == 0)
		continue;
	return(zsaddr->zs_data);
}

static void
zs_putc(zsaddr, c)
register struct zsdevice *zsaddr;
{
	while ((zsaddr->zs_ctrl & RR0_TXEMPTY) == 0)
		continue;
	zsaddr->zs_data = c;
}

static int
zs_ischar(zsaddr)
register struct zsdevice *zsaddr;
{
	return(zsaddr->zs_ctrl & RR0_RXAVAIL);
}

static void
zs_scan(zsaddr)
register struct zsdevice *zsaddr;
{
	if (zs_ischar(zsaddr)) {
		switch (zs_getc(zsaddr) & 0x7f) {
		case CTRL('S'):
			zs_stop = 1;
			break;
		case CTRL('Q'):
		default:
			zs_stop = 0;
			break;
		}
	}
}

int
scc_getc(chan)
int chan;
{
	int c;

	if (! zs_initialized) {
		zs_init();
		zs_initialized++;
	}
	do {
		/*
		 * clear parity bit
		 */
		c = zs_getc(chan ? ZSADDR_B : ZSADDR_A) & 0x7f;
	} while (c == CTRL('S') || c == CTRL('Q'));
	return(c);
}

void
scc_putc(chan, c)
{
	register struct zsdevice *zsaddr = (chan ? ZSADDR_B : ZSADDR_A);

	if (! zs_initialized) {
		zs_init();
		zs_initialized++;
	}
	do {
		zs_scan(zsaddr);
	} while (zs_stop);
	/*
	 * sets parity bit to even parity
	 */
	c |= parity[ c & 0x7f ];
	zs_putc(zsaddr, c);
}
