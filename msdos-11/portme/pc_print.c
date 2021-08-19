/*
*****************************************************************************
	PC_PRINTF  - Mini printf for systems needing one

Summary
	#include <pcdisk.h>

	INT printf(format,[,argument]...)

format string description
	%[flags]['0'|' '][width][.precision][typemod]type

	flags		 if - left justify output 
	width 		 width of the output			
	.precision 	 may be present but is ignored
	typemod 	 l = LONG, h = Short
	type		c = char
				d = int
				i = int
				x,X = Hex display of int
				s = string
Additional functions:
	TEXT *pc_stoa(short int num, TEXT *dest, INT  base);
	TEXT *pc_itoa(INT  num, TEXT *dest, INT  base);
	TEXT *pc_ltoa(LONG num, TEXT *dest, INT  base);
	TEXT *pc_strjust(TEXT *to, TEXT *from, BOOL leftjust, INT width, TEXT padchart);
	VOID pc_putstr(TEXT *p);

Description
	Subset of printf.

NOTE:
	If your environment already has printf, use it, otherwise un-comment
	USEEBPRINTF in pcdisk.h, port the code and recompile.
PORTING Considerations:
	Your system is assumed to have the ANSI va_xx variable argument handling
	macros. If it doesn't you'll have to get them. See microsoft C or a 
	good ANSI compiler for your target system.

	All output is done via pc_putstr().The standard routine putchar() is used
	to print characters. You will have to supply a putchar() routine to put
	single characters to your output device.
	
Returns
 	The number of characters printed

Example:
	#include <pcdisk.h>
	printf("How about 3 here :%10i:\n",3);
	printf("How about 3 here :%-10i:\n",3);
	printf("How about X here :%c:\n",'X');
	printf("How about 7L here :%4ld:\n",7L);
	printf("How about joe here :%-20s:\n","JOE JACKSON");
*****************************************************************************
*/

#include <pcdisk.h>

#ifdef USEEBPRINTF

TEXT *pc_stoa(short int num, TEXT *dest, INT  base);
TEXT *pc_itoa(INT  num, TEXT *dest, INT  base);
TEXT *pc_ltoa(LONG num, TEXT *dest, INT  base);
TEXT *pc_strjust(TEXT *to, TEXT *from, BOOL leftjust, INT width, TEXT padchar);
VOID pc_putstr(TEXT *p);


#include <stdarg.h> /*PORTME*/
#include <stdio.h>  /*PORTME- (only here for putchar) */


