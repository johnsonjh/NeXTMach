/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 21-Apr-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */

#import "server.h"
#import "obj.h"
#import "log.h"
#import "reloc.h"
#import "misc.h"
#import <stdlib.h>
#import <fcntl.h>
#import <stdio.h>
#import <string.h>
#import <libc.h>
#import <mach_error.h>
#import <streams/streams.h>
#import <sys/loader.h>
#import <rld.h>
#import <sys/stat.h>
#import <vm/vm_param.h>

extern boolean_t debug;

const char *libloadserv = "/usr/lib/libloadserv.a";
const char *libloadserv_g = "/usr/local/lib/libloadserv_g.a";

mutex_t rld_mutex;
#define rld_lock()	mutex_lock(rld_mutex)
#define rld_unlock()	mutex_unlock(rld_mutex)

static server_t *rld_server;
static const char *rld_executable;

#if !defined(page_round)
#define page_trunc(p) ((int)(p)&~(vm_page_size-1))
#define page_round(p) page_trunc((int)(p)+vm_page_size-1)
#endif

/*
 * Allocate space in ourselves and the executable and allocate virtual
 * memory starting with the address addr.
 */
static vm_address_t obj_vm_allocate (
	vm_size_t	file_size,
	vm_size_t	header_size)
{
	vm_address_t	exec_addr, my_addr, addr;
	vm_size_t	exec_size, my_size, vm_size;
	vm_prot_t	prot, max_prot;
	vm_inherit_t	inheritance;
	boolean_t	shared;
	port_t		obj_name;
	vm_offset_t	offset;
	kern_return_t	r;

	/*
	 * First be sure it's worth having the header on a separate page.
	 */
	if (page_round(file_size - header_size) == page_round(file_size))
		header_size = 0;

	rld_server->rld_size = file_size;
	vm_size = page_round(file_size - header_size);
	file_size = vm_size + page_round(header_size);
	exec_addr = my_addr = 0;

	/*
	 * Find the first allocated region in each task and start
	 * after the greater of each of those.
	 */
	r = vm_region(rld_server->executable_task,
			&exec_addr, &exec_size,
			&prot, &max_prot, &inheritance,
			&shared, &obj_name, &offset);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "Server %s can't find first region in "
			"executable \"%s\": %s (%d)\n",
			rld_server->name, rld_server->executable,
			mach_error_string(r), r);
		return 0;
	}

	r = vm_region(task_self(),
			&my_addr, &my_size,
			&prot, &max_prot, &inheritance,
			&shared, &obj_name, &offset);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "Server %s can't find first region in "
			"kern_loader: %s (%d)\n",
			rld_server->name, mach_error_string(r), r);
		return 0;
	}

	r = KERN_SUCCESS;
    again:
	if (r == KERN_NO_SPACE)
		return 0;

	addr = exec_addr + exec_size + page_round(header_size);
	if (my_addr + my_size > addr)
		addr = my_addr + my_size;

	exec_addr = addr;
	my_addr = addr - page_round(header_size);

 	/*
	 * See if this address is okay in the executable.
	 */
	r = vm_region(rld_server->executable_task,
			&exec_addr, &exec_size,
			&prot, &max_prot, &inheritance,
			&shared, &obj_name, &offset);
	if (r == KERN_SUCCESS && addr + vm_size > exec_addr) {
		addr = exec_addr + exec_size;
#if	DEBUG
		kllog(LOG_DEBUG, "next region in executable at %#x\n",
			addr);
#endif	DEBUG
		goto again;
	}

	/*
	 * See if this address is okay for us.
	 */
	r = vm_region(task_self(),
			&my_addr, &my_size,
			&prot, &max_prot, &inheritance,
			&shared, &obj_name, &offset);
	if (r == KERN_SUCCESS && addr + vm_size > my_addr) {
		addr = my_addr + my_size;
#if	DEBUG
		kllog(LOG_DEBUG, "next region in kern_loader at %#x\n",
			addr);
#endif	DEBUG
		goto again;
	}

	exec_addr = addr;
	my_addr = addr - page_round(header_size);

	/*
	 * Go ahead and allocate the memory in both servers.
	 */
	r = vm_allocate(rld_server->executable_task, &exec_addr, vm_size,
		FALSE);
	switch (r) {
	case KERN_SUCCESS:
		rld_server->alloc_in_exec = TRUE;
		break;
	case KERN_NO_SPACE:
	case KERN_INVALID_ADDRESS:
		kllog(LOG_DEBUG, "couldn't allocate in executable\n");
		addr = exec_addr + exec_size;
		goto again;
	default:
		return 0;
	}

	r = vm_allocate(task_self(), &my_addr, file_size, FALSE);
	switch (r) {
	case KERN_SUCCESS:
		rld_server->alloc_in_self = TRUE;
		break;
	case KERN_NO_SPACE:
	case KERN_INVALID_ADDRESS:
		kllog(LOG_ERR, "Server %s couldn't allocate in kern_loader: "
			"%s (%d)\n", rld_server->name, mach_error_string(r),
			r);
		r = vm_deallocate(rld_server->executable_task, addr, vm_size);
		if (r != KERN_SUCCESS)
			kllog(LOG_ERR, "vm_deallocate in exec failed: "
				"%s (%d)\n",
				mach_error_string(r), r);
		kllog(LOG_DEBUG, "Server %s deallocated %#x bytes "
			"at address %#x in executable \"%s\"\n",
			rld_server->name, vm_size, addr,
			rld_server->executable);
		rld_server->alloc_in_exec = FALSE;
		addr = my_addr + my_size;
		goto again;
	default:
		kllog(LOG_ERR, "Server %s couldn't allocate in kern_loader: "
			"%s (%d)\n", rld_server->name, mach_error_string(r),
			r);
		r = vm_deallocate(rld_server->executable_task, addr, vm_size);
		if (r != KERN_SUCCESS)
			kllog(LOG_ERR, "vm_deallocate in exec failed: "
				"%s (%d)\n",
				mach_error_string(r), r);
		kllog(LOG_DEBUG, "Server %s deallocated %#x bytes "
			"at address %#x in executable \"%s\"\n",
			rld_server->name, vm_size, addr,
			rld_server->executable);
			rld_server->alloc_in_exec = FALSE;
			return 0;
	}

	kllog(LOG_DEBUG, "Server %s allocated %#x bytes at address %#x "
		"in executable \"%s\" and %#x bytes at address %#x in "
		"kern_loader\n",
		rld_server->name, vm_size, exec_addr, rld_server->executable,
		vm_size + page_round(header_size), my_addr);

	rld_server->vm_addr = exec_addr;
	rld_server->vm_size = vm_size;
	rld_server->header = (struct mach_header *)(exec_addr - header_size);

	return (vm_address_t)(rld_server->header);
}

