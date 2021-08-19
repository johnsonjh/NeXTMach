/*
*****************************************************************************
	PR_   -  Miscelaneous print functions.

					   
Summary
	#include <pcdisk.h>

	VOID pr_er_str(p,p1)
		TEXT *p;
		TEXT *p1;

	VOID pr_er_putstr(p)
		TEXT *p;

	VOID pr_db_int (p, i)
		TEXT *p;
		INT i;

	VOID pr_db_str(p,p1)
		TEXT *p;
		TEXT *p1;

	VOID pr_db_putstr(p)
		TEXT *p;
	


Description
	Various print statements. Instead of calling fprintf the libraries
	call these. These are wrappers arount printf, but if you don't have
	a printf you can turn these into stubs or use your own IO routines

Returns

Example:

*****************************************************************************
*/
#include <pcdisk.h>
#include <stdio.h>

#ifndef ANSIFND
VOID pr_er_str(p,p1)
	TEXT *p;
	TEXT *p1;
	{
#else
VOID pr_er_str(TEXT *p, TEXT *p1) /* _fn_ */
	{
#endif
	printf( "%s%s\n",p , p1);
	}

#ifndef ANSIFND
VOID pr_er_putstr(p)
	TEXT *p;
	{
#else
VOID pr_er_putstr(TEXT *p) /* _fn_ */
	{
#endif
	printf( "%s",p);
	}

#ifndef ANSIFND
VOID pr_db_int (p, i)
	TEXT *p;
	INT i;
	{
#else
VOID pr_db_int (TEXT *p, INT i) /* _fn_ */
	{
#endif
	printf("%s%x\n",p,i);
	}

#ifndef ANSIFND
VOID pr_db_str(p,p1)
	TEXT *p;
	TEXT *p1;
	{
#else
VOID pr_db_str(TEXT *p, TEXT *p1) /* _fn_ */
	{
#endif
	printf("%s%s\n",p , p1);
	}

#ifndef ANSIFND
VOID pr_db_putstr(p)
	TEXT *p;
	{
#else
VOID pr_db_putstr(TEXT *p) /* _fn_ */
	{
#endif
	printf( "%s",p);
	}
