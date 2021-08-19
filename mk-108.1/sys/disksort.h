/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 * 10-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Created
 */ 

/*
 * Structures necessary for supporting generic disksorting.
 */
#ifndef	_SYS_DISKSORT_
#define	_SYS_DISKSORT_

#import <sys/types.h>
#import <sys/buf.h>
#import <kern/queue.h>

/*
 * Each device must define one of these disksort structures to control
 * the device that it's sorting.  Access to the sorted queue is achieved
 * exclisively through the routines disksort_enter, disksort_first, and
 * disksort_remove.
 */

typedef struct ds_bucket ds_bucket_t;

struct ds_bucket {
	struct buf	*head;
	struct buf	*tail;
	int		sort_key;		// key bucket's sorted on
	int		pri;
	queue_chain_t	link;
};

typedef struct ds_queue ds_queue_t;

struct ds_queue {
	queue_chain_t		ds_link;	// links active volume queues.
	void			*ds_data;	// pointer to allocated data.
	u_int			indirect:1,	// use the indirect version?
				active:2,	// queue active
						// (driver maintained)
				busy:1;		// first entry in use
	queue_head_t		activeq;	// active buckets
	int			navail;		// number of alloc'able buckets
	int			last_loc;	// head position
	int			last_pri;	// last priority
	simple_lock_data_t	lock;
};

/*
 * Structure defining routines to use for disksorting (loadable support).
 */
typedef struct ds_call ds_call_t;

struct ds_call {
		/*
		 * Enter a request into the disk queue.
		 */
	void (*enter)(ds_queue_t *dsq, struct buf *bp);
		/*
		 * Enter a request at the head of the queue.
		 */
	void (*enter_head)(ds_queue_t *dsq, struct buf *bp);
		/*
		 * Enter a request at the tail of the queue.
		 */
	void (*enter_tail)(ds_queue_t *dsq, struct buf *bp);
		/*
		 * Return the address of the first buffer in the queue.
		 */
	struct buf *(*first)(ds_queue_t *dsq);
		/*
		 * Remove a buffer from the queue.  If the first buffer
		 * is removed the queue is marked not busy.
		 */
	struct buf *(*remove)(ds_queue_t *dsq, struct buf *bp);
		/*
		 * Allocate and initialize data needed for this queue.
		 */
	void (*alloc)(ds_queue_t *dsq);
		/*
		 * Free data from this queue.
		 */
	void (*free)(ds_queue_t *dsq);
		/*
		 * True if the service is loaded and ready to go.
		 * False when shutting down or not loaded.
		 */
	u_int available;	// is service available?
};

#if	KERNEL
void disksort_enter(ds_queue_t *dsq, struct buf *bp);
void disksort_enter_head(ds_queue_t *dsq, struct buf *bp);
void disksort_enter_tail(ds_queue_t *dsq, struct buf *bp);
struct buf *disksort_first(ds_queue_t *dsq);
struct buf *disksort_remove(ds_queue_t *dsq, struct buf *bp);
void disksort_init(ds_queue_t *dsq);
void disksort_free(ds_queue_t *dsq);

/*
 * Inlines
 */
static inline void disksort_qbusy(ds_queue_t *dsq)
{
	dsq->busy = 1;
}
static inline void disksort_qidle(ds_queue_t *dsq)
{
	dsq->busy = 0;
}

extern ds_call_t ds_call;
#endif	KERNEL

#endif	_SYS_DISKSORT_
