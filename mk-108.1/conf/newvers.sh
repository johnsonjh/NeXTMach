#!/bin/sh -
#
rm -f vers.c
edit="$1"; major="$2"; minor="$3"; variant="$4"
v="${major}.${minor}" d=`pwd` t=`date`
CONFIG=`expr "$d" : '.*/\([^/_]*\).*$'`
(
  /bin/echo "char version[128] = \"NeXT Mach ${v}: ${t}; $d\\n\";" ;
) > vers.c
if [ -s vers.suffix -o ! -f vers.suffix ]; then
    rm -f vers.suffix
    echo ".${variant}${edit}.${CONFIG}" >vers.suffix
fi
