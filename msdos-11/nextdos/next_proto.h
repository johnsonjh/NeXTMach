/*	@(#)next_proto.h	2.0	27/06/90	(c) 1990 NeXT	*/

/* 
 * Prototypes of global NeXT_specific functions in MSDOS file system.
 *
 * HISTORY
 * 27-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <sys/time.h>
#import <header/pcdisk.h>
#import <nextdos/msdos.h>

/*
 * in nextdos/unix_rw.c
 */
int unix_open(ddi_t ddip);
void unix_close(ddi_t ddip);
int unix_read(ddi_t ddip, 
	int block, 
	int block_count, 
	char *addrs);
int unix_write(ddi_t ddip, 
	int block, 
	int block_count, 
	char *addrs);
/*
 * in nextdos/dos_utils.c
 */
msdnode_t msdnode_new(msdnode_t pmnp, UCOUNT fcluster, vm_size_t vnode_size,
	boolean_t is_dir);
void msdnode_free(msdnode_t mnp);
void msd_free_dir(msdnode_t mnp);
msdnode_t msdnode_search(char *pathname);
int msd_genpath(msdnode_t pmnp, char *nm, char *path, boolean_t translate);
void msd_str_to_upper(char *s1);
void msd_str_to_lower(char *s1);
int msd_errno();
int msd_lookup(msdnode_t pmnp, char *pathname);
void msd_dosfname_to_unix(DSTAT *dstat_p, char *oname);
void msd_unixfname_to_dos(char *iname, char *oname);
DDRIVE *msd_getdrive(int drivenum);
UCOUNT msd_fd_to_cluster(PCFD fd);
int msd_extend(msdnode_t mnp, LONG newlength);
boolean_t dos_isdot(char *name);
PCFD dos_do_open(TEXT *name, UCOUNT flag, UCOUNT mode);

/*
 * others
 */
int tm_to_sec(struct tm *tmp);
void msd_time_to_timeval(UCOUNT time, UCOUNT date, struct timeval *tvp);

/* end of next_proto.h */