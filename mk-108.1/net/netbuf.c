/* 
 * Copyright (C) 1990 by NeXT, Inc., All Rights Reserved
 *
 */

/* 
 * Network Buffer handling
 * These should be used instead of mbufs. Currently, they are just
 * wrappers around mbufs, but we hope to flush mbufs one day. Third parties
 * must use this API, or risk breakage after an OS upgrade.
 *
 * HISTORY
 * 09-Apr-90  Bradley Taylor (btaylor) at NeXT, Inc.
 *	Created.
 */

#import <sys/types.h>
#import <sys/param.h>
#import <sys/mbuf.h>
#import <net/netbuf.h>


static void
nb_alloc_free(
	      void *orig_data
	      )
{
	int orig_size = *(unsigned *)orig_data;

	kfree(orig_data, orig_size);
}

netbuf_t
nb_alloc(
	 unsigned data_size
	 )
{
	void *data;
	netbuf_t nb;
	unsigned *orig_data;
	unsigned orig_size;

	orig_size = data_size + sizeof(orig_data[0]);
	orig_data = (unsigned *)kalloc(orig_size);
	if (orig_data == NULL) {
		return (NULL);
	}
	orig_data[0] = orig_size;
	data = (void *)&orig_data[1];
	nb = nb_alloc_wrapper(data, data_size, 
			      nb_alloc_free, 
			      (void *)orig_data);
	if (nb == NULL) {
		nb_alloc_free((void *)orig_data);
		return (NULL);
	}
	return (nb);
}

netbuf_t
nb_alloc_wrapper(
		 void *data,
		 unsigned data_size,
		 void data_free(void *arg),
		 void *data_free_arg
		 )
{
	struct mbuf *m;

	m = mclgetx(data_free, data_free_arg, data, data_size, M_DONTWAIT);
	if (m == NULL) {
		return (NULL);
	}
	return ((netbuf_t)m);
}

char *
nb_map(netbuf_t nb)
{
	return (mtod(((struct mbuf *)nb), char *));
}

void
nb_free(netbuf_t nb)
{
	m_free((struct mbuf *)nb);
}

/*
 * Frees the netbuf wrapper, but not the data held 
 */
void
nb_free_wrapper(netbuf_t nb)
{
	struct mbuf *m = (struct mbuf *)nb;

	m->m_off = MMINOFF;	/* XXX: to fool mfree */
	m->m_len = 0;		/* XXX: to fool mfree */
	m_free(m);
}


unsigned
nb_size(netbuf_t nb)
{
	return (((struct mbuf *)nb)->m_len);
}

int
nb_read(netbuf_t nb, unsigned offset, unsigned size, void *target)
{
	struct mbuf *m = (struct mbuf *)nb;
	void *data;
	
	if (offset + size > m->m_len) {
		return (-1);
	}
	data = mtod(m, void *);
	bcopy(data + offset, target, size);
	return (0);
}

int
nb_write(netbuf_t nb, unsigned offset, unsigned size, void *source)
{
	struct mbuf *m = (struct mbuf *)nb;
	void *data;
	
	if (offset + size > m->m_len) {
		return (-1);
	}
	data = mtod(m, void *);
	bcopy(source, data + offset, size);
	return (0);
}

int
nb_shrink_top(netbuf_t nb, unsigned size)
{
	struct mbuf *m = (struct mbuf *)nb;
	
	m->m_len -= size;
	m->m_off += size;
	return (0); /* XXX should error check */
}

int
nb_grow_top(netbuf_t nb, unsigned size)
{
	struct mbuf *m = (struct mbuf *)nb;
	
	m->m_len += size;
	m->m_off -= size;
	return (0); /* XXX should error check */
}

int
nb_shrink_bot(netbuf_t nb, unsigned size)
{
	struct mbuf *m = (struct mbuf *)nb;
	
	m->m_len -= size;
	return (0); /* XXX should error check */
}


int
nb_grow_bot(netbuf_t nb, unsigned size)
{
	struct mbuf *m = (struct mbuf *)nb;
	
	m->m_len += size;
	return (0); /* XXX should error check */
}


