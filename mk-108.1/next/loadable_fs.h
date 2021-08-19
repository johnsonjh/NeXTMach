/*	@(#)loadable_fs.h	2.0	26/06/90	(c) 1990 NeXT	*/

/* 
 * loadable_fs.h - message struct for loading and initializing loadable
 *		   file systems.
 *
 * HISTORY
 * 26-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#ifndef	_LOADABLE_FS_
#define _LOADABLE_FS_

#import <sys/types.h>
#import <sys/message.h>

/*
 * This message is used in an RPC between a lightweight app and a loaded
 * kernel server. 
 */
struct load_fs_msg {
	msg_header_t	lfm_header;
	msg_type_t	lfm_status_t;
	u_int		lfm_status;	/* status returned by server */
};

/*
 * msg_id values for load_fs_msg
 */
#define LFM_ID			0x123456	/* base */
#define LFM_DUMP_MOUNT		(LFM_ID+1)	/* dump active FS and VNODE 
						 * list */
#define LFM_TERMINATE		(LFM_ID+2)	/* terminate */
#define LFM_PURGE		(LFM_ID+3)	/* purge all vnodes except 
						 * root */
#define LFM_DUMP_LOCAL		(LFM_ID+4)	/* dump local FS info */
#define LFM_ENABLE_TRACE	(LFM_ID+5)	/* enable debug trace */
#define LFM_DISABLE_TRACE	(LFM_ID+6)	/* disable debug trace */
					 
/*
 * lfm_stat values 
 */
#define LFM_STAT_GOOD		0	/* file system loaded OK */
#define LFM_STAT_NOSPACE	1	/* no space in vfssw */
#define LFM_STAT_BADMSG		2	/* bad message received */
#define LFM_STAT_UNDEFINED	3

/*
 * Constants for Loadabls FS Utilities (in "/usr/FileSystems")
 *
 * Example of a /usr/filesystems directory
 *
 * /usr/filesystems/msdos.fs/msdos.util		utility with which WSM 
 *							communicates
 * /usr/filesystems/msdos.fs/msdos.name 	"MS_DOS Floppy" 
 * /usr/filesystems/msdos.fs/msdos_reloc	actual loadable filesystem
 * /usr/filesystems/msdos.fs/msdos.openfs.tiff	"open folder" icon 
 * /usr/filesystems/msdos.fs/msdos.fs.tiff	"closed folder" icon 
 */
#define FS_DIR_LOCATION		"/usr/filesystems"	
#define FS_DIR_SUFFIX		".fs"
#define FS_UTIL_SUFFIX		".util"
#define FS_OPEN_SUFFIX		".openfs.tiff"
#define FS_CLOSED_SUFFIX	".fs.tiff"
#define FS_NAME_SUFFIX		".name"

/*
 * .util program commands - all sent in the form "-p" or "-m" ... as argv[1].
 */
#define FSUC_PROBE		'p'	/* probe FS on argv[2] */
#define FSUC_MOUNT		'm'	/* mount FS on argv[2] on mount point
					 * argv[3] */
#define FSUC_REPAIR		'r'	/* repair ('fsck') FS on argv[2] */
#define FSUC_UNMOUNT		'u'	/* unmount FS on argv[2] */
#define FSUC_MOUNT_FORCE	'M'	/* like FSUC_MOUNT, but proceed even on
					 * error */
/*
 * Return codes from .util program
 */
#define FSUR_RECOGNIZED		(-1)	/* positive response to FSUC_PROBE */
#define FSUR_UNRECOGNIZED	(-2)	/* negative response to FSUC_PROBE */
#define FSUR_IO_SUCCESS		(-3)	/* mount, unmount, repair succeeded */
#define FSUR_IO_FAIL		(-4)	/* unrecoverable I/O error */
#define FSUR_IO_UNCLEAN		(-5)	/* mount failed, file system not clean 
					 */
#define FSUR_INVAL		(-6)	/* invalid argument */
#define FSUR_LOADERR		(-7)	/* kern_loader error */

#endif	_LOADABLE_FS_
