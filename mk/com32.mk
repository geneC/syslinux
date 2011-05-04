## -*- makefile -*- -------------------------------------------------------
##
##   Copyright 2008-2009 H. Peter Anvin - All Rights Reserved
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
## COM32 common configurables
##

include $(MAKEDIR)/syslinux.mk

GCCOPT := $(call gcc_ok,-std=gnu99,)
GCCOPT += $(call gcc_ok,-m32,)
GCCOPT += $(call gcc_ok,-fno-stack-protector,)
GCCOPT += $(call gcc_ok,-fwrapv,)
GCCOPT += $(call gcc_ok,-freg-struct-return,)
GCCOPT += -mregparm=3 -DREGPARM=3 -march=i386 -Os
GCCOPT += $(call gcc_ok,-fPIE,-fPIC)
GCCOPT += $(call gcc_ok,-fno-exceptions,)
GCCOPT += $(call gcc_ok,-fno-asynchronous-unwind-tables,)
GCCOPT += $(call gcc_ok,-fno-strict-aliasing,)
GCCOPT += $(call gcc_ok,-falign-functions=0,-malign-functions=0)
GCCOPT += $(call gcc_ok,-falign-jumps=0,-malign-jumps=0)
GCCOPT += $(call gcc_ok,-falign-labels=0,-malign-labels=0)
GCCOPT += $(call gcc_ok,-falign-loops=0,-malign-loops=0)
GCCOPT += $(call gcc_ok,-mpreferred-stack-boundary=2,)
GCCOPT += $(call gcc_ok,-incoming-stack-boundary=2,)

com32  := $(topdir)/com32
RELOCS := $(com32)/tools/relocs

ifneq ($(NOGPL),1)
GPLLIB     = $(com32)/gpllib/libcom32gpl.a
GPLINCLUDE = -I$(com32)/gplinclude
else
GPLLIB     =
GPLINCLUDE =
endif

CFLAGS     = $(GCCOPT) $(GCCWARN) -march=i386 \
	     -fomit-frame-pointer -D__COM32__ \
	     -nostdinc -iwithprefix include \
	     -I$(com32)/libutil/include -I$(com32)/include $(GPLINCLUDE)
SFLAGS     = $(GCCOPT) $(GCCWARN) -march=i386 \
	     -fomit-frame-pointer -D__COM32__ \
	     -nostdinc -iwithprefix include \
	     -I$(com32)/libutil/include -I$(com32)/include $(GPLINCLUDE)

COM32LD	   = $(com32)/lib/com32.ld
LDFLAGS    = -m elf_i386 --emit-relocs -T $(COM32LD)
LIBGCC    := $(shell $(CC) $(GCCOPT) --print-libgcc)

LNXCFLAGS  = -I$(com32)/libutil/include $(GCCWARN) -O -g \
	     -D_GNU_SOURCE -D_FORTIFY_SOURCE=0 -Wno-error
LNXSFLAGS  = -g
LNXLDFLAGS = -g

C_LIBS	   = $(com32)/libutil/libutil_com.a $(GPLLIB) \
	     $(com32)/lib/libcom32.a $(LIBGCC)
C_LNXLIBS  = $(com32)/libutil/libutil_lnx.a

.SUFFIXES: .lss .c .lo .o .elf .c32 .lnx

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(MAKEDEPS) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.elf
%.elf: %.o $(LIBS) $(C_LIBS) $(COM32LD)
	$(LD) $(LDFLAGS) -o $@ $(filter-out $(COM32LD),$^)

.PRECIOUS: %.lo
%.lo: %.S
	$(CC) $(MAKEDEPS) $(LNXSFLAGS) -c -o $@ $<

.PRECIOUS: %.lo
%.lo: %.c
	$(CC) $(MAKEDEPS) $(LNXCFLAGS) -c -o $@ $<

.PRECIOUS: %.lnx
%.lnx: %.lo $(LNXLIBS) $(C_LNXLIBS)
	$(CC) $(LNXCFLAGS) -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@
	$(RELOCS) $< >> $@ || ( rm -f $@ ; false )
