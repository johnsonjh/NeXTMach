/*	@(#)clock.c	1.0	1/2/87		(c) 1987 NeXT	*/

/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 14-Jun-90  John Seamons (jks) at NeXT
 *	In inittodr() disallow base time being < 0.  This causes an NFS root mount
 *	to fail.
 *
 * 15-Mar-90  John Seamons (jks) at NeXT
 *	Added support for new clock chip.  Remove obsolete routine stoprtclock().
 *
 * 07-Jun-89  Mike DeMoney (mike) at NeXT
 *	Changed us_time_init to us_timer_init.  Modified RTC read and
 *	write routines to use block mode transfers.  Minor tweaks for
 *	new us_timer code.
 *
 * 22-Dec-87  Gregg Kellogg (gk) at NeXT
 *	Added support for microsecond timer and counter.
 *
 * 02-Jan-87  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)clock.c	7.1 (Berkeley) 6/5/86
 */

#import <sys/param.h>
#import <sys/time.h>
#import <sys/kernel.h>
#import <sys/errno.h>
#import <vm/vm_kern.h>
#import <next/clock.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/eventc.h>
#import <nextdev/video.h>
#import <mon/nvram.h>
#import	<mon/global.h>

#import <machine/spl.h>
#import <kern/xpr.h>

int	new_clock_chip;

/*
 * Machine-dependent clock routines.
 *
 * Startrtclock restarts the process scheduling clock, which provides
 * hardclock interrupts to kern_clock.c.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.  Its primary function is to use some file
 * system information in case the hardare clock lost state.
 *
 * Resettodr restores the time of day hardware after a time change.
 */

rtc_set_clr (addr, val, mode)
	u_char addr, val;
{
	rtc_write (addr, (rtc_read (addr) & ~val) | (val & mode));
}

rtc_set_auto_poweron (auto_pon)
{
#if	NCC
	printf ("rtc_set_auto_pon: %d\n", auto_pon);
#endif	NCC
	if (new_clock_chip)
		rtc_set_clr (RTC_CONTROL, RTC_AUTO_PON, auto_pon? RTC_SET : RTC_CLR);
}

rtc_intr()
{
	extern struct mon_global *mon_global;
	struct mon_global *mg = mon_global;
	int p_down = 0, status;
	
	while (*mg->mg_intrstat & I_BIT(I_POWER)) {
		if ((status = rtc_read (RTC_STATUS)) & RTC_NEW_CLOCK) {
#if	NCC
			printf ("rtc_intr: status 0x%x\n", status);
#endif	NCC
			if (status & RTC_FTU)
				rtc_set_clr (RTC_CONTROL, RTC_FTUC, RTC_SET);
			if (status & RTC_LOW_BATT)
				rtc_set_clr (RTC_CONTROL, RTC_LBE, RTC_CLR);
			if (status & RTC_ALARM)
				rtc_set_clr (RTC_CONTROL, RTC_AC, RTC_SET);
			if (status & RTC_RPD) {
				rtc_set_clr (RTC_CONTROL, RTC_RPDC, RTC_SET);
				p_down = 1;
			}
		} else {
			return (1);
		}
	}
	return (p_down);
}

rtc_alarm (set, get)
	struct alarm *set, *get;
{
	struct rtc_clk_map_new rcm_new;

	if (!new_clock_chip)
		return (EINVAL);
	if (get) {
		get->alarm_enabled = rtc_read (RTC_CONTROL) & RTC_AE;
		rtc_blkread (RTC_ALARM0, &rcm_new, sizeof (rcm_new));
		get->alarm_time = (rcm_new.cntr0 << 24) |
			(rcm_new.cntr1 << 16) | (rcm_new.cntr2 << 8) | rcm_new.cntr3;
#if	NCC
		printf ("rtc_alarm (get): tv_sec 0x%x\n", get->alarm_time);
#endif	NCC
	}
	if (set) {
#if	NCC
		printf ("rtc_alarm (set): tv_sec 0x%x\n", set->alarm_time);
#endif	NCC
		rcm_new.cntr3 = set->alarm_time & 0xff;
		rcm_new.cntr2 = (set->alarm_time >> 8) & 0xff;
		rcm_new.cntr1 = (set->alarm_time >> 16) & 0xff;
		rcm_new.cntr0 = (set->alarm_time >> 24) & 0xff;
		rtc_blkwrite (RTC_ALARM0, &rcm_new, sizeof (rcm_new));
		rtc_set_clr (RTC_CONTROL, RTC_AE,
			set->alarm_enabled? RTC_SET : RTC_CLR);
	}
	return (0);
}

