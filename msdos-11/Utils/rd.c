/*
 * rd - readdir test
 */
 
#import <sys/types.h>
#import <sys/param.h>
#import <sys/stat.h>
#import <sys/dir.h>
#import <sys/printf.h>
#import <libc.h>

void usage(char **argv);
void prompt(char *p);
int dump_stat(char *dir, char *file);

int single_step = 0;
int verbose = 0;
int do_stat = 1;

int main(int argc, char **argv)
{
	DIR *dirp;
	register struct direct *dp;
	int arg;
	
	if(argc < 2)
		usage(argv);
	for(arg=2; arg<argc; arg++) {
		switch(argv[arg][0]) {
		    case 's':
		    	single_step++;
			break;
		    case 'v':
		    	verbose++;
			break;
		    case 'n':
		    	do_stat = 0;
			break;
		    default:
		    	usage(argv);
			break;
		}
	}
	dirp = opendir(argv[1]);
	if (dirp == NULL) {
		printf("opendir(%s) FAILED\n", argv[1]);
		exit(1);
	}
	prompt("opendir() successful");
	while(1) {
		dp = readdir(dirp);
		if(dp == NULL) {
			prompt("readdir returned NULL\n");
			break;
		}
		/*
		 * Dump directory entry 
		 */
		printf("\n");
		dp->d_name[dp->d_namlen] = '\0';
		printf("d_name   = %s\n", dp->d_name);
		printf("d_fileno = 0x%x\n", dp->d_fileno);
		printf("d_reclen = %d\n", dp->d_reclen);
		printf("d_namlen = %d\n", dp->d_namlen);
		printf("d_reclen = %d\n", dp->d_reclen);
		prompt("");
		if(dump_stat(argv[1], dp->d_name))
			exit(0);
	}
	closedir(dirp);
	printf("Directory closed\n");
	exit(0);
}

void usage(char **argv)
{
	printf("\tusage: %s dir_name [s(ingle step)] [n(o stat)]\n", argv[0]);
	exit(1);
}

void prompt(char *p)
{
	char instr[80];
	
	if(!(single_step || verbose))
		return;
	if(single_step) {
		if(p[0])
			printf("%s; ",p);
		printf("Hit CR to continue: ");
		gets(instr);
	}
	else
		printf("%s\n");
}

int dump_stat(char *dir, char *file)
{
	char path[100];
	struct stat sb;
	
	if(!do_stat)
		return(0);
	strcpy(path, dir);
	strcat(path,"/");
	strcat(path,file);
	if(stat(path, &sb)) {
		perror("stat");
		return(1);
	}
	printf("st_mode  = 0%o\n",  sb.st_mode);
	printf("st_ino   = 0x%x\n", sb.st_ino);
	printf("st_dev  =  0x%x\n", sb.st_dev);
	printf("st_rdev  = 0x%x\n", sb.st_rdev);
	printf("st_size  = %d\n",   sb.st_size);
	printf("st_blksz = 0x%x\n", sb.st_blksize);
	printf("st_ctime = %d\n",   sb.st_mtime);
	printf("st_mtime = %d\n",   sb.st_ctime);
	printf("st_uid   = %d\n",   sb.st_uid);
	printf("st_gid   = %d\n",   sb.st_gid);
	printf("st_mode  = %4o\n",  sb.st_mode);
	prompt("");
	return(0);
}
