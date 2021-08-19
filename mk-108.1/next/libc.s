/*
 * Copyright (c) 1987 NeXT, INC
 *
 * HISTORY
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Added bytecopy() routine for byte-wide I/O devices on the 040 that must 
 *	only use byte instructions.
 */

/*
 * PLEASE REPORT ANY BUGS IN THESE ROUTINES TO ME!
 * --mike
 */

#import <next/cframe.h>

/* abs - int absolute value */

PROCENTRY(abs)
	movl	a_p0,d0
	bpl	1f
	negl	d0
1:
	PROCEXIT

#define	BCMP_THRESH	64

PROCENTRY(bcmp)
	movl	a_p0,a0
	movl	a_p1,a1
	movl	a_p2,d0
	movl	d2,sp@-
	movl	d0,d2
	cmpl	#BCMP_THRESH,d2
	ble	cklng1		// not worth aligning
	movl	a0,d0
	movl	a1,d1
	eorl	d0,d1
	andl	#3,d1
	bne	cklng1		// cant align
	subl	d0,d1
	andl	#3,d1		// bytes til aligned
	beq	cklng1		// already aligned
	subl	d1,d2		// decr byte count
	subql	#1,d1		// compensate for top entry of dbcc loop
1:	cmpmb	a0@+,a1@+
	dbne	d1,1b
	bne	fail		// last compare failed

cklng1:	movl	d2,d0
	lsrl	#2,d0		// longwords to compare
	beq	cmpb		// byte count less than longword
	subql	#1,d0		// compensate for first pass
	cmpl	#65535,d0
	ble	cmpl		// in range of dbcc loop
	movl	#65535,d0	// knock off 65536 longs
1:	cmpml	a0@+,a1@+
	dbne	d0,1b
	bne	fail		// last compare failed
	subl	#65536*4,d2	// decr byte count appropriately
	bra	cklng1		// see if now in dbcc range

cmpl:	cmpml	a0@+,a1@+
	dbne	d0,cmpl
	bne	fail
	andl	#3,d2		// must be less than a longword to go
	moveq	#0,d0		// preset condition codes
	bra	cmpb

1:	cmpmb	a0@+,a1@+
cmpb:	dbne	d2,1b
	bne	fail
	movl	sp@+,d2
	PROCEXIT

fail:	moveq	#1,d0
	movl	sp@+,d2
	PROCEXIT

/* bcopy(src, dst, n) */
/* memmove(dst, src, n) */

#define	BCOPY_THRESH	64

PROCENTRY(memmove)
	movl	a_p0,a1		// a1 = destination address
	movl	a_p1,a0		// a0 = source address
	bra	bcopy_common

PROCENTRY(bcopy)
	movl	a_p0,a0		// a0 = source address
	movl	a_p1,a1		// a1 = destination address
bcopy_common:
	movl	a_p2,d0		// d0 = length
	jle	5f		// length <= 0!

	cmpl	a0,a1
	jhi	rcopy		// gotta do reverse copy
	jeq	5f		// src == dst!

	movl	a1,d1		// compute dst - (dst & 3)
	negl	d1
	andl	#3,d1
	jra	2f

1:	movb	a0@+,a1@+	// copy bytes to get dst to long word boundary
	subql	#1,d0
2:	dbeq	d1,1b		// byte count or alignment count exhausted

	movl	d0,d1		// 4 bytes moved per 2 byte instruction
	andl	#0x1c,d1	// so instruction offset is:
	lsrl	#1,d1		// 2 * (k - n % k)
	negl	d1
	addl	#18,d1		// + fudge term of 2 for indexed jump
	jmp	pc@(0,d1)	// now dive into middle of unrolled loop

3:	movl	a0@+,a1@+	// copy next <k> longs
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+

	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+

	subl	#32,d0		// decrement loop count by k*4
	jge	3b

	btst	#1,d0		// copy last short
	jeq	4f
	movw	a0@+,a1@+

4:	btst	#0,d0		// copy last byte
	jeq	5f
	movb	a0@,a1@

5:	PROCEXIT			// that's all folks!

/*
 * Reverse copy
 */
rcopy:	addl	d0,a0		// point at end of source
	addl	d0,a1		// point at end of destination
	movl	a1,d1		// compute (dst & 3)
	andl	#3,d1
	jra	2f

1:	movb	a0@-,a1@-	// copy bytes to get src to long word boundary
	subql	#1,d0