/*
 * Find the modification date of the specified file.
 */
boolean_t obj_date(const char *reloc, u_int *date)
{
	struct	stat	statb;

	if (stat(reloc, &statb) < 0) {
		kllog(LOG_ERR, "stat relocatable failed: %s(%d)\n",
			strerror(cthread_errno()), cthread_errno());
		return FALSE;
	}

	*date = (u_int)statb.st_mtime;
	return TRUE;
}

/*
 * Return the address of the symbol in the relocatable with the
 * given name (or NULL).
 */
vm_address_t obj_symbol_value(const char *filename, const char *name)
{
	vm_address_t symvalue;
	struct nlist nl[3];

	if (sym_value(filename, name, &symvalue) == KERN_SUCCESS) {
		kllog(LOG_DEBUG, "symbol \"%s\" in file %s "
			"cached value is %#x\n",
			name, filename, symvalue);
		return symvalue;
	}

	nl[0].n_un.n_name = (char *)name;
	nl[1].n_un.n_name = (char *)malloc(strlen(name)+2);
	sprintf(nl[1].n_un.n_name, "_%s", name);
	nl[2].n_un.n_name = NULL;
	unix_lock();
	nlist(filename, nl);
	unix_unlock();
	free(nl[1].n_un.n_name);
	symvalue = nl[0].n_value;
	if (!symvalue)
		symvalue = nl[1].n_value;

	kllog(LOG_DEBUG, "symbol \"%s\" in file %s value is %#x\n",
		name, filename, symvalue);

	if (symvalue)
		save_symvalue(filename, name, symvalue);
	
	return symvalue;
}

/*
 * Allocate space for and map in the Mach-O file on a page-aligned boundary.
 */
boolean_t obj_map_reloc (server_t *server)
{
	int fd;
	kern_return_t r;

	if ((fd = open(server->reloc, O_RDONLY, 0)) < 0) {
		kllog(LOG_ERR, "Server %s can't open relocatable %s: %s(%d)\n",
			server->name, server->reloc,
			strerror(cthread_errno()), cthread_errno());
		return FALSE;
	}

	r = map_fd(fd, 0, (vm_address_t *)&server->header, TRUE,
		server->reloc_size);
	if (r != KERN_SUCCESS) {
		kllog(LOG_ERR, "Server %s can't map relocatable %s: %s(%d)\n",
			server->name, server->reloc, mach_error_string(r), r);
		return FALSE;
	}
	server->alloc_in_self = TRUE;
	server->vm_addr = (vm_offset_t)server->header
			+ server->header->sizeofcmds
			+ sizeof(*server->header);
	server->vm_size = server->reloc_size
			- ((vm_offset_t)server->header - server->vm_addr);

	if (server->header->magic != MH_MAGIC) {
		kllog(LOG_ERR, "%s is not a Mach object file\n",
			server->reloc);
		return FALSE;
	}
	close(fd);

	return TRUE;
}

