#!/bin/sh

set -e

# Initialise the gnu-efi submodule and ensure the source is up-to-date.
# Then build and install it for the given architecture.

if [ $# -lt 3 ]; then
cat <<EOF
Usage: $0: <arch> <srcdir> <objdir>

Build the <arch> gnu-efi libs and header files and install in <objdir>.

  <arch>   - A gnu-efi \$ARCH argument, i.e. ia32, x86_64
  <srcdir> - The top-level directory of the Syslinux source
  <objdir> - The Syslinux object directory

EOF
    exit 1
fi

ARCH=$1
if gcc -dumpmachine|grep x86_64 > /dev/null;then
	HOSTARCH=x86_64
else
	HOSTARCH=ia32
fi
if which realpath > /dev/null;then
	REALPATH="realpath"
elif which readlink > /dev/null;then
	REALPATH="readlink -f"
else
	echo "No realpath or readlink found; aborting"
	return 1
fi
srcdir=`$REALPATH $2`
objdir=`$REALPATH $3`

do_build() (
	echo Building gnu-efi for $ARCH on $HOSTARCH from $srcdir to $objdir
	cd $srcdir
	git submodule update --init

	cd gnu-efi/gnu-efi-3.0/

	echo Building gnu-efi: make
	make ARCH=$ARCH HOSTARCH=$HOSTARCH
	echo Built gnu-efi, installing

	make ARCH=$ARCH HOSTARCH=$HOSTARCH PREFIX=$objdir install
	make ARCH=$ARCH HOSTARCH=$HOSTARCH clean
)

do_build
