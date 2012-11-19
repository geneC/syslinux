#!/bin/sh

# Find where the gnu-efi package has been installed as this location
# differs across distributions.

include_dirs="/usr/include /usr/local/include"
lib_dirs="/usr/lib /usr/lib64 /usr/local/lib /usr/lib32"

function find_include()
{
    for d in $include_dirs; do
	found=`find $d -name efi -type d 2> /dev/null`
	if [ "$found"x != "x" ] && [ -e $found/$ARCH/efibind.h ]; then
	    echo $found
	    break;
	fi
    done
}

function find_lib()
{
    for d in $lib_dirs; do
	found=`find $d -name libgnuefi.a 2> /dev/null`
	if [ "$found"x != "x" ]; then
	    crt_name='crt0-efi-'$ARCH'.o'
	    crt=`find $d -name $crt_name 2> /dev/null`
	    if [ "$crt"x != "x" ]; then
		echo $d
		break;
	    fi
	fi
    done
}

ARCH=$2
case $1 in
    include)
	find_include
	;;
    lib)
	find_lib
	;;
esac
