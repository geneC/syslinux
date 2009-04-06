## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
##   Boston MA 02110-1301, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## Hardware Detection Tool
##

topdir = ../..
include ../MCONFIG

LIBS	   = ../cmenu/libmenu/libmenu.a ../libutil/libutil_com.a \
	     ../lib/libcom32.a $(LIBGCC)
CFLAGS    += -I$(com32)/cmenu/libmenu

MODULES	  = hdt.c32
TESTFILES =

OBJS	  = $(patsubst %.c,%.o,$(wildcard *.c))

all: $(MODULES) $(TESTFILES)

hdt.elf : $(OBJS) $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o *.lo *.a *.lst *.elf .*.d *.tmp

clean: tidy
	rm -f *.lnx

spotless: clean
	rm -f *.lss *.c32 *.com
	rm -f *~ \#*

install:

-include .*.d
