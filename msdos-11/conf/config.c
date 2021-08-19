/*
 * configure MSDOS project build directory Makefiles
 *
 * History
 * 31-May-90	Doug Mitchell at NeXT
 *	Created.
 *
 *	input: 	files.msdos
 *		    contains:
 *			usr/pc_abort.c	
 *			lowl/pc_clalc.c
 *			etc
 *		Makefile.{DEBUG, RELEASE}
 *		    contains:
 *			CFLAGS= whatever
 *			other config-specific stuff
 *		Makefile.master
 *		    contains:
 *			common makefile stuff
 *			C_RULE_{1,2,3,4}=
 *			clean:
 *			etc.
 *		Load_Commands.seg
 *		Unload_Commands.seg
 *	output: ../{DEBUG,RELEASE}
 *		../{DEBUG,RELEASE}/Makefile
 *		../{DEBUG,RELEASE}/Makefile.ofiles
 *		../{DEBUG,RELEASE}/Makefile.build_rules
 *		    contains:
 *			pc_abort.o:	../lowl/pc_abort.c
 *				${C_RULE_1} ../lowl/pc_abort.c			
 *				${C_RULE_2}				
 *				${C_RULE_3}				
 *				${C_RULE_4}
 *			etc.
 *		{DEBUG,RELEASE}/Load_Commands.seg
 *		{DEBUG,RELEASE}/Unload_Commands.seg
 *				
 * {DEBUG,RELEASE}/Makefile:
 *	config-specific stuff		-- from conf/Makefile.{RELEASE,DEBUG}
 *	common stuff			-- from conf/Makefile.master
 *	-include Makefile.ofiles	-- ditto
 *	-include Makefile.build_rules
 *	-include Makedep
 */
 
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <libc.h>

void usage(char **argv);
int do_copy(int source_fd, int dest_fd, char *op, char *buf);

/*
 * hard-coded filenames
 */
#define FILES_FILE		"files.msdos"
#define MASTER_FILE		"Makefile.master"
#define CONFIG_MAKEFILE		"Makefile."
#define MAKEFILE_FILE		"Makefile"
#define OFILES_FILE		"Makefile.ofiles"
#define BUILD_RULES_FILE	"Makefile.build_rules"
#define LOAD_COMMANDS_FILE	"Load_Commands.sect"
#define UNLOAD_COMMANDS_FILE	"Unload_Commands.sect"

/*
 * build rules
 */
#define RULE1	"${C_RULE_1}"
#define RULE2	"${C_RULE_2}"
#define RULE3	"${C_RULE_3}"
#define RULE4	"${C_RULE_4}"

#define BUFSIZE		0x8000

