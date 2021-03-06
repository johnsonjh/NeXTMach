#!/bin/csh -f
#######################################################################
# HISTORY
# 09-May-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Added -prefix option.
#	[ V5.1(XF11) ]
#
# 17-Apr-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Added "abort" option to top level action prompt.
#	[ V5.1(F8) ]
#
# 28-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Modified to also display prior history messages for the current
#	version and default to "edit" rather than "whist" when any
#	exist.
#	[ V5.1(F8) ]
#
# 16-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Add -delete switch to remove working file after archival.
#	[ V5.1(F7) ]
#
# 13-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Modified to force the creation of a history marker in the file
#	if necessary before calling whist.
#	[ V5.1(F7) ]
#
# 11-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Changed to make include files readable by default when
#	checked-in for the first time.
#	[ V5.1(F6) ]
#
# 07-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Fixed to allow the -version switch to be omitted.
#	[ V5.1(F5) ]
#
#######################################################################

set CO=rcsco
set CI=rcsci
set RCS=rcs

set tempd=/tmp/arch$$
set temp=$tempd/diff
set wtemp=$tempd/whist
set tfile=$tempd/TMP

set prog=$0
set prog=${prog:t}

unset revlog histlog delete
set arch whist exit=0 V="" prefix=""

while ($#argv > 0)
    if ("$argv[1]" !~ -*) break
    switch ("$argv[1]")
    case -v:
    case -version:
	if ($#argv < 2) then
	    echo "${prog}: missing argument to $argv[1]"
	    exit 1
	endif
	shift
	set V="$argv[1]"
        breaksw
    case -prefix:
	if ($#argv < 2) then
	    echo "${prog}: missing argument to $argv[1]"
	    exit 1
	endif
	shift
	set prefix="$argv[1]"
        breaksw
    case -now:
    case -nowhist:
	unset whist
	breaksw
    case -noa:
    case -noarch:
	unset arch
	breaksw
    case -d:
    case -delete:
	set delete
	breaksw
    case -revlog:
	if ($#argv < 2) then
	    echo "${prog}: missing argument to $argv[1]"
	    exit 1
	endif
	shift
	set revlog="$argv[1]"
	breaksw
    case -histlog:
	if ($#argv < 2) then
	    echo "${prog}: missing argument to $argv[1]"
	    exit 1
	endif
	shift
	set histlog="$argv[1]"
	breaksw
    default:
	echo "${prog}: unknown switch: $argv[1]"
	exit 1
    endsw
    shift
end

set branch=-r`expr "$V" : "^\([0-9]*\).*"`

onintr cleanup
mkdir $tempd

foreach file ($*)
    set file="$prefix$file"
    if ("$file:h" == "$file") set file="./$file"
    set rfile="$file:h/RCS/$file:t,v"
    set wfile="$tempd/$file:t"

    echo "[ ${file} ]"
    rm -f $tfile
    if (-f $rfile) then
	$CO -q -p $rfile >$tfile
	if ("$status" != 0) continue
	unset first
    else
	set first
	echo "(no previous version)"
    endif

    unset s abort

    if ($?whist) then
	#
	#  Loop comparing the previous and current versions until 
	#  no further edits or history messages are requested.
	#
	set defwhist=whist
	while (1)
	    set whist=$defwhist
	    if (-f $tfile) then
		echo "- - - - - - - - - -" >$temp
		awk '\
BEGIN {V="[ V'$V' ]"; L=length(V)-1; E=1;}\
s==0 && $0~/HISTORY/ {s=1; next;}\
s==1 {i=index($0,V);if(i && length($0)==(i+L)){E=0;exit;}else{print $0;}}\
END {exit E;}' <$tfile >>$temp
		if ($status == 0 && "$whist" == "whist") then
		    echo "- - - - - - - - - -" >>$temp
		    set whist="edit"
		else
		    echo >$temp
		endif
		diff $tfile $file >> $temp
		set s=$status
	    else
		if ("$whist" == "whist") set whist=done
		echo >$temp
		set s=2
	    endif
	    if ("$s" == 0) then
		rm -f $temp
		break
	    endif
	    <$temp (rm -f $temp; more)
	    while (1)
		if ("$whist" =~ [A-Z]*) then
		    set ans="$whist"
		else
		    echo -n "Action?  [$whist]  "
		    set ans="$<"
		endif
		if ("$ans" == "") set ans="$whist"
		switch ("$ans")
		    case [Aa][Bb]:
		    case [Aa][Bb][Oo][Rr][Tt]:
			rm -i $file
			set abort ans="done"
			breaksw
		    case [Ee]:
		    case [Ee][Dd][Ii][Tt]:
			chmod +w $file
			$EDITOR $file
			if ("$whist" == "EDIT") set whist=WHIST
			breaksw
		    case [Ww]:
		    case [Ww][Hh][Ii][Ss][Tt]:
			echo >$wtemp
			fgrep -s HISTORY $file
			if ("$status" != 0) then
			    echo "[ must add HISTORY marker line first ]"
			    set whist=EDIT
			    breaksw
			endif
			set whist="$defwhist"
			unset wdone
			echo "[ Enter HISTORY message for $file ]"
			$EDITOR $wtemp
			set def=update
			while (1)
			    echo -n "Action (update, edit, type, abort, ?)  [$def]  "
			    set ans="$<"
			    if ("$ans" == "") set ans="$def"
			    set def=update
			    switch ("$ans")
				case "u":
				case "update":
				    set e=$file:e
				    switch ("$e")
					case "h":
					case "c":
					    set ll=63 t=""
					    breaksw;
					case "s":
					    set ll=63 t="-t c"
					    breaksw;
					case "csh":
					    set ll=64 t=""
					    breaksw
					default:
					    set ll=64 t="-t makefile"
					    breaksw
				    endsw
				    awk <$wtemp '{\
				      if(length($0)>'$ll'){\
				        print "[ line " NR " too long (" length($0) ">'$ll') ]";\
				        exit 123;\
				      }\
				    }'
				    if ("$status" == 0) then
					if ("$V" != "") then
					    echo "[ V$V ]" >>$wtemp
					endif
					chmod +w $file
					whist - $t $file <$wtemp
					set wdone
				    else
					set def=edit
				    endif
				    breaksw
				case "e":
				case "edit":
				    $EDITOR $wtemp
				    breaksw
				case "t":
				case "type":
				    cat $wtemp
				    breaksw
				case "ab":
				case "abort":
				    set wdone
				    breaksw
				default:
				    cat <<!
[
  Options are:

  u, update	Add HISTORY message to $file
  e, edit	Edit HISTORY message for $file
  t, type	Type HISTORY message for $file
  ab, abort	Abort HISTORY message for $file
]
!
				    breaksw
			    endsw
			    set ans=whist
			    if ($?wdone) then
		                break
			    endif
		        end
			set defwhist=done
			breaksw
		    case [Dd]:
		    case [Dd][Oo][Nn][Ee]:
			set ans="done"
			breaksw
		    default:
			set ans=""
			cat <<!
[
  Options are:

  ab, abort	Abort and remove $file
  d,  done	Archive $file and go on to next
  e,  edit	Edit $file
  w,  whist	Add HISTORY message to $file
]
!
			breaksw
		endsw
		if ("$ans" != "" && "$whist" !~ [A-Z]*) break
	    end
	    if ("$ans" == done) break
	end
    endif

    #
    #  Without -noarch, archive the current file.  We do this from
    #  a temporary copy since the check-in would otherwise delete the
    #  original (or "touch" it again on check-out).  Thus, we also must
    #  manually re-protect the original and unlock the RCS file (as long
    #  as nothing went wrong).
    #
    if ($?arch && ! $?abort) then
	if (! $?s) then
	    cmp -s $tfile $file
	    set s="$status"
	endif
	if ("$s" != 0) then
	    rm -f $wfile
	    if ($?delete) then
		set twfile=$file
	    else
		set twfile=$wfile
		cp $file $wfile
	    endif
	    if (-f $rfile) then
		$RCS -q -l $rfile
		set s="$status"
	    else
		set s=0
	    endif
	    if ("$s" == 0) then
		$CI -t/dev/null -m" " $branch $twfile $rfile
		set s="$status"
	    endif
	else
	    if ($?delete) then
	        rm -f $file
	    endif
	    rcs -q -l $rfile
	    rcs -q -u $rfile
	endif
	sync
	if ("$s" != 0) then
	    @ exit = ("$exit" || 1)
	else
	    if ($file:e == "h" && $?first) then
		set mode=",a+r"
	    else
		set mode=""
	    endif
	    if (! $?delete) then
	        chmod -w$mode $file
	    endif
	endif
    endif
end

cleanup:
rm -rf $tempd

exit $exit
