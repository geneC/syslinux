## -----------------------------------------------------------------------
##  $Id$
##
##   Copyright 1998 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
##   USA; either version 2 of the License, or (at your option) any later
##   version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Main Makefile for SYSLINUX
#

NASM	= nasm

all:	bootsect.bin ldlinux.sys syslinux.com syslinux

ldlinux.bin: ldlinux.asm
	$(NASM) -f bin -dHEX_TIME="`perl now.pl`" -l ldlinux.lst -o ldlinux.bin ldlinux.asm
	ls -l ldlinux.bin

bootsect.bin: ldlinux.bin
	dd if=ldlinux.bin of=bootsect.bin bs=512 count=1

ldlinux.sys: ldlinux.bin
	dd if=ldlinux.bin of=ldlinux.sys  bs=512 skip=1
	ls -l ldlinux.sys

syslinux.com: syslinux.asm bootsect.bin ldlinux.sys
	$(NASM) -f bin -l syslinux.lst -o syslinux.com syslinux.asm
	ls -l syslinux.com

syslinux: syslinux.pl.in bootsect.bin ldlinux.sys
	@if [ ! -x `which perl` ]; then \
		echo 'ERROR: cannot find perl'; exit 1 ; fi
	echo '#!' `which perl` > syslinux
	cat syslinux.pl.in bootsect.bin ldlinux.sys >> syslinux
	chmod a+x syslinux

clean:
	rm -f *.bin *.lst *.sys

distclean: clean
	rm -f *~ \#*
