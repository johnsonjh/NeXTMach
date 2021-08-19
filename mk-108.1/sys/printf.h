/*	@(#)printf.h	1.0	2/2/90		(c) 1990 NeXT	*/

/* 
 * HISTORY
 *  2-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef _SYS_PRINTF_
#define _SYS_PRINTF_

#import <sys/types.h>
#import <sys/buf.h>
#import <sys/tty.h>

extern const char *panicstr;

int printf(const char *format, ...);
int uprintf(const char *format, ...);
int tprintf(struct tty *tp, const char *format, ...);
int log(int level, const char *format, ...);
int prf(const char *fmt, u_int *adx, int flags, struct tty *ttyp);
void panic_init(void);
void panic(const char *s);
void tablefull(const char *tab);
void harderr(struct buf *bp, const char *cp);
int (putchar)(int c);
void logchar(int c);
int sprintf(char *s, const char *format, ...);
#endif _SYS_PRINTF_




