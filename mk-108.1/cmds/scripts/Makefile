#
# Makefile for the Administration/Public/scripts directory
#

# Comment out this next line if you want to see all of the make output
.SILENT:

#	The directory we are building
PRIVATE_DIR = /private/etc
USR_DIR = /usr/etc

#	The directory that must be created
DSTOWNER = root
DSTGROUP = staff
DSTMODE = 775
DSTDIR = ${DSTROOT}${USR_DIR}
PRIVDIR = ${DSTROOT}${PRIVATE_DIR}

#	Ownership and group of the files and directories installed here
OWNER = root
GROUP = staff

#	Executable scripts
EXECFILES = MAKEDEV
EXECMODE = 755

#	It doesn't mean anything to make in this directory
all:

#	Clean out editor lint
clean:
	echo "    Removing" *~ .[a-zA-Z]*~ \#* *.BAK
	rm -f *~ .[a-zA-Z]*~ \#* *.BAK

install: ${DSTDIR}
	echo -n "    Executable scripts:"
	for i in ${EXECFILES} ; \
	  do \
		echo -n " $$i" ; \
		install -c -o ${OWNER} -g ${GROUP} -m ${EXECMODE} $$i ${DSTROOT}${USR_DIR}/$$i ; \
		rm -f ${DSTROOT}${PRIV_DIR}/$$i; \
		ln -s ../../${USR_DIR}/$$i ${DSTROOT}${PRIVATE_DIR}; \
	  done
	echo "."

${DSTDIR}: DSTROOT ${DSTROOT}
	echo "    Making ${DSTDIR}"
	mkdirs -o ${DSTOWNER} -g ${DSTGROUP} -m ${DSTMODE} ${DSTDIR}
	echo "    Making ${PRIVDIR}"
	mkdirs -o ${DSTOWNER} -g ${DSTGROUP} -m ${DSTMODE} ${PRIVDIR}

$(DSTROOT):
	@echo 'Please make a directory for $@ and try again'; exit 1

DSTROOT:
	@if [ -n "${$@}" ]; then exit 0; else echo Must define $@; exit 1; fi

