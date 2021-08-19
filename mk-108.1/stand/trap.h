#define	VEC_BUSERR	2
#define	VEC_ADDRERR	3
#define	VEC_ILLINST	4
#define	VEC_TRACE	9
#define	VEC_BRKPT	46

#define	R_D0		0
#define	R_D1		1
#define	R_D2		2
#define	R_D3		3
#define	R_D4		4
#define	R_D5		5
#define	R_D6		6
#define	R_D7		7

#define	R_A0		8
#define	R_A1		9
#define	R_A2		10
#define	R_A3		11
#define	R_A4		12
#define	R_A5		13
#define	R_A6		14
#define	R_A7		15
#define	R_SP		R_A7

#define	NREGS		16

#define	SR_T		0x8000

/*
 * 0 - 15 regs, 16 pc
 */
typedef int jmpbuf[17];
