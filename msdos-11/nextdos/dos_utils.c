/*	@(#)dos_utils.c	2.0	29/06/90	(c) 1990 NeXT	*/

/* 
 * dos_utils.c - Miscellaneous utlities for MSDOS file system
 *
 * HISTORY
 * 29-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <sys/vnode.h>
#import <kern/lock.h>
#import <kern/mfs.h>
#import <kern/queue.h>
#import <sys/errno.h>
#import <sys/time.h>
#import <sys/vfs.h>
#import <nextdev/ldd.h>
#import <header/pcdisk.h>
#import <header/posix.h>
#ifdef	MACH_ASSERT
#undef	MACH_ASSERT
#endif	MACH_ASSERT
#define MACH_ASSERT 1
#import <kern/assert.h>
#import <nextdos/msdos.h>
#import <nextdos/next_proto.h>
#import <nextdos/dosdbg.h>

extern int strncmp(void *s1, void *s2, int len);
extern int strlen(void *s1);
extern void *strcpy(void *s1, void *s2);
extern void mfs_cache_clear();
extern void vm_object_cache_clear();

static void msd_map_chars(char *s1);

/*
 * Create & return a new msdnode. msdnode is locked and referenced on return.
 * Parent is also held (if not request for new root msdnode).
 */
msdnode_t msdnode_new(msdnode_t pmnp,		/* parent. Must be locked
						 * on entry. pmnp = NULL for 
						 * creating root msdnode). */
	UCOUNT fcluster,			/* DOS cluster # */
	vm_size_t vnode_size,			/* file size */
	boolean_t is_dir) 
{
	msdnode_t mnp;
	struct vfs *vfsp;
#ifdef	DEBUG
	if(pmnp) {
		ASSERT(pmnp->m_msddp);
		dbg_utils(("msdnode_new; parent = %s\n", pmnp->m_path));
	}
	else {
		dbg_utils(("msdnode_new; ROOT\n"));
	}
#endif	DEBUG
	if(msdnode_free_cnt) {
		mnp = (msdnode_t)queue_first(&msdnode_free_list);
		queue_remove(&msdnode_free_list, mnp, msdnode_t, m_link);
		msdnode_free_cnt--;
	}
	else {
		mnp = kalloc(sizeof(struct msdnode));
		if(mnp == NULL) {
			printf("new_msdnode: kalloc() returned NULL\n");
			return(mnp);
		}
	}
	bzero(mnp, sizeof(struct msdnode));
	mnp->m_lockp = lock_alloc();
	lock_init(mnp->m_lockp, TRUE);
	mn_lock(mnp);
	mnp->m_fd = -1;				/* no DOS open yet */
	mnp->m_parent = pmnp ? pmnp : mnp;	/* Root's parent = root */
	mnp->m_path[0] = '\0';
	mnp->m_vnode.v_data = (caddr_t)mnp;
	mnp->m_vnode.v_op = &dos_vnodeops;
	mnp->m_vnode.vm_info = VM_INFO_NULL;
	mnp->m_vnode.v_count = 1;
	mnp->m_fcluster = fcluster;
	vm_info_init(&mnp->m_vnode);
	/*
	 * MFS maintains vnode_size after we init it...EBS maintains its
	 * own version, but we have to report vnode_size in getattr()...
	 */
	vm_set_vnode_size(MTOV(mnp), vnode_size);
	vm_set_close_flush(MTOV(mnp), FALSE);
	if(pmnp) {
		/*
		 * Hold this mnp's parent until mnp is freed in msdnode_free().
		 */
		VN_HOLD(MTOV(pmnp));
		/*
		 * Root msdnode has no parent; caller has to initialize v_vfsp.
		 */
		vfsp = (MTOV(pmnp))->v_vfsp;
	}
	else
		vfsp = NULL;
	VN_INIT(MTOV(mnp), 
		vfsp, 				/* file system - same as
						 * parent's FS */
		is_dir ? VDIR : VREG, 		/* only two types supported */
		0);				/* no special devices here */
	if(vfsp) {
		ddi_t ddip;
		
		ddip = (ddi_t)vfsp->vfs_data;
		queue_enter(&ddip->msdnode_valid_list, 
			mnp, 
			msdnode_t, 
			m_link);
	}
	if(is_dir) {
		msd_dir_t msddp;
		
		msddp = kalloc(sizeof(struct msd_dir));
		if(msddp == NULL) {
			printf("new_msdnode: kalloc() returned NULL\n");
			return(0);
		}
		bzero(msddp, sizeof(struct msd_dir));
		mnp->m_msddp = msddp;
	}
	return(mnp);
}

