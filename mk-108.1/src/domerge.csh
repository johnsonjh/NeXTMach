#!/bin/csh -f
#
######################################################################
# HISTORY
# 09-Oct-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Add log file parameter to -onlynew;  changed to display binary
#	file message before ignoring file.
#	[ V5.1(XF18) ]
#
# 08-May-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Added -autoupdate option to automatically install all updates
#	for which the current and original files are identical (i.e.
#	only the new file has changed) with the diff ouptut logged to
#	the file named as the switch argument and all other new files
#	ignored; added -onlynew option to consider only those new files
#	which don't currently exist with all other new files ignored.
#	
#	[ V5.1(XF11) ]
#
# 16-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Flush check-in of "original" version (which was merged) prior
#	to installing merged version since this just confuses things.
#	[ V5.1(F7) ]
#
# 21-Feb-87  Mike Accetta (mja) at Carnegie-Mellon University
#	Updated to support wider range of options at first merge
#	prompt and insert the two-way differences under a OOPS
#	conditional for manual editing when no prior base version
#	exists from which to do a three-way merge;  fixed to create
#	RCS sub-directory in any new directory;  added sync after
#	installing new file;  fixed to exit with error if file to be
#	merged is protected.
#	[ V5.1(F3) ]
#
######################################################################
#

set prog=$0
set prog=${prog:t}

set CI=rcsci
set CO=rcsco
set RCS=rcs

unset restart autoupdate onlynew

set branch=
set version=

