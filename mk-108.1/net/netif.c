/* 
 * Copyright (C) 1990 by NeXT, Inc., All Rights Reserved
 *
 */

/*
 * Network Interface Support.
 *
 * These should be used instead of BSD's ifnet structure. Currently, it is just
 * a wrapper around ifnets, but we hope to flush ifnets one day. Third parties
 * must use this API, or risk breakage after an OS upgrade.
 *
 * HISTORY
 * 09-Apr-90  Bradley Taylor (btaylor) at NeXT, Inc.
 *	Created.
 */

#import <sys/types.h>
#import <sys/param.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <sys/mbuf.h>
#import <net/netif.h>
#import <net/if.h>
#import <sys/ioctl.h>

extern struct ifnet *ifnet;
extern int ifqmaxlen;

typedef struct virtual_dispatcher {
	if_attach_func_t vif_attach;
	void *vif_private;
	struct virtual_dispatcher *vif_next;
} virtual_dispatcher_t;

static virtual_dispatcher_t *if_dispatchers;

const char IFCONTROL_SETFLAGS[] = "setflags";
const char IFCONTROL_SETADDR[] = "setaddr";
const char IFCONTROL_GETADDR[] = "getaddr";
const char IFCONTROL_AUTOADDR[] = "autoaddr";
const char IFCONTROL_UNIXIOCTL[] = "unix-ioctl";

int 
if_output(
	  netif_t netif, 
	  netbuf_t packet, 
	  void *addr
	  )
{
	struct ifnet *ifp = (struct ifnet *)netif;

	if (ifp->if_output == NULL) {
		return (ENXIO);
	} else {
		return (ifp->if_output(netif, packet, addr));
	}
}

int 
if_control(
	   netif_t netif, 
	   const char *command,
	   void *data
	   )
{
	struct ifnet *ifp = (struct ifnet *)netif;

	if (ifp->if_control == NULL) {
		return (ENXIO);
	} else {
		return (ifp->if_control(netif, command, data));
	}
}

int 
if_ioctl(
	 netif_t netif, 
	 unsigned command,
	 void *data
	 )
{
	struct ifnet *ifp = (struct ifnet *)netif;
	struct ifreq *ifr = (struct ifreq *)data;
	if_ioctl_t ioctl_stuff;

	if (ifp->if_control == NULL) {
		return (ENXIO);
	} 
	switch (command) {
	case SIOCAUTOADDR:
		return (ifp->if_control(netif, IFCONTROL_AUTOADDR,
					(void *)&ifr->ifr_ifru));
					
		
	case SIOCSIFADDR:
		/*
		 * XXX: IP calls this incorrectly with an ifaddr
		 * struct instead of an ifreq struct
		 */
		return (ifp->if_control(netif, IFCONTROL_SETADDR,
					(void *)data));
		
	case SIOCGIFADDR:
		return (ifp->if_control(netif, IFCONTROL_GETADDR,
					(void *)&ifr->ifr_ifru));
		
	case SIOCSIFFLAGS:
		return (ifp->if_control(netif, IFCONTROL_SETFLAGS,
					(void *)&ifr->ifr_ifru));
		
	default:
		ioctl_stuff.ioctl_command = command;
		ioctl_stuff.ioctl_data = data;
		return (ifp->if_control(netif, IFCONTROL_UNIXIOCTL, 
					(void *)&ioctl_stuff));
	}
}

int
if_init(
	netif_t netif
	)
{
	struct ifnet *ifp = (struct ifnet *)netif;

	if (ifp->if_init == NULL) {
		return (ENXIO);
	} else {
		return (ifp->if_init(netif));
	}
}

netbuf_t
if_getbuf(
	  netif_t netif
	  )
{
	struct ifnet *ifp = (struct ifnet *)netif;

	if (ifp->if_getbuf == NULL) {
		return (NULL);		/* should never happen */
	} else {
		return (ifp->if_getbuf(netif));
	}
}

void *
if_private(
	   netif_t netif
	   )
{
	return (((struct ifnet *)netif)->if_private);
}

unsigned
if_unit(
	netif_t netif
	)
{
	return (((struct ifnet *)netif)->if_unit);
}

const char *
if_name(
	netif_t netif
	)
{
	return (((struct ifnet *)netif)->if_name);
}

const char *
if_type(
	netif_t netif
	)
{
	return (((struct ifnet *)netif)->if_type);
}

unsigned
if_mtu(
       netif_t netif
       )
{
	return (((struct ifnet *)netif)->if_mtu);
}

unsigned
if_flags(
	 netif_t netif
	 )
{
	return (((struct ifnet *)netif)->if_flags);
}

unsigned
if_opackets(
	    netif_t netif
	    )
{
	return (((struct ifnet *)netif)->if_opackets);
}

unsigned
if_ipackets(
	    netif_t netif
	    )
{
	return (((struct ifnet *)netif)->if_ipackets);
}

unsigned
if_oerrors(
	   netif_t netif
	   )
{
	return (((struct ifnet *)netif)->if_oerrors);
}

unsigned
if_ierrors(
	   netif_t netif
	   )
{
	return (((struct ifnet *)netif)->if_ierrors);
}

unsigned
if_collisions(
	      netif_t netif
	      )
{
	return (((struct ifnet *)netif)->if_collisions);
}

void
if_flags_set(
	     netif_t netif,
	     unsigned flags
	     )
{
	((struct ifnet *)netif)->if_flags = flags;
}

