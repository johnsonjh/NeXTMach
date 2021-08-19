"	gmon.ex	(c) 1988 NeXT, Inc. "
"	fix funny things done by mcount()"
"	like its name. "
g/_mcount/s//mcount/g
" undo thrashing of file name for GDB "
g/subrmcount/s//subr_mcount/g
w
q