2:	dbeq	d1,1b		// byte count or alignment count exhausted

	movl	d0,d1		// 4 bytes moved per 2 byte instruction
	andl	#0x1c,d1	// so instruction offset is:
	lsrl	#1,d1		// 2 * (k - n % k)
	negl	d1
	addl	#18,d1		// + fudge term of 2 for indexed jump
	jmp	pc@(0,d1)	// now dive into middle of unrolled loop

3:	movl	a0@-,a1@-	// copy next <k> longs
	movl	a0@-,a1@-
	movl	a0@-,a1@-
	movl	a0@-,a1@-

	movl	a0@-,a1@-
	movl	a0@-,a1@-
	movl	a0@-,a1@-
	movl	a0@-,a1@-

	subl	#32,d0		// decrement loop count by k*4
	jge	3b

	btst	#1,d0		// copy last short
	jeq	4f
	movw	a0@-,a1@-

4:	btst	#0,d0		// copy last byte
	jeq	5f
	movb	a0@-,a1@-

5:	PROCEXIT			// that's all folks!

PROCENTRY(bytecopy)
	movl	a_p0,a0		// a0 = source address
	movl	a_p1,a1		// a1 = destination address
	movl	a_p2,d0		// d0 = length
	subql	#1,d0
1:	movb	a0@+,a1@+
	dbra	d0,1b
	PROCEXIT
  
/* bzero(s1, n) */

PROCENTRY(bzero)
	movl	a_p0,a0		// a0 = destination address
	movl	a_p1,d0		// d0 = length
	jle	5f		// length <= 0!

	movl	a0,d1		// compute dst - (dst & 3)
	negl	d1
	andl	#3,d1
	jra	2f

1:	clrb	a0@+		// clear bytes to get dst to long word boundary
	subql	#1,d0
2:	dbeq	d1,1b		// byte count or alignment count exhausted

	movl	d0,d1		// 4 bytes moved per 2 byte instruction
	andl	#0x1c,d1	// so instruction offset is:
	lsrl	#1,d1		// 2 * (k - n % k)
	negl	d1
	addl	#18,d1		// + fudge term of 2 for indexed jump
	jmp	pc@(0,d1)	// now dive into middle of unrolled loop

3:	clrl	a0@+		// clear next <k> longs
	clrl	a0@+
	clrl	a0@+
	clrl	a0@+

	clrl	a0@+
	clrl	a0@+
	clrl	a0@+
	clrl	a0@+

	subl	#32,d0		// decrement loop count by k*4
	jge	3b

	btst	#1,d0		// clear last short
	jeq	4f
	clrw	a0@+

4:	btst	#0,d0		// clear last byte
	jeq	5f
	clrb	a0@

5:	PROCEXIT			// thats all folks!

/* bit = ffs(value) */

PROCENTRY(ffs)
	moveq	#0,d0
	movl	a_p0,d1
	beq	3f
	tstw	d1
	bne	1f
	swap	d1
	addl	#16,d0
1:	tstb	d1
	bne	2f
	lsrl	#8,d1
	addl	#8,d0
2:	andl	#0xff,d1
	lea	fbit,a0
	addl	d1,a0
	addb	a0@,d0
3:	PROCEXIT

//		0xn0,0xn1,0xn2,0xn3,0xn4,0xn5,0xn6,0xn7,0xn8,0xn9,0xnA,0xnB,0xnC,0xnD,0xnE,0xnF
fbit:
	.byte	0x00,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x0m
	.byte	0x05,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x1m
	.byte	0x06,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x2m
	.byte	0x05,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x3m
	.byte	0x07,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x4m
	.byte	0x05,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x5m
	.byte	0x06,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x6m
	.byte	0x05,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x7m
	.byte	0x08,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x8m
	.byte	0x05,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0x9m
	.byte	0x06,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0xAm
	.byte	0x05,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0xBm
	.byte	0x07,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0xCm
	.byte	0x05,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0xDm
	.byte	0x06,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0xEm
	.byte	0x05,0x01,0x02,0x01,0x03,0x01,0x02,0x01,0x04,0x01,0x02,0x01,0x03,0x01,0x02,0x01	// 0xFm

/* bit = rffs(value) */

PROCENTRY(msb)
	moveq	#0,d0
	movl	a_p0,d1
	beq	3f
	moveq	#24,d0
	roll	#8,d1
	tstb	d1
	bne	1f
	subql	#8,d0
	roll	#8,d1
	tstb	d1
	bne	1f
	moveq	#8,d0
	roll	#8,d1
	tstb	d1
	bne	1f
	moveq	#0,d0
	roll	#8,d1
