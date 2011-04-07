## -----------------------------------------------------------------------
##
##   Copyright 2011 Intel Corporation - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

topdir = ../../..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/elf.mk

CFLAGS += -I../modules -I$(topdir)/core/elflink -I$(topdir)/core/include

all: ldlinux.c32

ldlinux.c32 : ldlinux.o cli.o readconfig.o refstr.o colors.o getadv.o \
		adv.o ipappend.o
	$(LD) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o *.lo *.a *.lst .*.d 

clean: tidy
	rm -f *.lss *.lnx *.com *.c32

spotless: clean
	rm -f *~ \#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)
	install -m 644 ldlinux.c32 $(INSTALLROOT)$(AUXDIR)


-include .*.d
