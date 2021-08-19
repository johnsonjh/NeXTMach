/*	@(#)clock.h	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 15-Mar-90  John Seamons (jks) at NeXT
 *	Added support for new clock chip.
 *
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 **********************************************************************
 */ 

#ifndef	_CLOCK_
#define	_CLOCK_

#import <next/eventc.h>

/*
 * NeXT clock registers
 */

/* system timer */
struct timer_reg {
	u_char	t_counter_latch[2];	/* counted up at 1 MHz */
	u_char	: 8;
	u_char	: 8;
	u_char	t_enable : 1,		/* counter enable */
		t_update : 1,		/* copy latch to counter */
		: 6;
};

volatile u_char		*timer_high;	/* high byte of timer value/latch */
volatile u_char		*timer_low;	/* low byte of timer value/latch */
volatile u_char		*timer_csr;	/* pointer to timer csr reg */

/* real time clock -- old is MC68HC68T1 chip, new is MCS1850 chip */
#define	RTC_RAM		0x00		/* both */
#define	RTC_SEC		0x20		/* old */
#define	RTC_MIN		0x21
#define	RTC_HRS		0x22
#define	RTC_DAY		0x23
#define	RTC_DATE	0x24
#define	RTC_MON		0x25
#define	RTC_YR		0x26
#define	RTC_ALARM_SEC	0x28
#define	RTC_ALARM_MIN	0x29
#define	RTC_ALARM_HR	0x2a

#define	RTC_STATUS	0x30		/* both */
#define	RTC_CONTROL	0x31		/* both */
#define	RTC_INTRCTL	0x32		/* old */

#define	RTC_CNTR0	0x20		/* new */
#define	RTC_CNTR1	0x21
#define	RTC_CNTR2	0x22
#define	RTC_CNTR3	0x23
#define	RTC_ALARM0	0x24
#define	RTC_ALARM1	0x25
#define	RTC_ALARM2	0x26
#define	RTC_ALARM3	0x27

/*
 * This struct can't be really mapped on top of the device, but it
 * is useful if you want to fill it up before a block transfer to the
 * rtc or as a destination of a block transfer from the rtc.
 */
struct rtc_clk_map {
	u_char	sec;
	u_char	min;
	u_char	hrs;
	u_char	day;		/* day of the week */
	u_char	date;		/* day of the month */
	u_char	mon;
	u_char	yr;
};

struct rtc_clk_map_new {
	u_char	cntr0, cntr1, cntr2, cntr3;
};

/* bits in RTC_STATUS */
#define	RTC_NEW_CLOCK	0x80	/* new: set in new clock chip */
#define	RTC_FTU		0x10	/* both: set when powered up but uninitialized */
#define	RTC_INTR	0x08	/* new: interrupt asserted */
#define	RTC_LOW_BATT	0x04	/* new: low battery */
#define	RTC_ALARM	0x02	/* new: alarm interrupt */
#define	RTC_RPD		0x01	/* new: request to power down */

/* bits in RTC_CONTROL */
#define	RTC_START	0x80	/* both: start counters */
#define	RTC_STOP	0x00	/* both: stop counters */
#define	RTC_XTAL	0x30	/* old: xtal: line = 0, sel0 = sel1 = 1 */
#define	RTC_AUTO_PON	0x20	/* new: auto poweron after power fail */
#define	RTC_AE		0x10	/* new: alarm enable */
#define	RTC_AC		0x08	/* new: alarm clear */
#define	RTC_FTUC	0x04	/* new: first time up clear */
#define	RTC_LBE		0x02	/* new: low battery enable */
#define	RTC_RPDC	0x01	/* new: request to power down clear */

/* bits in RTC_INTRCTL */
#define	RTC_PDOWN	0x40	/* both: power down, bit in RTC_CONTROL on new chip */
#define	RTC_64HZ	0x06	/* old: periodic select = 64 Hz */
#define	RTC_128HZ	0x05	/* old: periodic select = 128 Hz */
#define	RTC_512HZ	0x03	/* old: periodic select = 512 Hz */

/* RTC address byte format */
#define	RTC_WRITE	0x80
#define	RTC_ADRS	0x3f

/* arguments to rtc_set_clr() */
#define	RTC_SET		0xffffffff
#define	RTC_CLR		0

#define	SECDAY		((unsigned)(24*60*60))		/* seconds per day */
#define	SECYR		((unsigned)(365*SECDAY))	/* per common year */
#define	MINSEC		60
#define HRSEC		3600
#define	YRREF		70	/* UNIX time referenced to 1970 */
#define	YRSTART		68	/* year #0 in RTC is 1968 (note: 68%4 = 0) */
#define	LEAPYEAR(year)	((year)%4==0)
#endif	_CLOCK_
