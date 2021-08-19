/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 28-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Intuit server version from a possibly fouled up version section.
 *	It's an error for a relocatable not to have a version section.
 *
 * 19-Oct-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */

#import "server.h"
#import "obj.h"
#import "log.h"
#import "misc.h"
#import <kernserv/kern_loader_types.h>
#import <kernserv/kern_loader_error.h>
#import <stdlib.h>
#import <fcntl.h>
#import <stdio.h>
#import <string.h>
#import <ctype.h>
#import <syslog.h>
#import <libc.h>
#import <mach_error.h>
#import <mach_host.h>
#import <objc/hashtable.h>
#import <sys/stat.h>
#import <sys/file.h>
#import <servers/netname.h>

extern port_set_name_t port_set;
extern const char *libloadserv;
extern const char *libloadserv_g;
extern boolean_t debug;

/*
 * Tokens
 */
/*
 * Tokens used for load and unload commands
 */
static enum load_cmds {
	ADVERTISE,
	CALL,
	HMAP,
	PORT_DEATH,
	SMAP,
	START,
	WIRE,
	UNKNOWN
} load_cmds[] = {
	ADVERTISE,
	CALL,
	HMAP,
	SMAP,		// MAP == SMAP
	PORT_DEATH,
	SMAP,
	START,
	WIRE};

static const char *load_cmd_strings[] = {
	"ADVERTISE",
	"CALL",
	"HMAP",
	"MAP",
	"PORT_DEATH",
	"SMAP",
	"START",
	"WIRE",
};

const char *Loaded_Server = "Loaded Server";
const char *Server_Name = "Server Name";
const char *Instance_Var = "Instance Var";
const char *Load_Commands = "Load Commands";
const char *Unload_Commands = "Unload Commands";
const char *Loadable_Name = "Loadable Name";
const char *Executable_Name = "Executable Name";
const char *Server_Version = "Server Version";

const int timeout_max = 2*60;

#if !defined(page_round)
#define page_trunc(p) ((int)(p)&~(vm_page_size-1))
#define page_round(p) page_trunc((int)(p)+vm_page_size-1)
#endif

static inline void lc_add_sym(server_t *server, const char *sym)
{
	server->symbols = realloc(server->symbols,
		sizeof(NXAtom)*(server->nsymbols+2));
	server->symbols[server->nsymbols++] = sym;
	server->symbols[server->nsymbols] = NULL;
}

static const char *lc_cmd(const char *sect_data, enum load_cmds *token);
static kern_return_t lc_parse_cmds (
	server_t	*server,
	queue_t		cmd_queue,
	const char	*ptr);

