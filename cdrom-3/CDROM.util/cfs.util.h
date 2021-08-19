/*
 * HISTORY
 * 6-Aug-90 Simson L. Garfinkel at NeXT
 *	created
 */ 

#define	FS_NAME			"CDROM"
#define FS_MOUNT_NAME		"cfs"
#define	CDROM_BLOCK_SIZE	2048
#define	RELOCATABLE_FILE	"CDROM_reloc"
#define SERVER_NAME		"CDROM"
#define	MOUNT			"/usr/etc/mount"
#define	UMOUNT			"/usr/etc/umount"

#define FS_LABEL_SUFFIX		".label"

#ifdef	DEBUG
#define dprintf(s)	printf s;
#else	DEBUG
#define dprintf(s)
#endif	DEBUG

/*
 * return codes from kl_com_get_state()
 */
typedef	int klc_server_state;
#define KSS_LOADED	1
#define KSS_ALLOCATED	2
#define KSS_UNKNOWN	3

extern int exit_code;

/*
 * Files from kl_com.c
 */
int kl_com_add(char *path_name,char *server_name);
int kl_com_delete(char *server_name);
int kl_com_load(char *server_name);
int kl_com_unload(char *server_name);
void kl_com_wait();

