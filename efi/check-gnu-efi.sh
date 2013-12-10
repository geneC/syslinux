#!/bin/sh

ARCH=$1

if [ ! -f $objdir/include/$ARCH/efibind.h ]; then
    # Build the external project with a clean make environment, as
    # Syslinux disables built-in implicit rules.
    export MAKEFLAGS=

    $topdir/efi/build-gnu-efi.sh $ARCH $objdir $topdir &> /dev/null
fi
