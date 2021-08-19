#import <machine/spl.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/mbuf.h>
#import <sys/buf.h>
#import <sys/socket.h>
#import <nextdev/dma.h>
#import <net/if.h>
#import <net/netbuf.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/in_var.h>
#import <netinet/ip.h>
#import <netinet/if_ether.h>
#import <vm/vm_kern.h>

#import <nextif/if_bus.h>

static enbuf_t	en_bufs = NULL;
static int	enbuf_allocated = 0;
static int	enbuf_free = 0;

/*
 * Any packets larger than enbuf_cutoff will have their enbufs "loaned"
 * to a funny mbuf.
 */
static int	enbuf_cutoff = MCLBYTES;

#if	DEBUG
#define STAT_BUCKETS	8
int	rbus_gatherstats = 0;
static	int	stats[STAT_BUCKETS];
#endif	DEBUG

/*
 * Ethernet buffer allocation and management routines.
 */

static void
enbuf_put(void *buf)
{
	int s = splimp();
	enbuf_t enb = (enbuf_t)buf;

	enb->enb_next = en_bufs;
	en_bufs = enb;
	enbuf_free++;
	splx(s);
}

static
enbuf_alloc()
{
	enbuf_t	enb;
	caddr_t limit;
	int i;
	int s;

	s = splimp();
	enb = (enbuf_t)m_clalloc(1, MPG_SPACE, M_DONTWAIT);
	splx(s);
	if (enb) {
		limit = (caddr_t)enb + PAGE_SIZE;
		enb = (enbuf_t)roundup((int)enb, DMA_ENDALIGNMENT);
		while ((caddr_t)enb + sizeof(struct enbuf) <= limit) {
			enbuf_put((void *)enb);
			enbuf_allocated++;
			enb++;
			enb = (enbuf_t)roundup((int)enb, DMA_ENDALIGNMENT);
		}
		return(0);
	}
	return(1);

}

static enbuf_t
enbuf_get()
{
	enbuf_t enb;
	int s = splimp();

	if (en_bufs == NULL && enbuf_alloc()) {
		splx(s);
		return (NULL);
	}
	enb = en_bufs;
	en_bufs = enb->enb_next;
	enbuf_free--;
	splx(s);
	return (enb);
}

netbuf_t
engetbuf(
	 struct ifnet *ifp
	 )
{
	enbuf_t buf;
	netbuf_t nb;

	buf = enbuf_get();
	if (buf == NULL) {
		return (NULL);
	}
	nb = nb_alloc_wrapper((void *)buf, HDR_SIZE + ETHERMTU, 
			      enbuf_put, (void *)buf);
	if (nb == NULL) {
		enbuf_put((void *)buf);
		return (NULL);
	}
	return (nb);
}

/*
 * External routines.
 */
if_busalloc(
	    en_dmabuf_t endp,
	    netbuf_t nb
	    )
{
	pmap_t	pmap;

	if (nb == NULL) {
		endp->end_bp = (vm_offset_t)enbuf_get();
		if (endp->end_bp == 0) {
			return (0);
		}
	} else {
		endp->end_bp = (vm_offset_t)nb_map(nb);
		nb_free_wrapper(nb);
	}
	
	pmap = vm_map_pmap(mb_map);
	end2dh(endp)->dh_start = (char *)pmap_resident_extract(pmap,
							       endp->end_bp);
	return (1);
}


if_busfree(endp)
	en_dmabuf_t endp;
{

	enbuf_put((void *)endp->end_bp);
	end2dh(endp)->dh_start = NULL;
}


netbuf_t
if_rbusget(
	   en_dmabuf_t endp,
	   int totlen
	   )
{
#if	DEBUG
	if (rbus_gatherstats)
	stats[totlen * STAT_BUCKETS / BUF_SIZE]++;
#endif	DEBUG

	return (nb_alloc_wrapper((void *)endp->end_bp, totlen, 
				 enbuf_put, (void *)endp->end_bp));
}