main(int argc, char **argv)
{
	int files_fd, master_makefile_fd, config_makefile_fd;
	int ofiles_fd, build_rules_fd, out_makefile_fd;
	FILE *files_file, *ofiles_file, *build_rules_file;
	char filename[80], ofilename[80];
	int rtn;
	char *buf;
	int num_files;
	int len;
	char *cp;
	
	if(argc != 2) 
		usage(argv);
	/*
	 * open all the files we'll need.
	 */
	files_fd = open(FILES_FILE, O_RDONLY, 0);
	if(files_fd <= 0) {
		printf("Couldn't open %s\n", FILES_FILE);
		perror("open");
		exit(1);
	}
	files_file = fdopen(files_fd, "r");
	if(files_file == NULL) {
		printf("Couldn't open %s\n", filename);
		perror("fdopen");
		exit(1);
	}
	master_makefile_fd = open(MASTER_FILE, O_RDONLY, 0);
	if(master_makefile_fd <= 0) {
		printf("Couldn't open %s\n", MASTER_FILE);
		perror("open");
		exit(1);
	}
	sprintf(filename, "%s%s", CONFIG_MAKEFILE, argv[1]);
	config_makefile_fd = open(filename, O_RDONLY, 0);
	if(config_makefile_fd <= 0) {
		printf("Couldn't open %s\n", filename);
		perror("open");
		exit(1);
	}
	sprintf(filename, "mkdirs ../%s", argv[1]);
	if(system(filename)) {
		printf("Can't create %s\n", filename);
		perror("mkdir");
		exit(1);
	}
	sprintf(filename, "../%s/%s", argv[1], MAKEFILE_FILE);
	out_makefile_fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if(out_makefile_fd <= 0) {
		printf("Couldn't create %s\n", filename);
		perror("open");
		exit(1);
	}
	sprintf(filename, "../%s/%s", argv[1], OFILES_FILE);
	ofiles_fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if(ofiles_fd <= 0) {
		printf("Couldn't create %s\n", filename);
		perror("open");
		exit(1);
	}
	ofiles_file = fdopen(ofiles_fd, "w");
	if(ofiles_file == NULL) {
		printf("Couldn't open %s\n", filename);
		perror("fdopen");
		exit(1);
	}
	sprintf(filename, "../%s/%s", argv[1], BUILD_RULES_FILE);
	build_rules_fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if(build_rules_fd <= 0) {
		printf("Couldn't create %s\n", filename);
		perror("open");
		exit(1);
	}
	build_rules_file = fdopen(build_rules_fd, "w");
	if(build_rules_file == NULL) {
		printf("Couldn't open %s\n", filename);
		perror("fdopen");
		exit(1);
	}

	/*
	 * First generate the Makefile
	 */
	buf = malloc(BUFSIZE);
	if(!buf) {
		perror("malloc");
		exit(1);
	}
	if(do_copy(config_makefile_fd, out_makefile_fd, CONFIG_MAKEFILE, buf))
		exit(1);
	if(do_copy(master_makefile_fd, out_makefile_fd, CONFIG_MAKEFILE, buf))
		exit(1);
	close(out_makefile_fd);
	 
	/* 
	 * Generate ofiles, build_rules
	 */
	num_files = 0;
	fprintf(ofiles_file, "OFILES= ");
	while(1) {
		/*
		 * Get one filename 
		 */
		rtn = fscanf(files_file, "%s\n", filename);
		if(rtn == 0) {
			printf("parse error in %s\n", FILES_FILE);
			exit(1);
		}
		if(rtn == EOF)
			break;
		/*
		 * newline every 5 filenames
		 */
		if((num_files % 5 == 0) && num_files) 
			fprintf(ofiles_file, "\\\n\t");
		/*
		 * We're assuming a filename starting with "path/" and ending
		 * in ".c", possibly with trailing whitespace. Strip off path,
		 * convert to ".o", and write to ofile.
		 */
		cp = filename;
		while(*cp != '/')
			cp++;
		cp++;
		strcpy(ofilename, cp);
		len = strlen(ofilename);
		for(; len; len--) {
			if(ofilename[len] == 'c') {
				ofilename[len] = 'o';
				break;
			}
		}
		if(len == 0) {
			printf("parse error in %s\n", FILES_FILE);
			exit(1);
		}
		fprintf(ofiles_file, "%s ", ofilename);
		num_files++;
		/*
		 * Write a dependency rule.
		 */
		fprintf(build_rules_file, "%s:\t../%s\n", 
			ofilename, filename);
		fprintf(build_rules_file, "\t%s ../%s\n",
			RULE1, filename);
		fprintf(build_rules_file, "\t%s\n\t%s\n\t%s\n",
			RULE2, RULE3, RULE4);
	}
	fprintf(ofiles_file,"\n");
	
	/*
	 * Copy over the kern_loader segment files
	 */
	sprintf(filename, "cp %s ../%s", LOAD_COMMANDS_FILE, argv[1]);
	rtn = system(filename);
	if(rtn) {
		printf("copying %s: error %d\n", LOAD_COMMANDS_FILE, rtn);
		exit(1);
	}
	sprintf(filename, "cp %s ../%s", UNLOAD_COMMANDS_FILE, argv[1]);
	rtn = system(filename);
	if(rtn) {
		printf("copying %s: error %d\n", UNLOAD_COMMANDS_FILE, rtn);
		exit(1);
	}
	
	/* 
	 * all right! 
	 */
	return(0);
	 
} /* main() */

void usage(char **argv)
{	
	printf("usage: %s build_directory\n");
	exit(1);
}

int do_copy(int source_fd, int dest_fd, char *opname, char *buf)
{
	int bytes_read;
	int bytes_written;
	
	do {
		bytes_read = read(source_fd, buf, BUFSIZE);
		if(bytes_read == 0)
			break;
		bytes_written = write(dest_fd, buf, bytes_read);
		if(bytes_written != bytes_read) {
			printf("Error writing %s\n", opname);
			perror("write");
			return(1);
		}
	} while(bytes_read != 0);
	return(0);
}
/* end of config.c */