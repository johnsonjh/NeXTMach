#
#	Makefile for CDROM file system - loadable image
#
#
# This makefile makes both the loadable file system, the NFS server,
# and the stand-alone utilities.
#
NAME=CDROM
CFILES= cfs_vnodeops.c cfs_vfsops.c cfs_subr.c cdrom.c unix_rw.c load_unld.c
HFILES= cdrom.h cfs.h
SRCFILES= $(CFILES) $(HFILES) Makefile Load_Commands.sect Unload_Commands.sect
OFILES= $(CFILES:.c=.o)

# 	keep DSTDIR up to date with FS_DIR_LOCATION in 
#		/usr/include/next/loadable_fs.h
DSTDIR=/usr/filesystems/CDROM.fs


# comment these out to avoid producing a cfs_loadable at load time
#
# DEBUGLOAD= -d ./$(NAME)_loadable
# GDB_SYMS= -g

CFLAGS= -O -MD -DKERNEL -DKERNEL_FEATURES -DMACH -DNEXT -DNeXT_MOD -I.. $(GDB_SYMS) -Wall 

all:	$(NAME)_reloc

install: $(DSTROOT)$(DSTDIR) $(NAME)_reloc
	install $(NAME)_reloc $(DSTROOT)$(DSTDIR)

installsrc: $(SRCDIR)
	tar cf - $(SRCFILES) | (cd $(SRCDIR); tar xpf -)
	
$(SRCDIR) $(DSTROOT)$(DSTDIR):
	mkdirs $@

clean:
	-rm -rf $(NAME)_reloc $(NAME)_reloc_g $(OFILES) \
		*.d Makedep Server_Name.seg \
		Executable_Name.seg Instance_Var.seg 

vers.o: $(OFILES)
	@rm -f $*.c
	vers_string -c $(NAME)_reloc > $*.c
	$(CC) -c $*.c -o $*.o

tags:
	etags *.c ../cfs.util/*.c

$(NAME)_reloc $(NAME)_reloc_g: $(OFILE_DIR) $(OFILES) vers.o \
		Load_Commands.sect Unload_Commands.sect
	kl_ld -n $(NAME) -l Load_Commands.sect -u Unload_Commands.sect \
		-i cfs_ks_var  $(DEBUGLOAD) -o $@ $(OFILES) vers.o
				