rtc_power_down()
{
	int addr, s;
	
	s = spldma();
#if	NCC
	printf ("rtc_power_down\n");
#endif	NCC
	addr = (rtc_read (RTC_STATUS) & RTC_NEW_CLOCK)? RTC_CONTROL : RTC_INTRCTL;
	rtc_set_clr (addr, RTC_PDOWN, RTC_SET);
	while (1)
		;
	/* NOT_REACHED */
}

/*
 * Start the real time clock (RTC) and
 * process scheduling clock (system timer & event counter).
 */
startrtclock()
{
	struct nvram_info ni;
	
	nvram_check (&ni);
	new_clock_chip = rtc_read (RTC_STATUS) & RTC_NEW_CLOCK;
#if	NCC
	printf ("NCC = %d, ni.auto_pon %d\n", new_clock_chip, ni.ni_auto_poweron);
#endif	NCC
	if (new_clock_chip) {
		rtc_set_clr (RTC_CONTROL, RTC_AUTO_PON,
			ni.ni_auto_poweron? RTC_SET : RTC_CLR);
		rtc_set_clr (RTC_CONTROL, RTC_START, RTC_SET);
	}
	
	/* make clock chip version externally visible */
	ni.ni_new_clock_chip = new_clock_chip? 1 : 0;
	nvram_set (&ni);
	us_timer_init();	/* start up the usec timer and counter */
}

