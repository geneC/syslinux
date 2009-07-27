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

topdir = ../../..
include MCONFIG

MODULES	  = hello.c32 sort.c32
TESTFILES =

all: $(MODULES) $(TESTFILES)
	
hello.c32 : hello.o
	$(LD) $(LDFLAGS) -o $@ $^
	
sort.c32 : sort.o
	$(LD) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o *.lo *.a *.lst .*.d 

clean: tidy
	rm -f *.lss *.lnx *.com

spotless: clean
	rm -f *~ \#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)
	install -m 644 $(MODULES) $(INSTALLROOT)$(AUXDIR)

-include .*.d
