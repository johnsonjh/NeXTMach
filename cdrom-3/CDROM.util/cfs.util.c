/*	@(#)cdrom.util.c	2.0	24/07/90	(c) 1990 NeXT	*/

/* 
 * cfs.util.c -- Loadable High Sierra / ISO 9660  file system utility
 *
 * HISTORY
 *  6-Aug-90	Simson L. Garfinkel at NeXT
 *	copied from msdos.util.c	
 * 24-Jul-90	Doug Mitchell at NeXT
 *	Created.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <libc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/dir.h>

#include <errno.h>
#include <tzfile.h>
#import <nextdev/disk.h>

#import "cfs.util.h"
#import <next/loadable_fs.h>
#import <sys/mount.h>

#ifdef DEBUG
#define dprint(x) printf(x)
#define dprint1(x,y) printf(x,y)
#else
#define dprint(x) {}
#define dprint1(x,y) {}
#endif


/*
 * The following code is re-usable for all FS_util programs
 */
void usage(char **argv);

static int fs_probe(char *device);
static int fs_mount(char *device, char *mount_point);
static int fs_repair(char *device);
static int fs_unmount(char *device);
static int fs_fmount(char *device, char *mount_point);

int exit_code=0;
int wait_for_kl=0;

main(int argc, char **argv)
{
	char device_name[80];
	int length;
	
	if((argc < 3) || (argv[1][0] != '-'))
		usage(argv);
	strcpy(device_name, "/dev/r");
	strcat(device_name, argv[2]);
	/*
	 * This can come out when drivers and autodiskmount have been modified
	 * to support a live partition letter in the device name. Until then,
	 * we'll fake it.
	 */
	length = strlen(device_name);
	if((device_name[length-1] < 'a') || (device_name[length-1] > 'z')) {
		switch(device_name[6]) {
		    case 's':
			device_name[length] = 'h';	/* scsi */
			break;
		    case 'f':
			device_name[length] = 'b';	/* floppy */
			break;
		    default:
		    	printf("Unknown device type (%s)\n", argv[2]);
			exit(FSUR_INVAL);
		}
		device_name[length+1] = '\0';
	}
	switch(argv[1][1]) {
	    case FSUC_PROBE:
	    	exit_code = fs_probe(device_name);
		break;
	    case FSUC_MOUNT:
		if(argc<4){
			usage(argv);
		}
	    	exit_code = fs_mount(device_name, argv[3]);
		break;
	    case FSUC_REPAIR:
	    	exit_code = fs_repair(device_name);
		break;
	    case FSUC_UNMOUNT:
	    	exit_code = fs_unmount(device_name);
		break;
	    case FSUC_MOUNT_FORCE:
	    	exit_code = fs_fmount(device_name, argv[3]);
		break;
	    default:
		usage(argv);
	}
	if(wait_for_kl)
		kl_com_wait();
	dprint1("...exit_code = %d\n", exit_code);
	exit(exit_code);
}

void usage(char **argv)
{
	printf("usage: %s action device [mount_point]\n", argv[0]);
	printf("action:\n");
	printf("       -%c (Probe)\n", FSUC_PROBE);
	printf("       -%c (Mount)\n", FSUC_MOUNT);
	printf("       -%c (Repair)\n", FSUC_REPAIR);
	printf("       -%c (Unmount)\n", FSUC_UNMOUNT);
	printf("       -%c (Force Mount)\n", FSUC_MOUNT_FORCE);
	exit(FSUR_INVAL);
}

/*
 * Begin Filesystem-specific code
 */

/* Set the name of this file system */
static	void	fs_set_file(char *which,char *name)
{
	int	fd;
	char	fname[100];
	char	*c;

	/* Remove any trailing white space */

	if(strlen(name)){
		c	= name + strlen(name) - 1;
		while(*c==' ' && c>=name){
			*c = '\000';
			c--;
		}
	}

	strcpy(fname,FS_DIR_LOCATION "/" FS_NAME FS_DIR_SUFFIX "/" FS_NAME);
	strcat(fname,which);

	unlink(fname);				/* erase existing string */

	if(strlen(which)){
		fd	= open(fname, O_CREAT | O_TRUNC | O_WRONLY, 0644);
		if(fd>0){
			write(fd,name,strlen(name));
			close(fd);
		}
		else{
			perror(fname);
		}
	}
}


