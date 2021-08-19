/*	@(#)ldd.h	2.0	03/20/90	(c) 1990 NeXT	
 *
 * ldd.h - kernel prototypes used by loadable device drivers
 *
 * HISTORY
 * 16-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Removed a lot of stuff that's defined in other header files.  Eventually
 *	this file should either go away or contain only imports of other files.
 *
 * 20-Mar-90	Doug Mitchell at NeXT
 *	Created.
 *
 */

#ifndef	_LDD_
#define _LDD_

#import <sys/types.h>
#import <sys/buf.h>
#import <sys/printf.h>
#import <sys/kernel.h>
#import <sys/uio.h>
#import <kern/kalloc.h>
#import <kern/sched_prim.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/xpr.h>
#import <next/autoconf.h>
#import <next/us_timer.h>
#import <nextdev/disk.h>
#import <nextdev/dma.h>

typedef int (*PFI)();

int physio(int (*strat)(), struct buf *bp, dev_t dev, int rw, 
	unsigned (*mincnt)(), struct uio *uio, int blocksize);
void biodone(struct buf *bp);
void biowait(struct buf *bp);

int suser();
int copyin(void *p1, void *p2, int size);
int copyout(void *p1, void *p2, int size);
void bcopy(void *src, void *dest, int length);
void bzero(void *bp, int size);
char *strcat(char *s1, char *s2);

u_short checksum_16 (u_short *wp, int shorts);
int sdchecklabel(struct disk_label *dlp, int blkno);

int sleep(void *chan, int pri);
void wakeup(void *chan);
void psignal(struct proc *p, int sig);

void untimeout(int (*fun)(), void *arg);
void timeout(int (*fun)(), void *arg, int time);

int probe_rb(void *addrs);

void dma_close(struct dma_chan *dcp);

kern_return_t vm_map_delete(vm_map_t map, 
	vm_offset_t start, 
	vm_offset_t end);
kern_return_t vm_map_pageable(vm_map_t map, vm_offset_t start, vm_offset_t end,
	boolean_t new_pageable);
#endif	_LDD_
