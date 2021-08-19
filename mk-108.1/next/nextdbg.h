/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 *  2-Jul-90  Morris Meyer (mmeyer) at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <sys/param.h>
#import <sys/socket.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/if_ether.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>

#define	PBUFSIZ		300

struct kdb_net {
	enum {SERIAL, NET} dev;
	int ri, ti, rlen, seq, locked;
	struct rpkt {
		struct	ether_header ether;
		struct	ip ip;
		struct udphdr udp;
		int seq;
		char buf[PBUFSIZ];
	} rpkt, *rp;
	struct tpkt {
		struct	ether_header ether;
		struct	ip ip;
		struct udphdr udp;
		int seq;
		char buf[PBUFSIZ];
	} tpkt, *tp;
} kdb_net;
#define	HDRSIZE	\
	(sizeof (struct ether_header) + sizeof (struct ip) + \
	sizeof (struct udphdr) + sizeof (kdb_net.rpkt.seq))
