
#
#  NeXT Internal Library Makefile
#  Copyright 1987, NeXT, Inc.
#
#  This Makefile is used for maintaining and installing NeXT software
#  libraries.
#
#  Fill in values for the various variables at the places marked by
#  **number**.  Any rules or dependencies of your own can be added
#  at the end of the file.
#

#directory where make gets its includes.  Do not edit.
MAKE_DIR = /usr/local/lib/Makefiles

#  **1**  -LIBRARY NAME-
#  The base name of the library goes here. This should also be the same
#  name as the SGS project the library is built under.  For example, if
#  you enter "appkit" as the name, then the name of the SGS project
#  should be "appkit", and the library created will be "libappkit.a".
NAME = streams

#  **2**  -SOURCE FILES-
#  Here list the .m, .c, .psw, .pswm, .s, .l, .lm, .y, .ym, .ps and
#  .spec files.  You may delete any lines that you don't use.
#  OTHER_SRCS is used for files with other suffixes that you wish
#  to be treated like source (i.e., printed with the source, copied
#  with the source, etc.).  For files in OTHER_SRCS, you need to
#  specify the corresponding .o file in OTHER_SRCS_OFILES.
CFILES = streams.c streams_internal.c indir_ops.c \
	file_funcs.c ipc_funcs.c memory_funcs.c \
	streams_doscan.c streams_doprnt.c globals.c
OTHER_SRCS = StreamsNotes.rtf
OTHER_SRCS_OFILES =

#  **3**  -COMMAND OPTIONS-
#  These are passed as arguments to the Objective-C preprocessor, the C
#  compiler, pswrap, the assembler, lex and yacc.  You may delete any
#  lines that you don't use.  All CFLAGS also get passed to Objective-C. 
#  DEBUGCFLAGS is substituted for CFLAGS when "debug" is made.
#  PROFCFLAGS is substituted for CFLAGS when "profile" is made.
#  Under CFLAGS, for each library that you use that supplies class
#  and message group files, you must use a -I$(OBJDIR)/XXX, where
#  XXX is the base name of the library.
COMMONCFLAGS = -I. -Wall
CFLAGS = -O $(COMMONCFLAGS)
DEBUGCFLAGS = -g $(COMMONCFLAGS)
PROFCFLAGS = -O -pg $(COMMONCFLAGS)
SHLIBCFLAGS = -O -g $(COMMONCFLAGS) -I$(SHLIBIDIR)


#  **4**  -INCLUDE FILES-
#  Private include files are used by the library's code, but not needed
#  by the library's users.  Public include files are needed by others who
#  will use the library, and must be installed along with the library.
PRIVATE_INCFILES = defs.h
PUBLIC_INCFILES = streams.h streamsimpl.h

#  **6**  -BY_PRODUCTS GENERATED FROM BUILDS-
#  If your build procedure creates any files other than .o files and the
#  .c and .m files left over from pswrap, you should list them here so
#  they are removed when a make clean is done.
BY_PRODUCTS =

#this file included for standard functionality
include $(MAKE_DIR)/Makefile.lib


#  **7**  -ADDITIONAL INSTALLS-
#  If there are other actions that you need to do during an installation,
#  add them as actions under the below target, "additional_installs".
#  These actions will be carried out right after the library, include
#  files, class, and message group files have been installed.
all: profile_ofiles
install: profile_ofiles common_install relnotes_install

installhdrs: inst_incfiles

#  any extra rules or dependencies can be added after this line
#include Makefile.depends

relnotes_install:
	mkdirs $(DSTROOT)/usr/local/RelNotes/
	install -c -m 444 StreamsNotes.rtf $(DSTROOT)/usr/local/RelNotes/