/*
 * Free an msdnode. It's either kfree'd or placed on msdnode_free_list.
 */
void msdnode_free(msdnode_t mnp)
{
	ddi_t ddip;
	struct vnode *vp = MTOV(mnp);
	
	dbg_utils(("msdnode_free\n"));
	ASSERT(vp->v_count == 0);
	/*
	 * lose a reference to the parent directory (if this is not root).
	 */
	if(!(vp->v_flag & VROOT))
		VN_RELE(MTOV(mnp->m_parent));
	if(mnp->m_msddp) 
		msd_free_dir(mnp);
	ddip = (ddi_t)(vp->v_vfsp->vfs_data);
	queue_remove(&ddip->msdnode_valid_list, mnp, msdnode_t, m_link);
	lock_free(mnp->m_lockp);

	/*
	 * Flush the vnode from the file map cache and free the vm_info struct.
	 */
	vm_info_free(MTOV(mnp));
	if(msdnode_free_cnt >= MN_FREE_MAX) 
		kfree(mnp, sizeof(struct msdnode));
	else {
		dbg_tmp(("msdnode_free: adding 0x%x to free list; cnt = %d\n",
			 mnp, msdnode_free_cnt));
		queue_enter(&msdnode_free_list, mnp, msdnode_t, m_link);
		msdnode_free_cnt++;
	}		
}

void msd_free_dir(msdnode_t mnp)
{
	ASSERT(mnp->m_msddp);
	if(mnp->m_msddp->offset) 
		pc_gdone(&mnp->m_msddp->dstat);
	kfree(mnp->m_msddp, sizeof(struct msd_dir));
	mnp->m_msddp = 0;
}

/*
 * Search msdnode_valid_list for given pathname. If it exists,
 * reference it and return it; else return NULL.
 */
msdnode_t msdnode_search(char *pathname)
{
	msdnode_t cmnp, cmnp_next;
	ddi_t ddip;
	queue_head_t *qhp;
	char drive_letter;
	
	dbg_utils(("msdnode_search: pathname %s (", pathname));
	drive_letter = pathname[0];
	ASSERT((drive_letter >= 'A') && (drive_letter < ('A' + HIGHESTDRIVE)));
	ddip = &dos_drive_info[drive_letter - 'A'];
	qhp = &ddip->msdnode_valid_list;
	cmnp = (msdnode_t)queue_first(qhp);
	while(!queue_end(qhp, (queue_entry_t)cmnp)) {
		cmnp_next = (msdnode_t)cmnp->m_link.next;
		if(strncmp(cmnp->m_path, pathname, EMAXPATH) == 0) {
		 
		 	/*
			 * Found it.
			 */
			VN_HOLD(MTOV(cmnp));
			dbg_utils(("SUCCESS)\n"));
			return(cmnp);  
		}
		/*
		 * else next in queue.
		 */
		cmnp = cmnp_next;
	}
	dbg_utils(("FAILURE)\n"));
	return(NULL);
}

/*
 * Generate a full path name given a directory and an element. Returns
 * ENAMETOOLONG if result would be longer than EMAXPATH.
 */
int msd_genpath(msdnode_t pmnp,		/* parent directory's msdnode */
	char *nm,			/* name within directory */
	char *path,			/* path RETURNED here */
	boolean_t translate)		/* translate from UNIX to DOS */
{
	char dos_nm[FNAME_LEN];
	
	ASSERT(pmnp->m_msddp);
	if(translate)
		msd_unixfname_to_dos(nm, dos_nm);
	else {
		if(strlen(nm) > (FNAME_LEN - 1))
			return(ENAMETOOLONG);
		strcpy(dos_nm, nm);
	}
	/*
	 * one extra byte for trailing NULL, one for possible '/'
	 */
	if((strlen(pmnp->m_path) + strlen(dos_nm)) >= (EMAXPATH-2))
		return(ENAMETOOLONG);
	strcpy(path, pmnp->m_path);
	/*
	 * a file in the root directory looks like "A:FOO", with no '/'.
	 */
	if(!(pmnp->m_vnode.v_flag & VROOT))
		strcat(path, "/");
	strcat(path, dos_nm);
	return(0);
}

/*
 * Convert EBS global p_errno to UNIX errno
 */
