/*
 * Copyright (C) 1990 by NeXT, Inc., All Rights Reserved
 */

/*
 * Network driver for running IP protocol over Ethernet. It is the glue
 * between the IP protocol (which knows nothing about hardware) and the
 * ethernet drivers (which know nothing about IP protocols).
 *
 * HISTORY
 * 30-Jul-90  John Seamons (jks) at NeXT
 *	Import gdb.h!
 *
 * 09-Apr-90  Bradley Taylor (btaylor) at NeXT, Inc.
 *	Created. 
 */
#import <gdb.h>

#import <sys/types.h>
#import <sys/param.h>
#import <sys/socket.h>
#import <sys/errno.h>
#import <net/netif.h>
#import <net/netbuf.h>
#import <netinet/in.h>
#import <net/etherdefs.h>

#if GDB
extern const char IFCONTROL_SETIPADDRESS[];
#endif

static const char IFTYPE_IP[] = "Internet Protocol";

#define ETYPEOFFSET 12

static const verbose = 1;

typedef struct venip_private {
	struct ether_addr vp_enaddr;
	struct in_addr vp_ipaddr;
	netif_t rifp;
} venip_private_t;

inline venip_private_t *
VENIP_PRIVATE(netif_t ifp)
{
	return ((venip_private_t *)if_private(ifp));
}

inline u_char *
VENIP_ENADDRP(netif_t ifp)
{
	return (VENIP_PRIVATE(ifp)->vp_enaddr.ether_addr_octet);
}

inline struct in_addr
VENIP_IPADDR(netif_t ifp) 
{
	return (VENIP_PRIVATE(ifp)->vp_ipaddr);
}

inline netif_t
VENIP_RIF(netif_t ifp)
{
	return (VENIP_PRIVATE(ifp)->rifp);
}


static int
venip_output(
	     netif_t ifp,
	     netbuf_t nb,
	     void *addr
	     )
{
	struct sockaddr *dst = (struct sockaddr *)addr;
	struct ether_header eh;
	struct in_addr idst;
	int off;
	int usetrailers;
	netif_t rifp = VENIP_RIF(ifp);
	int error;

	switch (dst->sa_family) {
	case AF_UNSPEC:
		bcopy(dst->sa_data, &eh, sizeof(eh));
		break;
	case AF_INET:
		idst = ((struct sockaddr_in *)dst)->sin_addr;
		if (!arpresolve(ifp, VENIP_ENADDRP(ifp), VENIP_IPADDR(ifp),
				nb, &idst, 
				&eh.ether_dhost, &usetrailers)) {
			return (0);	/* if not yet resolved */
		}
		/*
		 * XXX: trailers not supported for output
		 */
		eh.ether_type = htons(ETHERTYPE_IP);
		break;
	default:
		nb_free(nb);
		return (EAFNOSUPPORT);
	}
	nb_grow_top(nb, ETHERHDRSIZE);
	nb_write(nb, ETYPEOFFSET, sizeof(eh.ether_type), 
		 (void *)&eh.ether_type);
	error = if_output(rifp, nb, (void *)&eh.ether_dhost);
	if (error == 0) {
		if_opackets_set(ifp, if_opackets(ifp) + 1);
	} else {
		if_oerrors_set(ifp, if_oerrors(ifp) + 1);
	}
	return (error);
}

static int
venip_control(
	      netif_t ifp,
	      const char *command,
	      void *data
	      )
{
	netif_t rifp = VENIP_RIF(ifp);
	unsigned ioctl_command;
	void *ioctl_data;
	int s;
	struct sockaddr_in *sin = (struct sockaddr_in *)data;

	if (strcmp(command, IFCONTROL_AUTOADDR) == 0) {
		/*
		 * Automatically set address
		 */
		if (sin->sin_family != AF_INET) {
			return (EAFNOSUPPORT);
		}
		/*
		 * XXX: netif_t passed as struct ifnet *
		 */
		return (in_bootp(ifp, sin, VENIP_ENADDRP(ifp)));

	} else if (strcmp(command, IFCONTROL_SETADDR) == 0) {
		/*
		 * Manually set address
		 */
		if (sin->sin_family != AF_INET) {
			return (EAFNOSUPPORT);
		}
		s = splimp();
		if_flags_set(ifp, if_flags(ifp) | IFF_UP);
		if_init(rifp);
#if GDB
		/*
		 * Inform ethernet driver of our IP address
		 */
		if_control(rifp, IFCONTROL_SETIPADDRESS, 
			   (void *)&sin->sin_addr);
#endif
		VENIP_PRIVATE(ifp)->vp_ipaddr = sin->sin_addr;
		if (!(if_flags(ifp) & IFF_AUTOCONF)) {
			/*
			 * XXX: netif_t passed as struct ifnet *
			 */
			arpwhohas(ifp, VENIP_ENADDRP(ifp), 
				  VENIP_IPADDR(ifp),
				  &sin->sin_addr);
		}
		splx(s);
	} else {
		/*
		 * Let lower layer handle
		 */
		return (if_control(rifp, command, data));
	}
	return (0);

}


/*
 * There are two cases:
 * offset = 512 implies...
 *	max data size = 512
 * 	max header size = (1500 - 512) = 988
 *
 * offset = 1024 implies...
 *	max data size = 1024
 * 	max header size = (1500 - 1024) = 976
 *
 * The maximum min buffer is the data size in the first case, 512
 */
