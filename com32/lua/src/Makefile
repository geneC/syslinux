## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## samples for syslinux users
##

TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)
CC	= gcc

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) -c -x c /dev/null -o $$tmpf 2>/dev/null; \
	           then echo $(1); else echo $(2); fi; rm -f $$tmpf)

M32     := $(call gcc_ok,-m32,) $(call gcc_ok,-fno-stack-protector,)

LD         = ld -m elf_i386
AR	   = ar
NASM	   = nasm
NASMOPT	   = -O9999
RANLIB	   = ranlib
CFLAGS     = $(M32) -mregparm=3 -DREGPARM=3 -W -Wall -march=i386 -Os \
	     -fomit-frame-pointer -D__COM32__ \
	     -nostdinc -iwithprefix include \
	     -I../../libutil/include -I../../include \
	     -Wp,-MT,$@,-MD,$(dir $@).$(notdir $@).d
LNXCFLAGS  = -W -Wall -O -g -I../../libutil/include
LNXSFLAGS  = -g
LNXLDFLAGS = -g
SFLAGS     = -D__COM32__ -march=i386
LDFLAGS    = -T ../../lib/com32.ld
OBJCOPY    = objcopy
LIBGCC    := $(shell $(CC) --print-libgcc)
LIBS	   = liblua.a ../../libutil/libutil_com.a ../../lib/libcom32.a  $(LIBGCC)
LNXLIBS	   = ../../libutil/libutil_lnx.a

.SUFFIXES: .lss .c .o .elf .c32 .lnx

LIBLUA_OBJS := lapi.o lcode.o ldebug.o ldo.o ldump.o lfunc.o
LIBLUA_OBJS += lgc.o llex.o lmem.o  lobject.o lopcodes.o lparser.o
LIBLUA_OBJS += lstate.o lstring.o ltable.o ltm.o lvm.o lzio.o lundump.o

LIBLUA_OBJS += lauxlib.o lbaselib.o ldblib.o  ltablib.o \
	lstrlib.o loadlib.o linit.o


CFLAGS += -DLUA_ANSI

all:	liblua.a lua.c32

liblua.a: $(LIBLUA_OBJS)
	$(AR) cq $@ $^
	$(RANLIB) $@



.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.elf
%.elf: %.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: %.lo
%.lo: %.S
	$(CC) $(LNXSFLAGS) -c -o $@ $<

.PRECIOUS: %.lo
%.lo: %.c
	$(CC) $(LNXCFLAGS) -c -o $@ $<

.PRECIOUS: %.lnx
%.lnx: %.lo $(LNXLIBS)
	$(CC) $(LNXLDFLAGS) -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

tidy dist:
	rm -f *.o *.lo *.a *.lst *.elf .*.d

clean: tidy
	rm -f *.lss *.c32 *.lnx *.com

spotless: clean
	rm -f *~ \#*

install:	# Don't install samples

-include .*.d
