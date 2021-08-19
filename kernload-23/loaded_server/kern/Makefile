MIG= mig
MIGOPTS = -I.. -DKERNEL
MIGKERNEL = -DKERNEL_SERVER
SOURCEFILES= Makefile
FEATURE_FILES= mach_ipc_stats.h mach_ipc_xxxhack.h mach_xp.h mach_net.h mach_np.h
GEN_HFILES=mach_interface.h mach_user_internal.h mach_host.h mach_host_user_internal.h exc.h exc_user_internal.h
GEN_MIGFILES=mach_migfiles mach_host_migfiles exc_migfiles

INCDIR=/usr/include

all: Makefile.gen
	make -f Makefile.gen all

Makefile.gen: Makefile $(GEN_HFILES) $(GEN_MIGFILES)
	rm -f Makefile.gen
	echo "CFILES=`echo *.c`"			> Makefile.gen
	echo						>> Makefile.gen
	echo 'OFILES=$$(CFILES:.c=.o)'			>> Makefile.gen
	echo 'CFLAGS=-g -O -MD -Wall -DMACH -DKERNEL_FEATURES' \
							>> Makefile.gen
	echo						>> Makefile.gen
	echo 'all: $$(OFILES)'				>> Makefile.gen
	echo '	-md -u Makedep -d *.d'			>> Makefile.gen
	echo						>> Makefile.gen
	echo '-include Makedep'				>> Makefile.gen

mach_interface.h:
	@rm -f mach_interface.h
	echo '#include "mach_user_internal.h"'		> mach_interface.h
	echo '#include <kern/mach_interface.h>'		>> mach_interface.h

mach_migfiles: $(INCDIR)/kern/mach.defs $(FEATURE_FILES)
	rm -fr gen; mkdirs gen
	cd gen; ${MIG} ${MIGOPTS} ${MIGKERNEL} -i -server mach_server.c \
		-header mach_interface.h $(INCDIR)/kern/mach.defs
	rm -f gen/xxx_* gen/mach_server.c gen/mach_interface.h
	cd gen; for i in *mach_ports_*.c; do mv $$i `echo $$i | sed 's/mach_ports_/mp_/'`; done
	cd gen; for i in *port_extract*.c; do mv $$i `echo $$i | sed 's/port_extract/p_extract/'`; done
	cd gen; for i in *port_insert*.c; do mv $$i `echo $$i | sed 's/port_insert/p_insert/'`; done
	cd gen; for i in *port_set*.c; do mv $$i `echo $$i | sed 's/port_set/pset/'`; done
	cd gen; install -d -c -u *.c ..
	rm -f mach_migfiles; touch mach_migfiles
	rm -rf gen

mach_user_internal.h: $(INCDIR)/kern/mach_interface.h
	@rm -f mach_user_internal.h
	-egrep 'Routine ' < $(INCDIR)/kern/mach_interface.h | sed -e 's/.*Routine \([a-z_]*\).*/#define \1 \1_EXTERNAL/' > mach_user_internal.h

mach_host.h:
	@rm -f mach_host.h
	echo '#include "mach_host_user_internal.h"'	> mach_host.h
	echo '#include <kern/mach_host.h>'		>> mach_host.h

mach_host_migfiles: $(INCDIR)/kern/mach_host.defs $(FEATURE_FILES)
	rm -fr gen; mkdirs gen
	cd gen; ${MIG} ${MIGOPTS} ${MIGKERNEL} -i -server mach_host_server.c \
		-header mach_host.h $(INCDIR)/kern/mach_host.defs
	rm -f gen/mach_host_server.c
	cd gen; for i in *processor_set*.c; do mv $$i `echo $$i | sed 's/processor_set/prset/'`; done
	cd gen; for i in *processor*.c; do mv $$i `echo $$i | sed 's/processor/proc/'`; done
	rm -f gen/xxx_* gen/mach_host_server.c gen/mach_host.h
	cd gen; install -d -c -u *.c ..
	rm -f mach_host_migfiles; touch mach_host_migfiles
	rm -rf gen

mach_host_user_internal.h: $(INCDIR)/kern/mach_host.h
	@rm -f mach_host_user_internal.h
	-egrep 'Routine ' < $(INCDIR)/kern/mach_host.h | sed -e 's/.*Routine \([a-z_]*\).*/#define \1 \1_EXTERNAL/' > mach_host_user_internal.h

exc.h:
	@rm -f exc.h
	echo '#include "exc_user_internal.h"'		> exc.h
	echo '#include <kern/exc.h>'			>> exc.h

exc_migfiles: $(INCDIR)/kern/exc.defs $(FEATURE_FILES)
	rm -fr gen; mkdirs gen
	cd gen; ${MIG} ${MIGOPTS} ${MIGKERNEL} -i -server exc_server.c \
		-header exc.h $(INCDIR)/kern/exc.defs
	rm -f gen/xxx_* gen/exc_server.c gen/exc.h
	cd gen; install -d -c -u *.c ..
	rm -f exc_migfiles; touch exc_migfiles
	rm -rf gen

exc_user_internal.h: $(INCDIR)/kern/exc.h
	@rm -f exc_user_internal.h
	-egrep 'Routine ' < $(INCDIR)/kern/exc.h | sed -e 's/.*Routine \([a-z_]*\).*/#define \1 \1_EXTERNAL/' > exc_user_internal.h

mach_ipc_stats.h: $(INCDIR)/next/FEATURES.h
	@rm -f mach_ipc_stats.h
	egrep MACH_IPC_STATS $(INCDIR)/next/FEATURES.h > mach_ipc_stats.h

mach_ipc_xxxhack.h: $(INCDIR)/next/FEATURES.h
	@rm -f mach_ipc_xxxhack.h
	egrep MACH_IPC_XXXHACK $(INCDIR)/next/FEATURES.h > mach_ipc_xxxhack.h

mach_xp.h: $(INCDIR)/next/FEATURES.h
	@rm -f mach_xp.h
	egrep MACH_XP $(INCDIR)/next/FEATURES.h > mach_xp.h

mach_net.h: $(INCDIR)/next/FEATURES.h
	@rm -f mach_net.h
	egrep MACH_NET $(INCDIR)/next/FEATURES.h > mach_net.h

mach_np.h: $(INCDIR)/next/FEATURES.h
	@rm -f mach_np.h
	egrep MACH_NP $(INCDIR)/next/FEATURES.h > mach_np.h

installsrc: $(SRCDIR)
	tar cf - $(SOURCEFILES) | (cd $(SRCDIR); tar xpf -)

clean:
	-rm -f *.o *.c *.h *.d *_migfiles Makedep Makefile.gen
	-rm -rf gen

$(SRCDIR):
	mkdirs $@
