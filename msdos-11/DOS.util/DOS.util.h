/*	@(#)DOS.util.h	2.0	28/07/90	(c) 1990 NeXT	*/

/* 
 * HISTORY
 * 27-Jul-90	Doug Mitchell at NeXT
 *	Created.
 */

#define RELOCATABLE_FILE	"dosfs_reloc"

#define SERVER_NAME		"dosfs"			/* for kl_util */
#define FS_NAME			"msdos"			/* for mount */
#define MOUNT			"/usr/etc/mount"
#define UMOUNT			"/usr/etc/umount"
/* #define DEBUG			1		/* */
	
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
int kl_com_add(char *path_name, char *server_name);
int kl_com_delete(char *server_name);
int kl_com_load(char *server_name);
int kl_com_unload(char *server_name);
void kl_com_wait();
klc_server_state kl_com_get_state(char *server_name);
