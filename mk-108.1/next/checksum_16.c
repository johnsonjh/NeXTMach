#import <sys/types.h>

#define	ADDCARRY(x)  (x > 65535 ? x -= 65535 : x)
#define	REDUCE {l_util.l = sum; sum = l_util.s[0] + l_util.s[1]; ADDCARRY(sum);}

u_short checksum_16 (wp, shorts)
	u_short *wp;
{
	int sum = 0;
	union {
		u_short s[2];
		long	l;
	} l_util;

	while (shorts--)
		sum += *wp++;
	REDUCE;
	return (sum);
}
