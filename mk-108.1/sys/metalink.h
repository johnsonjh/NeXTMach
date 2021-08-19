/*
 *	Copyright (C) 1989, NeXT, Inc.
 */
/*
 * metalink.h
 *
 * This structure describes the different expressions that will be
 * expanded into current kernel values when found in symbolic links.
 */

#define	ML_ESCCHAR	'$'	/* The escape character */

struct metalink {
	char *ml_token;		/* The token to be changed */
	char *ml_variable;	/* The kernel variable to substitute with */
	char *ml_default;	/* The default character */
};

extern char boot_file[];

struct metalink metalinks[] = {
	{ "BOOTFILE", boot_file, NULL },
	{ NULL, NULL, NULL }
};
