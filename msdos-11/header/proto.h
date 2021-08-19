/*****************************************************************************
*Filename: PROTO.H - Function prototypes for ms-dos utilities
*					   
*
* EBS - Embedded file manager -
*
* Copyright Peter Van Oudenaren , 1989
* This code may not be redistributed in source or linkable object form
* without the consent of its author.
*
*
*
* Description: 
*	
*
*
*
*
*****************************************************************************/

BOOL pc_alloc_blk(BLKBUFF **, DDRIVE *, BLOCKT);
BLKBUFF *pc_blkpool(DDRIVE *);
VOID pc_free_all_blk(DDRIVE *);
VOID pc_free_buf(BLKBUFF *, BOOL);
BLKBUFF *pc_init_blk(DDRIVE *, BLOCKT);
VOID pc_buffpanic(TEXT *);
PROCL pc_inhibit(VOID);
VOID pc_allow(PROCL);
VOID pc_sleep(VOID *);
VOID pc_wakeup(VOID *);
VOID pc_errout(TEXT *);
BLKBUFF *pc_read_blk(DDRIVE *, BLOCKT);
BOOL pc_write_blk(BLKBUFF *);
DROBJ *pc_allocobj(VOID);
FINODE *pc_alloci(VOID);
VOID pc_dos2inode (FINODE *, DOSINODE *);
BOOL pc_findin( DROBJ *, TEXT *, TEXT *);
DROBJ *pc_fndnode(TEXT *);
VOID pc_free_all_i( DDRIVE *);
VOID pc_freei( FINODE *);
VOID pc_freeobj( DROBJ *);
BLOCKT pc_firstblock( DROBJ *);
DROBJ *pc_get_inode( DROBJ *,  DROBJ *, TEXT *, TEXT *);
DROBJ *pc_get_mom( DROBJ *);
DROBJ *pc_get_root( DDRIVE *);
VOID pc_init_inode(FINODE *, TEXT *, TEXT *, UTINY, UCOUNT, ULONG, DATESTR *);
VOID pc_ino2dos (DOSINODE *, FINODE *);
BOOL pc_insert_inode( DROBJ *,  DROBJ *);
BOOL pc_isadir( DROBJ *);
BOOL pc_isroot( DROBJ *);
BLOCKT pc_l_next_block(DDRIVE *, BLOCKT);
VOID pc_marki( FINODE *, DDRIVE *, BLOCKT, COUNT);
DROBJ *pc_mkchild( DROBJ *);
DROBJ *pc_mknode(TEXT *, UTINY);
BOOL pc_next_block( DROBJ *);
BLKBUFF *pc_read_obj(DROBJ *);
BOOL pc_rmnode( DROBJ *);
FINODE *pc_scani( DDRIVE *, BLOCKT,COUNT);
BOOL pc_update_inode( DROBJ *);
VOID pc_upstat(DSTAT *);
BOOL pc_write_obj(DROBJ *);
BLOCKT pc_cl2sector(DDRIVE *, UCOUNT);
UCOUNT pc_clalloc( DDRIVE *);
UCOUNT pc_clgrow( DDRIVE *, UCOUNT);
UCOUNT pc_clnext( DDRIVE *, UCOUNT);
VOID pc_clrelease( DDRIVE *, UCOUNT);
BOOL pc_clzero(DDRIVE *, UCOUNT);
DDRIVE *pc_drno2dr(COUNT);
BOOL pc_dskfree(COUNT, BOOL);

#ifdef USEFATBUF
BOOL pc_pfinit( DDRIVE *);
VOID pc_pfclose( DDRIVE *);
BOOL pc_pfswap( DDRIVE *, COUNT);
BOOL pc_pfpbyte( DDRIVE *, COUNT, UTINY);
BOOL pc_pfgbyte( DDRIVE *, COUNT, UTINY *);
BOOL pc_pfflush( DDRIVE *);
UCOUNT pc_pfgcind( DDRIVE *);
#endif 

BOOL pc_faxx( DDRIVE *, UCOUNT, UCOUNT *);
BOOL pc_flushfat(COUNT);
BOOL pc_flushfat(COUNT);
VOID pc_freechain( DROBJ *);
BOOL gblk0(UCOUNT, struct pcblk0 *);
LONG pc_ifree(COUNT);
BOOL pc_pfaxx( DDRIVE *, UCOUNT, UCOUNT);
BOOL pc_pfaxx( DDRIVE *, UCOUNT, UCOUNT);
UCOUNT pc_sec2cluster(DDRIVE *, BLOCKT);
UCOUNT pc_sec2index(DDRIVE *, BLOCKT);