while ($#argv > 0)
    if ("$1" =~ -*) then
        switch ("$1")
	    case -r[0-9]*:
		set branch=`expr "$1" : "-r\(.*\)"`
		breaksw
	    case "-autoupdate":
		if ($#argv < 2) then
		    echo "${prog}: missing argument to $1"
		    exit 1
		endif
		shift
		set autoupdate="$1"
	        breaksw
	    case "-onlynew":
		if ($#argv < 2) then
		    echo "${prog}: missing argument to $1"
		    exit 1
		endif
		shift
		set onlynew="$1"
	        breaksw
	    case "-restart":
		if ($#argv < 2) then
		    echo "${prog}: missing argument to $1"
		    exit 1
		endif
		shift
		set restart=$1
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

if ($#argv != 2) then
    echo  "Usage: ${prog} <into-directory> <old-directory>"
    exit 0
endif

set dir=$1
set odir=$2

if ($?EDITOR) then
    set tmp="$EDITOR"
    if ($tmp:t == "vi") set vimode
else
    setenv EDITOR /usr/ucb/vi
    set vimode
endif

if ($?branch) then
    set version=(-version $branch)
endif

set tmp=/tmp/${prog}$$
set xtmp=$tmp/X
set TMP=$tmp/TMP
set TMP2=$tmp/TMP2
set fprefix=${tmp}/F-
onintr cleanup
mkdir $tmp $xtmp

echo "[ Merging from $odir to $cwd into $dir ]"
find -nosymlink . -type f -print | sed -e "s;^\./;;" | sort >$TMP

split -350 $TMP $fprefix

foreach fp (${fprefix}*)
  foreach newfile (`cat $fp`)

    if ($?restart) then
        if ("$newfile" != "$restart") continue
    endif
    unset restart

    echo ""
    echo "[ $newfile ]"
    unset ignore
    unset merged
    unset didmerge
    unset differ

    set tmpfile=$tmp/$newfile:t
    set xtmpfile=$xtmp/$newfile:t
    set dstfile=$dir/$newfile
    set oldfile=$odir/$newfile
    set origfile=$newfile

    rm -f $tmpfile $xtmpfile

    #  Loop until file matches exp version or no installation
    while (! $?ignore)

	#  Loop until no more editing requested
	while (1)
	    set install=yes
	    set merge=yes
	    set edit=no
	    set editat=""
	    #  File currently exists
	    if (-f $dstfile) then
		if ($?onlynew) then
		    set ignore
		    break
		endif
		echo "[ comparing $dstfile and $newfile ]"
		diff $dstfile $newfile > $TMP
		if ($status == 0) then
		    if (! $?differ) then
			echo "[ $dstfile and $newfile are (now) identical ]"
			set ignore
			break
		    else
			if ($?merged) then
			    set newfile=$origfile
			    set install=no
			    echo "[ $oldfile and $newfile still differ from $dstfile ]"
			endif
		    endif
		else
		    set differ binary=(`head -1 $TMP`)
		    if ($#binary < 1) then
			exit 1
		    endif
		    if (! $?autoupdate) then
			more $TMP
		    endif
		    if ($binary[1] == "Binary") then
			set ignore
			break
		    endif
		endif
		if (! $?merged) then
		    cmp -s $oldfile $dstfile
		    if ("$status" == 0) then
			if ($?autoupdate) then
			    set edit="NO"
			    set install="YES"
    			    echo "[ $newfile ]" >>$autoupdate
			    cat $TMP >>$autoupdate
			else
			    set edit="no"
			endif
		    else
			if ($?autoupdate) then
			    set ignore
			    break;
			endif
			echo -n "Merge $newfile?  [ yes ]  "
			set ans="$<"
			switch ("$ans")
			case "":
			case [yY]:
			case [yY][eE][sS]:
			case [Mm]:
			case [Mm][Ee][Rr][Gg][Ee]:
			    if (-f $oldfile) then
				cp $newfile $tmpfile
				chmod u+w $tmpfile
				echo "[ merging from $oldfile to $newfile into $dstfile ]"
				set overlaps=(`unset echo; (merge -p $dstfile $oldfile $newfile >$tmpfile) |& tee /dev/tty`)
				set didmerge
				set newfile=$tmpfile
				if ($#overlaps > 0) then
				    set edit=YES
				    if ($?vimode) then
					set editat="+/<<<</"
				    endif
				else
				    unset edit
				endif
			    else
				echo "[ -DOOPS $dstfile $newfile ]"
				diff -DOOPS $dstfile $newfile >$tmpfile
				set newfile=$tmpfile
				set didmerge
				set edit=YES
				if ($?vimode) then
				    set editat="+/OOPS/"
				endif
			    endif
			    breaksw
			case [Ee]:
			case [Ee][Dd][Ii][Tt]:
			    set newfile=$dstfile
			    set edit=YES
			    breaksw
			case [Uu]:
			case [Uu][Pp][Dd][Aa][Tt][Ee]:
			    cp $newfile $tmpfile
			    chmod +w $tmpfile
			    set newfile=$tmpfile
			    set didmerge
			    set edit=no
			    breaksw
			case [Nn]:
			case [Nn][Oo]:
			    set ignore
			    break
			    breaksw
			default:
			    cat <<!
[
  Options are:
  
  y, yes, m, merge Merge from $oldfile to $newfile into $dstfile
  n, no		   Abort merge for $newfile and proceed to next file
  e, edit	   Edit $dstfile
  u, update	   Update $dstfile directly from $newfile
]
!
			    continue
			    breaksw
			endsw
		    endif
		    set merged
		endif
	    else
		if ($?autoupdate) then
		    set ignore
		    break
		endif
		echo "[ $dstfile does not currently exist ]"
		if ($?onlynew) then
		    set edit=NO
		    set install=YES
		    echo $newfile >>$onlynew
		endif
	    endif
	    if ($?edit) then
	      while (1)
		unset differ
		if ($edit != "YES") then
		    if ($edit != "NO") then
			echo -n "Edit $newfile?  [ $edit ]  "
			set ans="$<"
			if ("$ans" == "") set ans="$edit"
		    else
			set edit=no
			set ans=$edit
		    endif
		else
		    set ans=$edit
		endif
		unset diff1 diff2
		switch ("$ans")
		case [Ee]:
		case [Ee][Dd][Ii][Tt]:
		case [yY]:
		case [yY][eE][sS]:
		    if ($newfile != $tmpfile) then
			cp $newfile $tmpfile
			set newfile=$tmpfile
			chmod u+w $tmpfile
		    endif
		    $EDITOR $editat $newfile
		    set install=yes
		    set edit="diff"
		    continue
		    breaksw
		case [Nn]:
		case [Nn][Oo]:
		    if ("$edit" == "no") set ans="DONE"
		    breaksw
		case [Dd]:
		case [Dd][Ii][Ff][Ff]:
		    breaksw
		case [Dd][Oo]:
		    set diff1=$oldfile diff2=$dstfile edit="$ans"
		    breaksw
		case [Dd][Nn][Mm]:
		    set diff1=$origfile diff2=$newfile edit="$ans"
		    breaksw
		case [Dd][Oo][Nn]:
		    set diff1=$oldfile diff2=$origfile edit="$ans"
		    breaksw
		default:
		    cat <<!
[
  Options are:
  
  y, yes, e, edit Edit $newfile
  n, no		  Proceed to install $newfile (or final diff)
  d, diff	  Compare current $dstfile against merged $newfile (final diff)
  do		  Compare old $oldfile against current $dstfile
  dnm		  Compare new $origfile against merged $newfile
  don		  Compare old $oldfile against new $origfile
]
!
		    continue
		    breaksw
		endsw
		if ($?diff1) then
		    set editat=""
		    echo "[ comparing $diff1 and $diff2 ]"
		    diff $diff1 $diff2 | more
		    continue
		endif
		break
	      end
		if ("$ans" == "DONE") break
	    endif
	end
	if ($?autoupdate && "$install" != "YES") then
	    set ignore
	endif
	if (! $?ignore) then
	    if ("$install" != "YES") then
		echo -n "Install $newfile?  [ $install ]  "
		set ans="$<"
		if ("$ans" == "") set ans="$install"
	    else
		set install=yes
		set ans=$install
	    endif
	    if ("$ans" =~ [yY] || "$ans" =~ [yY][eE][sS]) then
		set ok
		set mkdir=()
		set dstdir=$dstfile
		while (1)
		    set dstdir=$dstdir:h
		    if (! -d $dstdir ) then
			set mkdir=($dstdir $mkdir)
		    else
			break
		    endif
		end
		if ($#mkdir > 0) then
		    set mkdir=($mkdir ${dstfile:h}/RCS)
		    echo "[ creating $mkdir ]"
		    mkdir $mkdir
		    if ($dstfile =~ *.h) chmod a+x $mkdir 
		endif
		set rcsfile=$dstfile:h/RCS/$dstfile:t,v
		if (-w $dstfile) then
		    echo "[ old $dstfile not checked in ]"
		    arch $version $dstfile
		    if ("$status" != 0) exit 1
		endif
		if (-f $dstfile) chmod +w $dstfile
		if (-f $rcsfile) $RCS -q -l $rcsfile
#		if ($?didmerge) then
#		    cp $origfile $xtmpfile
#		    $CI -l$branch -m" " $xtmpfile $rcsfile
#		    if ("$status" != 0) exit 1
#		    rm -f $xtmpfile
#		endif
		cp $newfile $dstfile
		if ("$status" != 0) exit 1
		sync
		if (! $?didmerge) then
		    arch -nowhist $version $dstfile
		    if ("$status" != 0) exit 1
		endif
		if ($dstfile =~ *.h) then
		    chmod a+r $dstfile
		endif
	    else
		set ignore
	    endif
	endif
    end
    rm -f $tmpfile
  end
end

cleanup:
rm -rf $tmp
exit 0