/*
 * Initialze the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
inittodr(base)
	time_t base;
{
	long deltat;
	struct timeval rtc_get();

	if (base < (87-YRREF) * SECYR || base < 0) {	/* fs younger than 1987? */
		printf("WARNING: preposterous time in file system");
		time.tv_sec = (87-YRREF)*SECYR + 186*SECDAY + SECDAY/2;
		resettodr();
		goto check;
	}

	time = rtc_get();
	if (time.tv_sec < SECYR) {
		printf ("WARNING: clock not set properly");
		time.tv_sec = base;
		resettodr();
		goto check;
	}

	/*
	 * See if we gained/lost two or more days;
	 * if so, assume something is amiss.
	 */
	deltat = time.tv_sec - base;
	if (deltat < 0)
		deltat = -deltat;
	if (deltat < 2*SECDAY)
		return;
	if (deltat > 90*SECDAY) {	/* assume rtc is way off */
		printf ("WARNING: clock not set properly");
		time.tv_sec = base;
		resettodr();
		goto check;
	}
	printf("WARNING: clock %s %d days",
	    time.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
check:
	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * resynctime is only called after an nmi to resync the time
 * It is called at splhigh()
 */
void
resynctime()
{
	struct timeval rtc_get();

	time = rtc_get();
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.
 */
resettodr()
{
	rtc_set (&time);
}

short dayyr[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, };

tm_to_sec (tm)
	struct tm	*tm;
{
	register int	yr, days;

	for (yr = YRREF, days = 0; yr < tm->tm_year; yr++) {
		days += 365;
		if (LEAPYEAR(yr))
			days++;
	}
	days += (dayyr[tm->tm_mon-1]+tm->tm_mday-1);
	if (LEAPYEAR(tm->tm_year) && tm->tm_mon > 2)
		days++;
	return (days*SECDAY + tm->tm_hour*HRSEC + tm->tm_min*MINSEC +
		tm->tm_sec);
}

static	short	dmsize[12] = { 31, 28, 31, 30, 31, 30,
			       31, 31, 30, 31, 30, 31 };
static	short	ly_dmsize[12] =
			     { 31, 29, 31, 30, 31, 30,
			       31, 31, 30, 31, 30, 31 };

#define	dysize(year)	((year)%4 == 0 ? 366 : 365)

sec_to_tm (sec, tm)
	int	sec;
	struct tm	*tm;
{
	register int	hms, day, d0, d1;
	register short	*dmtab;

	hms = sec % 86400;
	day = sec / 86400;
	if (hms < 0) {
		hms += 86400;
		day -= 1;
	}

	/*
	 *	generate seconds/minutes/hours
	 */
	tm->tm_sec = hms % 60;
	d1         = hms / 60;
	tm->tm_min = d1  % 60;
	tm->tm_hour = d1 / 60;

	/*
	 *	year number
	 */
	if (day >= 0) {
		for (d1 = YRREF; day >= dysize(d1); d1++)
			day -= dysize(d1);
	}
	else {
		for (d1 = YRREF; day < 0; d1--)
			day += dysize(d1-1);
	}
	tm->tm_year = d1;

	/*
	 *	generate month.  'day' is day-in-year, from 0.
	 */
	if (dysize(d1) == 366)
		dmtab = ly_dmsize;
	else
		dmtab = dmsize;
	for (d1 = 0; day >= dmtab[d1]; d1++)
		day -= dmtab[d1];
	tm->tm_mon = d1+1;
	tm->tm_mday = day + 1;
}

rtc_set (tp)
	struct timeval *tp;
{
	struct tm tm;
	struct rtc_clk_map rcm;
	struct rtc_clk_map_new rcm_new;

	if (new_clock_chip) {
		rcm_new.cntr3 = tp->tv_sec & 0xff;
		rcm_new.cntr2 = (tp->tv_sec >> 8) & 0xff;
		rcm_new.cntr1 = (tp->tv_sec >> 16) & 0xff;
		rcm_new.cntr0 = (tp->tv_sec >> 24) & 0xff;
#if	NCC
		printf ("rtc_set: tv_sec 0x%x\n", tp->tv_sec);
#endif	NCC

		/* stop rtc update while we write */
		rtc_set_clr (RTC_CONTROL, RTC_START, RTC_CLR);
		rtc_blkwrite(RTC_CNTR0, &rcm_new, sizeof(rcm_new));
		rtc_set_clr (RTC_CONTROL, RTC_START, RTC_SET);
	} else {
		sec_to_tm (tp->tv_sec, &tm);
		bzero(&rcm, sizeof(rcm));
		rcm.sec = byte_to_bcd (tm.tm_sec);
		rcm.min = byte_to_bcd (tm.tm_min);
		rcm.hrs = byte_to_bcd (tm.tm_hour);  /* selects 24hr mode */
		rcm.day = 1;			     /* too hard to calc day of week */
		rcm.date = byte_to_bcd (tm.tm_mday);
		rcm.mon = byte_to_bcd (tm.tm_mon);
		rcm.yr = byte_to_bcd (tm.tm_year);

		/* stop rtc update while we write */
		rtc_write (RTC_CONTROL, RTC_STOP|RTC_XTAL);
		rtc_blkwrite(RTC_SEC, &rcm, sizeof(rcm));
		rtc_write (RTC_CONTROL, RTC_START|RTC_XTAL);
	}
}

struct timeval
rtc_get()
{
	struct timeval tv;
	struct tm tm;
	struct rtc_clk_map rcm;
	struct rtc_clk_map_new rcm_new;

	XPR(XPR_TIMER, ("rtc_get entry\n"));
	tv.tv_usec = 0;
	if (rtc_read (RTC_STATUS) & RTC_FTU) {
		tv.tv_sec = 0;
		XPR(XPR_TIMER, ("rtc_get exit\n"));
		return (tv);
	}

	if (new_clock_chip) {
		rtc_blkread(RTC_CNTR0, &rcm_new, sizeof(rcm_new));
		tv.tv_sec =  (rcm_new.cntr0 << 24) | (rcm_new.cntr1 << 16) |
			(rcm_new.cntr2 << 8) | rcm_new.cntr3;
#if	NCC
		printf ("rtc_get: tv_sec 0x%x\n", tv.tv_sec);
#endif	NCC
	} else {
		do {
			rtc_blkread(RTC_SEC, &rcm, sizeof(rcm));
		} while (rcm.sec != rtc_read(RTC_SEC));
	
		tm.tm_sec = bcd_to_byte(rcm.sec);
		tm.tm_min = bcd_to_byte(rcm.min);
		tm.tm_hour = bcd_to_byte(rcm.hrs);
		tm.tm_mday = bcd_to_byte(rcm.date);
		tm.tm_mon = bcd_to_byte(rcm.mon);
		tm.tm_year = bcd_to_byte(rcm.yr);
		tv.tv_sec = tm_to_sec(&tm);
	}
	XPR(XPR_TIMER, ("rtc_get exit\n"));
	return (tv);
}

bcd_to_byte (bcd)
	register u_char bcd;
{
	return ((bcd>>4)*10 + (bcd&0xf));
}

byte_to_bcd (byte)
	register u_char byte;
{
	register u_char hi_nib = 0;

	while (byte > 9) {
		hi_nib += 0x10;
		byte -= 10;
	}
	return (hi_nib | byte);
}

nvram_check (nip)
	register struct nvram_info *nip;
{
	register u_short verify, computed;

	bzero (nip, sizeof (*nip));
	rtc_blkread(RTC_RAM, nip, sizeof(*nip));
	verify = nip->ni_cksum;
	nip->ni_cksum = 0;
	if ((computed = ~checksum_16 ((caddr_t) nip, sizeof (*nip) >> 1)) == 0 ||
	    computed != verify) {
		printf ("non-volatile memory checksum wrong\n");
		return (-1);
	}
	nip->ni_cksum = verify;
	return (0);
}

nvram_set (nip)
	register struct nvram_info *nip;
{
	struct nvram_info verify;
	register int i;
	register u_char *cp;

	nip->ni_cksum = 0;
	nip->ni_cksum = ~checksum_16 ((caddr_t) nip, sizeof (*nip) >> 1);
	rtc_blkwrite(RTC_RAM, nip, sizeof(*nip));
	bcopy ((caddr_t) nip, (caddr_t) &verify, sizeof (*nip));
	nvram_check (nip);
	if (bcmp ((caddr_t) &verify, (caddr_t) nip, sizeof (*nip)) != 0)
		printf ("non-volatile memory readback error!\n");
}

rtc_write (addr, val)
	u_char addr, val;
{
	register int i, def, s;

	ASSERT((addr & ~0x3f) == 0);
#if	NCC
#else	NCC
	addr |= RTC_WRITE;
#endif	NCC
	s = spldma();
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCLK);
	ASSERT((def & (SCR2_RTDATA|SCR2_RTCE|SCR2_RTCLK)) == 0);
	*scr2 = def | SCR2_RTCE;
	DELAY (1);
	splx (s);

	for (i = 0;  i < 8;  i++) {
		s = spldma();
		def = *scr2 & ~SCR2_RTDATA;
		ASSERT((def & (SCR2_RTCE|SCR2_RTCLK)) == SCR2_RTCE);
		if (addr & 0x80)
			def |= SCR2_RTDATA;
		*scr2 = def;
		DELAY (1);
		def |= SCR2_RTCLK;
		*scr2 = def;
		DELAY (1);
		def &= ~SCR2_RTCLK;
		*scr2 = def;
		addr <<= 1;
		DELAY (1);
		splx (s);
	}

	for (i = 0;  i < 8;  i++) {
		s = spldma();
		def = *scr2 & ~SCR2_RTDATA;
		ASSERT((def & (SCR2_RTCE|SCR2_RTCLK)) == SCR2_RTCE);
		if (val & 0x80)
			def |= SCR2_RTDATA;
		*scr2 = def;
		DELAY (1);
		def |= SCR2_RTCLK;
		*scr2 = def;
		DELAY (1);
		def &= ~SCR2_RTCLK;
		*scr2 = def;
		val <<= 1;
		DELAY (1);
		splx (s);
	}

	s = spldma();
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCE | SCR2_RTCLK);
	*scr2 = def;
	splx (s);
}

