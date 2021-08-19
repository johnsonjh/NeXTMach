/*	@(#)dosdbg.h	2.0	16/07/90	(c) 1990 NeXT	*/

/* 
 * dosdbg.h
 *
 * HISTORY
 * 16-Jul-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/printf.h>

/*
 * Debugging macros.
 */
#ifdef	DEBUG
extern int dos_dbg_all;
extern int dos_dbg_vfs;
extern int dos_dbg_vop;
extern int dos_dbg_load;
extern int dos_dbg_io;
extern int dos_dbg_err;
extern int dos_dbg_malloc;
extern int dos_dbg_utils;
extern int dos_dbg_api;
extern int dos_dbg_rw;
extern int dos_dbg_tmp;
extern int dos_dbg_cache;
extern int dos_dbg_stat;

#define dbg_all(x)				\
	{					\
		if(dos_dbg_all)			\
			printf x;		\
	}
#define dbg_vfs(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_vfs)	\
			printf x;		\
	}
#define dbg_vop(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_vop)	\
			printf x;		\
	}
#define dbg_load(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_load)	\
			printf x;		\
	}
#define dbg_io(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_io)	\
			printf x;		\
	}
#define dbg_err(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_err)	\
			printf x;		\
	}
#define dbg_malloc(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_malloc)	\
			printf x;		\
	}
#define dbg_utils(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_utils)	\
			printf x;		\
	}
#define dbg_api(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_api)	\
			printf x;		\
	}
#define dbg_rw(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_rw)	\
			printf x;		\
	}
#define dbg_tmp(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_tmp)	\
			printf x;		\
	}
#define dbg_cache(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_cache)	\
			printf x;		\
	}
#define dbg_stat(x)				\
	{					\
		if(dos_dbg_all || dos_dbg_stat)	\
			printf x;		\
	}
#else	DEBUG
#define dbg_all(x)
#define dbg_vfs(x)
#define dbg_vop(x)
#define dbg_load(x)
#define dbg_io(x)
#define dbg_malloc(x)
#define dbg_utils(x)
#define dbg_api(x)
#define dbg_rw(x)
#define dbg_tmp(x)
#define dbg_cache(x)
#define dbg_stat(x)
#define dbg_err(x) printf x;
#endif	DEBUG