kern_return_t lc_init(const char *reloc, server_t *server)
{
	const char *s;
	int timeout;
	kern_return_t r;
	struct stat statb;
	server_t *other_server;

	if (stat(reloc, &statb) < 0) {
		kllog(LOG_ERR, "Can't stat file %s, %s(%d)\n",
			reloc, strerror(errno), errno);
		server_unlock(server);
		return KERN_LOADER_CANT_OPEN_SERVER;
	}

	server->reloc_date = statb.st_mtime;
	server->reloc_size = statb.st_size;

	if (!obj_map_reloc(server)) {
		kllog(LOG_ERR, "Mapping relocatable %s failed\n",
			reloc);
		server_unlock(server);
		return KERN_LOADER_CANT_OPEN_SERVER;
	}

	if (server->please_die) {
		server_unlock(server);
		return KERN_FAILURE;
	}

	/*
	 * Figure out the name of this server.
	 */
	s = getMachoString(server, Loaded_Server, Server_Name);

	/*
	 * Make sure that there isn't already a server
	 * loaded with this name.
	 */
	server_unlock(server);
	other_server = server_by_name(s);
	if (other_server && other_server != server) {
		server_lock(other_server);
		if (other_server->state == Deallocated)
			server_delete(other_server);
		else {
			server_unlock(other_server);
			kllog(LOG_ERR, "server named %s already exists\n", s);
			return KERN_LOADER_SERVER_EXISTS;
		}
	}

	server->name = s;
	if (!server->name) {
		kllog(LOG_ERR, "can't find name for server %s\n", reloc);
		return KERN_LOADER_NEED_SERVER_NAME;
	}

	kllog(LOG_INFO, "Allocating server %s\n", server->name);

	/*
	 * Load and parse load and unload commands.
	 */
	s = getMachoData(server, Loaded_Server, Load_Commands);
	if (!s) {
		kllog(LOG_ERR, "server %s has no load commands\n",
			server->name);
		return KERN_LOADER_BAD_RELOCATABLE;
	}
	r = lc_parse_cmds(server, &server->load_cmds, s);
	free((void *)s);
	if (r != KERN_SUCCESS)
		return r;

	/*
	 * Unload commands are optional.
	 */
	s = getMachoData(server, Loaded_Server, Unload_Commands);
	if (s) {
		r = lc_parse_cmds(server, &server->unload_cmds, s);
		free((void *)s);
		if (r != KERN_SUCCESS)
			return r;
	}

	s = getMachoString(server, Loaded_Server, Instance_Var);
	if (!s) {
		kllog(LOG_ERR, "server %s has no instance variables\n",
			server->name);
		return KERN_LOADER_BAD_RELOCATABLE;
	}
	server->instance = NXUniqueString(s);

	lc_add_sym(server, server->instance);

	/*
	 * See if we need a loadable to be created.
	 */
	s = getMachoString(server, Loaded_Server, Loadable_Name);
	if (!s) {
		server->loadable = NULL;
	} else {
		char buf[FILENAME_MAX];
		char *s1;

		/*
		 * Make the location of the loadable relative
		 * to the location of the relocatable.
		 */
		s1 = strrchr(server->reloc, '/');
		if (*s == '/' || !s1)
			strcpy(buf, s);
		else {
			strncpy(buf, server->reloc, s1-server->reloc+1);
			strcpy(buf+(s1-server->reloc)+1, s);
		}
		server->loadable = NXUniqueString(buf);
	}

	/*
	 * Get the server version
	 */
	s = getMachoString(server, Loaded_Server, Server_Version);
	if (!s) {
		kllog(LOG_ERR, "server %s has no version section\n",
			server->name);
		return KERN_LOADER_BAD_RELOCATABLE;
	} else {
		char *s1;
		s1 = strrchr(s, ',');
		if (s1) {
			while (*s1 && !isdigit(*s1))
				s1++;
			s = s1;
		}
	}
	server->version = atoi(s);

	/*
	 * Make sure that the thing we're going to load into is there
	 * and get it's version string.
	 */
	server->executable = getMachoString(server, Loaded_Server,
					    Executable_Name);
	if (!server->executable)
		server->executable = NXUniqueString("/mach");

	/*
	 * If the executable file can't be stat'ed, wait around for it
	 * a bit, it might not be mounted yet.
	 */
	for (timeout = timeout_max; timeout; timeout--) {
		/*
		 * If we can access the file, we're done.
		 */
		if (stat(server->executable, &statb) == 0)
			break;

		if (timeout == timeout_max)
			kllog(LOG_INFO, "executable %s for server %s "
				"not available, sleeping\n",
				server->executable, server->name);
		else if ((timeout%30) == 0)
			kllog(LOG_INFO, "executable %s for server %s "
				"not available, still trying\n",
				server->executable, server->name);
		if (server->please_die)
			return KERN_FAILURE;

		sleep(2);
	}
	if (timeout == 0) {
		kllog(LOG_ERR, "server %s giving up waiting for "
			"executable %s\n",
			server->name, server->executable);
		return KERN_LOADER_BAD_RELOCATABLE;
	} else if (timeout < timeout_max)
		kllog(LOG_INFO, "executable %s available for server %s\n",
			server->executable, server->name);

	r = task_by_name(server->executable, &server->executable_task);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "can't get task port for executable "
			"%s (server %s): %s(%d)\n", server->executable,
			server->name, mach_error_string(r), r);
		return r;
	}

	server->kernel = obj_symbol_value(server->executable,
				"_msg_send_from_kernel") != 0;
	lc_add_sym(server,
		server->kernel ? "_kern_server_main" : "_loaded_server_main");

	if (server->please_die)
		return KERN_FAILURE;

	/*
	 * Link the loadable against the executable importing any needed
	 * libraries.
	 */
	
	if (!obj_link(server)) {
		kllog(LOG_ERR, "server %s won't link\n", server->name);
		return KERN_LOADER_SERVER_WONT_LINK;
	}

	if (server->please_die)
		return KERN_FAILURE;

	/*
	 * Get the entry point.
	 */
	r = sym_value(server->name,
		server->kernel ? "_kern_server_main" : "_loaded_server_main",
		&server->server_start);
 	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "can't find \"%s\" symbol value\n",
			server->kernel ? "_kern_server_main"
				  : "_loaded_server_main");
		return FALSE;
	}

	return KERN_SUCCESS;
}


