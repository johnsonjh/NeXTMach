/*	@(#)load_unld.c	2.0	25/06/90	(c) 1990 NeXT	*/

/* 
 * load_unld.c - load/unload functions for loadable DOS file system
 *
 * HISTORY
 * 25-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h> 
#import <sys/printf.h>
#import <sys/port.h>
#import <sys/vnode.h>
#import <sys/vfs.h>
#import <sys/mount.h>
#import <sys/errno.h>
#import <kern/queue.h>
#import <nextdev/ldd.h>
#import <next/loadable_fs.h>
#import <nextdos/msdos.h>
#import <nextdos/next_proto.h>
#import <nextdos/dosdbg.h>
#import <kern/mfs.h>
#import <vm/vm_param.h>

void strcpy(char *s1, char *s2);
void bcopy(void *s1, void *s2, int len);

static int msdos_load();
static int msdos_unload();
static void msd_empty_q(queue_head_t *qhp, boolean_t valid);

#ifdef	DEBUG
static int unload_force = 1;	/* allow unload of mounted fs */
static int msdos_dump();
static int msdos_purge();
static int msdos_dump_finode();
#endif	DEBUG

#define INIT_ON_LOAD	1	/* */

/*
 * Called when we are loaded into kernel memory space. Actual initialization 
 * occurs in msdos_server in DEBUG version.
 */
int msdos_announce(int dummy)
{
	dbg_load(("DOS File System: LOADED\n"));
#ifdef	INIT_ON_LOAD
	return(msdos_load());
#else	INIT_ON_LOAD
	return(0);
#endif	INIT_ON_LOAD
}

/*
 * Called on reception of message for us. 
 */
