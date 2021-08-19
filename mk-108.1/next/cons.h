/* 
 * Copyright (c) 1987 NeXT, Inc.
 */ 
#if	defined (KERNEL) && !defined(KERNEL_FEATURES)
#import <gdb.h>
#else	defined (KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined (KERNEL) && !defined(KERNEL_FEATURES)

#import <sys/types.h>

struct tty	cons;
struct tty	*cons_tp;		/* current console device */
#if	GDB
struct tty	dbug;
struct tty	*dbug_tp;		/* debugging device */
#endif	GDB

