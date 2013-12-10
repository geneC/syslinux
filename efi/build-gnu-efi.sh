#!/bin/sh

ARCH=$1
objdir=$2
topdir=$3

pushd $topdir
git submodule init
git submodule update

cd gnu-efi/gnu-efi-3.0/

make ARCH=$ARCH

make ARCH=$ARCH PREFIX=$objdir install
make ARCH=$ARCH clean

popd
