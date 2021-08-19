/*	if_bus.h	6.1	83/07/29	*/

/* Some ethernet constants */
#define	HDR_SIZE	sizeof (struct ether_header)
#define	FCS_SIZE	4
#define	PARANOIA	32
#define	BUF_SIZE	(HDR_SIZE + ETHERMTU + FCS_SIZE + \
			DMA_ENDALIGNMENT*2 + PARANOIA)

/*
 * Ethernet buffer DMA headers.
 */
typedef struct	en_dmabuf {
	struct dma_hdr	end_dhp;	/* Must be first */
	vm_offset_t	end_bp;
} *en_dmabuf_t;
#define	end2dh(end) ((struct dma_hdr *)(end))
#define dh2end(dh) ((en_dmabuf_t)(dhp))

/*
 * Ethernet buffers
 *	Wired down memory used for Ethernet DMA.
 */
typedef struct enbuf {
	union  {
		struct enbuf 	*Uenb_next;
		char 		Uenb_data[BUF_SIZE];
	} Uenbuf;
} *enbuf_t;
#define enb_next Uenbuf.Uenb_next
#define enb_data Uenbuf.Uenb_data

#ifdef 	KERNEL
netbuf_t if_rbusget(en_dmabuf_t, int);
#endif
