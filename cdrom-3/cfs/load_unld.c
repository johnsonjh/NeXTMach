/*	@(#)load_unld.c	2.0	25/06/90	(c) 1990 NeXT	*/

/* 
 * load_unld.c - load/unload functions for loadable CDROM file system
 *
 * HISTORY
 * 25-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h> 
#import <sys/printf.h>
#import <sys/port.h>
#import <sys/param.h>
#import <sys/proc.h>
#import <sys/user.h>
#import <sys/vnode.h>
#import <sys/vfs.h>
#import <sys/mount.h>
#import <sys/errno.h>
#import <kern/queue.h>
#import <nextdev/ldd.h>
#import <kern/mfs.h>
#include <vm/vm_page.h>
#import <next/loadable_fs.h>

#include "cdrom.h"
#include "cfs.h"



void strcpy(char *s1, char *s2);
void bcopy(void *s1, void *s2, int len);

static int cfs_load();

#define FS_MESSAGE_NAME		"CDROM"

/*
 * Called when we are loaded into kernel memory space. 
 */
int cfs_announce(int dummy)
{
	printf("%s File System: LOADED\n", FS_MESSAGE_NAME);
	return(cfs_load());
}

static int cfs_load()
{
	/*
	 * Load ourself into vfssw
	 */
	dprint1("%s File System: Initializing\n", FS_MESSAGE_NAME);
	if(vfssw[MOUNT_CFS].vsw_name != NULL) {
		printf("cfs_server: VFS Switch Already In Use\n");
		return(LFM_STAT_NOSPACE);
	}
	vfssw[MOUNT_CFS].vsw_ops  = &cfs_vfsops;
	vfssw[MOUNT_CFS].vsw_name = "cfs";

	cfs_init();
	printf("%s File System: Initialized\n", FS_MESSAGE_NAME);
	return(LFM_STAT_GOOD);
}

int cfs_port_death(port_name_t port)
{
	dprint1("%s File System: Port Death Notification\n", FS_MESSAGE_NAME);
	return(0);
}

int cfs_terminate(int dummy)
{
	/*
	 * Null out our entry in vfssw & free cache
	 */
	vfssw[MOUNT_CFS].vsw_ops  = (struct vfsops *)0;
	vfssw[MOUNT_CFS].vsw_name = (char *)0;
	cache_free();

	printf("%s File System: UNLOADED\n", FS_MESSAGE_NAME);
	return(0);
}

#ifdef DEBUG
/*
 * Purge all vnodes except root to allow clean unmount
 */
static 	int 	cfs_purge()
{
	return(0);				/* not implemented */
}

static 	int	 cfs_dump_finode()
{
	return(0);
}

static	int	cfs_unload()
{
	return(0);
}

static	int	cfs_dump()
{
	return(0);
}

#endif


/*
 * Called on reception of message for us. 
 */
int cfs_server(msg_header_t *msg, int dummy)
{
	struct load_fs_msg *lfmp = (struct load_fs_msg *)msg;
	
	dprint1("%s File System: Message Received\n", FS_MESSAGE_NAME);
	switch(lfmp->lfm_header.msg_id) {
	    case LFM_ID:
	    	/*
		 * Normal "initialize" message
		 */
		lfmp->lfm_status = cfs_load();
		break;
		
#ifdef	DEBUG
	    case LFM_DUMP_MOUNT:
	    	lfmp->lfm_status = cfs_dump();
	    	break;
	    case LFM_TERMINATE:
	    	lfmp->lfm_status = cfs_unload();
		dprint1("%s File System: Unloaded\n", FS_MESSAGE_NAME);
	    	break;
	    case LFM_PURGE:
		lfmp->lfm_status = cfs_purge();
		break;
	    case LFM_DUMP_LOCAL:
		lfmp->lfm_status = cfs_dump_finode();
		break;
	    case LFM_ENABLE_TRACE:
		lfmp->lfm_status = LFM_STAT_GOOD;
		break;
	    case LFM_DISABLE_TRACE:
		lfmp->lfm_status = LFM_STAT_GOOD;
		break;
#endif	DEBUG
	    default:
		dprint1("cfs_server: BOGUS MESSAGE (id = %d)\n", 
			lfmp->lfm_header.msg_id);
		lfmp->lfm_status = LFM_STAT_BADMSG;
		break;
	}
	/*
	 * Return message as ACK to initiator
	 */
	msg_send(msg, 
		MSG_OPTION_NONE,
		0);	
	return(0);
}