/*
 * Link the .o file against the executable.
 */
boolean_t obj_link(server_t *server)
{
	int			ok;
	struct stat		statb;
	NXStream		*rld_stream;
	const char		*rld_files[3];
	const char		**syms;
	struct mach_header	*rld_header;
	kern_return_t		r;

	if (!rld_mutex) {
		rld_mutex = mutex_alloc();
		mutex_init(rld_mutex);
	}

	kllog(LOG_INFO, "Server %s linking %s against %s\n",
		server->name, server->reloc, server->executable);

	delete_symfile(server->name);

	r = vm_deallocate(task_self(), (vm_address_t)server->header,
		server->reloc_size);
	if (r != KERN_SUCCESS)
		kllog(LOG_ERR, "vm_deallocate in self failed: "
			"%s (%d)\n",
			mach_error_string(r), r);
	kllog(LOG_DEBUG, "Server %s deallocated %#x bytes "
		"at address %#x in self\n",
		server->name, server->reloc_size,
		(vm_address_t)server->header);
	server->alloc_in_self = FALSE;

	/*
	 * Link the relocatable against the executable and libraries to
	 * create the loadable.  We need to hold the rld lock until we've
	 * unloaded the loadable from the rld package.
	 */
	rld_stream = NXOpenMemory(NULL, 0, NX_READWRITE);
	rld_files[0] = server->reloc;
	if (debug && stat(libloadserv_g, &statb) == 0)
		rld_files[1] = libloadserv_g;
	else
		rld_files[1] = libloadserv;
	rld_files[2] = NULL;

	rld_lock();
	rld_server = server;
	rld_address_func((u_long (*)(u_long, u_long))obj_vm_allocate);
	if (rld_executable != server->executable) {
		kllog(LOG_INFO, "Server %s loading executable \"%s\" for "
			"linkage base file\n", server->name,
			server->executable);
		if (rld_executable)
			rld_unload_all(NULL, TRUE);
		rld_executable = server->executable;
		ok = rld_load_basefile(rld_stream, rld_executable);
		if (!ok) {
			kllog(LOG_ERR, "Basefile load failed\n");
			kllog_stream(LOG_WARNING, rld_stream);
			NXCloseMemory(rld_stream, NX_FREEBUFFER);
			rld_executable = NULL;
			rld_unload_all(NULL, TRUE);
			rld_unlock();
			kllog(LOG_ERR, "Server %s couldn't load "
				"executable \"%s\" for linking\n",
				server->name, server->executable);
			return FALSE;
		}
	}

	if (server->loadable)
		kllog(LOG_NOTICE, "Server %s linking relocatable \"%s\" "
			"into loadable \"%s\"\n",
			server->name, server->reloc, server->loadable);
	else
		kllog(LOG_NOTICE, "Server %s linking relocatable \"%s\"\n",
			server->name, server->reloc);

	ok = rld_load(rld_stream, &rld_header, rld_files,
			server->loadable);
	if (!ok) {
		kllog(LOG_ERR, "Link failed\n");
		kllog_stream(LOG_WARNING, rld_stream);
		NXCloseMemory(rld_stream, NX_FREEBUFFER);
		rld_unlock();
		return FALSE;
	}

	/*
	 * Copy the data from the location returned by rld to where
	 * we really want it.
	 */
	memcpy((char *)server->header, (char *)rld_header, server->rld_size);

	/*
	 * Look up needed symbols.
	 */
	for (syms = server->symbols; *syms; syms++) {
		vm_address_t symvalue;
		
		ok = rld_lookup(rld_stream, *syms, (u_long *)&symvalue);
		if (!ok) {
			char *s = malloc(strlen(*syms)+2);
			sprintf(s, "_%s", *syms);
			ok = rld_lookup(rld_stream, s, (u_long *)&symvalue);
			free(s);
		}
		if (!ok) {
			kllog(LOG_ERR, "Server %s can't find symbol \"%s\" "
				"in loadable\n", server->name, *syms);
			kllog_stream(LOG_WARNING, rld_stream);
			NXCloseMemory(rld_stream, NX_FREEBUFFER);
			rld_unload(NULL);
			rld_unlock();
			return FALSE;
		}
		save_symvalue(server->name, *syms, symvalue);
	}

	NXCloseMemory(rld_stream, NX_FREEBUFFER);

	rld_unload(NULL);
	rld_unlock();
	return TRUE;
}
