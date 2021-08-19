#!/bin/csh -f
######################################################################
# HISTORY
# 21-Feb-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Fixed to optimize comparison of current directory against
#	itself to avoid actually comparing the identical files.
#	[ V5.1(F3) ]
#
######################################################################

set prog=$0
set prog=${prog:t}

unset yes
unset new
unset part2
set move=""
set mdir=""
set xmdir=""
while ($#argv > 0)
    if ("$1" =~ -*) then
        switch ("$1")
	    case "-yes":
		set yes
	        breaksw
	    case "-xmerge":
	    case "-xmerge="*:
		set xmerge="$1"
		set xmdir=`expr "$1" : ".*=\(.*\)"`
	        breaksw
	    case "-merge":
	    case "-merge="*:
		set merge="$1"
		set mdir=`expr "$1" : ".*=\(.*\)"`
	        breaksw
	    case "-new":
		set new
	        breaksw
	    case "-part2":
		set part2
	        breaksw
	    default:
		echo "${prog}: unkown switch $1"
		exit 1
		breaksw
	endsw
	shift
    else
	break
    endif
end

if ($#argv != 1) then
    echo  "Usage: ${prog} <w.r.t-directory>"
    exit 0
endif

set dir=$1
if ("$mdir" == "") set mdir="$dir"
if ("$xmdir" == "") set xmdir="$dir"
if ($?merge) set move=(M:$mdir $move)
if ($?xmerge) set move=(X:$xmdir $move)
if ($?new) set move=(X-NEW $move)
if ("$move" != "") set move="($move) "

if (! $?part2) then
    if (! $?yes) then
	echo -n "Prune $cwd against $dir ${move}- Confirm?  [ no ]  "
	set ans="$<"
	if ("$ans" !~ [Yy] && "$ans" !~ [Yy][Ee][Ss]) exit 0
    else
	echo "[ Pruning $cwd against $dir ]"
    endif

    set args=($dir)
    if ($?merge) then
	set args=($merge $args)
    endif
    if ($?xmerge) then
	set args=($xmerge $args)
    endif
    if ($?new) then
	set args=(-new $args)
    endif
    find -nosymlink . -type f -print | exec $0 -part2 $args
endif

while (1)
    set f="$<"
    if ($f == "") break
    if ("$dir" == ".") then
	set s=0
    else
	cmp -s $f $dir/$f
	set s="$status"
    endif
    if ("$s" == 0) then
	if ($?xmerge) then
	    set d=()
	    set t="$xmdir/$f"
	    while (! -d "$t:h")
		set t="$t:h"
		set d=($t:q $d:q)
	    end
	    if ($#d > 0) mkdir $d
	    mv $f $xmdir/$f
	    echo "M $f => $xmdir/$f"
	else
	    echo "x $f"
	    rm -f $f
	endif
    else
	if ($?new && ! -f $dir/$f) then
	    if ($?xmerge) then
		set d=()
		set t="$xmdir/$f"
		while (! -d "$t:h")
		    set t="$t:h"
		    set d=($t:q $d:q)
		end
		if ($#d > 0) mkdir $d
		mv $f $xmdir/$f
		echo "M $f => $xmdir/$f"
	    else
		echo "X $f"
		rm -f $f
	    endif
	else
	    if ($?merge) then
		mv $f $mdir/$f
		echo "M $f => $mdir/$f"
	    else
		echo "  $f"
	    endif
	endif
    endif
end
