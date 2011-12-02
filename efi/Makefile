## -----------------------------------------------------------------------
##
##   Copyright 2011 Intel Corporation; author: Matt Fleming
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

topdir = ..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/lib.mk
include $(MAKEDIR)/efi.mk

CORE_CSRC := $(wildcard $(core)/*.c $(core)/*/*.c $(core)/*/*/*.c)
CORE_COBJ := $(patsubst %.c,%.o,$(CORE_CSRC))

# Don't include console objects
CORE_OBJS = $(filter-out $(core)/hello.o $(core)/rawcon.o \
	$(core)/plaincon.o $(core)/strcasecmp.o,$(CORE_COBJ))
LIB_OBJS = $(addprefix $(com32)/lib/,$(MINLIBOBJS))

CSRC = $(wildcard *.c)
OBJ = $(patsubst %.c,%.o,$(CSRC))
OBJS = $(filter-out wrapper.o,$(OBJ))

OBJS += $(core)/codepage.o

# The targets to build in this directory
BTARGET  = syslinux.efi

syslinux.so: $(OBJS) $(CORE_OBJS) $(LIB_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ -lgnuefi -lefi

# We need to rename the .hash section because the EFI firmware
# linker really doesn't like it.
# $(OBJCOPY) --rename-section .gnu.hash=.sdata,load,data,alloc $^ $@
#syslinux.so: syslinux1.so
#	cp $^ $@

wrapper: wrapper.c
	$(CC) $^ -o $@

#
# Build the wrapper app and wrap our .so to produce a .efi
syslinux.efi: syslinux.so wrapper
	./wrapper syslinux.so $@

all: $(BTARGET)

$(core)/codepage.o: ../codepage/cp865.cp
	cp ../codepage/cp865.cp codepage.cp
	$(CC) $(SFLAGS) -c -o $@ $(core)/codepage.S

tidy dist:
	rm -f *.so *.o
	find . \( -name \*.o -o -name \*.a -o -name .\*.d -o -name \*.tmp \) -print0 | \
		xargs -0r rm -f

clean: tidy

spotless: clean
	rm -f $(BTARGET)