extern INT p_errno;

int msd_errno() {
	switch(p_errno) {
	    case PEBADF:
	    	return(EBADF);
	    case PENOENT:
	    	return(ENOENT);
	    case PEMFILE:
	    	return(EMFILE);
	    case PEEXIST:
	    	return(EEXIST);
	    case PEACCES:
	    	return(EACCES);
	    case PEINVAL:
	    	return(EINVAL);
	    case PENOSPC:
	    	return(ENOSPC);
	    case PENOTEMPTY:
	    	return(ENOTEMPTY);
	    default:
	    	return(EINVAL);
	}
}

/*
 * Get a DSTAT for a file given the file's parent's mnp and the pathname.
 * Returns ENAMETOOLONG, ENOENT, or 0. DSTAT info is in pmnp->m_msddp->dstat.  
 * Parent's msdnode should be locked on entry. We use the parent's dstat
 * for the DOS directory ops. We leave this valid for caller who need
 * things from *dstat.pobj. Caller must pc_gdone() this dstat when finished.
 */
int msd_lookup(msdnode_t pmnp,			/* parent directory; DSTAT
						 * RETURNED in 
						 * pmnp->m_msddp->dstat */
	char *pathname)				/* full path to stat */
{
	BOOL ertn;
	int rtn;
	
	ASSERT(pmnp->m_msddp);
	dbg_utils(("msd_lookup: parent 0x%x filename %s\n", pmnp, pathname));
	/*
	 * Always start stat'ing from the first dir entry.
	 */
	if(pmnp->m_msddp->offset) {
		pc_gdone(&pmnp->m_msddp->dstat);
		pmnp->m_msddp->offset = 0;
	}
	ertn = pc_gfirst(&pmnp->m_msddp->dstat, pathname);
	if(ertn == NO)
		rtn = msd_errno();
	else 
		rtn = 0;
		
	/*
	 * Leave directory in clean state on error.
	 */
	if(rtn) {
		pc_gdone(&pmnp->m_msddp->dstat);
		pmnp->m_msddp->offset = 0;
	}
	return(rtn);
}	

/*
 * Convert DOS filename to UNIX format. Strip off spaces in the DOS-format 
 * name; ensure result is NULL terminated. The output filename will only 
 * contain a '.' if the DOS 'fext' is non-trivial.
 */
 
#define D2U_LOWER_CASE	1		/* */

void msd_dosfname_to_unix(DSTAT *dstat_p, char *oname)
{
	char *ip, *op;
	int is_dot=0;
	int i;
	
	dbg_utils(("msd_dosfname_to_unix: filename <%s> fext <%s>\n",
		dstat_p->fname, dstat_p->fext));
	ip = dstat_p->fname;
	op = oname;
	for(i=0; i<MSD_NAME_LEN; i++) {
		if((*ip == ' ') || (*ip == '\0'))
			break;
		*oname++ = *ip++;
	}
	ip = dstat_p->fext;
	for(i=0; i<MSD_NAME_LEN; i++) {
		if((*ip == ' ') || (*ip == '\0'))
			break;
		if(!is_dot) {
			/*
			 * first char of fext
			 */
			*oname++ = '.';
			is_dot++;
		}
		*oname++ = *ip++;
	}
#ifdef	D2U_LOWER_CASE
	msd_str_to_lower(op);
#endif	D2U_LOWER_CASE
	*oname = '\0';
}

/*
 * Make a best attempt at generating a DOS filename from a unix filename.
 * Rules:
 * -- oname is one of two formats:
 *    	filename.ext
 *	   OR
 *    	filename
 *    '.ext' is only generated if there is a '.' in iname.
 *    filename has at most 8 characters.
 *    ext has at most 3 characters.
 * -- characters from iname[0] will be copied to oname until
 *    -- '.' is encountered, or
 *    -- end of iname is encountered, or
 *    -- MSD_NAME_LEN chars are copied
 * -- A '.' will appear in oname iff at least one '.' is in oname. Up to
 *    MSD_EXT_LEN chars will be copied from iname, starting after the first
 *    '.' in iname, until
 *    -- '.' is encountered, or
 *    -- end of iname is encountered, or
 *    -- MSD_EXT_LEN chars are copied
 * -- All lowercase letters are converted to upper case.
 * -- All characters in msd_char_map[] will be mapped to the corresponding 
 *    replacement.
 */

