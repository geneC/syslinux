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

MODULES	  = chain.c32 ethersel.c32 mboot.c32 dmitest.c32 cpuidtest.c32 \
	    pcitest.c32 elf.c32 linux.c32 reboot.c32 pmload.c32 meminfo.c32 \
	    sdi.c32 sanboot.c32 ifcpu64.c32

TESTFILES =

all: $(MODULES) $(TESTFILES)

pcitest.elf : pcitest.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

cpuidtest.elf : cpuidtest.o cpuid.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

dmitest.elf : dmitest.o dmi_utils.o dmi.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

ethersel.elf : ethersel.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o *.lo *.a *.lst *.elf .*.d *.tmp

clean: tidy
	rm -f *.lss *.c32 *.lnx *.com

spotless: clean
	rm -f *~ \#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)
	install -m 644 $(MODULES) $(INSTALLROOT)$(AUXDIR)

-include .*.d
