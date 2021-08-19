/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 * 28-Aug-90  Morris Meyer (mmeyer) at NeXT
 *	Created.
 */
#define	FPTRACE_START	1
#define	FPTRACE_STOP	2
#define FPTRACE_CLEAR	3

#define	NUM_UNIMP	0x80		/* Bits 6:0 */


struct fptrace_data  {
	int	unimp[NUM_UNIMP];
	int	unsupp;
	int	denorm;
	int	unnorm;
	int	bsun;
	int	inex;
	int	dz;
	int	unfl;
	int	operr;
	int	ovfl;
	int	snan;
};