#ifdef IMNOTDEFINED
/* A test program if you are proting printf to your system */
#ifndef ANSIFND
main()
	{
#else
main() /* _fn_ */
	{
#endif
	char *pc_itoa();
	char buff[100];	
	char buff2[100];	
	TEXT *pc_strjust();



	printf("How about 3 here :%10i:\n",3);
	printf("How about 3 here :%-10i:\n",3);
	printf("How about X here :%c:\n",'X');
	printf("How about 7L here :%4ld:\n",7L);
	printf("How about 7L here :%-4ld:\n",7L);
	printf("How about joe here :%30s:\n","JOE BLOW");
	printf("How about joe here :%30.30s:\n","JOE BLOW");
	printf("How about joe here :%-30s:\n","JOE BLOW");
	printf("How about joe here :%-30.30s:\n","JOE BLOW");
	printf("How about 0x456 here :%x:\n",0x456);
	}
#endif

/* Code starts here */

/* States */
#define PRNDOCHAR	1
#define PRNDOFMT	2
#define PRNDOFLAG	3				
#define PRNDOWID	4				
#define PRNDOPREC	5
#define PRNDOMOD	6
#define PRNDOTYPE	7
#define PRNDOPAD	8

/* Integer types expected */
#define PRISINT		1
#define PRISLONG	2
#define PRISSHORT	3
	
#ifndef ANSIFND
INT printf(fmt)
	TEXT *fmt;
	{
#else
INT printf(TEXT *fmt) /* _fn_ */
	{
#endif
	va_list arg_m; /*PORTME*/
	INT fmstate;
	TEXT buff[20];
	TEXT boutstr[132];
	BOOL leftjustify;
	INT  outpwidth;
	INT  outpmult;
	INT  outptype;
	INT  cbase;
	TEXT *outstr = &boutstr[0];
	TEXT c;
	TEXT padchar;
	

	va_start( arg_m, fmt); /*PORTME*/

	fmstate = PRNDOCHAR;	/* Start by looking for output */

	while (*fmt)
		{
		switch (fmstate)
			{
		case PRNDOCHAR:		/* Normal: switch states if 5 else just print*/
			if (*fmt == '%')
				fmstate = PRNDOFMT;
			else
				{
				*outstr++ = *fmt;
				fmt++;
				}
			break;		
		case PRNDOFMT:	/* got a '%' reset print variable & go to flag state */
			leftjustify = NO;
			outpwidth = 0;
			outpmult = 1;
			outptype = PRISINT;
			padchar = ' ';
			if (*(fmt+1) == '%') /* print '%' if you get %% */
				{
				fmstate = PRNDOCHAR;
				*outstr++ = '%';
				fmt += 2;
				}
			else
				{
				fmstate =  PRNDOFLAG;
				fmt++;
				}
			break;
		case PRNDOFLAG:	 /* Test flags and go to width state */
			if (*fmt == '-')
				{
				leftjustify = YES;
				*fmt++;
				}
			fmstate =  PRNDOPAD;
			break;
		case PRNDOPAD:	 /* Test for padding character */
			if ( (*fmt == '0') || (*fmt == ' ') )
				{
				padchar = *fmt++;
				}
			fmstate =  PRNDOWID;
			break;
		case PRNDOWID:	/* In width state. convert width specifier to INT*/		
			if ( ('0' <= *fmt) && (*fmt <= '9')  )
				{
				outpwidth *= outpmult;
				outpmult *= 10;
				outpwidth += *fmt - '0';
				fmt++;
				}
			else		/* Done with width. Go to precision state. */
				fmstate =  PRNDOPREC;
			break;
		case PRNDOPREC:	/* Discard precision info and Go to MOD state*/
			if ( (('0' <= *fmt) && (*fmt <= '9')) || (*fmt == '.') )
				fmt++;
			else
				fmstate =  PRNDOMOD;
			break;
		case PRNDOMOD:	/* Look for type modifiers. And go to TYPE state */
			fmstate =  PRNDOTYPE;
			outptype = PRISINT;
			if (*fmt == 'l')
				outptype = PRISLONG;
			else if (*fmt == 'h')
				outptype = PRISSHORT;
			else
				break;
			fmt++;
			break;
		case PRNDOTYPE: /* Determine the type and display it based on the 
						   flag:width:modier info */
			fmstate = PRNDOCHAR;
			switch(*fmt)
				{
				case 'c': /*  = char */
					c = va_arg(arg_m,char); /*PORTME*/
					*outstr++ = c;
					fmt++;
					break;
				default:	/* INTs LONGS and SHORTS */
				case 'd':
				case 'i':
				case 'x':
				case 'X':
					*outstr = '\0';
					/* Hex or decimal output form */
					cbase =( (*fmt == 'i') || (*fmt == 'd')) ? 10 : 16;
					fmt++;
					switch (outptype)
						{
						case PRISINT: /*PORTME*/
							pc_itoa(va_arg(arg_m,int), buff,cbase);
							break;
						case PRISLONG: /*PORTME*/
							pc_ltoa(va_arg(arg_m,long int), buff,cbase);
							break;
						case PRISSHORT: /*PORTME*/
							pc_stoa(va_arg(arg_m,short int), buff,cbase);
							break;
						default:
							*buff = '\0';
						}
					/* Now justify the the string */
					pc_strjust( outstr, buff, leftjustify, outpwidth, padchar);
					/* We xtoa'd into the buffer,now we have to go to the end*/
					while (*outstr)
						outstr++;
					break;
				case 's': /*  = string */
					fmt++;
					pc_strjust( outstr,va_arg(arg_m,char *), /*PORTME*/
								leftjustify, outpwidth, padchar);
					while (*outstr)
						outstr++;
					break;
				}
			fmstate = PRNDOCHAR; /* Back to normal after doing a %xxx */
			break;
		default:	/* If lost just print the format string */
			fmstate = PRNDOCHAR;
			*outstr++ = *fmt++;
			break;
			}
		}
	*outstr = '\0';
	pc_putstr(&boutstr[0]); /*PORTME*/
	return ( (INT) (outstr - &boutstr[0]) );
	}

/* Short to HEX or Decimal converter */	
/* Note dest buffer must hold at least 15 bytes */
#ifndef ANSIFND
TEXT *pc_stoa( num, dest, base)
	short int num;
	TEXT *dest;
	INT  base;
	{
#else
TEXT *pc_stoa(short int num, TEXT *dest, INT  base) /* _fn_ */
	{
#endif
	return (pc_ltoa( (LONG) num, dest, base));
	}

/* Int to HEX or Decimal converter */	
/* Note dest buffer must hold at least 15 bytes */
#ifndef ANSIFND
TEXT *pc_itoa( num, dest, base)
	INT  num;
	TEXT *dest;
	INT  base;
	{
#else
TEXT *pc_itoa(INT  num, TEXT *dest, INT  base) /* _fn_ */
	{
#endif
	return (pc_ltoa( (LONG) num, dest, base));
	}

/* Long to HEX or Decimal converter */	
/* Note dest buffer must hold at least 15 bytes */
#ifndef ANSIFND
TEXT *pc_ltoa( num, dest, base)
	LONG num;
	TEXT *dest;
	INT  base;
	{
#else
TEXT *pc_ltoa(LONG num, TEXT *dest, INT  base) /* _fn_ */
	{
#endif
	LONG digit;
	TEXT *olddest = dest;
	TEXT *p;

	dest += 15;	
	*dest = '\0';

	/* Convert num to a string going from dest[15] backwards */	
	/* Nasty little ItoA algorith */
	do
		{
		digit = num % base;
		*(--dest) =
          (TEXT)(digit<10 ? (TEXT)(digit + '0') : (TEXT)((digit-10) + 'A'));
		num = num / base;
		}
		while (num);

	/* Now put the converted string at the beginning of the buffer */
	for (p = olddest; *dest;)
		*p++ = *dest++;
	*p = '\0';
	return (olddest);
	}
		
/* Left or right justify a string into a buffer padding with blanks.
   or padchar when right justifying
   If width is zero use the width of the input string */
#ifndef ANSIFND
TEXT *pc_strjust( to, from, leftjust, width, padchar)
	TEXT *to;
	TEXT *from;
	BOOL leftjust;
	INT	 width;
	TEXT padchar;
	{
#else
TEXT *pc_strjust(TEXT *to, TEXT *from, BOOL leftjust, INT	 width, TEXT padchar) /* _fn_ */
	{
#endif
	INT fromlen;
	FAST TEXT *todest;
	FAST TEXT *fromdest;
	FAST TEXT *p;
	INT i;

	fromlen = 0;
	p = from;
	while (*p++)
		fromlen++;
	
	*to = '\0';

	if (!fromlen)
		return (to);
	if (!width)
		width = fromlen;

	if (leftjust)
		{
		pc_cppad(to, from, width);
		*(to + width) = '\0';
		}
	else /* Right justify. Filling left side with padchar */
		{
		/* Blank fill the string to start */
		p = to;
		for (i = 0; i < width; i++)
			*p++ = padchar;
		todest = to + width;
		fromdest = (from + fromlen);
		while ( (fromdest >= from) && (todest >= to) )
			*todest-- = *fromdest--;
		}
	return (to);
	}

#ifndef ANSIFND
VOID pc_putstr(p)	/*PORTME*/
	TEXT *p;
	{
#else
VOID pc_putstr(TEXT *p) /* _fn_ */
	{
#endif
	while(*p)
		{
		putchar(*p);	/* You might have to roll your own putchar routine */
		p++;			/* If so get rid of stdio.h above */
		}
	}
	
#endif