rtc_blkwrite (addr, valp, count)
	u_char addr, *valp;
	int count;
{
	register int i, def, s, val;

	ASSERT((addr & ~0x3f) == 0);
#if	NCC
#else	NCC
	addr |= RTC_WRITE;
#endif	NCC
	s = spldma();
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCLK);
	ASSERT((def & (SCR2_RTDATA|SCR2_RTCE|SCR2_RTCLK)) == 0);
	*scr2 = def | SCR2_RTCE;
	DELAY (1);
	splx (s);

	for (i = 0;  i < 8;  i++) {
		s = spldma();
		def = *scr2 & ~SCR2_RTDATA;
		ASSERT((def & (SCR2_RTCE|SCR2_RTCLK)) == SCR2_RTCE);
		if (addr & 0x80)
			def |= SCR2_RTDATA;
		*scr2 = def;
		DELAY (1);
		def |= SCR2_RTCLK;
		*scr2 = def;
		DELAY (1);
		def &= ~SCR2_RTCLK;
		*scr2 = def;
		addr <<= 1;
		DELAY (1);
		splx (s);
	}

	while (count-- > 0) {
		val = *valp++;
		for (i = 0;  i < 8;  i++) {
			s = spldma();
			def = *scr2 & ~SCR2_RTDATA;
			ASSERT((def & (SCR2_RTCE|SCR2_RTCLK)) == SCR2_RTCE);
			if (val & 0x80)
				def |= SCR2_RTDATA;
			*scr2 = def;
			DELAY (1);
			def |= SCR2_RTCLK;
			*scr2 = def;
			DELAY (1);
			def &= ~SCR2_RTCLK;
			*scr2 = def;
			val <<= 1;
			DELAY (1);
			splx (s);
		}
	}

	s = spldma();
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCE | SCR2_RTCLK);
	*scr2 = def;
	splx (s);
}

