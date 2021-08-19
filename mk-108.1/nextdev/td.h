/*	@(#)td.h	1.0	10/12/87	(c) 1987 NeXT	*/

/*
 * Tom Duff memorial unrolled loop macro
 */
#define LOOP32(nwords, op) { \
        register int n = (nwords)>>5; \
        switch ((nwords)&31) { \
        case 0:  while (n-- > 0){ op; \
        case 31: op; case 30: op; case 29: op; case 28: op; case 27: op; \
        case 26: op; case 25: op; case 24: op; case 23: op; case 22: op; \
        case 21: op; case 20: op; case 19: op; case 18: op; case 17: op; \
        case 16: op; case 15: op; case 14: op; case 13: op; case 12: op; \
        case 11: op; case 10: op; case 9: op; case 8: op; case 7: op; \
        case 6: op; case 5: op; case 4: op; case 3: op; case 2: op; \
        case 1: op; \
                } \
        } \
}
