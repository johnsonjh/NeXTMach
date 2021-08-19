#!/bin/csh -f

set prog=$0
set base={$prog:h}
set prog=${prog:t}

unset yes
unset single
unset part2
unset restart
set   restartfile=.check_cond.restart
set   when=.check_cond.when
set   when_new=.check_cond.new
set   when_old=.check_cond.old
while ($#argv > 0)
    if ("$1" =~ -*) then
        switch ("$1")
	    case "-part2":
		set part2
	        breaksw
	    case "-r":
	    case "-restart":
		if (-f $restartfile) then
		    set restart=(`cat $restartfile`)
		    if ($#restart == 0) unset restart
		endif
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
    echo  "Usage: ${prog} <against-directory>"
    exit 0
endif

set dir=$1

if (! $?part2) then
    set m=""
    if ($?restart) then
        set m=" (restarting with $restart)"
    endif
    echo "[ Checking conditionals in $cwd against $dir$m ]"

    set args=($dir)
    if ($?restartfile) then
	set args=(-restart $args)
    endif
    set sed=(-e "s;^\./;;" -e "/^\..*\//d" -e "/^[A-Z].*\//d" -e "/^stand\.4\.2\//d" -e "/^src\/as\//d" -e "/^src\/as\//d")
    if ($?restart) then
	set restart=(`echo $restart | sed -e "s;\.;\\.;g" -e "s;/;\\/;g"`)
        set sed=(-n $sed -e "/^${restart}"'$'"/{" -e ': loop' $sed -e p -e n -e 'b loop' -e '}')
    endif
    if (! -f $when_new) touch $when_new
    find -nosymlink . -type f -newer $when -print | sed $sed:q | sort | exec $0 -part2 $args
endif

set TMPE=/tmp/${prog}E$$
set TMPO=/tmp/${prog}O$$
set TMPS=/tmp/${prog}S$$
echo "-error=$TMPE" >>$TMPS

onintr cleanup

while (1)
    set f="$<"
    if ($f == "") break

    if ($?restartfile) then
	echo $f >$restartfile
    endif
    if ($f =~ *.[csh] && -d $dir/$f:h) then
	echo "[ checking $f ]"
	unset okcond okdiff
	set ckcond ckdiff
	while (1)
	    if ($?ckcond && ! $?okcond) then
		awk -f lib/$prog.awk $TMPS $f
		set xstatus=$status
		if ($xstatus == 0) then
		    unset ckcond
		    set okcond ckdiff
		endif
	    else
		set xstatus=0
	    endif
	    if ($xstatus == 0) then
		if (! -f $dir/$f) then
		    echo "[ no $dir/$f to diff ]"
		    break
		endif
		if ($?ckdiff && ! $?okdiff) then
		    (echo "-file=$f -error=$TMPE"; diff $dir/$f $f;exit 0) | awk -f lib/check_diff.awk |& tail -10l
		    set xstatus=$status
		    if ($xstatus == 0) then
			unset ckdiff
			set okdiff ckcond
		    endif
		else
		    set xstatus=0
		endif
	    endif
	    if ("$xstatus" != 0) then
		set error=(`cat $TMPE`)
		rm -f $TMPE
		chmod +w $f
		set x=$f
		if ($xstatus == 2 || $xstatus == 3) set x=($x $dir/$f)
		$EDITOR </dev/tty +"$error[1]" $x
		unset okcond okdiff
	    else
		if ($?okcond && $?okdiff) then
		    break
		endif
	    endif
	end
    else
	echo "[ ignoring $f ]"
    endif
end

mv $when $when_old
mv $when_new $when
ls -l $when $when_old
if ($?restartfile) then
    rm -rf $restartfile
endif

cleanup:
rm -f $TMPE $TMPO $TMPS
