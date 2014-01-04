#!/bin/sh

# Verify that gnu-efi is installed in the object directory for our
# firmware. If it isn't, build it.

if [ $# -lt 3 ]; then
cat <<EOF
Usage: $0: <arch> <srcdir> <objdir>

Check for gnu-efi libraries and header files in <objdir> and, if none
exist, build and install them.

  <arch>   - A gnu-efi \$ARCH argument, i.e. ia32, x86_64
  <srcdir> - The top-level directory of the Syslinux source
  <objdir> - The Syslinux object directory

EOF
    exit 1
fi

ARCH=$1
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

if [ ! -f $objdir/include/efi/$ARCH/efibind.h ]; then
    # Build the external project with a clean make environment, as
    # Syslinux disables built-in implicit rules.
    export MAKEFLAGS=

    build=$srcdir/efi/build-gnu-efi.sh
    $build $ARCH $srcdir $objdir &> /dev/null
    if [ $? -ne 0 ]; then
	printf "Failed to build gnu-efi. "
	printf "Execute the following command for full details: \n\n"
	printf "$build $ARCH $srcdir $objdir\n\n"

	exit 1
    fi
fi
