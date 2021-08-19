/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 * HISTORY
 * 18-Mar-87  John Seamons (jks) at NeXT
 *	Pass second argument to putchar().
 */
#if	DEBUG
int indent = 0;

/*ARGSUSED2*/
iprintf(a, b, c, d, e, f, g, h)
	char *a;
{
	register int i;

	for (i = indent; i > 0;){
		if (i >= 8) {
			putchar('\t', 0);
			i -= 8;
		}
		else {
			putchar(' ', 0);
			i--;
		}
	}

	printf(a, b, c, d, e, f, g, h);
}
#endif	DEBUG