static int fs_probe(char *device) {

	int 	fd;
	int 	q;
	long	block;
	char buf[CDROM_BLOCK_SIZE];
	struct drive_info di;
	
	fd = open(device, O_RDONLY, 0);
	if(fd <= 0)
		return(FSUR_IO_FAIL);
	/*
	 * Don't verify block size, because we want to be able to read
	 * CDROM file systems from other kinds of disks...
	 */
#ifdef CDROM_ONLY
	if(ioctl(fd, DKIOCINFO, &di)) 
		return(FSUR_IO_FAIL);
	if(di.di_devblklen != CDROM_BLOCK_SIZE)
		return(FSUR_UNRECOGNIZED);
#endif
		
	/* Now scan for the "CDROM" or "CD001" flag.
	 * We get "CDROM" for High Sierra Disks.
	 * We get "CD001" for ISO 9660 disks.
	 *
	 * Scan the first 30 blocks; remember that ISO9660
	 * volume descriptors start at block 16
	 */
	for(block=16;block<32;block++){
		dprint1("Checking block %d\n",block);
		lseek(fd, (block * CDROM_BLOCK_SIZE), 0);

		if(read(fd,buf,CDROM_BLOCK_SIZE)!=CDROM_BLOCK_SIZE){
			dprint("Read size does not match\n");
			return(FSUR_IO_FAIL);
		}

#ifdef DEBUG
		{
			int 	i;
			printf("buf[0..20]: ");
			for(i=0;i<20;i++){
				putchar(buf[i]);
			}
			printf("\n");
		}
#endif		

		/* See if we are a High Sierra disk */
		if(!strncmp(buf+9,"CDROM",5)){
			dprint("High Sierra CDROM\n");
			fs_set_file(FS_NAME_SUFFIX,"CDROM (High Sierra)");

			buf[48+32] = '\000';
			fs_set_file(FS_LABEL_SUFFIX,buf+48);
			close(fd);
			return(FSUR_RECOGNIZED);
		}

		/* See if we are a ISO 9660 disk */
		if(!strncmp(buf+1,"CD001",5)){
			dprint("ISO 9660 CDROM\n");
			fs_set_file(FS_NAME_SUFFIX,"CDROM (ISO 9660)");

			/* Look for the primary volume descriptor */
			while(!strncmp(buf+1,"CD001",5)
			      && buf[6]!='\001'){
				bzero(buf,sizeof(buf));
				read(fd,buf,CDROM_BLOCK_SIZE);
			}
			if(buf[6]=='\001'){
				buf[40+32] = '\000';
				fs_set_file(FS_LABEL_SUFFIX,buf+40);
			}
			
			close(fd);
			return(FSUR_RECOGNIZED);
		}

		/* Hmph.  If this block is not a Primary Volume Descriptor,
		 * then we can't identify this disk.  Otherwise, just loop
		 * and check the next block...
		 */
		
		if(buf[6]!='\001'){
			dprint1("Not a primary volume descriptor (%d)\n",
				buf[6]);
			break;
		}
	}

	close(fd);
	return(FSUR_UNRECOGNIZED);

}

static int fs_mount(char *device, char *mount_point) {

	char path[MAXPATHLEN];
	int rtn;
	char sys_string[80];
	
	/*
	 * First load the file system into kernel memory.
	 */
	getwd(path);
	strcat(path, "/");
	strcat(path, RELOCATABLE_FILE);
	rtn = kl_com_add(path, SERVER_NAME);
	if(rtn) {
		return(FSUR_LOADERR);
	}
	wait_for_kl=1;
	rtn = kl_com_load(SERVER_NAME);
	if(rtn) {
		return(FSUR_LOADERR);
	}

#ifdef DEBUG
	printf("press return to mount...\n");
	getchar();
#endif
	/* If mount_point is the null string, make it something... */
	if(!mount_point){
		mount_point	= "/cd";
	}

	/*
	 * mount. CDROM can use the system mount command.
	 */
	sprintf(sys_string, "%s -o ro -t %s %s %s\n", 
		MOUNT, FS_MOUNT_NAME, device, mount_point);
	dprint1("executing: %s\n", sys_string);
	rtn = system(sys_string);
	if(rtn)
		return(FSUR_IO_FAIL);
	else
		return(FSUR_IO_SUCCESS);
}

static int fs_repair(char *device) {
	return(FSUR_INVAL);			/* no fsck for CDROM disk */
}

static int fs_unmount(char *device) {
	char sys_string[80];
	int rtn;
	
	/*
	 * unmount. MSDOS can use the system unmount command.
	 */
	sprintf(sys_string, "%s %s\n", UMOUNT, device);
	dprint1("executing: %s\n", sys_string);
	rtn = system(sys_string);
	if(rtn)
		return(FSUR_IO_FAIL);
#ifdef	UNLOAD_ON_UNMOUNT
	/*
	 * Unload the file system from kernel memory.
	 */
	rtn = kl_com_unload(SERVER_NAME);
	if(rtn)
		return(FSUR_LOADERR);
	wait_for_kl=1;
#endif	UNLOAD_ON_UNMOUNT
	return(FSUR_IO_SUCCESS);
}

static int fs_fmount(char *device, char *mount_point) {
	return(fs_mount(device, mount_point));		/* try again */
}

/* end of msdos.util.c */
