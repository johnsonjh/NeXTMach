#! /bin/csh -f

set usrdir = /usr/include
set cmudir = /usr/cs/include

unsetenv MORE

foreach d (net netimp netinet netpup stand sys vax vaxif vaxmba vaxuba)
    if ($d == "sys") then
	cd /usr/uk/h
    else
	cd /usr/uk/$d
    endif
    foreach f (*.h)
	if (-e $usrdir/$d/$f) then
	    cmp -s $f $usrdir/$d/$f
	    set stat = $status
	    if ($stat == 0) then
		if (-e $cmudir/$d/$f) then
		    cmp -s $f $cmudir/$d/$f
		    set stat = $status
		    echo ""
		    echo "[ $d/$f ]"
		    if ($stat == 0) then
			echo "[ cs version of $d/$f needs to be deleted ]"
			echo -n "Remove $cmudir/$d/$f ?  [no] "
			set ans = "$<"
			if ("$ans" =~ [yY] || "$ans" =~ [yY][eE][sS]) then
			    echo "[ removing $cmudir/$d/$f ]"
			    rm -f $cmudir/$d/$f /dist/$cmudir/$d/$f
			endif
		    else
			echo "[ comparing $cmudir/$d/$f and $f ]"
			diff $cmudir/$d/$f $f | more
			echo "[ cs version of $d/$f needs to be merged ]"
		    endif
		endif
		continue
	    endif
	    if (! -e $cmudir/$d/$f) then
		echo ""
		echo "[ $d/$f ]"
		echo "[ comparing $usrdir/$d/$f and $f ]"
		diff $usrdir/$d/$f $f | more
		echo "[ cs version of $d/$f needs to be created ]"
		echo -n "Create $cmudir/$d/$f ?  [yes] "
		set ans = "$<"
		if ("$ans" == "" || "$ans" =~ [yY] || "$ans" =~ [yY][eE][sS]) then
		    echo "[ creating $cmudir/$d/$f ]"
		    cp $f $cmudir/$d/$f
		    chown cmu $cmudir/$d/$f
		    chgrp cmu $cmudir/$d/$f
		    chmod 644 $cmudir/$d/$f
		    /usr/cs/etc/reinstall -y $cmudir/$d/$f
		endif
		continue
	    endif
	endif
	if (-e $cmudir/$d/$f) then
	    cmp -s $f $cmudir/$d/$f
	    set stat = $status
	    if ($stat == 0) continue
	    echo ""
	    echo "[ $d/$f ]"
	    echo "[ comparing $cmudir/$d/$f and $f ]"
	    diff $cmudir/$d/$f $f | more
	    echo "[ cs version of $d/$f needs to be updated ]"
	    echo -n "Update $cmudir/$d/$f ?  [no] "
	    set ans = "$<"
	    if ("$ans" =~ [yY] || "$ans" =~ [yY][eE][sS]) then
		echo "[ updating $cmudir/$d/$f ]"
		cp -i $f $cmudir/$d/$f
		chown cmu $cmudir/$d/$f
		chgrp cmu $cmudir/$d/$f
		chmod 644 $cmudir/$d/$f
		/usr/cs/etc/reinstall -y $cmudir/$d/$f
	    endif
	    continue
	endif
	echo ""
	echo "[ $d/$f ]"
	echo "[ cs version of $d/$f needs to be created ]"
	echo -n "Create $cmudir/$d/$f ?  [yes] "
	set ans = "$<"
	if ("$ans" == "" || "$ans" =~ [yY] || "$ans" =~ [yY][eE][sS]) then
	    echo "[ creating $cmudir/$d/$f ]"
	    cp $f $cmudir/$d/$f
	    chown cmu $cmudir/$d/$f
	    chgrp cmu $cmudir/$d/$f
	    chmod 644 $cmudir/$d/$f
	    /usr/cs/etc/reinstall -y $cmudir/$d/$f
	endif
    end
end
