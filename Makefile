#ident "$Id$"
## -----------------------------------------------------------------------
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

all:	bootsect.bin ldlinux.sys

ldlinux.bin: ldlinux.asm
	$(NASM) -f bin -dHEX_TIME="`perl now.pl`" -l ldlinux.lst -o ldlinux.bin ldlinux.asm

bootsect.bin: ldlinux.bin
	dd if=ldlinux.bin of=bootsect.bin bs=512 count=1

ldlinux.sys:
	dd if=ldlinux.bin of=ldlinux.sys  bs=512 skip=1

clean:
	rm -f *.bin *.lst *.sys

distclean: clean
	rm -f *~ \#*