rtc_read (addr)
	u_char addr;
{
	u_char val;
	
	val = rtc_real_read (addr);
#if	NCC
	rtc_real_read (RTC_STATUS);	/* MCS1850 bug: park address to reduce Ibatt */
#endif	NCC
	return (val);
}

rtc_real_read (addr)
	u_char addr;
{
	register int i, def, s;
	register u_char val;

#if	NCC
	addr |= RTC_WRITE;
#endif	NCC
	s = spldma();
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCLK);
	ASSERT((def & (SCR2_RTDATA|SCR2_RTCE|SCR2_RTCLK)) == 0);
	*scr2 = def | SCR2_RTCE;
	DELAY (1);
	splx (s);

	for (i = 0;  i < 8;  i++) {
		s = spldma();
		def = *scr2 & ~SCR2_RTDATA;
		ASSERT((def & (SCR2_RTCE|SCR2_RTCLK)) == SCR2_RTCE);
		if (addr & 0x80)
			def |= SCR2_RTDATA;
		*scr2 = def;
		DELAY (1);
		def |= SCR2_RTCLK;
		*scr2 = def;
		DELAY (1);
		def &= ~SCR2_RTCLK;
		*scr2 = def;
		addr <<= 1;
		DELAY (1);
		splx (s);
	}

	val = 0;
	for (i = 0;  i < 8;  i++) {
		s = spldma();
		def = *scr2 & ~SCR2_RTDATA;
		ASSERT((def & (SCR2_RTCE|SCR2_RTCLK)) == SCR2_RTCE);
		*scr2 = def | SCR2_RTCLK;
		DELAY (1);
		val <<= 1;
		*scr2 = def;
		DELAY (1);
		val |= (*scr2 & SCR2_RTDATA)? 1 : 0;
		DELAY (1);
		splx (s);
	}

	s = spldma();
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCE | SCR2_RTCLK);
	*scr2 = def;
	splx (s);
	return (val);
}

rtc_blkread (addr, valp, count)
	u_char addr, *valp;
	int count;
{
	rtc_real_blkread (addr, valp, count);
#if	NCC
	rtc_real_read (RTC_STATUS);	/* MCS1850 bug: park address to reduce Ibatt */
#endif	NCC
}

rtc_real_blkread (addr, valp, count)
	u_char addr, *valp;
	int count;
{
	register int i, def, s;
	register u_char val;

#if	NCC
	addr |= RTC_WRITE;
#endif	NCC
	s = spldma();
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCLK);
	ASSERT((def & (SCR2_RTDATA|SCR2_RTCE|SCR2_RTCLK)) == 0);
	*scr2 = def | SCR2_RTCE;
	DELAY (1);
	splx (s);

	for (i = 0;  i < 8;  i++) {
		s = spldma();
		def = *scr2 & ~SCR2_RTDATA;
		ASSERT((def & (SCR2_RTCE|SCR2_RTCLK)) == SCR2_RTCE);
		if (addr & 0x80)
			def |= SCR2_RTDATA;
		*scr2 = def;
		DELAY (1);
		def |= SCR2_RTCLK;
		*scr2 = def;
		DELAY (1);
		def &= ~SCR2_RTCLK;
		*scr2 = def;
		addr <<= 1;
		DELAY (1);
		splx (s);
	}

	while (count-- > 0) {
		val = 0;
		for (i = 0;  i < 8;  i++) {
			s = spldma();
			def = *scr2 & ~SCR2_RTDATA;
			ASSERT((def & (SCR2_RTCE|SCR2_RTCLK)) == SCR2_RTCE);
			*scr2 = def | SCR2_RTCLK;
			DELAY (1);
			val <<= 1;
			*scr2 = def;
			DELAY (1);
			val |= (*scr2 & SCR2_RTDATA)? 1 : 0;
			DELAY (1);
			splx (s);
		}
		*valp++ = val;
	}

	s = spldma();
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCE | SCR2_RTCLK);
	*scr2 = def;
	splx (s);
}