/*
 * Find the next command from the load/unload commands segment.
 */
static const char *lc_cmd(const char *sect_data, enum load_cmds *token)
{
	/*
	 * Search forward until we either find a token, or we run off
	 * the end of the string.
	 */
	while (sect_data && *sect_data) {
		char *s;
		int i;
		if (*sect_data <= 'Z' && *sect_data >= 'A') {
			/*
			 * Find out which token matched.
			 */
			for (  i = 0
			     ; i <   sizeof(load_cmd_strings)
				   / sizeof(load_cmd_strings[0])
			     ; i++)
			{
				int size = strlen(load_cmd_strings[i]);
				if (!strncmp(sect_data, load_cmd_strings[i],
					     size))
					break;
			}
			*token = load_cmds[i];
			return sect_data;
		}
		/*
		 * Go to the beginning of the next line.
		 */
		if (s = index(sect_data, '\n'))
			sect_data = s+1;
		else
			break;
	}
	*token = UNKNOWN;
	return NULL;
}

static kern_return_t lc_parse_cmds (
	server_t	*server,
	queue_t		cmd_queue,
	const char	*ptr)
{
	enum load_cmds token;
	server_command_t *scp;
	char buf[80];
	const char *pname, *fname, *arg;
	kern_return_t r;

	while (ptr = lc_cmd(ptr, &token)) {
		const char *s;
		switch (token) {
		case HMAP: case SMAP:
			/*
			 *	"HMAP	port_name	function	arg"
			 *	"SMAP	port_name	function	arg"
			 *
			 * Set up to pass messages received on "port_name"
			 * to the specified "function", along with int "arg"
			 * HMAP calls function has a handler.
			 * SMAP calls function as a server.
			 * (MAP is a synonym for SMAP).
			 */
			ptr += strcspn(ptr, " \t");
			ptr += strspn(ptr, " \t");
			s = ptr;
			ptr += strcspn(ptr, " \t");
			strncpy(buf, s, ptr-s);
			buf[ptr-s] = '\0';
			pname = NXUniqueString(buf);
			ptr += strspn(ptr, " \t");
			s = ptr;
			ptr += strcspn(ptr, " \t");
			strncpy(buf, s, ptr-s);
			buf[ptr-s] = '\0';
			fname = NXUniqueString(buf);
			ptr += strcspn(ptr, " \t");
			ptr += strspn(ptr, " \t");
			s = ptr;
			ptr += strcspn(ptr, " \t\n");
			strncpy(buf, s, ptr-s);
			buf[ptr-s] = '\0';
			arg = NXUniqueString(buf);

			server_lock(server);
			scp = server_command_by_port_name(server, pname);
			server_unlock(server);
			if (scp) {
				kllog(LOG_ERR, "port %s already declared "
					"for server %s\n",
						pname, server->name);
				return KERN_LOADER_PORT_EXISTS;
			}
			scp = (server_command_t *)malloc(sizeof(*scp));
			scp->msg_type = (token == HMAP) ? S_C_HMAP : S_C_SMAP;
			scp->advertised = FALSE;
 			scp->port_name = pname;
			scp->function = fname;
			scp->arg = arg;
			lc_add_sym(server, fname);
			if (!isdigit(*arg)) {
				/*
				 * Arg is a symbol.
				 */
				lc_add_sym(server, arg);
			}

			/*
			 * Allocate the port to be advertised.
			 */
			r = port_allocate(task_self(), &scp->port);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"can't allocate advertized port %s "
					"for server %s\n",
					scp->port_name, server->name);
				free(scp);
				return r;
			}

			/*
			 * Add the port to our port set and advertise it with
			 * the netname server.
			 */
			r = port_set_add(task_self(), port_set,
					 scp->port);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"can't add advertized port %s "
					"to port set for server %s\n",
					scp->port_name, server->name);
				free(scp);
				return r;
			}

			server_lock(server);
			queue_enter(cmd_queue, scp, server_command_t *, link);
			server_unlock(server);
			break;

		case ADVERTISE:
			/*
			 *	"ADVERTISE	port_name"
			 *
			 * Set up to pass messages received on "port_name"
			 * to the specified "function", along with int "arg"
			 */
			ptr += strcspn(ptr, " \t");
			ptr += strspn(ptr, " \t");
			s = ptr;
			ptr += strcspn(ptr, " \t\n");
			strncpy(buf, s, ptr-s);
			buf[ptr-s] = '\0';
			pname = NXUniqueString(buf);

			server_lock(server);
			scp = server_command_by_port_name(server, pname);
			server_unlock(server);
			if (scp == NULL) {
				kllog(LOG_ERR, "port %s not declared in "
						"server %s\n",
						pname, server->name);
				return KERN_LOADER_PORT_EXISTS;
			}

			scp->advertised = TRUE;
			r = netname_check_in(name_server_port,
						(char *)pname,
						task_self(),
						scp->port);
			if (r != KERN_SUCCESS) {
				kllog(LOG_ERR,
					"can't checkin advertised port"
					"%s for server %s\n",
					scp->port_name,
					server->name);
				port_deallocate(task_self(),
					scp->port);
				free(scp);
				return r;
			}
			break;

		case CALL:
			/*
			 *	"CALL	function	arg"
			 *
			 * Startup sequence, call the specified "function"
			 * passing it "arg"
			 */
			scp = (server_command_t *)malloc(sizeof(*scp));
			scp->msg_type = S_C_CALL;
			ptr += strcspn(ptr, " \t");
			ptr += strspn(ptr, " \t");
			s = ptr;
			ptr += strcspn(ptr, " \t");
			strncpy(buf, s, ptr-s);
			buf[ptr-s] = '\0';
			fname = NXUniqueString(buf);
			ptr += strspn(ptr, " \t");
			s = ptr;
			ptr += strcspn(ptr, " \t\n");
			strncpy(buf, s, ptr-s);
			buf[ptr-s] = '\0';
			arg = NXUniqueString(buf);
			scp->function = fname;
			scp->arg = arg;
			lc_add_sym(server, fname);
			if (!isdigit(*arg)) {
				/*
				 * Arg is a symbol.
				 */
				lc_add_sym(server, arg);
			}

			server_lock(server);
			queue_enter(cmd_queue, scp, server_command_t *, link);
			server_unlock(server);
			break;

		case PORT_DEATH:
			/*
			 *	"PORT_DEATH	function"
			 *
			 * Call the specified "function" passing it "arg"
			 * when a port dies.
			 */
			scp = (server_command_t *)malloc(sizeof(*scp));
			scp->msg_type = S_C_DEATH;
			ptr += strcspn(ptr, " \t");
			ptr += strspn(ptr, " \t");
			s = ptr;
			ptr += strcspn(ptr, " \t\n");
			strncpy(buf, s, ptr-s);
			buf[ptr-s] = '\0';
			scp->function = NXUniqueString(buf);
			lc_add_sym(server, scp->function);

			server_lock(server);
			queue_enter(cmd_queue, scp, server_command_t *, link);
			server_unlock(server);
			break;

		case WIRE:
			/*
			 *	"WIRE"
			 *
			 * The kernel server must be wired down into memory.
			 */
			server->wire = TRUE;
			break;

		case START:
			/*
			 *	"START"
			 *
			 * Cause the kernel server to be loaded and started
			 * up immediately.
			 */
			server->load = TRUE;
			break;

		default:
			kllog(LOG_ERR,
				"unrecognized token in script file\n");
			return KERN_LOADER_BAD_RELOCATABLE;
		}
		ptr = index(ptr, '\n');
		if (ptr)
			ptr++;
	}
	return KERN_SUCCESS;
}
