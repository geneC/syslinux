## -----------------------------------------------------------------------
##
##   Copyright 2001-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

topdir = ../..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/com32.mk

MODULES	  = gfxboot.c32

all: $(MODULES)

gfxboot.elf : gfxboot.o realmode_callback.o $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

realmode_callback.o: realmode_callback.asm
	$(NASM) -f bin -O99 -o $*.tmp -l $*.lst $<
	$(OBJCOPY) -B i386 -I binary -O elf32-i386 \
	  --redefine-sym _binary_$*_tmp_start=$*_start \
	  --redefine-sym _binary_$*_tmp_end=$*_end \
	  --strip-symbol _binary_$*_tmp_size \
	  $*.tmp $@

tidy dist:
	rm -f *.o *.lo *.a *.lst *.elf .*.d *.tmp

clean: tidy
	rm -f *.lnx

spotless: clean
	rm -f *.lss *.c32 *.com
	rm -f *~ \#*

install:

-include .*.d