void msd_unixfname_to_dos(char *iname,		/* unix filename */
	char *oname)				/* filename RETURNED */
{
	int i;
	char *ip=iname;
	char *op=oname;
		
	/*
	 * Copy from iname to oname, up to '.', end of oname, or
	 * MSD_NAME_LEN characters.
	 */
	for(i=0; i<MSD_NAME_LEN; i++) {
		if(*ip == '.')	
			break;
		if(*ip == '\0')		/* end of input */
			goto done;
		*op++ = *ip++;
	}
	
	/*
	 * Search for '.' in iname
	 */
	while(*ip) {
		if(*ip == '.') 
			break;
		ip++;
	}
	if(*ip == '\0')
		goto done;
					
	/*
	 * A '.' was found (ip points to the '.'.) Copy over MSD_EXT_LEN
	 * characters to oname. 
	 */
	*op++ = *ip++;
	for(i=0; i<MSD_EXT_LEN; i++) {
		if(*ip == '.')	
			break;			/* only one of these allowed */
		if(*ip == '\0')			/* end of input */
			goto done;
		*op++ = *ip++;
	}
done:
	*op = '\0';
	/*
	 * Convert to upper case and perform character mapping.
	 */
	ASSERT(strlen(oname) <= FNAME_LEN);
	msd_str_to_upper(oname);
	msd_map_chars(oname);
	dbg_utils(("msd_unixfname_to_dos: iname <%s>  oname <%s>\n", 
		iname, oname));

} /* msd_unixfname_to_dos() */

/*
 * Convert string (like a UNIX filename) to upper/lower case.
 */
void msd_str_to_upper(char *s1)
{
	for(; *s1; s1++) {
		if((*s1 >= 'a') && (*s1 <= 'z'))
			*s1 = 'A' + (*s1 - 'a');
	}
}
void msd_str_to_lower(char *s1)
{
	for(; *s1; s1++) {
		if((*s1 >= 'A') && (*s1 <= 'Z'))
			*s1 = 'a' + (*s1 - 'A');
	}
}


static void msd_map_chars(char *s1)
{
	struct msd_char_mapping *cmp;
	
	/*
	 * Go thru the whole map for each character
	 */
	for( ; *s1; s1++) {
		for(cmp=msd_char_map; cmp->in_char; cmp++) {
			if(*s1 == cmp->in_char)
				*s1 = cmp->out_char;
			
		}
	}
}

DDRIVE *msd_getdrive(int drivenum)
{
	char path[3];
	DDRIVE *pdr;
	COUNT driveno;		/* EBS version - probably same as drivenum */
	
	strcpy(path, "A:");
	path[0] += drivenum;
	if (!pc_parsedrive(&driveno, path )) {
		dbg_err(("msd_getdrive(%d): INVALID DRIVE\n", drivenum));
		return(NULL);
	}
	if (!(pdr = pc_drno2dr(driveno))) {
		dbg_err(("msd_getdrive(%d): INVALID DRIVE\n", drivenum));
		return(NULL);
	}
	return(pdr);
}

UCOUNT msd_fd_to_cluster(PCFD fd)
{
	PC_FILE *pfile;

	pfile = pc_fd2file(fd, YES);
	if(pfile == NULL) {
		dbg_err(("msd_fd_to_cluster: COULD NOT GET FILE POINTER\n"));
		return(0);
	}
	return(pfile->pobj->finode->fcluster);
}

