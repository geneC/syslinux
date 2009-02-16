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
## COM32 standard modules
##

topdir = ../..
include ../MCONFIG

MODULES	  = chain.c32 config.c32 ethersel.c32 mboot.c32 dmitest.c32 \
	    cpuidtest.c32 \
	    pcitest.c32 elf.c32 linux.c32 reboot.c32 pmload.c32 meminfo.c32 \
	    sdi.c32 sanboot.c32 ifcpu64.c32 vesainfo.c32 kbdmap.c32 cmd.c32

TESTFILES =

all: $(MODULES) $(TESTFILES)

pcitest.elf : pcitest.o $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

cpuidtest.elf : cpuidtest.o cpuid.o $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

dmitest.elf : dmi.o dmitest.o dmi_utils.o $(LIBS) $(C_LIBS)
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

install: all

-include .*.d
