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
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/elf.mk

CFLAGS += -I$(topdir)/core/elflink

MODULES	  = hello.c32 sort.c32 mytest.c32 menumain.c32 printmsg.c32 background.c32 passwd.c32 sha1hash.c32 \
	unbase64.c32 sha512crypt.c32 md5.c32 crypt-md5.c32 sha256crypt.c32 get_key.c32 ansiraw.c32 test.c32 \
	meminfo.c32 menu.c32 drain.c32 vesainfo.c32

TESTFILES =

all: $(MODULES) $(TESTFILES)
	
test.c32 : test.o
	$(LD) $(LDFLAGS) -o $@ $^

menu.c32 : menu.o 
	$(LD) $(LDFLAGS) -o $@ $^

hello.c32 : hello.o
	$(LD) $(LDFLAGS) -o $@ $^
	
sort.c32 : sort.o
	$(LD) $(LDFLAGS) -o $@ $^

mytest.c32 : mytest.o
	$(LD) $(LDFLAGS) -o $@ $^

menumain.c32 : menumain.o
	$(LD) $(LDFLAGS) -o $@ $^

printmsg.c32 : printmsg.o
	$(LD) $(LDFLAGS) -o $@ $^

background.c32 : background.o
	$(LD) $(LDFLAGS) -o $@ $^

drain.c32 : drain.o
	$(LD) $(LDFLAGS) -o $@ $^

passwd.c32 : passwd.o
	$(LD) $(LDFLAGS) -o $@ $^

sha1hash.c32 : sha1hash.o
	$(LD) $(LDFLAGS) -o $@ $^

unbase64.c32 : unbase64.o
	$(LD) $(LDFLAGS) -o $@ $^

sha512crypt.c32 : sha512crypt.o
	$(LD) $(LDFLAGS) -o $@ $^

sha256crypt.c32 : sha256crypt.o
	$(LD) $(LDFLAGS) -o $@ $^

md5.c32 : md5.o
	$(LD) $(LDFLAGS) -o $@ $^

crypt-md5.c32 : crypt-md5.o
	$(LD) $(LDFLAGS) -o $@ $^

get_key.c32 : get_key.o
	$(LD) $(LDFLAGS) -o $@ $^

ansiraw.c32 : ansiraw.o
	$(LD) $(LDFLAGS) -o $@ $^

meminfo.c32 : meminfo.o
	$(LD) $(LDFLAGS) -o $@ $^

pcitest.c32 : pcitest.o
	$(LD) $(LDFLAGS) -o $@ $^

vesainfo.c32 : vesainfo.o
	$(LD) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o *.lo *.a *.lst .*.d 

clean: tidy
	rm -f *.lss *.lnx *.com *.c32

spotless: clean
	rm -f *~ \#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)
	install -m 644 $(MODULES) $(INSTALLROOT)$(AUXDIR)

-include .*.d
