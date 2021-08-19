#!/bin/csh -f
######################################################################
# HISTORY
######################################################################

set prog="$0"
set prog="${prog:t}"

set SRC=/4.3/usr/src
set syslink_dat=/usr/cs/lib/syslink.dat
set I=""
set DST="$cwd"
set DESTDIR=/dist/ALPHA

set auxfiles=(/bin/as)

unset build_flag install_flag

set configs=()
set clean=""

while ($#argv > 0)
    if ("$1" !~ -*) break
    switch ("$1")
	case "-b":
	case "-build":
	    set build_flag; breaksw
	case "-i":
	case "-install":
	    set install_flag; breaksw
	case "-c":
	case "-config":
	    if ($#argv < 2) then
	        echo "${prog}: missing configuration name for $1"; exit 1
	    endif
	    set configs=($configs $2)
	    shift
	    breaksw
	case "-destdir":
	    if ($#argv < 2) then
	        echo "${prog}: missing destination directory for $1"; exit 1
	    endif
	    set destdir="$2"
	    shift
	    breaksw
	case "-I"*:
	    set I="$1"
	    breaksw
	case "-noclean":
	    set noclean
	    breaksw
	case "-src":
	    if ($#argv < 2) then
	        echo "${prog}: missing argument to $1"; exit 1
	    endif
	    set SRC="$2"
	    shift
	    breaksw
	case "-syslink":
	    if ($#argv < 2) then
	        echo "${prog}: missing argument to $1"; exit 1
	    endif
	    set syslink_dat="$2"
	    shift
	    breaksw
	default:
	    echo "${prog}: unknown switch: $1"; exit 1
    endsw
    shift
end

if ($#argv > 0) then
    set files=($argv)
else
    set files=(`awk <$syslink_dat '$0 !~ /^#/{print $1;}'`)
endif

if ($#configs == 0) set configs=(DEFAULT)

set failed=()

foreach file ($auxfiles CONFIGS $files)
    if ("$file" == "CONFIGS") then
	set CONFIGS=($configs)
	continue
    endif
    set dir=${file}
    set program=${file:t}
    set DIR=${SRC}${dir}
    if (-d "$DIR") then
	set target="" #${program:t}
	set install=install
	if (! $?noclean) set clean="make clean"
    else
	set dir=${dir:h}
	set DIR=${SRC}${dir}
	if (! -f "${DIR}/RCS/${program}.c,v") then
	    echo "[ no source for $file - skipped ]"
	    continue
	endif
	set target=${program}.x
	set install=${program}.install
	if (! $?noclean) set clean="rm -f $target"
    endif
    set RCS=${dir}/RCS
    /usr/cs/etc/makepath $DST$RCS
    rm -f $DST$RCS
    ln -s $SRC$RCS $DST$RCS
    foreach f (VAX SUN IBMRT)
	set LINK=${dir}/$f/RCS
	if (-d $SRC$LINK) then
	    /usr/cs/etc/makepath $DST$LINK
	    rm -f $DST$LINK
	    ln -s $SRC$LINK $DST$LINK
	endif
    end
    set build=$DST${dir}

    if (! $?CONFIGS) then
	continue
    endif

    foreach config ($CONFIGS)
 	set ok
	set makeflags='CENV='"$I"' -DCONFIG_'"$config"'="" -DKERNEL_FEATURES'
	if ($?build_flag) then
	    echo "[ Executing 'make $target' in $build ]"
	    (cd $build; $clean; exec make $makeflags:q $target)
	    if ("$status" != 0) then
	        unset ok
		echo "*** ${file}: $config configuration failed ***"
		set failed=($failed $file)
	    endif
	endif
	if ($?install_flag && $?ok) then
	    echo "[ Executing 'make $install' from $build ]"
	    (cd $build; exec make DESTDIR=$destdir $makeflags:q $install)
	    if ("$status" != 0) then
		echo "*** ${file}: $config configuration not installed ***"
		set failed=($failed $file)
	    endif
	endif
    end
end

if ($#failed > 0) then
    echo "*** Failures: $failed ***"
endif
