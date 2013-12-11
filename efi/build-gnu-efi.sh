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
srcdir=`realpath $2`
objdir=`realpath $3`

pushd $srcdir
git submodule init
git submodule update

cd gnu-efi/gnu-efi-3.0/

make ARCH=$ARCH

make ARCH=$ARCH PREFIX=$objdir install
make ARCH=$ARCH clean

popd
