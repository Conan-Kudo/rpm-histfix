#!/bin/sh

libtoolize --copy --force
aclocal
autoheader
autoconf
automake

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

if [ X"$@" = X  -a "X`uname -s`" = "XLinux" ]; then
    ./configure --prefix=/usr "$@"
else
    ./configure "$@"
fi
