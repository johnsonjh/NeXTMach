#
# Copyright (c) 1980, 1986 Regents of the University of California.
# All rights reserved.  The Berkeley software License Agreement
# specifies the terms and conditions for redistribution.
#
#	@(#)Makefile	7.1 (Berkeley) 6/5/86
#
DSTROOT=
DSTDIR= /usr/standalone
DSTDIRDIAG= /private/tftpboot
INCPATH= -I.. -I../mon
COPTS=	${INCPATH} -DSUN_NFS -DSTANDALONE -bsd -DMONITOR
CFLAGS=	-O ${COPTS}
BOOT_RELOC=	04380000
DIAG_RELOC=	04340000

CC=cc
AS=as
LD=ld

SRCS=	sacrt0.s sys.c conf.c prf.c boot.c dma.c misc.s \
	sainit.c sc.c sd.c cksum.c od.c zs.c ctype_.c en.c tftp.c fd_driver.c \
	fd_subr.c fd_io.c fd_cmds.c ../next/libc.s

DRIVERS= sc.o sd.o dma.o od.o zs.o en.o tftp.o fd_driver.o fd_subr.o fd_io.o fd_cmds.o

HFILES=	fd_extern.h saio.h scsivar.h zsreg.h enreg.h screg.h \
	trap.h exec.h fd_vars.h scsireg.h varargs.h

LIBSA=	libsa.a

ALL=	boot diagnostics

all: ${ALL}

${LIBSA}: sys.o conf.o sainit.o ${DRIVERS} prf.o cksum.o ctype_.o libc.o misc.o
	rm -rf ${LIBSA}
	ar crv ${LIBSA} sys.o conf.o sainit.o ${DRIVERS} prf.o cksum.o ctype_.o libc.o misc.o
	ranlib ${LIBSA}

sacrt0.o: .depend_done sacrt0.s
	${CC} -E ${COPTS} sacrt0.s >xx.s
	${AS} xx.s -o sacrt0.o
	rm -f xx.s

misc.o: .depend_done misc.s
	${CC} -E ${COPTS} -DASSEMBLER misc.s >xx.s
	${AS} xx.s -o misc.o
	rm -f xx.s

libc.o: .depend_done ../next/libc.s
	${CC} -E ${COPTS} -DASSEMBLER ../next/libc.s >xx.s
	${AS} xx.s -o libc.o
	rm -f xx.s

BOOTOBJS=	sacrt0.o boot.o
boot: 	.depend_done ${BOOTOBJS} bootvers.o ${LIBSA}
	${LD} -p -segaddr __TEXT ${BOOT_RELOC} -o $@ ${BOOTOBJS} bootvers.o ${LIBSA}

bootvers.c: ${LIBSA} ${BOOTOBJS}
	@rm -f $@
	vers_string -c boot > $@

DIAGOBJS=	sacrt0.o diagnostics.o
diagnostics: 	.depend_done ${DIAGOBJS} diagvers.o ${LIBSA}
	${LD} -p -segaddr __TEXT ${DIAG_RELOC} -o $@ ${DIAGOBJS} diagvers.o ${LIBSA}

diagvers.c: ${DIAGOBJS}
	@rm -f $@
	vers_string -c diag > $@

# utilities

print:
	@pr makefile
	@ls -l | pr 
	@pr *.h *.c

clean:
	rm -f *.o errs Makedep .depend_done
	rm -f a.out boot
	rm -f boot[a-z]? boot[a-wyz][a-z].c conf[a-wyz][a-z].c diagnostics
	rm -f tags
	touch Makedep

lint:
	lint ${COPTS} -hxbn boot.c diagnostics.c ${SRCS} | \
	    grep -v 'possible pointer alignment' | \
	    grep -v 'struct/union .* never defined'

tags:	
	ctags $(SRCS) $(HFILES)

install: all DSTROOT ${DSTROOT}${DSTDIR} DSTROOT ${DSTROOT}${DSTDIRDIAG}
	install -s -m 444 -l ${DSTROOT}${DSTDIRDIAG}/boot boot ${DSTROOT}${DSTDIR}
	install -s -m 444 diagnostics ${DSTROOT}${DSTDIRDIAG}

DSTROOT:
	@if [ -n "${$@}" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi

${DSTROOT}${DSTDIR}:
	mkdirs $@

${DSTROOT}${DSTDIRDIAG}:
	mkdirs $@

depend .depend_done:
	-rm -f Makedep
	${CC} ${COPTS} -M ${INCPATH} ${SRCS} | \
	awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$2) > 78) { print rec; rec = $$0; } \
		else rec = rec " " $$2 } } \
		END { print rec } ' > Makedep
	-touch .depend_done

-include Makedep