int msdos_server(msg_header_t *msg, int dummy)
{
	struct load_fs_msg *lfmp = (struct load_fs_msg *)msg;
	
	dbg_load(("DOS File System: Message Received\n"));
	switch(lfmp->lfm_header.msg_id) {
	    case LFM_ID:
	    	/*
		 * Normal "initialize" message
		 */
		lfmp->lfm_status = msdos_load();
		break;
		
#ifdef	DEBUG
	    case LFM_DUMP_MOUNT:
	    	lfmp->lfm_status = msdos_dump();
	    	break;
	    case LFM_TERMINATE:
	    	lfmp->lfm_status = msdos_unload();
		dbg_load(("DOS File System: Unloaded\n"));
	    	break;
	    case LFM_PURGE:
		lfmp->lfm_status = msdos_purge();
		break;
	    case LFM_DUMP_LOCAL:
		lfmp->lfm_status = msdos_dump_finode();
		break;
	    case LFM_ENABLE_TRACE:
	    	dos_dbg_vop = 1;
	    	dos_dbg_vfs = 1;
		dos_dbg_utils = 1;
		lfmp->lfm_status = LFM_STAT_GOOD;
		break;
	    case LFM_DISABLE_TRACE:
	    	dos_dbg_vop = 0;
		dos_dbg_rw = 0;
		dos_dbg_utils = 0;
		dos_dbg_vfs = 0;
		lfmp->lfm_status = LFM_STAT_GOOD;
		break;
#endif	DEBUG
	    default:
		dbg_load(("dos_server: BOGUS MESSAGE (id = %d)\n", 
			lfmp->lfm_header.msg_id));
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

static int msdos_load()
{
	/*
	 * Load ourself into vfssw
	 */
	dbg_load(("DOS File System: Initializing\n"));
	if(vfssw[MOUNT_PC].vsw_name != NULL) {
		printf("dos_server: VFS Switch Already In Use\n");
		return(LFM_STAT_NOSPACE);
	}
	vfssw[MOUNT_PC].vsw_ops  = &dos_vfsops;
	vfssw[MOUNT_PC].vsw_name = "msdos";
	queue_init(&msdnode_free_list);
	msdnode_free_cnt = 0;
	/*
	 * Initialize buffer pool lock and free list lock.
	 */
	buff_lockp = lock_alloc();
	lock_init(buff_lockp, TRUE);

	/*
 	 * Initialize paging buffer and lock.
	 */
	pageio_buf = kalloc(PAGE_SIZE);
	pageio_buf_lockp = lock_alloc();
	lock_init(pageio_buf_lockp, TRUE);
	
	printf("DOS File System: Initialized\n");
	return(LFM_STAT_GOOD);
}

int msdos_port_death(port_name_t port)
{
	dbg_load(("DOS File System: Port Death Notification\n")); /* huh? */
	return(0);
}

int msdos_terminate(int dummy)
{
	int drivenum;
	ddi_t ddip = dos_drive_info;
	
	printf("DOS File System: Terminating\n"); 
	for(drivenum=0; drivenum<HIGHESTDRIVE; drivenum++, ddip++) {
		if(ddip->dev) {
#ifdef	DEBUG
			if(unload_force)
				break;
#endif	DEBUG
			
			printf("DOS File system still mounted\n");
			return(EINVAL);				
		}	
	}
	return(msdos_unload());
}

static int msdos_unload()
{
	ddi_t ddip = dos_drive_info;
	int drivenum;
	char diskname[3];
	/*
	 * Free resources associated with each open drive.
	 */
	for(drivenum=0; drivenum<HIGHESTDRIVE; drivenum++, ddip++) {
		if(ddip->dev == 0)
			continue;
		/*
		 * Free all active msdnodes for this volume.
		 */
		msd_empty_q(&ddip->msdnode_valid_list, TRUE);
		diskname[0] = 'A' + drivenum;
		diskname[0] = ':';
		diskname[0] = '\0';
		pc_dskclose(diskname);
		
	} /* for each drive */
		
	/*
	 * Free all free msdnodes and locks.
	 */
	msd_empty_q(&msdnode_free_list, FALSE);
	lock_free(buff_lockp);
	
	/*
	 * Null out our entry in vfssw
	 */
	vfssw[MOUNT_PC].vsw_ops  = (struct vfsops *)0;
	vfssw[MOUNT_PC].vsw_name = (char *)0;
	dbg_load(("DOS File System: UNLOADED\n"));
	return(0);

} /* msdos_unload() */

static void msd_empty_q(queue_head_t *qhp, boolean_t valid)
{
	msdnode_t mnp, mnp_next;
	/*
	 * Free all free msdnodes in *qhp.
	 */
	mnp = (msdnode_t)qhp->next;
	while(!queue_end(qhp, (queue_t)mnp)) {
		mnp_next = (msdnode_t)mnp->m_link.next;
		mnp->m_vnode.v_count = 0;
		if(valid)
			msdnode_free(mnp);
		else {
			vm_info_free(MTOV(mnp));
			kfree(mnp, sizeof(struct msdnode));
		}
		mnp = mnp_next;
	}
}

#ifdef	DEBUG
/*
 * Debugging functions invoked via lfs utility
 */
static int msdos_dump() 
{
	/*
	 * Dump list of mounted VFS's and all active msdnodes to console
	 */
	int drivenum;
	char drive;
	msdnode_t mnp, mnp_next;
	ddi_t ddip = dos_drive_info;
	char fullpath[EMAXPATH];
	int found_fs=0;
	int found_mnp;
	
	for(drivenum=0; drivenum<HIGHESTDRIVE; drivenum++, ddip++) {
		if(ddip->dev == 0)
			continue;
		found_fs++;
		drive = 'A' + drivenum;
		printf("DOS drive %c:\n", drive);
		
		/*
		 * Scan thru list of msdnodes mounted on this drive.
		 */
		mnp = (msdnode_t)ddip->msdnode_valid_list.next;
		found_mnp=0;
		while(!queue_end(&ddip->msdnode_valid_list, (queue_t)mnp)) {
		    found_mnp++;
		    mnp_next = (msdnode_t)mnp->m_link.next;
		    /*
		     * Print pathname and statistics
		     */
		    printf("   %s\n", mnp->m_path);
		    printf("\tmsdnode 0x%x  m_parent 0x%x  m_fd %d  "
			    "m_flags 0x%x  v_count %d\n",
			    mnp, mnp->m_parent, mnp->m_fd, mnp->m_flags,
			    mnp->m_vnode.v_count);
		    if(mnp->m_msddp) {
			printf("\tdir offset %d\n", 
				mnp->m_msddp->offset);
		    }
		    else {
			/*
			 * Regular file...
			 */
			printf("\tm_fp %d vnode_size %d vm_info 0x%x",
				mnp->m_fp, vm_get_vnode_size(MTOV(mnp)),
				mnp->m_vnode.vm_info);
			if(mnp->m_fd >= 0) {
				PC_FILE *pfile;
				
				pfile = pc_fd2file(mnp->m_fd, YES);
				if(pfile == NULL) 
				    printf(" fsize UNAVAILABLE\n");
				else
				    printf(" fsize %d\n",
				    	pfile->pobj->finode->fsize);

			}
			else
				printf("\n");
		    }
		    /*
		     * Next in list
		     */
		    mnp = mnp_next;
		}
		if(!found_mnp)
			printf("***NO MSDNODES ACTIVE ON THIS DRIVE***\n");
		else
			printf("\n");
	} /* for each drive */
	if(!found_fs)
		printf("***NO MOUNTED FILE SYSTEMS***\n");
	return(LFM_STAT_GOOD);
}

/*
 * Purge all vnodes except root to allow clean unmount
 */
static int msdos_purge()
{
	ddi_t ddip = dos_drive_info;
	msdnode_t mnp, mnp_prev, mnp_root;
	int drivenum;
	queue_t qhp;

	for(drivenum=0; drivenum<HIGHESTDRIVE; drivenum++, ddip++) {
		if(ddip->dev == 0)
			continue;
		qhp = &ddip->msdnode_valid_list;
		
		/*
		 * Free all active msdnodes for this volume except for 
		 * root.
		 */
		mnp_root = (msdnode_t)qhp->next;	/* root = first */
		if(queue_end(qhp, (queue_t)mnp_root))
			continue;			/* no root */ 
		mnp = (msdnode_t)qhp->prev;		/* last */
		/*
		 * scan backwards thru the valid list, end at root 
		 */
		while(mnp != mnp_root) {
			mnp_prev = (msdnode_t)mnp->m_link.prev;
			mnp->m_vnode.v_count = 0;
			msdnode_free(mnp);
			mnp = mnp_prev;
		}
	} /* for each drive */
	printf("...DOS vnodes purged\n");
	return(0);
}

extern FINODE *inoroot;

static int msdos_dump_finode()
{
	/*
	 * Dump EBS DOS FINODE list
	 */
	 
	FINODE *fip = inoroot;
	char fname[MSD_NAME_LEN + 1];
	char fext[MSD_EXT_LEN + 1];
	
	printf("finode dump:\n");
	while(fip) {
		bcopy(fip->fname, fname, MSD_NAME_LEN);
		fname[MSD_NAME_LEN] = '\0';
		bcopy(fip->fext, fext, MSD_EXT_LEN);
		fext[MSD_EXT_LEN] = '\0';
		printf("finode 0x%x fname <%s> fext <%s>\n",
			fip, fname, fext);
		printf("       fcluster %d opencount %d fsize %d\n", 
			fip->fcluster, fip->opencount, fip->fsize);
		fip = fip->pnext;
	}
}
#endif	DEBUG

/* end of load_unld.c */