void
if_opackets_set(
		netif_t netif,
		unsigned opackets
		)
{
	((struct ifnet *)netif)->if_opackets = opackets;
}

void
if_ipackets_set(
		netif_t netif,
		unsigned ipackets
		)
{
	((struct ifnet *)netif)->if_ipackets = ipackets;
}

void
if_oerrors_set(
		netif_t netif,
		unsigned oerrors
		)
{
	((struct ifnet *)netif)->if_oerrors = oerrors;
}

void
if_ierrors_set(
		netif_t netif,
		unsigned ierrors
		)
{
	((struct ifnet *)netif)->if_oerrors = ierrors;
}

void
if_collisions_set(
		netif_t netif,
		unsigned collisions
		)
{
	((struct ifnet *)netif)->if_collisions = collisions;
}

static void
pingvirtuals(
	     netif_t rnetif
	     )
{
	virtual_dispatcher_t *vdp;
	struct ifnet *newifp;
	
	for (vdp = if_dispatchers; vdp != NULL; vdp = vdp->vif_next) {
		vdp->vif_attach(vdp->vif_private, rnetif);
	}
}

netif_t 
if_attach(
	  if_init_func_t init_func, 
	  if_input_func_t input_func,
	  if_output_func_t output_func,
	  if_getbuf_func_t getbuf_func,
	  if_control_func_t control_func,
	  const char *name,
	  unsigned unit,
	  const char *type,
	  unsigned mtu,
	  unsigned flags,
	  netif_class_t class,
	  void *private
	  )
{
	struct ifnet *ifp;
	struct ifnet **ifpp = &ifnet;

	ifp = (struct ifnet *)kalloc(sizeof(struct ifnet));
	bzero((void *)ifp, sizeof(struct ifnet));
	ifp->if_name = (char *)name;
	ifp->if_type = (char *)type;
	ifp->if_unit = unit;
	ifp->if_mtu = mtu;
	ifp->if_flags = flags;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_init = init_func;
	ifp->if_output = output_func;
	ifp->if_control = control_func;
	ifp->if_input = input_func;
	ifp->if_getbuf = getbuf_func;
	ifp->if_private = private;
	ifp->if_class = class;
	
	for (ifpp = &ifnet; 
	     *ifpp != NULL && (*ifpp)->if_class >= class;
	     ifpp = &(*ifpp)->if_next) {
	}
	ifp->if_next = *ifpp;
	*ifpp = ifp;	
	if (ifp->if_class == NETIFCLASS_REAL) {
		pingvirtuals((netif_t)ifp);
	}
	return ((netif_t)ifp);
}



void
if_registervirtual(
		   if_attach_func_t vif_attach,
		   void *vif_private
		   )
{
	virtual_dispatcher_t *vdp;
	virtual_dispatcher_t **vdpp = &if_dispatchers;
	struct ifnet *rifp;
	struct ifnet *ifp;

	vdp = (virtual_dispatcher_t *)kalloc(sizeof(*vdp));
	vdp->vif_attach = vif_attach;
	vdp->vif_private = vif_private;
	vdp->vif_next = NULL;

	for (vdpp = &if_dispatchers; *vdpp != NULL; 
	     vdpp = &(*vdpp)->vif_next) {
	}
	*vdpp = vdp;

	for (rifp = ifnet; rifp != NULL; rifp = rifp->if_next) {
		if (rifp->if_class == NETIFCLASS_REAL) {
			vif_attach(vif_private, (netif_t)rifp);
		}
	}
}


int
if_handle_input(
		netif_t netif,
		netbuf_t data,
		void *extra
		)
{
	struct ifnet *ifp = (struct ifnet *)netif;
	
	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		if ((ifp->if_input != NULL) &&
		    ifp->if_class != NETIFCLASS_REAL &&
		    (ifp->if_input((netif_t)ifp, netif, data, extra) == 0)) {
			return (0);
		}
	}
	nb_free(data);
	return (EAFNOSUPPORT);
}



int
mbuf_read(
	  struct mbuf *mb,
	  char *target,
	  unsigned offset,
	  unsigned len
	  )
{
	unsigned moff;
	unsigned thislen;
	unsigned skip;
	struct mbuf *m;
	char *data;

	moff = 0;
	for (m = mb; m != NULL; m = m->m_next) {
		if (moff <= offset && offset < moff + m->m_len) {
			data = mtod(m, char *);
			skip = (offset - moff);
			/* assertion: 0 <= skip  && skip < m->m_len */
			thislen = m->m_len - skip;
			if (thislen > len) {
				thislen = len;
			}
			bcopy(data + skip, target, thislen);
			target += thislen;
			offset += thislen;
			len -= thislen;
			if (len == 0) {
				return (0);
			}
		}
		moff += m->m_len;
	}
	return (-1);
}

if_output_mbuf(
	       struct ifnet *ifp,
	       struct mbuf *m,
	       struct sockaddr *addr
	       )
{	
	netbuf_t nb;
	int len;
	struct mbuf *n;
	char *map;

	len = 0;
	for (n = m; n != NULL; n = n->m_next) {
		len += n->m_len;
	}
	if (len > ifp->if_mtu) {
		m_freem(m);
		return (EMSGSIZE);
	}
	nb = ifp->if_getbuf(ifp);
	if (nb == NULL) {
		m_freem(m);
		return (ENOBUFS);
	}
	map = nb_map(nb);
	mbuf_read(m, map, 0, len);
	nb_shrink_bot(nb, nb_size(nb) - len);
	m_freem(m);
	return (ifp->if_output(ifp, nb, addr));
}






