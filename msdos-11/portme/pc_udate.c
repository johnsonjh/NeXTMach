/*
*****************************************************************************
	PC_GETSYSDATE - Get the current system time and date (USER SUPPLIED)

				   
Summary
	
	#include <pcdisk.h>

	DATESTR *pc_getsysdate(pd)
		DATESTR *pd;     put result here  


 Description
 	When the system needs to date stamp a file it will call this routine
 	to get the current time and date. YOU must modify the shipped routine
 	to support your hardware's time and date routines. If you don't modify
 	this routine the file date on all files will be the same.

	The source for this routine is in file pc_udate.c and is self
	explanatory.

	NOTE: Be sure to retain the property of returning its argument. The 
		  package depends on it.

 Returns
 	The argument it was passed. A structure containing date and time info.

Example:
	#include <pcdisk.h>

	DATESTR crdate;   

	Load the inode copy name,ext,attr,cluster, size,datetime
	pc_init_inode( pobj->finode, filename, fileext, 
					attr, cluster, 
					0L ,pc_getsysdate(&crdate) );

*****************************************************************************
*/

#include <pcdisk.h>
#import <sys/types.h>
#import <sys/time.h>
#import <tzfile.h>

/* #define TIME_DEBUG 1		/* */

static void sec_to_tm(long sec, struct tm *tmp);
extern void microtime(struct timeval *tvp);

/* get date and time from the host system */
#ifndef ANSIFND
DATESTR *pc_getsysdate(pd)
	DATESTR *pd;     /* put result here  */
	{	
#else
DATESTR *pc_getsysdate(DATESTR *pd) /* _fn_ */
	{
#endif
	UTINY  year;       /* relative to 1980 */ 
	UTINY  month;      /* 1 - 12 */ 
	UTINY  day;        /* 1 - 31 */ 
	UTINY  hour;
	UTINY  minute;
	UTINY  sec;				/* Note: seconds are 2 
						 * second/per. ie 3 == 6 
						 * seconds */
	struct timeval tv;
	struct tm tm;
	
#ifdef	FAKEIT
	/* Hardwire for now */

	/* 7:37:28 PM */
	hour = 19;
	minute = 37;
	sec = 14;
	pd->time = ( (hour << 11) | (minute << 5) | sec);

	/* 3-28-88 */
	year = 8;       /* relative to 1980 */ 
	month = 3;      /* 1 - 12 */ 
	day = 28;        /* 1 - 31 */ 

	pd->date = ( (year << 9) | (month << 5) | day);
#else	FAKEIT

	microtime(&tv);
	sec_to_tm(tv.tv_sec, &tm);
	pd->time = ( (tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
	pd->date = ( ((tm.tm_year-10) << 9) | (tm.tm_mon << 5) | tm.tm_mday);
#endif	FAKEIT
	return (pd);
	}

/*
 * Cloned from libc/gen/ctime.c
 */
 
static int mon_lengths[2][MONS_PER_YEAR] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  }
};

static int year_lengths[2] = {
	DAYS_PER_NYEAR, DAYS_PER_LYEAR
};

static void sec_to_tm(long sec, 
	struct tm *tmp) 			/* RETURNED */
{
	register long		days;
	register long		rem;
	register int		y;
	register int		yleap;
	register int *		ip;

#ifdef	TIME_DEBUG
	printf("sec_to_tm: sec = %d\n", sec);
#endif	TIME_DEBUG
	days = sec / SECS_PER_DAY;
	rem  = sec % SECS_PER_DAY;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}
	tmp->tm_hour = (int) (rem / SECS_PER_HOUR);
	rem = rem % SECS_PER_HOUR;
	tmp->tm_min = (int) (rem / SECS_PER_MIN);
	tmp->tm_sec = (int) (rem % SECS_PER_MIN);
	tmp->tm_wday = (int) ((EPOCH_WDAY + days) % DAYS_PER_WEEK);
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYS_PER_WEEK;
	y = EPOCH_YEAR;
	if (days >= 0)
		for ( ; ; ) {
			yleap = isleap(y);
			if (days < (long) year_lengths[yleap])
				break;
			++y;
			days = days - (long) year_lengths[yleap];
		}
	else do {
		--y;
		yleap = isleap(y);
		days = days + (long) year_lengths[yleap];
	} while (days < 0);
	tmp->tm_year = y - TM_YEAR_BASE;
	tmp->tm_yday = (int) days;
	ip = mon_lengths[yleap];
	for (tmp->tm_mon = 0; days >= (long) ip[tmp->tm_mon]; ++(tmp->tm_mon))
		days = days - (long) ip[tmp->tm_mon];
	tmp->tm_mday = (int) (days + 1);
	tmp->tm_isdst = 0;
#ifdef	TIME_DEBUG
	printf("   sec_to_tm: year %d mon %d mday %d hour %d min %d sec %d\n",
	tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_hour, tmp->tm_min,
	tmp->tm_sec); 
#endif	TIME_DEBUG
}

/*
 * Returns seconds since 1-1-80 for time indicated by *tmp.
 */
int msdtm_to_sec(struct tm *tmp)
{
	time_t stamp;
	register long days;
	register int month, year, y;
	register int *ip;

	year = tmp->tm_year + TM_YEAR_BASE;
	month = tmp->tm_mon;
	if (month < 0) {
	    year -= (11-month) / 12;
	    month = 11 - ((11-month) % 12);
	} else {
            year += month / 12;
	    month = month % 12;
	}
	days = (long) tmp->tm_mday - 1;
	ip = mon_lengths[isleap(year)];
	while (month>0) {
	    month--;
	    days += ip[month];
	}
	y = EPOCH_YEAR;
	while (y < year) {
	      days += (long) year_lengths[isleap(y)];
	      y++;
	}
	while (y > year) {
	      y--;
	      days -= (long) year_lengths[isleap(y)];
	}
        stamp = tmp->tm_sec
	      + SECS_PER_MIN*tmp->tm_min
	      + SECS_PER_HOUR*tmp->tm_hour
	      + SECS_PER_DAY*days;
	return(stamp);
}

/*
 * Convert DOS file system time to timeval
 */
void msd_time_to_timeval(UCOUNT time, UCOUNT date, struct timeval *tvp)
{
	struct tm tm;
	struct timeval tv;
	
#ifdef	TIME_DEBUG
	printf("msd_time_to_timeval: time 0x%x  date 0x%x\n", time, date);
#endif	TIME_DEBUG
	tm.tm_sec  =  (time & 0x1f) * 2;
	tm.tm_min  = (time >> 5) & 0x3f;
	tm.tm_hour = (time >> 11) & 0x1f;
	tm.tm_mday =  date & 0x1f;
	tm.tm_mon  = (date >> 5) & 0xf;
	tm.tm_year = ((date >> 9) & 0xff) + 10; 	/* DOS - base = 1980 */
	tvp->tv_usec = 0;
#ifdef	TIME_DEBUG
	printf("   year %d mon %d mday %d hour %d min %d sec %d\n",
	tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min,
	tm.tm_sec); 
#endif	TIME_DEBUG
	tvp->tv_sec = msdtm_to_sec(&tm);
#ifdef	TIME_DEBUG
	printf("   tv_sec = %d\n", tvp->tv_sec);
#endif	TIME_DEBUG
	return;
}

/* end of pc_udate.c */