int msd_extend(msdnode_t mnp, LONG newlength)
{
	/*
	 * Set file length to newlength. File must be open. Returns new
	 * file pointer on success, else returns -1.
	 *
	 * If newlength is after EOF, we have to pad with zeroes from EOF until
	 * the desired offset. Ufs and DOS just don't behave the same here...
	 */
	PC_FILE *pfile;
	int new_fp;
	int bytes_written;
	int rtn;
	int fsize;
	
	dbg_vop(("msd_extend: path <%s> newlength %d\n", 
		mnp->m_path, newlength));
	if(mnp->m_fd < 0) {
		dbg_err(("msd_extend: FILE NOT OPEN\n"));
		return(-1);
	}
	pfile = pc_fd2file(mnp->m_fd, YES);
	if(pfile == NULL) {
		dbg_err(("msd_extend: COULD NOT GET FILE POINTER\n"));
		return(-1);
	}
	fsize = pfile->pobj->finode->fsize;
	if(newlength > fsize) {
		char *pad_buf;
		int pad_count = newlength - fsize;
		
		dbg_utils(("msd_extend: paddding %d bytes @ %d\n",
			pad_count, fsize));
		pad_buf = kalloc(pad_count);
		bzero(pad_buf, pad_count);
		new_fp = po_lseek(mnp->m_fd, 0, PSEEK_END);
		if(new_fp < 0) {
		    	dbg_err(("msd_extend: LSEEK TO EOF FAILED\n"));
		    	return(-1);
		}
		bytes_written = po_write(mnp->m_fd, (UTINY *)pad_buf, 
			pad_count);
		if(bytes_written != pad_count) {
		    	rtn = msd_errno();
		    	dbg_err(("msd_extend: po_write() returned %d\n",
				rtn));
			return(-1);
		}	
		/*
		 * EOF = filepointer = newlength.
		 */
		kfree(pad_buf, pad_count);
	}
	else if(newlength == fsize) {
		return(fsize);	
	}
	else {
#ifdef	DO_TRUNCATE
		new_fp = po_lseek(mnp->m_fd, newlength, PSEEK_SET);
		if(new_fp != newlength) {
			dbg_err(("dos_pageout: LSEEK FAILED (newlength "
				"%d, new_fp %d)\n", newlength,
				new_fp));
			return(-1);
		}
#else	DO_TRUNCATE
		dbg_err(("msd_extend: BOGUS LENGTH\n"));
		return(-1);
#endif	DO_TRUNCATE
	}
	mnp->m_fp = newlength;
	return(newlength);
}

boolean_t dos_isdot(char *name)
{
	/*
	 * returns TRUE if *char is "." or ".."; else FALSE. These two strings
	 * may be terminated by either NULL or space.
	 */
	if(name[0] == '.') {
		if((name[1] == '\0') || (name[1] == ' '))
		   	return(TRUE);
		if(name[1] == '.') {
			if((name[2] == '\0') || (name[2] == ' '))
		   		return(TRUE);
		}
	}
	return(FALSE);
}

PCFD dos_do_open(TEXT *name, UCOUNT flag, UCOUNT mode) {

	PCFD rtn_fd;
	
	dbg_utils(("dos_do_open name %s\n", name));
	rtn_fd = po_open(name, flag, mode);
	if(rtn_fd >= 0)
		return(rtn_fd);
	if(p_errno != PEMFILE) 
		return(rtn_fd);
	/*
	 * Too many files open. This means that mfs is hoarding them. Flush 'em
	 * all out and try one more time.
	 */
#ifdef	DEBUG
	printf("dos_do_open: clearing MFS cache\n");
#endif	DEBUG
	mfs_cache_clear();		/* clear the MFS cache */
	vm_object_cache_clear();
	return(po_open(name, flag, mode));
}

#ifdef	DEBUG

int check_cluster(PC_FILE *pfile, char *called_from)
/*
 * returns 0 if all clusters for pfile are consistent with 
 * pfile->pobj->finode->fsize.
 */
{
	int fatcount;
	int filesize = pfile->pobj->finode->fsize;
	int fatcount_sb;
	UCOUNT cluster = pfile->pobj->finode->fcluster;
	DDRIVE *pdrive = pfile->pobj->pdrive;
	
	fatcount_sb = (filesize + pfile->pobj->pdrive->bytespcluster - 1) / 
		       pfile->pobj->pdrive->bytespcluster;
	fatcount = 0;
	while (cluster) {
		cluster = pc_clnext(pdrive, cluster);
		fatcount ++;
	}
	
	if (fatcount != fatcount_sb) {
		dbg_err(("check_cluster: called from %s\n", called_from));
		dbg_err(("\tCLUSTER CORRUPTED!!!!\n"));
		dbg_err(("\tfatcount: %d  fatcount_sb: %d filesize: %d\n", 
			fatcount, fatcount_sb, filesize));
		dbg_err(("\tbcc1: %d  bdirty: %d  fptr: %d\n", 
			pfile->bccl, pfile->bdirty, pfile->fptr));
		/*
		 * Dump the bogus fat chain
		 */
		fatcount = 0;
		cluster = pfile->pobj->finode->fcluster;
		while (cluster != 0) {
			dbg_err(("\tcluster list #: %d  cluster #: %d\n", 
				fatcount,  cluster));
			cluster = pc_clnext(pdrive,  cluster);
			fatcount ++;
		}
		return(1);
	}
	return(0);
}
#endif	DEBUG
/* end of dos_utils.c */