1:	andl	#0xff,d1
	cmpw	#16,d1
	blt	2f
	addql	#4,d0
	lsrl	#4,d1
2:	andl	#0xf,d1
	lea	mbit,a0
	addl	d1,a0
	addb	a0@,d0
3:	PROCEXIT

mbit:
	.byte	0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4

/*
 * Find the first occurence of c in the string cp.
 * Return pointer to match or null pointer.
 *
 * char *
 * index(cp, c)
 *	char *cp, c;
 */

PROCENTRY(index)
	movl	a_p0,a0		// cp
	movl	a_p1,d0		// c
	beq	3f		// c == 0 special cased
1:	movb	a0@+,d1		// *cp++
	beq	2f		// end of string
	cmpb	d1,d0
	bne	1b		// not c
	subw	#1,a0		// undo post-increment
	movl	a0,d0
	PROCEXIT

2:	moveq	#0,d0		// didnt find c
	PROCEXIT

3:	tstb	a0@+
	bne	3b
	subw	#1,a0		// undo post-increment
	movl	a0,d0
	PROCEXIT

/*
 * Copy string s2 over top of string s1.
 * Truncate or null-pad to n bytes.
 *
 * char *
 * strncpy(s1, s2, n)
 *	char *s1, *s2;
 */

PROCENTRY(strncpy)
	movl	a_p0,a0		// dst
	movl	a_p1,a1		// src
	movl	a_p2,d1		// n
	movl	a0,d0
1:	subql	#1,d1
	blt	4f		// n exhausted
	movb	a1@+,a0@+
	bne	1b		// more string to move
	bra	3f		// clear to null until n exhausted

2:	clrb	a0@+
3:	subql	#1,d1
	bge	2b		// n not exhausted
4:	PROCEXIT

/*
 * Concatenate string s2 to the end of s1
 * and return the base of s1.
 *
 * char *
 * strcat(s1, s2)
 *	char *s1, *s2;
 */

PROCENTRY(strcat)
	movl	a_p0,a0		// s1
	movl	a_p1,a1		// s2
	movl	a0,d0
1:	tstb	a0@+
	bne	1b
	subqw	#1,a0
2:	movb	a1@+,a0@+
	bne	2b
	PROCEXIT

/*
 * Compare string s1 lexicographically to string s2.
 * Return:
 *	0	s1 == s2
 *	> 0	s1 > s2
 *	< 0	s2 < s2
 *
 * strcmp(s1, s2)
 *	char *s1, *s2;
 */

PROCENTRY(strcmp)
	movl	a_p0,a0
	movl	a_p1,a1
1:	movb	a0@,d0
	cmpb	a1@+,d0
	beq	3f
	extbl	d0
	movb	a1@-,d1
	extbl	d1
	subl	d1,d0
	PROCEXIT

3:	tstb	a0@+
	bne	1b
	moveq	#0,d0
	PROCEXIT

/*
 * Copy string s2 over top of s1.
 * Return base of s1.
 *
 * char *
 * strcpy(s1, s2)
 *	char *s1, *s2;
 */

PROCENTRY(strcpy)
	movl	a_p0,a0
	movl	a_p1,a1
	movl	a0,d0
1:	movb	a1@+,a0@+
	bne	1b
	PROCEXIT

/*
 * Compare at most n characters of string
 * s1 lexicographically to string s2.
 * Return:
 *	0	s1 == s2
 *	> 0	s1 > s2
 *	< 0	s2 < s2
 *
 * strncmp(s1, s2, n)
 *	char *s1, *s2;
 *	int n;
 */

PROCENTRY(strncmp)
	movl	a_p0,a0
	movl	a_p1,a1
	movl	a_p2,d1
1:	subql	#1,d1
	blt	2f
	movb	a0@+,d0
	cmpb	a1@+,d0
	bne	3f
	tstb	d0
	bne	1b
2:	moveq	#0,d0
	PROCEXIT

3:	extbl	d0
	movb	a1@-,d1
	extbl	d1
	subl	d1,d0
	PROCEXIT

/*
 * Return the length of cp (not counting '\0').
 *
 * strlen(cp)
 *	char *cp;
 */

PROCENTRY(strlen)
	movl	a_p0,a0
	moveq	#-1,d0
1:	addql	#1,d0
	tstb	a0@+
	bne	1b
	PROCEXIT

