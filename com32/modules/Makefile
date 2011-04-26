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

topdir = ../..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/com32.mk

MODULES	  = chain.c32 config.c32 ethersel.c32 dmitest.c32 cpuidtest.c32 \
	    disk.c32 pcitest.c32 elf.c32 linux.c32 reboot.c32 pmload.c32 \
	    meminfo.c32 sdi.c32 sanboot.c32 ifcpu64.c32 vesainfo.c32 \
	    kbdmap.c32 cmd.c32 vpdtest.c32 host.c32 ls.c32 gpxecmd.c32 \
	    ifcpu.c32 cpuid.c32 cat.c32 pwd.c32 ifplop.c32 zzjson.c32 whichsys.c32

TESTFILES =

all: $(MODULES) $(TESTFILES)

pcitest.elf : pcitest.o $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

cpuidtest.elf : cpuidtest.o $(GPLLIB) $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: %.o
dmitest.o: dmitest.c
	$(CC) $(CFLAGS) $(GPLINCLUDE) -c -o $@ $<

dmitest.elf :  dmi_utils.o dmitest.o $(GPLLIB) $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

ethersel.elf : ethersel.o $(LIBS) $(C_LIBS)
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
