#!/bin/csh -f
#
######################################################################
# HISTORY
# 09-Oct-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Add -old companion switch to -update and log file parameter to
#	-onlynew.
#	[ V5.1(XF18) ]
#
# 07-Oct-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Add -R companion switch for use with -update.
#	[ V5.1(XF17) ]
#
# 08-May-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Added -autoupdate and -onlynew options for domerge.
#	[ V5.1(XF11) ]
#
######################################################################
#

set prog=$0
set base=$prog:h
set base=$base:h
set prog=${prog:t}

set options=()

while ($#argv > 0)
    if ("$1" =~ -*) then
        switch ("$1")
	    case "-c":
	    case "-collapse":
		set collapsef
		breaksw
	    case "-d":
	    case "-delete":
		set deletef
		breaksw
	    case "-m":
	    case "-merge":
		echo "${prog}: missing branch number with $1"
		exit 1
		breaksw
	    case -m[0-9]*:
	    case -merge[0-9]*:
		set branch=`expr "$1" : '-m[a-z]*\([0-9.]*\)$'`
		set mergef
		breaksw
	    case "-restart":
		if ($#argv < 2) then
		    echo "${prog}: missing argument to $1"
		    exit 1
		endif
		set options=($options -restart $2)
		shift
		breaksw
	    case "-au":
	    case "-autoupdate":
		if ($#argv < 2) then
		    echo "${prog}: missing argument to $1"
		    exit 1
		endif
		set options=($options -autoupdate $2)
		shift
		breaksw
	    case "-on":
	    case "-onlynew":
		if ($#argv < 2) then
		    echo "${prog}: missing argument to $1"
		    exit 1
		endif
		set options=($options -onlynew $2)
		shift
		breaksw
	    case "-u":
	    case "-update":
		set updatef=v
		breaksw
	    case "-R":
		set updatef="${updatef}R"
		breaksw
	    case "-o":
	    case "-old":
		set updatef="${updatef}o"
		breaksw
	    case "-prune":
	    	set prunef
		breaksw
	    default:
		echo "${prog}: unknown switch $1"
		exit 1
		breaksw
	endsw
	shift
    else
	break
    endif
end

if ($#argv != 1) then
    cat <<!
Usage:

[ update collection into "REMERGE" area ]
${prog} -update|-u [ -R ] [ -old|-o ] <coll>

[ prune "REMERGE" area against $base and its base (implicit with -update) ]
${prog} -prune <coll>

[ merge collection with $base ]
${prog} -merge [ -restart <file> ] [ -autoupdate|-au <logfile> ] <coll>
${prog} -merge [ -restart <file> ] <coll>
${prog} -merge [ -restart <file> ] [ -onlynew|-on <logfile> ] <coll>

[ collapse "REMERGE" area into its base ]
${prog} -collapse|-c <coll>

[ purge collection of files removed from repository ]
${prog} -delete|-d <coll>
!
    exit 0
endif

set coll="$1"
set supfile="$base/sup/$coll/supfile"
set gone="$base/sup/$coll/gone"
set last="$base/sup/$coll/last"
if (! -r $supfile) then
    echo "${prog}: ${coll}: no such collection in $base"
    exit 1
endif

set dir=`sed <$supfile -n -e "/^${coll} /s;.* prefix=\([^ 	]*\)/REMERGE .*;\1;p"`

if ($?updatef) then
    sup -${updatef} $supfile
    set prunef
endif

if ($?prunef) then
    cd $dir/REMERGE
    prune -xmerge=$dir $base
    exec prune -xmerge $dir
endif

if ($?mergef) then
    cd $dir/REMERGE
    exec domerge -r$branch $options $base $dir
#   cd $base
#   check_cond -restart ${dir}4.3
#   exec check_cond -restart ${dir}4.3
endif

if ($?collapsef) then
    cd $dir/REMERGE
    prune -new -xmerge=$dir .
    (cd $dir; find -nosymlink [a-z]* -print | sort) >$last
    sed <$supfile -n -e "/^${coll} /s;/REMERGE ; ;p" \
    | \
    sup -fv - \
    |& \
    tee /dev/tty \
    | \
    exec sed -n -e "s;^SUP Would delete ;;p" >$gone
    exit 0
endif

if ($?deletef) then
    foreach d ($dir $base)
	if (! -w $d) continue
	sed <$gone -n \
	    -e "s,^file \(.*\),set f=$d/\1;rm </dev/tty -fi "'$f $f:h/RCS/$f:t'",p" \
	    -e "s,^directory \(.*\),set f=$d/\1;rm </dev/tty -rfi "'$f'",p" \
        | \
	csh -f
    end
    exit 0
endif