#ifdef USEEBMALLOC
void pc_i_free(TEXT *);
static HEADER *morecore(unsigned);
TEXT *pc_i_malloc(UCOUNT);
TEXT *pc_sbrk(int);
#endif 

ULONG to_DWORD ( UTINY *);
UCOUNT to_WORD ( UTINY *);
VOID fr_WORD ( UTINY *, UCOUNT );
VOID fr_DWORD ( UTINY *, ULONG );
BOOL gblock(UCOUNT, BLOCKT, VOID *, COUNT);
VOID *pc_malloc(int);
VOID pc_mfree(VOID *);
BOOL pblock(UCOUNT, BLOCKT, VOID *, COUNT);

#ifdef USEEBPRINTF
INT printf(TEXT *);
TEXT *pc_stoa(short, TEXT *, INT);
TEXT *pc_itoa(INT, TEXT *, INT);
TEXT *pc_ltoa(LONG, TEXT *, INT);
TEXT *pc_strjust(TEXT *, TEXT *, BOOL, INT , TEXT);
VOID pc_putstr(TEXT *);
#endif

DATESTR *pc_getsysdate(DATESTR *);
VOID pr_er_str(TEXT *, TEXT *);
VOID pr_er_putstr(TEXT *);
VOID pr_db_int (TEXT *, INT);
VOID pr_db_str(TEXT *, TEXT *);
VOID pr_db_putstr(TEXT *);
VOID pc_diskabort(TEXT *);
BOOL pc_dskclose(TEXT *);
BOOL pc_dskinit(COUNT);
BOOL pc_dskopen(TEXT *);
LONG pc_free(TEXT *);
DROBJ *pc_get_cwd(DDRIVE *);
BOOL pc_set_cwd(TEXT *);
BOOL pc_pwd(TEXT *, TEXT *);
VOID pc_gdone(DSTAT *);
BOOL pc_gfirst(DSTAT *, TEXT *);
BOOL pc_gnext(DSTAT *);
COUNT pc_getdfltdrvno(VOID);
BOOL pc_idskclose(COUNT);
BOOL pc_isdir(TEXT *);
BOOL pc_mkdir(TEXT *);
BOOL pc_mkfs(COUNT,  FMTPARMS *);
BOOL pc_mv(TEXT *, TEXT *);
BOOL pc_rmdir(TEXT *);
BOOL pc_setdfltdrvno(COUNT);
BOOL pc_unlink(TEXT *);
INT po_close(PCFD);
LONG po_lseek(PCFD, LONG, COUNT);
UCOUNT pc_lastinchain( DROBJ *, UCOUNT);
PCFD po_open(TEXT *, UCOUNT, UCOUNT);
PC_FILE *pc_fd2file(PCFD, BOOL);
PCFD pc_allocfile(VOID);
VOID pc_freefile(PCFD);
VOID pc_free_all_fil( DDRIVE *);
LONG po_read(PCFD,  UTINY *, LONG);
INT po_seteof(PCFD, ULONG);
UCOUNT pc_rd_next( DROBJ *, UTINY *, UCOUNT);
BOOL pc_rd_cluster(UTINY *, DDRIVE *, UCOUNT);
LONG po_write(PCFD,  UTINY *, LONG);
UCOUNT pc_clm_next( DROBJ *, UCOUNT);
BOOL pc_wr_cluster( DROBJ *, UTINY *, UCOUNT);
BOOL pc_fileflush ( PC_FILE *);
BOOL pc_allspace( UTINY *, INT );
VOID copybuff(VOID *, VOID *, INT);
VOID pc_cppad( UTINY *,  UTINY *, INT );
BOOL pc_isdot( UTINY *,  UTINY *);
BOOL pc_isdotdot( UTINY *,  UTINY *);
VOID pc_memfill(VOID *, INT, UTINY);
TEXT *pc_mfile( TEXT *, TEXT *, TEXT *);
TEXT *pc_mpath( TEXT *, TEXT *, TEXT *);
TEXT *pc_parsedrive(COUNT *, TEXT *);
BOOL pc_fileparse(TEXT *, TEXT *, TEXT *);
TEXT *pc_nibbleparse(TEXT *, TEXT *, TEXT *);
BOOL pc_parsepath(TEXT *, TEXT *, TEXT *, TEXT *);
BOOL pc_patcmp( TEXT *,  TEXT *, INT);
VOID pc_str2upper( UTINY *, UTINY *);
VOID pc_strcat( TEXT *, TEXT *);