#define MAXTRAILERBUF 512

typedef struct trailer_data_t {
	short etype;
	short length;
} trailer_data_t;


/*
 * Fix trailer packets
 */
static void
trailer_fix(
	    netbuf_t nb, 
	    short offset,
	    short hdrsize
	    )
{
	char buf[MAXTRAILERBUF];
	char *map = nb_map(nb);

	if (offset > MAXTRAILERBUF) {
		/*
		 * Save copy of header
		 */
		nb_read(nb, ETHERHDRSIZE + offset + sizeof(trailer_data_t),
			hdrsize, &buf);

		/*
		 * Shift data down by size of header (trashes original
		 * header)
		 */
		bcopy(&map[ETHERHDRSIZE], &map[ETHERHDRSIZE + hdrsize],
		      offset);

		/*
		 * Copy saved header to front
		 */
		bcopy(buf, &map[ETHERHDRSIZE], hdrsize);
	} else {
		/*
		 * Save copy of data
		 */
		nb_read(nb, ETHERHDRSIZE, offset, &buf);

		/*
		 * Copy header to front (trashes original data)
		 */
		bcopy(&map[ETHERHDRSIZE + offset + sizeof(trailer_data_t)],
		      &map[ETHERHDRSIZE], hdrsize);
		
		/*
		 * Copy saved data to shifted location
		 */
		bcopy(buf, &map[ETHERHDRSIZE + hdrsize], offset);
	}
	nb_shrink_bot(nb, sizeof(trailer_data_t));
}



static netbuf_t
venip_getbuf(
	     netif_t ifp
	     )
{
	netif_t rifp = VENIP_RIF(ifp);
	netbuf_t nb;

	nb = if_getbuf(rifp);
	if (nb == NULL) {
		return (NULL);
	}
	nb_shrink_top(nb, ETHERHDRSIZE);
	return (nb);
}

static int
venip_input(
	    netif_t ifp,
	    netif_t rifp,
	    netbuf_t nb,
	    void *extra
	    )
{
	short etype;
	short offset;
	short size;
	trailer_data_t trailer_data;

	if (VENIP_RIF(ifp) != rifp) {
		return (EAFNOSUPPORT);
	}
	nb_read(nb, ETYPEOFFSET, sizeof(etype), &etype);
	etype = htons(etype);
	if (etype >= ETHERTYPE_TRAIL &&
	    etype < ETHERTYPE_TRAIL + ETHERTYPE_NTRAILER) {
		offset = (etype - ETHERTYPE_TRAIL) * 512;
		if (offset == 0 || (ETHERHDRSIZE + offset + 
				    sizeof(trailer_data) >=
				    nb_size(nb))) {
			return (EAFNOSUPPORT);
		}
		nb_read(nb, ETHERHDRSIZE + offset, sizeof(trailer_data),
			&trailer_data);
		etype = htons(trailer_data.etype);
		if (etype != ETHERTYPE_IP &&
		    etype != ETHERTYPE_ARP) {
			return (EAFNOSUPPORT);
		}
		size = htons(trailer_data.length);
		if (ETHERHDRSIZE + offset + size > nb_size(nb)) {
			return (EAFNOSUPPORT);
		}
		trailer_fix(nb, offset, size - sizeof(trailer_data));
	}
	switch (etype) {
	case ETHERTYPE_IP:
		nb_shrink_top(nb, ETHERHDRSIZE);
		if_ipackets_set(ifp, if_ipackets(ifp) + 1);
		inet_queue(ifp, nb);
		break;

	case ETHERTYPE_ARP:
		if_ipackets_set(ifp, if_ipackets(ifp) + 1);
		if (!(if_flags(ifp) & IFF_AUTOCONF) ) {
			nb_shrink_top(nb, ETHERHDRSIZE);
			/*
			 * XXX: netif_t passed as struct ifnet *
			 */
			arpinput(ifp, VENIP_ENADDRP(ifp), VENIP_IPADDR(ifp),
				 nb);
		} else {
			nb_free(nb);
		}
		break;
	default:
		/* 
		 * Do not free buf: let others handle it
		 */
		return (EAFNOSUPPORT);
	}
	return (0);
}

static void
venip_attach(
	     void *private,
	     netif_t rifp
	     )
{
	netif_t ifp;
	const char *name;
	int unit;
	void *ifprivate;

	if (strcmp(if_type(rifp), IFTYPE_ETHERNET) != 0) {
		return;
	}

	ifprivate = (void *)kalloc(sizeof(venip_private_t));
	name = if_name(rifp);
	unit = if_unit(rifp);
	ifp = if_attach(NULL, venip_input, venip_output, venip_getbuf,
			venip_control, name, unit, IFTYPE_IP, ETHERMTU, 
			IFF_BROADCAST, NETIFCLASS_VIRTUAL, ifprivate);
	
	VENIP_PRIVATE(ifp)->rifp = rifp;

	if_control(rifp, IFCONTROL_GETADDR, VENIP_ENADDRP(ifp));

	if (verbose) {
		printf("IP protocol enabled for interface %s%d, type \"%s\"\n", 
		       name, unit, IFTYPE_ETHERNET);
	}
	return;
}

void
venip_config(void)
{
	if_registervirtual(venip_attach, NULL);
}


