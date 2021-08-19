/*	@(#)globals.c	2.0	25/06/90	(c) 1990 NeXT	*/

/* 
 * globals.c -- next-specific global variables for loadable DOS file system
 *
 * HISTORY
 * 25-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <kernserv/kern_server_types.h>
#import <nextdos/msdos.h>
#import <header/pcdisk.h>

kern_server_t msdos_ks_va;	
kern_server_t msdos_ks_var3;	
kern_server_t msdos_ks_var;			/* used only by kern_server */
kern_server_t msdos_ks_var1;			/* something keep writing 
						 * a 0x00000009 to the word
						 * after msdos_ks_var! FIXME!
						 */
kern_server_t msdos_ks_var2;	
struct dos_drive_info dos_drive_info[HIGHESTDRIVE];	
						/* maps dos driveno to unix
						 * info */
/* queue_head_t  foo_queue;			/* something keep writing 
						 * a 0x00000009 to the word
						 * after msdos_ks_var! FIXME!
						 */
/* queue_head_t  bar_queue; */
queue_head_t msdnode_free_list;			/* linked list of free msdnodes
						 */
/* queue_head_t  foobar_queue; */
int msdnode_free_cnt;				/* # of msdnodes in 
						 * msdnode_free */
lock_t buff_lockp;				/* locks access to block buffer 
						 * pool */
char *pageio_buf;				/* for copying pageout/pagein()
						 * data */
lock_t pageio_buf_lockp;			/* locks use of pageio_buf */

/*
 * Filename character mapping definitions
 */
struct msd_char_mapping msd_char_map [] = 
{
/*	    in_char   out_char	*/
/*	    (UNIX)    (DOS)	*/

	{     	' ',	'_'	},	/* WSM's spaces ==> '_' */
	{	'\0',	'\0'	}
};

/*
 * Debugging filters
 */
#ifdef	DEBUG
int dos_dbg_all    = 0;			/* show everything */
int dos_dbg_vfs    = 0;			/* show vfs ops */
int dos_dbg_vop    = 0;			/* show vnode ops */
int dos_dbg_load   = 1;			/* show load/unload stuff */
int dos_dbg_io     = 0;			/* show I/O */
int dos_dbg_err    = 1;			/* show errors */
int dos_dbg_malloc = 0;			/* show malloc/free activity */
int dos_dbg_utils  = 0;			/* show activity in dos_utils.c */
int dos_dbg_api    = 0;			/* EBS DOS calls */
int dos_dbg_rw     = 0;			/* read/write activity */
int dos_dbg_tmp    = 0;			/* for temporary debug */
int dos_dbg_cache  = 0;			/* buffer cache activity */
int dos_dbg_stat   = 0;			/* pc_gfirst/gdone/gnext */
#endif	DEBUG