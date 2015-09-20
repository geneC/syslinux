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

##
## COM32 standard modules
##

VPATH = $(SRC)
include $(MAKEDIR)/elf.mk

# BIOS-specific modules
MOD_BIOS = disk.c32 elf.c32 ethersel.c32 gpxecmd.c32 ifmemdsk.c32 ifplop.c32 \
	   kbdmap.c32 kontron_wdt.c32 pcitest.c32 pmload.c32 poweroff.c32 \
	   prdhcp.c32 pxechn.c32 sanboot.c32 sdi.c32 vesainfo.c32

# All-architecture modules
MOD_ALL  = cat.c32 cmd.c32 config.c32 cptime.c32 cpuid.c32 cpuidtest.c32 \
	   debug.c32 dir.c32 dmitest.c32 hexdump.c32 host.c32 ifcpu.c32 \
	   ifcpu64.c32 linux.c32 ls.c32 meminfo.c32 pwd.c32 reboot.c32 \
	   vpdtest.c32 whichsys.c32 zzjson.c32

ifeq ($(FIRMWARE),BIOS)
MODULES = $(MOD_ALL) $(MOD_BIOS)
else
MODULES = $(MOD_ALL)
endif

TESTFILES =

all: $(MODULES) $(TESTFILES)

.PRECIOUS: %.o
dmitest.o: dmitest.c
	$(CC) $(CFLAGS) $(GPLINCLUDE) -c -o $@ $<

dmitest.elf : dmi_utils.o dmitest.o $(C_LIBS)
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
