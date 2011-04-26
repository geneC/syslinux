## -----------------------------------------------------------------------
##
##   Copyright 2001-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
##   Boston MA 02110-1301, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## Lua Makefile
##

topdir = ../../..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/com32.mk

LNXLIBS	   = 

# Temporarily allow warnings not being treated as errors
#GCCWARN	  += -Wno-error

CFLAGS    += -DSYSLINUX
MODULES	  = lua.c32
TESTFILES =

OBJS	  = lua.o

LIBLUA    = liblua.a

LIBLUA_OBJS := lapi.o lcode.o ldebug.o ldo.o ldump.o lfunc.o
LIBLUA_OBJS += lgc.o llex.o lmem.o  lobject.o lopcodes.o lparser.o
LIBLUA_OBJS += lstate.o lstring.o ltable.o ltm.o lvm.o lzio.o lundump.o
LIBLUA_OBJS += syslinux.o

LIBLUA_OBJS += lauxlib.o lbaselib.o ldblib.o ltablib.o
LIBLUA_OBJS += lstrlib.o loadlib.o linit.o
LIBLUA_OBJS += liolib.o
LIBLUA_OBJS += dmi.o
LIBLUA_OBJS += cpu.o
LIBLUA_OBJS += pci.o
LIBLUA_OBJS += vesa.o

CFLAGS += -DLUA_ANSI

all: $(MODULES) $(TESTFILES)

$(LIBLUA) : $(LIBLUA_OBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

lua.elf : $(OBJS) $(LIBLUA) $(C_LIBS)
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
