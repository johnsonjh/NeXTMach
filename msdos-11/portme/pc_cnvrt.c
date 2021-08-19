/*
*****************************************************************************
	PC_CNVRT -  Convert intel byte order to native byte order.

					   
Summary
	#include <pcdisk.h>

	ULONG to_DWORD (from)  Convert intel style 32 bit to native 32 bit
		UTINY *from;

	UCOUNT to_WORD (from)  Convert intel style 16 bit to native 16 bit
		UTINY *from;

	VOID fr_WORD (to,from) Convert native 16 bit to 16 bit intel
		UTINY *to;
		UCOUNT from;

	VOID fr_DWORD (to,from) Convert native 32 bit to 32 bit intel
		UTINY *to;
		ULONG from;

 Description
	This code is known to work on 68K and 808x machines. It has been left
	as generic as possible. You may wish to hardwire it for your CPU/Code
	generator to shave off a few bytes and microseconds, be careful though.

	Note: Optimize at your own peril, and after everything else is debugged.
	
 	Bit shift operators are used to convert intel ordered storage
 	to native. The host byte ordering should not matter.

Returns

Example:
	See other sources.

*****************************************************************************
*/

#include <pcdisk.h>

/* Convert a 32 bit intel item to a portable 32 bit */
#ifndef ANSIFND
ULONG to_DWORD (from)   /* i[3] << 24 | i[2] << 16 | i[1] << 8 | i[0] */
	FAST UTINY *from;
		{
#else
ULONG to_DWORD (FAST UTINY *from) /* _fn_ */
	{
#endif
		FAST ULONG t,res;
		t = ((ULONG) *(from + 3)) & 0xff;
		res = (t << 24);
		t = ((ULONG) *(from + 2)) & 0xff;
		res |= (t << 16);
		t = ((ULONG) *(from + 1)) & 0xff;
		res |= (t << 8);
		t = ((ULONG) *from) & 0xff;
		res |= t;
		return(res);
		}


/* Convert a 16 bit intel item to a portable 16 bit */
#ifndef ANSIFND
UCOUNT to_WORD (from) /*  i[1] << 8 | i[0] */
	FAST UTINY *from;
		{
#else
UCOUNT to_WORD (FAST UTINY *from) /* _fn_ */
	{
#endif
		FAST UCOUNT nres;
		FAST UCOUNT t;
		t = ((UCOUNT) *(from + 1)) & 0xff;
		nres = (t << 8);
		t = ((UCOUNT) *from) & 0xff;
		nres |= t;
		return(nres);
		}

/* Convert a portable 16 bit to a  16 bit intel item */ 
#ifndef ANSIFND
VOID fr_WORD (to,from) 
	FAST UTINY *to;
	FAST UCOUNT from;
		{
#else
VOID fr_WORD (FAST UTINY *to, FAST UCOUNT from) /* _fn_ */
	{
#endif
		*to        =    (UTINY) (from & 0x00ff);
		*(to + 1)   =   (UTINY) ((from >> 8) & 0x00ff);
		}

/* Convert a portable 32 bit to a  32 bit intel item */
#ifndef ANSIFND
VOID fr_DWORD (to,from) 
	FAST UTINY *to;
	FAST ULONG from;
		{
#else
VOID fr_DWORD (FAST UTINY *to, FAST ULONG from) /* _fn_ */
	{
#endif
		*to   = (UTINY) (from & 0xff);
		*(to + 1)   =  (UTINY) ((from >> 8) & 0xff);
		*(to + 2)   =  (UTINY) ((from >> 16) & 0xff);
		*(to + 3)   =  (UTINY) ((from >> 24) & 0xff);
		}
