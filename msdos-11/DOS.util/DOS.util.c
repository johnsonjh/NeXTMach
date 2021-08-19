/*	@(#)DOS.util.c	2.0	24/07/90	(c) 1990 NeXT	*/

/* 
 * DOS.util.c -- Loadable MS-DOS file system utility
 *
 * HISTORY
 * 24-Jul-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <stdlib.h>
#import <libc.h>
#import <sys/file.h>
#import <nextdev/disk.h>
#import <next/loadable_fs.h>
#import <nextdos/msdos.h>
#import "DOS.util.h"

/* #define SEND_INIT	1			/* */
/* #define UNLOAD_ON_UNMOUNT	1		/* unmount implies unload */

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
	dprintf(("...exit_code = %d\n", exit_code));
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
static int fs_probe(char *device) {

	int fd;
	int rtn;
	char buf[DOS_SECTOR_SIZE];
	struct drive_info di;
	
	fd = open(device, O_RDONLY, 0);
	if(fd <= 0)
		return(FSUR_IO_FAIL);
	/*
	 * Verify that this is a 512-byte per sector disk
	 */
	if(ioctl(fd, DKIOCINFO, &di)) 
		return(FSUR_IO_FAIL);
	if(di.di_devblklen != DOS_SECTOR_SIZE)
		return(FSUR_UNRECOGNIZED);
		
	/* 
	 * get 1 block starting at 0
	 */
	lseek(fd, 0, L_SET);
	rtn = read(fd, buf, DOS_SECTOR_SIZE);
	if(rtn != DOS_SECTOR_SIZE)
		return(FSUR_IO_FAIL);
	/*
	 * This is the block 0 "jump' field from EBS pc_dskinit()...
	 */
	if((buf[0] == (char)0xe9) || (buf[0] == (char)0xeb))
		rtn = FSUR_RECOGNIZED;
	else
		rtn = FSUR_UNRECOGNIZED;
	close(fd);
	return(rtn);
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
#ifdef	SEND_INIT
	/*
	 * DEBUG file system needs an "init" message.
	 */
	sprintf(sys_string, "lfs %s\n", SERVER_NAME);
	printf("executing: %s\n", sys_string);
	system(sys_string);
#endif	SEND_INIT

	/*
	 * mount. MSDOS can use the system mount command.
	 */
	sprintf(sys_string, "%s -t %s %s %s\n", 
		MOUNT, FS_NAME, device, mount_point);
	dprintf(("executing: %s\n", sys_string));
	rtn = system(sys_string);
	if(rtn)
		return(FSUR_IO_FAIL);
	else
		return(FSUR_IO_SUCCESS);
}

static int fs_repair(char *device) {
	return(FSUR_INVAL);			/* no fsck for DOS disk */
}

static int fs_unmount(char *device) {
	char sys_string[80];
	int rtn;
	
	/*
	 * unmount. MSDOS can use the system unmount command.
	 */
	sprintf(sys_string, "%s %s\n", UMOUNT, device);
	dprintf(("executing: %s\n", sys_string));
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