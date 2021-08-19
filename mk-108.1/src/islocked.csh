#!/bin/csh -f

set result=0
foreach file ($argv)
	set rcsfile=RCS/"$file",v
	switch ($file)
		case "*,v":
			breaksw
		default:
			if (! -r $rcsfile) then
				echo "$file"': no RCS file'
				set result=2
			 else
				sed '/^[ 	]*$/,$d' < $rcsfile | egrep '^locks    ; strict;$' > /dev/null
				if ($status) then
					set result=$status
					echo -n "$file"': locked by '
					egrep '^locks    ' < $rcsfile | sed 's/locks    \([^ :]*\):.*/\1/' | head -1
				endif
			endif
			breaksw
	endsw
end
exit $result
